# rpi4-led-act

# build klernel

In pc
~~~~
git clone --depth=1 --branch rpi-6.1.y https://github.com/raspberrypi/linux
sudo apt install crossbuild-essential-arm64
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

- make all

In pc
~~~~
export KERNEL_SRC=<kernel path>
git clone https://github.com/HidetoKimura/rpi4-led-act.git
cd rpi-led-act
make
scp  rpi4_led_act_driver.ko rpi@raspberrypi.local
~~~~

In rpi
~~~~
insmod rpi4_led_act_driver.ko 
rmmod rpi4_led_act_driver 
~~~~

- make clean

In Pc
~~~~
make clean
~~~~
