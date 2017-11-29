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
#include <asm/io.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <linux/poll.h>
#include <linux/mm.h>
#include <linux/sched.h>

#include<linux/string.h>

#include <linux/miscdevice.h>

#define DEBUG

#ifdef DEBUG 
#define LOGD(fmt...) printk(KERN_EMERG fmt)
#else 
#define LOGD(fmt...) 
#endif

 
#define AUX_DET_GPIO 422
#define DEVICE_NAME "aux_det_dev"

#define  AUX_IN  1
#define  AUX_OUT 0

struct work_struct aux_det_work_in;
struct work_struct aux_det_work_out;

typedef struct 
{																	//data for kernel
	int is_insert;   												//判断插入状态：0 没插入   1 插入
	struct fasync_struct *fasync_queue;								//异步通知信号
  
}ASYNC_AUX_DET ;

static ASYNC_AUX_DET *sync_devp = NULL;


typedef struct 
{
	int is_aux_used;			//data for  app 
	
}AUX_STATUS;


static AUX_STATUS aux_status_ex = {
	.is_aux_used = 0,
};


static void aux_det_work_func_in(struct work_struct *work)
{
	if(sync_devp->is_insert ==1 )
	{
		if (&sync_devp->fasync_queue)
		{
			aux_status_ex.is_aux_used  = AUX_IN;
			kill_fasync(&sync_devp->fasync_queue, SIGIO, POLL_IN);
			printk(KERN_EMERG  "<schedule_work>send a fsync signal in[1]----> !!!\n");
			
		}
	}
	
}

static void aux_det_work_func_out(struct work_struct *work)
{
	if(sync_devp->is_insert == 0 )
	{
		if (&sync_devp->fasync_queue)
		{
			aux_status_ex.is_aux_used  = AUX_OUT;
			kill_fasync(&sync_devp->fasync_queue, SIGIO, POLL_IN);
			printk(KERN_EMERG  "<schedule_work>send a fsync signal out[0]--> !!!\n");
		}
	}
	
}


irqreturn_t aux_sw_irq_handler(int irq,void *dev_id)
{
	
	int value = gpio_get_value(AUX_DET_GPIO);
	if(value == 1){
		
		if(sync_devp->is_insert == 0)
		{	
			printk(KERN_EMERG "------------<irq  value = 1 insert>\n");		
			sync_devp->is_insert = 1;
			schedule_work(&aux_det_work_in);
		}
	}
	
	if(value == 0){
		
		if(sync_devp->is_insert == 1)
		{
			printk(KERN_EMERG "------------<irq   value = 0 out of  device>\n");		
			sync_devp->is_insert = 0;
			schedule_work(&aux_det_work_out);
		}
		
	}
	
        return IRQ_HANDLED;
	
}



int sync_open(struct inode *inode, struct file *filp)
{
    filp->private_data = sync_devp;
    printk("BSP：sync_open： ok!\n");
    return 0; 
}

int  sync_read(struct file* filp, char *buffer, size_t length, loff_t *offset)
{   
	
	printk(KERN_EMERG "--------read--out :[0] in :[1] === [status = %d] -----\n",aux_status_ex.is_aux_used);     
	
	if (copy_to_user((AUX_STATUS __user *)buffer,&aux_status_ex,sizeof(AUX_STATUS)))
	{
		LOGD("copy_to_user the failed !!!\n");
		return -1;
	}
	
	return 0;
}
int sync_write(struct file* filp, const char *buff, size_t len, loff_t *off)
{        
	return 0;
}

static int sync_fasync(int fd, struct file * filp, int on) 
{
    int retval;
    ASYNC_AUX_DET *dev = filp->private_data;

    retval=fasync_helper(fd,filp,on,&dev->fasync_queue);
    if(retval<0)
      return retval;
    return 0;
}

int sync_release(struct inode *inode, struct file *filp)
{
    ASYNC_AUX_DET *dev = filp->private_data;
	
    sync_fasync(-1, filp, 0);     
    return 0;
}



static const struct file_operations fsync_fops =
{
    .owner = THIS_MODULE,
    .open = sync_open,
	.read = sync_read,
	.write = sync_write,
    .release = sync_release,
    .fasync = sync_fasync, /* 必须要的  */
};

static struct miscdevice misc = {
 .minor = MISC_DYNAMIC_MINOR,
 .name = DEVICE_NAME,
 .fops = &fsync_fops,
};


//enter 
int __init aux_det_init(void)
{
    int ret ;
	

	sync_devp = kmalloc(sizeof(ASYNC_AUX_DET),GFP_KERNEL);    
    if(!sync_devp)
    {
        return -1;
    }
    memset(sync_devp,0,sizeof(ASYNC_AUX_DET));
	
	//初始化设备，当前状态没有插入
	sync_devp->is_insert = 0;
	
	
	// app notify
	misc_register(&misc);
	
	
    INIT_WORK(&aux_det_work_in, aux_det_work_func_in);
	
	INIT_WORK(&aux_det_work_out, aux_det_work_func_out);
	
    gpio_direction_input(AUX_DET_GPIO);
	
	//注册中断
	ret=request_irq(gpio_to_irq(AUX_DET_GPIO), aux_sw_irq_handler, IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING, "aux_det",NULL);
	
    if (ret<0) 
    {
    	printk(KERN_EMERG "------------request irq aux failed\n");		
		return -1;
    }

    return 0;
}


void __exit aux_det_exit(void)
{
	misc_deregister(&misc);
    printk(KERN_EMERG " aux  has been destroyed\n");
}


module_init(aux_det_init);
module_exit(aux_det_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("jczhang");
