#include <linux/debug_uart.h>

#define NUMBYTES 1024



static unsigned char communicate_buf[NUMBYTES];
wait_queue_head_t read_wait_queue;

static int __uart3_init_kthread(void *data)
{
	unsigned long ul_jiffies = msecs_to_jiffies(100);
	schedule_timeout_interruptible(ul_jiffies);
	memset(uart_recv_buf,0,UART_RX_BUF_SIZE);
	debug_uart_switch_to_pio();
	debug_uart_port_startup();
	debug_uart_switch_recovery();
	debug_uart_recv_work_init();
	return 0;
}

int uart_debug_drv_open(struct inode *inode, struct file *filp)
{	
	LOGD("------------------uart_debug_drv_open------------------------");
	//itfc_startup_uart();
	//debug_sirfsoc_uart_startup();
	return 0;
}

int uart_debug_drv_close(struct inode *inode, struct file *filp)
{	
	LOGD("-------------------uart_debug_drv_close-----------------------");
	return 0;
}

ssize_t uart_debug_drv_write(struct file *filp, const char __user *buf, size_t count, loff_t *fpos)
{
	LOGD("-----------------uart_debug_drv_write----------count = %d---------------",count);
	memset(communicate_buf,0,NUMBYTES);
	if (!copy_from_user(communicate_buf, (unsigned char *)buf, count))
	{
		LOGD("the user write data is = %s",communicate_buf);
		debug_uart_serial_tx(communicate_buf,count);
		return count;
	}
	return -1;
}

ssize_t  uart_debug_drv_read(struct file *filp, char __user *buf, size_t count, loff_t *fpos)
{
	struct sk_buff *skb; 
//	int bytes_len = 0;
	
	LOGD("uart_debug_drv_read--------skb_queue_empty(&uart_recv_queue) = %d",skb_queue_empty(&uart_recv_queue));
#if 0	
	if (skb_queue_empty(&uart_recv_queue)) 
	{
		LOG_ERR("------------- skb_queue_empty is empty---------------");
		return 0;
	}
#else
	 wait_event_interruptible(read_wait_queue, skb_queue_empty(&uart_recv_queue) == false);
	
#endif
	
	skb = skb_dequeue(&uart_recv_queue); 
	if (!skb) { 
		LOG_ERR("skb_dequeue is error");
		return -EIO; 
	} 

	//pr_err("skb->data = %s\n",skb->data);
	//print_hex_dump(KERN_ERR,"",DUMP_PREFIX_OFFSET,16,1,skb->data,skb->len,1);
#if 1
	//skb->data[skb->len - 1] = '\0';
	if (copy_to_user(buf, skb->data, skb->len)) { 
		LOG_ERR("copy_to_user is fault!");
		return -EFAULT; 
	} 
#else
	memset(communicate_buf,0,NUMBYTES);
	strncpy(communicate_buf,skb->data,skb->len);
	bytes_len = skb->len;
	if (copy_to_user(buf, communicate_buf, bytes_len)) { 
		LOG_ERR("copy_to_user is fault!");
		return -EFAULT; 
	} 
#endif	
    
	kfree_skb(skb);
	return skb->len;
}


const struct file_operations uart_debug_fops = {
	.open = uart_debug_drv_open,
	.release = uart_debug_drv_close,
	.read = uart_debug_drv_read,
	.write = uart_debug_drv_write,
};

static struct miscdevice uart_debug_cdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = DEVICE_NAME,       
	.fops    = &uart_debug_fops,          
};


static int __init uart_itfc_init(void)
{
	int ret;
	
	memset(communicate_buf,0,NUMBYTES);
	init_waitqueue_head(&read_wait_queue);

	ret = misc_register(&uart_debug_cdev);
	if (ret) {
		LOG_ERR("failed to misc_register");
		goto misc_exit;
	}
	kthread_run(__uart3_init_kthread, NULL, "uart3_init");
	
	return 0;
misc_exit:
	misc_deregister(&uart_debug_cdev);
	return -1;
}

static void __exit uart_itfc_exit(void)
{
	misc_deregister(&uart_debug_cdev);
	debug_uart_port_close();
	debug_uart_recv_work_deinit();
}

module_init(uart_itfc_init);
module_exit(uart_itfc_exit);

MODULE_DESCRIPTION("The Uart Interface driver");
MODULE_LICENSE("GPL v2");
