#ifndef __RVC_GUI_SHOW_H__
#define __RVC_GUI_SHOW_H__


#define RADAR_NUM_MAX               5

typedef struct
{
    unsigned int ui_car_status;                                 /* 0:OK, 1:car err */
    unsigned int ui_car_distance;                               /* 车距值 */
    unsigned char uc_radara[RADAR_NUM_MAX];                     /* idx:0、后左，1、后中，2、后右； 数值 0:不显示，1、2、3，other:故障 */
}RADAR_PIC_STATUC_S;

/* 
    此接口为普通GUI，由于没有做缓存，各个接口的绘画请注意区域不能有冲突，如果有冲突，可以参考radar的设计
    所有接口没有互斥，只能在一个task调用
 */


/*
    显示倒车线条
*/
int rvc_gui_line_show(int i_bclear);

/*
    显示倒车摄像头状态，error时，i_bclear==1
*/
int rvc_gui_camera_show(int i_bclear);

/*
    显示倒车雷达状态
*/
int rvc_gui_radar_show_normal(RADAR_PIC_STATUC_S *pst_radar_status);

int rvc_gui_radar_show_err(int i_bshow, int i_radarexit);

#endif //__RVC_GUI_SHOW_H__

