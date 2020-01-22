#include <linux/module.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/sched.h>
#include <linux/pm.h>
#include <linux/sysctl.h>
#include <linux/proc_fs.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/irq.h>
#include <asm/uaccess.h>
#include <asm/io.h>

/* ����/����/ע��һ��platform_driver */

static int major;
static struct class *cls;
static volatile unsigned long *gpio_con;
static volatile unsigned long *gpio_dat;
static int pin;

static int led_open(struct inode *inode, struct file *file)
{
	//printk("first_drv_open\n");
	/* ����Ϊ��� */
	*gpio_con &= ~(0x3<<(pin*2));
	*gpio_con |= (0x1<<(pin*2));
	return 0;	
}

static ssize_t led_write(struct file *file, const char __user *buf, size_t count, loff_t * ppos)
{
	int val;

	//printk("first_drv_write\n");

	copy_from_user(&val, buf, count);

	if (val == 1)
	{
		// ���
		*gpio_dat &= ~(1<<pin);
	}
	else
	{
		// ���
		*gpio_dat |= (1<<pin);
	}
	
	return 0;
}

static struct file_operations led_fops = {
    .owner  =   THIS_MODULE,    /* ����һ���꣬�������ģ��ʱ�Զ�������__this_module���� */
    .open   =   led_open,     
	.write	=	led_write,	   
};

static int led_probe(struct platform_device *pdev)
{
	/* ����platform_device����Դ����ioremap */
	struct resource *res;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);	//0:������Դ�ĵ�0��
	
	gpio_con = ioremap(res->start, res->end - res->start + 1);
	gpio_dat = gpio_con + 1;

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);	//0:������Դ�ĵ�0��
	pin = res->start;	//led_dev.c�������ĸ���

	/* ע���ַ��豸�������� */
	printk("led_probe, found led\n");
	major = register_chrdev(0, "myled", &led_fops);
	cls = class_create(THIS_MODULE, "myled");
	class_device_create(cls, NULL, MKDEV(major, 0), NULL, "led"); /* /dev/led */
	
	return 0;
}

static int led_remove(struct platform_device *pdev)
{
	printk("led_remove, remove led\n");
	class_device_destroy(cls, MKDEV(major, 0));
	class_destroy(cls);

	/* ж���ַ��豸�������� */
	unregister_chrdev(major, "myled");
	iounmap(gpio_con);
	
	return 0;
}

struct platform_driver led_drv = {
	.probe		= led_probe,
	.remove		= led_remove,
	.driver		= {
		.name	= "myled",
	}
};

static int led_drv_init(void)
{
	platform_driver_register(&led_drv);
	return 0;
}

static void led_drv_exit(void)
{
	platform_driver_unregister(&led_drv);
}

module_init(led_drv_init);
module_exit(led_drv_exit);
MODULE_LICENSE("GPL");
