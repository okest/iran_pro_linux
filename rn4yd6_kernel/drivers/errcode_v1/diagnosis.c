#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/poll.h>

#include <asm/io.h>
#include <asm/uaccess.h>

//设计一个故障码类型的结构体--类似于一个数据包，给用户
struct dia_event{
	char dia_head;
	char dia_code;
	char dia_para;
	char dia_count;
};

//上报数据的结构体
struct ERRCODE{
	dev_t  devno;
	struct cdev *cdev;
	struct class *cls;
	struct device *dev;
	struct dia_event event;
	wait_queue_head_t wq_head;
	int dia_state; //描述按键是否有数据
};

//定义一个对象
struct ERRCODE *err_dev;

int errcode_repo(char dia_code,char dia_para)
{
	wake_up_interruptible(&err_dev->wq_head);
	err_dev->dia_state = 1;//有数据了
	
	//填充数据
	err_dev->event.dia_head = 0xa0;
	err_dev->event.dia_code = dia_code;
	err_dev->event.dia_para = dia_para;
	err_dev->event.dia_count = 1;
	//printk(KERN_EMERG "[BSP] new data is comming ----------\n");
	return 0;
}
EXPORT_SYMBOL(errcode_repo);


//  read(fd, buf, size);
ssize_t dia_drv_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	
	int ret;
	////printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	//printk(KERN_EMERG "read 1 ------err_dev->dia_state =%d \n",err_dev->dia_state);
	
	//实现非阻塞
	if((filp->f_flags & O_NONBLOCK) &&  !err_dev->dia_state)
			return -EAGAIN;

	//如果没有数据就休眠
	wait_event_interruptible(err_dev->wq_head, err_dev->dia_state);

	ret = copy_to_user(buf, &err_dev->event,count);
	if(ret > 0)
	{
		//printk("copy_to_user  error\n");
		return -EFAULT;
	}

	//清空这次数据，以准备下次数据
	memset(&err_dev->event,0,sizeof(struct dia_event));
	err_dev->dia_state = 0;// 这次的数据已经处理完，下次继续等
	//printk(KERN_EMERG "read 2 ------err_dev->dia_state =%d \n",err_dev->dia_state);

	return count;
}

// write(fd, buf, size);
ssize_t dia_drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{

	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	return 0;

}

int dia_drv_open(struct inode *inode, struct file *filp)
{
	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	
	return 0;
}

int dia_drv_close (struct inode *inode, struct file *filp)
{
	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	return 0;
}




unsigned int dia_drv_poll (struct file *filp, struct poll_table_struct *pts)
{
	int mask = 0;
	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	// 在有数据的时候返回一个POLLIN
	//没有数据的时候返回一个0
	

	// 将当前的等待队列头注册到vfs层
	poll_wait(filp, &err_dev->wq_head, pts);

	//printk(KERN_EMERG "------err_dev->dia_state =%d \n",err_dev->dia_state);
	if(err_dev->dia_state != 0){
		//printk("=========mask\n");
		mask |= POLLIN;
	}
	//printk(KERN_EMERG "[BSP]---------*_*- %s poll_wait after----------\n", __FUNCTION__);
	return mask;
}


const struct file_operations dia_fops = {
	.open = dia_drv_open,
	.release = dia_drv_close,
	.write = dia_drv_write,
	.read = dia_drv_read,
	.poll = dia_drv_poll,
	
};


static int __init diagnosis_drv_init(void)
{
	int ret;
	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	
	// 0，构建一个全局设备对象--面向对象
	// GFP_KERNEL如果内存暂时分配不到的话，就需要等待
	err_dev = kzalloc(sizeof(struct ERRCODE), GFP_KERNEL);
	if(err_dev == NULL)
	{	
		//printk(KERN_ERR "kmalloc error\n");
		return -ENOMEM;
	}

	// 1， 申请主设备号
	ret = alloc_chrdev_region(&err_dev->devno, 0,  1, "dia_drv");
	if(ret < 0){
		//printk("register/alloc_chrdev_region error\n");
		goto err_0;
	}

	//构建cdev和注册cdev
	err_dev->cdev = cdev_alloc();
	cdev_init(err_dev->cdev, &dia_fops);
	cdev_add(err_dev->cdev,err_dev->devno,1);

	
	// 2, 创建设备节点
	//  /sys/class/diagnosis_cls
	err_dev->cls = class_create(THIS_MODULE, "diagnosis_cls");
	if(IS_ERR(err_dev->cls))
	{
		//printk(KERN_ERR "class_create error\n");
		ret = PTR_ERR(err_dev->cls); //将错误的指针转换成一个出错码
		goto err_1;
	}

	err_dev->dev = device_create(err_dev->cls, NULL, err_dev->devno, NULL, "diagnosis");
	if(IS_ERR(err_dev->dev))
	{
		//printk(KERN_ERR "device_create error\n");
		ret = PTR_ERR(err_dev->dev); //将错误的指针转换成一个出错码
		goto err_2;
	}



	init_waitqueue_head(&err_dev->wq_head);
	memset(&err_dev->event,0,sizeof(struct dia_event));
	err_dev->dia_state = 0;

	return 0;

//err_3:
//	device_destroy(err_dev->cls, err_dev->devno);	
	
err_2:
	class_destroy(err_dev->cls);

err_1:
	cdev_del(err_dev->cdev);
	unregister_chrdev_region(err_dev->devno, 1);

err_0:
	kfree(err_dev);
	return ret;

}


static void __exit diagnosis_drv_exit(void)
{
	//printk(KERN_EMERG "[BSP]---------*_*- %s----------\n", __FUNCTION__);
	//模块卸载函数一般来说都是完成资源的释放--都是void
	device_destroy(err_dev->cls, err_dev->devno);	
	class_destroy(err_dev->cls);
	cdev_del(err_dev->cdev);
	unregister_chrdev_region(err_dev->devno, 1);
	kfree(err_dev);
	
}


module_init(diagnosis_drv_init);
module_exit(diagnosis_drv_exit);
MODULE_LICENSE("GPL");

