/***************************************************************
 Copyright © ALIENTEK Co., Ltd. 1998-2029. All rights reserved.
 文件名    : gt9xx.c
 作者      : 邓涛
 版本      : V1.0
 描述      : GOODiX GT9147/GT9271触摸屏驱动程序
 其他      : 无
 论坛      : www.openedv.com
 日志      : 初版V1.0 2020/7/26 邓涛创建
 ***************************************************************/

#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>

/* 寄存器定义 */
#define GOODIX_REG_COMMAND		0x8040
#define GOODIX_REG_CFG_DATA		0x8047
#define GOODIX_REG_CFG_CSM		0x80FF
#define GOODIX_REG_ID			0x8140
#define GOODIX_READ_COOR_ADDR	0x814E

/*
 *GT9271配置参数表
 *第一个字节为版本号,必须保证新的版本号大于等于GT9147内部
 *flash原有版本号,才会更新配置.
 */
static u8 gt9271_cfg_data[]=
{
	0x41,0x00,0x05,0x20,0x03,0x0A,0x3d,0x20,0x01,0x0A,
	0x28,0x0F,0x6E,0x5A,0x03,0x05,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x18,0x1A,0x1E,0x14,0x8F,0x2F,0xAA,
	0x26,0x24,0x0C,0x08,0x00,0x00,0x00,0x81,0x03,0x2D,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x1A,0x3C,0x94,0xC5,0x02,0x07,0x00,0x00,0x04,
	0x9E,0x1C,0x00,0x89,0x21,0x00,0x77,0x27,0x00,0x68,
	0x2E,0x00,0x5B,0x37,0x00,0x5B,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x19,0x18,0x17,0x16,0x15,0x14,0x11,0x10,
	0x0F,0x0E,0x0D,0x0C,0x09,0x08,0x07,0x06,0x05,0x04,
	0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x02,0x04,0x06,0x07,0x08,0x0A,0x0C,
	0x0D,0x0F,0x10,0x11,0x12,0x13,0x14,0x19,0x1B,0x1C,
	0x1E,0x1F,0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27,
	0x28,0x29,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x5D,0x01,
};

/*
 *GT9147配置参数表
 *第一个字节为版本号,必须保证新的版本号大于等于GT9147内部
 *flash原有版本号,才会更新配置.
 */
static u8 gt9147_cfg_data[]=
{
	0x41,0x20,0x03,0xE0,0x01,0x05,0x0d,0x00,0x01,0x08,
	0x28,0x05,0x50,0x32,0x03,0x05,0x00,0x00,0xff,0xff,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x89,0x28,0x0a,
	0x17,0x15,0x31,0x0d,0x00,0x00,0x02,0x9b,0x03,0x25,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x32,0x00,0x00,
	0x00,0x0f,0x94,0x94,0xc5,0x02,0x07,0x00,0x00,0x04,
	0x8d,0x13,0x00,0x5c,0x1e,0x00,0x3c,0x30,0x00,0x29,
	0x4c,0x00,0x1e,0x78,0x00,0x1e,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x08,0x0a,0x0c,0x0e,0x10,0x12,0x14,0x16,
	0x18,0x1a,0x00,0x00,0x00,0x00,0x1f,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0x00,0x02,0x04,0x05,0x06,0x08,0x0a,0x0c,
	0x0e,0x1d,0x1e,0x1f,0x20,0x22,0x24,0x28,0x29,0xff,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff,0xff,
	0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,
	0xff,0xff,0xff,0xff,
};

/* 自定义结构体,用于描述goodix触摸屏设备 */
struct goodix_gt9xx_dev {
	struct i2c_client *client;
	struct input_dev *input;
	int max_support_points;		//支持的最大触摸点数
	int reset_gpio;
	int irq_gpio;
};

