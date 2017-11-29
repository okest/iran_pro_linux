/*
 * CSR SiRF Atlas7DA Rearview driver
 *
 * Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/kthread.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/dma-mapping.h>
#include <linux/input.h>
#include <linux/of_platform.h>
#include <linux/videodev2.h>
#include <linux/workqueue.h>
#include <video/sirfsoc_vdss.h>
#include "vip_capture.h"
#include <linux/sched.h>
#include <linux/jiffies.h>
#include <linux/cdev.h>  
#include <linux/types.h>	
#include <linux/fs.h>		
#include <linux/miscdevice.h>
#include <linux/uaccess.h>

#include "foryou_cvbs.h"  //add by pt

struct display_info {
	char		display[16];
	struct vdss_rect	src_rect;
	struct vdss_rect	sca_rect;
	struct vdss_rect	dst_rect;
	struct sirfsoc_vdss_panel *panel;
	struct sirfsoc_vdss_layer *l;		/* rearviw data layer */
	struct sirfsoc_vdss_screen *scn;

#ifdef CONFIG_REARVIEW_AUXILIARY
	struct sirfsoc_vdss_layer *aux_l;	/* rearviw auxiliary layer */
	/* auxiliary layer might take over from other layer, need restore */
	struct sirfsoc_vdss_layer_info saved_l_info;
	enum vdss_layer	saved_toplayer;
#endif
};

struct rv_dev {
	struct device	*dev;
	unsigned int	width;
	unsigned int	height;
	int				ipc_irq;
	struct mutex	hw_lock;
	struct vdss_vpp_colorctrl color_ctrl;
	enum vdss_deinterlace_mode di_mode;
	bool		mirror_en;	
	struct work_struct rv_work;
	struct workqueue_struct *rv_wq;
	void		*rv_vip;
	void		*rv_vpp;	
	v4l2_std_id	source_std;
	struct display_info d_info;
	dma_addr_t	data_dma_addr, table_dma_addr;
	void		*data_virt_addr, *table_virt_addr;
	void __iomem	*ipc_int_addr, *ipc_msg_addr;
	
#ifdef CONFIG_REARVIEW_AUXILIARY
	dma_addr_t	aux_dma_addr;
	void		*aux_virt_addr;
	unsigned int	aux_bytesperlength, aux_size;
	atomic_t	aux_value;	//draw aux;0,kernel; 1,app
#endif
		
	bool		running;				
	atomic_t	value;
	atomic_t 	timeout_rvc;		//add by 	0,nomarl,1,timeout
	atomic_t	rvc_status;		//0, not rvc ;   1,rvc
	atomic_t 	app_status;		//0, kernel  ;   1,app	
	atomic_t 	show_status;		//0, kernel  ;   1,app	
	atomic_t    rvc_show_flag;	
};

struct cvbs_Param
{
	u8  bFlag;			//
	u8  *u32Data;		// data: 0xXX	
	u32  u32Len;
};

typedef struct cvbs_Param cvbs_Param_t;

static int   	cvbs_Open (struct inode *inode, struct file *filp);
static int		cvbs_Release(struct inode *inode, struct file *filp);
static ssize_t	cvbs_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos);
static ssize_t	cvbs_Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

struct device *dev_cvbs;	
static struct task_struct *get_rvc_gpio_task = NULL;
//static unsigned int * g_pFrameBuf = NULL;
static DEFINE_MUTEX(rvcmutex);
static DEFINE_MUTEX(cameramutex);

static int  rvc_flag = 0;
static int  g_running  = 0;
static int  gpio_data[1]={1};
static int  camera_gpio_status = 0;

static unsigned long ul_last_jiffies;

int g_rvc_err_flag = 0 ;
unsigned long rvc_is_open =0;
int g_port = 0;
int g_connecte_flag = 0;

extern int g_vip_using;
extern int g_input_status;
extern int g_overflow_status;


static int rv_start(struct rv_dev *rv);
static void rv_stop(struct rv_dev *rv);  
static int rvc_kthread(void *data) ;
static void  kernel_thread_exit(void);

extern ssize_t screen_toplayer_store(struct sirfsoc_vdss_screen *scn,const char *buf, size_t size);
extern bool lcdc_get_layer_status(u32 lcdc_index, enum vdss_layer layer);
extern ssize_t screen_toplayer_show(struct sirfsoc_vdss_screen *scn,char *buf);

static struct task_struct *pcurrent;
static void print_current_task_info(int line)
{
	pcurrent = get_current();	
	printk(KERN_ALERT"\n[%d] process[%s]  pid=%d tgid=%d \n",line,pcurrent->comm,pcurrent->pid,pcurrent->tgid);	
}

static void set_camera_gpio_high(void)
{
	//gpio_direction_output( RVC_POWER_GPIO_NUM, 1);	
}

static void set_camera_gpio_low(void)
{
	//gpio_direction_output( RVC_POWER_GPIO_NUM, 0);	
}

#if 0

static int layer_status0 = 0;
static int layer_status1 = 0;;
static int layer_status2 = 0;;
static int layer_status3 = 0;;

static void get_layer_status( int line )
{
	layer_status0 = lcdc_get_layer_status(0,0);	
	layer_status1 = lcdc_get_layer_status(0,1);	
	layer_status2 = lcdc_get_layer_status(0,2);	
	layer_status3 = lcdc_get_layer_status(0,3);		
	//printk(KERN_ERR"[%s][%d]: layer0=%d layer1=%d layer2=%d layer3=%d\n",__func__,line,layer_status0,layer_status1,layer_status2,layer_status3); 	
}
#endif
static void clear_rvc_running(struct rv_dev *rv,int port,int line )
{
	g_port = port ;			
	atomic_set(&rv->value, 0);		
	rv->running 	= false;
	g_running       =rv->running;
	g_rvc_err_flag = 0;	
	
	printk(KERN_ERR"\n[%s][%d]:  running=%d  rv->value =%d \n",__func__,line,rv->running, atomic_read(&rv->value));	
}

static void rv_start_rvc_running(struct rv_dev *rv,int port,int line)
{	
	g_port          = port;
	atomic_set(&rv->value, 1);	
	rv->running     = 1;
	g_running       =rv->running;
		
	printk(KERN_ERR"\n[%s][%d]:  running=%d  rv->value =%d \n",__func__,line,rv->running, atomic_read(&rv->value));
}
				
static void rv_stop_rvc_running(struct rv_dev *rv,int port,int line)
{
	g_port = port ;		
	rv_stop(rv);		
	rv->running 	= false;
	g_running       =rv->running;
	atomic_set(&rv->value, 0);	
	g_rvc_err_flag = 0;	
	
	printk(KERN_ERR"\n[%s][%d]:  running=%d  rv->value =%d \n",__func__,line,rv->running, atomic_read(&rv->value));		
}

static ssize_t rv_enabled_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&rv->value));
}

static ssize_t rv_enabled_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r;
	bool e;
	struct rv_dev *rv = dev_get_drvdata(dev);

	r = strtobool(buf, &e);
	if (r)
		return r;

	atomic_set(&rv->value, e);

	return size;
}

static void rv_setup_color(struct rv_dev *rv)
{
	struct vdss_vpp_op_params vpp_op_params = {0};
	vpp_op_params.type = VPP_OP_IBV;
	/*vpp color ctrl*/
	vpp_op_params.op.ibv.color_update_only = true;
	vpp_op_params.op.ibv.color_ctrl = rv->color_ctrl;
	/* start vpp */
	sirfsoc_vpp_present(rv->rv_vpp, &vpp_op_params);
}

static ssize_t rv_brightness_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->color_ctrl.brightness);
}

static ssize_t rv_brightness_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	rv->color_ctrl.brightness = clamp(val, (s32)VIDEO_BRIGHTNESS_MIN,(s32)VIDEO_BRIGHTNESS_MAX);
	rv_setup_color(rv);

	return size;
}

static ssize_t rv_contrast_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->color_ctrl.contrast);
}

static ssize_t rv_contrast_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	rv->color_ctrl.contrast = clamp(val, (s32)VIDEO_CONTRAST_MIN,(s32)VIDEO_CONTRAST_MAX);
	rv_setup_color(rv);

	return size;
}

static ssize_t rv_hue_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->color_ctrl.hue);
}

static ssize_t rv_hue_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);

	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;

	rv->color_ctrl.hue = clamp(val, (s32)VIDEO_HUE_MIN,(s32)VIDEO_HUE_MAX);
	rv_setup_color(rv);

	return size;
}

static ssize_t rv_saturation_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->color_ctrl.saturation);
}

static ssize_t rv_saturation_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;

	rv->color_ctrl.saturation = clamp(val, (s32)VIDEO_SATURATION_MIN,(s32)VIDEO_SATURATION_MAX);
	rv_setup_color(rv);

	return size;
}

static ssize_t rv_di_mode_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->di_mode);
}

static ssize_t rv_di_mode_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);

	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	if (val < VDSS_VPP_DI_RESERVED || val > VDSS_VPP_DI_VMRI)
		return -EINVAL;

	rv->di_mode = (enum vdss_deinterlace_mode)val;

	return size;
}

#ifdef CONFIG_REARVIEW_AUXILIARY
static ssize_t aux_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);	
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->aux_value);
}

static ssize_t aux_status_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	if(val == 0)
	{
		memset(rv->aux_virt_addr, 0, rv->aux_size);
		atomic_set(&rv->aux_value,0);
	}
	else
	{
		atomic_set(&rv->aux_value,1);
	}
	return size;
}
#endif

