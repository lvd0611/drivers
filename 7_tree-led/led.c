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


struct myled_dev {
    int led_gpio;   /*GPIO��*/
    int led_flag;   /*LED״̬*/
    spinlock_t lock;    /*������*/
};

static struct myled_dev myled;

int led_set_state(int state)
{
    spin_lock(&myled.lock);
    printk("led_set_state:%d\n",state);
    gpio_set_value(myled.led_gpio,state);
    spin_unlock(&myled.lock);
    return 0;
}
EXPORT_SYMBOL(led_set_state);

int led_get_state(void)
{
    int state;
    spin_lock(&myled.lock);
    state = gpio_get_value(myled.led_gpio);
    printk("led_get_state:%d\n",state);
    spin_unlock(&myled.lock);
    return state;

}
EXPORT_SYMBOL(led_get_state);

static int myled_init(struct device_node *nd)
{
    int ret;
    const char *str;

    printk("myled_init start\n");
    myled.led_gpio=of_get_named_gpio(nd,"led-gpio",0);
    if(!gpio_is_valid(myled.led_gpio)){
        printk("can't find led-gpio");
        return -ENODEV;
    }
    printk("led-gpio:%d\n",myled.led_gpio);

    ret = gpio_request(myled.led_gpio,"ps_led");
    if(ret){
        printk("request gpio failed\n");
        return -ENODEV;
    }
    printk("request gpio success\n");

    ret = of_property_read_string(nd,"default-state",&str);
    if(!ret)
    {
        if(strcmp(str,"on"))
            myled.led_flag = 1;
        else
            myled.led_flag = 0;            
    }else{
        myled.led_flag = 0;
    }

    gpio_direction_output(myled.led_gpio,1);

    spin_lock_init(&myled.lock);
    printk("myled_init end\n");

    return 0;
}


static int led_probe(struct platform_device *pdev)
{
    int ret;
    ret = myled_init(pdev->dev.of_node);
    if(ret)
        return ret;

    // ��ӡ��ǰ���̵� PID
    printk("led_probe Current process PID: %d\n", current->pid);
    
    return 0;
}

static int led_remove(struct platform_device *pdev)
{
    gpio_set_value(myled.led_gpio,0);
    gpio_free(myled.led_gpio);
    printk("led_remove\n");
    return 0;
}




/*ƥ���б�*/
static const struct of_device_id led_of_match[] = {
    { .compatible = "gpio-leds", },
    { /* Sentinel */ }
};

/*platform�����ṹ��*/
static struct platform_driver led_driver = {
    .driver = {
        .name = "led_platform",
        .owner = THIS_MODULE,
        .of_match_table = led_of_match,
    },
    .probe = led_probe,
    .remove = led_remove,
};

module_platform_driver(led_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("lvd");
MODULE_DESCRIPTION("led driver");
