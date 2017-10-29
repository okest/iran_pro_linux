/*
 *  Copyright (C) 2017, Lvyou <lvyou@foryouge.com.cn>
 *  RN4Y56 analog audio source switcher driver
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under  the terms of the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the License, or (at your
 *  option) any later version.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/cdev.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>

#define AUDIO_SWITCHER_MAJOR 230
#define AUDIO_SWITCHER_MAGIC_NUM  'x'

//#ifdef  	RN4Y56_CTL_CONFIG
#if 0
#define OPEN_RADIO _IO(AUDIO_SWITCHER_MAGIC_NUM,1000)
#define OPEN_AUX _IO(AUDIO_SWITCHER_MAGIC_NUM,1001)
#define OPEN_DAB _IO(AUDIO_SWITCHER_MAGIC_NUM,1002)
#define OPEN_ISDB _IO(AUDIO_SWITCHER_MAGIC_NUM,1003)

#elif  1     //RM4YC5_CTL_CONFIG

#define OPEN_AUX _IO(AUDIO_SWITCHER_MAGIC_NUM,1000)
#define OPEN_RADIO _IO(AUDIO_SWITCHER_MAGIC_NUM,1001)
#define OPEN_DAB _IO(AUDIO_SWITCHER_MAGIC_NUM,1002)
#define OPEN_ISDB _IO(AUDIO_SWITCHER_MAGIC_NUM,1003)
	
#endif

dev_t devno;
static int audio_switcher_major = AUDIO_SWITCHER_MAJOR;
struct audio_switcher {
		struct cdev cdev;
		struct class *cls;
		struct device *dev;
		unsigned gpio[3];
		unsigned status;
		
};
struct audio_switcher *switcher;

ssize_t  audio_switcher_drv_read(struct file *filp, char __user *buf, size_t 
count, loff_t *fpos)
{	char state[3];
	struct audio_switcher *switcher=filp->private_data;
	int value_a,value_b,value_final;
	state[2]='\0';
	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	value_a=gpio_get_value(switcher->gpio[0]);
	value_b=gpio_get_value(switcher->gpio[1]);
	value_final=value_a*2+value_b;
	//dev_err(NULL,"---------*_*value_final= %d----------\n", value_final);
	switch(value_final){
		case 0:
			state[0]='0';
			break;
		case 1:
			state[0]='1';
			break;
		case 2:
			state[0]='2';
			break;
		case 3:
			state[0]='3';
			break;
		default:
			dev_err(NULL,"---------can't get state----------\n");
			return -EINVAL;
			}
	switch(switcher->status){
		case 0:
			state[1]='0';
			//dev_err(NULL,"---------state=  0 ----------\n");
			break;
		case 1:
			state[1]='1';
			//dev_err(NULL,"---------state=  1 ----------\n");
			break;
		default:
			dev_err(NULL,"---------can't get state----------\n");
			return -EINVAL;
			}
	//dev_err(NULL,"---------state = %s----------\n",state);
	if(copy_to_user(buf,state,count)){
	return -EFAULT;
	}
	return count;

}

int audio_switcher_drv_open(struct inode *inode, struct file *filp)
{	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	filp->private_data = switcher;
	return 0;

}
int audio_switcher_drv_close(struct inode *inode, struct file *filp)
{	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	return 0;

}
long audio_switcher_drv_ioctl (struct file *filp, unsigned int cmd, unsigned 
long arg)
{
	//int a,b;
	struct audio_switcher *switcher=filp->private_data;
	//dev_err(NULL,"---------*_*- %s----------\n", __FUNCTION__);
	switch(cmd){
		case OPEN_RADIO:
			//dev_err(NULL,"\n@@@@@@@@@ OPEN_RADIO @@@@@@@@\n");
			gpio_set_value(switcher->gpio[0],0);
			gpio_set_value(switcher->gpio[1],0);
			//a=gpio_get_value(switcher->gpio[0]);
			//b=gpio_get_value(switcher->gpio[1]);
			//dev_err(NULL,"---------*_*- a=%d,b=%d----------\n",a,b);
			break;
		case OPEN_AUX:
			//dev_err(NULL,"@@@@@@@@@ OPEN_AUX @@@@@@@@\n");
			gpio_set_value(switcher->gpio[0],0);
			gpio_set_value(switcher->gpio[1],1);
			//a=gpio_get_value(switcher->gpio[0]);
			//b=gpio_get_value(switcher->gpio[1]);
			//dev_err(NULL,"---------*_*- a=%d,b=%d----------\n",a,b);
			break;
		case OPEN_DAB:
			//dev_err(NULL,"\n@@@@@@@@@ OPEN_DAB @@@@@@@@\n");
			gpio_set_value(switcher->gpio[0],1);
			gpio_set_value(switcher->gpio[1],0);
			//a=gpio_get_value(switcher->gpio[0]);
			//b=gpio_get_value(switcher->gpio[1]);
			//dev_err(NULL,"---------*_*- a=%d,b=%d----------\n",a,b);
			break;
		
		case OPEN_ISDB:
			//dev_err(NULL,"\n@@@@@@@@@ OPEN_ISDB @@@@@@@@");
			gpio_set_value(switcher->gpio[0],1);
			gpio_set_value(switcher->gpio[1],1);
			//a=gpio_get_value(switcher->gpio[0]);
			//b=gpio_get_value(switcher->gpio[1]);
			//dev_err(NULL,"---------*_*- a=%d,b=%d----------\n",a,b);
			break;
		default:
			//dev_err(NULL,"*_*- %s\n@@@@@@@@@failed@@@@@@@@", __FUNCTION__);
			return -EINVAL;
		}
	return 0;
}

irqreturn_t audio_switcher_irq_handler(int irq,void *dev_id)
{
	switcher->status=gpio_get_value(switcher->gpio[2]);
	//dev_err(NULL,"*_*@@@@@@@@@ irq action  state=%d @@@@@@@@\n",switcher->status);
         return IRQ_HANDLED;
}

const struct file_operations audio_switcher_fops = {
	.open = audio_switcher_drv_open,
	.release = audio_switcher_drv_close,
	.read = audio_switcher_drv_read,
	.unlocked_ioctl =audio_switcher_drv_ioctl,
};

int audio_switcher_probe(struct platform_device *pdev)
{	
	
	int ret,err;
	//int a,b;
	devno = MKDEV(audio_switcher_major,0);
	//dev_err(&pdev->dev,"---------*_*- %s----------\n", __FUNCTION__);
	if (audio_switcher_major)
	{	
		ret	= register_chrdev_region(devno,1,"audioswitcher");
	}
	else
	{
		alloc_chrdev_region(&devno,0,1,"audioswitcher");
		audio_switcher_major = MAJOR(devno);
	}
	if (ret<0)
		return ret;
	switcher = kzalloc(sizeof(struct audio_switcher),GFP_KERNEL);
	if (!switcher){
		ret =-ENOMEM;
		goto fail_malloc;
		}
	devno = MKDEV(audio_switcher_major,0);
	cdev_init(&switcher->cdev,&audio_switcher_fops);
	switcher->cdev.owner = THIS_MODULE;
	err = cdev_add(&switcher->cdev,devno,1);
	if (err)
		dev_err(&pdev->dev,"Error %d adding audio switcher", err);
	
	switcher->cls = class_create(THIS_MODULE, "audio_switcher_cls");
	if(IS_ERR(switcher->cls))
		{
			printk(KERN_ERR "class_create error\n");
			ret = PTR_ERR(switcher->cls); //将错误的指针转换成一个出错码
			goto fail_malloc;
		}
	
	switcher->dev = device_create(switcher->cls, NULL,devno, NULL, "audio_switcher");	
	
	if(IS_ERR(switcher->dev))
		{
			printk(KERN_ERR "device_create error\n");
			ret = PTR_ERR(switcher->dev); //将错误的指针转换成一个出错码
			goto err_2;
		}
	
	
	//get gpio from DTS
	switcher->gpio[0]=of_get_named_gpio(pdev->dev.of_node, "SW_A", 0);
	switcher->gpio[1]=of_get_named_gpio(pdev->dev.of_node, "SW_B", 0);
	switcher->gpio[2]=of_get_named_gpio(pdev->dev.of_node, "SW_INT", 0); 

	//dev_err(&pdev->dev,"---------*_*-switcher->gpio[0]=%d,switcher->gpio[1]=%d----------\n",switcher->gpio[0],switcher->gpio[1]);
	ret = gpio_request(switcher->gpio[0], "SW_A");
      if (ret < 0) 
    {
        dev_err(&pdev->dev,"Failed to request GPIO:%d, ERRNO:%d", switcher->gpio[0], ret);
        ret = -ENODEV;
    }
	ret = gpio_request(switcher->gpio[1], "SW_B");
      if (ret < 0) 
    {
        dev_err(&pdev->dev,"Failed to request GPIO:%d, ERRNO:%d", switcher->gpio[1], ret);
        ret = -ENODEV;
    }
	ret = gpio_request(switcher->gpio[2], "SW_INT");
      if (ret < 0) 
    {
       dev_err(&pdev->dev,"Failed to request GPIO:%d, ERRNO:%d", switcher->gpio[2], ret);
        ret = -ENODEV;
    }
	gpio_direction_output(switcher->gpio[0],0);
	gpio_direction_output(switcher->gpio[1],0);
	gpio_direction_input(switcher->gpio[2]);

	switcher->status=gpio_get_value(switcher->gpio[2]);	
	//dev_err(&pdev->dev,"#########state = %d#############\n",switcher->status);

	ret=request_irq(gpio_to_irq(switcher->gpio[2]), audio_switcher_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "audio_switcher",NULL);   //申请中断

	if (ret<0) 
	{
	dev_err(&pdev->dev,"request irq failed");
        goto err_3;
	}
	platform_set_drvdata(pdev, switcher);
	return 0;
	
err_3:
		device_destroy(switcher->cls,devno); 
		
err_2:
		class_destroy(switcher->cls);

fail_malloc:
		unregister_chrdev_region(devno,1);
		kfree(switcher);
		return ret;

}
int audio_switcher_remove(struct platform_device *pdev)
{	
	struct audio_switcher *switcher = platform_get_drvdata(pdev);
	//dev_err(&pdev->dev,"---------*_*- %s----------\n", __FUNCTION__);
	device_destroy(switcher->cls,devno); 
	class_destroy(switcher->cls);
	unregister_chrdev_region(devno,1);
	kfree(switcher);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int audio_switcher_suspend(struct device *dev)
{
	return 0;
}

static int audio_switcher_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(audio_switcher_pm_ops,
			audio_switcher_suspend, audio_switcher_resume);

#define AUDIO_SWITCHER_PM_OPS (&audio_switcher_pm_ops)
#else
#define AUDIO_SWITCHER_PM_OPS NULL
#endif

#ifdef CONFIG_OF
static const struct of_device_id audio_switcher_match[] = {
	{ .compatible = "gpio-74HC4052", },
	{ },
};
#endif

static struct platform_driver audio_switcher_driver = {
	.probe	= audio_switcher_probe,
	.remove = audio_switcher_remove,
	.driver = {
		.name	= "audio_switcher",
		.owner	= THIS_MODULE,
		.pm	= AUDIO_SWITCHER_PM_OPS,
		.of_match_table = of_match_ptr(audio_switcher_match),
	},
};
module_platform_driver(audio_switcher_driver);

MODULE_AUTHOR("Lvyou <lvyou@foryouge.com.cn>");
MODULE_DESCRIPTION("audio switcher driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:audio_switcher");

