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
#include <linux/workqueue.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/kern_levels.h>


#include <linux/proc_fs.h>

#include "rpmsg_rvc.h"
#include "rvc_gui_show.h"
#include "adayo_debug.h"

extern void get_rvc_flag(int *pi_flag, unsigned long *pul_jiffies);
extern int cvbsCheck(void);

#ifdef RPMSG_RVC_DEBUG
    extern int rpmsg_rvc_proc_test_init(void);
#endif

static DEFINE_MUTEX(rvc_lock);

typedef struct
{
    unsigned char uc_time_onoff;                    /* 定时器开关 */
    unsigned char uc_time_tick;                     /* 定时器多少个tick发送一次msg，每次200ms, 根据kthread的调度时间 */
    unsigned char uc_time_tick_cp;                  /* 计算kthread的轮训次数 */
    unsigned char uc_time_status;                   /* 记录定时器状态 */
    unsigned char uc_reserve;
}RPMSG_RVC_TIMER_S;

typedef struct
{
    int i_binit;                                    /* 是否初始化 */
    struct rpmsg_channel *rpdev;                    /* rpmsg 通道 */
    int i_breversing;                               /* 是否进入倒车, 0:未进入倒车，1:进入倒车，2:进入app接管倒车 */
    struct sk_buff_head queue;
    struct workqueue_struct *keventd_wq;
    struct work_struct work;
    
    int i_bkthread_rcv_run;                         /* kthread_rcv_status暂停监控，用于调试 */
    struct task_struct *kthread_rcv_status;
    int i_bdebugopen;
    int i_brpmsgdbgopen;                            /* 开启rpmsg的msg打印 */
    unsigned int ui_rpmsgdbgfltcmd;                 /* 过滤rmpsg的cmd，只有相应匹配的cmd才打印,65535 表示全匹配 */
   
    int i_radarexit;                                /* radar bit : 0、后左；1、后左中；2、后右中；3、后右；4、前左；5、前左中；6、前右中；7、前右  */
    RPMSG_RVC_TIMER_S st_timer;                     /* 定时器处理 */

    int i_camera_status;                            /* 后视摄像头是否有信号，0:无, 1:有 */
    int i_camera_ct;                                /* 当检测到无信号时，次数超过三次就上报信号 */

    RADAR_PIC_STATUC_S st_last_radarpic_status;     /* 上一次雷达状态信息，解决频繁绘画问题 */

}RPMSG_RVC_INFO_S;

typedef enum
{
    RPMSG_RVC_MSG_TYPE_MCU_RADAR_E = 0,             /* MCU传递过来的雷达信号 */ 
    RPMSG_RVC_MSG_TYPE_REVERS_IN_E,                 /* 进入倒车 */
    RPMSG_RVC_MSG_TYPE_REVERS_OUT_E,                /* 退出倒车 */
    RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E,              /* 所有雷达通讯异常，需要定时发送信息进行显示 */
    RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E,           /* 后置摄像头异常 */
    RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E,            /* 后置摄像头正常 */
    RPMSG_RVC_MSG_TYPE_MCU_BUFF_E,
}RPMSG_RVC_MSG_TYPE_E;

typedef enum
{
    RPMSG_RVC_PCK_MPEG_E = 0x00,
    RPMSG_RVC_PCK_DR_E = 0x01,
    RPMSG_RVC_PCK_AC_E = 0x02,
    RPMSG_RVC_PCK_CD_E = 0x03,
    RPMSG_RVC_PCK_RADAR_E = 0x04,
    RPMSG_RVC_PCK_BUFF_E,
}RPMSG_RVC_PCK_TYPE_E;

typedef struct
{
    unsigned char uc_post_radar;                            /* 后置雷达，0:关闭，1:开启*/
    unsigned char uc_lead_radar;                            /* 前置雷达，0:关闭，1:开启*/
    unsigned char uc_hou_zuo;                               /* 后左离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_hou_zuo_zhong;                         /* 后左中离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_hou_you_zhong;                         /* 后右中离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_hou_you;                               /* 后右离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_qian_zuo;                              /* 前左离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_qian_zuo_zhong;                        /* 前左中离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_qian_you_zhong;                        /* 前右中离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_qian_you;                              /* 前右离障碍物的距离 0x00:不显示；0x01:最近*/
    unsigned char uc_radar_fault;                           /* 雷达故障 */
    unsigned char uc_radar_signal;                          /* 雷达信号 0雷达无信号（无有效雷达信号），1雷达有信号 */
    unsigned char uc_radar_node;                            /* 雷达节点 0雷达正常，1雷达节点丢失*/
}RPMSG_RVC_RADAR_S;

typedef struct
{
    unsigned char uc_bsendrpmsg;                            /* for debug */ 
    unsigned char uc_bcheckerr; 
    unsigned char uc_pkgnum;
    RPMSG_RVC_PCK_TYPE_E    e_type;
    union 
    {
        RPMSG_RVC_RADAR_S st_radar;
    }u_pkg_info;
}RPMSG_RVC_PKG_INFO_S;

static RPMSG_RVC_INFO_S st_rpmsg_rvc = 
{
    .i_binit = 0,
    .rpdev = NULL,
    .i_breversing = 0,
};

/*******************************
int pwm_beeper_radar( int value )
输入参数：int i_hz
value ：传0 表示不响 pwm,  >0以上的值用于频率 		
返回：0成功，其他值不成功
*******************************/

//#define MPC_A7_BEERPER
#ifdef MPC_A7_BEERPER
#include <linux/of.h>
#include <linux/of_platform.h>

extern int pwm_beeper_radar( int value );
static int __rpmsg_rvc_beerper(int i_hz)
{
    struct device_node *pst_pn;
    int i_ret;
    
    pst_pn = of_find_node_by_name(NULL, "pwm-beeper");
    if (pst_pn == NULL)
    {
        printk(KERN_ERR"%s.%d no fine beer \n", __FUNCTION__, __LINE__);
        pst_pn = of_find_node_by_name(NULL, "buzzer");
        if (pst_pn == NULL)
        {
            printk(KERN_ERR"%s.%d no fine buzzer \n", __FUNCTION__, __LINE__);
        }
        else
        {
            printk(KERN_ERR"%s.%d fine buzzer pst_pn=%#x \n", __FUNCTION__, __LINE__, (int)(pst_pn));
        }
    }
    if (pst_pn != NULL)
    {
        of_node_put(pst_pn);
    }

    
	pst_pn = of_find_compatible_node(NULL, NULL, "pwm-beeper");
    if (pst_pn != NULL)
    {
        struct platform_device *pst_beeper_dev;
        
    	pst_beeper_dev = of_find_device_by_node(pst_pn);
        if (pst_beeper_dev != NULL)
        {
            put_device(&pst_beeper_dev->dev);
        }
    }
    if (pst_pn != NULL)
    {
        of_node_put(pst_pn);
    }

    i_ret = pwm_beeper_radar(i_hz);

    pr_info("%s.%d set beer hz=%d, ret=%d \n", __FUNCTION__, __LINE__, i_hz, i_ret);
    return 0;
}
#else
static int __rpmsg_rvc_beerper(int i_hz)
{
    return 0;
}
#endif

