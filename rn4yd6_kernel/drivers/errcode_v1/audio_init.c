#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/miscdevice.h>
#include <linux/kernel.h>  
#include <linux/uaccess.h>
#include <linux/kdev_t.h>


#define AUDIO_SLAVE_ADDRESS 0x40	
#define AUDIO_INIT  "audio_init"

unsigned char	audio_addr [19 ] = {0x01, 0x02, 0x03, 0x05, 0x06, 0x20, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x41, 0x44, 0x47, 0x51, 0x54, 0x57, 0x75};
unsigned char	audio_buf  [19 ] = {0xA4, 0x00, 0x01, 0x00, 0x00, 0x94, 0x80, 0x80, 0x80, 0x80, 0x80, 0X80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

struct audio_data
{
	unsigned char addr;		// i2c reg addr
	unsigned char data;		// i2c data: 	
};


typedef struct  audio_data AUDIO_DATA;


struct i2c_client *audio_client = NULL;

static int   audio_open (struct inode *inode, struct file *filp);
static int   audio_release(struct inode *inode, struct file *filp);
static ssize_t   audio_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t   audio_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);


static int i2c_read_buff(struct i2c_client *client,unsigned char *buf, int  len)
{
    struct i2c_msg msgs[2];
	int  ret = -1;
	
    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = client->addr;
    msgs[0].len   = 1;
    msgs[0].buf   = &buf[0]; 

	printk(KERN_EMERG"[%s][%d]=======> addr=0x%x len=0x%x flags =0x%x :",__func__, __LINE__,msgs[0].addr,msgs[0].len,msgs[0].flags);
    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = 1;
    msgs[1].buf   = &buf[1];  

	printk(KERN_EMERG"[%s][%d]=======> addr=0x%x len=0x%x flags =0x%x :",__func__, __LINE__,msgs[1].addr,msgs[1].len,msgs[1].flags);
	
    ret = i2c_transfer(client->adapter, msgs, 2);         
		
    return (ret == 1) ? len : ret; 
}

static int audio_open (struct inode *inode, struct file *filp)
{
	printk(KERN_EMERG " ------------------------>> audio_open  \n");
    return 0;
}

static int audio_release(struct inode *inode, struct file *filp)
{
    printk(KERN_EMERG " ------------------------>> audio_release	\n");
    return 0;
}

static ssize_t audio_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int ret = -1;
	AUDIO_DATA  audio_data_para;
	unsigned char u8Data[2];
	
	copy_from_user(&audio_data_para, (AUDIO_DATA __user *)buf, sizeof(audio_data_para));
	//memcpy(&audio_data_para, (AUDIO_DATA *)buf ,sizeof(audio_data_para));
		
		u8Data[0] = audio_data_para.addr;

		ret = i2c_read_buff(audio_client, u8Data, 2);
		
		printk(KERN_EMERG "read data is [%s] [%d] u8Data[0]=%x u8Data[1]=%x \n",__func__,__LINE__,u8Data[0],u8Data[1]);
		
		audio_data_para.data = u8Data[1];

	copy_to_user(((AUDIO_DATA __user *)buf), &audio_data_para, sizeof(audio_data_para));
		
	printk(KERN_EMERG " ------------------------>> audio_read	\n");
	return ret;	
}

static int i2c_write_buff(struct i2c_client *client,unsigned char  *buf,int  len)
{
    	int ret=-1; 
	struct i2c_msg msgs;
    
	int i=0;

	msgs.flags = !I2C_M_RD;
	msgs.addr  = client->addr;
	msgs.len   = len ;
	msgs.buf   = buf;

#if 1
	printk(KERN_EMERG"[%s][%d]=======> addr=0x%x len=0x%x flags =0x%x :",__func__, __LINE__,msgs.addr,msgs.len,msgs.flags);
	for(i = 0; i < msgs.len; i++)
	{
		printk(KERN_EMERG "  0x%x   ", msgs.buf[i] );  
	}
	printk(KERN_ALERT"  \n ");  
	
#endif	

	ret = i2c_transfer(client->adapter, &msgs, 1);
	
	return  ( ret == 1)?len:ret;  
}

static ssize_t audio_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int ret;

	AUDIO_DATA  audio_data_para;
	
	unsigned char u8Data[2];

	if (!copy_from_user(&audio_data_para, (AUDIO_DATA __user *)buf, sizeof(audio_data_para)))
	{		
		u8Data[0] = audio_data_para.addr;
		u8Data[1] = audio_data_para.data;
	
		ret = i2c_write_buff(audio_client, u8Data, 2);
	}
	
	printk(KERN_EMERG " ------------------------>> audio_write	\n");
	return ret;
}

