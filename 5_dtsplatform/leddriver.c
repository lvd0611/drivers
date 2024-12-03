/***************************************************************
 Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
 文件名    : leddriver.c
 作者      : 邓涛
 版本      : V1.0
 描述      : platform总线编程示例之platform驱动模块
 其他      : 无
 论坛      : www.openedv.com
 日志      : 初版V1.0 2019/1/30 邓涛创建
 ***************************************************************/

#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/platform_device.h>

#define MYLED_CNT		1			/* 设备号个数 */
#define MYLED_NAME		"myled"		/* 名字 */

/* LED设备结构体 */
struct myled_dev {
	dev_t devid;			/* 设备号 */
	struct cdev cdev;		/* cdev结构体 */
	struct class *class;	/* 类 */
	struct device *device;	/* 设备 */
	int led_gpio;			/* GPIO号 */
};

static struct myled_dev myled;		/* led设备 */

/*
 * @description		: 打开设备
 * @param – inode	: 传递给驱动的inode
 * @param - filp	: 设备文件，file结构体有个叫做private_data的成员变量
 * 					  一般在open的时候将private_data指向设备结构体。
 * @return			: 0 成功;其他 失败
 */
static int myled_open(struct inode *inode, struct file *filp)
{
	return 0;
}

/*
 * @description		: 向设备写数据 
 * @param – filp	: 设备文件，表示打开的文件描述符
 * @param - buf		: 要写给设备写入的数据
 * @param - cnt		: 要写入的数据长度
 * @param - offt	: 相对于文件首地址的偏移
 * @return			: 写入的字节数，如果为负值，表示写入失败
 */
static ssize_t myled_write(struct file *filp, const char __user *buf, 
			size_t cnt, loff_t *offt)
{
	int ret;
	char kern_buf[1];

	ret = copy_from_user(kern_buf, buf, cnt);	// 得到应用层传递过来的数据
	if(0 > ret) {
		printk(KERN_ERR "myled: Failed to copy data from user buffer\r\n");
		return -EFAULT;
	}

	if (0 == kern_buf[0])
		gpio_set_value(myled.led_gpio, 0);		// 如果传递过来的数据是0则关闭led
	else if (1 == kern_buf[0])
		gpio_set_value(myled.led_gpio, 1);		// 如果传递过来的数据是1则点亮led

	return 0;
}

static int myled_init(struct device_node *nd)
{
	const char *str;
	int val;
	int ret;

	/* 从设备树中获取GPIO */
	myled.led_gpio = of_get_named_gpio(nd, "led-gpio", 0);
	if(!gpio_is_valid(myled.led_gpio)) {
		printk(KERN_ERR "myled: Failed to get led-gpio\n");
		return -EINVAL;
	}

	/* 申请使用GPIO */
	ret = gpio_request(myled.led_gpio, "PS_LED0 Gpio");
	if (ret) {
		printk(KERN_ERR "myled: Failed to request led-gpio\n");
		return ret;
	}

	/* 确定LED初始状态 */
	ret = of_property_read_string(nd, "default-state", &str);
	if(!ret) {
		if (!strcmp(str, "on"))
			val = 1;
		else
			val = 0;
	} else
		val = 0;

	/* 将GPIO设置为输出模式并设置GPIO初始电平状态 */
	gpio_direction_output(myled.led_gpio, val);

	return 0;
}

/* LED设备操作函数 */
static struct file_operations myled_fops = {
	.owner = THIS_MODULE,
	.open = myled_open,
	.write = myled_write,
};

/*
 * @description		: platform驱动的probe函数，当驱动与设备
 * 					  匹配成功以后此函数就会执行
 * @param - pdev	: platform设备指针
 * @return			: 0，成功;其他负值,失败
 */
static int myled_probe(struct platform_device *pdev)
{
	int ret;

	printk(KERN_INFO "myled: led driver and device has matched!\r\n");
	printk("myled: %s %s %d\r\n", pdev->name, pdev->dev.of_node->full_name, pdev->id);

	/* led初始化 */
	ret = myled_init(pdev->dev.of_node);
	if (ret)
		return ret;

	/* 初始化cdev */
	ret = alloc_chrdev_region(&myled.devid, 0, MYLED_CNT, MYLED_NAME);
	if (ret)
		goto out1;

	myled.cdev.owner = THIS_MODULE;
	cdev_init(&myled.cdev, &myled_fops);

	/* 添加cdev */
	ret = cdev_add(&myled.cdev, myled.devid, MYLED_CNT);
	if (ret)
		goto out2;

	/* 创建类class */
	myled.class = class_create(THIS_MODULE, MYLED_NAME);
	if (IS_ERR(myled.class)) {
		ret = PTR_ERR(myled.class);
		goto out3;
	}

	/* 创建设备 */
	myled.device = device_create(myled.class, &pdev->dev,
				myled.devid, NULL, MYLED_NAME);
	if (IS_ERR(myled.device)) {
		ret = PTR_ERR(myled.device);
		goto out4;
	}

	return 0;

out4:
	class_destroy(myled.class);

out3:
	cdev_del(&myled.cdev);

out2:
	unregister_chrdev_region(myled.devid, MYLED_CNT);

out1:
	gpio_free(myled.led_gpio);

	return ret;
}

/*
 * @description		: platform驱动模块卸载时此函数会执行
 * @param - dev		: platform设备指针
 * @return			: 0，成功;其他负值,失败
 */
static int myled_remove(struct platform_device *dev)
{
	printk(KERN_INFO "myled: led platform driver remove!\r\n");

	/* 注销设备 */
	device_destroy(myled.class, myled.devid);

	/* 注销类 */
	class_destroy(myled.class);

	/* 删除cdev */
	cdev_del(&myled.cdev);

	/* 注销设备号 */
	unregister_chrdev_region(myled.devid, MYLED_CNT);

	/* 删除地址映射 */
	gpio_free(myled.led_gpio);

	return 0;
}

/* 匹配列表 */
static const struct of_device_id led_of_match[] = {
	{ .compatible = "alientek,led" },
	{ /* Sentinel */ }
};

/* platform驱动结构体 */
static struct platform_driver myled_driver = {
	.driver = {
		.name			= "zynq-led",		// 驱动名字，用于和设备匹配
		.of_match_table	= led_of_match,		// 设备树匹配表，用于和设备树中定义的设备匹配
	},
	.probe		= myled_probe,	// probe函数
	.remove		= myled_remove,	// remove函数
};

/*
 * @description		: 模块入口函数
 * @param			: 无
 * @return			: 无
 */
static int __init myled_driver_init(void)
{
	return platform_driver_register(&myled_driver);
}

/*
 * @description		: 模块出口函数
 * @param			: 无
 * @return			: 无
 */
static void __exit myled_driver_exit(void)
{
	platform_driver_unregister(&myled_driver);
}

module_init(myled_driver_init);
module_exit(myled_driver_exit);

MODULE_AUTHOR("DengTao <773904075@qq.com>");
MODULE_DESCRIPTION("Led Platform Driver");
MODULE_LICENSE("GPL");
