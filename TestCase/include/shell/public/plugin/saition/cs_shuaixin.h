#ifndef _CS_SHUAIXIN_H_
#define _CS_SHUAIXIN_H_

#ifdef __cplusplus
extern "C" {
#endif
 

/**
* ˦��JAVA�ص�����
*/
typedef enum {
	EM_CONNECT,/*�ֻ��ͻ��������ӳɹ���*/
	EM_DISCONNECT ,	/*�ֻ��ͻ����жϿ�����*/
	EM_SETURL,   /*2 �м�������ŵ�URL����Ӧ�ã����͸�Ӧ�õĵ�һ������Ƭ��URL���ڶ�����������ƬURL,���;�Ϊ�ַ�����*/ 	
	EM_SETVOLUME ,     /*3 �ֻ�ң�������û����в�������,���͸�Ӧ�õĵ�һ������������ֵ������Ϊint*/
	EM_SETPROCESS,    /*4�ֻ����û����в��Ž���,���͸�Ӧ�õĵ�һ�������ǲ��Ž���ֵ��������int,ֵΪ��ǰ����ռ�ܽ��ȵİٷֱ�*/
	EM_SETTICKET,   /*5�м�������ŵ�ticket����Ӧ��,���͸�Ӧ�õĵ�һ��������Ӧ����ticket����  */
	EM_MTOSNOTIFY  /*6�Զ�������֪ͨ��STB->MOBILE���ص��ĵ�һ��������Ӧ�����ֻ����������ַ���  */
}CSSXEvent_E;

/*
@brief ˦��ģ�鷵��ֵ
*/
typedef enum  
{
    EM_CSSX_SUCCESS = 0, 
    EM_CSSX_FAILURE,
    EM_CSSX_TIMEOUT,
    EM_CSSX_INVALPARA,
    EM_CSSX_SOCKET_ERROR 
} CSSXRet_E;

/*
@brief ˦��ģ��ʹ�õĴ����豸
*/
typedef enum  
{
    EM_CSSX_TANSDEVTYPE_UNDEF = 0, 
    EM_CSSX_TANSDEVTYPE_BLUETOOTH ,
    EM_CSSX_TANSDEVTYPE_ETHERNET ,
    EM_CSSX_TANSDEVTYPE_COM
} CSSXTransDevType_E;


/**************************************************************************************************************
@brief 	����˦������URL

@param[char*] pcURL: ˦������URL, pcURL��������'\0'��β���ַ���, ���ַ����м䲻�ܺ���'\0\.��pcURL�ĳ�����
					 С��(1024)�� �����ܽ�˦������URL��ȷ���õ�Cģ������

@return �ɹ�����TRUE;ʧ�ܷ���FALSE
@note 
-- 
**************************************************************************************************************/
BOOL CSSXSetSaitionBuildURL(char *pcURL);

/**************************************************************************************************************
@brief 	��ȡ˦������URL

@param ��

@return ��ȡ˦������URL
@note 
-- 
**************************************************************************************************************/
char* CSSXGetSaitionBuildURL();

/**************************************************************************************************************
@brief 	��ȡ˦������URL

@param ��

@return ����˦�����ĵ�URL
@note 
-- 
**************************************************************************************************************/
char* CSSXGetHomeURL();

/**************************************************************************************************************
@brief 	����˦������URL

@param[char*] pcURL: ˦������URL, pcURL��������'\0'��β���ַ���, ���ַ����м䲻�ܺ���'\0\.��pcURL�ĳ�����
					 С��(1024)�� �����ܽ�˦������URL��ȷ���õ�Cģ������

@return �ɹ�����TRUE;ʧ�ܷ���FALSE
@note 
-- 
**************************************************************************************************************/
BOOL CSSXSetHomeURL(char *pcURL);

/**************************************************************************************************************
@brief 	��ȡ�Զ�������֪ͨ

@param ��

@return �����Զ�������֪ͨ������
@note 
-- 
**************************************************************************************************************/
char* CSSXGetNotifyContent();

/**************************************************************************************************************
@brief 	�����Զ�������֪ͨ

@param[char*] pcNotifyContent: �Զ�������֪ͨ����, pcNotifyContent��������'\0'��β���ַ���, ���ַ����м䲻�ܺ���'\0\.
					 ��pcNotifyContent�ĳ�����С��(2*1024)�� �����ܽ�˦������URL��ȷ���õ�Cģ������

@return �ɹ�����TRUE;ʧ�ܷ���FALSE
@note 
-- 
**************************************************************************************************************/
BOOL CSSXSetNotifyContent(char *pcNotifyContent);

/**************************************************************************************************************
@brief 	����˦��ģ��ʹ�õĴ����豸����������Ethernet��

@param[in] eDeviceType: ˦��ģ��ʹ�õĴ����豸,���嶨���CSSXTransDevType_E

@return 
@note 
-- �ýӿڱ�����CSSXInit֮ǰ����
- ����û�û�е��ô˽ӿ����ã�Ĭ�ϲ���EM_CSSX_TANSDEVTYPE_ETHERNET��ʽ���д���
**************************************************************************************************************/
void CSSXSetTransDevice(CSSXTransDevType_E eDeviceType);

/**************************************************************************************************************
@brief ��ȡ˦��ģ��ʹ�õĴ����豸����������Ethernet��

@param

@return ˦��ģ��ʹ�õĴ����豸
@note 
**************************************************************************************************************/
CSSXTransDevType_E CSSXGetTransDevice(void);


/**************************************************************************************************************
@brief 	����˦�ŷ����

@param[in] nPort: ˦�ŷ���˶˿ڣ��Ƿ���Ҫ������?

@return �ɹ�EM_CSSX_SUCCESS��ʧ��EM_CSSX_FAILURE
@note 
- �����Ҫ����˦�ŷ��񣬵��ô˺����Ϳ����ˡ������ڲ�����˦�ſͻ��˺ͻ�����֮������ݴ���ͽ���
**************************************************************************************************************/
int CSSXInit(unsigned int nPort);

/**************************************************************************************************************
@brief 	ж��˦��ģ��ʱ�ͷų���Դ

@param[in] ��

@return �ɹ�EM_CSSX_SUCCESS��ʧ��EM_CSSX_FAILURE
@note 
- ���˦��ģ���������ɹ������Ե��ô˽ӿ��ͷ���Դ
**************************************************************************************************************/
int CSSXUnInit();

/**************************************************************************************************************
@brief ����CSSXAddCallback��CSSXRemoveCallback��ʱ��, ��Ҫ��������͵ĺ���ָ��
@param[in] dwUserData:���������û�����
@param[in] eMsg:��ʾ��ǰ�յ�����Ϣ����
@param[in] dwPara1: ����1
@param[in] dwPara2: ����2
**************************************************************************************************************/
typedef void (*CSOnSXEventCallBack_p)(DWORD dwUserData, CSSXEvent_E eMsg, void *pvPara1, void *pvPara2);



/*************************************************************************
@brief  ��˦��ģ��ע��һ��callback

@param[in] dwUserData: �����û�������Ϣ
@param[in] Callback:  Ҫ���ӵĻص�	
*************************************************************************/
BOOL CSSXAddCallback(DWORD dwUserData, CSOnSXEventCallBack_p fnOnSXEventCallBack);


/*************************************************************************
@brief  ��˦��ģ���Ƶ�һ��callback

@param[in] dwUserData: �����û�������Ϣ
@param[in] Callback:  Ҫɾ���Ļص�
*************************************************************************/
BOOL CSSXRemoveCallback(DWORD dwUserData, CSOnSXEventCallBack_p fnOnSXEventCallBack);

#ifdef __cplusplus
}
#endif
/** @} */
#endif  
