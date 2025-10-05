obj-m += barrier_test.o

KDIR ?= /lib/modules/$(shell uname -r)/build

all:
	make -C $(KDIR) M=$(PWD) modules
	objdump -d --disassemble=init_module barrier_test.ko | tee barrier_test.ko.objdump
clean:
	make -C $(KDIR) M=$(PWD) clean