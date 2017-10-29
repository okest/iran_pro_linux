// $Change: 593163 $
//-----------------------------------------------------------------------------
//
// Copyright (c) 2008 MStar Semiconductor, Inc.  All rights reserved.
//
//-----------------------------------------------------------------------------
// FILE
//      ipodi2c.c
//
// DESCRIPTION
//      
//	  
// HISTORY
//-----------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Include Files
//------------------------------------------------------------------------------

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/poll.h>
#include <linux/wait.h>
#include <linux/cdev.h>
#include <linux/time.h>
#include <linux/timer.h>
#include <asm/io.h>
#include <linux/miscdevice.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include "mfi_i2c.h"

//#include <mach/pinmux_reg.h>
//#include <mach/gpio.h>
//#include "mach/ac83xx_gpio_pinmux.h"
//#include "mach/ac83xx_pinmux_table.h"

struct iic_param
{
    u8 ucIdIIC;      	// IIC ID: Channel 1~7
    u8 ucClockIIC;   	// IIC clock speed
    u8 ucSlaveIdIIC;    // Device slave ID
    u8 ucAddrSizeIIC;	// Address length in bytes
    u8 ucAddrIIC[4];	    /// Starting address inside the device
    u8 *pucBufIIC;     	/// buffer
    u32 dwDataSizeIIC;	/// size of buffer
};

typedef struct iic_param iic_param_t;
struct i2c_client *mfi_client = NULL;

static s32 i2c_read(struct i2c_client *client, char *reg_addr, char *buf, int len)
{
	int ret = 0, i;

	//printk(KERN_ALERT "=ipod ic =>i2c_read====\n");

	for(i=0; i<5; i++) 
	{
		if ((ret = i2c_master_send(client, reg_addr, 1)) > 0)
		{
			break;
		}
		else
		{
		    //printk(KERN_ALERT "i2c_master_send failed: %d\n", ret);
		}
	}

	for(i=0; i<5; i++) 
	{
		if ((ret = i2c_master_recv(client, buf, len)) > 0)
		{
			break;
		}
		else
		{
		    //printk(KERN_ALERT "i2c_master_recv failed: %d\n", ret);
		}
	}
    
	return ret;
}


static u8 i2c_write(struct i2c_client *client, char reg_addr, char *reg_val, int len)
{
    int ret;
	struct i2c_msg msg;
	//int ret = -1;
	// Note: 此处设置为128, 但实际上传下来的数据长度只能为127, 因为[0]用来存储寄存器地址.
	//       ipod鉴权最多传入长度为20的字符串.
	uint8_t buf[128] = "";

	buf[0] = reg_addr;
	memcpy(&buf[1], reg_val, len);

	msg.flags = !I2C_M_RD;
	msg.addr = client->addr;
	msg.len = len + 1;
	msg.buf = buf;

	ret = i2c_transfer(client->adapter, &msg, 1);
	if(ret == 1)
		ret = len;
	return ret;
	
	// 保留下面注释代码的用意: 
	//     通常读写寄存器都是1个字节,所以下面sizeof(tmp_buf)为2(1个字节的寄存器地址+1字节的写入数据).
	//     但ipod鉴权时最长会要求写入20个字节的数据,所以下面这种做法就有问题了
#if 0 
	int ret =0, i;
	u8 tmp_buf[2] = {0};

	tmp_buf[0] = reg_addr;
	tmp_buf[1] = reg_val;

	for(i=0; i<len+1; i++)
		pr_info(" %x \n", tmp_buf[i]);

	for(i=0; i<5; i++) {
		ret = i2c_master_send(client, tmp_buf, sizeof(tmp_buf));
		if (ret < 0)
			pr_err("ipod i2c write %x to reg:%x failed, ret:%d\n", *reg_val, *reg_addr, ret);
		else
			break;
	}
	if (i == 5)
		pr_err("i2c_write() failed, pls check device power!\n");

	return ret;
#endif 
}

static int mfi_i2c_open (struct inode *inode, struct file *filp)
{	 
	//pr_info("==> %s:%d\n", __func__, __LINE__);
	return 0;
	//gpio_set_value(PIN_125_GPIO125, 0);
	//msleep(50);
	//gpio_set_value(PIN_125_GPIO125, 1);
}

static ssize_t mfi_i2c_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	u32 u32RetCountIIC = 0;
	iic_param_t iic_read_param;
	//int i;
	
	//pr_info("\n==> start %s\n", __func__);

	if( copy_from_user(&iic_read_param, (iic_param_t *)buf, count) )
	{
		printk(KERN_ALERT "copy from user failed\n");
		return -1;
	}

	
	iic_read_param.pucBufIIC = kmalloc(iic_read_param.dwDataSizeIIC, GFP_ATOMIC);

	if( iic_read_param.pucBufIIC == NULL )
	{
	    printk(KERN_ALERT "mfi_i2c_read====>iic_read_param.pucBufIIC  kalloc erro\n");
		return -1;
	}

	//pr_info("\n==> start %s\n", __func__);
	u32RetCountIIC = i2c_read(mfi_client, iic_read_param.ucAddrIIC, 
					       iic_read_param.pucBufIIC,
					       iic_read_param.dwDataSizeIIC);

