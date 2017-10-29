#include <linux/kernel.h>  
#include <linux/module.h>  
#include <linux/scatterlist.h>  
#include <linux/slab.h>  
#include <linux/idr.h>  
#include <linux/fs.h>  
#include <linux/poll.h>  
#include <linux/cdev.h>  
#include <linux/jiffies.h>  
#include <linux/mutex.h>  
#include <linux/wait.h>  
#include <linux/skbuff.h>  
#include <linux/sched.h>  
#include <linux/rpmsg.h>  
#include "rpmsg_rvc.h"

#define RPMSG_MAX_SIZE		(512 - sizeof(struct rpmsg_hdr))
#define DEV_NAME		"tty_rpmsg"
#define DEBUG
#ifdef DEBUG 
#define LOGD(format,...) printk(KERN_ERR "[File:%s][Line:%d] "format"\n",__FILE__, __LINE__,##__VA_ARGS__)
#else 
#define LOGD(format,...) 
#endif

struct rpmsg_cdev_port{
	struct cdev		cdev_port;
	struct device 	*dev;
	int minor;
	struct rpmsg_channel *rpdev;
	struct sk_buff_head queue;
	struct mutex lock;  
};

static struct rpmsg_cdev_port rpmsg_port;
static dev_t rpmsg_c_dev; 
static struct class *rpmsg_cdev_class; 

static int rpmsg_cdev_open(struct inode *inode, struct file *filp)
{
	struct rpmsg_cdev_port *rpcdev_port;
	rpcdev_port = container_of(inode->i_cdev, struct rpmsg_cdev_port, cdev_port);

	filp->private_data = rpcdev_port;

    pr_info("%s.%d rpmsg_cdev_open [comm=%s]\n", \
        __FUNCTION__, __LINE__, current->comm);
    
	return 0;
}

static int rpmsg_cdev_read(struct file *filp, char __user *buf, size_t len, loff_t *offp)
{
	struct rpmsg_cdev_port *rpcdev_port = filp->private_data;
	struct sk_buff *skb; 

//	pr_err("rpmsg_cdev_read is start!\n");
	if (skb_queue_empty(&rpcdev_port->queue)) 
	{
		return 0;
	}
	
	skb = skb_dequeue(&rpcdev_port->queue); 
	if (!skb) { 
		pr_err("skb_dequeue is error!\n");
		mutex_unlock(&rpcdev_port->lock); 
		return -EIO; 
	} 

//	pr_err("skb->len = %d\n",skb->len);
	if (copy_to_user(buf, skb->data, skb->len)) { 
		pr_err("copy_to_user is fault!\n");
		return -EFAULT; 
	} 
    
	kfree_skb(skb);
	return skb->len;
}

static int rpmsg_cdev_write(struct file *filp, const char __user *ubuf,size_t len, loff_t *offp) 
{
	int count, ret = 0;
	const unsigned char *tbuf;
	
	struct rpmsg_cdev_port *rpcdev_port = filp->private_data;
	
	if (NULL == ubuf) {
		pr_err("buf shouldn't be null.\n");
		return -ENOMEM;
	}

    //__ZGR_DEBUG_BUFF(ubuf, len);
	count = len;
	tbuf = ubuf;
//	pr_err("rpmsg_cdev_write len=%d,data=%s\n",count,tbuf);
	do {
		ret = rpmsg_send(rpcdev_port->rpdev, (void *)tbuf,
			count > RPMSG_MAX_SIZE ? RPMSG_MAX_SIZE : count);
		if (ret) {
			pr_err("rpmsg_send failed: %d\n", ret);
			return ret;
		}

		if (count > RPMSG_MAX_SIZE) {
			count -= RPMSG_MAX_SIZE;
			tbuf += RPMSG_MAX_SIZE;
		} else {
			count = 0;
		}
	} while (count > 0);

	return len;
}


