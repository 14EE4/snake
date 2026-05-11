CR_C := arm-linux-gnueabihf-
obj-m := itr_driver.o

KDIR := /home/knu/linux-f16e91dad2af9d57aef477cc1f522040353849f5/
PWD := $(shell pwd)

default:
	make ARCH=arm CROSS_COMPILE=$(CR_C) -C $(KDIR) M=$(PWD) modules

clean:
	make ARCH=arm CROSS_COMPILE=$(CR_C) -C $(KDIR) M=$(PWD) clean
