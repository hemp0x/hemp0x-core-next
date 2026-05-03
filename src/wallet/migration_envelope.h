// Copyright (c) 2021-2026 Hemp0x developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef HEMP0X_WALLET_MIGRATION_ENVELOPE_H
#define HEMP0X_WALLET_MIGRATION_ENVELOPE_H

#include <boost/filesystem.hpp>

#include <stdint.h>

#include <string>
#include <vector>

struct MigrationDerivationProfileEntry {
    std::string profile_id;
    int purpose = -1;
    int coin_type = 0;
    std::string address_type;
};

struct MigrationEnvelopeValidation {
    bool valid = false;
    bool restorable = false;
    int envelope_version = 0;
    std::string schema_identifier;
    std::string source_client_version;
    int64_t exported_at = 0;
    std::string network;
    int coin_type_bip44 = 0;
    bool matches_current_chain = false;
    bool private_keys_included = false;
    bool hd_enabled = false;
    bool bip44_enabled = false;
    bool mnemonic_available = false;
    bool private_present = false;
    bool private_encrypted = false;
    bool decryption_successful = false;
    std::string kdf_profile;
    int64_t kdf_iterations = 0;
    std::string cipher_profile;
    std::string aad_profile;
    std::string payload_format;
    std::string wallet_type;
    int payload_coin_type = 0;
    int account = 0;
    std::string mnemonic_language;
    int mnemonic_word_count = 0;
    std::string derivation_profile_id;
    int64_t external_count_hint = 0;
    int64_t change_count_hint = 0;
    std::string restorable_reason;
    std::vector<MigrationDerivationProfileEntry> derivation_profiles;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

bool ValidateMigrationEnvelopeFile(
    const boost::filesystem::path& path,
    const std::string& passphrase,
    MigrationEnvelopeValidation& out,
    std::string& rpc_error);

#endif // HEMP0X_WALLET_MIGRATION_ENVELOPE_H
