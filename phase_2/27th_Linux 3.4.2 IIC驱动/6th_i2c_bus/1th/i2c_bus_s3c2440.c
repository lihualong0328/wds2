
static int s3c2440_i2c_xfer(struct i2c_adapter *adap,
			struct i2c_msg *msgs, int num)
{
	// 难点：如何发出i2c数据, 板子相关
}

static u32 s3c2440_i2c_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL | I2C_FUNC_PROTOCOL_MANGLING;						// I2C_FUNC_SMBUS_EMUL: smbus是i2c_transfer里的一小部分
}


static const struct i2c_algorithm s3c2440_i2c_algo = {
//	.smbus_xfer     = ,													// smbus是i2c的子集, 如果struct i2c_adapter支持smbus_xfer, 给.smbus_xfer赋值.
	.master_xfer	= s3c2440_i2c_xfer,																// 如果struct i2c_adapter不支持smbus_xfer, 用.master_xfer也可支持(模拟I2C_FUNC_SMBUS_EMUL).smbus_xfer, 
	.functionality	= s3c2440_i2c_func,
};

/* 1. 分配/设置i2c_adapter
 */
static struct i2c_adapter s3c2440_i2c_adapter = {
 .name			 = "s3c2440_100ask",
 .algo			 = &s3c2440_i2c_algo,
 .owner 		 = THIS_MODULE,
};

static int i2c_bus_s3c2440_init(void)
{
	/* 2. 注册i2c_adapter */

	i2c_add_adapter(&s3c2440_i2c_adapter);						// 系统自动编号，2410使用i2c_add_numbered_adapter已编号
	
	return 0;
}

static void i2c_bus_s3c2440_exit(void)
{
	i2c_del_adapter(&s3c2440_i2c_adapter);	
}

module_init(i2c_bus_s3c2440_init);
module_exit(i2c_bus_s3c2440_init);
MODULE_LICENSE("GPL");


