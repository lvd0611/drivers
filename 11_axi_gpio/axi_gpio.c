#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio/consumer.h>
#include <linux/err.h>
#include "asm-generic/gpio.h"
#include "gpiolib.h"

static int __init gpio_dt_test_init(void)
{
    struct device_node *node;
    int lcd_gpios[3]; /* 用于存储三个 GPIO 的描述符 */
    int i, gpio_value;
    int ret;

    /* 1. 查找设备树节点 */
    node = of_find_node_by_name(NULL, "xlnx_vdma_lcd");
    if (!node) {
        pr_err("Failed to find node xlnx_vdma_lcd\n");
        return -ENODEV;
    }
    pr_info("Found node: %s\n", node->name);

    /* 2. 获取 GPIO 并读取值 */
    for (i = 0; i < 3; i++) {
        lcd_gpios[i] = of_get_named_gpio(node, "lcdID-gpio",i);
        if(lcd_gpios[i] < 0) {
            pr_err("Failed to get lcdID-gpio %d\n", i);
            return -ENODEV;
        }
        ret = gpio_request(lcd_gpios[i], "lcdID-gpio");
        if (ret) {
            pr_err("Failed to request lcdID-gpio %d\n", i);
            return ret;
        }
        /* 3. 读取 GPIO 值 */
        gpio_direction_input(lcd_gpios[i]);
        gpio_value = gpio_get_value(lcd_gpios[i]);
        pr_info("LCD ID GPIO %d value: %d\n", i, gpio_value);

        /* 4. 释放 GPIO 资源 */
        gpio_free(lcd_gpios[i]);
    }

    return 0;
}

static void __exit gpio_dt_test_exit(void)
{
    pr_info("GPIO test module exited\n");
}

module_init(gpio_dt_test_init);
module_exit(gpio_dt_test_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("Device Tree GPIO Parsing Test");