static ssize_t timeout_rvc_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&rv->timeout_rvc));
}

static ssize_t timeout_rvc_status_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r;
	bool e;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = strtobool(buf, &e);
	if (r)
		return r;

	atomic_set(&rv->timeout_rvc, e);

	return size;
}

static ssize_t rvc_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);		
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->rvc_status);
}

static ssize_t rvc_status_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	//struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	if(val == 0)
	{
		//atomic_set(&rv->rvc_status,0);
	}
	else
	{
		//atomic_set(&rv->rvc_status,1);
	}
	return size;
}

static ssize_t app_status_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);		
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->app_status);
}

static ssize_t app_status_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	if(val == 0)
	{
		//atomic_set(&rv->app_status,0);
	}
	else
	{
		atomic_set(&rv->app_status,1);
	}
	return size;
}

static ssize_t rvc_show_flag_show(struct device *dev,struct device_attribute *attr, char *buf)
{
	struct rv_dev *rv = dev_get_drvdata(dev);		
	return snprintf(buf, PAGE_SIZE, "%d\n", rv->rvc_show_flag);
}

static ssize_t rvc_show_flag_store(struct device *dev,struct device_attribute *attr,const char *buf, size_t size)
{
	int r, val;
	struct rv_dev *rv = dev_get_drvdata(dev);
	r = kstrtoint(buf, 0, &val);
	if (r)
		return r;
	if(val == 0)
	{
		//atomic_set(&rv->rvc_show_flag,0);
	}
	else
	{
		atomic_set(&rv->rvc_show_flag,1);
	}
	return size;
}

static DEVICE_ATTR(enabled, S_IRUGO|S_IWUSR,rv_enabled_show, rv_enabled_store);
static DEVICE_ATTR(brightness, S_IRUGO|S_IWUSR,rv_brightness_show, rv_brightness_store);
static DEVICE_ATTR(contrast, S_IRUGO|S_IWUSR,rv_contrast_show, rv_contrast_store);
static DEVICE_ATTR(hue, S_IRUGO|S_IWUSR,rv_hue_show, rv_hue_store);
static DEVICE_ATTR(saturation, S_IRUGO|S_IWUSR,rv_saturation_show, rv_saturation_store);
static DEVICE_ATTR(di_mode, S_IRUGO|S_IWUSR,rv_di_mode_show, rv_di_mode_store);

#ifdef CONFIG_REARVIEW_AUXILIARY
static DEVICE_ATTR(aux_status, S_IRUGO|S_IWUSR,aux_status_show,aux_status_store);
#endif

static DEVICE_ATTR(timeout_rvc, S_IRUGO|S_IWUSR,timeout_rvc_status_show,timeout_rvc_status_store);		
static DEVICE_ATTR(rvc_status, S_IRUGO|S_IWUSR,rvc_status_show,rvc_status_store);
static DEVICE_ATTR(app_status, S_IRUGO|S_IWUSR,app_status_show,app_status_store);
static DEVICE_ATTR(rvc_show_flag, S_IRUGO|S_IWUSR,rvc_show_flag_show,rvc_show_flag_store);
		
static const struct attribute *rv_sysfs_attrs[] = {
	&dev_attr_enabled.attr,
	&dev_attr_brightness.attr,
	&dev_attr_contrast.attr,
	&dev_attr_hue.attr,
	&dev_attr_saturation.attr,
	&dev_attr_di_mode.attr,
#ifdef CONFIG_REARVIEW_AUXILIARY
	&dev_attr_aux_status.attr,	
#endif
	&dev_attr_timeout_rvc.attr,	
	&dev_attr_rvc_status.attr,	
	&dev_attr_app_status.attr,	
	&dev_attr_rvc_show_flag.attr,	
	NULL
};

static inline void rv_init_dma_table(struct rv_dev *rv)
{
	writel(DMA_FLAG_PAUSE | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_1_LOW);
	writel(rv->data_dma_addr, DMA_TABLE_1_HIGH);

	writel(DMA_FLAG_PAUSE | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_2_LOW);
	writel(rv->data_dma_addr + FRAME_SIZE, DMA_TABLE_2_HIGH);

	writel(DMA_FLAG_PAUSE | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_3_LOW);
	writel(rv->data_dma_addr + 2*FRAME_SIZE, DMA_TABLE_3_HIGH);

	writel(DMA_FLAG_PAUSE, DMA_TABLE_4_LOW);
}

static inline void rv_set_dma_table_run(struct rv_dev *rv)
{
	writel(DMA_FLAG_NORMAL | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_1_LOW);
	writel(DMA_FLAG_NORMAL | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_2_LOW);
	writel(DMA_FLAG_NORMAL | DMA_SET_LENGTH(FRAME_SIZE), DMA_TABLE_3_LOW);
	writel(DMA_FLAG_LOOP, DMA_TABLE_4_LOW);
}

static inline void rv_set_dma_table_stop(struct rv_dev *rv)
{
	writel(DMA_FLAG_END, DMA_TABLE_1_LOW);
	writel(DMA_FLAG_END, DMA_TABLE_2_LOW);
	writel(DMA_FLAG_END, DMA_TABLE_3_LOW);
	writel(DMA_FLAG_END, DMA_TABLE_4_LOW);
}

static int rv_setup_dma(struct rv_dev *rv)
{
	int ret = 0, xres, yres;

	ret = dma_set_coherent_mask(rv->dev, DMA_BIT_MASK(32));
	if (ret) {
		dev_err(rv->dev, "set dma coherent mask error\n");
		return ret;
	}

	if (!rv->dev->dma_mask)
		rv->dev->dma_mask = &rv->dev->coherent_dma_mask;
	else
		dma_set_mask(rv->dev, DMA_BIT_MASK(32));

	rv->data_virt_addr = dma_alloc_coherent(rv->dev,DATA_DMA_SIZE + TABLE_DMA_SIZE,&rv->data_dma_addr, GFP_KERNEL);
	
	if (rv->data_virt_addr == NULL) {
		dev_err(rv->dev, "can't alloc dma memory\n");
		return -ENOMEM;
	}

#ifdef CONFIG_REARVIEW_AUXILIARY
	/* alloc panel resolution@ARGB8888 format buffer */
	xres = rv->d_info.panel->timings.xres;
	yres = rv->d_info.panel->timings.yres;
	rv->aux_bytesperlength = xres * 4;
	rv->aux_size = yres * rv->aux_bytesperlength;

	//adayo add
	rv->aux_size = PAGE_ALIGN(rv->aux_size);
	DEFINE_DMA_ATTRS(attrs);
	dma_set_attr(DMA_ATTR_WRITE_COMBINE,&attrs);
	
	rv->aux_virt_addr = dma_alloc_attrs(rv->dev,rv->aux_size,&rv->aux_dma_addr,GFP_KERNEL,&attrs);
	if(rv->aux_virt_addr == NULL) {
		dev_err(rv->dev, "can't alloc aux memory\n");
		return -ENOMEM;
	}
#endif

	rv->table_dma_addr = rv->data_dma_addr + DATA_DMA_SIZE;
	rv->table_virt_addr = rv->data_virt_addr + DATA_DMA_SIZE;
	rv_init_dma_table(rv);

	return ret;
}

int show_pic( int nPicXDest, int nPicYDest, int nWidth, int nHeight,unsigned int *pSrcData ,unsigned int BgColor)
{
	int i, j,cnt=0;
	struct rv_dev *rv = dev_get_drvdata(dev_cvbs);

	//printk(KERN_ALERT"\n[%s][%d][%s] ******* \n", __FUNCTION__, __LINE__,RVCVERSION);	
	
	mutex_lock(&rv->hw_lock);
	
	if( atomic_read(&rv->show_status) ==0 ) {	
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}

	if(rv->aux_virt_addr==NULL)
	{
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}
	if(pSrcData==NULL)
	{
		mutex_unlock(&rv->hw_lock);
		return -1;
	}

    for (i = 0; i < nHeight; i++)
    {
        for (j = 0; j < nWidth; j++,cnt++)
        {
            if ( pSrcData[cnt] == BgColor)
            {
                ;//null
            }
            else
            {			
				*(unsigned int *)( rv->aux_virt_addr   + ( (i + nPicYDest) * DIS_WIDTH + j + nPicXDest ) *4 ) = pSrcData[cnt];
			}			
        }	
    }	
	mutex_unlock(&rv->hw_lock);
	
	return 0; 
}
EXPORT_SYMBOL(show_pic);
	
int  reflush_pic( int nPicXDest, int nPicYDest, int nWidth, int nHeight,unsigned int BgColor)
{
	int i, j;
	struct rv_dev *rv = dev_get_drvdata(dev_cvbs);

	mutex_lock(&rv->hw_lock);
	
	if( atomic_read(&rv->show_status) ==0 ) {	
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}

	if(rv->aux_virt_addr==NULL)
	{
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}

	for (i = 0; i < nHeight; i++)
	{
		for (j = 0; j < nWidth; j++)
		{
			*(unsigned int *)( rv->aux_virt_addr   + ( (i + nPicYDest) * DIS_WIDTH + j + nPicXDest ) *4 ) = BgColor;
		}
	}	
		
	mutex_unlock(&rv->hw_lock);		
	return 0;
}
EXPORT_SYMBOL(reflush_pic);