static int __rpmsg_rvc_radar_msg_filter(unsigned char *puc_buff, unsigned int ui_len, unsigned int ui_fltcmd)
{
    unsigned int ui_tmpcmd;
    unsigned char uc_funcid = 0xff;
    
    if ((puc_buff == NULL) || (ui_len < 8) || (ui_len > 512))
    {
        return -1;
    }


    if ((puc_buff[0] != 0x6A) || (puc_buff[1] != 0xA6))
    {
        return -1;
    }

    
    ui_tmpcmd = (puc_buff[7] << 8);
    ui_tmpcmd |= puc_buff[8];

    if (ui_fltcmd == 0xffff)
    {
        return 0;
    }
 
    if (ui_tmpcmd != ui_fltcmd)
    {
        return -1;
    }

    uc_funcid = puc_buff[12];
    if (uc_funcid != 1)
    {
        return -1;
    }

    return 0;
}

static int __rpmsg_rvc_radar_msg_analysis(unsigned char *puc_buff, unsigned int ui_len, RPMSG_RVC_PKG_INFO_S *pst_pkginfo)
{
    unsigned char uc_tmpchar;
    int i_mm;

    if (st_rpmsg_rvc.i_bdebugopen > 1)
    {
        ADAYO_DEBUG_BUFF(puc_buff, ui_len);
    }
                
    pst_pkginfo->uc_pkgnum = puc_buff[9];
    /* 新协议去除type */
    pst_pkginfo->e_type = RPMSG_RVC_PCK_RADAR_E;

    /* 数据校验 in */
    uc_tmpchar = 0;
    for (i_mm = 2; i_mm < (ui_len - 1); i_mm++)
    {
        uc_tmpchar += puc_buff[i_mm];
    }
    uc_tmpchar = ~(uc_tmpchar) + 1;
    /* 数据校验 out */
    
    if (uc_tmpchar != puc_buff[ui_len - 1])
    {
        printk(KERN_ERR"%s.%d recv data checksum error [0x%x, 0x%x] !! \n", \
            __FUNCTION__, __LINE__, uc_tmpchar, puc_buff[ui_len - 1]);
        pst_pkginfo->uc_bcheckerr = 1;
        goto _RET;
    }

    pst_pkginfo->u_pkg_info.st_radar.uc_post_radar = puc_buff[14] & 0x1;
    pst_pkginfo->u_pkg_info.st_radar.uc_lead_radar = ((puc_buff[14] >> 1) & 0x1);
    pst_pkginfo->u_pkg_info.st_radar.uc_hou_zuo = (puc_buff[15]);
    pst_pkginfo->u_pkg_info.st_radar.uc_hou_zuo_zhong = (puc_buff[16]);
    pst_pkginfo->u_pkg_info.st_radar.uc_hou_you_zhong = (puc_buff[17]);
    pst_pkginfo->u_pkg_info.st_radar.uc_hou_you = (puc_buff[18]);
    pst_pkginfo->u_pkg_info.st_radar.uc_qian_zuo = (puc_buff[19]);
    pst_pkginfo->u_pkg_info.st_radar.uc_qian_zuo_zhong = (puc_buff[20]);
    pst_pkginfo->u_pkg_info.st_radar.uc_qian_you_zhong = (puc_buff[21]);
    pst_pkginfo->u_pkg_info.st_radar.uc_qian_you = (puc_buff[22]);
    pst_pkginfo->u_pkg_info.st_radar.uc_radar_fault = (puc_buff[23]);
    pst_pkginfo->u_pkg_info.st_radar.uc_radar_signal = (puc_buff[24]);
    pst_pkginfo->u_pkg_info.st_radar.uc_radar_node = (puc_buff[25]);

    if (pst_pkginfo->u_pkg_info.st_radar.uc_post_radar != 0)
    {
        st_rpmsg_rvc.i_radarexit = 0xf;
    }
    else
    {
        st_rpmsg_rvc.i_radarexit = 0;
    }

    if (pst_pkginfo->u_pkg_info.st_radar.uc_lead_radar != 0)
    {
        st_rpmsg_rvc.i_radarexit |= 0xf0;
    }
    else
    {
        st_rpmsg_rvc.i_radarexit &= (~0xf0);
    }

_RET:
    if (st_rpmsg_rvc.i_bdebugopen > 0)
    {
        printk(KERN_EMERG"pkginfo change  [%d, %d] [uc_pkgnum=%d] [type:%d]\n" ,\
            pst_pkginfo->uc_bsendrpmsg, pst_pkginfo->uc_bcheckerr, pst_pkginfo->uc_pkgnum, pst_pkginfo->e_type);
        printk(KERN_EMERG"    uc_post_radar=%d left->right : [%d][%d][%d][%d]  \n", \
            pst_pkginfo->u_pkg_info.st_radar.uc_post_radar, pst_pkginfo->u_pkg_info.st_radar.uc_hou_zuo, \
            pst_pkginfo->u_pkg_info.st_radar.uc_hou_zuo_zhong, \
            pst_pkginfo->u_pkg_info.st_radar.uc_hou_you_zhong, pst_pkginfo->u_pkg_info.st_radar.uc_hou_you);
        printk(KERN_EMERG"    uc_lead_radar=%d left->right [%d][%d][%d][%d]  \n", \
            pst_pkginfo->u_pkg_info.st_radar.uc_lead_radar, pst_pkginfo->u_pkg_info.st_radar.uc_qian_zuo, \
            pst_pkginfo->u_pkg_info.st_radar.uc_qian_zuo_zhong, \
            pst_pkginfo->u_pkg_info.st_radar.uc_qian_you_zhong, pst_pkginfo->u_pkg_info.st_radar.uc_qian_you);
        printk(KERN_EMERG"    radar status fault=%#x, signal=%#x, node=%#x, radarexit=%#x \n\n", \
            pst_pkginfo->u_pkg_info.st_radar.uc_radar_fault, \
            pst_pkginfo->u_pkg_info.st_radar.uc_radar_signal, pst_pkginfo->u_pkg_info.st_radar.uc_radar_node,\
            st_rpmsg_rvc.i_radarexit);
    }

    return 0;
}

static int __rpmsg_rvc_radar_msg_resp(struct rpmsg_channel *rpdev, RPMSG_RVC_PKG_INFO_S *pst_pkginfo)
{
    #if 0
    unsigned char ac_buff[16], uc_tmpchar;
    int i_ret;
    int i_mm;

    if (pst_pkginfo->uc_bsendrpmsg == 0)
    {
        return 0;
    }

    memset(ac_buff, 0, sizeof(ac_buff));

    ac_buff[0] = 0x6a;
    ac_buff[1] = 0xa6;
    if (pst_pkginfo->uc_bcheckerr == 0)
    {
        ac_buff[2] = 2;
    }
    else
    {
        ac_buff[2] = 3;
    }

    /* 长度 */
    ac_buff[3] = 0;
    ac_buff[4] = 5;

    ac_buff[5] = (unsigned char)0x9;
    ac_buff[6] = (unsigned char)0x00;

    ac_buff[7] = pst_pkginfo->uc_pkgnum;
    ac_buff[8] = pst_pkginfo->e_type;
    
    if (pst_pkginfo->uc_bcheckerr == 0)
    {
        ac_buff[9] = 0;                 /* 校验成功 */
    }
    else
    {
        ac_buff[9] = 0x1;               /* 校验失败 */
    }
    
	// 计算校验值  in
    for (i_mm = 2; i_mm < 10; i_mm++)
    {
        uc_tmpchar += ac_buff[i_mm];
    }
    ac_buff[10] = ~(uc_tmpchar) + 1;
	// 计算校验值  out

    //__ZGR_DEBUG_BUFF(ac_buff, 11);
    
    /* 数据发送 */
    
    mutex_lock_interruptible(&rvc_lock); 
    i_ret = rpmsg_send(rpdev, (void *)ac_buff, 11);
    if (i_ret) 
    {
        pr_err("zgr rpmsg_send failed: %d\n", i_ret);
    }
    mutex_unlock(&rvc_lock);
    
    return i_ret;
    #else
    return 0;
    #endif
}

