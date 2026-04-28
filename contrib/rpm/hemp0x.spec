%define bdbv 4.8.30
%global selinux_variants mls strict targeted

%define buildargs

Name:		hemp0x
Version:	0.12.0
Release:	2%{?dist}
Summary:	Peer to Peer Cryptographic Currency

Group:		Applications/System
License:	MIT
URL:		https://hemp0x.org/
Source0:	https://hemp0x.org/bin/hemp0x-core-%{version}/hemp0x-%{version}.tar.gz
Source1:	http://download.oracle.com/berkeley-db/db-%{bdbv}.NC.tar.gz

Source10:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/contrib/debian/examples/hemp0x.conf

#man pages
Source20:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/doc/man/hemp0xd.1
Source21:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/doc/man/hemp0x-cli.1

#selinux
Source30:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/contrib/rpm/hemp0x.te
# Source31 - what about hemp0x-tx and bench_hemp0x ???
Source31:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/contrib/rpm/hemp0x.fc
Source32:	https://raw.githubusercontent.com/hemp0x/hemp0x/v%{version}/contrib/rpm/hemp0x.if

Source100:	https://upload.wikimedia.org/wikipedia/commons/4/46/Hemp0x.svg

%if 0%{?_use_libressl:1}
BuildRequires:	libressl-devel
%else
BuildRequires:	openssl-devel
%endif
BuildRequires:	boost-devel
BuildRequires:	miniupnpc-devel
BuildRequires:	autoconf automake libtool
BuildRequires:	libevent-devel


Patch0:		hemp0x-0.12.0-libressl.patch


%description
Hemp0x is a digital cryptographic currency that uses peer-to-peer technology to
operate with no central authority or banks; managing transactions and the
issuing of hemp0xs is carried out collectively by the network.

%package libs
Summary:	Hemp0x shared libraries
Group:		System Environment/Libraries

%description libs
This package provides the hemp0xconsensus shared libraries. These libraries
may be used by third party software to provide consensus verification
functionality.

Unless you know need this package, you probably do not.

%package devel
Summary:	Development files for hemp0x
Group:		Development/Libraries
Requires:	%{name}-libs = %{version}-%{release}

%description devel
This package contains the header files and static library for the
hemp0xconsensus shared library. If you are developing or compiling software
that wants to link against that library, then you need this package installed.

Most people do not need this package installed.

%package server
Summary:	The hemp0x daemon
Group:		System Environment/Daemons
Requires:	hemp0x-utils = %{version}-%{release}
Requires:	selinux-policy policycoreutils-python
Requires(pre):	shadow-utils
Requires(post):	%{_sbindir}/semodule %{_sbindir}/restorecon %{_sbindir}/fixfiles %{_sbindir}/sestatus
Requires(postun):	%{_sbindir}/semodule %{_sbindir}/restorecon %{_sbindir}/fixfiles %{_sbindir}/sestatus
BuildRequires:	systemd
BuildRequires:	checkpolicy
BuildRequires:	%{_datadir}/selinux/devel/Makefile

%description server
This package provides a stand-alone hemp0x-core daemon. For most users, this
package is only needed if they need a full-node without the graphical client.

Some third party wallet software will want this package to provide the actual
hemp0x-core node they use to connect to the network.

If you use the graphical hemp0x-core client then you almost certainly do not
need this package.

%package utils
Summary:	Hemp0x utilities
Group:		Applications/System

%description utils
This package provides several command line utilities for interacting with a
hemp0x-core daemon.

The hemp0x-cli utility allows you to communicate and control a hemp0x daemon
over RPC, the hemp0x-tx utility allows you to create a custom transaction, and
the bench_hemp0x utility can be used to perform some benchmarks.

This package contains utilities needed by the hemp0x-server package.


%prep
%setup -q
%patch0 -p1 -b .libressl
cp -p %{SOURCE10} ./hemp0x.conf.example
tar -zxf %{SOURCE1}
cp -p db-%{bdbv}.NC/LICENSE ./db-%{bdbv}.NC-LICENSE
mkdir db4 SELinux
cp -p %{SOURCE30} %{SOURCE31} %{SOURCE32} SELinux/