int clean_ui(void)
{
	struct rv_dev *rv = dev_get_drvdata(dev_cvbs);
	
	mutex_lock(&rv->hw_lock);
	
	if( atomic_read(&rv->show_status) ==0 ) {	
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}

	if(rv->aux_virt_addr==NULL)
	{
		mutex_unlock(&rv->hw_lock);
		return -ENOMEM;
	}

    memset( rv->aux_virt_addr, 0x0, DIS_WIDTH * DIS_HEIGHT * 4);
	
	mutex_unlock(&rv->hw_lock);
	
	return 0;
}
EXPORT_SYMBOL(clean_ui);

static  unsigned char get_rvc_signal(void)
{
	void* reg_base;
	unsigned char  flag;	
	
	reg_base = ioremap(CVBS_ADDR, 4);
	if( reg_base == NULL )
		return -1; 
	
	flag = readl(reg_base);	
	iounmap(reg_base);
	
	return flag;
}

/*******************************
功能：cvbsCheck	
输入参数：void			
返回：成功 iResult   根据状态来判断信号的状态
1 表示有信号
0 表示无信号
-1表示读取失败	
-2表示CAMERA_NO_CONNECTE CAMERA没有连接		
*******************************/
int cvbsCheck(void)
{
	int  dwReadValue = 0;
	int  iResult     = 0;	
	
	dwReadValue= get_rvc_signal();
	if( dwReadValue == -1 )
	{	
		if( g_connecte_flag != CAMERA_CONNECTE_STATUS_OK )
		{
			iResult = WRITE_CAMERA_NO_CONNECTE;
			printk(KERN_ALERT"[%s][%d]  Not CONNECTE CAMERA iResult=%d\n",__func__,__LINE__,iResult);
			return iResult;
		}
		else
		{
			iResult = -1;
			printk(KERN_ALERT"[%s][%d]  error iResult=%d\n",__func__,__LINE__,iResult);
			return iResult;
		}
	}

	if( dwReadValue ==0x03 || dwReadValue ==0x0B || dwReadValue ==0x05 || dwReadValue ==0x0D )
	{
		if( ((dwReadValue >>2) & 0x01) == 0x01 )
		{
			if( g_connecte_flag != CAMERA_CONNECTE_STATUS_OK )
			{
				iResult = WRITE_CAMERA_NO_CONNECTE;
				printk(KERN_DEBUG"[%s][%d] Not CONNECTE CAMERA iResult=%d\n",__func__,__LINE__,iResult);
			}
			else
			{
				iResult = 0;
				printk(KERN_ALERT"[%s][%d] No signal iResult=%d\n",__func__,__LINE__,iResult);
			}
		}
		else if( ((dwReadValue >>2) & 0x01) == 0x00 )
		{
			if( g_rvc_err_flag )
			{
				iResult = 0;
				printk(KERN_ALERT"[%s][%d] No signal iResult=%d\n",__func__,__LINE__,iResult);			
			}
			else
			{
				iResult =1;
			}
		}
	}
	else
	{
		if( g_connecte_flag != CAMERA_CONNECTE_STATUS_OK )
		{
			iResult = WRITE_CAMERA_NO_CONNECTE;
			printk(KERN_DEBUG"[%s][%d] Not CONNECTE CAMERA iResult=%d\n",__func__,__LINE__,iResult);
		}
		else
		{
			iResult = 0;
			printk(KERN_ALERT"[%s][%d] No signal iResult=%d\n",__func__,__LINE__,iResult);
		}
	}
	
    return iResult;
}
EXPORT_SYMBOL(cvbsCheck);
	
#ifdef CONFIG_REARVIEW_AUXILIARY
static int rv_auxiliary_start(struct rv_dev *rv)
{
	struct sirfsoc_vdss_layer *l;
	struct sirfsoc_vdss_layer *active_layers[2]; /* rearview & auxiliary */
	struct sirfsoc_vdss_layer_info info;
	struct sirfsoc_vdss_screen_info sinfo;

	if( atomic_read(&rv->rvc_status) == 0 ) //kernel draw
	{			
		//printk(KERN_ALERT"[%s][%d][%s]kernel draw  g_port=%d running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__, RVCVERSION,g_port,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag ,gpio_data[0]);	
		
		rv->d_info.aux_l = sirfsoc_vdss_get_layer_from_screen(rv->d_info.scn,REARVIEW_AUXILIARY_LAYER, false);
		if (!rv->d_info.aux_l) 
		{
			printk(KERN_ALERT"kernel draw no layer for rearview auxiliary %d \n",__LINE__);					
			return -EBUSY;
		}
	}
	else //app draw
	{				
		//printk(KERN_ALERT"[%s][%d][%s]app draw  g_port=%d running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__, RVCVERSION,g_port,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag ,gpio_data[0]);	
		
		rv->d_info.aux_l = sirfsoc_vdss_get_layer(rv->d_info.scn->lcdc_id,REARVIEW_AUXILIARY_LAYER); 
		if (!rv->d_info.aux_l) 
		{
			printk(KERN_ALERT"app draw no layer for rearview auxiliary %d \n",__LINE__);				
			return -EBUSY;
		}
	} 

	/* set all the layer data to 0, default transparent 100% */
	memset(rv->aux_virt_addr, 0, rv->aux_size);

	/* unlock rearview & auxiliary layers, disable and lock other layers */
	sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, false);
	
	active_layers[0] = rv->d_info.l;
	active_layers[1] = rv->d_info.aux_l;
	
	if( rv->d_info.aux_l) {
		sirfsoc_vdss_set_exclusive_layers(&active_layers[0], 2, true);
	} 
	else 
	{
		sirfsoc_vdss_set_exclusive_layers(&active_layers[0], 1, true);
	}

	if( atomic_read(&rv->rvc_status) == 1  ) //app draw
	{				
		return 0;	
	}
	
	/* get the layer which used for auxiliary original info and backup */
	l = rv->d_info.aux_l;
	l->get_info(l, &info);
	rv->d_info.saved_l_info = info;

	/* apply rearview auxiliary layer setting */
	info.src_surf.base = rv->aux_dma_addr;
	info.disp_mode = VDSS_DISP_NORMAL;

	info.src_rect.left = 0;
	info.src_rect.top = 0;
	info.src_rect.right = rv->d_info.panel->timings.xres - 1;
	info.src_rect.bottom = rv->d_info.panel->timings.yres - 1;

	info.dst_rect.left = 0;
	info.dst_rect.top = 0;
	info.dst_rect.right =  rv->d_info.panel->timings.xres - 1;
	info.dst_rect.bottom = rv->d_info.panel->timings.yres - 1;
	info.src_surf.fmt = VDSS_PIXELFORMAT_8888;

	info.src_surf.width = rv->d_info.panel->timings.xres;
	info.src_surf.height = rv->d_info.panel->timings.yres;

	info.global_alpha	= false;
	info.ckey_on		= false;
	info.dst_ckey_on	= false;
	info.pre_mult_alpha	= false;
	info.source_alpha	= true;

	l->set_info(l, &info);
	l->screen->apply(l->screen);
	l->enable(l);

	/* get the original toplayer info and backup */
	l->screen->get_info(l->screen, &sinfo);
	rv->d_info.saved_toplayer = sinfo.top_layer;

	/* set the auxiliary layer to the new toplayer */
	sinfo.top_layer = l->id;
	l->screen->set_info(l->screen, &sinfo);
	l->screen->apply(l->screen);
	
	/* start to draw distance alarm lines */
	//XX
	
	return 0;
}

static int rv_auxiliary_stop(struct rv_dev *rv)
{
	struct sirfsoc_vdss_layer *l = rv->d_info.aux_l;
	struct sirfsoc_vdss_screen_info sinfo;
	if (!l)
	{
		return 0;
	}

	if( atomic_read(&rv->aux_value) ) {
		return 0;	
	}

	/* remove the distance alarm lines */
	memset(rv->aux_virt_addr, 0, rv->aux_size);

	/* disable auxiliary layer and restore its original setting */
	l->disable(l);
	l->set_info(l, &rv->d_info.saved_l_info);
	l->screen->apply(l->screen);

	/* restore original toplayer setting */
	l->screen->get_info(l->screen, &sinfo);
	sinfo.top_layer = rv->d_info.saved_toplayer;
	l->screen->set_info(l->screen, &sinfo);
	l->screen->apply(l->screen);
	
	return 1;
}
#endif

static void rv_callback_from_vpp(void *arg,enum vdss_vpp id,enum vdss_vpp_op_type type)
{
	struct rv_dev *rv = (struct rv_dev *)arg;
	if (type != VPP_OP_IBV)
		sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, true);
}

static int rv_panel_match(struct sirfsoc_vdss_panel *panel, void *data)
{
	if (strcmp(panel->alias, data) == 0)
		return 1;
	else
		return 0;
}

