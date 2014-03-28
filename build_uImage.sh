#/bin/sh
make meson_stv_mbx_mc_initrd_defconfig
make -j4
make modules -j4
mkdir -p ramdisk/root/boot
cp -vf drivers/amlogic/ump/ump.ko ramdisk/root/boot/
cp -vf drivers/amlogic/mali/mali.ko ramdisk/root/boot/
cp -vf drivers/amlogic/wifi/rtl8xxx_EU/8188eu.ko ramdisk/root/boot/
cp -vf drivers/amlogic/wifi/rtl8xxx_CU/8192cu.ko ramdisk/root/boot/
mkdir -p ramdisk/root_release/boot
cp -vf drivers/amlogic/ump/ump.ko ramdisk/root_release/boot/
cp -vf drivers/amlogic/mali/mali.ko ramdisk/root_release/boot/
cp -vf drivers/amlogic/wifi/rtl8xxx_EU/8188eu.ko ramdisk/root_release/boot/
cp -vf drivers/amlogic/wifi/rtl8xxx_CU/8192cu.ko ramdisk/root_release/boot/
make uImage -j4
cp -rf arch/arm/boot/uImage ../out/target/product/xiosm3/
