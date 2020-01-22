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

/* ����/����/ע��һ��platform_device */

static struct resource led_resource[] = {
    [0] = {	//�ڴ���Դ
        .start = 0x56000050,    //��ʼpa
        .end   = 0x56000050 + 8 - 1,    //����pa, һ���Ƶ�con��dat
        .flags = IORESOURCE_MEM,    //.flags ��Դ����
    },
    [1] = {	//�ж���Դ
        .start = 5, //�ĸ�����
        .end   = 5,
        .flags = IORESOURCE_IRQ,
    }
};

static void led_release(struct device * dev)	//����дӲ����ش��룬����ֻ���ṩ�˺�������
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
	platform_device_register(&led_dev); //����device_add
	return 0;
}

static void led_dev_exit(void)
{
	platform_device_unregister(&led_dev);
}

module_init(led_dev_init);
module_exit(led_dev_exit);
MODULE_LICENSE("GPL");