/* goodix触摸IC信息 */
struct goodix_i2c_chip_data {
	int max_support_points;		//支持的最大触摸点数
	int abs_x_max;				//X轴最大值
	int abs_y_max;				//Y轴最大值
	int (*chip_cfg)(struct goodix_gt9xx_dev *);	//配置函数
};

static int goodix_gt9xx_ts_write(struct goodix_gt9xx_dev *gt9xx,
			u16 addr, u8 *buf, u16 len)
{
	struct i2c_client *client = gt9xx->client;
	struct i2c_msg msg;
	u8 send_buf[190] = {0};		//gt9147/gt9271最大配置长度+4
	int ret;

	send_buf[0] = addr >> 8;
	send_buf[1] = addr & 0xFF;
	memcpy(&send_buf[2], buf, len);

	msg.flags = 0;			//i2c写
	msg.addr = client->addr;
	msg.buf = send_buf;
	msg.len = len + 2;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if (1 == ret)
		return 0;
	else {
		dev_err(&client->dev, "%s: write error, addr=0x%x len=%d.\n",
					__func__, addr, len);
		return -1;
	}
}

static int goodix_gt9xx_ts_read(struct goodix_gt9xx_dev *gt9xx,
			u16 addr, u8 *buf, u16 len)
{
	struct i2c_client *client = gt9xx->client;
	struct i2c_msg msg[2];
	u8 send_buf[2];
	int ret;

	send_buf[0] = addr >> 8;
	send_buf[1] = addr & 0xFF;

	msg[0].flags = 0;			// i2c写
	msg[0].addr = client->addr;
	msg[0].buf = send_buf;
	msg[0].len = 2;				// 2个字节

	msg[1].flags = I2C_M_RD;	//i2c读
	msg[1].addr = client->addr;
	msg[1].buf = buf;
	msg[1].len = len;

	ret = i2c_transfer(client->adapter, msg, 2);
	if (2 == ret)
		return 0;
	else {
		dev_err(&client->dev, "%s: read error, addr=0x%x len=%d.\n",
					__func__, addr, len);
		return -1;
	}
}

static int goodix_gt9147_cfg(struct goodix_gt9xx_dev *gt9xx)
{
	struct i2c_client *client = gt9xx->client;
	u8 buf[5] = {0};
	int i;

	/* 读取Chip ID */
	goodix_gt9xx_ts_read(gt9xx, GOODIX_REG_ID, buf, 4);
	dev_info(&client->dev, "Chip ID: %s\n",  buf);

	/* 软件复位 */
	buf[0] = 0x2;
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_COMMAND, buf, 1);

	/* 读取配置文件版本号 */
	goodix_gt9xx_ts_read(gt9xx, GOODIX_REG_CFG_DATA, buf, 1);
	gt9147_cfg_data[0] = buf[0];	//写入版本号等于IC原有版本号

	/* 计算校验和 */
	buf[0] = 0;
	buf[1] = 1;
	for(i = 0; i < sizeof(gt9147_cfg_data); i++)
		buf[0] += gt9147_cfg_data[i];
	buf[0] = (~buf[0]) + 1;

	/* 配置寄存器 */
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_CFG_DATA,
				gt9147_cfg_data, sizeof(gt9147_cfg_data));
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_CFG_CSM, buf, 2);	// 写入校验和,更新配置

	/* 结束软件复位,回到读坐标模式 */
	msleep(1);
	buf[0] = 0x0;
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_COMMAND, buf, 1);
	goodix_gt9xx_ts_write(gt9xx, GOODIX_READ_COOR_ADDR, buf, 1);

	return 0;
}

