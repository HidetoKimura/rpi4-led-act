# rpi4-led-act

# build kernel

In pc
~~~~
sudo apt install crossbuild-essential-arm64
git clone --depth=1 --branch rpi-6.1.y https://github.com/raspberrypi/linux
cd linux
export KERNEL=kernel8
export ARCH=arm64
export CROSS_COMPILE=aarch64-linux-gnu- 
export INSTALL_MOD_PATH=../
make bcm2711_defconfig
make -j4 Image modules dtbs
make modules_install
~~~~

# build LKM

In pc
~~~~
export KERNEL_SRC=<kernel path>
git clone https://github.com/HidetoKimura/rpi4-led-act.git
cd rpi4-led-act
make
scp  rpi4_led_act_driver.ko rpi@raspberrypi.local:
~~~~

In rpi
~~~~
insmod rpi4_led_act_driver.ko 
rmmod rpi4_led_act_driver 
~~~~

# build DTB

In pc
~~~~
cd dts
dtc -O dtb -o ledact.dtbo ledact.dts 
scp ledact.dtbo rpi@raspberrypi.local:
~~~~

In rpi
~~~~
cp ledact.dtbo /boot/overlay/
sync
~~~~

# modify /boot/config.txt

In rpi
~~~~
dtoverlay=ledact
~~~~
