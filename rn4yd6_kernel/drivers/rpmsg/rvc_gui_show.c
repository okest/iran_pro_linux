#include <linux/kernel.h>  
#include <linux/module.h> 
#include <linux/types.h>
#include <linux/kern_levels.h>
#include <linux/slab.h>

#include "rvc_gui_show.h"


#include "pic/car.h"  
#include "pic/car_d.h"  
#include "pic/line.h"  
#include "pic/s_back_left_1.h"  
#include "pic/s_back_left_2.h"  
#include "pic/s_back_left_3.h"  
#include "pic/s_back_left_d.h" 
#include "pic/s_back_mid_1.h" 
#include "pic/s_back_mid_2.h" 
#include "pic/s_back_mid_3.h"  
#include "pic/s_back_mid_d.h" 
#include "pic/s_back_right_1.h" 
#include "pic/s_back_right_2.h" 
#include "pic/s_back_right_3.h" 
#include "pic/s_back_right_d.h" 
#include "pic/camera_err_c.h" 
#include "pic/camera_err_e.h" 
#include "pic/radar_comerr_c.h" 
#include "pic/radar_comerr_e.h" 
#include "pic/radar_err_c.h" 
#include "pic/radar_err_e.h" 
#include "pic/s_font_left_1.h" 
#include "pic/s_font_left_2.h" 
#include "pic/s_font_left_3.h" 
#include "pic/s_font_left_d.h" 
#include "pic/s_font_right_1.h" 
#include "pic/s_font_right_2.h" 
#include "pic/s_font_right_3.h" 
#include "pic/s_font_right_d.h" 
#include "pic/all_radar_d.h" 

extern int show_pic( int nPicXDest, int nPicYDest, int nWidth, int nHeight,unsigned int *pSrcData ,unsigned int BgColor);
extern int  reflush_pic( int nPicXDest, int nPicYDest, int nWidth, int nHeight,unsigned int BgColor);

typedef struct
{
    int i_x;
    int i_y;
    int i_w;
    int i_h;
}RADAR_PIC_RECT_S;

typedef struct
{
    const unsigned int *pui_picdata;
    unsigned int pui_picarr_len;
    RADAR_PIC_RECT_S st_rect;
}RADAR_PIC_SHOW_INFO_S;


static int i_language_type = 0;                 /* 1:英文; other:中文; */
static int __init __rvc_language_setup(char *str)
{
    int i_len = strlen(str);
    int i_mm;

    i_language_type = 0;
    for (i_mm = 0; i_mm < i_len; i_mm++)
    {
        (i_language_type) = ((i_language_type) * 10) + (str[i_mm] - '0');
    }

    i_language_type = ((i_language_type == 1) ? 1 : 0);

    pr_info("%s.%d doing command CN_EN=[%s], i=%d \n", __FUNCTION__, __LINE__, str, i_language_type);
    
    return 0;
}

__setup("language=", __rvc_language_setup);