static int __rpmsg_rvc_radar_dis_to_pic(unsigned char uc_radar_data)
{
	int i_radara_pic = 0xff;

	switch(uc_radar_data)
	{
		case 0x01:
			i_radara_pic = 1;
			break;
		case 0x02:
		case 0x03:
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			i_radara_pic = 2;
			break;
		case 0x08:
		case 0x09:
		case 0x0A:
		case 0x0B:
		case 0x0C:
			i_radara_pic = 3;
			break;
		default:
			i_radara_pic = 0;
			break;
	}
	return i_radara_pic;
}

static int __rpmsg_rvc_radar_msg_do(struct rpmsg_channel *rpdev, RPMSG_RVC_PKG_INFO_S *pst_pkginfo)
{
    RPMSG_RVC_RADAR_S *pst_radar;
    RADAR_PIC_STATUC_S ast_picstatuc;
    
    __rpmsg_rvc_radar_msg_resp(rpdev, pst_pkginfo);

    /* 显示雷达信息 */
    if (pst_pkginfo->e_type != RPMSG_RVC_PCK_RADAR_E)
    {
        return 0;
    }
    pst_radar = &(pst_pkginfo->u_pkg_info.st_radar);

    /*  */
    memset(&ast_picstatuc, 0, sizeof(ast_picstatuc));
    if ((pst_radar->uc_radar_signal == 0) || (pst_radar->uc_radar_node != 0))
    {
        /* 雷达系统通讯故障，2Hz闪烁 */
        ast_picstatuc.ui_car_status = 1;
    }
    else
    {
        ast_picstatuc.ui_car_status = 0;
        /* ------------------------------ 后雷达 in  ------------------------------  */
        if (pst_radar->uc_post_radar == 0)
        {
            ast_picstatuc.uc_radara[0] = 0;
            ast_picstatuc.uc_radara[1] = 0;
            ast_picstatuc.uc_radara[2] = 0;
        }
        else
        {
            ast_picstatuc.uc_radara[0] = __rpmsg_rvc_radar_dis_to_pic(pst_radar->uc_hou_zuo);
            ast_picstatuc.uc_radara[1] = __rpmsg_rvc_radar_dis_to_pic(pst_radar->uc_hou_zuo_zhong);
            ast_picstatuc.uc_radara[2] = __rpmsg_rvc_radar_dis_to_pic(pst_radar->uc_hou_you);
        }
        /* ------------------------------ 后雷达 out ------------------------------  */

        /* ------------------------------ 前雷达 in  ------------------------------  */
        if (pst_radar->uc_lead_radar == 0)
        {
            ast_picstatuc.uc_radara[3] = 0;
            ast_picstatuc.uc_radara[4] = 0;
        }
        else
        {
            ast_picstatuc.uc_radara[3] = __rpmsg_rvc_radar_dis_to_pic(pst_radar->uc_qian_zuo);
            ast_picstatuc.uc_radara[4] = __rpmsg_rvc_radar_dis_to_pic(pst_radar->uc_qian_you);
        }
        /* ------------------------------ 前雷达 out ------------------------------  */


        /* 是否有故障 */
        if (pst_radar->uc_radar_fault != 0)
        {
            /* 其中一/两个通道有故障 */
            if (pst_radar->uc_post_radar != 0)
            {
                ast_picstatuc.uc_radara[0] = (((pst_radar->uc_radar_fault & 0x1) == 0) ? ast_picstatuc.uc_radara[0] : 4);
                ast_picstatuc.uc_radara[1] = (((pst_radar->uc_radar_fault & 0x2) == 0) ? ast_picstatuc.uc_radara[1] : 4);
                ast_picstatuc.uc_radara[2] = (((pst_radar->uc_radar_fault & 0x8) == 0) ? ast_picstatuc.uc_radara[2] : 4);
            }

            if (pst_radar->uc_lead_radar != 0)
            {
                ast_picstatuc.uc_radara[3] = (((pst_radar->uc_radar_fault & 0x10) == 0) ? ast_picstatuc.uc_radara[3] : 4);
                ast_picstatuc.uc_radara[4] = (((pst_radar->uc_radar_fault & 0x80) == 0) ? ast_picstatuc.uc_radara[4] : 4);
            }
        }
    }

    if (ast_picstatuc.ui_car_status == 1)
    {
        st_rpmsg_rvc.st_timer.uc_time_onoff = 1;
        /* 所有 radar 异常 进入闪烁 */
	
        if (st_rpmsg_rvc.st_timer.uc_time_status == 0)
        {
            rvc_gui_radar_show_err(1, st_rpmsg_rvc.i_radarexit);
            st_rpmsg_rvc.st_timer.uc_time_status = 1;
        }
        else
        {
            rvc_gui_radar_show_err(0, st_rpmsg_rvc.i_radarexit);
            st_rpmsg_rvc.st_timer.uc_time_status = 0;
        }
        
        return 0;
    }
    else
    {
        st_rpmsg_rvc.st_timer.uc_time_onoff = 0;
        
        if (memcmp(&(st_rpmsg_rvc.st_last_radarpic_status), &ast_picstatuc, sizeof(RADAR_PIC_STATUC_S)) != 0)
        {
            memcpy(&(st_rpmsg_rvc.st_last_radarpic_status), &ast_picstatuc, sizeof(RADAR_PIC_STATUC_S));
            rvc_gui_radar_show_normal(&ast_picstatuc);
        }
        
    }

    
    return 0;
}

/* 此接口非对称定义，因为recv只能是 __rpmsg_rvc_work_handle 调用 */
static int __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_E e_msgtype, unsigned char *puc_param, unsigned int ui_len)
{
    struct sk_buff *skb; 
    unsigned char *skbdata;
    RPMSG_RVC_MSG_TYPE_E *pe_type;
    
    skb = alloc_skb(ui_len + sizeof(e_msgtype), GFP_KERNEL); 
    if (!skb) 
    { 
        printk(KERN_ERR"%s.%d alloc_skb error !! \n", __FUNCTION__, __LINE__);
        return -1;
    } 
	
    skbdata = skb_put(skb, ui_len + sizeof(e_msgtype));

    pe_type = (RPMSG_RVC_MSG_TYPE_E *)skbdata;
    *pe_type = e_msgtype;
    memcpy(&(skbdata[sizeof(e_msgtype)]), puc_param, ui_len); 

    skb_queue_tail(&(st_rpmsg_rvc.queue), skb); 

    return 0;
}

