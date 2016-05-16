Name:       capi-network-wifi-direct
Summary:    Network WiFi-Direct Library
Version:    1.2.61
Release:    1
Group:      Network & Connectivity/API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
Requires(post): /sbin/ldconfig
Requires(postun): /sbin/ldconfig
BuildRequires:  pkgconfig(gio-2.0)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  pkgconfig(capi-system-info)
BuildRequires:  cmake
BuildRequires:  gettext-devel

%description
Network WiFi-Direct library in Tizen CAPI (Shared Library)

%package devel
Summary:    Network WiFi-Direct Library (Development)
Group:      Development/Libraries
Requires:   %{name} = %{version}-%{release}
Requires: capi-base-common-devel

%description devel
Network WiFi-Direct library in Tizen CAPI (Shared Library) (Development)

#%package -n test-wifi-direct
#Summary:    Test Application for Wi-Fi Direct
#Group:      Network & Connectivity/Testing
#Requires:   %{name} = %{version}-%{release}

#%description -n test-wifi-direct
#Test Application for Wi-Fi Direct Framework

%prep
%setup -q

%ifarch %{arm}
export ARCH=arm
%else
export ARCH=i586
%endif

chmod 644 %{SOURCE0}

%build

export CFLAGS="$CFLAGS -DTIZEN_DEBUG_ENABLE"
export CXXFLAGS="$CXXFLAGS -DTIZEN_DEBUG_ENABLE -fprofile-arcs -ftest-coverage"
export FFLAGS="$FFLAGS -DTIZEN_DEBUG_ENABLE"
export LDFLAGS="-lgcov"

MAJORVER=`echo %{version} | awk 'BEGIN {FS="."}{print $1}'`


cmake -DCMAKE_INSTALL_PREFIX=%{_prefix} \
%if "%{profile}" == "common"
	-DTIZEN_FEATURE_SERVICE_DISCOVERY=1 \
	-DTIZEN_FEATURE_WIFI_DISPLAY=1 \
%else
%if "%{profile}" == "wearable"
	-DTIZEN_FEATURE_SERVICE_DISCOVERY=0 \
	-DTIZEN_FEATURE_WIFI_DISPLAY=0 \
%else
%if "%{profile}" == "mobile"
	-DTIZEN_FEATURE_SERVICE_DISCOVERY=1 \
	-DTIZEN_FEATURE_WIFI_DISPLAY=1 \
%else
%if "%{profile}" == "tv"
	-DTIZEN_TV=1 \
	-DTIZEN_FEATURE_SERVICE_DISCOVERY=1 \
	-DTIZEN_FEATURE_WIFI_DISPLAY=1 \
%endif
%endif
%endif
%endif
	. -DVERSION=%{version} -DMAJORVERSION=${MAJORVER} -DCMAKE_LIB_DIR=%{_libdir}
make %{?jobs:-j%jobs}
%install
rm -rf %{buildroot}
%make_install

mkdir -p %{buildroot}/usr/share/license
cp %{_builddir}/%{buildsubdir}/LICENSE.APLv2 %{buildroot}/usr/share/license/%{name}

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig


%files
%manifest capi-network-wifi-direct.manifest
%defattr(-,root,root,-)
%{_libdir}/libwifi-direct.so*
/usr/share/license/%{name}

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/capi-network-wifi-direct.pc
%{_includedir}/wifi-direct/wifi-direct.h
%{_includedir}/wifi-direct/wifi-direct-internal.h
%{_libdir}/libwifi-direct.so

#%files -n test-wifi-direct
#%manifest test-wifi-direct.manifest
#%defattr(-,app,app,-)
#%attr(755,-,-) %{_bindir}/test-wifi-direct
