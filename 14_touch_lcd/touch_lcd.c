#include "linux/device.h"
#include "linux/err.h"
#include "linux/gfp.h"
#include "linux/gpio/consumer.h"
#include "linux/input.h"
#include "linux/irqreturn.h"
#include "linux/leds.h"
#include "linux/mod_devicetable.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/input/mt.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/of_device.h>


/*寄存器定义*/
#define GOODIX_REG_COMMAND		0x8040
#define GOODIX_REG_CFG_DATA		0x8047
#define GOODIX_REG_CFG_CSM		0x80FF
#define GOODIX_REG_ID			0x8140
#define GOODIX_READ_COOR_ADDR	0x814E

#define GT9271_addr             0x5D

/*GT9271配置表*/
//初始触摸屏配置
static u8 GT9271_CFG_TBL[]= {
    0x41,0x00,0x05,0x20,0x03,0x0A,0x3D,0x20,0x01,0x0A,
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
    0x00,0x00,0x00,0x00,0x5D,0x01
};


/*自定义结构体，描述触摸屏设备*/
struct touch_dev{
    struct i2c_client *client; //I2C设备
    struct input_dev *input; //输入设备
    struct gpio_desc *reset_gpio; //复位引脚
    struct gpio_desc *irq_gpio; //中断引脚
    bool cur_state[10];
    bool pre_state[10];
};


/*触摸IC信息*/
struct chip_data{
    int max_support_points; //最大支持点数
    int max_x; //最大X坐标
    int max_y; //最大Y坐标
};

/*GT9271设备信息*/
static const struct chip_data gt9271_chip_data = {
    .max_support_points = 10,
    .max_x = 1280,
    .max_y = 800,
};



/*
iic读取信息函数
@tdev:触摸屏设备
@reg:寄存器地址
@buf:读取数据缓冲区
@len:读取数据长度
*/
static int gt9271_read_info(struct touch_dev *tdev, u16 reg, u8 *buf, int len){
    int ret;
    struct i2c_msg msgs[2];
    struct i2c_client *client = tdev->client;
    u8 sendreg[2];
    sendreg[0] = reg >> 8;
    sendreg[1] = reg & 0xff;

    msgs[0].addr = client->addr;
    msgs[0].flags = 0;  //写
    msgs[0].buf = sendreg;
    msgs[0].len = 2;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD; //读
    msgs[1].buf = buf;
    msgs[1].len = len;

    ret = i2c_transfer(client->adapter, msgs, 2);
    if(ret != 2){
        dev_err(&client->dev, "%s:read error , addr =0x%x , len = %d.\n", __func__, reg, len);
        return -EIO;
    }  
    return 0;    
}


/*iic写函数*/
static int gt9271_write_info(struct touch_dev *tdev, u16 reg, u8 *buf, int len){
    int ret;
    struct i2c_msg msgs;
    struct i2c_client *client = tdev->client;
    u8 sendbuf[len+2];  //2字节地址

    sendbuf[0] = reg >> 8;
    sendbuf[1] = reg & 0xff;
    memcpy(&sendbuf[2], buf, len);

    msgs.addr = client->addr;
    msgs.flags = 0;
    msgs.buf = sendbuf;
    msgs.len = len+2;

    ret = i2c_transfer(client->adapter, &msgs, 1);
    if(ret != 1){
        dev_err(&client->dev, "%s:write error , addr =0x%x , len = %d.\n", __func__, reg, len);
        return -EIO;
    }
    return 0;
}


/*gt9271初始化*/
static int gt9271_init(struct touch_dev *tdev){
    struct i2c_client *client = tdev->client;
    
    tdev->reset_gpio = devm_gpiod_get(&client->dev, "reset", GPIOD_OUT_HIGH);
    if(IS_ERR(tdev->reset_gpio)){
        dev_err(&client->dev, "get reset gpio error.\n");
        return PTR_ERR(tdev->reset_gpio);
    }

    tdev->irq_gpio = devm_gpiod_get(&client->dev, "interrupt", GPIOD_OUT_HIGH);
    if(IS_ERR(tdev->irq_gpio)){
        dev_err(&client->dev,"get irq gpio error.\n");
        return PTR_ERR(tdev->irq_gpio);
    }

    /*
    按照芯片手册复位时序，addr：0x5d，拉低INT引脚100us以上,再拉高reset引脚55ms以上,最后将INT引脚转为输入模式
    设备树中gpio定义为GPIO_ACTIVE_LOW，所以这里1是低电平，0是高电平
    */
    gpiod_set_value(tdev->reset_gpio,1);
    msleep(20);

    gpiod_set_value(tdev->irq_gpio, 1);
    msleep(1);
    gpiod_set_value(tdev->reset_gpio,0);
    msleep(60);
    gpiod_direction_input(tdev->irq_gpio);


    return 0;
}

