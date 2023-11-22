obj-m := rpi4_led_act_driver.o

all:
	make -C $(KERNEL_SRC) M=$(shell pwd) modules

clean:
	rm -rf *.o *.ko *.mod *.mod.c *.order *.symvers .*.cmd
