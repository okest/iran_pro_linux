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
#include <linux/proc_fs.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/string.h>


#define AUDIO_SLAVE_ADDRESS 0x40	
#define AUDIO_INIT  "audio_init"
#define AUDIO_ASP_MUTE  426

//unsigned char	audio_addr [19 ] = {0x01, 0x02, 0x03, 0x05, 0x06, 0x20, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x41, 0x44, 0x47, 0x51, 0x54, 0x57, 0x75};
//unsigned char	audio_buf  [19 ] = {0xA4, 0x03, 0x09, 0x08, 0x00, 0x94, 0x80, 0x80, 0x80, 0x80, 0x80, 0X80, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};


unsigned char	audio_addr [19 ] = {0x01, 0x02, 0x03, 0x05, 0x06, 0x20, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x30, 0x41, 0x44, 0x47, 0x51, 0x54, 0x57, 0x75};
unsigned char	audio_buf  [19 ] = {0xE4, 0x03, 0x09, 0x08, 0x09, 0x89, 0x80, 0x80, 0x80, 0x80, 0x80, 0X80, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

//unsigned char	audio_addr [15 ] = {0x01, 0x02, 0x03, 0x05, 0x06, 0x20, 0x28, 0x29 ,0x2A, 0x2B, 0x2C, 0x30, 0x41, 0x51, 0x75};
//unsigned char	audio_buf  [15 ] = {0xA4, 0x0B, 0x09, 0x08, 0x0f, 0x86, 0x80, 0x80 ,0x80, 0x80, 0x80, 0x80, 0x33, 0x0f, 0x0f};

int gpio_asp_mute ;

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
	
#if 0 
	
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
#if 1

static struct proc_dir_entry *BD37033_debug_proc = NULL;

static ssize_t audio_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t audio_write_proc(struct file *, const char __user *, size_t, loff_t *);

static const struct file_operations audio_proc_ops = {
    .owner = THIS_MODULE,
    .read = audio_read_proc,
    .write = audio_write_proc,
};

static ssize_t audio_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	printk(KERN_EMERG "BD37033_debug_proc = %p\n",BD37033_debug_proc);
	return 0;
}
#define		B_Q_0_5		0x00 
#define		B_Q_1_0		0x01 
#define		B_Q_1_5		0x02 
#define		B_Q_2_0		0x03

#define		B_fo_60		0x00
#define		B_fo_80		0x10
#define		B_fo_100	0x20
#define		B_fo_120	0x30

#define		M_Q_0_75	0x00 
#define		M_Q_1_0		0x01 
#define		M_Q_1_25	0x02 
#define		M_Q_1_5		0x03

#define		M_fo_0_5k	0x00
#define		M_fo_1k		0x10
#define		M_fo_1_5k	0x20
#define		M_fo_2_5k	0x30

#define		T_Q_0_75	0x00 
#define		T_Q_1_25	0x01 


#define		T_fo_7_5k	0x00
#define		T_fo_10k	0x10
#define		T_fo_12_5k	0x20
#define		T_fo_15k	0x30

#define		ASP_B_gain_addr	0x51
#define		ASP_M_gain_addr	0x54
#define		ASP_T_gain_addr	0x57
unsigned char	ASP_EQ_addr [ 6 ] = { 	0x41, 	0x44, 	0x47, ASP_B_gain_addr, ASP_M_gain_addr, ASP_T_gain_addr };

unsigned char	POPS_data [ 6 ] = { B_fo_120 | B_Q_1_5 ,M_fo_1k | M_Q_1_0 ,   T_fo_7_5k | T_Q_0_75 , 0x05, 0x00, 0x03 };
unsigned char	CLASSIC_data [ 6 ] = { B_fo_80 | B_Q_1_0 ,M_fo_2_5k | M_Q_1_0 , T_fo_7_5k | T_Q_1_25 , 0x00, 0x05, 0x05 };
unsigned char	ROCK_data [ 6 ]	= { B_fo_60 | B_Q_1_0 , M_fo_0_5k | M_Q_1_25 , T_fo_15k | T_Q_1_25 ,  0x09, 0x13, 0x07 };
unsigned char	MOVIE_data [ 6 ] = { B_fo_120 | B_Q_0_5 ,M_fo_1k | M_Q_0_75 ,  T_fo_7_5k | T_Q_0_75 , 0x19, 0x05, 0x19 };
unsigned char	JAZZ_data [ 6 ]	 = { B_fo_60 | B_Q_0_5 , M_fo_0_5k | M_Q_1_25 ,T_fo_15k | T_Q_0_75 ,  0x08, 0x13, 0x05 };

#define	POPS_mode	22
#define	CLASSIC_mode	23
#define	ROCK_mode	24
#define	MOVIE_mode	25
#define	JAZZ_mode	26
#define ASP_mute_on     27
#define ASP_mute_off    28

static ssize_t audio_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	//com format : 01
	int ret;
	char recv[3];
	unsigned char u8Data[2];
	
    if (copy_from_user(recv,buffer,count))
        return -EFAULT;
	
	  recv[2] = '\0';

	if(strcmp(recv,"01") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x12;//q=1.5  f=80k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
					
	}
	if(strcmp(recv,"02") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x22;//q=1.5  f=100k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
			
		
	}
	if(strcmp(recv,"03") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x32;//q=1.5  f=120k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
	}
