package=miniupnpc
$(package)_version=2.3.3
$(package)_download_path=https://miniupnp.tuxfamily.org/files
$(package)_file_name=$(package)-$($(package)_version).tar.gz
$(package)_sha256_hash=d52a0afa614ad6c088cc9ddff1ae7d29c8c595ac5fdd321170a05f41e634bd1a

define $(package)_set_vars
$(package)_build_opts=CC="$($(package)_cc)"
$(package)_build_opts_darwin=OS=Darwin LIBTOOL="$($(package)_libtool)"
$(package)_build_opts_mingw32=-f Makefile.mingw
$(package)_build_env+=CFLAGS="$($(package)_cflags) $($(package)_cppflags)" AR="$($(package)_ar)"
endef

define $(package)_preprocess_cmds
  mkdir -p dll
endef

define $(package)_build_cmds
	$(MAKE) $($(package)_build_opts) $(if $(filter mingw32,$(host_os)),libminiupnpc.a,build/libminiupnpc.a)
endef

define $(package)_stage_cmds
	mkdir -p $($(package)_staging_prefix_dir)/include/miniupnpc $($(package)_staging_prefix_dir)/lib &&\
	install include/*.h $($(package)_staging_prefix_dir)/include/miniupnpc &&\
	if test "$(host_os)" = "mingw32"; then \
	  install libminiupnpc.a $($(package)_staging_prefix_dir)/lib; \
	else \
	  install build/libminiupnpc.a $($(package)_staging_prefix_dir)/lib; \
	fi
endef
