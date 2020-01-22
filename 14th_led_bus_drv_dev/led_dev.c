#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/list.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/serial_core.h>
#include <linux/platform_device.h>

/* 分配/设置/注册一个platform_device */

static struct resource led_resource[] = {
    [0] = {	//内存资源
        .start = 0x56000050,    //起始pa
        .end   = 0x56000050 + 8 - 1,    //结束pa, 一个灯的con和dat
        .flags = IORESOURCE_MEM,    //.flags 资源种类
    },
    [1] = {	//中断资源
        .start = 5, //哪个引脚
        .end   = 5,
        .flags = IORESOURCE_IRQ,
    }
};

static void led_release(struct device * dev)	//可以写硬件相关代码，这里只需提供此函数即可
{
}

static struct platform_device led_dev = {
    .name          = "myled",
    .id            = -1,
    .num_resources = ARRAY_SIZE(led_resource),
    .resource      = led_resource,
    .dev = { 
    	.release = led_release, 
	},
};

static int led_dev_init(void)
{
	platform_device_register(&led_dev); //调用device_add
	return 0;
}

static void led_dev_exit(void)
{
	platform_device_unregister(&led_dev);
}

module_init(led_dev_init);
module_exit(led_dev_exit);
MODULE_LICENSE("GPL");