int rv_get_display_info(struct rv_dev *rv,int layersnum)
{
	if (!sirfsoc_vdss_is_initialized())
		return -ENXIO;

	rv->d_info.panel = sirfsoc_vdss_find_panel(rv->d_info.display,rv_panel_match);
	//rv->d_info.panel = sirfsoc_vdss_get_secondary_device();
	if (!rv->d_info.panel)
	{
		rv->d_info.panel = sirfsoc_vdss_get_primary_device();
		printk(KERN_ERR"[%s][%d]  sirfsoc_vdss_get_primary_device \n",__func__,__LINE__); 
	}
	
	if (!rv->d_info.panel) {
		printk(KERN_ERR"[%s][%d] find panel failed \n",__func__,__LINE__); 
		return	-ENODEV;
	}

	rv->d_info.scn = sirfsoc_vdss_find_screen_from_panel(rv->d_info.panel);
	if (!rv->d_info.scn) {
		printk(KERN_ERR"[%s][%d]  no screen for the panel    EBUSY=%d ",__func__, __LINE__,EBUSY);
		return -ENODEV;
	}

	if(layersnum==1)
	{
		//printk(KERN_ALERT"[%s][%d] ******REARVIEW_LAYER%d****** g_port=%d running=%d  aux_value=%d  rvc_status=%d app_status=%d  rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,layersnum, g_port,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status),rvc_flag ,gpio_data[0]);			
						
		rv->d_info.l = sirfsoc_vdss_get_layer_from_screen(rv->d_info.scn,REARVIEW_LAYER1, true);//1
		if (!rv->d_info.l) 
		{
			printk(KERN_ERR"[%s][%d]  no layer%d for rearview ",__func__, __LINE__,REARVIEW_LAYER2);
			return -EBUSY;
		}
	}
	else
	{
		//pr_info("[%s][%d] ******REARVIEW_LAYER%d****** g_port=%d running=%d  aux_value=%d  rvc_status=%d app_status=%d  rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,layersnum, g_port,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status),rvc_flag ,gpio_data[0]);			
			
		rv->d_info.l = sirfsoc_vdss_get_layer_from_screen(rv->d_info.scn,REARVIEW_LAYER2, true);//2
		if (!rv->d_info.l) 
		{
			return -EBUSY;
		}
	}

	/* full source capture full screen display */
	rv->d_info.src_rect.left	= 0;
	rv->d_info.src_rect.top		= 0;
	rv->d_info.src_rect.right	= rv->width - 1;
	rv->d_info.src_rect.bottom	= rv->height - 1;

	rv->d_info.sca_rect.left	= 0;
	rv->d_info.sca_rect.top		= 0;
	rv->d_info.sca_rect.right	= rv->d_info.panel->timings.xres - 1;
	rv->d_info.sca_rect.bottom	= rv->d_info.panel->timings.yres - 1;

	rv->d_info.dst_rect.left	= 0;
	rv->d_info.dst_rect.top		= 0;
	rv->d_info.dst_rect.right	= rv->d_info.panel->timings.xres - 1;
	rv->d_info.dst_rect.bottom	= rv->d_info.panel->timings.yres - 1;

	return	0;
}

static  void rv_stop(struct rv_dev *rv)
{	
	//printk(KERN_ALERT"[%s][%d]APP RVC  g_port=%d running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__, g_port,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag ,gpio_data[0]);	
	
	if( atomic_read(&rv->aux_value)==0 ) 
	{	
		#ifdef CONFIG_REARVIEW_AUXILIARY
			atomic_set(&rv->show_status,0);
			rv_auxiliary_stop(rv);
		#endif
	}
	
	/* stop lcd layer */
	if (rv->d_info.l->is_enabled(rv->d_info.l))
	{
		rv->d_info.l->disable(rv->d_info.l);
	}
		
	/* stop vpp */
	sirfsoc_vpp_destroy_device(rv->rv_vpp);
	rv->rv_vpp = NULL;
	rv->d_info.l->screen->wait_for_vsync(rv->d_info.l->screen);
	
	/* enable all other layers */
	sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, false);
	
	/* stop vip dma */
	rv_set_dma_table_stop(rv);
	/* stop vip */
	vip_rv_stop(rv->rv_vip);

	clear_bit(1, &rvc_is_open);	
	if( test_bit(1, &rvc_is_open) != 0 )
	{
		printk(KERN_ALERT"[%s][%d] ********test_bit(1, &rvc_is_open) = %d \n",__func__,__LINE__,test_bit(1, &rvc_is_open) );
		clear_bit(1, &rvc_is_open);	
	}	
}

static int rv_start(struct rv_dev *rv)
{
	struct vip_rv_info rv_info = {0};
	struct vdss_vpp_op_params vpp_op_params = {0};
	struct vdss_vpp_create_device_params vpp_dev_params = {0};
	struct sirfsoc_vdss_layer_info info;
	struct vdss_surface src_surf;
	int src_skip, dst_skip;
	int ret ;

	rv_info.cvbs_port = g_port;
	rv->running 	= g_running;
	atomic_set(&rv->show_status,1);
	
	if (test_and_set_bit(1, &rvc_is_open)) {
		printk(KERN_ALERT"[%s][%d] ********** sirfsoc_vout_device is busy %d \n",__func__,__LINE__,rvc_is_open );
		return -EBUSY;
	}
	
	if( g_port == 0 || rv->running == 1 )
	{
		ret = rv_get_display_info(rv,2);
		if (ret != 0) 
		{	
			clear_bit(1, &rvc_is_open);	
			clear_rvc_running(rv,0,__LINE__);
			return ret;
		}	
	}
	
	/* vip setting */
	rv_info.std		= rv->source_std;

	rv_info.rv_vip		= rv->rv_vip;
	rv_info.mirror_en	= rv->mirror_en;
	rv_info.match_addrs[0]	= rv->data_dma_addr + FRAME_SIZE - 128;
	rv_info.match_addrs[1]	= rv->data_dma_addr + 2*FRAME_SIZE - 128;
	rv_info.match_addrs[2]	= rv->data_dma_addr + 3*FRAME_SIZE - 128;
	rv_info.dma_table_addr	= rv->table_dma_addr;
	
	vip_rv_config(&rv_info);

	/* start vip dma */
	rv_set_dma_table_run(rv);

	/* if mirror enabled, line buffer will disorder the pixel data */
	if (rv->mirror_en)
		src_surf.fmt = VDSS_PIXELFORMAT_YVYU;
	else
		src_surf.fmt = VDSS_PIXELFORMAT_YUYV;

	src_surf.width = rv->width;
	src_surf.height = rv->height;
	src_surf.field = VDSS_FIELD_SEQ_TB;
	src_surf.base = 0;

	if (!sirfsoc_vdss_check_size(VDSS_DISP_IBV, &src_surf,
	    &rv->d_info.src_rect, &src_skip, rv->d_info.l,&rv->d_info.sca_rect, &dst_skip)) 
	{
		printk(KERN_ERR"[%s][%d]: vdss check size failed \n",__func__, __LINE__);
		clear_bit(1, &rvc_is_open);
		clear_rvc_running(rv,0,__LINE__);			
		return -1;
	}

	/* lcd layer setting */
	rv->d_info.l->get_info(rv->d_info.l, &info);

	info.src_surf.base = 0;
	info.disp_mode = VDSS_DISP_IBV;
	info.src_rect = rv->d_info.sca_rect;
	info.dst_rect = rv->d_info.dst_rect;
	info.line_skip = dst_skip;
	info.src_surf.fmt = VPP_TO_LCD_PIXELFORMAT;
	info.src_surf.width = rv->d_info.sca_rect.right -rv->d_info.sca_rect.left + 1;
	info.src_surf.height = rv->d_info.sca_rect.bottom -rv->d_info.sca_rect.top + 1;

	rv->d_info.l->set_info(rv->d_info.l, &info);
	rv->d_info.l->screen->apply(rv->d_info.l->screen);

	/* vpp setting */
	vpp_dev_params.func = rv_callback_from_vpp;
	vpp_dev_params.arg = rv;

	/* passthrough mode: VPP0->LCDC0, VPP1->LCDC1, default use VPP0 */
	if (!strcmp(rv->d_info.display, "display1"))
	{
		rv->rv_vpp = sirfsoc_vpp_create_device(SIRFSOC_VDSS_VPP1,&vpp_dev_params);
	}
	else
	{
		rv->rv_vpp = sirfsoc_vpp_create_device(SIRFSOC_VDSS_VPP0,&vpp_dev_params);
	}
	
	vpp_op_params.type = VPP_OP_IBV;
	vpp_op_params.op.ibv.src_id = is_cvd_vip((struct vip_dev *)rv->rv_vip) ?SIRFSOC_VDSS_VIP0_EXT : SIRFSOC_VDSS_VIP1_EXT;
	vpp_op_params.op.ibv.src_size	= 3;
	vpp_op_params.op.ibv.interlace.di_mode = rv->di_mode;
	vpp_op_params.op.ibv.src_rect = rv->d_info.src_rect;
	vpp_op_params.op.ibv.dst_rect = rv->d_info.sca_rect;
	vpp_op_params.op.ibv.src_surf[0] = src_surf;
	vpp_op_params.op.ibv.src_surf[0].base = rv->data_dma_addr;
	vpp_op_params.op.ibv.src_surf[1] = src_surf;
	vpp_op_params.op.ibv.src_surf[1].base = rv->data_dma_addr+ 1*FRAME_SIZE;
	vpp_op_params.op.ibv.src_surf[2] = src_surf;
	vpp_op_params.op.ibv.src_surf[2].base = rv->data_dma_addr+ 2*FRAME_SIZE;
	
	/*vpp color ctrl*/
	vpp_op_params.op.ibv.color_update_only = false;
	vpp_op_params.op.ibv.color_ctrl = rv->color_ctrl;
	
	/* start vpp */
	sirfsoc_vpp_present(rv->rv_vpp, &vpp_op_params);
	
	if(rv_info.cvbs_port==0)
	{	
		sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, true);//				
	}

	/* start vip */
	if( vip_rv_start(rv->rv_vip) == -1 )
	{
		clear_bit(1, &rvc_is_open);		
		rv_stop_rvc_running(rv,0,__LINE__);	
		return -1;
	}

	/* enable lcd rearview layer */
	if (!rv->d_info.l->is_enabled(rv->d_info.l))
	{
		rv->d_info.l->enable(rv->d_info.l);	
	}
	else
	{
		if(rv_info.cvbs_port==0)
		{
			clear_bit(1, &rvc_is_open);	
			rv_stop_rvc_running(rv,0,__LINE__);			
			return -1;
		}
	}

	sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, true);//
		
	#ifdef CONFIG_REARVIEW_AUXILIARY
		ret = rv_auxiliary_start(rv);
		if( ret < 0 )
		{
			printk(KERN_ERR"[%s][%d]:  rv_auxiliary_start err  ret = %d\n",__func__,__LINE__,ret); 	
			clear_bit(1, &rvc_is_open);
			rv_stop_rvc_running(rv,0,__LINE__);	
			return -1;
		}
	#endif		
	return 0 ;
}