static void __rpmsg_rvc_work_handle(struct work_struct *work)
{
    struct sk_buff *p_skb;
    RPMSG_RVC_PKG_INFO_S st_pkginfo;
    RPMSG_RVC_MSG_TYPE_E e_msgtype = RPMSG_RVC_MSG_TYPE_MCU_BUFF_E;
       
    while (1)
    {
        mutex_lock_interruptible(&rvc_lock); 
        if (skb_queue_empty(&(st_rpmsg_rvc.queue))) 
    	{
            mutex_unlock(&rvc_lock);
    		break;
    	}
        p_skb = skb_dequeue(&(st_rpmsg_rvc.queue));
        if (!p_skb) 
        { 
            printk(KERN_ERR"%s.%d skb_dequeue is error \n", __FUNCTION__, __LINE__);
            mutex_unlock(&rvc_lock);
            break;
    	} 

        e_msgtype = *((RPMSG_RVC_MSG_TYPE_E *)(p_skb->data));

        if (e_msgtype == RPMSG_RVC_MSG_TYPE_MCU_RADAR_E)
        {
            memset(&st_pkginfo, 0, sizeof(st_pkginfo));
            st_pkginfo.uc_bsendrpmsg = 1;
            __rpmsg_rvc_radar_msg_analysis(&(p_skb->data[sizeof(RPMSG_RVC_MSG_TYPE_E)]), (p_skb->len - sizeof(RPMSG_RVC_MSG_TYPE_E)), &st_pkginfo);
        }
        
    	kfree_skb(p_skb);

        mutex_unlock(&rvc_lock); 

        if (st_rpmsg_rvc.i_bdebugopen > 0)
        {
            printk(KERN_EMERG" e_msgtype=%d time_onoff=%d\n", e_msgtype, st_rpmsg_rvc.st_timer.uc_time_onoff);
        }
        
        switch (e_msgtype)
        {
            case RPMSG_RVC_MSG_TYPE_MCU_RADAR_E:
                __rpmsg_rvc_radar_msg_do(st_rpmsg_rvc.rpdev, &st_pkginfo);                
                break;
            case RPMSG_RVC_MSG_TYPE_REVERS_IN_E:
                rvc_gui_line_show(0);
                //解决快速倒车进入退出，在进入时，图层被修改
                st_rpmsg_rvc.i_camera_status = -1;
                st_rpmsg_rvc.st_last_radarpic_status.ui_car_status = 0xffffffff;
                st_rpmsg_rvc.st_last_radarpic_status.ui_car_distance = 0xffffffff;
                break;
            case RPMSG_RVC_MSG_TYPE_REVERS_OUT_E:
                rvc_gui_line_show(1);
                break;
            case RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E:
                if (st_rpmsg_rvc.st_timer.uc_time_onoff == 1) //再次判断的原因是防止kthread没有刹住车
                {
                    memset(&st_pkginfo, 0, sizeof(st_pkginfo));
                    
                    st_pkginfo.uc_bsendrpmsg = 0;
                    st_pkginfo.uc_bcheckerr = 0;
                    st_pkginfo.uc_pkgnum = 1;
                    st_pkginfo.e_type = RPMSG_RVC_PCK_RADAR_E;
                    st_pkginfo.u_pkg_info.st_radar.uc_radar_signal = 0;
                    __rpmsg_rvc_radar_msg_do(st_rpmsg_rvc.rpdev, &st_pkginfo);                
                }
                break;
            case RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E:
                rvc_gui_camera_show(0);
                break;
            case RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E:
                rvc_gui_camera_show(1);
                break;
            default:
                printk(KERN_ERR "%s.%d recv err type=%#x \n", __FUNCTION__, __LINE__, e_msgtype);
                break;
        }
    }

    return;
}

static int __rpmsg_rvc_status_kthread(void *data)
{
    unsigned long ul_jiffies = msecs_to_jiffies(200);
    int i_new_rcv_stats = 0;
    int i_ret, i_bworkup, i_tmpcamera;
    unsigned long ul_new_status_jiffies = 0, ul_last_status_jiffies = 0;

    unsigned char auc_buff[28];    

    pr_info("%s.%d you will come in rvc status, make time=2017-7-13 14:10. \n", __FUNCTION__, __LINE__);
    
    for (;;) 
    {
        if (kthread_should_stop())
        {
            break;
        }

        if (st_rpmsg_rvc.i_bkthread_rcv_run == 0)
        {
            schedule_timeout_interruptible(ul_jiffies);
            continue;
        }
        
        mutex_lock_interruptible(&rvc_lock);

        get_rvc_flag(&i_new_rcv_stats, &ul_new_status_jiffies);
        #if 0
        printk(KERN_EMERG"%s.%d i_new_rcv_stats=%d old=%d [%d.%d.%d]\n", \
            __FUNCTION__, __LINE__, i_new_rcv_stats, st_rpmsg_rvc.i_breversing,\
            st_rpmsg_rvc.st_timer.uc_time_onoff, st_rpmsg_rvc.st_timer.uc_time_tick_cp,\
            st_rpmsg_rvc.st_timer.uc_time_tick);
        #endif

        /* 倒车交由应用处理 --------------in-------------- */
        if (i_new_rcv_stats == 2)
        {
            st_rpmsg_rvc.i_breversing = i_new_rcv_stats;
            memset(auc_buff, 0, sizeof(auc_buff));
            __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_REVERS_OUT_E, auc_buff, sizeof(auc_buff));
            
            mutex_unlock(&rvc_lock);
            queue_work(st_rpmsg_rvc.keventd_wq, &(st_rpmsg_rvc.work));  

            pr_info("%s.%d you now goto app to  show rvc \n", __FUNCTION__, __LINE__);
            
            break;
        }
        /* 倒车交由应用处理 --------------out-------------- */

        i_bworkup = 0;

        /* 定时器处理  --------------in-------------- */
        if (st_rpmsg_rvc.st_timer.uc_time_onoff == 1)
        {
            st_rpmsg_rvc.st_timer.uc_time_tick_cp++;
            if (st_rpmsg_rvc.st_timer.uc_time_tick <= st_rpmsg_rvc.st_timer.uc_time_tick_cp)
            {
                st_rpmsg_rvc.st_timer.uc_time_tick_cp = 0;
                memset(auc_buff, 0, sizeof(auc_buff));
                i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, auc_buff, sizeof(auc_buff));
                i_bworkup = 1;
            }
        }
        /* 定时器处理  --------------out-------------- */

        /* 倒车状态处理  --------------in-------------- */
        if ((i_new_rcv_stats != st_rpmsg_rvc.i_breversing) || \
            (ul_last_status_jiffies != ul_new_status_jiffies))
        {
            pr_info("%s.%d you status[new:%d,old:%d] , tick[new:%lu,old:%lu] !! \n", \
                __FUNCTION__, __LINE__, i_new_rcv_stats, st_rpmsg_rvc.i_breversing, ul_new_status_jiffies, ul_last_status_jiffies);
            
            memset(auc_buff, 0, sizeof(auc_buff));
            if (i_new_rcv_stats == 1)
            {
                i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_REVERS_IN_E, auc_buff, sizeof(auc_buff));
            }
            else
            {
                i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_REVERS_OUT_E, auc_buff, sizeof(auc_buff));
            }
            if (i_ret == 0)
            {
                ul_last_status_jiffies = ul_new_status_jiffies;
                st_rpmsg_rvc.i_breversing = i_new_rcv_stats;
                i_bworkup = 1;
            }
        }
        
        /* 倒车状态处理  --------------out-------------- */


        /* 后置摄像头 ----------------- in ----------------------------- */
        i_tmpcamera = cvbsCheck();
        if (i_tmpcamera != 1) /* 摄像头异常 */
        {
            if (st_rpmsg_rvc.i_camera_status != i_tmpcamera)
            {
                st_rpmsg_rvc.i_camera_ct++;
                if (st_rpmsg_rvc.i_camera_ct >= 3)
                {
                    pr_info("%s.%d camera err !! \n", __FUNCTION__, __LINE__);
                    st_rpmsg_rvc.i_camera_status = i_tmpcamera;
                    st_rpmsg_rvc.i_camera_ct = 0;
                    i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E, auc_buff, sizeof(auc_buff));
                    i_bworkup = 1;
                }
            }
        }
        else
        {
            if (st_rpmsg_rvc.i_camera_status != i_tmpcamera)
            {
                pr_info("%s.%d camera ok !! \n", __FUNCTION__, __LINE__);
                st_rpmsg_rvc.i_camera_status = i_tmpcamera;
                st_rpmsg_rvc.i_camera_ct = 0;
                i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E, auc_buff, sizeof(auc_buff));
                i_bworkup = 1;
            }
        }
        /* 后置摄像头 ----------------- out ----------------------------- */        
        mutex_unlock(&rvc_lock);
        
        if (i_bworkup == 1)
        {
            queue_work(st_rpmsg_rvc.keventd_wq, &(st_rpmsg_rvc.work));  
        }
        schedule_timeout_interruptible(ul_jiffies);
    }

    pr_info("%s.%d you will goto leave fast rvc \n", __FUNCTION__, __LINE__);

    for (;;) 
    {
        if (kthread_should_stop())
        {
            break;
        }

        ul_jiffies = msleep_interruptible(1000 * 100);
    }    
    pr_info("%s.%d you will exit rvc status ul_jiffies=%lu \n", __FUNCTION__, __LINE__, ul_jiffies);

    //do_exit(0);

    return 0;
}

