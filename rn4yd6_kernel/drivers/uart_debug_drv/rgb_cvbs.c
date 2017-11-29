#include <linux/debug_uart.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include "proc_debug.h"
#include <linux/proc_fs.h>

#define CH7025_DEVICE_ADDR 0x76
#define CH7025_REG_ADDR_LEN 1
#define CH7025_DEVICE_ID 0x00
#define CH7025_DEVICE_VERSION 0x01

#define CH7025_POWER

#define CH7025_ADJ_REG 0x30
//add ======================================
#define CH7025_DID 		0x55
#define CH7026_DID		0x54

static u8 reg_val_0RGB[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};

static u8 reg_val_1RBG[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0C, 0x10 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};

static u8 reg_val_2GRB[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0C, 0x20 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};

static u8 reg_val_3GBR[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0C, 0x30 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};

static u8 reg_val_4BRG[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0C, 0x40 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};

static u8 reg_val_5BGR[][2] = {
	{ 0x02, 0x01 },
	{ 0x02, 0x03 },
	{ 0x03, 0x00 },
	{ 0x04, 0x39 },
	{ 0x07, 0x1C },
	{ 0x0A, 0x10 },
	{ 0x0C, 0x50 },
	{ 0x0F, 0x23 },
	{ 0x10, 0x20 },
	{ 0x11, 0x18 },
	{ 0x12, 0x40 },
	{ 0x13, 0xCC },
	{ 0x14, 0x01 },
	{ 0x15, 0x11 },
	{ 0x16, 0xE0 },
	{ 0x17, 0x0E },
	{ 0x19, 0x17 },
	{ 0x1A, 0x01 },
	{ 0x1C, 0xB9 },
	{ 0x4D, 0x03 },
	{ 0x4E, 0xC5 },
	{ 0x4F, 0x7F },
	{ 0x50, 0x7B },
	{ 0x51, 0x59 },
	{ 0x52, 0x12 },
	{ 0x53, 0x1B },
	{ 0x55, 0xE5 },
	{ 0x5E, 0x80 },
	{ 0x69, 0x64 },
	{ 0x7D, 0x62 },
	{ 0x04, 0x38 },
	{ 0x06, 0x71 },
};



//end add===================================

static struct i2c_client *ch7025_i2c_client;
static int ch7025_power_pin = 424;
static int ch7025_reset_pin = 360;
//static int ch7025_en_pin = 0;


static void ms_delay(unsigned int msecs)
{
	unsigned long timeout = msecs_to_jiffies(msecs) + 1;

	while (timeout)
		timeout = schedule_timeout_uninterruptible(timeout);
}

static void CH7025_startup(void)
{
	gpio_direction_output(ch7025_power_pin,1);
	ms_delay(200);
	gpio_direction_output(ch7025_reset_pin,0);
	ms_delay(200);
	gpio_direction_output(ch7025_reset_pin, 1);
	ms_delay(200);	
	//gpio_direction_output(ch7025_en_pin, 1);
	//ms_delay(10);	
}

static void blue_rgb_mux(void)
{
	u32 val;
	val = readl(ioremap(0x10E400C8, 0x4));
	val |= (0x2 << 28);
	writel(val,ioremap(0x10E400C8, 0x4));
	
	val = 0;
	val = readl(ioremap(0x10E400B8, 0x4));
	val |= (0x1 << 28);
	writel(val,ioremap(0x10E400B8, 0x4));

#if 0
	writel(0,ioremap(0x1330010C, 0x4));
	writel(0,ioremap(0x1330017C, 0x4));
	writel(0,ioremap(0x13300178, 0x4));
	writel(0,ioremap(0x1330014C, 0x4));
	writel(0,ioremap(0x13300104, 0x4));
	writel(0,ioremap(0x13300134, 0x4));
	writel(0,ioremap(0x13300130, 0x4));
	writel(0,ioremap(0x13300170, 0x4));
#endif

}


static int ch7025_cvbs_write_reg(struct i2c_client *client,u8 *buf,s32 len)
{
	int ret=-1; 
	struct i2c_msg msgs;
	
	msgs.addr  = CH7025_DEVICE_ADDR;
	msgs.flags = !I2C_M_RD;
	msgs.len   = len;
	msgs.buf   = buf;
	ret = i2c_transfer(client->adapter, &msgs, 1);
	
	return  ( ret == 1)?len:ret;  
}