%build
CWD=`pwd`
cd db-%{bdbv}.NC/build_unix/
../dist/configure --enable-cxx --disable-shared --with-pic --prefix=${CWD}/db4
make install
cd ../..

./autogen.sh
%configure LDFLAGS="-L${CWD}/db4/lib/" CPPFLAGS="-I${CWD}/db4/include/" --with-miniupnpc --enable-glibc-back-compat %{buildargs}
make %{?_smp_mflags}

pushd SELinux
for selinuxvariant in %{selinux_variants}; do
	make NAME=${selinuxvariant} -f %{_datadir}/selinux/devel/Makefile
	mv hemp0x.pp hemp0x.pp.${selinuxvariant}
	make NAME=${selinuxvariant} -f %{_datadir}/selinux/devel/Makefile clean
done
popd


%install
make install DESTDIR=%{buildroot}

mkdir -p -m755 %{buildroot}%{_sbindir}
mv %{buildroot}%{_bindir}/hemp0xd %{buildroot}%{_sbindir}/hemp0xd

# systemd stuff
mkdir -p %{buildroot}%{_tmpfilesdir}
cat <<EOF > %{buildroot}%{_tmpfilesdir}/hemp0x.conf
d /run/hemp0xd 0750 hemp0x hemp0x -
EOF
touch -a -m -t 201504280000 %{buildroot}%{_tmpfilesdir}/hemp0x.conf

mkdir -p %{buildroot}%{_sysconfdir}/sysconfig
cat <<EOF > %{buildroot}%{_sysconfdir}/sysconfig/hemp0x
# Provide options to the hemp0x daemon here, for example
# OPTIONS="-testnet -disable-wallet"

OPTIONS=""

# System service defaults.
# Don't change these unless you know what you're doing.
CONFIG_FILE="%{_sysconfdir}/hemp0x/hemp0x.conf"
DATA_DIR="%{_localstatedir}/lib/hemp0x"
PID_FILE="/run/hemp0xd/hemp0xd.pid"
EOF
touch -a -m -t 201504280000 %{buildroot}%{_sysconfdir}/sysconfig/hemp0x

mkdir -p %{buildroot}%{_unitdir}
cat <<EOF > %{buildroot}%{_unitdir}/hemp0x.service
[Unit]
Description=Hemp0x daemon
After=syslog.target network.target

[Service]
Type=forking
ExecStart=%{_sbindir}/hemp0xd -daemon -conf=\${CONFIG_FILE} -datadir=\${DATA_DIR} -pid=\${PID_FILE} \$OPTIONS
EnvironmentFile=%{_sysconfdir}/sysconfig/hemp0x
User=hemp0x
Group=hemp0x

Restart=on-failure
PrivateTmp=true
TimeoutStopSec=120
TimeoutStartSec=60
StartLimitInterval=240
StartLimitBurst=5

[Install]
WantedBy=multi-user.target
EOF
touch -a -m -t 201504280000 %{buildroot}%{_unitdir}/hemp0x.service
#end systemd stuff

mkdir %{buildroot}%{_sysconfdir}/hemp0x
mkdir -p %{buildroot}%{_localstatedir}/lib/hemp0x

#SELinux
for selinuxvariant in %{selinux_variants}; do
	install -d %{buildroot}%{_datadir}/selinux/${selinuxvariant}
	install -p -m 644 SELinux/hemp0x.pp.${selinuxvariant} %{buildroot}%{_datadir}/selinux/${selinuxvariant}/hemp0x.pp
done

# man pages
install -D -p %{SOURCE20} %{buildroot}%{_mandir}/man1/hemp0xd.1
install -p %{SOURCE21} %{buildroot}%{_mandir}/man1/hemp0x-cli.1

# nuke these, we do extensive testing of binaries in %%check before packaging
rm -f %{buildroot}%{_bindir}/test_*

%check
make check
srcdir=src test/hemp0x-util-test.py
test/functional/test_runner.py --extended

%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig

%pre server
getent group hemp0x >/dev/null || groupadd -r hemp0x
getent passwd hemp0x >/dev/null ||
	useradd -r -g hemp0x -d /var/lib/hemp0x -s /sbin/nologin \
	-c "Hemp0x wallet server" hemp0x
exit 0

