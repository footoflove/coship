/****************************************************************************/
/*                   Entropic (Shanghai) Co, LTD                            */
/*                        SOFTWARE FILE/MODULE HEADER                       */
/*                 Copyright Entropic Co, LTD                               */
/*                            All Rights Reserved                           */
/****************************************************************************/
/*
 * Filename:        cs_eeprom_plus.c
 *
 *
 * Description:     API implementation for COSHIP interface layer .
 *
 *
 *-------------------------------------------------------------------------------
 *ENTROPIC COMMENTS ON COSHIP HEADER FILE:
	 2013/11/06
		 The APIs in this header file are NOT required for Android DVB-S2 plus OTT project. 
 *-------------------------------------------------------------------------------

 ****************************************************************************/
#include "udi2_error.h"
#include "udi2_public.h"
#include "udi2_typedef.h"
#include "udidrv_log.h"

#include "cs_eeprom_plus.h"

/*
@brief ִ�д���EEPROM��ʼ����

��Ҫ�����ǹ涨����EEPROM��������ַ�������ͺ�
@param[in] nIndex �������ڵ�I2C���ߵ�����
@param[in] dwEPRDevAddr ����EEPROM��I2C������ַ(0b 1 0 1 0 x x x 0)(����x�ɾ���Ӳ������)
@param[in] nEPRChipType ����EEPROM������, �μ�ö��CSEPRTYPE��
@return CSHDI_SUCCESS:��ʼ���ɹ�;CSHDI_FAILURE����ʼ��ʧ��
@note 
- �������ֻ����ϵͳ��λ��ʱ�򱻵���һ�Σ�����ʱ�����ٱ����á�
- �ú���������������EEPROM API������ʹ��֮ǰ���á��������ɲ���Ԥ֪�Ĵ���
*/
CSHDI_Error_t CSEPRInitialize(  CSHDII2CIndex nIndex, BYTE bEPRDevAddr, CSEPRTYPE nEPRChipType )
{
	CSHDI_Error_t Retcode = 0;	
	UDIDRV_LOGI("%s %s (Retcode =%d)\n", __FUNCTION__, UDIDRV_NOT_REQUIRED, Retcode);    
	return Retcode;
} 	

/*
@brief ��ȡEEPROM�����汾�š�

@param[in] pchVer ��д��洢������ʼ��ַ��
@param[in] nSize ��ȡ���ַ������ȡ�
@return ʵ�ʵõ��İ汾�ַ�������
@note ��ʽ��"[Module Name]-[Module Type]-[Version Number]-[Date]-[Time]\n"
*/
int CSEEPROMGetVersion(PCHAR pchVer, int nSize)
{
	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;	
	UDIDRV_LOGI("%s %s (Retcode =%d)\n", __FUNCTION__, UDIDRV_NOT_REQUIRED, Retcode);    
	return Retcode;
}

