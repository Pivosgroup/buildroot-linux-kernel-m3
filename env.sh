export ARCH=arm
export CROSS_COMPILE=/root/buildroot-AmLogic/output/external-toolchain/bin/arm-none-linux-gnueabi-
#export CROSS_COMPILE=arm-none-linux-gnueabi-
make clean
make meson_reff16_gbox_defconfig
make menuconfig
#make vmlinux

TOP=${PWD}
export MKIMAGE=${TOP}/arch/arm/boot/mkimage
make uImage
#make modules
