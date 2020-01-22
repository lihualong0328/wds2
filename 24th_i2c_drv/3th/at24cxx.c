#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/mutex.h>

static unsigned short ignore[]      = { I2C_CLIENT_END };
static unsigned short normal_addr[] = { 0x50, I2C_CLIENT_END }; /* 地址值是7位 */

//强制认为存在此设备
static unsigned short force_addr[] = {ANY_I2C_BUS, 0x60, I2C_CLIENT_END};	//{在哪些总线上寻找此设备，设备地址，退出for}
static unsigned short * forces[] = {force_addr, NULL};
										
static struct i2c_client_address_data addr_data = {
	.normal_i2c	= normal_addr,  /* 要发出S信号和设备地址并得到ACK信号,才能确定存在这个设备 */
	.probe		= ignore,
	.ignore		= ignore,
	//.forces     = forces, /* 强制认为存在这个设备 */
};

static struct i2c_driver at24cxx_driver;

static int at24cxx_detect(struct i2c_adapter *adapter, int address, int kind)
{
	printk("at24cxx_detect\n");

	struct i2c_client *new_client;
	
	/* 构构一个 i2c_client: 以后收发数据就用 i2c_client */
	new_client = kzalloc(sizeof(struct i2c_client), GFP_KERNEL);
	new_client->addr    = address;
	new_client->adapter = adapter;
	new_client->driver  = &at24cxx_driver;
	strcpy(new_client->name, "at24cxx");

	i2c_attach_client(new_client);	//这样rmmod时才会detach_client
	
	return 0;
}

static int at24cxx_attach(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, at24cxx_detect);
}

static int at24cxx_detach(struct i2c_client *client)
{
	printk("at24cxx_detach\n");
	
	//必须free掉，否则第二次 insmod 失败
	i2c_detach_client(client);
	kfree(i2c_get_clientdata(client));

	return 0;
}


/* 1. 分配一个i2c_driver */
/* 2. 设置i2c_driver */
static struct i2c_driver at24cxx_driver = {
	.driver = {
		.name	= "at24cxx",
	},
	.attach_adapter = at24cxx_attach,
	.detach_client  = at24cxx_detach,
};

static int at24cxx_init(void)
{
	i2c_add_driver(&at24cxx_driver);
	return 0;
}

static void at24cxx_exit(void)
{
	i2c_del_driver(&at24cxx_driver);
}

module_init(at24cxx_init);
module_exit(at24cxx_exit);

MODULE_LICENSE("GPL");

