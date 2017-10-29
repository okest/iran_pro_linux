/*
 */

#ifndef __FORYOU_CVBS_H__
#define __FORYOU_CVBS_H__

#include <linux/mm.h>
#include <linux/io.h>
#include <linux/sched.h> 
#include <linux/err.h> 
#include <linux/delay.h>

#define  VERSION    	"V3.66-20170916-Camera"
#define  RVCVERSION     "V3.66-20170916"

#define REARVIEW_TIMEOUT	4
#define FRAME_WIDTH_DEFAULT	720
#define FRAME_HEIGHT_DEFAULT	480

#define FRAME_SIZE		(rv->width * rv->height * 2)
#define DATA_DMA_SIZE		(3 * FRAME_SIZE)
#define TABLE_DMA_SIZE		(1 * SZ_32)

#define DMA_FLAG_NORMAL		(1 << 25)
#define DMA_FLAG_PAUSE		(2 << 25)
#define DMA_FLAG_LOOP		(3 << 25)
#define DMA_FLAG_END		(4 << 25)
#define DMA_SET_LENGTH(x)	(((x) >> 2) - 1)

#define DMA_TABLE_1_LOW		(rv->table_virt_addr + 0)
#define DMA_TABLE_1_HIGH	(rv->table_virt_addr + 4)
#define DMA_TABLE_2_LOW		(rv->table_virt_addr + 8)
#define DMA_TABLE_2_HIGH	(rv->table_virt_addr + 12)
#define DMA_TABLE_3_LOW		(rv->table_virt_addr + 16)
#define DMA_TABLE_3_HIGH	(rv->table_virt_addr + 20)
#define DMA_TABLE_4_LOW		(rv->table_virt_addr + 24)

#define IPC_MSG_RV_MASK		BIT(31)

#define VIDEO_BRIGHTNESS_MAX 128
#define VIDEO_BRIGHTNESS_MIN (-128)
#define VIDEO_CONTRAST_MAX 256
#define VIDEO_CONTRAST_MIN 0
#define VIDEO_HUE_MAX 360
#define VIDEO_HUE_MIN 0
#define VIDEO_SATURATION_MAX 1026
#define VIDEO_SATURATION_MIN 0

#define REARVIEW_LAYER1				SIRFSOC_VDSS_LAYER1
#define REARVIEW_LAYER2				SIRFSOC_VDSS_LAYER2  
#define REARVIEW_AUXILIARY_LAYER 	SIRFSOC_VDSS_LAYER3

#define RV_DRV_NAME		"sirf,rearview"
#define DEVICE_NAME		"rearview"

#define GPIO_RVC_KEY_NUM  376
#define CVBS_ADDR 0x10db3010

#define  DIS_WIDTH   800
#define  DIS_HEIGHT  480

#define DEFINE_RVC_TIME   50
#define DEFINE_RVC_TIMES  7

#define DEFINE_RVC_THREAD_TIMES  10

#define INIT_RVC_FLAG    0
#define KERNEL_RVC_FLAG  1
#define APP_RVC_FLAG     2

#define FLAG_STATUS0     0
#define FLAG_STATUS1    1

#define RVC_TIME   150
#define RVC_POWER_GPIO_NUM  380

#define CAMERA_POWER_GPIO_TYPE       'Z' 
#define CAMERA_POWER_GPIO_OFF        _IOW(CAMERA_POWER_GPIO_TYPE, 0, int)
#define CAMERA_POWER_GPIO_ON         _IOW(CAMERA_POWER_GPIO_TYPE, 1, int)
#define CAMERA_POWER_GPIO_STATUS     _IOW(CAMERA_POWER_GPIO_TYPE, 2, int)

#define CAMERA_CONNECTE_STATUS_OK	     	0 
#define WRITE_STATUS_TURE	    		 	0 
#define WRITE_STATUS_FAIL	    			-1 
#define WRITE_CAMERA_NO_CONNECTE	    	-2 
#define WRITE_STATUS_FAIL2	   			 	-3 
#define WRITE_STATUS_FAIL3	   			 	-4

#endif  /* __FORYOU_CVBS_H__ */