static ssize_t cvbs_enabled_write(struct device *dev,const char *buf, size_t size)
{
	struct rv_dev *rv = dev_get_drvdata(dev);
	int ret = 0;

	printk(KERN_ERR"\n[%s][%d][%s][APPRVC][buf[1]:%d  0--off  1--on ][buf[0]:%d  0--RVC ] size= %d  rvc_status=%d camera_gpio_status =%d g_connecte_flag =%d \n",__func__,__LINE__,RVCVERSION,buf[1],buf[0],size,atomic_read(&rv->rvc_status),camera_gpio_status,g_connecte_flag);
											
	if( atomic_read(&rv->rvc_status) == 0 ) 
	{
		printk(KERN_ERR"[%s][%d] kernel RVC Mode!!! gpio_data[0]=%d   rvc_status=%d  app_status=%d \n",__func__,__LINE__,gpio_data[0], atomic_read(&rv->rvc_status),atomic_read(&rv->app_status) ); 
		return WRITE_STATUS_TURE;
	}	
	if(size < 2 )
	{
		printk(KERN_ERR"[%s][%d] app RVC Mode : size err!! gpio_data[0]=%d   rvc_status=%d  app_status=%d \n",__func__,__LINE__,gpio_data[0], atomic_read(&rv->rvc_status),atomic_read(&rv->app_status) ); 
		return WRITE_STATUS_FAIL;
	}

	if( buf[1] == 0 )//0---off
	{
		if( (rv->running == 0) )
		{
			if(buf[0] == 0)//0
			{	
				clear_rvc_running(rv,0,__LINE__);	
				printk(KERN_ALERT"\n[%s][%d][APPRVC ERR OFF] running=%d app_status=%d rvc_gpio_data[0]=%d camera_gpio_status=%d  g_connecte_flag=%d g_rvc_err_flag=%d ret=%d\n",__func__, __LINE__,rv->running, atomic_read(&rv->app_status),gpio_data[0],camera_gpio_status,g_connecte_flag,g_rvc_err_flag,ret);	
				return WRITE_STATUS_TURE;
			}
		}
		else if( (rv->running == 1)   )
		{
			if(buf[0] == 0 )//0
			{							
				rv_stop_rvc_running(rv,0,__LINE__);		
				printk(KERN_ALERT"\n[%s][%d][APPRVC OFF] running=%d app_status=%d rvc_gpio_data[0]=%d camera_gpio_status=%d  g_connecte_flag=%d \n",__func__, __LINE__,rv->running, atomic_read(&rv->app_status),gpio_data[0],camera_gpio_status,g_connecte_flag);	
				return WRITE_STATUS_TURE;
			}
		}	
	}
	else if(buf[1] == 1)
	{
		if( (rv->running == 0) )
		{
			if(buf[0]==0)//0
			{
				g_port          = 0;
			}
			ret =  rv_start(rv);
			if( ret < 0 )
			{	
				clear_rvc_running(rv,0,__LINE__);	
				ret = WRITE_STATUS_FAIL;
				printk(KERN_ALERT"\n[%s][%d][APPRVC ERR OFF] running=%d app_status=%d rvc_gpio_data[0]=%d camera_gpio_status=%d  g_connecte_flag=%d g_rvc_err_flag=%d ret=%d\n",__func__, __LINE__,rv->running, atomic_read(&rv->app_status),gpio_data[0],camera_gpio_status,g_connecte_flag,g_rvc_err_flag,ret);					
				return ret;
			}
			else
			{	
				if(buf[0] == 0)//0
				{
					rv_start_rvc_running(rv,0,__LINE__);		
				}	
			}
		}
		else if( (rv->running == 1) )
		{
			//printk(KERN_ALERT"\n[%s][%d][APPRVC] running=%d app_status=%d rvc_gpio_data[0]=%d camera_gpio_status=%d  g_connecte_flag=%d g_rvc_err_flag=%d ret=%d\n",__func__, __LINE__,rv->running, atomic_read(&rv->app_status),gpio_data[0],camera_gpio_status,g_connecte_flag,g_rvc_err_flag,ret);	
		}	
	}	
	
	g_running 	= rv->running;
						
	printk(KERN_ALERT"[%s][%d][APPRVC ON] running=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  rvc_gpio_data[0]=%d camera_gpio_status=%d  g_connecte_flag=%d ret=%d\n",__func__, __LINE__,rv->running,atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,gpio_data[0],camera_gpio_status,g_connecte_flag,ret);	
											
	return ret ;
}

static ssize_t cvbs_Write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	int s32Ret1 = 0;
	int s32Ret  = 0;
	cvbs_Param_t cvbs_WriteParam;
	char u8Data[8];
	struct rv_dev *rv = dev_get_drvdata(dev_cvbs);
	g_rvc_err_flag =0;

	print_current_task_info(__LINE__);
	
	mutex_lock(&rvcmutex);  
	
	if (!copy_from_user(&cvbs_WriteParam, (cvbs_Param_t __user *)buf, sizeof(cvbs_WriteParam)))
	{		
		u32 u32Len = cvbs_WriteParam.u32Len;
        u32Len = ((u32Len > 64) ? 64 : u32Len);
        copy_from_user(u8Data, (char __user *)cvbs_WriteParam.u32Data, u32Len);

        if (u8Data[0] == 1)
        {
			printk(KERN_ERR"[%s][%d][DVR]  u8Data[1]=%d  u8Data[0]= %d  \n",__FUNCTION__,__LINE__, u8Data[1],u8Data[0]);	
			goto __ret;
		}
		
		s32Ret = cvbs_enabled_write(dev_cvbs, u8Data, u32Len);
__ret:
		if((copy_to_user((char __user *)cvbs_WriteParam.u32Data, u8Data, u32Len) != 0) || \
            (copy_to_user(((cvbs_Param_t __user *)buf), &cvbs_WriteParam, sizeof(cvbs_WriteParam)) != 0))
		{
			s32Ret1 = WRITE_STATUS_FAIL;	
		}
		else
		{			
			s32Ret1 = s32Ret;
		}
	}

	if( strcmp(pcurrent->comm,"app_video") == 0 )
	{
		if( (g_vip_using == 1) || (g_input_status == 1) || (g_overflow_status == 1) )
		{
			s32Ret1 = WRITE_STATUS_FAIL3;		
			printk(KERN_ALERT"\n[%s][%d][APPRVC END ERR] process[%s] tgid=%d s32Ret1=%d g_vip_using=%d g_input_status=%d g_overflow_status=%d \n", __FUNCTION__,__LINE__,pcurrent->comm,current->tgid,s32Ret1,g_vip_using,g_input_status,g_overflow_status);	
			g_vip_using = 0;
			g_input_status = 0;
			g_overflow_status = 0;
			rv_stop_rvc_running(rv,0,__LINE__);		
			mutex_unlock(&rvcmutex);	
			return s32Ret1;
		}
	}

	if( g_rvc_err_flag  )
	{
		s32Ret1 = WRITE_STATUS_FAIL2;		
		printk(KERN_ALERT"\n[%s][%d][APPRVC END ERR] process[%s]  tgid=%d s32Ret1=%d g_rvc_err_flag=%d \n", __FUNCTION__,__LINE__,pcurrent->comm,current->tgid,s32Ret1,g_rvc_err_flag);	

		rv_stop_rvc_running(rv,0,__LINE__);		
		mutex_unlock(&rvcmutex);	
		return s32Ret1;
	}

	printk(KERN_ALERT"\n[%s][%d][APPRVC END] process[%s]  tgid=%d s32Ret1=%d \n", __FUNCTION__, __LINE__,pcurrent->comm, current->tgid,s32Ret1);	
	mutex_unlock(&rvcmutex);	
	return s32Ret1;
}

static ssize_t cvbs_Read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	int err =0;

	err = gpio_direction_input(GPIO_RVC_KEY_NUM);
	if (err < 0) {
		printk(KERN_ALERT"[%s][%d]: gpio_direction_input failed\n",  __FUNCTION__,__LINE__);
		gpio_free(GPIO_RVC_KEY_NUM);
		return -1;
	}
	
	gpio_data[0] = gpio_get_value(GPIO_RVC_KEY_NUM);		
	if( copy_to_user(buf,gpio_data,count) )
	{
		return -EFAULT;
	}
	
	return count;	
}