static int goodix_gt9271_cfg(struct goodix_gt9xx_dev *gt9xx)
{
	struct i2c_client *client = gt9xx->client;
	u8 buf[5] = {0};
	int i;

	/* 读取Chip ID */
	goodix_gt9xx_ts_read(gt9xx, GOODIX_REG_ID, buf, 4);
	dev_info(&client->dev, "Chip ID: %s\n",  buf);

	/* 软件复位 */
	buf[0] = 0x2;
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_COMMAND, buf, 1);

	/* 读取配置文件版本号 */
	goodix_gt9xx_ts_read(gt9xx, GOODIX_REG_CFG_DATA, buf, 1);
	gt9271_cfg_data[0] = buf[0];	//写入版本号等于IC原有版本号

	/* 计算校验和 */
	buf[0] = 0;
	buf[1] = 1;
	for(i = 0; i < sizeof(gt9271_cfg_data) - 2; i++)
		buf[0] += gt9271_cfg_data[i];
	buf[0] = (~buf[0]) + 1;

	/* 配置寄存器 */
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_CFG_DATA,
				gt9271_cfg_data, sizeof(gt9271_cfg_data));
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_CFG_CSM, buf, 2);	// 写入校验和,更新配置

	/* 结束软件复位,回到读坐标模式 */
	msleep(1);
	buf[0] = 0x0;
	goodix_gt9xx_ts_write(gt9xx, GOODIX_REG_COMMAND, buf, 1);
	goodix_gt9xx_ts_write(gt9xx, GOODIX_READ_COOR_ADDR, buf, 1);

	return 0;
}

static int goodix_gt9xx_ts_reset(struct goodix_gt9xx_dev *gt9xx)
{
	struct i2c_client *client = gt9xx->client;
	int ret;

	/* 从设备树中获取复位管脚和中断管脚 */
	gt9xx->reset_gpio = of_get_named_gpio(client->dev.of_node, "reset-gpios", 0);
	if (!gpio_is_valid(gt9xx->reset_gpio)) {
		dev_err(&client->dev, "Failed to get ts reset gpio\n");
		return gt9xx->reset_gpio;
	}

	gt9xx->irq_gpio = of_get_named_gpio(client->dev.of_node, "interrupt-gpios", 0);
	if (!gpio_is_valid(gt9xx->irq_gpio)) {
		dev_err(&client->dev, "Failed to get ts interrupt gpio\n");
		return gt9xx->irq_gpio;
	}

	/* 申请使用管脚 */
	ret = devm_gpio_request_one(&client->dev, gt9xx->reset_gpio,
				GPIOF_OUT_INIT_HIGH, "gt9xx reset PIN");
	if (ret < 0)
		return ret;

	ret = devm_gpio_request_one(&client->dev, gt9xx->irq_gpio,
				GPIOF_OUT_INIT_HIGH, "gt9xx interrupt PIN");
	if (ret < 0)
		return ret;

	/*
	 *硬件复位开始
	 *这里严格按照官方参考手册提供的复位时序
	 */
	msleep(5);

	/* begin select I2C slave addr */
	gpio_set_value_cansleep(gt9xx->reset_gpio, 0);
	msleep(20);					/* T2: > 10ms */

	/* HIGH: 0x28/0x29, LOW: 0xBA/0xBB */
	gpio_set_value_cansleep(gt9xx->irq_gpio, client->addr == 0x14);
	usleep_range(200, 1000);	/* T3: > 100us */
	gpio_set_value_cansleep(gt9xx->reset_gpio, 1);
	msleep(10);					/* T4: > 5ms */

	/* end select I2C slave addr */
	gpio_direction_input(gt9xx->reset_gpio);

	/* 中断管脚拉低 */
	gpio_set_value_cansleep(gt9xx->irq_gpio, 0);
	msleep(50);					/* T5: 50ms */

	/* 将中断引脚设置为输入模式 */
	gpio_direction_input(gt9xx->irq_gpio);
	return 0;
}