#if 0	
	printk(KERN_ALERT "=mfi ic =>i2c addr:%x reg:%x  DataSize:%d====", mfi_client->addr, iic_read_param.ucAddrIIC[0], iic_read_param.dwDataSizeIIC);
	for(i=0; i<iic_read_param.dwDataSizeIIC; i++)
	    printk(KERN_ALERT " 0x%02x", iic_read_param.pucBufIIC[i]);

	printk(KERN_ALERT "\n");
#endif

	//printk(KERN_ALERT "i2c_read: u32RetCountIIC = %d", u32RetCountIIC);


	if (u32RetCountIIC)
	{
		copy_to_user(((iic_param_t *)buf)->pucBufIIC, iic_read_param.pucBufIIC, iic_read_param.dwDataSizeIIC);
	} 
	else
	{
		printk(KERN_ALERT "==> mfi i2c read erro! i2c addr = 0x%02x, reg = %x\n", 
				    mfi_client->addr, iic_read_param.ucAddrIIC[0]);
	}

	kfree(iic_read_param.pucBufIIC);

	//pr_info("\n==> end %s\n", __func__);
	return u32RetCountIIC;
}

static ssize_t mfi_i2c_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = 0;
	int j;
	iic_param_t iic_write_param;

	//pr_info("\n==> start %s", __func__);

	if( copy_from_user(&iic_write_param, (iic_param_t *)buf, count) )
	{
		pr_err("copy from user failed\n");
		return -1;
	}

	iic_write_param.pucBufIIC = kmalloc(iic_write_param.dwDataSizeIIC, GFP_ATOMIC);

	if( iic_write_param.pucBufIIC == NULL )
	{
		return -1;
	}

	if( copy_from_user(iic_write_param.pucBufIIC, ((iic_param_t *)buf)->pucBufIIC, iic_write_param.dwDataSizeIIC) )
	{
		kfree(iic_write_param.pucBufIIC);
		return -1;
	}

    //最多尝试10次写入数据
	for(j=0; j<10; j++)
	{
		ret = i2c_write(mfi_client, iic_write_param.ucAddrIIC[0], 
							iic_write_param.pucBufIIC,
				           iic_write_param.dwDataSizeIIC);
		
		if(ret < 0)
		{
		    pr_err("==> mfi i2c write erro! i2c addr = 0x%02x, reg = %x\n", 
				    mfi_client->addr, iic_write_param.ucAddrIIC[0]);
			break;
		}
		else if(ret == iic_write_param.dwDataSizeIIC)
		{
		    //pr_info("==> mfi i2c write ok! reg = 0x%02x\n", iic_write_param.ucAddrIIC[0]);
		    break;
		}
		else
		{
		    pr_err("==> mfi i2c write incomplete! reg = 0x%02x, %d data\n", iic_write_param.ucAddrIIC[0], ret);
		}
	}

	kfree(iic_write_param.pucBufIIC);

	//pr_info("==> end %s\n\n", __func__);
	return ret;
}

static long mfi_i2c_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
#if 0 
	switch (cmd) {
		case HIDIOCGDEVINFO:
			{
				struct usb_device *dev = hid_to_usb_dev(hid);
				struct usbhid_device *usbhid = hid->driver_data;

				memset(&dinfo, 0, sizeof(dinfo));

				dinfo.bustype = BUS_USB;
				dinfo.busnum = dev->bus->busnum;
				dinfo.devnum = dev->devnum;
				dinfo.ifnum = usbhid->ifnum;
				dinfo.vendor = le16_to_cpu(dev->descriptor.idVendor);
				dinfo.product = le16_to_cpu(dev->descriptor.idProduct);
				dinfo.version = le16_to_cpu(dev->descriptor.bcdDevice);
				dinfo.num_applications = hid->maxapplication;

				r = copy_to_user(user_arg, &dinfo, sizeof(dinfo)) ?
					-EFAULT : 0;
				break;
			}
		case HIDIOCGVERSION:
			break;
		default:
			break;
	}

#endif 
	return 0;
}

static struct file_operations mfi_i2c_fops = {
	.owner    = THIS_MODULE,
	.open     = mfi_i2c_open,    
	.read 	  = mfi_i2c_read,
	.write 	  = mfi_i2c_write,   
	.unlocked_ioctl = mfi_i2c_ioctl,
};

static struct miscdevice mfi_i2c_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = DEVICE_NAME,       
	.fops    = &mfi_i2c_fops,          
};