static long cvbs_ioctl(struct file *file, unsigned int cmd,unsigned long arg)
{
	mutex_lock(&cameramutex); 
	
	switch (cmd) 
	{
		case CAMERA_POWER_GPIO_OFF:	
		{
			set_camera_gpio_low();
			
			camera_gpio_status = gpio_get_value(RVC_POWER_GPIO_NUM);				
			
			//printk(KERN_ALERT"[%s][%d]  ****** camera_gpio_status=%d \n",__func__,__LINE__,camera_gpio_status);
			if( copy_to_user( (int *)arg, &camera_gpio_status, sizeof(int)) )
			{
				mutex_unlock(&cameramutex);	
				return -EFAULT;
			}
		}break;
			
		case CAMERA_POWER_GPIO_ON:
		{
			set_camera_gpio_high();
			camera_gpio_status = gpio_get_value(RVC_POWER_GPIO_NUM);				
			
			//printk(KERN_ALERT"[%s][%d]  ****** data=%d \n",__func__,__LINE__,camera_gpio_status);
			if( copy_to_user( (int *)arg, &camera_gpio_status, sizeof(int)) )
			{
				mutex_unlock(&cameramutex);	
				return -EFAULT;
			}
		}break;
			
		case CAMERA_POWER_GPIO_STATUS:
		{
			camera_gpio_status = gpio_get_value(RVC_POWER_GPIO_NUM);	
			
			//printk(KERN_ALERT"[%s][%d]  ****** camera_gpio_status=%d \n",__func__,__LINE__,camera_gpio_status);
			if( copy_to_user( (int *)arg, &camera_gpio_status, sizeof(int)) )
			{
				mutex_unlock(&cameramutex);	
				return -EFAULT;
			}
		}break;
			
		default:
			printk(KERN_ALERT"[%s][%d]  cvbs_ioctl Failed \n",__func__,__LINE__);
			mutex_unlock(&cameramutex);	
			return -EFAULT;
	}	
	
	mutex_unlock(&cameramutex);	
	
	return 0;	
}

static int cvbs_Open (struct inode *inode, struct file *filp)
{
    return 0;
}
static int cvbs_Release(struct inode *inode, struct file *filp)
{
    return 0;
}

static struct file_operations cvbs_fops = {
	.owner    = THIS_MODULE,
	.open     = cvbs_Open,    
	.read 	  = cvbs_Read,
	.write 	  = cvbs_Write,  
	.unlocked_ioctl    = cvbs_ioctl,
	.release  = cvbs_Release,
};

static struct miscdevice cvbs_miscdev = {
	.minor   = MISC_DYNAMIC_MINOR, 
	.name    = DEVICE_NAME,       
	.fops    = &cvbs_fops,          
};

static int  kernel_thread_init(void)
{
    int err;
    pr_info("Kernel Rvcthread initalizing...\n");
	
	if (get_rvc_gpio_task == NULL) 
	{
		get_rvc_gpio_task = kthread_run(rvc_kthread, "rvcGPIO", "Rvcthread");
		if (IS_ERR(get_rvc_gpio_task)) 
		{
			printk(KERN_ALERT"[%s][%d]: Failed to Create get_rvc_gpio_task thread \n",__func__,__LINE__);	
			err = PTR_ERR(get_rvc_gpio_task);
			get_rvc_gpio_task = NULL;			
			return err;
		}
	}
    return 0;
}	

static void  kernel_thread_exit(void)
{
    if(get_rvc_gpio_task)
	{
		set_camera_gpio_low();
        kthread_stop(get_rvc_gpio_task);
		get_rvc_gpio_task = NULL;  
		printk(KERN_ALERT"[%s] kernel_thread_exit Cancel \n",RVCVERSION);
    }
}

static int  rvc_gpio_get_value(void)
{
    int rvc_gpio_value = 1 ;
	int i=0;
    unsigned long ul_jiffies = msecs_to_jiffies(DEFINE_RVC_TIME);
	
	rvc_gpio_value = gpio_get_value(GPIO_RVC_KEY_NUM);
	if(rvc_gpio_value==0)
	{
		for(i=0;i<DEFINE_RVC_TIMES;i++)
		{
			schedule_timeout_interruptible(ul_jiffies);
			
			rvc_gpio_value = gpio_get_value(GPIO_RVC_KEY_NUM);
			if(rvc_gpio_value !=0)
			{
				break;
			}
		}
	}	
	else if( rvc_gpio_value<0)
	{
		printk(KERN_ALERT"[%s][%d]: gpio_get_value fialed\n",__func__,__LINE__);			
	}
    return rvc_gpio_value;
}

/*******************************
unsigned int get_rvc_flag(void)
输入参数：void			
返回：成功 rvc_flag   根据状态来判断
0 表示未进入内核倒车
1 表示进入内核倒车
2 表示进入app,并退出内核倒车
*******************************/
void get_rvc_flag(int *pi_flag, unsigned long *pul_jiffies)
{
    *pi_flag = rvc_flag;
    *pul_jiffies = ul_last_jiffies;
}
EXPORT_SYMBOL(get_rvc_flag);

static void __set_rvc_flag(int i_svc_flag)
{
	//struct rv_dev *rv = dev_get_drvdata(dev_cvbs);
    if (i_svc_flag != rvc_flag)
    {
        ul_last_jiffies = jiffies;
        rvc_flag = i_svc_flag;
    }
	//atomic_set(&rv->rvc_show_flag,i_svc_flag);
}

static int rv_start_kernel(struct rv_dev *rv)
{
	struct vip_rv_info rv_info = {0};
	struct vdss_vpp_op_params vpp_op_params = {0};
	struct vdss_vpp_create_device_params vpp_dev_params = {0};
	struct sirfsoc_vdss_layer_info info;
	struct vdss_surface src_surf;
	int src_skip, dst_skip;
	int ret ;

	rv_info.cvbs_port = g_port;
	rv->running 	= g_running;
	atomic_set(&rv->show_status,1);

	if (test_and_set_bit(1, &rvc_is_open)) {
		printk(KERN_ALERT"[%s][%d] ********** sirfsoc_vout_device is busy \n",__func__,__LINE__ );
		return -EBUSY;
	}
	
	if( g_port == 0 || rv->running == 1 )
	{
		ret = rv_get_display_info(rv,2);
		if (ret != 0) 
		{	
			clear_bit(1, &rvc_is_open);
			clear_rvc_running(rv,0,__LINE__);
			return ret;
		}	
	}
	
	/* vip setting */
	rv_info.std		= rv->source_std;

	rv_info.rv_vip		= rv->rv_vip;
	rv_info.mirror_en	= rv->mirror_en;
	rv_info.match_addrs[0]	= rv->data_dma_addr + FRAME_SIZE - 128;
	rv_info.match_addrs[1]	= rv->data_dma_addr + 2*FRAME_SIZE - 128;
	rv_info.match_addrs[2]	= rv->data_dma_addr + 3*FRAME_SIZE - 128;
	rv_info.dma_table_addr	= rv->table_dma_addr;
	vip_rv_config(&rv_info);

	/* start vip dma */
	rv_set_dma_table_run(rv);

	/* if mirror enabled, line buffer will disorder the pixel data */
	if (rv->mirror_en)
		src_surf.fmt = VDSS_PIXELFORMAT_YVYU;
	else
		src_surf.fmt = VDSS_PIXELFORMAT_YUYV;

	src_surf.width = rv->width;
	src_surf.height = rv->height;
	src_surf.field = VDSS_FIELD_SEQ_TB;
	src_surf.base = 0;

	if (!sirfsoc_vdss_check_size(VDSS_DISP_IBV, &src_surf,
	    &rv->d_info.src_rect, &src_skip, rv->d_info.l,&rv->d_info.sca_rect, &dst_skip)) 
	{
		printk(KERN_ERR"[%s][%d]: vdss check size failed \n",__func__, __LINE__);
		clear_bit(1, &rvc_is_open);
		clear_rvc_running(rv,0,__LINE__);			
		return -1;
	}

	/* lcd layer setting */
	rv->d_info.l->get_info(rv->d_info.l, &info);

	info.src_surf.base = 0;
	info.disp_mode = VDSS_DISP_IBV;
	info.src_rect = rv->d_info.sca_rect;
	info.dst_rect = rv->d_info.dst_rect;
	info.line_skip = dst_skip;
	info.src_surf.fmt = VPP_TO_LCD_PIXELFORMAT;
	info.src_surf.width = rv->d_info.sca_rect.right -rv->d_info.sca_rect.left + 1;
	info.src_surf.height = rv->d_info.sca_rect.bottom -rv->d_info.sca_rect.top + 1;

	rv->d_info.l->set_info(rv->d_info.l, &info);
	rv->d_info.l->screen->apply(rv->d_info.l->screen);

	/* vpp setting */
	vpp_dev_params.func = rv_callback_from_vpp;
	vpp_dev_params.arg = rv;

	/* passthrough mode: VPP0->LCDC0, VPP1->LCDC1, default use VPP0 */
	if (!strcmp(rv->d_info.display, "display1"))
	{
		rv->rv_vpp = sirfsoc_vpp_create_device(SIRFSOC_VDSS_VPP1,&vpp_dev_params);
	}
	else
	{
		rv->rv_vpp = sirfsoc_vpp_create_device(SIRFSOC_VDSS_VPP0,&vpp_dev_params);
	}
	
	vpp_op_params.type = VPP_OP_IBV;
	vpp_op_params.op.ibv.src_id = is_cvd_vip((struct vip_dev *)rv->rv_vip) ?SIRFSOC_VDSS_VIP0_EXT : SIRFSOC_VDSS_VIP1_EXT;
	vpp_op_params.op.ibv.src_size	= 3;
	vpp_op_params.op.ibv.interlace.di_mode = rv->di_mode;
	vpp_op_params.op.ibv.src_rect = rv->d_info.src_rect;
	vpp_op_params.op.ibv.dst_rect = rv->d_info.sca_rect;
	vpp_op_params.op.ibv.src_surf[0] = src_surf;
	vpp_op_params.op.ibv.src_surf[0].base = rv->data_dma_addr;
	vpp_op_params.op.ibv.src_surf[1] = src_surf;
	vpp_op_params.op.ibv.src_surf[1].base = rv->data_dma_addr+ 1*FRAME_SIZE;
	vpp_op_params.op.ibv.src_surf[2] = src_surf;
	vpp_op_params.op.ibv.src_surf[2].base = rv->data_dma_addr+ 2*FRAME_SIZE;
	
	/*vpp color ctrl*/
	vpp_op_params.op.ibv.color_update_only = false;
	vpp_op_params.op.ibv.color_ctrl = rv->color_ctrl;
	
	/* start vpp */
	sirfsoc_vpp_present(rv->rv_vpp, &vpp_op_params);
	
	if(rv_info.cvbs_port==0)
	{	
		sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, true);//				
	}

	/* start vip */
	if( vip_rv_start(rv->rv_vip) == -1 )
	{	
		clear_bit(1, &rvc_is_open);
		rv_stop_rvc_running(rv,0,__LINE__);	
		return -1;
	}

	/* enable lcd rearview layer */
	if (!rv->d_info.l->is_enabled(rv->d_info.l))
	{
		rv->d_info.l->enable(rv->d_info.l);	
	}
	else
	{
		if(rv_info.cvbs_port==0)
		{
			clear_bit(1, &rvc_is_open);
			rv_stop_rvc_running(rv,0,__LINE__);			
			return -1;
		}
	}
	
	sirfsoc_vdss_set_exclusive_layers(&rv->d_info.l, 1, true);//

	ret = rv_auxiliary_start(rv);
	if( ret < 0 )
	{
		printk(KERN_ERR"[%s][%d]:  rv_auxiliary_start err1  ret = %d\n",__func__,__LINE__,ret); 	
		ret = rv_auxiliary_start(rv);
		if( ret < 0 )
		{
			printk(KERN_ERR"[%s][%d]:  rv_auxiliary_start err2  ret = %d\n",__func__,__LINE__,ret); 	
			clear_bit(1, &rvc_is_open);
			rv_stop_rvc_running(rv,0,__LINE__);	
			return -1;
		}	
	}	
	return 0 ;
}