static unsigned int ch7025_cvbs_read_reg(struct i2c_client *client,u8 *buf,s32 len)
{
	struct i2c_msg msgs[2];
    s32 ret=-1;
    s32 retries = 0;

    msgs[0].flags = !I2C_M_RD;
    msgs[0].addr  = client->addr;
    msgs[0].len   = CH7025_REG_ADDR_LEN;
    msgs[0].buf   = &buf[0];
    
    msgs[1].flags = I2C_M_RD;
    msgs[1].addr  = client->addr;
    msgs[1].len   = len - CH7025_REG_ADDR_LEN;
    msgs[1].buf   = &buf[CH7025_REG_ADDR_LEN];
    //msgs[1].scl_rate = 300 * 1000;

    while(retries < 5)
    {
        ret = i2c_transfer(client->adapter, msgs, 2);
        if(ret == 2) return ret;
        retries++;
    }
	
	return -1;
}

//add ======================================
static unsigned char ch7025_i2c_read(u8 reg)
{
	u8	u8Data[3]={0x00,0x00};
	int ret;
	u8Data[0] = reg;
	ret = ch7025_cvbs_read_reg(ch7025_i2c_client,u8Data,2);
	if(ret < 0)
	{
		LOG_ERR("read the reg failed!");
		return 0xff;
	}
	return u8Data[1];
}

static void ch7025_i2c_write(u8 reg,u8 val)
{
	u8	u8Data[3]={0x00,0x00};
	int ret;
	u8Data[0] = reg;
	u8Data[1] = val;
	ret = ch7025_cvbs_write_reg(ch7025_i2c_client,u8Data,2);
	if(ret < 0)
	{
		LOG_ERR("write the reg failed!");
		return ;
	}
	return ;
}

static int CH7025_init(void)
{
	int write_num = 0,i;
	u8 reg_val_map[][2] = {
		{ 0x02, 0x01 },
		{ 0x02, 0x03 },
		{ 0x03, 0x00 },
		{ 0x04, 0x39 },
		{ 0x07, 0x1C },
		{ 0x0A, 0x10 },
		{ 0x0C, 0x50 },
		{ 0x0F, 0x23 },
		{ 0x10, 0x20 },
		{ 0x11, 0x18 },
		{ 0x12, 0x40 },
		{ 0x13, 0xCC },
		{ 0x14, 0x01 },
		{ 0x15, 0x11 },
		{ 0x16, 0xE0 },
		{ 0x17, 0x0E },
		{ 0x19, 0x17 },
		{ 0x1A, 0x01 },
		{ 0x1C, 0xB9 },
		{ 0x4D, 0x03 },
		{ 0x4E, 0xC5 },
		{ 0x4F, 0x7F },
		{ 0x50, 0x7B },
		{ 0x51, 0x59 },
		{ 0x52, 0x12 },
		{ 0x53, 0x1B },
		{ 0x55, 0xE5 },
		{ 0x5E, 0x80 },
		{ 0x69, 0x64 },
		{ 0x7D, 0x62 },
		{ 0x04, 0x38 },
		{ 0x06, 0x71 },
	};
	
	write_num = (sizeof(reg_val_map) / (2 * sizeof(unsigned char)));
	for(i = 0; i < write_num; i++)
		ch7025_i2c_write(reg_val_map[i][0],reg_val_map[i][1]);
	
	LOG_ERR("write %d val OK!",write_num);
	
	return 0;		
}

static int CH7025_init_wait(void)
{
	int value = 0;
	int i;
	int write_num = 0;
	
	u8 reg_val_map[][2] = {
		{ 0x06, 0x70 },
		{ 0x02, 0x02 },
		{ 0x02, 0x03 },
		{ 0x04, 0x20 },
	};
	
	value = ch7025_i2c_read(0x7e);
	while(0x08 != value){//wait
		value = ch7025_i2c_read(0x7e);
		LOG_ERR("read the value = %d",value);
		ms_delay(100);
	}
	
	write_num = (sizeof(reg_val_map) / (2 * sizeof(unsigned char)));
	for(i = 0; i < 4; i++)
		ch7025_i2c_write(reg_val_map[i][0],reg_val_map[i][1]);
	
	LOG_ERR("write %d val OK!",write_num);
	
	return 0;	
	
}