static const struct file_operations rpmsg_cdev_ops = {
	.open		   = rpmsg_cdev_open, 
	.read		   = rpmsg_cdev_read, 
	.write		   = rpmsg_cdev_write, 
	.owner		   = THIS_MODULE,
};

static void rpmsg_cdev_driver_cb(struct rpmsg_channel *rpdev, void *data, int len,
						void *priv, u32 src)
{
	struct rpmsg_cdev_port *cport = &rpmsg_port;
	struct sk_buff *skb; 
	char *skbdata;
	
	if (len == 0)
		return;
#if 0
	pr_err("msg(<- src 0x%x) len %d\n", src, len);
	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
			data, len,  true);
#endif
    skbdata = (char *)data;

    if (rpmsg_rcv_msgmng(rpdev, data, len) == 0)
    {
        return;
    }

	skb = alloc_skb(len, GFP_KERNEL); 
	if (!skb) { 
		pr_err("alloc_skb err: %u\n", len);
		return;
	} 
	
	skbdata = skb_put(skb, len); 
	memcpy(skbdata,data,len); 

//	pr_err("skbdata = %s\n",skbdata);
	mutex_lock(&cport->lock); 
	skb_queue_tail(&cport->queue, skb); 
	mutex_unlock(&cport->lock); 	
}

static int rpmsg_cdev_probe(struct rpmsg_channel *rpdev)
{
	int err=0;
	int major=0, minor=1;
	int ret;
	struct rpmsg_cdev_port *cport = &rpmsg_port;
	
	unsigned char mcu_tbuf[12];//add rxhu
	const unsigned char *mcu_tmp;
	int mcu_count = sizeof(mcu_tbuf);

	dev_info(&rpdev->dev, "new channel: 0x%x -> 0x%x!\n",
			rpdev->src, rpdev->dst);

	mutex_init(&cport->lock); 
	skb_queue_head_init(&cport->queue);
	
	cdev_init(&cport->cdev_port,&rpmsg_cdev_ops);
	major = MAJOR(rpmsg_c_dev); 
	cport->cdev_port.owner = THIS_MODULE; 
	
	err = cdev_add(&cport->cdev_port, MKDEV(major, minor), 1); 
	if (err) { 
		pr_err("cdev_add failed: %d\n", err); 
		goto error; 
	} 

	cport->dev = device_create(rpmsg_cdev_class, &rpdev->dev, 
		MKDEV(major, minor), NULL, DEV_NAME); 
	if (IS_ERR(cport->dev)) { 
		err = PTR_ERR(cport->dev); 
		dev_err(&rpdev->dev, "device_create failed: %d\n", err); 
		goto error; 
	} 
	cport->rpdev = rpdev;
	cport->minor = minor;
	dev_set_drvdata(&rpdev->dev,cport);

    rpmsg_rcv_init(rpdev);
	LOGD("send the channel num to mcu");
	/*发送消息告知mcu通道号*/
	//0x6A 0xA6 0x00 0x00 0x00 0x00 0x02 0x06 0x01 0xF7
//	strncpy(mcu_tbuf,"send anaounce to the mcu",strlen("send anaounce to the mcu"));
#if 1
	mcu_tbuf[0] = 0xAA;
	mcu_tbuf[1] = 0xA6;
	mcu_tbuf[2] = 0x00;
	mcu_tbuf[3] = 0x00;
	mcu_tbuf[4] = 0x00;
	mcu_tbuf[5] = 0x00;
	mcu_tbuf[6] = 0x02;
	mcu_tbuf[7] = 0x06;
	mcu_tbuf[8] = 0x01;
	mcu_tbuf[9] = 0xF0;
	mcu_tmp = mcu_tbuf;
#else
	mcu_tbuf[0] = 0x6a ;
	mcu_tbuf[1] = 0xa6 ;
	mcu_tbuf[2] = 0x00 ;
	mcu_tbuf[3] = 0x00 ;
	mcu_tbuf[4] = 0x00 ;
	mcu_tbuf[5] = 0x00 ;
	mcu_tbuf[6] = 0x03 ;
	mcu_tbuf[7] = 0x04 ;
	mcu_tbuf[8] = 0x00 ;
	mcu_tbuf[9] = 0x04 ;

	mcu_tbuf[10] =  mcu_tbuf[2]+mcu_tbuf[3]+mcu_tbuf[4]+mcu_tbuf[5]+mcu_tbuf[6]+mcu_tbuf[7]+mcu_tbuf[8]+mcu_tbuf[9];
	mcu_tbuf[10] =(~ mcu_tbuf[10])+1;
	mcu_tmp = mcu_tbuf;
#endif
	do {
		ret = rpmsg_send(cport->rpdev, (void *)mcu_tmp,//需要保存rpcdev_port 指针
			mcu_count > RPMSG_MAX_SIZE ? RPMSG_MAX_SIZE : mcu_count);
		if (ret) {
			pr_err("rpmsg_send failed: %d\n", ret);
			return ret;
		}

		if (mcu_count > RPMSG_MAX_SIZE) {
			mcu_count -= RPMSG_MAX_SIZE;
			mcu_tmp += RPMSG_MAX_SIZE;
		} else {
			mcu_count = 0;
		}
	} while (mcu_count > 0);
    
	return 0;
error:
	cdev_del(&cport->cdev_port); 
	return err;
}

