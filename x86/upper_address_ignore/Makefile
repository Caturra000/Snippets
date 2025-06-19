obj-m += uai.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules
	$(CXX) test.cpp -o test

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	rm -f test