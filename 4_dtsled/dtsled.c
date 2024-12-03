/**************************************************************
 Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
 文件名    : dtsled.c
 作者      : 邓涛
 版本      : V1.0
 描述      : ZYNQ LED驱动文件。
 其他      : 无
 论坛      : www.openedv.com
 日志      : 初版V1.0 2019/1/30 邓涛创建
 ***************************************************************/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/ide.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/gpio.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/of_address.h>
//#include <linux/device.h>

#define DTSLED_CNT          1            /* 设备号个数 */
#define DTSLED_NAME         "dtsled"     /* 名字 */

/* 映射后的寄存器虚拟地址指针 */
static void __iomem *data_addr;
static void __iomem *dirm_addr;
static void __iomem *outen_addr;
static void __iomem *intdis_addr;
static void __iomem *aper_clk_ctrl_addr;

/* dtsled设备结构体 */
struct dtsled_dev {
    dev_t devid;            /* 设备号 */
    struct cdev cdev;       /* cdev */
    struct class *class;    /* 类 */
    struct device *device;  /* 设备 */
    int major;              /* 主设备号 */
    int minor;              /* 次设备号 */
    struct device_node *nd; /* 设备节点 */
};

static struct dtsled_dev dtsled;     /* led设备 */

/*
 * @description         : 打开设备
 * @param – inode       : 传递给驱动的inode
 * @param - filp        : 设备文件，file结构体有个叫做private_data的成员变量
 *                        一般在open的时候将private_data指向设备结构体。
 * @return              : 0 成功;其他 失败
 */
static int led_open(struct inode *inode, struct file *filp)
{
    filp->private_data = &dtsled;   /* 设置私有数据 */
    return 0;
}

/*
 * @description         : 从设备读取数据 
 * @param - filp        : 要打开的设备文件(文件描述符)
 * @param - buf         : 返回给用户空间的数据缓冲区
 * @param - cnt         : 要读取的数据长度
 * @param - offt        : 相对于文件首地址的偏移
 * @return              : 读取的字节数，如果为负值，表示读取失败
 */
static ssize_t led_read(struct file *filp, char __user *buf,
            size_t cnt, loff_t *offt)
{
    return 0;
}

/*
 * @description         : 向设备写数据 
 * @param - filp        : 设备文件，表示打开的文件描述符
 * @param - buf         : 要写给设备写入的数据
 * @param - cnt         : 要写入的数据长度
 * @param - offt        : 相对于文件首地址的偏移
 * @return              : 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t led_write(struct file *filp, const char __user *buf,
            size_t cnt, loff_t *offt)
{
    int ret;
    int val;
    char kern_buf[1];

    ret = copy_from_user(kern_buf, buf, cnt);       // 得到应用层传递过来的数据
    if(0 > ret) {
        printk(KERN_ERR "kernel write failed!\r\n");
        return -EFAULT;
    }

    val = readl(data_addr);
    if (0 == kern_buf[0])
        val &= ~(0x1U << 12);            // 如果传递过来的数据是0则关闭led
    else if (1 == kern_buf[0])
        val |= (0x1U << 12);                     // 如果传递过来的数据是1则点亮led

    writel(val, data_addr);
    return 0;
}

/*
 * @description         : 关闭/释放设备
 * @param – filp        : 要关闭的设备文件(文件描述符)
 * @return              : 0 成功;其他 失败
 */
static int led_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static inline void led_ioremap(void)
{
    data_addr = of_iomap(dtsled.nd, 0);
    dirm_addr = of_iomap(dtsled.nd, 1);
    outen_addr = of_iomap(dtsled.nd, 2);
    intdis_addr = of_iomap(dtsled.nd, 3);
    aper_clk_ctrl_addr = of_iomap(dtsled.nd, 4);
}

static inline void led_iounmap(void)
{
    iounmap(data_addr);
    iounmap(dirm_addr);
    iounmap(outen_addr);
    iounmap(intdis_addr);
    iounmap(aper_clk_ctrl_addr);
}

