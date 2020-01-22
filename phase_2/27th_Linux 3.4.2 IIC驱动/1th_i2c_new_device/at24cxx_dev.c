#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/slab.h>


static struct i2c_board_info at24cxx_info = {	
	I2C_BOARD_INFO("at24c08", 0x50),					// mini2440中是0x50
};
static struct i2c_client *at24cxx_client;

static int at24cxx_dev_init(void)
{
	struct i2c_adapter *i2c_adap;

	i2c_adap = i2c_get_adapter(0);					// 用哪个i2c控制器/适配器发出i2c信号，2440只有一个i2c控制器/适配器
	at24cxx_client = i2c_new_device(i2c_adap, &at24cxx_info);				// 在此总线下创建新设备，以后用此总线发出i2c信号
	i2c_put_adapter(i2c_adap);							// 用完i2c控制器/适配器之后
	
	return 0;
}

static void at24cxx_dev_exit(void)
{
	i2c_unregister_device(at24cxx_client);
}


module_init(at24cxx_dev_init);
module_exit(at24cxx_dev_exit);
MODULE_LICENSE("GPL");