static  int rv_do_worker(struct rv_dev *rv)
{
	int ret = 0;
	mutex_lock(&rv->hw_lock);
	
	rv->running 	= g_running;
	if (atomic_read(&rv->value) && !rv->running) 
	{	
		g_port =0;
		atomic_set(&rv->value, 1);			
		ret = rv_start_kernel(rv);
		if( ret == -1 )
		{
			g_port =0;
			atomic_set(&rv->value, 1);	
			printk(KERN_ERR"[%s][%d]:  rv_start_kernel err1`  ret = %d\n",__func__,__LINE__,ret); 	
			ret = rv_start_kernel(rv);
			if( ret == -1 )
			{
				printk(KERN_ERR"[%s][%d]:  rv_start_kernel err2  ret = %d\n",__func__,__LINE__,ret); 
				return -1 ;				
			}
		}	
		rv->running     = true;
		g_running       = rv->running;

	}	
	if (!atomic_read(&rv->value) && rv->running) 
	{		
		g_port =0;			
		atomic_set(&rv->value, 0);		
		rv_stop(rv);	
		rv->running 	= false;
		g_running       =rv->running;
	}	
	mutex_unlock(&rv->hw_lock);	
	
	return 0 ;
}

/*******************************
rvc_status
0 表示进入内核未进行倒车模式
1表示并退出内核倒车，进入app模式。
*******************************/
static int rvc_kthread(void *data)  
{ 
	struct rv_dev *rv = dev_get_drvdata(dev_cvbs);
    int rvc_gpio_value = true ;
    unsigned long ul_jiffies = msecs_to_jiffies(DEFINE_RVC_THREAD_TIMES);
	int i = 0;
	int ret = 0;

	__set_rvc_flag(INIT_RVC_FLAG);			
	atomic_set(&rv->rvc_status,false);

	while(!kthread_should_stop())
    {	
		if( ( atomic_read(&rv->rvc_status) == FLAG_STATUS0 ) && ( atomic_read(&rv->app_status) == FLAG_STATUS0 ) ) 
		{
			rvc_gpio_value = rvc_gpio_get_value();
			if(rvc_gpio_value < 0)
			{
				printk(KERN_ALERT"[%s][%d]: gpio_get_value fialed\n",__func__,__LINE__);	
			}

			if(!rvc_gpio_value) //on
			{
				if( atomic_read(&rv->app_status) == FLAG_STATUS0 )
				{
					if( atomic_read(&rv->value) == FLAG_STATUS0 )
					{
						g_port = 0;
						atomic_set(&rv->value, true);
						
						ret = rv_do_worker(rv);
						if( ret == -1 )
						{
							g_port = 0;
							atomic_set(&rv->value, false);
							
							printk(KERN_ERR"[%s][%d]:  rv_start_kernel err2  ret = %d\n",__func__,__LINE__,ret); 
							
							if( atomic_read(&rv->app_status) == FLAG_STATUS1 )
							{
								g_port = 0;
								atomic_set(&rv->value, false);
								atomic_set(&rv->rvc_status,true);
								__set_rvc_flag(APP_RVC_FLAG);

								printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );		
										
								kernel_thread_exit();
								break;
							}
						}

						atomic_set(&rv->rvc_status,false);
						__set_rvc_flag(KERNEL_RVC_FLAG);
						
						printk(KERN_ALERT"[%s][%d][%s]***KERNEL RVC ON*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );	
						
					}
					else
					{
						g_port = 0;
						atomic_set(&rv->value, true);
						atomic_set(&rv->rvc_status,false);
						__set_rvc_flag(KERNEL_RVC_FLAG);
						
						//printk(KERN_ALERT"[%s][%d][%s]***KERNEL RVC ON*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );						
					}				
				}
				else
				{
					if( atomic_read(&rv->value) == FLAG_STATUS0 )
					{
						g_port = 0;
						atomic_set(&rv->value, false);
						rv_do_worker(rv);
						atomic_set(&rv->rvc_status,true);
						__set_rvc_flag(APP_RVC_FLAG);

						printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );		 	
								
						kernel_thread_exit();
						break;
					}
					else
					{
						g_port = 0;
						atomic_set(&rv->value, true);
						atomic_set(&rv->rvc_status,false);
						__set_rvc_flag(KERNEL_RVC_FLAG);
						
						//printk(KERN_ALERT"[%s][%d][%s]***KERNEL RVC ON*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );		
					}
				}
			}
			
			if(rvc_gpio_value)  //off
			{
				if( ( atomic_read(&rv->app_status) == FLAG_STATUS0 ) )
				{
					g_port = 0;
					atomic_set(&rv->value, false);	
					rv_do_worker(rv);
					
					atomic_set(&rv->rvc_status,false);				
					__set_rvc_flag(INIT_RVC_FLAG);
								
					//printk(KERN_ALERT"[%s][%d][%s]***KERNEL RVC OFF*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );	
					
				}
				else
				{
					g_port = 0;
					atomic_set(&rv->value, false);
					rv_do_worker(rv);					
					atomic_set(&rv->rvc_status,true);
					__set_rvc_flag(APP_RVC_FLAG);
					
					printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );	 						
					kernel_thread_exit();
					break;					
				}
			}
		}
		else if( ( atomic_read(&rv->rvc_status) == FLAG_STATUS0 ) && ( atomic_read(&rv->app_status) == FLAG_STATUS1 ) ) 
		{	
			rvc_gpio_value = rvc_gpio_get_value();
			if(rvc_gpio_value < 0)
			{
				printk(KERN_ALERT"[%s][%d]: gpio_get_value fialed\n",__func__,__LINE__);	
			}

			if(rvc_gpio_value)  //off
			{		
				g_port = 0;
				atomic_set(&rv->value, false);					
				rv_do_worker(rv);
				atomic_set(&rv->rvc_status,true);
				__set_rvc_flag(APP_RVC_FLAG);
				
				printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );		  				
				kernel_thread_exit();	
				break;
			}
			else   //on
			{
				if( atomic_read(&rv->value) == FLAG_STATUS0 )
				{
					g_port = 0;
					atomic_set(&rv->value, false);					
					atomic_set(&rv->rvc_status,true);
					__set_rvc_flag(APP_RVC_FLAG);
					
					printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );
					
					kernel_thread_exit();
					break;
				}
				else
				{
					g_port = 0;
					atomic_set(&rv->value, true);
					atomic_set(&rv->rvc_status,false);
					__set_rvc_flag(KERNEL_RVC_FLAG);
						
					i ++ ;
					if( i >= 5 )
					{
						i = 0;
						printk(KERN_ALERT"[%s][%d][%s]***KERNEL RVC ON*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );						
					}
				}
			}		
		}
		else if( ( atomic_read(&rv->rvc_status) == FLAG_STATUS1 ) && ( atomic_read(&rv->app_status) == FLAG_STATUS1 ) ) 
		{	
			g_port = 0;
			atomic_set(&rv->value, false);
			atomic_set(&rv->rvc_status,true);
			__set_rvc_flag(APP_RVC_FLAG);
			
			printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );	
		
			kernel_thread_exit();
			break;
		}
		else if( ( atomic_read(&rv->rvc_status) == FLAG_STATUS1 ) && ( atomic_read(&rv->app_status) == FLAG_STATUS0 ) ) 
		{
			g_port = 0;
			atomic_set(&rv->value, false);
			atomic_set(&rv->rvc_status,true);
			__set_rvc_flag(APP_RVC_FLAG);
			
			printk(KERN_ALERT"[%s][%d][%s]***IntoAPP*** running=%d  aux_value=%d  rvc_status=%d app_status=%d value =%d rvc_flag=%d  gpio_data[0]=%d \n",__func__, __LINE__,RVCVERSION,rv->running, atomic_read(&rv->aux_value),atomic_read(&rv->rvc_status),atomic_read(&rv->app_status), atomic_read(&rv->value),rvc_flag,rvc_gpio_value );	 
		
			kernel_thread_exit();
			break;
		}		
		schedule_timeout_interruptible(ul_jiffies);		
    }
    return true;
} 

