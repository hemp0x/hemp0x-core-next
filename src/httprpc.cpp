// Copyright (c) 2015-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Raven Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "httprpc.h"

#include "base58.h"
#include "chainparams.h"
#include "httpserver.h"
#include "rpc/protocol.h"
#include "rpc/server.h"
#include "random.h"
#include "sync.h"
#include "util.h"
#include "utilstrencodings.h"
#include "utiltime.h"
#include "ui_interface.h"
#include "crypto/hmac_sha256.h"
#include <stdio.h>
#include <map>

#include <boost/algorithm/string.hpp> // boost::trim

/** WWW-Authenticate to present with 401 Unauthorized response */
static const char* WWW_AUTH_HEADER_DATA = "Basic realm=\"jsonrpc\"";

/** Default auth-failure throttling parameters */
static const int DEFAULT_RPC_MAX_AUTH_FAILURES = 20;
static const int DEFAULT_RPC_AUTH_FAILURE_WINDOW = 300; // seconds
static const int DEFAULT_RPC_AUTH_FAILURE_BAN = 600;    // seconds

/** Per-source auth-failure tracking state */
struct RPCAuthFailureState {
    int count{0};
    int64_t windowStart{0};
    int64_t banUntil{0};
};

/** Map from source IP to auth-failure state. Protected by cs_authFailures. */
static std::map<std::string, RPCAuthFailureState> mapAuthFailures;
static CCriticalSection cs_authFailures;

/** Simple one-shot callback timer to be used by the RPC mechanism to e.g.
 * re-lock the wallet.
 */
class HTTPRPCTimer : public RPCTimerBase
{
public:
    HTTPRPCTimer(struct event_base* eventBase, std::function<void(void)>& func, int64_t millis) :
        ev(eventBase, false, func)
    {
        struct timeval tv;
        tv.tv_sec = millis/1000;
        tv.tv_usec = (millis%1000)*1000;
        ev.trigger(&tv);
    }
private:
    HTTPEvent ev;
};

class HTTPRPCTimerInterface : public RPCTimerInterface
{
public:
    explicit HTTPRPCTimerInterface(struct event_base* _base) : base(_base)
    {
    }
    const char* Name() override
    {
        return "HTTP";
    }
    RPCTimerBase* NewTimer(std::function<void(void)>& func, int64_t millis) override
    {
        return new HTTPRPCTimer(base, func, millis);
    }
private:
    struct event_base* base;
};


/* Pre-base64-encoded authentication token */
static std::string strRPCUserColonPass;
/* Stored RPC timer interface (for unregistration) */
static HTTPRPCTimerInterface* httpRPCTimerInterface = nullptr;

/** Check if a source IP is currently in auth-failure cooldown. */
static bool RPCAuthThrottled(const std::string& sourceIP)
{
    int maxFailures = gArgs.GetArg("-rpcmaxauthfailures", DEFAULT_RPC_MAX_AUTH_FAILURES);
    if (maxFailures <= 0)
        return false;

    int64_t now = GetTimeMillis();
    LOCK(cs_authFailures);

    auto it = mapAuthFailures.find(sourceIP);
    if (it == mapAuthFailures.end())
        return false;

    RPCAuthFailureState& state = it->second;

    // Check if currently in cooldown
    if (state.banUntil > 0 && now < state.banUntil)
        return true;

    // Cooldown expired, remove the entry so inactive sources do not remain in
    // the tracking map.
    if (state.banUntil > 0 && now >= state.banUntil) {
        mapAuthFailures.erase(it);
    }

    return false;
}

/** Record an auth-failure for the given source IP. Returns true if this
 *  failure triggered cooldown. */