static RADAR_PIC_SHOW_INFO_S g_ast_radar_picinfo[] =
{
    {car, sizeof(car)/sizeof(car[0]), {665, 65, 80, 173}},
    {s_back_left_d, sizeof(s_back_left_d)/sizeof(s_back_left_d[0]), {636, 233, 50, 30}},
    {s_back_left_1, sizeof(s_back_left_1)/sizeof(s_back_left_1[0]), {636, 233, 50, 30}},
    {s_back_left_2, sizeof(s_back_left_2)/sizeof(s_back_left_2[0]), {636, 233, 50, 30}},
    {s_back_left_3, sizeof(s_back_left_3)/sizeof(s_back_left_3[0]), {636, 233, 50, 30}},
    {s_back_mid_d, sizeof(s_back_mid_d)/sizeof(s_back_mid_d[0]), {680, 242, 51, 24}},
    {s_back_mid_1, sizeof(s_back_mid_1)/sizeof(s_back_mid_1[0]), {680, 242, 51, 24}},
    {s_back_mid_2, sizeof(s_back_mid_2)/sizeof(s_back_mid_2[0]), {680, 242, 51, 24}},
    {s_back_mid_3, sizeof(s_back_mid_3)/sizeof(s_back_mid_3[0]), {680, 242, 51, 24}},
    {s_back_right_d, sizeof(s_back_right_d)/sizeof(s_back_right_d[0]), {725, 233, 50, 30}},
    {s_back_right_1, sizeof(s_back_right_1)/sizeof(s_back_right_1[0]), {725, 233, 50, 30}},
    {s_back_right_2, sizeof(s_back_right_2)/sizeof(s_back_right_2[0]), {725, 233, 50, 30}},
    {s_back_right_3, sizeof(s_back_right_3)/sizeof(s_back_right_3[0]), {725, 233, 50, 30}},

    {s_font_left_d, sizeof(s_font_left_d)/sizeof(s_font_left_d[0]), {663, 11, 41, 44}},
    {s_font_left_1, sizeof(s_font_left_1)/sizeof(s_font_left_1[0]), {663, 11, 41, 44}},
    {s_font_left_2, sizeof(s_font_left_2)/sizeof(s_font_left_2[0]), {663, 11, 41, 44}},
    {s_font_left_3, sizeof(s_font_left_3)/sizeof(s_font_left_3[0]), {663, 11, 41, 44}},
        
    {s_font_right_d, sizeof(s_font_right_d)/sizeof(s_font_right_d[0]), {706, 11, 41, 44}},
    {s_font_right_1, sizeof(s_font_right_1)/sizeof(s_font_right_1[0]), {706, 11, 41, 44}},
    {s_font_right_2, sizeof(s_font_right_2)/sizeof(s_font_right_2[0]), {706, 11, 41, 44}},
    {s_font_right_3, sizeof(s_font_right_3)/sizeof(s_font_right_3[0]), {706, 11, 41, 44}},
};

static void __rvc_gui_pic_recv_cal(RADAR_PIC_RECT_S *pst_pic1, RADAR_PIC_RECT_S *pst_pic2, RADAR_PIC_RECT_S *pst_picout)
{
    RADAR_PIC_RECT_S st_tmprect = {0, 0, 0, 0};

    if (((pst_pic1->i_h == 0) || (pst_pic1->i_w == 0)) && ((pst_pic2->i_h == 0) || (pst_pic2->i_w == 0)))
    {
        memcpy(pst_picout, &st_tmprect, sizeof(RADAR_PIC_RECT_S));
        return ;
    }
    
    if ((pst_pic1->i_h == 0) || (pst_pic1->i_w == 0))
    {
        memcpy(pst_picout, pst_pic2, sizeof(RADAR_PIC_RECT_S));
        return ;
    }

    if ((pst_pic2->i_h == 0) || (pst_pic2->i_w == 0))
    {
        memcpy(pst_picout, pst_pic1, sizeof(RADAR_PIC_RECT_S));
        return ;
    }
    
    if (pst_pic1->i_x < pst_pic2->i_x)
    {
        st_tmprect.i_x = pst_pic1->i_x;
    }
    else
    {
        st_tmprect.i_x = pst_pic2->i_x;
    }

    if ((pst_pic1->i_x + pst_pic1->i_w) < (pst_pic2->i_x + pst_pic2->i_w))
    {
        st_tmprect.i_w = (pst_pic2->i_x + pst_pic2->i_w) - st_tmprect.i_x;
    }
    else
    {
        st_tmprect.i_w = (pst_pic1->i_x + pst_pic1->i_w) - st_tmprect.i_x;
    }
    
    if (pst_pic1->i_y < pst_pic2->i_y)
    {
        st_tmprect.i_y = pst_pic1->i_y;
    }
    else
    {
        st_tmprect.i_y = pst_pic2->i_y;
    }

    if ((pst_pic1->i_y + pst_pic1->i_h) < (pst_pic2->i_y + pst_pic2->i_h))
    {
        st_tmprect.i_h = (pst_pic2->i_y + pst_pic2->i_h) - st_tmprect.i_y;
    }
    else
    {
        st_tmprect.i_h = (pst_pic1->i_y + pst_pic1->i_h) - st_tmprect.i_y;
    }

    memcpy(pst_picout, &st_tmprect, sizeof(st_tmprect));

    return;
}

