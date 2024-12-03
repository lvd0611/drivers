#include "linux/mod_devicetable.h"
#include "linux/printk.h"
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/init.h>


struct i2c_adapter *my_adapter;
struct i2c_client *my_client;

static int test_i2c_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    printk("test_i2c_probe\n");
    printk("client->addr = 0x%x\n", client->addr);
    printk("client->adapter->nr = %d\n", client->adapter->nr);
    printk("i2c_device_id: %s\n", id->name);
    return 0;
}

static int test_i2c_remove(struct i2c_client *client)
{
    printk("test_i2c_remove\n");
    return 0;
}



/*设备描述*/
static struct i2c_board_info test_i2c_info = {
    .type = "test_i2c",
    .addr = 0x51,
};

static const struct i2c_device_id test_i2c_id[] = {
    {"test_i2c", 0},
    {}
};

/*驱动定义*/
static struct i2c_driver test_i2c_driver = {
    .driver = {
        .name = "test_i2c",
    },
    .probe = test_i2c_probe,
    .remove = test_i2c_remove,
    .id_table = test_i2c_id,
};


static int __init test_i2c_init(void)
{
        int ret = 0;
    printk("test_i2c_init\n");
    /*获取适配器*/
    my_adapter = i2c_get_adapter(0);
    if(!my_adapter) {
        printk("i2c_get_adapter failed\n");
        return -ENODEV;
    }
    /*注册设备*/
    my_client = i2c_new_device(my_adapter, &test_i2c_info);
    if(!my_client) {
        printk("i2c_new_device failed\n");
        return -ENODEV;
    }

    /*注册驱动*/
    ret = i2c_add_driver(&test_i2c_driver);
    if(ret) {
        printk("i2c_add_driver failed\n");
        return ret;
    }

    return 0;
}

static void __exit test_i2c_exit(void)
{
    printk("test_i2c_exit\n");
    i2c_del_driver(&test_i2c_driver);
}


module_init(test_i2c_init);
module_exit(test_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("zhangjl");
MODULE_DESCRIPTION("test i2c driver");