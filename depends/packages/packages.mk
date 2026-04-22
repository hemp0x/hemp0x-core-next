packages:=boost openssl libevent zeromq
native_packages := native_ccache native_b2

wallet_packages=bdb

upnp_packages=miniupnpc

darwin_native_packages = native_biplist native_ds_store native_mac_alias

ifneq ($(build_os),darwin)
darwin_native_packages += native_cctools native_libtapi native_cdrkit native_libdmg-hfsplus native_clang
endif