%post server
%systemd_post hemp0x.service
# SELinux
if [ `%{_sbindir}/sestatus |grep -c "disabled"` -eq 0 ]; then
for selinuxvariant in %{selinux_variants}; do
	%{_sbindir}/semodule -s ${selinuxvariant} -i %{_datadir}/selinux/${selinuxvariant}/hemp0x.pp &> /dev/null || :
done
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 8766
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 42069
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 18766
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 18767
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 18443
%{_sbindir}/semanage port -a -t hemp0x_port_t -p tcp 18444
%{_sbindir}/fixfiles -R hemp0x-server restore &> /dev/null || :
%{_sbindir}/restorecon -R %{_localstatedir}/lib/hemp0x || :
fi

%posttrans server
%{_bindir}/systemd-tmpfiles --create

%preun server
%systemd_preun hemp0x.service

%postun server
%systemd_postun hemp0x.service
# SELinux
if [ $1 -eq 0 ]; then
	if [ `%{_sbindir}/sestatus |grep -c "disabled"` -eq 0 ]; then
	%{_sbindir}/semanage port -d -p tcp 8766
	%{_sbindir}/semanage port -d -p tcp 42069
	%{_sbindir}/semanage port -d -p tcp 18766
	%{_sbindir}/semanage port -d -p tcp 18767
	%{_sbindir}/semanage port -d -p tcp 18443
	%{_sbindir}/semanage port -d -p tcp 18444
	for selinuxvariant in %{selinux_variants}; do
		%{_sbindir}/semodule -s ${selinuxvariant} -r hemp0x &> /dev/null || :
	done
	%{_sbindir}/fixfiles -R hemp0x-server restore &> /dev/null || :
	[ -d %{_localstatedir}/lib/hemp0x ] && \
		%{_sbindir}/restorecon -R %{_localstatedir}/lib/hemp0x &> /dev/null || :
	fi
fi

%clean
rm -rf %{buildroot}

%files libs
%defattr(-,root,root,-)
%license COPYING
%doc COPYING doc/README.md doc/shared-libraries.md
%{_libdir}/lib*.so.*

%files devel
%defattr(-,root,root,-)
%license COPYING
%doc COPYING doc/README.md doc/developer-notes.md doc/shared-libraries.md
%attr(0644,root,root) %{_includedir}/*.h
%{_libdir}/*.so
%{_libdir}/*.a
%{_libdir}/*.la
%attr(0644,root,root) %{_libdir}/pkgconfig/*.pc

%files server
%defattr(-,root,root,-)
%license COPYING db-%{bdbv}.NC-LICENSE
%doc COPYING hemp0x.conf.example doc/README.md doc/REST-interface.md doc/bips.md doc/dnsseed-policy.md doc/files.md doc/reduce-traffic.md doc/release-notes.md doc/tor.md
%attr(0755,root,root) %{_sbindir}/hemp0xd
%attr(0644,root,root) %{_tmpfilesdir}/hemp0x.conf
%attr(0644,root,root) %{_unitdir}/hemp0x.service
%dir %attr(0750,hemp0x,hemp0x) %{_sysconfdir}/hemp0x
%dir %attr(0750,hemp0x,hemp0x) %{_localstatedir}/lib/hemp0x
%config(noreplace) %attr(0600,root,root) %{_sysconfdir}/sysconfig/hemp0x
%attr(0644,root,root) %{_datadir}/selinux/*/*.pp
%attr(0644,root,root) %{_mandir}/man1/hemp0xd.1*

%files utils
%defattr(-,root,root,-)
%license COPYING
%doc COPYING hemp0x.conf.example doc/README.md
%attr(0755,root,root) %{_bindir}/hemp0x-cli
%attr(0755,root,root) %{_bindir}/hemp0x-tx
%attr(0755,root,root) %{_bindir}/bench_hemp0x
%attr(0644,root,root) %{_mandir}/man1/hemp0x-cli.1*



%changelog
* Wed Feb 24 2016 Alice Wonder <buildmaster@librelamp.com> - 0.12.0-1
- Initial spec file for 0.12.0 release

# This spec file is written from scratch but a lot of the packaging decisions are directly
# based upon the 0.11.2 package spec file from https://www.ringingliberty.com/hemp0x/