static void rpmsg_cdev_remove(struct rpmsg_channel *rpdev)
{
	struct rpmsg_cdev_port *cport = dev_get_drvdata(&rpdev->dev); 
	int major = MAJOR(rpmsg_c_dev);

	pr_err("rpmsg tty driver is removed\n");

    rpmsg_rcv_deinit();
    
	device_destroy(rpmsg_cdev_class, MKDEV(major, cport->minor)); 
	cdev_del(&cport->cdev_port); 
}

static struct rpmsg_device_id rpmsg_driver_cdev_id_table[] = {
	{ .name	= "rpmsg-client-sample" },
	{ },
};

MODULE_DEVICE_TABLE(rpmsg, rpmsg_driver_cdev_id_table);

static struct rpmsg_driver rpmsg_cdev_driver = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= rpmsg_driver_cdev_id_table,
	.probe		= rpmsg_cdev_probe,
	.callback	= rpmsg_cdev_driver_cb,
	.remove		= rpmsg_cdev_remove,
};

static int __init rpmsg_cdev_init(void)
{
	int ret;
	ret = alloc_chrdev_region(&rpmsg_c_dev, 0, 1, DEV_NAME); 
	if (ret) { 
		pr_err("alloc_chrdev_region failed: %d\n", ret); 
		goto out; 
	}
	
	rpmsg_cdev_class = class_create(THIS_MODULE, KBUILD_MODNAME); 
	if (IS_ERR(rpmsg_cdev_class)) { 
		ret = PTR_ERR(rpmsg_cdev_class); 
		pr_err("class_create failed: %d\n", ret); 
		goto unreg_region; 
	} 
	return register_rpmsg_driver(&rpmsg_cdev_driver); 
unreg_region: 
	unregister_chrdev_region(rpmsg_c_dev, 1); 
out: 
	return ret;  
}
module_init(rpmsg_cdev_init);

static void __exit rpmsg_cdev_fini(void)
{
	struct rpmsg_cdev_port *cport=&rpmsg_port;
	int major = MAJOR(rpmsg_c_dev); 
	unregister_rpmsg_driver(&rpmsg_cdev_driver);
	
	cdev_del(&cport->cdev_port);
	device_destroy(rpmsg_cdev_class, MKDEV(major, cport->minor)); 
	class_destroy(rpmsg_cdev_class); 
	unregister_chrdev_region(rpmsg_c_dev,1);
}
module_exit(rpmsg_cdev_fini);

MODULE_DESCRIPTION("Remote processor messaging sample client driver");
MODULE_LICENSE("GPL v2");