/*gt9271芯片配置*/
static int gt9271_config(struct touch_dev *tdev){
    struct i2c_client *client = tdev->client;
    u8 buf[5];
    int ret;

    //读取ID
    ret = gt9271_read_info(tdev, GOODIX_REG_ID, buf, 4);
    dev_info(&client->dev, "chip ID:%s.\n", buf);

    /*软件复位*/
    buf[0] = 0x02;
    ret = gt9271_write_info(tdev, GOODIX_REG_COMMAND, buf, 1);
    msleep(1);

    /*读取配置版本号*/
    ret = gt9271_read_info(tdev, GOODIX_REG_CFG_CSM, buf, 1);
    if(buf[0] < 0x41){
        gt9271_write_info(tdev,GOODIX_REG_CFG_DATA,GT9271_CFG_TBL,sizeof(GT9271_CFG_TBL));
        dev_info(&client->dev,"write gt9271 config\n");
    }

    	/* 结束软件复位,回到读坐标模式 */
	buf[0] = 0x0;
	ret = gt9271_write_info(tdev, GOODIX_REG_COMMAND, buf, 1);
	ret = gt9271_write_info(tdev, GOODIX_READ_COOR_ADDR, buf, 1);
    return ret;
}

/*读取屏幕触摸点*/
static int gt9271_get_points(struct touch_dev *tdev, u8 *buf){
    int ret;
    int touch_num;
    u8 state;
    
    ret = gt9271_read_info(tdev, GOODIX_READ_COOR_ADDR, &state, 1);
    
    //判断触摸数据是否有效
    if((state & 0x80) == 0){
        return -1;
    }
    /*
    获取触摸点数
    开机时存在数据准备好，但触摸点数为0的情况，此时需要将状态寄存器清0
    */
    touch_num = state & 0x0f;   
    if(touch_num > gt9271_chip_data.max_support_points){
        touch_num = -1;
        goto out;
    }

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
    if(touch_num ){
        ret = gt9271_read_info(tdev, GOODIX_READ_COOR_ADDR + 1, buf, touch_num*8);
        if(ret){
            touch_num = -1;
        }
    }

out:
    state = 0;
    gt9271_write_info(tdev, GOODIX_READ_COOR_ADDR, &state, 1);//清除状态寄存器
    return touch_num;
}
    

/*GT9271中断处理函数*/
static irqreturn_t gt9271_thread_isr(int irq , void *dev_id)
{
    struct touch_dev *tdev = dev_id;
    int cur_touch_num=0;
    u8 readbuf[10*8]={0};
    int i,x,y,id;
    x = 0;
    y = 0;
    //读取触摸点坐标信息
    cur_touch_num = gt9271_get_points(tdev, readbuf);
    if(cur_touch_num < 0){
        return IRQ_HANDLED;
    }

    //上报触摸点信息
    for(i = 0;i<cur_touch_num;i++){
        u8 *buf = &readbuf[i*8];
        id = buf[0];
        x = (buf[1] + ((u16)buf[2] << 8));
        y = (buf[3] + ((u16)buf[4] << 8));

        input_mt_slot(tdev->input, id);
        input_mt_report_slot_state(tdev->input,MT_TOOL_FINGER,true);
        input_report_abs(tdev->input, ABS_MT_POSITION_X, x);
        input_report_abs(tdev->input, ABS_MT_POSITION_Y, y);
        
        tdev->cur_state[id] = true;

        // 在触摸屏按下时触发 BTN_TOUCH 按钮事件
        if (!tdev->pre_state[id]) {
        // 如果之前没有触摸，表示这是一个新的触摸开始
            input_event(tdev->input, EV_KEY, BTN_TOUCH, 1); // 按下事件
            printk("id = %d , x = %d, y = %d\n", id,x, y);
        }
        
    }
    /*
    上报触摸点松开信息
    读取的硬件id为0~9，pre和cur_state数组下标代表的是触摸点的id，上述循环中将每次读取到的id数组设置为true
    */
    for(i = 0;i<gt9271_chip_data.max_support_points;i++){
        if(tdev->pre_state[i] && !tdev->cur_state[i]){
            input_mt_slot(tdev->input, i);
            input_mt_report_slot_state(tdev->input,MT_TOOL_FINGER,false);
            
            // 如果触摸点之前存在而现在消失，触发 BTN_TOUCH 松开事件
            input_event(tdev->input, EV_KEY, BTN_TOUCH, 0); // 松开事件
        }
    }

    //input_mt_report_pointer_emulation() 会将这些触摸点的状态合并并模拟一个单指的鼠标操作
    input_mt_report_pointer_emulation(tdev->input, true);
    input_sync(tdev->input);

    //更新触摸点状态
    for(i = 0;i<gt9271_chip_data.max_support_points;i++){
        tdev->pre_state[i] = tdev->cur_state[i];
        tdev->cur_state[i] = false;
    }
    return IRQ_HANDLED;

}





