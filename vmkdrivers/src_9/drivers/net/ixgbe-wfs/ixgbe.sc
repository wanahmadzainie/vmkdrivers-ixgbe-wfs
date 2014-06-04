
'''
Driver definition for ixgbe driver.

When developing a driver for release through the async program:
 * set "vendor" to the name of the vendor providing the driver
 * set "vendorEmail" to the contact e-mail address for the vendor
 * set "version" to the version contained in the driver source code
 * set "release" to 1
 
When porting an updated driver that has never been released through
the async program:
 * set "version" to the version contained in the driver source code
 * set "release" to 1
 
When bringing an async driver into an ESX release:
 * leave "version" as is from the async release
 * increment "release"
 * set "vendor" to 'vmware'
 * set "vendorEmail" to the VMware contact e-mail address
 
For all other driver fixes, just increment "release".
'''

driverName = 'ixgbe'
driverVersion = '3.21.4'

defineVmkDriver(
   name=driverName,
   version=driverVersion,
   description='Intel(R) 10 Gigabit Ethernet Network Driver', 
   driverType='net',
   files=[('drivers/net/ixgbe',
           Split('''
			ixgbe_82598.c
			ixgbe_82599.c
			ixgbe_x540.c
			ixgbe_mbx.c
			ixgbe_api.c
			ixgbe_cna.c
			ixgbe_common.c
			ixgbe_dcb_82598.c
			ixgbe_dcb_82599.c
			ixgbe_dcb_nl.c
			ixgbe_dcb.c
			ixgbe_ethtool.c
			ixgbe_lib.c
			ixgbe_fcoe.c
			ixgbe_main.c
			ixgbe_param.c
			ixgbe_phy.c
			ixgbe_sriov.c
			ixgbe_vmdq.c
			ixgbe_procfs.c
			kcompat.c
			kcompat_ethtool.c
                 '''))],
   appends=dict(CPPDEFINES={
		'VMKLINUX_MODULE_HEAP_ANY_MEM':None,
		'LINUX_MODULE_SKB_HEAP_MAX':"36*1024*1024",
		'IXGBE_MQ':None,
		'HAVE_DCBNL_OPS_GETAPP':None,
		'VMK_DEVKIT_USES_BINARY_COMPATIBLE_APIS':None,
		'NET_DRIVER':None,
		'CONFIG_PROC_FS':None,
                'DRIVER_IXGBE':None,
		'ESX_REV':'5.0',
                'ESX_DDK_BUILD_REV':'472560',
		'IXGBE_ESX_CNA':None,
		'CONFIG_PCI_MSI':None,
		'IXGBE_VMDQ':None,
		'IXGBE_NO_LRO':None,
		'CONFIG_INET_LRO':None,
		'CONFIG_NETDEVICES_MULTIQUEUE':None,
		'CONFIG_PCI_IOV':None,
		'IFLA_VF_MAX':None,
		'CONFIG_DCB':None,
		'CONFIG_FCOE':None,
		'__VMKLNX__':None,
		'__VMKNETDDI_QUEUEOPS__':None,
                },
                CCFLAGS=['-w -Wno-error'],
               ),
   heapinfo=('1024*100', '1024*4096'),
   vendor = 'Intel',
   vendorEmail='e1000-devel@lists.sourceforge.net',
   statelessReady='true',
   bulletinId='%s-%s' % (driverName, driverVersion),
   bulletinSummary='%s driver for ESX' % (driverName),
   bulletinKBUrl='http://support.intel.com',
   bulletinVendorCode='INT',
)