static int rv_probe(struct platform_device *pdev)
{
	struct device_node *rv_vip_np, *node = pdev->dev.of_node;
	struct platform_device *rv_vip_pdev;
	struct device *dev = &pdev->dev;
	struct rv_dev *rv = NULL;
	const char *std_name, *display_name;
	unsigned int mirror = 0;
	struct resource	*res;
	v4l2_std_id std;
	int ret = 0;
	
	dev_cvbs = &pdev->dev; //add by pt

	rv = devm_kzalloc(dev, sizeof(*rv), GFP_KERNEL);
	if (!rv) {
		ret = -ENOMEM;
		goto exit;
	}

	/*
	* The CAN stack is running on M3, M3 will filter the CAN FRAMES
	* and detect CAN messages of "Rearview start/stop".
	* So the Rearview driver needn't  VIRTIO_CAN and parse CAN frame.
	* M3 exports 2 addresses to Rearview driver, one for read-to-clear
	* dedicated interrupt flag and one for decoded message value.
	*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	rv->ipc_int_addr = devm_ioremap_resource(dev, res);
	if (!rv->ipc_int_addr) {
		dev_err(dev, "fail to ioremap ipc int regs\n");
		ret = -ENOMEM;
		goto exit;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	rv->ipc_msg_addr = devm_ioremap_resource(dev, res);
	if (!rv->ipc_msg_addr) {
		dev_err(dev, "fail to ioremap msg int regs\n");
		ret = -ENOMEM;
		goto exit;
	}

	/*
	* Because Rearview CAN can be triggered in uboot and Linux scenes,
	* and for time delay sensitive reason, they don't use the RPMSG to send
	* trigger message, adopt dedicated irq instead.
	*/
	rv->ipc_irq = platform_get_irq(pdev, 0);
	if (!rv->ipc_irq) {
		dev_err(dev, "fail to get ipc irq\n");
		ret = -EINVAL;
		goto exit;
	}

	atomic_set(&rv->aux_value,0);	
	atomic_set(&rv->rvc_status,0);
	atomic_set(&rv->app_status,0);
	atomic_set(&rv->show_status,0);
	atomic_set(&rv->rvc_show_flag,0);

	of_property_read_u32(node, "mirror", &mirror);
	of_property_read_string(node, "source-std", &std_name);
	of_property_read_string(node, "display-panel", &display_name);

	rv_vip_np = of_find_compatible_node(NULL, NULL, "sirf,rv-vip");
	if (!rv_vip_np) {
		dev_err(dev, "can't find rearview vip\n");
		ret = -ENODEV;
		goto exit;
	}

	rv_vip_pdev = of_find_device_by_node(rv_vip_np);
	if (!rv_vip_pdev) {
		dev_err(dev, "can't find rearview vip pdev\n");
		ret = -ENODEV;
		of_node_put(rv_vip_np);
		goto exit;
	}

	device_lock(&rv_vip_pdev->dev);
	rv->rv_vip = platform_get_drvdata(rv_vip_pdev);
	device_unlock(&rv_vip_pdev->dev);

	of_node_put(rv_vip_np);

	rv->dev		= dev;
	rv->mirror_en	= mirror ? true : false;
	rv->running	= false;
	mutex_init(&rv->hw_lock);
	strncpy(rv->d_info.display, display_name, sizeof(rv->d_info.display));

	/* set NTSC format as default */
	rv->source_std = V4L2_STD_NTSC;

	if (strcmp(std_name, "NTSC") == 0)
		rv->source_std = V4L2_STD_NTSC;
	
	if (strcmp(std_name, "PAL") == 0)
		rv->source_std = V4L2_STD_PAL;
	
	if (strcmp(std_name, "AUTO") == 0) {
		std = vip_rv_querystd(rv->rv_vip);
		
		if (std & (V4L2_STD_NTSC | V4L2_STD_NTSC_443))
			rv->source_std = V4L2_STD_NTSC;
		
		if (std & (V4L2_STD_PAL | V4L2_STD_PAL_Nc))
			rv->source_std = V4L2_STD_PAL;
		
		if (std == V4L2_STD_UNKNOWN)
			pr_info("AUTO detection fails, please check source\n");
	}

	if (rv->source_std == V4L2_STD_NTSC) {
		rv->width	= 720;
		rv->height	= 480;
	} else if (rv->source_std == V4L2_STD_PAL) {
		rv->width	= 720;
		rv->height	= 576;
	} else {
		rv->width	= FRAME_WIDTH_DEFAULT;
		rv->height	= FRAME_HEIGHT_DEFAULT;
	}

	ret = rv_get_display_info(rv,2);
	if (ret) {
		dev_err(dev, "get display info error\n");
		goto exit;
	}

	ret = rv_setup_dma(rv);
	if (ret) {
		dev_err(dev, "set memory error\n");
		goto exit;
	}

	ret= kernel_thread_init();
	if (ret) {
		dev_err(dev, "kernel_thread_init error\n");
		goto exit;
	}
	
	platform_set_drvdata(pdev, rv);

	/* set default colors */
	rv->color_ctrl.brightness = 0;
	rv->color_ctrl.contrast = 128;
	rv->color_ctrl.hue = 0;
	rv->color_ctrl.saturation = 128;

#ifdef CONFIG_VERTICAL_MEDIAN
	rv->di_mode = VDSS_VPP_DI_VMRI;
#elif defined(CONFIG_MEAVE)
	rv->di_mode = VDSS_VPP_DI_WEAVE;
#elif defined(CONFIG_CONFIG_3TAP_MEDIAN)
	rv->di_mode = VDSS_VPP_3MEDIAN;
#else
	rv->di_mode = VDSS_VPP_DI_VMRI;
#endif
	
	
	ret = misc_register(&cvbs_miscdev);
	if (ret) {
		dev_err(dev, "failed to misc_register\n");
		goto misc_exit;
	}

	ret = sysfs_create_files(&dev->kobj, rv_sysfs_attrs);
	if (ret) {
		dev_err(dev, "failed to create sysfs files\n");
		goto exit;
	}

	pr_info("rearview start on %s\n", display_name);

	return 0;
	
misc_exit:
	misc_deregister(&cvbs_miscdev);

exit:
	return ret;
}

static int rv_remove(struct platform_device *pdev)
{
	struct rv_dev *rv = platform_get_drvdata(pdev);

	mutex_lock(&rv->hw_lock);		
	misc_deregister(&cvbs_miscdev);	//add by pt	

	kernel_thread_exit();
	
	if (rv->running) 
	{
		rv_stop_rvc_running(rv,0,__LINE__);
	}

	dma_free_coherent(rv->dev, DATA_DMA_SIZE + TABLE_DMA_SIZE,rv->data_virt_addr, rv->data_dma_addr);
	
#ifdef CONFIG_REARVIEW_AUXILIARY
	dma_free_coherent(rv->dev, rv->aux_size,rv->aux_virt_addr, rv->aux_dma_addr);
#endif

	sysfs_remove_files(&rv->dev->kobj, rv_sysfs_attrs);

	mutex_unlock(&rv->hw_lock);
	
	pr_info("rv_remove done!!!\n");

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int rv_pm_suspend(struct device *dev)
{
	//struct rv_dev *rv = dev_get_drvdata(dev);

	/* we need to wait ongoing worker tasks for finishing*/
	return 0;
}
#endif

static SIMPLE_DEV_PM_OPS(rv_pm_ops, rv_pm_suspend, NULL);

static const struct of_device_id rv_match_tbl[] = {
	{ .compatible = "sirf,rearview", },
	{},
};

static struct platform_driver rv_driver = {
	.driver	= {
		.name = RV_DRV_NAME,
		.owner    = THIS_MODULE,
		.pm = &rv_pm_ops,
		.of_match_table = rv_match_tbl,
	},
	.probe = rv_probe,
	.remove = rv_remove,
};

static int __init sirfsoc_rv_init(void)
{
	return platform_driver_register(&rv_driver);
}

static void __exit sirfsoc_rv_exit(void)
{
	platform_driver_unregister(&rv_driver);
}

subsys_initcall_sync(sirfsoc_rv_init);
module_exit(sirfsoc_rv_exit);

MODULE_DESCRIPTION("SIRFSoC Atlas7 Rearview driver");
MODULE_LICENSE("GPL v2");
