obj-m += server.o

KERNEL_SOURCE := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