static unsigned int dacs_cntdtt(void)
{
	unsigned int retval = 0;
	
	unsigned char val = 0,dac[3] = {0};
	
	/*Power up CH7025/26B*/
	
	ch7025_i2c_write(0x04,ch7025_i2c_read(0x04) & 0xfe);
	
	/*Set bit 3,4,5 of register 04h to "1"*/
	ch7025_i2c_write(0x04,ch7025_i2c_read(0x04) | 0x38);
	
	/*Set SPPSNS to "1"*/
	ch7025_i2c_write(0x7d,ch7025_i2c_read(0x7d) | 0x01);
	
	/*delay some time (>= 100ms)*/
	ms_delay(200);
	
	/*read 0x7f to see the result*/
	val = ch7025_i2c_read(0x7f);
	
	/*set SPPSNS to 0*/
	ch7025_i2c_write(0x7d,ch7025_i2c_read(0x7d) & 0xfe);
	
	/*set bit 3,4,5 of register 04h to "0"*/
	ch7025_i2c_write(0x04,ch7025_i2c_read(0x04) & 0xc7);
	
	/*Power down CH7025/26B*/
	ch7025_i2c_write(0x04,ch7025_i2c_read(0x04) | 0x01);

	/*See the result*/
	dac[0] = (val & 0x03) >> 0;//Get DAC0 attach information
	dac[1] = (val & 0x0C) >> 2;//Get DAC1 attach information
	dac[2] = (val & 0x30) >> 4;//Get DAC1 attach information
	
	if(dac[0] == 0x01){
		retval |= (0x01 << 0);
	}
	
	if(dac[1] == 0x01){
		retval |= (0x01 << 1);
	}
	
	if(dac[2] == 0x01){
		retval |= (0x01 << 2);
	}
	
	return retval;
	
}

static int ch7025_initialiaze(void)
{
	int val;
	//make sure CH7025/26B in system
	val = ch7025_i2c_read(0x00);
	if((val != CH7025_DID) && (val != CH7026_DID))
		goto dev_err;
	//CH7025/26B was found , go on
	
	if(!dacs_cntdtt())
		goto det_err;
	
	if(CH7025_init())
		goto CH7025_init_failed;

	
	if(CH7025_init_wait())
		goto CH7025_wait_failed;
	
	return 0;
	
CH7025_wait_failed:
	LOG_ERR("CH7025_init wait failed!");
	return -1;
	
CH7025_init_failed:
	LOG_ERR("CH7025_init_failed");
	return -1;
	
det_err:
	LOG_ERR("det failed!");
	return -1;
	
dev_err:
	LOG_ERR("not have the devices");
	return -1;
}

static int ch7025_reset_init(u8 reg_val_map[][2],int write_num)
{
	int i;
	for(i = 0; i < write_num; i++)
		ch7025_i2c_write(reg_val_map[i][0],reg_val_map[i][1]);
	
	LOG_ERR("reset write %d val OK!",write_num);
	return 1;
}

static int ch7025_reset_boot(u8 reg_val_map[][2],int write_num)
{
	if(!dacs_cntdtt())
		goto det_err;

	if(!ch7025_reset_init(reg_val_map,write_num))
		goto CH7025_reset_failed;

	if(CH7025_init_wait())
		goto CH7025_wait_failed;
	
	return 0;

CH7025_wait_failed:
	LOG_ERR("CH7025_init wait failed!");
	return -1;

CH7025_reset_failed:
	LOG_ERR("CH7025_init_failed");
	return -1;

det_err:
	LOG_ERR("det failed!");
	return -1;
}

//end add===================================

//add proc_debug
static int __read_ch7025_reg_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
	u8 reg = 0,val = 0;
	reg = puc_paramlist[0];
	val = ch7025_i2c_read(reg);
	LOG_ERR("read: reg = 0x%x val = 0x%x",reg,val);
	return val;
}

static int __set_ch7025_reg_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
	u8 reg = 0,val = 0;
	reg = puc_paramlist[0];
	val = puc_paramlist[1];
	ch7025_i2c_write(reg,val);
	val = ch7025_i2c_read(reg); 
	LOG_ERR("check the set reg = 0x%x val = 0x%x",reg,val);
	return val;
}

static int __swap_sequences_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
	int write_num = 0;
	switch(puc_paramlist[0]){
	case 0:
		LOG_ERR("select the 0 RGB");
		write_num = (sizeof(reg_val_0RGB) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_0RGB,write_num);
		break;
	case 1:
		LOG_ERR("select the 1 RBG");
		write_num = (sizeof(reg_val_1RBG) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_1RBG,write_num);
		break;
	case 2:
		LOG_ERR("select the 1 GRB");
		write_num = (sizeof(reg_val_2GRB) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_2GRB,write_num);
		break;
	case 3:
		LOG_ERR("select the 3 GBR");
		write_num = (sizeof(reg_val_3GBR) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_3GBR,write_num);
		break;
	case 4:
		LOG_ERR("select the 4 BRG");
		write_num = (sizeof(reg_val_4BRG) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_4BRG,write_num);
		break;
	case 5:
		LOG_ERR("select the 5 BGR");
		write_num = (sizeof(reg_val_5BGR) / (2 * sizeof(unsigned char)));
		ch7025_reset_boot(reg_val_5BGR,write_num);
		break;
	default:
		break;
	}
	return 0;
}