static int __rvc_gui_radar_picdo_show(unsigned int *pui_pic_addr)
{
    int i_mm;
    int i_ret;
    RADAR_PIC_SHOW_INFO_S *pst_info;
    
    for (i_mm = 0; i_mm < sizeof(g_ast_radar_picinfo)/sizeof(g_ast_radar_picinfo[0]); i_mm++)
    {
        if (pui_pic_addr == g_ast_radar_picinfo[i_mm].pui_picdata)
        {
            pst_info = &g_ast_radar_picinfo[i_mm];
            i_ret = show_pic(pst_info->st_rect.i_x, pst_info->st_rect.i_y, pst_info->st_rect.i_w, pst_info->st_rect.i_h, (unsigned int *)(pst_info->pui_picdata), 0x0);
            return i_ret;
        }
    }
    return -1;
}

static RADAR_PIC_RECT_S *__rvc_gui_radar_picdo_rect_max(void)
{
    static RADAR_PIC_RECT_S st_max_rect = {0, 0, 0, 0};
    int i_mm;
    RADAR_PIC_RECT_S st_tmprect = {800, 480, 0, 0};

    if ((st_max_rect.i_x == 0) && (st_max_rect.i_y == 0) &&\
        (st_max_rect.i_w == 0) && (st_max_rect.i_h == 0))
    {
        for (i_mm = 0; i_mm < sizeof(g_ast_radar_picinfo)/sizeof(g_ast_radar_picinfo[0]); i_mm++)
        {
            __rvc_gui_pic_recv_cal(&st_tmprect, &(g_ast_radar_picinfo[i_mm].st_rect), &st_tmprect);
        }
        pr_info("%s.%d get rect max [x=%d,y=%d,w=%d,h=%d] \n", \
            __FUNCTION__, __LINE__, st_tmprect.i_x, st_tmprect.i_y, st_tmprect.i_w, st_tmprect.i_h);
        memcpy(&st_max_rect, &st_tmprect, sizeof(st_tmprect));
    }
    return &st_max_rect;
}

static unsigned int *pui_argb32_buff = NULL;

static int __rvc_gui_radarsur_fill(void)
{
    RADAR_PIC_RECT_S *pst_max_rect;
    
    pst_max_rect = __rvc_gui_radar_picdo_rect_max();
    if (pui_argb32_buff == NULL) 
    {
        return -1;
    }

    memset(pui_argb32_buff, 0, (pst_max_rect->i_w * pst_max_rect->i_h * 4));

    return 0;
}


static int __rvc_gui_radarsur_blit(unsigned int *pui_pic_addr)
{
    RADAR_PIC_RECT_S *pst_max_rect, st_tmprect = {0, 0, 0, 0};
    int i_mm;
    RADAR_PIC_SHOW_INFO_S *pst_info;
    int i_xx, i_yy, i_height, i_width;
    unsigned int *pui_tmpaddr;
    unsigned int ui_picidx = 0;

    pst_max_rect = __rvc_gui_radar_picdo_rect_max();
    if (pui_argb32_buff == NULL) 
    {
        return -1;
    }

    for (i_mm = 0; i_mm < sizeof(g_ast_radar_picinfo)/sizeof(g_ast_radar_picinfo[0]); i_mm++)
    {
        if (pui_pic_addr == g_ast_radar_picinfo[i_mm].pui_picdata)
        {
            pst_info = &g_ast_radar_picinfo[i_mm];
            st_tmprect.i_x = pst_info->st_rect.i_x - pst_max_rect->i_x;
            st_tmprect.i_y = pst_info->st_rect.i_y - pst_max_rect->i_y;
            st_tmprect.i_w = pst_info->st_rect.i_w;
            st_tmprect.i_h = pst_info->st_rect.i_h;
            break;
        }
    }
    
    i_height = st_tmprect.i_y + st_tmprect.i_h;
    i_width = st_tmprect.i_x + st_tmprect.i_w;
    for (i_yy = st_tmprect.i_y; i_yy < i_height; i_yy++)
    {
        pui_tmpaddr = pui_argb32_buff + (i_yy * pst_max_rect->i_w);
        for (i_xx = st_tmprect.i_x; i_xx < i_width; i_xx++, ui_picidx++)
        {
            if (pst_info->pui_picdata[ui_picidx] != 0)
            {
                pui_tmpaddr[i_xx] = pst_info->pui_picdata[ui_picidx];
            }
        }
    }

    if (ui_picidx != pst_info->pui_picarr_len)
    {
        printk(KERN_ERR"zgr %s.%d radar bilt error [%d] [%d] \n", \
            __FUNCTION__, __LINE__, ui_picidx, pst_info->pui_picarr_len);
    }

    return 0;
}