static int __rpmsg_rvc_driver_cb(struct rpmsg_channel *rpdev, char *data, int len)
{
    int i_ret;
    
    i_ret = __rpmsg_rvc_radar_msg_filter((unsigned char *)data, (unsigned int)len, 0x8900);
    if (i_ret != 0)
    {
        return -1;
    }

    mutex_lock_interruptible(&rvc_lock); 
    if (st_rpmsg_rvc.i_breversing != 1)
    {
        mutex_unlock(&rvc_lock); 
        return -1;
    }
    i_ret = __rpmsg_rvc_msg_send(RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, (unsigned char *)data, (unsigned int)len);

    mutex_unlock(&rvc_lock); 

    return i_ret;
}

/* cdev probe will call this func */
int rpmsg_rcv_init(struct rpmsg_channel *rpdev)
{
    if (rpdev == NULL)
    {
        printk(KERN_ERR"%s.%d init error , param is null \n", __FUNCTION__, __LINE__);
        return -1;
    }

    __rpmsg_rvc_beerper(0);
    
    mutex_lock_interruptible(&rvc_lock); 

    if (st_rpmsg_rvc.i_binit != 0)
    {
        mutex_unlock(&rvc_lock);
        return 0;
    }

    st_rpmsg_rvc.rpdev = rpdev;

    skb_queue_head_init(&(st_rpmsg_rvc.queue));

    /* 创建 rpmsg 工作队列 */
    st_rpmsg_rvc.keventd_wq = create_singlethread_workqueue("krvc_wq");
    INIT_WORK(&(st_rpmsg_rvc.work), __rpmsg_rvc_work_handle);

    /* 初始化 rvc 状态 */
    st_rpmsg_rvc.i_breversing = 0;

    /* 创建 rvc statuc 监控线程 */
    st_rpmsg_rvc.i_bkthread_rcv_run = 1;
    st_rpmsg_rvc.kthread_rcv_status = kthread_run(__rpmsg_rvc_status_kthread, NULL, "krvc_status");

    st_rpmsg_rvc.i_bdebugopen = 0;
    st_rpmsg_rvc.i_brpmsgdbgopen = 0;
    st_rpmsg_rvc.ui_rpmsgdbgfltcmd = 65535;

    st_rpmsg_rvc.i_radarexit = 0;

    st_rpmsg_rvc.st_timer.uc_time_onoff = 0;
    st_rpmsg_rvc.st_timer.uc_time_tick = 3;
    st_rpmsg_rvc.st_timer.uc_time_tick_cp = 0;
    st_rpmsg_rvc.st_timer.uc_time_status = 0;

    st_rpmsg_rvc.i_camera_status = -1;
    st_rpmsg_rvc.i_camera_ct = 0;

    st_rpmsg_rvc.st_last_radarpic_status.ui_car_status = 0xffffffff;
    st_rpmsg_rvc.st_last_radarpic_status.ui_car_distance = 0xffffffff;
    
    st_rpmsg_rvc.i_binit = 1;
    
    mutex_unlock(&rvc_lock);

    pr_info("%s.%d init success \n", __FUNCTION__, __LINE__);

#ifdef RPMSG_RVC_DEBUG
    rpmsg_rvc_proc_test_init();
#endif
    
    return 0;
}

/* */
int rpmsg_rcv_deinit(void)
{
    struct sk_buff *p_skb;
    
    mutex_lock_interruptible(&rvc_lock); 
    
    if (st_rpmsg_rvc.i_binit == 0)
    {
        printk(KERN_ERR"%s.%d has deinit \n", __FUNCTION__, __LINE__);
        mutex_unlock(&rvc_lock);
        return 0;
    }
    
    mutex_unlock(&rvc_lock);
    
    if (st_rpmsg_rvc.kthread_rcv_status != NULL)
    {
        st_rpmsg_rvc.i_bkthread_rcv_run = 0;
        kthread_stop(st_rpmsg_rvc.kthread_rcv_status);
        st_rpmsg_rvc.kthread_rcv_status = NULL;
    }

    flush_workqueue(st_rpmsg_rvc.keventd_wq);
    cancel_work_sync(&(st_rpmsg_rvc.work));
    destroy_workqueue(st_rpmsg_rvc.keventd_wq);

    mutex_lock_interruptible(&rvc_lock); 

    while (1)
    {
    	if (skb_queue_empty(&(st_rpmsg_rvc.queue))) 
    	{
    		break;
    	}
        p_skb = skb_dequeue(&(st_rpmsg_rvc.queue));
        if (!p_skb) 
        { 
            printk(KERN_ERR"%s.%d skb_dequeue is error \n", __FUNCTION__, __LINE__);
            break;
    	} 
    	kfree_skb(p_skb);
    }

    st_rpmsg_rvc.i_binit = 0;

    mutex_unlock(&rvc_lock);

    pr_info("%s.%d deinit success \n", __FUNCTION__, __LINE__);

    return 0;
}


