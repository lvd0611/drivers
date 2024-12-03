#include "linux/device.h"
#include "linux/gpio/consumer.h"
#include "linux/leds.h"
#include "linux/timer.h"
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>


struct key_info {
    struct gpio_desc *gpio;
    int irq;
    char *name;
    struct timer_list timer;
};

static struct key_info pl_key;



static irqreturn_t pl_key_irq_handler(int irq, void *dev_id)
{
    struct key_info *key = dev_id;
    /*按键消抖*/
    mod_timer(&key->timer, jiffies + msecs_to_jiffies(20));
    return IRQ_HANDLED;
}

static void pl_key_timer_handler(struct timer_list *timer)
{
    printk("pl_key_timer_handler\n");
}

static int pl_key_probe(struct platform_device *pdev)
{
    struct device_node *np = pdev->dev.of_node;
    int ret;
    

    dev_info(&pdev->dev, "pl_key_probe start\n");
    pl_key.gpio = devm_gpiod_get(&pdev->dev, "key", GPIOD_IN);
    if (IS_ERR(pl_key.gpio)) {
        printk("Failed to get pl_key gpio\n");
        return PTR_ERR(pl_key.gpio);
    }
    pl_key.irq = gpiod_to_irq(pl_key.gpio);
    if (pl_key.irq < 0) {
        printk("Failed to get pl_key irq\n");
        return pl_key.irq;
    }

    /*申请中断*/
    ret = devm_request_irq(&pdev->dev, pl_key.irq, pl_key_irq_handler, IRQF_TRIGGER_FALLING, "pl_key", &pl_key);
    if (ret) {
        printk("Failed to request irq\n");
        return ret;
    }

    /*初始化定时器*/
    timer_setup(&pl_key.timer, pl_key_timer_handler, 0);

    dev_info(&pdev->dev, "pl_key_probe\n");
    return 0;
}

static int pl_key_remove(struct platform_device *pdev)
{
    devm_free_irq(&pdev->dev, pl_key.irq, &pl_key);
    devm_gpiod_put(&pdev->dev, pl_key.gpio);
    del_timer_sync(&pl_key.timer);
    printk("pl_key_remove\n");
    return 0;
}


static const struct of_device_id pl_key_of_match[] = {
    { .compatible = "pl-key" , .data = "of_match_data"},
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, pl_key_of_match);


static struct platform_driver pl_key_driver = {
    .driver = {
        .name = "pl-key",
        .of_match_table = pl_key_of_match,
    },
    .probe = pl_key_probe,
    .remove = pl_key_remove,
};

module_platform_driver(pl_key_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lvd");
MODULE_DESCRIPTION("pl key driver");

