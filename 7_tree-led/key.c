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
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/irq.h>
#include <linux/timer.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>



extern int led_set_state(bool state);
extern int led_get_state(void);

struct mykey_dev {
    int key_gpio;   /*GPIO号*/
    int irqnum;     /*中断号*/
    struct tasklet_struct tasklet;  /*tasklet*/
    struct timer_list timer;    /*定时器*/
};

static struct mykey_dev mykey;


static irqreturn_t key_handler(int irq, void *dev_id)
{
    /*按键消抖*/
    disable_irq_nosync(mykey.irqnum);
    mod_timer(&mykey.timer, jiffies + msecs_to_jiffies(10));
    printk("hardirq Current process PID: %d\n", current->pid);
    return IRQ_HANDLED;
}

void key_debounce(struct timer_list *t)
{
    enable_irq(mykey.irqnum);
    tasklet_schedule(&mykey.tasklet);
}

void key_tasklet_handler(unsigned long data)
{
    int state;
    unsigned long stack_start = (unsigned long)task_stack_page(current);
    unsigned long stack_end = stack_start + THREAD_SIZE;
    unsigned long test_point;

    printk("Kernel stack range: 0x%lx - 0x%lx\n", stack_start, stack_end);
    printk("Address of test_point (automatic variable): 0x%lx\n", (unsigned long)&test_point);
    // 打印当前进程的 PID
    printk("led_probe Current process PID: %d\n", current->pid);

    state = led_get_state();
    printk("key value: %d\n", state);
    led_set_state(!state);
}


static int key_probe(struct platform_device *pdev)
{
    int ret;
    unsigned long irq_flags;
    struct device_node *nd = pdev->dev.of_node;

    unsigned long stack_start = (unsigned long)task_stack_page(current);
    unsigned long stack_end = stack_start + THREAD_SIZE;
    unsigned long test_point;


    mykey.key_gpio = of_get_named_gpio(nd, "key-gpio", 0);
    if (!gpio_is_valid(mykey.key_gpio)) {
        printk("can't find key-gpio");
        return -ENODEV;
    }

    ret = gpio_request(mykey.key_gpio, "key0");
    if(ret){
        printk("request gpio failed\n");
        return -ENODEV;
    }

    gpio_direction_input(mykey.key_gpio);

    mykey.irqnum = irq_of_parse_and_map(nd, 0);
    if(!mykey.irqnum){
        printk("can't find key irq\n");
        return -ENODEV;
    }

    irq_flags = irq_get_trigger_type(mykey.irqnum);
    if(irq_flags == IRQF_TRIGGER_NONE){
        irq_flags = IRQF_TRIGGER_FALLING;
    }

    ret = request_irq(mykey.irqnum, key_handler, irq_flags, "key0", NULL);
    if(ret){
        printk("request irq failed\n");
        return -ENODEV;
    }

    /*初始化takslet*/
    tasklet_init(&mykey.tasklet, key_tasklet_handler, 0);

    /*初始化定时器*/
    timer_setup(&mykey.timer, key_debounce, 0);


    printk("key_probe Kernel stack range: 0x%lx - 0x%lx\n", stack_start, stack_end);
    printk("key_probe Address of test_point (automatic variable): 0x%lx\n", (unsigned long)&test_point);
    return 0;
}

static int key_remove(struct platform_device *pdev)
{
    free_irq(mykey.irqnum, NULL);
    gpio_free(mykey.key_gpio);
    printk("key remove\n");
    return 0;
}

static const struct of_device_id mykey_of_match[] = {
    { .compatible = "gpio-keys" },
    { /* Sentinel */ }
};

static struct platform_driver mykey_driver = {
    .driver = {
        .name = "mykey",
        .of_match_table = mykey_of_match,
    },
    .probe = key_probe,
    .remove = key_remove,
};

module_platform_driver(mykey_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lvd");
MODULE_DESCRIPTION("key driver");
