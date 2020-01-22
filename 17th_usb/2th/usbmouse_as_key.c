/*
 * drivers\hid\usbhid\usbmouse.c
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/usb/input.h>
#include <linux/hid.h>

static struct usb_device_id usbmouse_as_key_id_table [] = {
	{ USB_INTERFACE_INFO(USB_INTERFACE_CLASS_HID, USB_INTERFACE_SUBCLASS_BOOT,
		USB_INTERFACE_PROTOCOL_MOUSE) },
	//{USB_DEVICE(0x1234,0x5678)},//֧��ĳ�����ҵ�ĳ����Ʒ
	{ }	/* Terminating entry */
};

static int usbmouse_as_key_probe(struct usb_interface *intf, const struct usb_device_id *id)
{
	//�ο�Usbmouse.c;ͨ��usb_interface�õ�usb_device => dev->usb_device_descriptor->idVendor
	struct usb_device *dev = interface_to_usbdev(intf);

	printk("found usbmouse!\n");
	//vendor id/device id������usb_device_decriptor��;������usb�豸ʱ,usb���������Ѱ�usb_device_decriptor����������;
	//��Щ��Ϣ��win��һ��
	printk("bcdUSB = %x\n", dev->descriptor.bcdUSB);
	printk("VID    = 0x%x\n", dev->descriptor.idVendor);
	printk("PID    = 0x%x\n", dev->descriptor.idProduct);
	
	return 0;
}

static void usbmouse_as_key_disconnect(struct usb_interface *intf)
{
	printk("disconnect usbmouse!\n");
}

/* 1. ����/����usb_driver */
static struct usb_driver usbmouse_as_key_driver = {
	.name		= "usbmouse_as_key_",
	.probe		= usbmouse_as_key_probe,
	.disconnect	= usbmouse_as_key_disconnect,
	.id_table	= usbmouse_as_key_id_table,
};

static int usbmouse_as_key_init(void)
{
	/* 2. ע�� */
	usb_register(&usbmouse_as_key_driver);
	return 0;
}

static void usbmouse_as_key_exit(void)
{
	usb_deregister(&usbmouse_as_key_driver);	
}

module_init(usbmouse_as_key_init);
module_exit(usbmouse_as_key_exit);
MODULE_LICENSE("GPL");

