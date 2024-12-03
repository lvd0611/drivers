# �ں�Դ��Ŀ¼
KDIR = /home/lvd/workspace/petalinux/linux-xlnx-xilinx-v2019.2

# ָ��ģ���ļ�
obj-m += pl_clk.o

# ����ͷ�ļ�����·��
ccflags-y += -I$(srctree)/drivers/media/platform/xilinx

all:
	make -C $(KDIR) M=$(PWD) CROSS_COMPILE=$(CROSS_COMPILE) modules

clean:
	make -C $(KDIR) M=$(PWD) clean