static bool RPCAuthRecordFailure(const std::string& sourceIP)
{
    int maxFailures = gArgs.GetArg("-rpcmaxauthfailures", DEFAULT_RPC_MAX_AUTH_FAILURES);
    if (maxFailures <= 0)
        return false;

    int windowSeconds = gArgs.GetArg("-rpcauthfailurewindow", DEFAULT_RPC_AUTH_FAILURE_WINDOW);
    int banSeconds = gArgs.GetArg("-rpcauthfailureban", DEFAULT_RPC_AUTH_FAILURE_BAN);
    if (windowSeconds <= 0)
        windowSeconds = DEFAULT_RPC_AUTH_FAILURE_WINDOW;
    if (banSeconds <= 0)
        banSeconds = DEFAULT_RPC_AUTH_FAILURE_BAN;
    int64_t now = GetTimeMillis();
    int64_t windowMs = static_cast<int64_t>(windowSeconds) * 1000;
    int64_t banMs = static_cast<int64_t>(banSeconds) * 1000;

    LOCK(cs_authFailures);

    // Opportunistic cleanup: remove expired cooldowns and stale below-threshold
    // windows so many one-off failures from distinct sources cannot grow memory
    // forever.
    for (auto it = mapAuthFailures.begin(); it != mapAuthFailures.end(); ) {
        const RPCAuthFailureState& entry = it->second;
        const bool expiredCooldown = entry.banUntil > 0 && now >= entry.banUntil;
        const bool expiredWindow = entry.banUntil == 0 && entry.windowStart > 0 && (now - entry.windowStart) >= windowMs;
        if (expiredCooldown || expiredWindow) {
            it = mapAuthFailures.erase(it);
        } else {
            ++it;
        }
    }

    RPCAuthFailureState& state = mapAuthFailures[sourceIP];

    // If window expired, reset counter
    if (state.windowStart > 0 && (now - state.windowStart) >= windowMs) {
        state.count = 0;
        state.windowStart = 0;
    }

    if (state.windowStart == 0)
        state.windowStart = now;

    state.count++;

    if (state.count >= maxFailures) {
        state.banUntil = now + banMs;
        return true;
    }

    return false;
}

/** Clear auth-failure state for a source that just authenticated successfully. */
static void RPCAuthClearFailures(const std::string& sourceIP)
{
    LOCK(cs_authFailures);
    mapAuthFailures.erase(sourceIP);
}

static void JSONErrorReply(HTTPRequest* req, const UniValue& objError, const UniValue& id)
{
    // Send error reply from json-rpc error object
    int nStatus = HTTP_INTERNAL_SERVER_ERROR;
    int code = find_value(objError, "code").get_int();

    if (code == RPC_INVALID_REQUEST)
        nStatus = HTTP_BAD_REQUEST;
    else if (code == RPC_METHOD_NOT_FOUND)
        nStatus = HTTP_NOT_FOUND;

    std::string strReply = JSONRPCReply(NullUniValue, objError, id);

    req->WriteHeader("Content-Type", "application/json");
    req->WriteReply(nStatus, strReply);
}

//This function checks username and password against -rpcauth
//entries from config file.
static bool multiUserAuthorized(std::string strUserPass)
{    
    if (strUserPass.find(":") == std::string::npos) {
        return false;
    }
    std::string strUser = strUserPass.substr(0, strUserPass.find(":"));
    std::string strPass = strUserPass.substr(strUserPass.find(":") + 1);

    for (const std::string& strRPCAuth : gArgs.GetArgs("-rpcauth")) {
        //Search for multi-user login/pass "rpcauth" from config
        std::vector<std::string> vFields;
        boost::split(vFields, strRPCAuth, boost::is_any_of(":$"));
        if (vFields.size() != 3) {
            //Incorrect formatting in config file
            continue;
        }

        std::string strName = vFields[0];
        if (!TimingResistantEqual(strName, strUser)) {
            continue;
        }

        std::string strSalt = vFields[1];
        std::string strHash = vFields[2];

        static const unsigned int KEY_SIZE = 32;
        unsigned char out[KEY_SIZE];

        CHMAC_SHA256(reinterpret_cast<const unsigned char*>(strSalt.c_str()), strSalt.size()).Write(reinterpret_cast<const unsigned char*>(strPass.c_str()), strPass.size()).Finalize(out);
        std::vector<unsigned char> hexvec(out, out+KEY_SIZE);
        std::string strHashFromPass = HexStr(hexvec);

        if (TimingResistantEqual(strHashFromPass, strHash)) {
            return true;
        }
    }
    return false;
}

static bool RPCAuthorized(const std::string& strAuth, std::string& strAuthUsernameOut)
{
    if (strRPCUserColonPass.empty()) // Belt-and-suspenders measure if InitRPCAuthentication was not called
        return false;
    if (strAuth.substr(0, 6) != "Basic ")
        return false;
    std::string strUserPass64 = strAuth.substr(6);
    boost::trim(strUserPass64);
    std::string strUserPass = DecodeBase64(strUserPass64);

    if (strUserPass.find(":") != std::string::npos)
        strAuthUsernameOut = strUserPass.substr(0, strUserPass.find(":"));

    //Check if authorized under single-user field
    if (TimingResistantEqual(strUserPass, strRPCUserColonPass)) {
        return true;
    }
    return multiUserAuthorized(strUserPass);
}