int rpmsg_rcv_msgmng(struct rpmsg_channel *rpdev, void *data, int len)
{
    int i_tmpcp = 0, i_tmplen;
    char *pc_tmpdata;
    int i_other_len = 2 + 1 + 2 + 2 + 1;// 2BYTE SYNC, 1BYTE ACK, 2BYTE FN, 2BYTE LEN, 1BYTE CHECKSUM
    int i_ret = 0, i_msgnum = 0, i_radarmsgnum = 0;

    pc_tmpdata = (void *)data;

    if ((len < i_other_len) || (len > 32768))
    {
        return -1;
    }


    if (st_rpmsg_rvc.i_brpmsgdbgopen != 0)
    {
        while (i_tmpcp < (len - 1))
        {
            i_tmplen = ((pc_tmpdata[i_tmpcp + 5] << 8) | pc_tmpdata[i_tmpcp + 6]);

            if (__rpmsg_rvc_radar_msg_filter((unsigned char *)(&pc_tmpdata[i_tmpcp]), (unsigned int)(i_tmplen + i_other_len), st_rpmsg_rvc.ui_rpmsgdbgfltcmd) == 0)
            {
                ADAYO_DEBUG_BUFF((unsigned char *)(&pc_tmpdata[i_tmpcp]), (unsigned int)(i_tmplen + i_other_len));
            }

            i_tmpcp = i_tmpcp + i_tmplen + i_other_len;
        }
    }

    i_tmpcp = 0;
    i_msgnum = 0;
    i_radarmsgnum = 0;
    if (st_rpmsg_rvc.i_breversing == 1)
    {
        while (i_tmpcp < (len - 1))
        {
            i_msgnum++;
            i_tmplen = ((pc_tmpdata[i_tmpcp + 5] << 8) | pc_tmpdata[i_tmpcp + 6]);
            i_ret = __rpmsg_rvc_driver_cb(rpdev, (&pc_tmpdata[i_tmpcp]), (i_tmplen + i_other_len));            
            i_tmpcp = i_tmpcp + i_tmplen + i_other_len;
            if (i_ret == 0)
            {
                i_radarmsgnum++;
            }
        }

        if (i_radarmsgnum > 0)
        {
            /* 唤醒工作队列处理问题 */
            //printk("<0>" "zgr %s.%d zgr you will queue_work \n", __FUNCTION__, __LINE__);
            queue_work(st_rpmsg_rvc.keventd_wq, &(st_rpmsg_rvc.work));  
        }

        if ((i_radarmsgnum < i_msgnum) || (i_radarmsgnum == 0))
        {
            pr_info("%s.%d get one more msg from mcu[%d.%d] \n", __FUNCTION__, __LINE__, i_radarmsgnum, i_msgnum);
            return -1;
        }
        return 0;
    }
    return -1;
}

#ifdef RPMSG_RVC_DEBUG

typedef struct
{
    unsigned char uc_back_on_off;
    unsigned char uc_back_left;
    unsigned char uc_back_mid;
    unsigned char uc_back_right;
    unsigned char uc_radar_err;
    unsigned char uc_radar_singed;
    unsigned char uc_radar_node;
}DUMP_RADATA_FOR_CMD_S;

typedef struct
{
    RPMSG_RVC_MSG_TYPE_E e_msg_type;
    DUMP_RADATA_FOR_CMD_S test_cmd;
}DUMP_RADATA_CMD_S;

unsigned char *__dump_radar_just_for_cmd(DUMP_RADATA_FOR_CMD_S *pst_radara_info)
{    
    static unsigned char auc_buff[22];    
    unsigned char uc_tmpchar;    
    int i_mm;        

    memset(auc_buff, 0, sizeof(auc_buff)); 

    /* head */    
    auc_buff[0] = 0x6a;    
    auc_buff[1] = 0xa6;
    
    /* ack */    
    auc_buff[2] = 0x80;
    
    /* len */    
    auc_buff[3] = 0;    
    auc_buff[4] = 16;    
    
    /* cmd */    
    auc_buff[5] = 0x89;    
    auc_buff[6] = 0x00;  
    
    /* 帧序列 */    
    auc_buff[7] = 0;   
    
    /* 包类型 */    
    auc_buff[8] = 0x4; 
    
    /* back/front rafar back(1/0)、front(2/0) */    
    auc_buff[9] = pst_radara_info->uc_back_on_off; 
    
    /* back left  rafar 0~0xff*/    
    auc_buff[10] = pst_radara_info->uc_back_left;  
    
    /* back left mid rafar 0~0xff*/    
    auc_buff[11] = pst_radara_info->uc_back_mid; 
    
    /* back right mid rafar */    
    auc_buff[12] = 0; 
    
    /* back right rafar 0~0xff*/    
    auc_buff[13] = pst_radara_info->uc_back_right;  
    
    /* front left  rafar 0~0xff*/    
    auc_buff[14] = 1; 
    
    /* front left mid rafar 0~0xff*/    
    auc_buff[15] = 1; 
    
    /* front right mid rafar */    
    auc_buff[16] = 0;  
    
    /* front right rafar 0~0xff*/    
    auc_buff[17] = 0; 
    
    /* back/front rafar error(bit 0 ~ 7) */    
    auc_buff[18] = pst_radara_info->uc_radar_err;  
    
    /* back/front rafar singed 0/1 */    
    auc_buff[19] = pst_radara_info->uc_radar_singed;          
    /* back/front rafar node 0/1 */    
    auc_buff[20] = pst_radara_info->uc_radar_node;   

    // 计算校验值  in    
    for (i_mm = 2; i_mm < 20; i_mm++)    
    {        
        uc_tmpchar += auc_buff[i_mm];    
    }    
    auc_buff[21] = ~(uc_tmpchar) + 1;    
    // 计算校验值  out    
    
    return auc_buff;
}