/* 设备操作函数 */
static struct file_operations dtsled_fops = {
    .owner   = THIS_MODULE,
    .open    = led_open,
    .read    = led_read,
    .write   = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    const char *str;
    u32 val;
    int ret;

    /* 1.获取led设备节点 */
    dtsled.nd = of_find_node_by_path("/led");
    if(NULL == dtsled.nd) {
        printk(KERN_ERR "led node can not found!\r\n");
        return -EINVAL;
    }

    /* 2.读取status属性 */
    ret = of_property_read_string(dtsled.nd, "status", &str);
    if(!ret) {
        if (strcmp(str, "okay"))
        return -EINVAL;
    }

    /* 2、获取compatible属性值并进行匹配 */
    ret = of_property_read_string(dtsled.nd, "compatible", &str);
    if(0 > ret)
        return -EINVAL;

    if (strcmp(str, "alientek,led"))
        return -EINVAL;

    printk(KERN_ERR "led device matching successful!\r\n");

    /* 4.寄存器地址映射 */
    led_ioremap();

    /* 5.使能GPIO时钟 */
    val = readl(aper_clk_ctrl_addr);
    val |= (0x1U << 24);
    writel(val, aper_clk_ctrl_addr);

    /* 6.关闭中断功能 */
    val |= (0x1U << 12);
    writel(val, intdis_addr);

    /* 7.设置GPIO为输出功能 */
    val = readl(dirm_addr);
    val |= (0x1U << 12);
    writel(val, dirm_addr);

    /* 8.使能GPIO输出功能 */
    val = readl(outen_addr);
    val |= (0x1U << 12);
    writel(val, outen_addr);

    /* 9.初始化LED的默认状态 */
    val = readl(data_addr);

    ret = of_property_read_string(dtsled.nd, "default-state", &str);
    if(!ret) {
        if (!strcmp(str, "on"))
            val |= (0x1U << 12);
        else
            val &= ~(0x1U << 12);
    } else
        val &= ~(0x1U << 12);

    writel(val, data_addr);

    /* 10.注册字符设备驱动 */
     /* 创建设备号 */
    if (dtsled.major) {
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, DTSLED_CNT, DTSLED_NAME);
        if (ret)
            goto out1;
    } else {
        ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_CNT, DTSLED_NAME);
        if (ret)
            goto out1;

        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }

    printk("dtsled major=%d,minor=%d\r\n",dtsled.major, dtsled.minor);

     /* 初始化cdev */
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

     /* 添加一个cdev */
    ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_CNT);
    if (ret)
        goto out2;

     /* 创建类 */
    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if (IS_ERR(dtsled.class)) {
        ret = PTR_ERR(dtsled.class);
        goto out3;
    }

     /* 创建设备 */
    dtsled.device = device_create(dtsled.class, NULL,
                dtsled.devid, NULL, DTSLED_NAME);
    if (IS_ERR(dtsled.device)) {
        ret = PTR_ERR(dtsled.device);
        goto out4;
    }

    return 0;

out4:
    class_destroy(dtsled.class);

out3:
    cdev_del(&dtsled.cdev);

out2:
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);

out1:
    led_iounmap();

    return ret;
}

static void __exit led_exit(void)
{
    /* 注销设备 */
    device_destroy(dtsled.class, dtsled.devid);

    /* 注销类 */
    class_destroy(dtsled.class);

    /* 删除cdev */
    cdev_del(&dtsled.cdev);

    /* 注销设备号 */
    unregister_chrdev_region(dtsled.devid, DTSLED_CNT);

    /* 取消地址映射 */
    led_iounmap();
}

/* 驱动模块入口和出口函数注册 */
module_init(led_init);
module_exit(led_exit);

MODULE_AUTHOR("DengTao <773904075@qq.com>");
MODULE_DESCRIPTION("Alientek ZYNQ GPIO LED Driver");
MODULE_LICENSE("GPL");
