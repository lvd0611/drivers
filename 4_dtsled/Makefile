KERN_DIR := /home/lvd/git.d/linux-xlnx
obj-m := dtsled.o
all:
	make -C $(KERN_DIR) M=`pwd` modules
clean:
	make -C $(KERN_DIR) M=`pwd` clean