static int __krvc_test_kthread(void *data)
{
    unsigned long ul_jiffies;
    DUMP_RADATA_CMD_S st_cmd_info[]=
    {
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 0, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 1, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 2, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 8, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 3, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 0xe, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 0xf, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 6, 0xc, 0, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 6, 0xc, 1, 0, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 0xc, 1, 6, 0, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_REVERS_IN_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_REVERS_OUT_E, {1, 0, 1, 23, 0xf, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_REVERS_IN_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_REVERS_OUT_E, {1, 0, 1, 23, 0xf, 1, 0}},


        {RPMSG_RVC_MSG_TYPE_REVERS_IN_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_REVERS_OUT_E, {1, 0, 1, 23, 0xf, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E, {1, 0, 1, 23, 0xf, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E, {1, 0, 1, 23, 0xf, 1, 0}},
            
        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_ERR_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_BACK_CAMERA_OK_E, {1, 0, 1, 23, 0xf, 1, 0}},

        {RPMSG_RVC_MSG_TYPE_MCU_RADAR_E, {1, 1, 1, 1, 0, 0, 0}},

        {RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, {1, 0, 1, 23, 0xf, 1, 0}},
        {RPMSG_RVC_MSG_TYPE_RADAR_ERR_TO_E, {1, 0, 1, 23, 0xf, 1, 0}},
    };
    unsigned int ui_test_idx = 0;
    unsigned char auc_buff[28];    

    printk(KERN_EMERG"%s.%d you will come in test status \n", __FUNCTION__, __LINE__);
    
    for (;;) 
    {
        if (kthread_should_stop())
        {
            break;
        }

        if (ui_test_idx >= (sizeof(st_cmd_info)/sizeof(st_cmd_info[0])))
        {
            ui_test_idx = 0;
        }
        if (RPMSG_RVC_MSG_TYPE_MCU_RADAR_E == st_cmd_info[ui_test_idx].e_msg_type)
        {
            //ADAYO_DEBUG_BUFF(__dump_radar_just_for_cmd(&(st_cmd_info[ui_test_idx].test_cmd)), 22);
            __rpmsg_rvc_msg_send(st_cmd_info[ui_test_idx].e_msg_type, __dump_radar_just_for_cmd(&(st_cmd_info[ui_test_idx].test_cmd)), 22);
        }
        else
        {
            memset(&auc_buff, 0, sizeof(auc_buff));
            __rpmsg_rvc_msg_send(st_cmd_info[ui_test_idx].e_msg_type, auc_buff, sizeof(auc_buff));
        }

        queue_work(st_rpmsg_rvc.keventd_wq, &(st_rpmsg_rvc.work));  
        ui_test_idx++;
        ul_jiffies = msleep_interruptible(800);
    }
    printk(KERN_EMERG"%s.%d you will exit test status ul_jiffies=%lu \n", __FUNCTION__, __LINE__, ul_jiffies);

    //do_exit(0);

    return 0;
}

static int __test_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    static struct task_struct *kthread_test = NULL;
    
    printk(KERN_EMERG"%s.%d  param=%d, password=[%d.%d.%d.%d.%d.%d] \n", \
        __FUNCTION__, __LINE__, puc_paramlist[0], \
        puc_paramlist[1], puc_paramlist[2], puc_paramlist[3],\
        puc_paramlist[4], puc_paramlist[5], puc_paramlist[6]);

    if ((puc_paramlist[1] != 4) || (puc_paramlist[2] != 5) ||\
        (puc_paramlist[3] != 6) || (puc_paramlist[4] != 7) ||\
        (puc_paramlist[5] != 8) || (puc_paramlist[6] != 9))
    {
        printk(KERN_ERR"%s.%d input password is error !!!\n", __FUNCTION__, __LINE__);
        return 0;
    }

    if (puc_paramlist[0] == 1)
    {
        if (kthread_test == NULL)
        {

            if (st_rpmsg_rvc.kthread_rcv_status != NULL)
            {
                st_rpmsg_rvc.i_bkthread_rcv_run = 0;
                kthread_stop(st_rpmsg_rvc.kthread_rcv_status);
                st_rpmsg_rvc.kthread_rcv_status = NULL;
            }
            
            st_rpmsg_rvc.i_breversing = 2;
            //st_rpmsg_rvc.i_bdebugopen = 1;
            kthread_test = kthread_run(__krvc_test_kthread, NULL, "krvc_test");
        }
    }
    else
    {
        if (kthread_test != NULL)
        {
            st_rpmsg_rvc.i_bdebugopen = 0;
            kthread_stop(kthread_test);
            kthread_test = NULL;

            st_rpmsg_rvc.i_bkthread_rcv_run = 1;
            st_rpmsg_rvc.kthread_rcv_status = kthread_run(__rpmsg_rvc_status_kthread, NULL, "krvc_status");
        }
    }
        
    return 0;
}

static int __post_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    RPMSG_RVC_PKG_INFO_S st_pkginfo;
    
    printk(KERN_EMERG"%s.%d come in [paramlist=%d,%d,%d,%d,%d]\n", __FUNCTION__, __LINE__, \
        puc_paramlist[0], puc_paramlist[1], puc_paramlist[2], puc_paramlist[3], puc_paramlist[4]);

    memset(&st_pkginfo, 0, sizeof(st_pkginfo));
    if (puc_paramlist[0] == 1)
    {
        st_pkginfo.u_pkg_info.st_radar.uc_hou_zuo = puc_paramlist[1];
        st_pkginfo.u_pkg_info.st_radar.uc_hou_zuo_zhong = puc_paramlist[2];
        st_pkginfo.u_pkg_info.st_radar.uc_hou_you_zhong = puc_paramlist[3];
        st_pkginfo.u_pkg_info.st_radar.uc_hou_you = puc_paramlist[4];
        st_pkginfo.u_pkg_info.st_radar.uc_radar_fault = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_signal = 1;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_node = 0;
        printk(KERN_EMERG" you will set post radar [%d,%d,%d,%d] \n", \
            puc_paramlist[1], puc_paramlist[2],\
            puc_paramlist[3], puc_paramlist[4]);
    }
    else
    {
        st_pkginfo.u_pkg_info.st_radar.uc_hou_zuo = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_hou_zuo_zhong = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_hou_you_zhong = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_hou_you = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_fault = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_signal = 1;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_node = 0;
        printk(KERN_EMERG" you will close post radar \n");
    }

    st_pkginfo.uc_bsendrpmsg = 0;
    __rpmsg_rvc_radar_msg_do(st_rpmsg_rvc.rpdev, &st_pkginfo);

    return 0;
}
static int __lead_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    RPMSG_RVC_PKG_INFO_S st_pkginfo;
    
    printk(KERN_EMERG"%s.%d come in [paramlist=%d,%d,%d,%d,%d]\n", __FUNCTION__, __LINE__, \
        puc_paramlist[0], puc_paramlist[1], puc_paramlist[2], puc_paramlist[3], puc_paramlist[4]);

    memset(&st_pkginfo, 0, sizeof(st_pkginfo));
    if (puc_paramlist[0] == 1)
    {
        st_pkginfo.u_pkg_info.st_radar.uc_qian_zuo = puc_paramlist[1];
        st_pkginfo.u_pkg_info.st_radar.uc_qian_zuo_zhong = puc_paramlist[2];
        st_pkginfo.u_pkg_info.st_radar.uc_qian_you_zhong = puc_paramlist[3];
        st_pkginfo.u_pkg_info.st_radar.uc_qian_you = puc_paramlist[4];
        st_pkginfo.u_pkg_info.st_radar.uc_radar_fault = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_signal = 1;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_node = 0;
        printk(KERN_EMERG" you will set lead radar [%d,%d,%d,%d] \n", \
            puc_paramlist[1], puc_paramlist[2],\
            puc_paramlist[3], puc_paramlist[4]);
    }
    else
    {
        st_pkginfo.u_pkg_info.st_radar.uc_qian_zuo = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_qian_zuo_zhong = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_qian_you_zhong = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_qian_you = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_fault = 0;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_signal = 1;
        st_pkginfo.u_pkg_info.st_radar.uc_radar_node = 0;
        printk(KERN_EMERG" you will close lead radar \n");
    }

    st_pkginfo.uc_bsendrpmsg = 0;
    __rpmsg_rvc_radar_msg_do(st_rpmsg_rvc.rpdev, &st_pkginfo);

    return 0;
}

static int __rcv_debug_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    st_rpmsg_rvc.i_bdebugopen = puc_paramlist[0];
    return 0;
}

static int __rcv_rpmsg_debug_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    st_rpmsg_rvc.i_brpmsgdbgopen = puc_paramlist[0];
    st_rpmsg_rvc.ui_rpmsgdbgfltcmd = puc_paramlist[1];

    printk(KERN_EMERG"%s.%d dbg [%s] cmd=%#x \n", __FUNCTION__, __LINE__, \
        (st_rpmsg_rvc.i_brpmsgdbgopen == 0 ? "close" : "open"), st_rpmsg_rvc.ui_rpmsgdbgfltcmd);
    
    return 0;
}