static ssize_t cvbs_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    adayo_proc_cmd_help();
    
    return 0;
}

static ssize_t cvbs_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
{
	unsigned char ac_tmpbuff[128];

    if (count > 128)
    {
        return -EFAULT;
    }

    memset(ac_tmpbuff, 0, sizeof(ac_tmpbuff));
    if (copy_from_user(ac_tmpbuff, buffer, count))
    {
        return -EFAULT;
    }
    printk(KERN_EMERG" you input [%s], [len=%d] \n", ac_tmpbuff, count);

    adayo_proc_cmd_do(ac_tmpbuff, count);

	return count;
}

static const struct file_operations cvbs_proc_ops = {
    .owner = THIS_MODULE,
    .read = cvbs_read_proc,
    .write = cvbs_write_proc,
};

int cvbs_proc_test_init(void)
{
	static struct proc_dir_entry *pst_rvc_proc = NULL;
	 if (pst_rvc_proc == NULL){
        pst_rvc_proc = proc_create("ch7025_set", 0777, NULL, &cvbs_proc_ops); 
		 if (pst_rvc_proc != NULL){
			 adayo_proc_cmd_init("/proc/ch7025_set");
			 /*add cmd*/
			 ADAYO_ADD_CMD("read_ch7026_register",1,"[reg]","00", __read_ch7025_reg_cmd_callback);/*read the ch7025 register*/
			 ADAYO_ADD_CMD("write_ch7026_register",2,"[reg] [val]","46 100",__set_ch7025_reg_cmd_callback);/*set the ch7025 register*/
			 ADAYO_ADD_CMD("SWAP_sequences_corlor",1,"[index 0~5]","0",__swap_sequences_cmd_callback);
		 }
	 }
	 return 0;
}
//end proc_debug

static int ch7025_cvbs_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int ret,val;
	ch7025_i2c_client = client;
	//ch7025_en_pin = of_get_named_gpio(ch7025_i2c_client->dev.of_node, "ch7025_en", 0);
	
	//LOG_ERR("ch7025_en_pin = %d",ch7025_en_pin);
	CH7025_startup();
	
	blue_rgb_mux();
		
	if (!i2c_check_functionality(ch7025_i2c_client->adapter, I2C_FUNC_I2C)) 
    {
        LOG_ERR("I2C check functionality failed.");
        return -ENODEV;
    }
	val = ch7025_i2c_read(CH7025_DEVICE_ID);
	LOG_ERR("read the DEVICE ID is : 0x%x",val);
	
	val = ch7025_i2c_read(CH7025_DEVICE_VERSION);
	LOG_ERR("read the DEVICE VERSION is : 0x%x",val);
	
	ret = ch7025_initialiaze();
	if(ret < 0)
		goto init_failed;

	LOG_ERR("come to the rew version the register---------------------rx----------------------------");
	
	val = ch7025_i2c_read(0x3d);//mono color
	LOG_ERR("read the reg = 0x3d  hue val = 0x%x",val);

	ch7025_i2c_write(0x2e,0x64);
	ch7025_i2c_write(0x2f,0x32);
	ch7025_i2c_write(0x30,0x64);
	ch7025_i2c_write(0x31,0x32);

	cvbs_proc_test_init();//init the proc
	
	return 0;
	
init_failed:	
	LOG_ERR("initialiaze device failed!");
	return -1;
}

static const struct of_device_id ch7025_match_table[] = {
		{.compatible = "rgb_to_cvbs",},
		{ },
};
MODULE_DEVICE_TABLE(of, ch7025_match_table);

static const struct i2c_device_id ch7025_cvbs_id[] = {
    { "cvbs7025", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, ch7025_cvbs_id);

static struct i2c_driver ch7025_cvbs_driver = {
    .probe      = ch7025_cvbs_probe,
   // .remove     = ch7025_cvbs_remove,
    .id_table   = ch7025_cvbs_id,
    .driver = {
        .name     = "cvbs7025",
        .owner    = THIS_MODULE,
        .of_match_table = ch7025_match_table,

    },
};



module_i2c_driver(ch7025_cvbs_driver);
MODULE_DESCRIPTION("CH7025 Series Driver");
MODULE_LICENSE("GPL");