static int gt9271_probe(struct i2c_client *client, const struct i2c_device_id *id){
    struct touch_dev *gt9271_dev;
    struct input_dev *input;
    int ret;


    /*实例化gt9271_dev结构体对象*/
    gt9271_dev = devm_kzalloc(&client->dev, sizeof(*gt9271_dev), GFP_KERNEL);
    if(!gt9271_dev){
        dev_err(&client->dev, "devm_kzalloc error.\n");
        return -ENOMEM;
    }

    gt9271_dev->client = client;

    /*初始化GT9271芯片*/
    ret = gt9271_init(gt9271_dev);
    if(ret){
        dev_err(&client->dev, "gt9271_init error.\n");
        return ret;
    }
    msleep(5);
    /*检查GT9271芯片版本配置*/
    ret = gt9271_config(gt9271_dev);
    if(ret){
        dev_err(&client->dev, "gt9271_config error.\n");
        return ret;
    }

    /*申请、注册线程中断*/
    ret = devm_request_threaded_irq(&client->dev,client->irq,NULL,gt9271_thread_isr,
                                    IRQF_TRIGGER_RISING | IRQF_ONESHOT,client->name,gt9271_dev);
    if(ret){
        dev_err(&client->dev, "request irq error.\n");
        return ret;
    }

   /*注册input设备*/
    input = devm_input_allocate_device(&client->dev);
    if(!input){
        dev_err(&client->dev, "devm_input_allocate_device error.\n");
        return -ENOMEM;
    }

    gt9271_dev->input = input;
    input->name = "gt9271 touchscreen";
    input->id.bustype = BUS_I2C;
    input->evbit[0] = BIT(EV_ABS) | BIT(EV_KEY); // 支持绝对位置和按钮事件
    input_set_capability(input, EV_KEY, BTN_TOUCH); // 启用 BTN_TOUCH 按钮事件

    /*设置触摸屏支持的事件类型*/
    input_set_abs_params(input, ABS_MT_POSITION_X, 0, gt9271_chip_data.max_x, 0, 0);
    input_set_abs_params(input, ABS_MT_POSITION_Y, 0, gt9271_chip_data.max_y, 0, 0);

    ret = input_mt_init_slots(input, gt9271_chip_data.max_support_points, INPUT_MT_DIRECT);
    if(ret){
        dev_err(&client->dev, "input_mt_init_slots error.\n");
        return ret;
    }

    ret = input_register_device(input);
    if(ret){
        dev_err(&client->dev, "input_register_device error.\n");
        return ret;
    }

    //保存私有数据
    i2c_set_clientdata(client, gt9271_dev);
    return 0;

}


static int gt9271_remove(struct i2c_client *client){
    struct touch_dev *gt9271_dev = i2c_get_clientdata(client);
    input_unregister_device(gt9271_dev->input);

    return 0;
}

static const struct of_device_id gt9271_of_match[] = {
    { .compatible = "goodix,gt9271", },
    { },
};
MODULE_DEVICE_TABLE(of, gt9271_of_match);

static const struct i2c_device_id gt9271_id[] = {
    { "gt9271", 0 },
    { },
};

static struct i2c_driver gt9271_driver = {
    .driver = {
        .name = "gt9271",
        .of_match_table = of_match_ptr(gt9271_of_match),
    },
    .probe = gt9271_probe,
    .remove = gt9271_remove,
    .id_table = gt9271_id,
};

module_i2c_driver(gt9271_driver);

MODULE_AUTHOR("lvd");
MODULE_DESCRIPTION("GT9271 touchscreen driver");
MODULE_LICENSE("GPL");

