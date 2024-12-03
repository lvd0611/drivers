# 内核源码目录
KDIR = /home/lvd/workspace/petalinux/linux-xlnx-xilinx-v2019.2

# 指定模块文件
obj-m += xlnx_vdmafb.o

# 添加头文件搜索路径
ccflags-y += -I$(srctree)/drivers/media/platform/xilinx

all:
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

