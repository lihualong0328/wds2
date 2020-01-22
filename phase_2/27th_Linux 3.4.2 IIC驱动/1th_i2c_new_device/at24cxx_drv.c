#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>


static int __devinit at24cxx_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static int __devexit at24cxx_remove(struct i2c_client *client)
{
	printk("%s %s %d\n", __FILE__, __FUNCTION__, __LINE__);
	return 0;
}

static const struct i2c_device_id at24cxx_id_table[] = {
	{ "at24c08", 0 },	// driver_data=0, 不用, 随便写
	{}
};


/* 1. 分配/设置i2c_driver */
static struct i2c_driver at24cxx_driver = {
	.driver	= {
		.name	= "100ask",
		.owner	= THIS_MODULE,
	},
	.probe		= at24cxx_probe,
	.remove		= __devexit_p(at24cxx_remove),
	.id_table	= at24cxx_id_table,										// match()是根据此name
};

static int at24cxx_drv_init(void)
{
	/* 2. 注册i2c_driver */
	i2c_add_driver(&at24cxx_driver);							// 把i2c_driver挂载右边，从左边取i2c_client依次比较
	
	return 0;
}

static void at24cxx_drv_exit(void)
{
	i2c_del_driver(&at24cxx_driver);
}


module_init(at24cxx_drv_init);
module_exit(at24cxx_drv_exit);
MODULE_LICENSE("GPL");


