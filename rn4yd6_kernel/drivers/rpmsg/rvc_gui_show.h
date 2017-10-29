#ifndef __RVC_GUI_SHOW_H__
#define __RVC_GUI_SHOW_H__


#define RADAR_NUM_MAX               5

typedef struct
{
    unsigned int ui_car_status;                                 /* 0:OK, 1:car err */
    unsigned int ui_car_distance;                               /* ����ֵ */
    unsigned char uc_radara[RADAR_NUM_MAX];                     /* idx:0������1�����У�2�����ң� ��ֵ 0:����ʾ��1��2��3��other:���� */
}RADAR_PIC_STATUC_S;

/* 
    �˽ӿ�Ϊ��ͨGUI������û�������棬�����ӿڵĻ滭��ע���������г�ͻ������г�ͻ�����Բο�radar�����
    ���нӿ�û�л��⣬ֻ����һ��task����
 */


/*
    ��ʾ��������
*/
int rvc_gui_line_show(int i_bclear);

/*
    ��ʾ��������ͷ״̬��errorʱ��i_bclear==1
*/
int rvc_gui_camera_show(int i_bclear);

/*
    ��ʾ�����״�״̬
*/
int rvc_gui_radar_show_normal(RADAR_PIC_STATUC_S *pst_radar_status);

int rvc_gui_radar_show_err(int i_bshow, int i_radarexit);

#endif //__RVC_GUI_SHOW_H__