static int __rvc_gui_radarsur_updata(void)
{
    RADAR_PIC_RECT_S *pst_max_rect;
    
    if (pui_argb32_buff == NULL) 
    {
        return 0;
    }
    pst_max_rect = __rvc_gui_radar_picdo_rect_max();

    return show_pic(pst_max_rect->i_x, pst_max_rect->i_y, pst_max_rect->i_w, pst_max_rect->i_h, (unsigned int *)(pui_argb32_buff), 0x1);
}

int rvc_gui_line_show(int i_bclear)
{
    int i_ret;
    
    if (i_bclear == 1)
    {
        i_ret = reflush_pic(0, 0, 800, 480, 0x0);
        if (pui_argb32_buff != NULL) 
        {
            kfree(pui_argb32_buff);
            pui_argb32_buff = NULL;
            pr_info("%s.%d surface buff free success \n", __FUNCTION__, __LINE__);
        }
    }
    else
    {
        RADAR_PIC_RECT_S *pst_max_rect;
        
        pst_max_rect = __rvc_gui_radar_picdo_rect_max();
        if (pui_argb32_buff == NULL) 
        {
            pui_argb32_buff = kmalloc((pst_max_rect->i_w * pst_max_rect->i_h * 4 + 256), GFP_KERNEL);
            if (pui_argb32_buff == NULL)
            {
                pr_info("%s.%d surface buff malloc[%d] fail \n", \
                    __FUNCTION__, __LINE__, (pst_max_rect->i_w * pst_max_rect->i_h * 4 + 256));
            }
            else
            {
                pr_info("%s.%d surface buff malloc[%d] success \n", \
                    __FUNCTION__, __LINE__, (pst_max_rect->i_w * pst_max_rect->i_h * 4 + 256));
            }
        }
        
        i_ret = show_pic(0, 0, 800, 480, (unsigned int *)line, 0x0);
        i_ret |= show_pic(665, 65, 80, 173, (unsigned int *)car, 0x0);
    }
    return i_ret;
}

int rvc_gui_camera_show(int i_bclear)
{
    int i_ret;
    
    if (i_bclear == 1)
    {
	// zjc 
        i_ret = reflush_pic(250, 290, 300, 50, 0x0);

	//zjc show line 
	i_ret = show_pic(0, 0, 800, 480, (unsigned int *)line, 0x0);
    }
    else
    {
	// flush all pic 800 * 480   add zjc
	reflush_pic(0, 0, 800, 480, 0x0);
	
	// show car   add zjc
        show_pic(665, 65, 80, 173, (unsigned int *)car, 0x0);

        if (i_language_type != 1)
        {
            i_ret = show_pic(250, 290, 300, 50, (unsigned int *)camera_err_c, 0x0);
        }
        else
        {
            i_ret = show_pic(250, 290, 300, 50, (unsigned int *)camera_err_e, 0x0);
        }
    }

    return i_ret;
}

