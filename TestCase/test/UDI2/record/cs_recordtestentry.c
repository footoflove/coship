#include "cs_testkit.h"
#include "cs_recordtestentry.h"
#include "cs_recordtestcase.h"

static CSTestInit_S s_sTestInit = {CSTC_RECORD_Init, CSTC_RECORD_UnInit};
static CSTestCase_S s_asTestCase[] = 
{
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_AddDelCallback_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_AddDelCallback_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_AddDelCallback_0003)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_AddDelCallback_0004)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_AddDelCallback_0005)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_OpenClose_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_OpenClose_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_OpenClose_0003)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_OpenClose_0004)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_StartStop_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_StartStop_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_StartStop_0003)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_GetSupportEncryptTypes_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_GetSupportEncryptTypes_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_GetRecTime_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_GetRecTime_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_GetRecTime_0003)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_FileCut_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_FileCut_0002)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_ChnlRec_0001)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_ChnlRec_0002)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TShiftRec_0001)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TShiftRec_0002)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TShiftRec_0003)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TShiftRec_0004)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TSRec_0001)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_TSRec_0002)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_Play_0001)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_Play_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_Play_0003)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_Play_0004)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_Play_0005)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_Play_0006)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_Play_0007)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_Play_0008)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_Play_0009)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0001)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_0002)},
	{CS_TK_AUTOCASE(CSTC_RECORD_IT_0003)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0004)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0005)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0006)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0007)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0008)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0009)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0010)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0011)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0012)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0013)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0014)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0015)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0016)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0017)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0018)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0019)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0020)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0021)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0022)},
	{CS_TK_MANUALCASE(CSTC_RECORD_IT_0023)},
	CS_TK_ENDCASE //��������������ǣ�����Ҫ�У��������һ��
};


static CSTestGetObj_F s_afnTestGetObj[]= 
{
	CSRECORDTESTGetObj //��Ӧ�Լ��������ǵ�һ��������Ҫ��
};

CS_TEST_BUILD_FRAME_CODE(RECORD)
