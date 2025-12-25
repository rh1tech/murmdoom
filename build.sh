rm -rf ./build
mkdir build
cd build
cmake -DPICO_PLATFORM=rp2350 -DUSB_HID_ENABLED=1 ..
make -j4
#picotool load pop2350.elf -f && picotool reboot
#sleep 2 && screen /dev/tty.usbmodem141201