static int __showpic_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    RPMSG_RVC_PKG_INFO_S st_pkginfo;
    RADAR_PIC_STATUC_S st_tmpinfo;
 
    printk(KERN_EMERG"%s.%d come in [paramlist=%d]\n", __FUNCTION__, __LINE__, puc_paramlist[0]);
    memset(&st_pkginfo, 0, sizeof(st_pkginfo));
    st_pkginfo.u_pkg_info.st_radar.uc_radar_node = puc_paramlist[0];
    
    st_tmpinfo.ui_car_status = ((puc_paramlist[0] == 0) ? 0 : 1);
    st_tmpinfo.ui_car_distance = 0;
    st_tmpinfo.uc_radara[0] = puc_paramlist[1];
    st_tmpinfo.uc_radara[1] = puc_paramlist[2];
    st_tmpinfo.uc_radara[2] = puc_paramlist[3];

    rvc_gui_radar_show_normal(&st_tmpinfo);
    
    return 0;
}

static int __kthread_onoff_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    printk(KERN_EMERG"%s.%d come in [paramlist=%d]\n", __FUNCTION__, __LINE__, puc_paramlist[0]);
    st_rpmsg_rvc.i_bkthread_rcv_run = puc_paramlist[0];

    return 0;
}

static int __beeper_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    printk(KERN_EMERG"%s.%d come in [paramlist=%d]\n", __FUNCTION__, __LINE__, puc_paramlist[0]);

    __rpmsg_rvc_beerper(puc_paramlist[0]);

    return 0;
}

static int __rcv_status_cmd_callback(unsigned int *puc_paramlist, unsigned int ui_paramnum)
{
    int i_new_rcv_stats = 0;
    unsigned long ul_new_status_jiffies;
    
    printk(KERN_EMERG"%s.%d in --------------------------------------- \n", __FUNCTION__, __LINE__);

    mutex_lock_interruptible(&rvc_lock); 

    printk(KERN_EMERG"  i_binit=%d, rpdev=%#x, i_breversing=%#x, i_bkthread_rcv_run=%d \n", \
        st_rpmsg_rvc.i_binit, (int)(st_rpmsg_rvc.rpdev), \
        st_rpmsg_rvc.i_breversing, st_rpmsg_rvc.i_bkthread_rcv_run);


    printk(KERN_EMERG"  i_bdebugopen=%d, timer:[onoff=%d, tick=%d, tick_cp=%d, status=%d] \n", \
        st_rpmsg_rvc.i_bdebugopen, st_rpmsg_rvc.st_timer.uc_time_onoff, \
        st_rpmsg_rvc.st_timer.uc_time_tick, st_rpmsg_rvc.st_timer.uc_time_tick_cp,\
        st_rpmsg_rvc.st_timer.uc_time_status);

    printk(KERN_EMERG"  i_brpmsgdbgopen=%d, ui_rpmsgdbgfltcmd=%#x \n", \
        st_rpmsg_rvc.i_brpmsgdbgopen, st_rpmsg_rvc.ui_rpmsgdbgfltcmd);

    printk(KERN_EMERG"  i_camera_status=%d, i_camera_ct=%d, cvbsCheck=%d \n", \
                st_rpmsg_rvc.i_camera_status, st_rpmsg_rvc.i_camera_ct, cvbsCheck());
    
    printk(KERN_EMERG"  i_camera_status=%d, i_camera_ct=%d, cvbsCheck=%d \n", \
                st_rpmsg_rvc.i_camera_status, st_rpmsg_rvc.i_camera_ct, cvbsCheck());

    printk(KERN_EMERG"  last radar status {status=%d, dist=%d, [%d.%d.%d.%d.%d]} [exit=%#x] \n", \
                st_rpmsg_rvc.st_last_radarpic_status.ui_car_status, \
                st_rpmsg_rvc.st_last_radarpic_status.ui_car_distance,
                st_rpmsg_rvc.st_last_radarpic_status.uc_radara[0], \
                st_rpmsg_rvc.st_last_radarpic_status.uc_radara[1],\
                st_rpmsg_rvc.st_last_radarpic_status.uc_radara[2],\
                st_rpmsg_rvc.st_last_radarpic_status.uc_radara[3],\
                st_rpmsg_rvc.st_last_radarpic_status.uc_radara[4],\
                st_rpmsg_rvc.i_radarexit);

    get_rvc_flag(&i_new_rcv_stats, &ul_new_status_jiffies);
    printk(KERN_EMERG"  i_new_rcv_stats=%d, ul_new_status_jiffies=%lu \n", \
                i_new_rcv_stats, ul_new_status_jiffies);
    
    mutex_unlock(&rvc_lock);
    
    printk(KERN_EMERG"%s.%d out --------------------------------------- \n", __FUNCTION__, __LINE__);

    return 0;
}


static ssize_t rvc_read_proc(struct file *file, char __user *page, size_t size, loff_t *ppos)
{
    adayo_proc_cmd_help();
    
    return 0;
}

static ssize_t rvc_write_proc(struct file *filp, const char __user *buffer, size_t count, loff_t *off)
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

static const struct file_operations enable_proc_ops = {
    .owner = THIS_MODULE,
    .read = rvc_read_proc,
    .write = rvc_write_proc,
};

int rpmsg_rvc_proc_test_init(void) 
{  
    static struct proc_dir_entry *pst_rvc_proc = NULL;

    if (pst_rvc_proc == NULL)
    {
        pst_rvc_proc = proc_create("radar_send", 0777, NULL, &enable_proc_ops);  
        if (pst_rvc_proc != NULL)
        {
            adayo_proc_cmd_init("/proc/radar_send");
            ADAYO_ADD_CMD("post", 5, "[enable] [hou_zuo] [hou_zuo_zhong] [hou_you_zhong] [hou_you]", "1 1 2 3 4", __post_cmd_callback);
            ADAYO_ADD_CMD("lead", 5, "[enable] [qian_zuo] [qian_zuo_zhong] [qian_you_zhong] [qian_you]", "0 * * * *", __lead_cmd_callback);
            ADAYO_ADD_CMD("showpic", 4, "[car status 0/1][l 0~4][m 0~4][r 0~4]", "0 1 2 3 4", __showpic_cmd_callback);      /* 显示图片 */
            ADAYO_ADD_CMD("kthread_run", 1, "[on/off]", "1", __kthread_onoff_cmd_callback);      /* 开关kthread */
            ADAYO_ADD_CMD("beeper", 1, "[0~1000000000]", "100", __beeper_cmd_callback);      /* beer调试 */
            
            ADAYO_ADD_CMD("test_001", 7, "[on/off][password]", "1 * * * * * *", __test_cmd_callback);      /* 开关kthread */
            ADAYO_ADD_CMD("rcv_debug", 1, "[level]", "1", __rcv_debug_cmd_callback);      /* 开启debug */
            ADAYO_ADD_CMD("rcv_status", 0, NULL, NULL, __rcv_status_cmd_callback);      /* 开启debug */
            ADAYO_ADD_CMD("rpmsg_debug", 2, "[on/off] [cmd/65535]", "1 35072", __rcv_rpmsg_debug_cmd_callback);      /* 开启debug */
        }
    }
    
    return 0;  
}


#endif
