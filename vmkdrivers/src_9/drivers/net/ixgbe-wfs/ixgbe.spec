Summary: Intel(R) 10GbE PCI Express Ethernet Connection driver for VMware ESX Server 5.0
Name: VMware-esx-drivers-net-ixgbe
Version: 500.3.21.4
Release: 472560
Source: %{name}-%{version}-%{release}.tar.gz
Vendor: Intel Corporation
License: GPL
Group: System Environment/Kernel
Obsoletes: VMware-esx-drivers
Requires: vmware-hwdata >= 1.00
Requires: DriverAPI-8.0
Provides: %{name}
URL: http://www.intel.com/network/connectivity/products/server_adapters.htm
BuildRoot: %{CURRENTDIR}/rpm

%description
This package contains the VMware ESX 5.0 driver for the Intel(R) 10GbE PCI Express Family of Server Adapters.

%prep

%install

%pre

%post
/usr/sbin/esxcfg-pciid -q
/usr/sbin/esxcfg-boot --sched-rdbuild

%preun

%postun
/usr/sbin/esxcfg-pciid -q
/usr/sbin/esxcfg-boot --sched-rdbuild

%files
%attr(-, root, root) /usr/lib/vmware/vmkmod
%attr(-, root, root) /usr/lib/vmware-debug/vmkmod
%attr(-, root, root) /etc/vmware/pciid
