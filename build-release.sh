# Create a stage
rm -rf ./stage && mkdir ./stage

# Extract vib
unzip ixgbe-3.21.4-1710200.zip Intel_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560.vib

# Populate stage
vibauthor -e -v Intel_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560.vib -o stage
rm -rf Intel_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560.vib

# Copy driver module, ixgbe
if [[ ! -f BLD/build/vmkdriver-ixgbe-wfs-CUR/release/vmkernel64/ixgbe ]]; then
	echo "ixgbe not found. Run ./build-ixgbe-wfs.sh first."
	rm -rf ./stage
	exit 0
fi
cp BLD/build/vmkdriver-ixgbe-wfs-CUR/release/vmkernel64/ixgbe ./stage/payloads/net-ixgb/usr/lib/vmware/vmkmod/ixgbe

# Generate packages
cp descriptor.xml ./stage
vibauthor -C -t ./stage \
-v ./Power-All_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560-wfs.vib \
-O ./ixgbe-3.21.4-offline_bundle-1710200-wfs.zip
zip ./ixgbe-3.21.4-1710200-wfs.zip \
./Power-All_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560-wfs.vib \
./ixgbe-3.21.4-offline_bundle-1710200-wfs.zip

# Clean up
rm -rf ./stage
rm -rf \
./Power-All_bootbank_net-ixgbe_3.21.4-1OEM.500.0.0.472560-wfs.vib \
./ixgbe-3.21.4-offline_bundle-1710200-wfs.zip