//---------------------------------------------------------------------------------
	if(strcmp(recv,"04") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x13;//q=2.0  f=80k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
	}
	if(strcmp(recv,"05") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x23;//q=2.0  f=100k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
	}
	if(strcmp(recv,"06") == 0 )
	{	 
		u8Data[0] = 0x41;
		u8Data[1] = 0x33;//q=2.0  f=120k
		ret =i2c_write_buff(audio_client,u8Data, 2);
		if( ret  < 0 )
		{
			printk(KERN_EMERG  " audio  i2c1_write_buff failed\n");
			return -1;
		}

		printk(KERN_EMERG " line = %d bass cmd  %s  \n",__LINE__, recv);
	}

	//-----------------EQ--in--------------------------
	//-----------------EQ--out-------------------------
	int mode;
	int i = 0;

	kstrtoint(recv,10,&mode);
	
	printk(KERN_EMERG "zjc  mode is  %d  (mode is  %d )\n",__LINE__, mode);
	switch(mode)
	{
		case (POPS_mode):
		{	
			 for (i=0;i<6;i++)
       			 {
              			  audio_i2c_write(ASP_EQ_addr[i],POPS_data[i]);
       			 }	
			 break;
		 }
		case (CLASSIC_mode): 
		{
			for (i=0;i<6;i++)
       			 {
              			  audio_i2c_write(ASP_EQ_addr[i],CLASSIC_data[i]);
       			 }	
						break;	
		 }
		case (ROCK_mode): 
		{
			for (i=0;i<6;i++)
       			 {
              			  audio_i2c_write(ASP_EQ_addr[i],ROCK_data[i]);
       			 }		
						break;
		 }
		case (MOVIE_mode): 
		{
			for (i=0;i<6;i++)
       			 {
              			  audio_i2c_write(ASP_EQ_addr[i], MOVIE_data[i]);
       			 }	
			break;
			
		 }
		case (JAZZ_mode): 
		{
			for (i=0;i<6;i++)
       			 {
              			  audio_i2c_write(ASP_EQ_addr[i],JAZZ_data[i]);
       			 }	
			break;
		 }
		case(ASP_mute_on):
		{
			//gpio_set_value(AUDIO_ASP_MUTE,0);
    			gpio_direction_output(AUDIO_ASP_MUTE, 0);
    			//int value = gpio_get_value(AUDIO_ASP_MUTE);
		 	//printk(KERN_EMERG "---------mute init value_mute = %d----------\n",value);
			break;
		}	
		case(ASP_mute_off):
		{
			
			//gpio_set_value(AUDIO_ASP_MUTE,1);
    			gpio_direction_output(AUDIO_ASP_MUTE,1);
    			//int value = gpio_get_value(AUDIO_ASP_MUTE);
    			//printk(KERN_EMERG "---------mute init value_mute = %d----------\n",value);
			break;
		}
	}
	return count;
}
//add  new func -----------------------------------------------------

static struct proc_dir_entry *addr_value_proc = NULL;

static ssize_t audio_addr_read_proc(struct file *, char __user *, size_t, loff_t *);
static ssize_t audio_addr_write_proc(struct file *, const char __user *, size_t, loff_t *);

static const struct file_operations audio_debug_proc_ops = {
    .owner = THIS_MODULE,
    .read = audio_addr_read_proc,
    .write = audio_addr_write_proc,
};
static ssize_t audio_addr_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
	printk(KERN_EMERG " addr_value_proc = %p\n",addr_value_proc);
	return 0;
}

static ssize_t audio_addr_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	
	char recv[128];
	char recv_2[128];		//addr
	char recv_3[128];		//value
	
	unsigned char addr;
	unsigned char value;
	
	
    if (copy_from_user(recv,buffer,count))
        return -EFAULT;
	
	int len = strlen(recv);
	printk(KERN_EMERG "proc  str len is  = %d\n",len);
	//if(len > 8)
	//{
	//	printk(KERN_EMERG " len is too long  : 0x210x21\n");
	//	return 0;
	//}
	
	strcpy(recv_2,recv);
	recv_2[4] = '\0';
	
	strcpy(recv_3,&recv[4]);
	recv_3[4] = '\0';
	
	addr  = simple_strtoul(recv_2,NULL,0);
	value = simple_strtoul(recv_3,NULL,0);
	
	printk(KERN_EMERG " addr = %p   value = %p  \n",addr,value);
	
	audio_i2c_write(addr,value);
	return count;
}


#endif

static int __init audio_init(void)
{
    int ret = -1;
    int value;
	
    i2c_add_driver(&audio_driver);

    value = gpio_get_value(AUDIO_ASP_MUTE);
    printk(KERN_EMERG "---------mute init value_mute = %d----------\n",value);
    gpio_direction_output(AUDIO_ASP_MUTE,value);

    // Create proc file system
    BD37033_debug_proc = proc_create("audio_bass", 0666, NULL, &audio_proc_ops);

	// Create proc file system--->addr_value
	addr_value_proc = proc_create("audio_bass_addr_value", 0666, NULL, &audio_debug_proc_ops);
	
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

    misc_deregister(&audio_i2c_miscdev);

  	i2c_del_driver(&audio_driver);

    if( audio_client != NULL )
    	i2c_unregister_device(audio_client);
}

module_init(audio_init);
module_exit(audio_exit);

MODULE_DESCRIPTION("audio init and BASS  Driver");
MODULE_LICENSE("GPL");
