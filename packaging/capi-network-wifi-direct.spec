Name:       capi-network-wifi-direct
Summary:    Network WiFi-Direct library in Tizen CAPI
Version:    0.0.4
Release:    3
Group:      API/C API
License:    Apache-2.0
Source0:    %{name}-%{version}.tar.gz
BuildRequires:  pkgconfig(dbus-glib-1)
BuildRequires:  pkgconfig(dlog)
BuildRequires:  pkgconfig(vconf)
BuildRequires:  pkgconfig(capi-base-common)
BuildRequires:  cmake

%description
WiFi-Direct library (Shared Library)

%package devel 
Summary:    WiFi-Direct library (Shared Library) (Developement)
Group:      Development/API
Requires: capi-base-common-devel
BuildRequires:  pkgconfig(wifi-direct)
%description devel
WiFi-Direct library (Shared Library) (Developement)

%prep
%setup -q

%build
%cmake . 
make %{?jobs:-j%jobs}

%install
%make_install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%manifest capi-network-wifi-direct.manifest

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/*.pc
%{_includedir}/*/*.h