static bool HTTPReq_JSONRPC(HTTPRequest* req, const std::string &)
{
    // JSONRPC handles only POST
    if (req->GetRequestMethod() != HTTPRequest::POST) {
        req->WriteReply(HTTP_BAD_METHOD, "JSONRPC server handles only POST requests");
        return false;
    }

    // Get source IP for auth-failure throttling
    CService peer = req->GetPeer();
    std::string sourceIP = peer.ToStringIP();

    // Check if source is in auth-failure cooldown
    if (RPCAuthThrottled(sourceIP)) {
        req->WriteReply(HTTP_FORBIDDEN);
        return false;
    }

    // Check authorization
    std::pair<bool, std::string> authHeader = req->GetHeader("authorization");
    if (!authHeader.first) {
        req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
        req->WriteReply(HTTP_UNAUTHORIZED);
        return false;
    }

    JSONRPCRequest jreq;
    if (!RPCAuthorized(authHeader.second, jreq.authUser)) {
        LogPrintf("ThreadRPCServer incorrect password attempt\n");

        /* Deter brute-forcing
           If this results in a DoS the user really
           shouldn't have their RPC port exposed. */
        MilliSleep(250);

        // Record failure and check if cooldown triggered
        bool cooldownTriggered = RPCAuthRecordFailure(sourceIP);
        if (cooldownTriggered) {
            int banSeconds = gArgs.GetArg("-rpcauthfailureban", DEFAULT_RPC_AUTH_FAILURE_BAN);
            LogPrintf("RPC auth failure cooldown triggered for %s: too many failed attempts, blocking for %d seconds\n",
                      sourceIP, banSeconds);
        }

        req->WriteHeader("WWW-Authenticate", WWW_AUTH_HEADER_DATA);
        req->WriteReply(HTTP_UNAUTHORIZED);
        return false;
    }

    // Successful auth: clear failure state for this source
    RPCAuthClearFailures(sourceIP);

    try {
        // Parse request
        UniValue valRequest;
        if (!valRequest.read(req->ReadBody()))
            throw JSONRPCError(RPC_PARSE_ERROR, "Parse error");

        // Set the URI
        jreq.URI = req->GetURI();

        std::string strReply;
        // singleton request
        if (valRequest.isObject()) {
            jreq.parse(valRequest);

            UniValue result = tableRPC.execute(jreq);

            // Send reply
            strReply = JSONRPCReply(result, NullUniValue, jreq.id);

        // array of requests
        } else if (valRequest.isArray())
            strReply = JSONRPCExecBatch(jreq, valRequest.get_array());
        else
            throw JSONRPCError(RPC_PARSE_ERROR, "Top-level object parse error");

        req->WriteHeader("Content-Type", "application/json");
        req->WriteReply(HTTP_OK, strReply);
    } catch (const UniValue& objError) {
        JSONErrorReply(req, objError, jreq.id);
        return false;
    } catch (const std::exception& e) {
        JSONErrorReply(req, JSONRPCError(RPC_PARSE_ERROR, e.what()), jreq.id);
        return false;
    }
    return true;
}

static bool InitRPCAuthentication()
{
    if (gArgs.GetArg("-rpcpassword", "") == "")
    {
        LogPrintf("No rpcpassword set - using random cookie authentication\n");
        if (!GenerateAuthCookie(&strRPCUserColonPass)) {
            uiInterface.ThreadSafeMessageBox(
                _("Error: A fatal internal error occurred, see debug.log for details"), // Same message as AbortNode
                "", CClientUIInterface::MSG_ERROR);
            return false;
        }
    } else {
        LogPrintf("Config options rpcuser and rpcpassword will soon be deprecated. Locally-run instances may remove rpcuser to use cookie-based auth, or may be replaced with rpcauth. Please see share/rpcuser for rpcauth auth generation.\n");
        strRPCUserColonPass = gArgs.GetArg("-rpcuser", "") + ":" + gArgs.GetArg("-rpcpassword", "");
    }
    return true;
}

bool StartHTTPRPC()
{
    LogPrint(BCLog::RPC, "Starting HTTP RPC server\n");
    if (!InitRPCAuthentication())
        return false;

    RegisterHTTPHandler("/", true, HTTPReq_JSONRPC);
#ifdef ENABLE_WALLET
    // ifdef can be removed once we switch to better endpoint support and API versioning
    RegisterHTTPHandler("/wallet/", false, HTTPReq_JSONRPC);
#endif
    assert(EventBase());
    httpRPCTimerInterface = new HTTPRPCTimerInterface(EventBase());
    RPCSetTimerInterface(httpRPCTimerInterface);
    return true;
}

void InterruptHTTPRPC()
{
    LogPrint(BCLog::RPC, "Interrupting HTTP RPC server\n");
}

void StopHTTPRPC()
{
    LogPrint(BCLog::RPC, "Stopping HTTP RPC server\n");
    UnregisterHTTPHandler("/", true);
    if (httpRPCTimerInterface) {
        RPCUnsetTimerInterface(httpRPCTimerInterface);
        delete httpRPCTimerInterface;
        httpRPCTimerInterface = nullptr;
    }
}