/* 实现所有雷达失效闪烁功能 */
int rvc_gui_radar_show_err(int i_bshow, int i_radarexit)
{
    __rvc_gui_radarsur_fill();
    __rvc_gui_radarsur_blit((unsigned int *)car);

#if 0
    if (i_bshow == 1)
    {
        if ((i_radarexit & 0xf) != 0)
        {
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_1);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_2);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_3);

            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_1);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_2);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_3);

            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_1);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_2);
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_3);
        }
        if ((i_radarexit & 0xf0) != 0)
        {
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_1);
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_2);
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_3);

            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_1);
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_2);
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_3);
        }

    }
    __rvc_gui_radarsur_updata();

    //reflush_pic(600, 210, 190, 40, 0x00);
    if (i_language_type != 1)
    {
        show_pic(605, 206, 200, 100, (unsigned int *)radar_comerr_c, 0x0);
    }
    else
    {
        show_pic(605, 206, 200, 100, (unsigned int *)radar_comerr_e, 0x0);
    }
#endif
	 // zjc  all radar diagnosic  change a car 
        show_pic(665, 65, 80, 173, (unsigned int *)car_d, 0x0);

        //zjc show a ! pic
        show_pic(678, 129, 53, 44, (unsigned int *)all_radar_d, 0x0);
    return 0;
}

int rvc_gui_radar_show_normal(RADAR_PIC_STATUC_S *pst_radar_status)
{
    // zjc  show a normal car 
    show_pic(665, 65, 80, 173, (unsigned int *)car, 0x0);

    RADAR_PIC_RECT_S *pst_rect;
    int i_bhaserr = 0;

    pst_rect = __rvc_gui_radar_picdo_rect_max();

    //printk(KERN_EMERG"%s.%d car is ok !\n", __FUNCTION__, __LINE__);
    
#if 0    
    printk(KERN_EMERG"zgr %s.%d radar status [status=%d] [distance=%d] [l=%d,m=%d,r=%d]\n", \
        __FUNCTION__, __LINE__, pst_radar_status->ui_car_status, pst_radar_status->ui_car_distance,\
        pst_radar_status->uc_radara[0], pst_radar_status->uc_radara[1], pst_radar_status->uc_radara[2]);
#endif 

    __rvc_gui_radarsur_fill();
    __rvc_gui_radarsur_blit((unsigned int *)car);
       
    switch(pst_radar_status->uc_radara[0])
    {
        case 1:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_1);
            break;
        case 2:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_2);
            break;
        case 3:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_3);
            break;
        case 0:
            break;
        default:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_left_d);
            i_bhaserr = 1;
            break;
    }

    switch(pst_radar_status->uc_radara[1])
    {
        case 1:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_1);
            break;
        case 2:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_2);
            break;
        case 3:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_3);
            break;
        case 0:
            break;
        default:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_mid_d);
            i_bhaserr = 1;
            break;
    }

    switch(pst_radar_status->uc_radara[2])
    {
        case 1:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_1);
            break;
        case 2:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_2);
            break;
        case 3:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_3);
            break;
        case 0:
            break;
        default:
            __rvc_gui_radarsur_blit((unsigned int *)s_back_right_d);
            i_bhaserr = 1;
            break;
    }

    switch(pst_radar_status->uc_radara[3])
    {
        case 1:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_1);
            break;
        case 2:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_2);
            break;
        case 3:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_3);
            break;
        case 0:
            break;
        default:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_left_d);
            i_bhaserr = 1;
            break;
    }

    switch(pst_radar_status->uc_radara[4])
    {
        case 1:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_1);
            break;
        case 2:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_2);
            break;
        case 3:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_3);
            break;
        case 0:
            break;
        default:
            __rvc_gui_radarsur_blit((unsigned int *)s_font_right_d);
            i_bhaserr = 1;
            break;
    }

    __rvc_gui_radarsur_updata();

#if 0
    if (i_bhaserr == 1)
    {
        if (i_language_type != 1)
        {
            show_pic(605, 206, 200, 100, (unsigned int *)radar_err_c, 0x0);
        }
        else
        {
            show_pic(605, 206, 200, 100, (unsigned int *)radar_err_e, 0x0);
        }
    }
    else
    {
        reflush_pic(605, 206, 200, 100, 0x00);
    }
#endif

    return 0;
}

