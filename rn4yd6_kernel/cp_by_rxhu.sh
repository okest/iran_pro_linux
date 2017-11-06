#!/bin/sh
make 
make atlas7-rn4yd6.dtb
echo "begin copy"
cp arch/arm/boot/dts/atlas7-rn4yd6.dtb   ~/share/swap
mv ~/share/swap/atlas7-rn4yd6.dtb ~/share/swap/dtb
cp arch/arm/boot/zImage  ~/share/swap
echo "end copy"
