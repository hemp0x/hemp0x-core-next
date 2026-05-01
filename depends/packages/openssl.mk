package=openssl
$(package)_version=3.5.6
$(package)_download_path=https://www.openssl.org/source
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=deae7c80cba99c4b4f940ecadb3c3338b13cb77418409238e57d7f31f2a3b736

define $(package)_set_vars
# OpenSSL 3.x removed many algorithm-specific no-* options (algorithms are
# modular/selectable via providers). Below we keep only the options known to
# be valid in OpenSSL 3.5.x.
$(package)_config_opts=--prefix=$(host_prefix) --openssldir=$(host_prefix)/etc/openssl --libdir=lib
$(package)_config_opts+=no-dso
$(package)_config_opts+=no-sctp
$(package)_config_opts+=no-shared
$(package)_config_opts+=no-ssl-trace
$(package)_config_opts+=no-tests
$(package)_config_opts+=no-zlib
$(package)_config_opts+=no-zlib-dynamic
$(package)_config_opts+=$($(package)_cflags) $($(package)_cppflags)
$(package)_config_opts_linux=-fPIC -Wa,--noexecstack
$(package)_config_opts_x86_64_linux=linux-x86_64
$(package)_config_opts_i686_linux=linux-generic32
$(package)_config_opts_arm_linux=linux-generic32
$(package)_config_opts_aarch64_linux=linux-generic64
$(package)_config_opts_mipsel_linux=linux-generic32
$(package)_config_opts_mips_linux=linux-generic32
$(package)_config_opts_powerpc_linux=linux-generic32
$(package)_config_opts_x86_64_darwin=AR=$(host_prefix)/native/bin/x86_64-apple-darwin14-ar
$(package)_config_opts_x86_64_darwin+=RANLIB=$(host_prefix)/native/bin/x86_64-apple-darwin14-ranlib
$(package)_config_opts_x86_64_darwin+=darwin64-x86_64-cc

$(package)_config_opts_x86_64_mingw32=mingw64
$(package)_config_opts_i686_mingw32=mingw32

ifneq (,$(findstring clang,$($(package)_cxx)))
$(package)_toolset_$(host_os)=clang
else
$(package)_toolset_$(host_os)=gcc
endif
endef

define $(package)_preprocess_cmds
endef

define $(package)_config_cmds
  CC="$($(package)_cc)" \
  CXXFLAGS="$($(package)_ccflags)" \
  WINDRES="$(host_WINDRES)" \
  ./Configure $($(package)_config_opts)
endef

define $(package)_build_cmds
  sed -i.old 's/^INSTALL_PROGRAMS=.*/INSTALL_PROGRAMS=/g' Makefile && \
  $(MAKE) -j1 build_libs
endef

define $(package)_stage_cmds
  $(MAKE) DESTDIR=$($(package)_staging_dir) -j1 install_dev
endef

define $(package)_postprocess_cmds
  rm -rf share bin etc
endef