//初始化地址
//addr [19 ] =   {0x01, 0x02, 0x03, 0x05, 0x06, 0x20, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x41, 0x44, 0x47, 0x51, 0x54, 0x57, 0x75};
//buf  [19 ] =   {0xA0, 0x00, 0x01, 0x00, 0x00, 0x94, 0x80, 0x80, 0x80, 0x80, 0x80, 0X80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


int count = 1;
int  audio_i2c_write(unsigned char  addr, unsigned char value) 
{
	
	int ret ;
	unsigned char u8Data[2];
	u8Data[0] = addr;
	u8Data[1] = value;
	ret =i2c_write_buff(audio_client,u8Data, 2);
	if( ret  < 0 )
	{
		printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
		return -1;
	}

	printk(KERN_EMERG "audio wirte times %d \n",count++);
	return 0;
}
	

static int audio_probe(struct i2c_client *client,const struct i2c_device_id *id)  
{	
	printk(KERN_EMERG "zjc ----audio_probe\n");
        int i;
	int  ret = 0;
	
	audio_client = client;

	printk(KERN_EMERG "audio_client   %p\n",audio_client);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) 
    	{
		printk(KERN_ALERT"I2C check functionality failed\n");
        	return -ENODEV;
    	}
	
	
	//音频通道初始化，输入通道+输出通道+低重音通道
        for (i=0;i<19;i++)
        {
                ret = audio_i2c_write(audio_addr[i],audio_buf[i]);
        }
	
#if 1 
	int c = 1000,b =1000;
	for(c; c > 0 ; c-- )
	{
		for(b; b> 0;b--)
		{
		}
	}
	unsigned char buf[2];
	buf[0] = 0x01;
	
	ret = i2c_read_buff(client, buf, 2);
	printk(KERN_EMERG "1 = 0x%x 2= 0x%x \n",buf[0],buf[1]);
	
	buf[0] = 0x02;

	ret = i2c_read_buff(client, buf, 2);
	printk(KERN_EMERG "1 = 0x%x 2= 0x%x \n",buf[0],buf[1]);


	buf[0] = 0x03;

	ret = i2c_read_buff(client, buf, 2);
	printk(KERN_EMERG "1 = 0x%x 2= 0x%x\n",buf[0],buf[1]);


	buf[0] = 0x05;
	
	ret = i2c_read_buff(client, buf, 2);
	printk(KERN_EMERG "1 = 0x%x 2= 0x%x \n",buf[0],buf[1]);
	
#endif 
 	return ret;
}

static int audio_remove(struct i2c_client *client)
{
	printk(KERN_EMERG " ------------------------>> audio_remove	\n");
    return 0;
}

static struct file_operations audio_i2c_fops = {
	.owner    = THIS_MODULE,
	.open     = audio_open,    
	.read 	  = audio_read,
	.write 	  = audio_write,   
	.release =  audio_release,
};

static struct miscdevice audio_i2c_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = AUDIO_INIT,       
	.fops    = &audio_i2c_fops,          
};

static const struct of_device_id audio_match_table[] = {
		{.compatible = "audio_init,audio",},
		{ },
};

static const struct i2c_device_id audio_id[] = {
    { AUDIO_INIT, 0 },
    { }
};

static struct i2c_driver audio_driver = {
    	.probe      = audio_probe,
    	.remove     = audio_remove,
	.id_table   = audio_id,
	    .driver = {
        	.name     = AUDIO_INIT,
	        .owner    = THIS_MODULE,
        	.of_match_table = audio_match_table,
   	},
};

static int __init audio_init(void)
{
	int ret =-1;
	
    i2c_add_driver(&audio_driver);

	ret = misc_register(&audio_i2c_miscdev);
	if (ret < 0) 
	{
		printk(KERN_EMERG " misc_register err\n");
		return 0; 
	}
    return 0; 
}

static void __exit audio_exit(void)
{
	//FMS_DBG("  \n");

    misc_deregister(&audio_i2c_miscdev);

  	i2c_del_driver(&audio_driver);

    if( audio_client != NULL )
    	i2c_unregister_device(audio_client);
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_DESCRIPTION("audio init and BASS  Driver");
MODULE_LICENSE("GPL");
