#!/bin/sh

echo "step : 1  ready to config"
make  atlas7_rn4yd6_defconfig

echo "step : 2  ready to make dtb"
make atlas7-rn4yd6.dtb

echo "step : 3  make zImage -j123"
make -j123


echo ".....done......"

echo "-----GPL-----"
echo "-----AUTHOR-----okest-----"
echo "-----AUTHOR-----koberx-----"
echo "------THANKS-----"

sleep 1s
echo "copy to ~/share/swap"

cp arch/arm/boot/dts/atlas7-rn4yd6.dtb   ~/share/swap
mv ~/share/swap/atlas7-rn4yd6.dtb ~/share/swap/dtb
cp arch/arm/boot/zImage  ~/share/swap