static int goodix_gt9xx_ts_get_points(struct goodix_gt9xx_dev *gt9xx, u8 *buf)
{
	u8 state = 0;
	int touch_num = 0;
	int ret;

	ret = goodix_gt9xx_ts_read(gt9xx, GOODIX_READ_COOR_ADDR, &state, 1);
	if (ret)
		return ret;

	if ((state & 0x80) == 0)		// 判断数据是否准备好
		return -1;

	touch_num = state & 0x0F;		// 获取触摸点数
	if (touch_num > gt9xx->max_support_points) {
		touch_num = -1;
		goto out;
	}

	if (touch_num) {
		/* 读取触摸点坐标数据，从0x814F寄存器开始读取
		 * 其中每一个触摸点使用8个寄存器来描述
		 * 以第一个触摸点为例，各寄存器描述信息如下：
		 * 0x814F: 触摸点id
		 * 0x8150: 触摸点X轴坐标低位字节
		 * 0x8151: 触摸点X轴坐标高位字节
		 * 0x8152: 触摸点Y轴坐标低位字节
		 * 0x8153: 触摸点Y轴坐标高位字节
		 * 0x8154~0x8155: 触摸点的大小信息，我们不需要
		 * 0x8156: 保留
		 */
		ret = goodix_gt9xx_ts_read(gt9xx, GOODIX_READ_COOR_ADDR + 1,
					buf, 8 * touch_num);
		if (ret)
			touch_num = -1;
	}

out:
	state = 0x0;
	goodix_gt9xx_ts_write(gt9xx, GOODIX_READ_COOR_ADDR, &state, 1);	//清buffer
	return touch_num;
}

static irqreturn_t goodix_gt9xx_ts_isr(int irq, void *dev_id)
{
	struct goodix_gt9xx_dev *gt9xx = dev_id;
	static int pre_touch = 0;		//上一次触摸点数
	static int pre_ids[10] = {0};	//上一次触摸点的id
	int cur_touch = 0;				//当前触摸点数
	int cur_ids[10] = {0};			//当前触摸点的id
	u8 rdbuf[8 * 10] = {0};			//gt9147最大支持10点触摸
	int i, x, y, id;

	/* 读取触摸点坐标信息 */
	cur_touch = goodix_gt9xx_ts_get_points(gt9xx, rdbuf);
	if (cur_touch < 0)
		goto out;

	/* 上报触摸屏按下相关事件 */
	for (i = 0; i < cur_touch; i++) {

		u8 *buf = &rdbuf[i * 8];
		id = buf[0];
		x = (buf[2] << 8) | buf[1];
		y = (buf[4] << 8) | buf[3];

		input_mt_slot(gt9xx->input, id);
		input_mt_report_slot_state(gt9xx->input, MT_TOOL_FINGER, true);
		input_report_abs(gt9xx->input, ABS_MT_POSITION_X, x);
		input_report_abs(gt9xx->input, ABS_MT_POSITION_Y, y);
		cur_ids[i] = id;
	}

	/* 上报触摸屏松开相关事件 */
	for (i = 0; i < pre_touch; i++) {

		int j;
		for (j = 0; j < cur_touch; j++) {

			if (pre_ids[i] == cur_ids[j])
				break;
		}

		if (j == cur_touch) {	//表示当前触摸点中不存在这个pre_ids[i]id,表示它已经松开了
			input_mt_slot(gt9xx->input, pre_ids[i]);
			input_mt_report_slot_state(gt9xx->input, MT_TOOL_FINGER, false);
		}
	}

	input_mt_report_pointer_emulation(gt9xx->input, true);
	input_sync(gt9xx->input);

	for (i = 0; i < cur_touch; i++)
		pre_ids[i] = cur_ids[i];
	pre_touch = cur_touch;

out:
	return IRQ_HANDLED;
}

static int goodix_gt9xx_ts_irq(struct goodix_gt9xx_dev *gt9xx)
{
	struct i2c_client *client = gt9xx->client;
	int ret;

	/* 注册中断服务函数 */
	ret = devm_request_threaded_irq(&client->dev, client->irq,
				NULL, goodix_gt9xx_ts_isr, IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				client->name, gt9xx);
	if (ret) {
		dev_err(&client->dev, "Failed to request touchscreen IRQ.\n");
		return ret;
	}

	return 0;
}

