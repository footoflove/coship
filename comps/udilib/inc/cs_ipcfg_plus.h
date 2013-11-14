/******************************************************************************
-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP HEADER FILE:
	2013/11/06
		The APIs in this header file are NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------
*******************************************************************************/

#ifndef _CS_IPCFG_PLUS_H_
#define _CS_IPCFG_PLUS_H_

#include "udi2_ipcfg.h"
#ifdef __cplusplus
extern "C" {
#endif

/**@brief ������Ϣ*/
typedef struct 
{
    char szEthName[CSUDI_ETHER_NETIFNAME_MAX_LEN];		///< ����������
    CSUDIEtherType_E eEthType;  //�������ͣ�������������������������pppoe��
}CSUDIEthInfo_S;

/***************************************************************************
* ����: CSIPCFG_TCPSetup
* ����: ��ʼ��������Э��ջ
* ����:  
*       pBuffer :  ΪЭ��ջ����Ŀռ䣬���û�����ռ䡣
*       nSize :ΪЭ��ջ����Ŀռ��С����С��0x10000��64K���ֽڡ�     
*	  nInputThreadPrio: Э��ջinput�߳����ȼ����������0
*       nInputThreadStackSize : Э��ջinput�̶߳�ջ��С����С��32*1024�ֽ�
*       nTimerThreadPrio: Э��ջtimer�߳����ȼ����������0
*       nInputThreadStackSize : Э��ջtimer�̶߳�ջ��С����С��32*1024�ֽ�
*	  usConfig :	��usConfig =CS_CONFIG_DHCPCʱ��ʾЭ��ջ��Ҫ����DHCPЭ�顣
*                   ��usConfig = CS_CONFIG_PPPOEʱ��ʾЭ��ջ��Ҫ����PPPOEЭ��
*
*                   
* ���: 
*     
* ����ֵ: 
*   �ɹ���0
*   ʧ�ܣ���0�������-1ͨ���ǲ�������
********************************************************************************/
int CSIPCFG_TCPSetup (
		char*          pchBuffer,
		unsigned int   unSize,
		int            nInputThreadPrio,
		int      stInputThreadStackSize,
		int            nTimerThreadPrio,
		int      stTimerThreadStackSize,
		unsigned short usConfig );

/**
@brief ��������

@param[in] pcEthName �����豸����
@param[in] eType ��������
@return �ɹ����� CSUDI_SUCCESS,ʧ�ܷ��� CSUDI_FAILURE
@��ע:�û������pppoe������Ҫ����
*/
CSUDI_Error_Code  CSIPCFGGAddEth(char * pcEthName,int nLen, CSUDIEtherType_E eType);

/**
@brief ɾ������,��Ҫ���ڿɲ�������豸
@param[in] pcEthName ��Ҫɾ���������豸����
@return �ɹ����� CSUDI_SUCCESS,ʧ�ܷ��� CSUDI_FAILURE
*/
CSUDI_Error_Code  CSIPCFGGRemoveEth(char * pcEthName);

#ifdef __cplusplus
}
#endif
#endif /* __CSIPCFGPLUS_H__ */

