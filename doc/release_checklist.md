**Steps to be performed before any Pull Request is accepted into the active development branch**

  1. Check PROTOCOL_VERSION in the following location: src/version.h

  2. Check Hemp0x Core version in the following locations: configure.ac, src/version.h

  3. All unit and functional tests pass

  4. Check Hemp0x Commander (desktop wallet) for any block serialization or RPC changes

  5. Check Hemp0x WebCom for any block serialization or RPC changes

  6. Build release notes for all new features and bug fixes

**If hard fork:**

   1. Coordinate with all exchanges, pools, and wallet providers on migration timeline

**Post Release :**

  1. Update hemp0x.com with correct popup version
  
  2. Update hemp0x.com with correct release download urls for validated
     platforms (Windows, Linux). Community builds for other platforms can
     be listed separately.

**Build Process**

  1. Verify that the release build doesn't say ***dirty*** in the commit message