static int goodix_gt9xx_ts_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct goodix_gt9xx_dev *gt9xx;
	const struct goodix_i2c_chip_data *chip_data;
	struct input_dev *input;
	int ret;

	/* 实例化一个struct goodix_gt9xx_dev对象 */
	gt9xx = devm_kzalloc(&client->dev, sizeof(struct goodix_gt9xx_dev), GFP_KERNEL);
	if (!gt9xx) {
		dev_err(&client->dev, "Failed to allocate ts driver data.\n");
		return -ENOMEM;
	}

	gt9xx->client = client;

	/* 获取gt9147、gt9271不同IC对应的信息 */
	chip_data = of_device_get_match_data(&client->dev);
	gt9xx->max_support_points = chip_data->max_support_points;

	/* 复位GT9xx触摸芯片 */
	ret = goodix_gt9xx_ts_reset(gt9xx);
	if (ret)
		return ret;

	msleep(5);

	/* 初始化GT9xx */
	ret = chip_data->chip_cfg(gt9xx);
	if (ret)
		return ret;

	/* 申请、注册中断服务函数 */
	ret = goodix_gt9xx_ts_irq(gt9xx);
	if (ret)
		return ret;

	/* 注册input设备 */
	input = devm_input_allocate_device(&client->dev);
	if (!input) {
		dev_err(&client->dev, "Failed to allocate input device.\n");
		return -ENOMEM;
	}

	gt9xx->input = input;
	input->name = "Goodix GT9xx TouchScreen";
	input->id.bustype = BUS_I2C;

	input_set_abs_params(input, ABS_MT_POSITION_X,
				0, chip_data->abs_x_max, 0, 0);
	input_set_abs_params(input, ABS_MT_POSITION_Y,
				0, chip_data->abs_y_max, 0, 0);

	ret = input_mt_init_slots(input, gt9xx->max_support_points,
				INPUT_MT_DIRECT);
	if (ret) {
		dev_err(&client->dev, "Failed to init MT slots.\n");
		return ret;
	}

	ret = input_register_device(input);
	if (ret)
		return ret;

	i2c_set_clientdata(client, gt9xx);
	return 0;
}

static int goodix_gt9xx_ts_remove(struct i2c_client *client)
{
	struct goodix_gt9xx_dev *gt9xx = i2c_get_clientdata(client);
	input_unregister_device(gt9xx->input);
	return 0;
}

static const struct goodix_i2c_chip_data goodix_gt9147_data = {
	.max_support_points = 5,
	.abs_x_max = 800,		//以4.3寸800*480屏幕为例
	.abs_y_max = 480,		//如果换成4.3寸480*272,这里要改,然后配置表也要改
	.chip_cfg = goodix_gt9147_cfg,
};

static const struct goodix_i2c_chip_data goodix_gt9271_data = {
	.max_support_points = 10,
	.abs_x_max = 1280,
	.abs_y_max = 800,
	.chip_cfg = goodix_gt9271_cfg,
};

static const struct of_device_id goodix_gt9xx_of_match[] = {
	{ .compatible = "goodix,gt9147", .data = &goodix_gt9147_data },
	{ .compatible = "goodix,gt9271", .data = &goodix_gt9271_data },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, goodix_gt9xx_of_match);

static struct i2c_driver goodix_gt9xx_ts_driver = {
	.driver = {
		.owner			= THIS_MODULE,
		.name			= "goodix-gt9xx",
		.of_match_table	= of_match_ptr(goodix_gt9xx_of_match),
	},
	.probe    = goodix_gt9xx_ts_probe,
	.remove   = goodix_gt9xx_ts_remove,
};

module_i2c_driver(goodix_gt9xx_ts_driver);

MODULE_AUTHOR("Deng Tao <773904075@qq.com>, ALIENTEK, Inc.");
MODULE_DESCRIPTION("Goodix gt9147/gt9271 I2C Touchscreen Driver");
MODULE_LICENSE("GPL");
