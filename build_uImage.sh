#/bin/sh
make meson_stv_mbx_mc_initrd_defconfig
make -j4
make modules -j4
mkdir -p ramdisk/root/boot
cp -vf drivers/amlogic/ump/ump.ko ramdisk/root/boot/
cp -vf drivers/amlogic/ump/ump.ko ramdisk/root_release/boot/
mkdir -p ramdisk/root_release/boot
cp -vf drivers/amlogic/mali/mali.ko ramdisk/root/boot/
cp -vf drivers/amlogic/mali/mali.ko ramdisk/root_release/boot/
make uImage -j4
