#ifndef __ERRCODE_ENUM_H_
#define __ERRCODE_ENUM_H_

#define  after_bootz 2  //����֮����ϱ�����
#define  on_bootz    0  //���������е��ϱ�����

//������ֹ������ͷ �̶����� 3��
typedef enum 
{ 	
	ENUM_HAL_HEAD_MCU = 0xaa,    //mcu
	ENUM_HAL_HEAD_OS = 0xa0,     //os
	ENUM_HAL_HEAD_MPU = 0xa1     //mpu
}CONFIG_DIAGNOSIS_HEAD;

//������ֹ������ֵ  ����� 
typedef enum 
{ 	
	ENUM_HAL_CODE_RV = 0x01,            //�����쳣
	ENUM_HAL_CODE_USBSD = 0x02,			//usb��sd�쳣
	ENUM_HAL_CODE_DIALOG = 0x03,		//�쳣����
	ENUM_HAL_CODE_UI = 0x04,    		//UI�쳣
	ENUM_HAL_CODE_CRASH = 0x05,    		//����
	ENUM_HAL_CODE_STRIKE = 0x06,		//�޷�����
	ENUM_HAL_CODE_DATASAFE = 0x07,		//���ݰ�ȫ
	
//---------------�ض��׶�------------------------------------------

	ENUM_HAL_CODE_TOUCHSCREEN = 0x08,   //�������쳣
	ENUM_HAL_CODE_UPDATE = 0x09,		//��ȡ������־λʧ��
	ENUM_HAL_CODE_WIFI = 0x0A,			//wifi��������ʧ��
	ENUM_HAL_CODE_RADAR = 0x0B			//�״����
	
	//....								//�����
}CONFIG_DIAGNOSIS_CODE;


//������ֹ������ֵ�Ĳ���  �����
typedef enum   
{ 	
//��������
	ENUM_HAL_VALUE_RV_ARM2_FAIL = 0x01,			
	ENUM_HAL_VALUE_RV_INIT_EMMC = 0x02,
	ENUM_HAL_VALUE_RV_NO_SIGNAL = 0x03,
	ENUM_HAL_VALUE_RV_VIDEO_CHANNEL = 0x04,
	ENUM_HAL_VALUE_RV_NO_CAMERA = 0x05,     //����ʱ��ⲻ������ͷ
	//.... 
//usb��sd�쳣
	ENUM_HAL_VALUE_USB_CANNOT_RECOG = 0x01,
	ENUM_HAL_VALUE_USB_UNMOUNT = 0x02,
	ENUM_HAL_VALUE_SD_CANNOT_RECOG = 0x03,
	ENUM_HAL_VALUE_SD_UNMOUNT = 0x04,
	ENUM_HAL_VALUE_USBSD_VIDEO_PLAY_ABNOMAL = 0x05,
	ENUM_HAL_VALUE_USBSD_VIDEO_PLAY_BLACKSCREEN  = 0x06,
	ENUM_HAL_VALUE_SD_INSERT_TO_DIA  = 0x07,
	ENUM_HAL_VALUE_SD_READONLY  = 0x08,				//sd��ֻ��
	
//�����쳣
	ENUM_HAL_VALUE_DIALOG_SERVICE = 0x01,
	ENUM_HAL_VALUE_DIALOG_APP = 0x02,
	ENUM_HAL_VALUE_DIALOG_NAVIGATION = 0x03,
	ENUM_HAL_VALUE_DIALOG_INSUFFICIENT_MEMERY = 0x04,
	ENUM_HAL_VALUE_DIALOG_MANDATORY_WARMING = 0x05,
	
//UI��ʾ�쳣
	ENUM_HAL_VALUE_UI_BUSINESS_LOGIC_ABNORMAL = 0x01,
	ENUM_HAL_VALUE_UI_DATE_CHANGED = 0x02,
	
//����
	ENUM_HAL_VALUE_CRASH_TS_ABNORMAL = 0x01,
	ENUM_HAL_VALUE_CRASH_DRIVER_THREAD = 0x02,
	ENUM_HAL_VALUE_CRASH_THIRD_PARTY = 0x03,
	ENUM_HAL_VALUE_CRASH_CRYSTAL_ABNORMAL = 0x04,

//�޷�����
	ENUM_HAL_VALUE_STRIKE_INIT_HANDSHAKE_FAIL = 0x01,
	ENUM_HAL_VALUE_STRIKE_LCD_FRAME = 0x02,
	ENUM_HAL_VALUE_STRIKE_FLASH = 0x03,
	ENUM_HAL_VALUE_STRIKE_CMU = 0x04,
	
//���ݰ�ȫ
	ENUM_HAL_VALUE_DATASAFE_LOST = 0x01,
	ENUM_HAL_VALUE_DATASAFE_CHECK_SELF = 0x02,
	
//---------------�ض��׶�------------------------------------------
	
//�������쳣
	ENUM_HAL_VALUE_TOUCHSCREEN_INIT_FAIL = 0x01,
	ENUM_HAL_VALUE_TOUCHSCREEN_INIT_DSP927_FAIL = 0x02,
	
//��ȡ������־λʧ��
	ENUM_HAL_VALUE_UPDATE_FLAG_FAIL = 0x01,

//wifi��������ʧ��	
	ENUM_HAL_VALUE_WIFI_INIT_FAIL = 0x01,

//�״����	
	ENUM_HAL_VALUE_RADAR = 0x01
	
//��������ӵĹ�����ֵ��ӹ��������...

}CONFIG_DIAGNOSIS_PARA;


#endif