static int mfi_i2c_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	return 0;
}
static int mfi_i2c_remove(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id ipod_i2c_id[] = {
	{ "ipod_i2c", 0 },
	{},
};

static struct i2c_driver mfi_i2c_driver = {
	.probe      = mfi_i2c_probe,
	.remove     = mfi_i2c_remove,
	.id_table   = ipod_i2c_id,
	.driver = {
		.name     = "ipod_i2c",
		.owner    = THIS_MODULE,
	},
};

struct device ipod_device;
static ssize_t ipod_sys_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	char val = 0x01;
	char reg = 0x10;
	char val_read=0;
    int ret;

	printk(KERN_ALERT "=ipod ic =>i2c ipod_sys_show  i2c_write====\n");
	i2c_write(mfi_client, reg, &val, 1);

    printk(KERN_ALERT "=ipod ic =>i2c ipod_sys_show  i2c_read====\n");
	ret = i2c_read(mfi_client, &reg, &val_read, 1);
	if (ret>0)
	{
		printk(KERN_ALERT "=ipod ic =>i2c addr:0x%02x reg:0x%02x data:0x%02x====\n", 
			              mfi_client->addr, reg, val_read);
	}
	
	return 0;
}

static ssize_t ipod_sys_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int j;
	char val[128],reg;
	int icount=0;
	int ret;


	reg = simple_strtol(buf, NULL, 16);
	//i2c_read(mfi_client, &reg, &val[0], 1);
	
	{
	    printk(KERN_ALERT "=apple mfi: test read  ic =>i2c addr:0x01-0x39 ====\n");
	    reg = 0x00;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x01;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x02;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x03;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x04;
        icount = 4;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x05;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x10;
        icount = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x11;
        icount = 2;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x12;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x20;
        icount = 2;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x21;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x30;
        icount = 2;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x31;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x32;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x33;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x34;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x35;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x36;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x37;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x38;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}

		reg = 0x39;
        icount = 128;
        ret = i2c_read(mfi_client, &reg, &val[0], icount);
		if (ret>0)
		{
			printk(KERN_ALERT "=ipod ic =>i2c addr:%x reg:%x ====\n", mfi_client->addr, reg);
			for(j=0; j<icount; j++)
			    printk(KERN_ALERT " 0x%02x", val[j]);

			printk(KERN_ALERT "\n");
		}
	}
	//printk(KERN_ALERT "==reg:%x=> %x\n ", reg, *val);
	return count;
}
static DEVICE_ATTR(ipod, S_IWUSR | S_IRUGO, ipod_sys_show, ipod_sys_store);

static int ipod_sys_dbg(void)
{
	int ret;

	dev_set_name(&ipod_device, "ipod");

	if (device_register(&ipod_device))
		printk("error device_register()\n");

	ret = device_create_file(&ipod_device, &dev_attr_ipod);
	if (ret)
		printk("device_create_file error\n");

        printk("mwave device register ok and device_create_file ok\n");
        return 0;
}

static int __init mfi_i2c_init(void)
{
	int ret, i;
	struct i2c_board_info info;
	struct i2c_adapter *adapter=NULL;
	unsigned short addr[2] = {0x10, 0x11};

	char val[128], reg = 0x30;
	int count=0;

	memset(&info, 0, sizeof(struct i2c_board_info));
	strlcpy(info.type, "ipod_i2c", 10);

	adapter = i2c_get_adapter(2);//MFI接在gpio模拟的i2c总线，总线为2
	if (!adapter)
	{
		pr_err("Can't get i2c adapter 2\n");
		return -ENODEV;
	}

	for(i=0; i<2; i++)
	{
		info.addr = addr[i];
		mfi_client = i2c_new_device(adapter, &info);
		if (!mfi_client)
		{
			pr_err("Can't add i2c device at 0x%x\n", info.addr);
			return -ENOMEM;
		}

		i2c_put_adapter(adapter);
	}

	ret = i2c_add_driver(&mfi_i2c_driver);

	ret = misc_register(&mfi_i2c_miscdev);
	if (ret < 0) 
		pr_err("Failed to register ipodi2c misc dev\n");

    //测试代码
	{
		reg = 0x02;
        count = 1;
        ret = i2c_read(mfi_client, &reg, &val[0], count);
		if (ret>0)
		{
			dev_info(&adapter->dev, "%s  addr:%x mfi reg:%x ====0x%02x\n", 
									mfi_i2c_driver.driver.name,
									mfi_client->addr, 
									reg, 
									val[0]);
		}
	}

	ipod_sys_dbg();
	
//err:
	return ret;
}

static void __exit mfi_i2c_exit(void)
{
    misc_deregister(&mfi_i2c_miscdev);
    i2c_del_driver(&mfi_i2c_driver);
    i2c_unregister_device(mfi_client);
}

module_init(mfi_i2c_init);
module_exit(mfi_i2c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("FGE");
MODULE_DESCRIPTION("IpodI2c Driver");

