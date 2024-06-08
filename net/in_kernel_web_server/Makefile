obj-m += server_kernel.o

KERNEL_SOURCE := /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

default: kernel

all: kernel user

kernel:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) modules

# Common kernel configs.
user:
	$(CC) server_user.c -O2 -pthread -std=gnu11 -o server_user \
	-fno-strict-aliasing -fno-common -fshort-wchar -funsigned-char \
	-Wundef -Werror=strict-prototypes -Wno-trigraphs \
	-Werror=implicit-function-declaration -Werror=implicit-int \
	-Werror=return-type -Wno-format-security

clean:
	$(MAKE) -C $(KERNEL_SOURCE) M=$(PWD) clean
	rm -f server_user
