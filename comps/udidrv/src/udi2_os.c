/****************************************************************************/
/*                   Entropic (Shanghai) Co, LTD                            */
/*                        SOFTWARE FILE/MODULE HEADER                       */
/*                 Copyright Entropic Co, LTD                               */
/*                            All Rights Reserved                           */
/****************************************************************************/
/*
 * Filename:        udi2_os.c
 *
 *
 * Description:     API implementation for COSHIP interface layer .
 *
 *
 *-------------------------------------------------------------------------------
 *ENTROPIC COMMENTS ON COSHIP HEADER FILE:
	 2013/11/06
		 The APIs in this header file are required for Android DVB-S2 plus OTT project.
		 Because there is no EEPROM device in the STB system.
 *-------------------------------------------------------------------------------
 ****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>

#include <errno.h>
#include <sched.h> 
#include <pthread.h>
#include <semaphore.h>

#include <signal.h>
#include <time.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <linux/msg.h>
#include <linux/sem.h>
#include <sys/timeb.h>
#include <sys/time.h>


#include "udi2_error.h"
#include "udi2_public.h"
#include "udi2_typedef.h"
#include "udidrv_log.h"

#include "udiplus_debug.h"
#include "udiplus_typedef.h"
#include "udiplus_error.h"

#include "udi2_os.h"


#define CSSTD_UNUSED(x) ((void)x)

typedef struct timeval cs_time_t;

#define TASK_PRIOR_STEP		17

/*The task priority are mapped to POSIX*/

#define POSIX_TASK_LOWEST            1
#define POSIX_TASK_LOW                  20
#define POSIX_TASK_NORMAL            40
#define POSIX_TASK_HIGH                 70
#define POSIX_TASK_HIGHEST           90


/*NPTL需要额外的8K大小作为堆栈的描述符存放区和内存保护区*/
#define NPTL_ADDED_STACK_SIZE	(8*1024)


/*for print module name*/
#define  MODULE_NAME   "CS_OS"


//vivian, can not find these definition in android toolchain
/* Cancellation */
enum
{
  PTHREAD_CANCEL_ENABLE,
#define PTHREAD_CANCEL_ENABLE   PTHREAD_CANCEL_ENABLE
  PTHREAD_CANCEL_DISABLE
#define PTHREAD_CANCEL_DISABLE  PTHREAD_CANCEL_DISABLE
};
enum
{
  PTHREAD_CANCEL_DEFERRED,
#define PTHREAD_CANCEL_DEFERRED	PTHREAD_CANCEL_DEFERRED
  PTHREAD_CANCEL_ASYNCHRONOUS
#define PTHREAD_CANCEL_ASYNCHRONOUS	PTHREAD_CANCEL_ASYNCHRONOUS
};

int sem_timedwait(sem_t *men, const struct timespec * abs_timeout);


typedef struct
{
    CSUDI_HANDLE semaphore;
    DWORD owner;
    int count;
}cs_mutex_t;



static void adjusttimespec(struct timespec* ts)
{
    CSASSERT(NULL != ts);

    if(NULL != ts)
    {
        /* 1000000000*ns = 1s */
        if(ts->tv_nsec > 1000000000)
        {
            ts->tv_nsec -= 1000000000;
            ++ts->tv_sec;
        }
    }

    return;
}


typedef struct CSThreadInfo_{
	pthread_t 	stThreadHndl;
	int			nPThreadPrio;
	CSUDIOSThreadEntry_F fnThreadEntry;
	void * pvArg;
	struct CSThreadInfo_ *pstNext;
}CSThreadInfo;



static CSThreadInfo *g_pstThreadListHead = NULL;
static CSUDI_HANDLE g_hThreadLock = NULL;

static BOOL ThreadInfoLock(void)
{
	if (g_hThreadLock == NULL)
	{
		if (CSUDI_SUCCESS != CSUDIOSMutexCreate("ThreadInfo", 0, &g_hThreadLock))
		{
			return FALSE;
		}
	}

	if (CSUDI_SUCCESS != CSUDIOSMutexWait(g_hThreadLock,CSUDIOS_TIMEOUT_INFINITY))
	{
		return FALSE;
	}

	return TRUE;
}


static BOOL ThreadInfoUnLock(void)
{
	if (g_hThreadLock == NULL)
	{
		CSDEBUG(MODULE_NAME,ERROR_LEVEL,"error:should lock fist before release lock\n");
		return FALSE;
	}

	if (CSUDI_SUCCESS != CSUDIOSMutexRelease(g_hThreadLock))
	{
		return FALSE;
	}

	return TRUE;
}



static void RemoveThreadInfo(CSThreadInfo *pstThreadInfo)
{
	CSThreadInfo *pstTmp;
		
	/*here thead life is over ,clean resource*/
	pstTmp = g_pstThreadListHead;

	/*the first thread node is the current thread*/
	if (pstTmp == pstThreadInfo)
	{
		g_pstThreadListHead = pstThreadInfo->pstNext;
	}
	else
	{
		while(pstTmp)
		{
			if (pstTmp->pstNext == pstThreadInfo)
			{
				break;
			}

			pstTmp = pstTmp->pstNext;
		}

		if (pstTmp)
		{
			pstTmp->pstNext = pstThreadInfo->pstNext;
		}
		else
		{
			CSDEBUG(MODULE_NAME,ERROR_LEVEL,"Error: can't find the thead info in list !!!\n");
		}
	}
	//CSDEBUG(MODULE_NAME,ERROR_LEVEL,"free thread info\n");
	CSUDIOSFree(pstThreadInfo);
}


static void PThreadEntryProxy(void * pvParam)
{
	CSThreadInfo *pstThreadInfo = (CSThreadInfo*)pvParam;

	if (pvParam == NULL)
	{
		CSDEBUG(MODULE_NAME,ERROR_LEVEL,"Error thread entry!!!\n");
		return ;
	}

	if (pstThreadInfo->fnThreadEntry == NULL)
	{
		CSDEBUG(MODULE_NAME,ERROR_LEVEL,"Error thread info!!!\n");
		return ;
	}
	
	pstThreadInfo->fnThreadEntry(pstThreadInfo->pvArg);
	
	ThreadInfoLock();
	RemoveThreadInfo(pstThreadInfo);
	ThreadInfoUnLock();
}



/**
@brief 创建任务

*创建一个任务,任务间共享逻辑地址,相当于线程的概念。
@param[in] pcName 任务名称,长度不能超过32字节,可以为CSUDI_NULL，如果长度超过32个字节则返回CSUDIOS_ERROR_BAD_PARAMETER，并且任务创建失败
@param[in] nPriority 任务的优先级,范围为0-255;0优先级最低,255优先级最高。如果超过最大优先级则返回CSUDIOS_ERROR_BAD_PARAMETER,并且任务创建失败
@param[in] nStackSize 任务堆栈大小,至少支持512KB的大小。如果操作系统支持自动大小任务堆栈管理,建议实现忽略该参数,但使用者必须给出该参数以适应不支持自动栈大小的系统
@param[in] fnThreadEntry 任务的入口函数
@param[in] pvArg 任务的入口参数
@param[out] phThread 生成的任务句柄,后续操作使用该句柄
@return 成功返回CSUDI_SUCCESS;如果失败返回错误代码值，且phThread为CSUDI_NULL
@note 
- pcName并不影响创建任务成功与否,即可以为CSUDI_NULL,可以重复,但不推荐这样用
- 后续对该任务的所有操作均使用phThread
- 线程名称应允许空格

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSThreadCreate(const char * pcName,int nPriority,int nStackSize, CSUDIOSThreadEntry_F fnThreadEntry,void * pvArg,CSUDI_HANDLE * phThread)
{
	
	CSUDI_Error_Code 			enRet = CSUDI_FAILURE;
	unsigned int 				dwNameLen = 0;
	unsigned int 				stacksize = 0;
	int 						nRetVal;
	int						nNewPrio;
	int						nTempPrio = nPriority/TASK_PRIOR_STEP;
	struct sched_param 		param;
	pthread_t 				thread;
	pthread_attr_t  			attr;
	CSThreadInfo 				*pstThreadInfo;
	
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);
	
	if (phThread == CSUDI_NULL || fnThreadEntry == CSUDI_NULL)
	{
		CSASSERT(phThread != CSUDI_NULL);
		CSASSERT(fnThreadEntry != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	if (nStackSize < 0 || nPriority < 0 || nPriority > 255)
	{
		CSASSERT((nStackSize > 0) && (nPriority >= 0 && nPriority <= 255));
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	
	CSASSERT(pcName != NULL);

	if(pcName != NULL)
	{
	    dwNameLen = strlen(pcName);
	}

	if (dwNameLen > 32)
	{
		CSASSERT(dwNameLen < 32);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	pstThreadInfo = CSUDIOSMalloc(sizeof(CSThreadInfo));
	if (pstThreadInfo == NULL)
	{
		return CSUDIOS_ERROR_NO_MEMORY;
	}
	
	*phThread = CSUDI_NULL;
        if(nTempPrio < 4)
        {
            nNewPrio = POSIX_TASK_LOWEST;
        }
        else if(nTempPrio  < 8)
        {
            nNewPrio = POSIX_TASK_LOW;
        }
        else if(nTempPrio  < 12)
        {
            nNewPrio = POSIX_TASK_NORMAL;
        }
        else if(nTempPrio  < 14)
        {
            nNewPrio = POSIX_TASK_HIGH;
        }
        else
        {
            nNewPrio = POSIX_TASK_HIGHEST;
        }
        
        /*
        	typedef struct __pthread_attr_s
        	{
        		  int __detachstate;
        		  int __schedpolicy;
        		  struct __sched_param __schedparam;
        		  int __inheritsched;
        		  int __scope;
        		  size_t __guardsize;
        		  int __stackaddr_set;
        		  void *__stackaddr;
        		  size_t __stacksize;
        	} pthread_attr_t;
        */
        nRetVal = pthread_attr_init(&attr);
        
#if 1
        CSASSERT(nRetVal == 0);
        if(nStackSize<0x4000)
        {
            nStackSize = 0x4000;
        }
        stacksize = (DWORD)nStackSize+NPTL_ADDED_STACK_SIZE;
        nRetVal = pthread_attr_setstacksize(&attr,stacksize);
        CSASSERT(nRetVal == 0);

				stacksize = 0;
        nRetVal = pthread_attr_getstacksize(&attr,(size_t *)&stacksize);
        CSASSERT(nRetVal == 0);

				pstThreadInfo->fnThreadEntry = fnThreadEntry;
				pstThreadInfo->pvArg = pvArg;
				pstThreadInfo->nPThreadPrio = nNewPrio;

				ThreadInfoLock();
				if (g_pstThreadListHead == NULL)
				{
					g_pstThreadListHead = pstThreadInfo;
					pstThreadInfo->pstNext = NULL;
				}
				else
				{
					pstThreadInfo->pstNext = g_pstThreadListHead;
					g_pstThreadListHead = pstThreadInfo;
				}
				ThreadInfoUnLock();
#endif

		 		nRetVal = pthread_create (&thread, &attr, (void *(*)(void *))PThreadEntryProxy, pstThreadInfo);
        CSASSERT(nRetVal == 0);

	 			pstThreadInfo->stThreadHndl = thread;
	//vivian
        //nRetVal = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE,NULL);
        //CSASSERT(nRetVal == 0);

        //nRetVal = pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS,NULL);
        //CSASSERT(nRetVal == 0);
        
        if (nRetVal == 0 && thread != 0)
        {
            /*use default shecdule no task priority*/
#if 1
#ifdef USE_OTHER_SCHED
						int nPolicy = SCHED_OTHER;
#else
						int nPolicy = SCHED_RR;
#endif     
						nPolicy = SCHED_RR;
            param.sched_priority = nNewPrio;
            nRetVal = pthread_setschedparam (thread, nPolicy, &param);
            CSASSERT(nRetVal == 0);
						//printf("create %x thread ret:%d===\n",thread,nRetVal);
#endif

            *phThread = (CSUDI_HANDLE)thread;

						enRet = CSUDI_SUCCESS ;
				}

				nRetVal = pthread_attr_destroy(&attr);
				CSASSERT(nRetVal >= 0);

				//CSDEBUG(MODULE_NAME,INFO_LEVEL, "[CS_OS] Warnning: nStackSize can not be set!!.\n",nStackSize);
				//CSDEBUG(MODULE_NAME,INFO_LEVEL, "[CS_OS] CSCreateThread(%s,%d,0x%08x)\n",pstrName, nPriority,nStackSize);
	
				UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, enRet);    
				return enRet;
				
}

/**
@brief 删除任务

通过句柄删除一个已经创建的任务
@param[in] hThread 已创建的任务句柄，即CSUDIOSThreadCreate的输出参数phThread
@return 如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值;当hThread无效时,删除失败
@note 
- 要求任务入口函数在完成任务后自行返回，然后再调用本接口销毁以释放资源
- 不允许在本任务中销毁本任务
- 试图去销毁一个正在运行的任务（即任务入口函数还没有返回的任务）时，会返回CSUDIOS_ERROR_TASK_BUSY。
@see  CSUDIOSThreadJoin

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSThreadDestroy (CSUDI_HANDLE hThread)
{
    	CSUDI_Error_Code enRet = CSUDI_FAILURE;
	int nRetVal;
	CSThreadInfo *pstTmp;

    	CSASSERT (hThread != CSUDI_NULL);

	if (hThread == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	nRetVal = pthread_cancel ((pthread_t)hThread);         /* Send cancel request   */
	CSASSERT(nRetVal == 0);

	nRetVal = pthread_join ((pthread_t) hThread, CSUDI_NULL);    /* Wait for task to exit */
	CSASSERT(nRetVal == 0);

	ThreadInfoLock();
	pstTmp = g_pstThreadListHead;
	while(pstTmp)
	{
		if (pstTmp->stThreadHndl == hThread)
		{
			break;
		}
		pstTmp=pstTmp->pstNext;
	}
	if (pstTmp)
	{
		RemoveThreadInfo(pstTmp);
	}
	else
	{
		CSDEBUG(MODULE_NAME,ERROR_LEVEL,"WARNING:can't find thread info,maybe has been free when thread exit\n");
	}
	
	ThreadInfoUnLock();
	
	enRet = CSUDI_SUCCESS;

    return enRet;
}

/**
@brief 挂起任务

通过句柄挂起一个正在运行的任务，使其停止运行
@param[in] hThread 已创建的任务句柄，即CSUDIOSThreadCreate的输出参数phThread
@return 如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值;当hThread无效时,任务挂起失败
@note 
- 挂起一个已经挂起的任务仍返回成功
- 挂起一个已经自行返回的任务返回失败
@deprecated 此接口仅用于调试，如果平台不支持，可不实现，此时此接口返回CSUDIOS_ERROR_FEATURE_NOT_SUPPORTED

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSThreadSuspend(CSUDI_HANDLE hThread)
{
#if 0
	CSUDI_Error_Code enRet = CSUDI_FAILURE;

	CSASSERT (hThread != NULL);

  	if (hThread == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	if (pthread_kill((pthread_t)hThread, SIGSTOP) == 0)
	{
	    enRet = CSUDI_SUCCESS;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
#else
	return CSUDIOS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}

/**
@brief 唤醒已挂起任务

通过句柄唤醒一个已挂起的任务，使其恢复运行
@param[in] hThread 已创建的任务句柄，即CSUDIOSThreadCreate的输出参数phThread
@return 如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值;当hThread无效时，唤醒任务失败
@note 
- 唤醒一个正在运行的任务仍返回成功
- 唤醒一个已经自行返回的任务返回失败
@deprecated 此接口仅用于调试，如果平台不支持，可不实现，此时此接口返回CSUDIOS_ERROR_FEATURE_NOT_SUPPORTED

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSThreadResume(CSUDI_HANDLE hThread)
{
#if 0
	CSUDI_Error_Code enRet = CSUDI_FAILURE;

	CSASSERT (hThread != NULL);

  	if (hThread == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	if (pthread_kill((pthread_t)hThread,SIGCONT) == 0)
	{
	    enRet = CSUDI_SUCCESS;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
#else
	return CSUDIOS_ERROR_FEATURE_NOT_SUPPORTED;
#endif
}
/**
@brief 使一个任务等待另一个任务结束

@param[in] hThread 已创建的任务句柄，即CSUDIOSThreadCreate的输出参数phThread
@return 如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值
@note 
- 当被等待任务返回时，该接口返回CSUDI_SUCCESS。
- 等待一个已经返回但还没有被销毁的任务时，该接口返回CSUDI_SUCCESS。
- 当多个任务同时调用此接口去等待一个任务时，其结果是未定义的。
- 不允许等待一个已经被销毁的任务。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSThreadJoin (CSUDI_HANDLE hThread)
{
	CSUDI_Error_Code enRet = CSUDI_FAILURE;

	CSASSERT (hThread != NULL);

  	if (hThread == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	if (pthread_join((pthread_t)hThread,(void**)CSUDI_NULL) == 0)
	{
		enRet = CSUDI_SUCCESS;
	}

	return enRet;
}

/**
@brief 获取当前任务的句柄
@param[out] phThread 当前的任务的句柄，即CSUDIOSThreadCreate的输出参数phThread
@return  如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值;

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code  CSUDIOSThreadSelf(CSUDI_HANDLE * phThread)
{
	CSUDI_Error_Code enRet = CSUDI_FAILURE;

	CSASSERT (phThread != NULL);

  	if (phThread == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	if (CSUDI_NULL != (*phThread = (CSUDI_HANDLE)pthread_self()))
	{
		enRet = CSUDI_SUCCESS;
	}

	return enRet;
}


static DWORD CSGetThreadId(VOID)
{
    return (DWORD)pthread_self();
}


/**
@brief 挂起当前任务一段时间

@param[in] uMilliSeconds 挂起的时间，单位是毫秒
@note 
该接口强制当前任务挂起至少uMilliSeconds毫秒，如果指定挂起的时间uMilliSeconds为0毫秒时，则由平台决定是以下哪种行为：
- 放弃本次执行权。
- 不关注该值。 

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
void CSUDIOSThreadSleep(unsigned int uMilliSeconds)
{
    struct timespec delay;
    struct timespec rem;
    int rc;

    if(uMilliSeconds==0)
    {
        return;
    }

    delay.tv_sec = (int)uMilliSeconds/1000;
    delay.tv_nsec = 1000 * 1000 * (uMilliSeconds%1000);

    for(;;) {
        rc = nanosleep(&delay, &rem); /* [u]sleep can't be used because it uses SIGALRM */
        if (rc!=0) {
            if (errno==EINTR) {
                delay = rem; /* sleep again */
                continue;
            }

            CSASSERT(0);

            return ;
        }
        break; /* done */
    }

    return;
}

/**
@brief 使任务放弃本次执行的机会

即放弃本次执行权。
@note 
- 该接口调用后立即生效并立即返回。
- 在多任务环境(程序)中，为了防止某任务独占CPU资源(此时其它任务无法得到响应)，可以让当前执行的任务放弃本次执行的机会。
- 调用该接口并不能保证下一个运行的任务不是该任务。即如果该接口返回时，该任务再次获得执行权，那么被该接口暂停的任务会马上开始运行。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
void CSUDIOSThreadYield (void)
{
	#if 0
	pthread_yield();
	#else
	CSUDIOSThreadSleep(3);
	#endif
}

/*******************************************************************
 **                    Message Queue definitions                  **
 *******************************************************************/
/**
@brief 创建消息队列

@param[in] pcName 消息队列名称,长度不能超过32个字节，可以为CSUDI_NULL；如果长度超过32个字节则返回CSUDIOS_ERROR_BAD_PARAMETER，并且消息队列创建失败
@param[in] nMaxMsgs 消息队列最大容许的消息个数，一般来说不会超过128个。至少容许一个消息，否则消息队列创建失败
@param[in] nSizePerMsg 每一个消息的最大长度,单位字节，一般来说不会超过128字节。至少支持一个字节，否则消息队列创建失败
@param[out] phMsgQueue 生成的消息队列句柄
@return 成功返回CSUDI_SUCCESS;失败返回错误代码值，且phMsgQue为CSUDI_NULL；
@note 
- pcName并不影响创建消息队列成功与否,即可以为CSUDI_NULL,可以重复,但不推荐这样用
- 虽然单个消息队列的最大消息数量及消息长度不会太大，但仍建议采用限制所有消息队列总内存或者直接使用系统堆内存的方式限制消息队列的数量
@note 

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/



typedef struct
{
    DWORD  Name;
    DWORD  ByteQueueSize;                   /* Size of the queue in bytes   */
    DWORD  ByteNodeSize;                   	/* Message size in bytes        */
    DWORD  MsgCount;
    DWORD  Head;                      		 /* Head offset from queue start */
    DWORD  Tail;                       		/* Tail offset from queue start */
    CSUDI_HANDLE QMutex;              	 /* Queue mutex                  */
    CSUDI_HANDLE QEvent;               	/* Queue event                  */
    CSUDI_HANDLE QEventAvailSpace;/* Queue event                  */
    BYTE  *StartPtr;                 		 /* Pointer to queue start       */
    BOOL	m_bIsRun;
}MSG_QUEUE;



CSUDI_Error_Code CSUDIOSMsgQueueCreate(const char * pcName,int nMaxMsgs,int nSizePerMsg,CSUDI_HANDLE * phMsgQueue)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	CSUDI_HANDLE 		Qmutex = (CSUDI_HANDLE)CSUDI_NULL;
	CSUDI_HANDLE 		Qevent = (CSUDI_HANDLE)CSUDI_NULL;
	CSUDI_HANDLE 		QEventAvailSpace = (CSUDI_HANDLE)CSUDI_NULL;
	MSG_QUEUE*			Qptr = CSUDI_NULL;
	DWORD 				nNameLen = 0;

	CSASSERT(nSizePerMsg > 0);

	if(pcName != CSUDI_NULL)
	{
	    nNameLen = strlen(pcName);
	}

	CSASSERT(nNameLen < 32);

	if((nSizePerMsg <= 0) || (nNameLen >= 32) || (nMaxMsgs <= 0) || (phMsgQueue == CSUDI_NULL))
	{
	    return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	*phMsgQueue = CSUDI_NULL;
    /*================================================
     * Create memory, mutex and event for the queue
     *===============================================*/
    Qptr = (MSG_QUEUE *) malloc ( ( sizeof ( MSG_QUEUE ) + (DWORD)( nMaxMsgs * ( (( nSizePerMsg + 3 ) / 4) * 4 ) ) ) * 2 ) ;

    if ( Qptr != CSUDI_NULL)
    {
        enRet = CSUDIOSMutexCreate("MsgQueueMutex", 0,&Qmutex);

        if ( enRet == CSUDI_SUCCESS)
        {
            enRet = CSUDIOSEventCreate(0,0 ,&Qevent);

            if ( enRet == CSUDI_SUCCESS)
            {
                enRet = CSUDIOSEventCreate( 0, CSUDIOS_EVENT_INITIAL_STATUS,&QEventAvailSpace);

                if ( enRet == CSUDI_SUCCESS)
                {
                    /*==========================================
                    * Initialize the queue
                    *=========================================*/
                    enRet = CSUDIOSMutexWait(Qmutex, CSUDIOS_TIMEOUT_INFINITY);
					
                    if(enRet == CSUDI_SUCCESS)
                    {
                        Qptr->Name				= 0;//pstrName;
                        Qptr->ByteQueueSize		= (DWORD)(nMaxMsgs * nSizePerMsg * 2);
                        Qptr->ByteNodeSize		= (DWORD)(nSizePerMsg * 2);
                        Qptr->MsgCount 			= 0;
                        Qptr->Head      		= 0;
                        Qptr->Tail      		= 0;
                        Qptr->QMutex    		= Qmutex;
                        Qptr->QEvent    		= Qevent;
                        Qptr->QEventAvailSpace  = QEventAvailSpace;
                        Qptr->StartPtr  		= (BYTE *) ( Qptr) + sizeof ( MSG_QUEUE ) * 2;
                        Qptr->m_bIsRun			= TRUE;

                        enRet = CSUDIOSMutexRelease(Qmutex);
                        if(enRet == CSUDI_SUCCESS)
                        {
                            *phMsgQueue = (CSUDI_HANDLE)Qptr;
				enRet = CSUDI_SUCCESS;
                        }
                    }
                }
            }
        }

        if ( *phMsgQueue == CSUDI_NULL )
        {
            CSASSERT(*phMsgQueue != CSUDI_NULL);
            if ( Qevent != CSUDI_NULL )
            {
                enRet = CSUDIOSEventDestroy( Qevent );
                if(enRet != CSUDI_SUCCESS)
                {
                    CSDEBUG(MODULE_NAME,INFO_LEVEL,"CSDestroyEventdwRet=%d\r\n",enRet);
                }
            }

            if ( Qmutex != (CSUDI_HANDLE)CSUDI_NULL )
            {
                enRet = CSUDIOSMutexDestroy(Qmutex);
                if(enRet != CSUDI_SUCCESS)
                {
                    CSDEBUG(MODULE_NAME,INFO_LEVEL,"CSDestroyMutex=%d\r\n",enRet);
                }
            }

            if(CSUDI_NULL != Qptr)
            {
                free (Qptr);
            }
        }
    }

    return enRet ;
}

/**
@brief 销毁消息队列

通过句柄销毁一个消息队列
@param[in] hMsgQueue 消息队列句柄，即CSUDIOSMsgQueueCreate的输出参数phMsgQueue。
@return 成功返回CSUDI_SUCCESS;失败返回错误代码值
@warning 删除一个正在被等待或者发送消息的消息队列,结果是不确定的,请慎用该接口

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMsgQueueDestroy(CSUDI_HANDLE hMsgQueue)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	CSUDI_HANDLE 			Qmutex;
	MSG_QUEUE   			*Qptr;

	if (hMsgQueue == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}        

	Qptr = (MSG_QUEUE *) hMsgQueue;
	Qmutex = Qptr->QMutex;

	if ( Qmutex != 0 )
	{
	    /*=============================================
	    * Delete queue event, mutex and memory
	    *============================================*/
	    enRet = CSUDIOSMutexWait( Qmutex, CSUDIOS_TIMEOUT_INFINITY );
	    if(enRet == CSUDI_SUCCESS)
	    {
	        if ( Qptr->m_bIsRun == TRUE )
	        {
	            Qptr->m_bIsRun = FALSE;
				
	            enRet = CSUDIOSEventDestroy(Qptr->QEvent);
	            enRet = CSUDIOSEventDestroy(Qptr->QEventAvailSpace);
	            enRet = CSUDIOSMutexRelease (Qmutex);				
	            enRet = CSUDIOSMutexDestroy (Qmutex);

	            Qptr->QMutex = 0;
	            Qptr->QEvent = 0;
	            Qptr->QEventAvailSpace = 0;

	            free (Qptr);

	            enRet = CSUDI_SUCCESS;
	        }
	    }
	}

    CSASSERT(enRet == CSUDI_SUCCESS);

    return enRet;
}

/**
@brief 从消息队列接收一条消息

从消息队列里接收一条消息，如果消息队列为空则等待直到消息队列中有消息或者超时为止
@param[in] hMsgQueue 消息队列句柄，即CSUDIMsgQueueCreate的输出参数phMsgQueue
@param[out] pvMsg 接收消息数据的内存地址
@param[in] nMaxMsgBytes 地址分配的内存长度
@param[in] uTimeout 等待超时时间，毫秒单位，也可选用:
- CSUDIOS_TIMEOUT_IMMEDIATE	不等待，马上超时
- CSUDIOS_TIMEOUT_INFINITY	永远不超时
@return 成功返回CSUDI_SUCCESS;超时返回CSUDIOS_ERROR_TIMEOUT;失败返回错误代码
@note 
- 当hMsgQueue无效时,或pvMsg无效,或nMaxMsgBytes==0时,返回失败,同时不从消息队列中读取消息
- 当nMaxMsgBytes小于消息长度时,取有效部分消息数据，同时返回成功，消息从队列中取走
- 当nMaxMsgBytes大于消息长度时，消息可完全放入pvMsg,但超出消息长度后部分内容不确定

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMsgQueueReceive(CSUDI_HANDLE hMsgQueue,void * pvMsg,int nMaxMsgBytes,unsigned int uTimeout)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	CSUDI_HANDLE Qmutex;
	MSG_QUEUE   *Qptr;
	DWORD      Head = 0;//, Tail;

	CSASSERT(hMsgQueue != NULL && pvMsg != NULL && nMaxMsgBytes > 0);

	if (hMsgQueue == NULL || pvMsg == NULL || nMaxMsgBytes <= 0)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

        Qptr = (MSG_QUEUE *) hMsgQueue;
        Qmutex = Qptr->QMutex;

        enRet = CSUDIOSEventWait(Qptr->QEvent, uTimeout);

        if ( enRet == CSUDI_SUCCESS)
        {
            enRet = CSUDIOSMutexWait( Qmutex, CSUDIOS_TIMEOUT_INFINITY );

            if ( enRet == CSUDI_SUCCESS )
            {
                Head = Qptr->Head;
                //Tail = Qptr->Tail;

                if ( Qptr->MsgCount > 0 )
                {
                    DWORD dwCopySize = (DWORD)nMaxMsgBytes > ( Qptr->ByteNodeSize / 2 ) ? ( Qptr->ByteNodeSize / 2 ) : (DWORD)nMaxMsgBytes;

                    memcpy ( pvMsg, Qptr->StartPtr + Head, dwCopySize );
                    memset( Qptr->StartPtr + Head, 0, dwCopySize );

                    Head += Qptr->ByteNodeSize;

                    if ( Head >= Qptr->ByteQueueSize )
                    {
                        Head = 0;
                    }

                    Qptr->Head = Head;
                    Qptr->MsgCount--;

                    if ( Qptr->MsgCount == 0 )
                    {
                        CSUDIOSEventReset(Qptr->QEvent);
                    }

                    CSUDIOSEventSet( Qptr->QEventAvailSpace );

                    enRet = CSUDI_SUCCESS;
                }

                CSUDIOSMutexRelease( Qmutex );
            }
            else
            {
                CSASSERT( enRet == CSUDI_SUCCESS );
            }
        }
        else if (enRet == CSUDIOS_ERROR_TIMEOUT)
        {
            enRet = CSUDIOS_ERROR_TIMEOUT;
        }

    CSASSERT( enRet == CSUDI_SUCCESS || enRet == CSUDIOS_ERROR_TIMEOUT);

    return enRet;
}

/**
@brief 往消息队列里发送一条消息

如果消息队列已满则等待直到消息队列中有空余位置或者超时为止
@param[in] hMsgQueue 消息队列句柄，即CSUDIOSMsgQueueCreate的输出参数phMsgQueue
@param[in] pvMsg 发送消息数据的内存地址
@param[in] nMsgBytes 地址分配的内存长度
@param[in] uTimeout 等待超时时间，毫秒单位，也可选用:
- CSUDIOS_TIMEOUT_IMMEDIATE	不等待，马上超时
- CSUDIOS_TIMEOUT_INFINITY	永远不超时
@return 成功返回CSUDI_SUCCESS;超时返回CSUDIOS_ERROR_TIMEOUT;失败返回错误代码
@note 
- 当hMsgQueue无效时,或pvMsg无效,或nMsgBytes==0时,返回失败。
- 当nMsgBytes大于最大长度时,取有效部分消息数据，同时返回成功。
- 消息队列以拷贝模式工作，pvMsg在本函数返回后即可释放。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMsgQueueSend(CSUDI_HANDLE hMsgQueue, const void * pvMsg, int nMsgBytes, unsigned int uTimeout)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	CSUDI_HANDLE 			Qmutex;
	MSG_QUEUE   			*Qptr;
	DWORD 				Tail;//Head

	CSASSERT(hMsgQueue != NULL && pvMsg != NULL && nMsgBytes > 0);
	if (hMsgQueue == CSUDI_NULL || pvMsg == CSUDI_NULL || nMsgBytes <= 0 )
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	Qptr = (MSG_QUEUE *) hMsgQueue;
	Qmutex = Qptr->QMutex;

	enRet = CSUDIOSEventWait( Qptr->QEventAvailSpace, uTimeout);

	if ( enRet == CSUDI_SUCCESS)
	{
	    enRet  = CSUDIOSMutexWait( Qmutex, CSUDIOS_TIMEOUT_INFINITY );

	    if ( enRet == CSUDI_SUCCESS)
	    {
	        Tail = Qptr->Tail;

	        if ( Qptr->ByteQueueSize >= ( ( Qptr->MsgCount + 1 ) * Qptr->ByteNodeSize ) )
	        {
	            DWORD dwCopySize = ( (DWORD)nMsgBytes > ( Qptr->ByteNodeSize / 2 ) ? ( Qptr->ByteNodeSize / 2 ) : (DWORD)nMsgBytes );

	            memcpy( Qptr->StartPtr + Tail, pvMsg, dwCopySize );

	            Tail += Qptr->ByteNodeSize;

	            if ( Tail >= Qptr->ByteQueueSize )
	            {
	                Tail = 0;
	            }

	            Qptr->Tail = Tail;
	            Qptr->MsgCount++;

	            if ( Qptr->MsgCount >= ( Qptr->ByteQueueSize/Qptr->ByteNodeSize ) )
	            {
	                CSUDIOSEventReset(Qptr->QEventAvailSpace);
	            }

	            CSUDIOSEventSet( Qptr->QEvent );            /* Set the queue event  */

	            enRet = CSUDI_SUCCESS;
	        }

	        CSUDIOSMutexRelease(Qmutex);
	    }
	    else
	    {
	        CSASSERT( enRet == CSUDI_SUCCESS);
	    }
	}
	else if ( enRet == CSUDIOS_ERROR_TIMEOUT )
	{
	    enRet = CSUDIOS_ERROR_TIMEOUT;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
}


/*******************************************************************
 **                     Semaphore definitions                     **
 *******************************************************************/
/**
@brief 创建信号量

@param[in] pcName 信号量名称,长度不能超过32个字节，如果长度超过32个字节则返回CSUDIOS_ERROR_BAD_PARAMETER，并且信号量创建失败
@param[in] nInitialCount 有效信号数量，相当于PV原语中的初始值
@param[in] nMaxCount 最大信号数量
@param[out] phSemaphore 生成的信号量句柄
@return 如果成功返回CSUDI_SUCCESS;如果失败返回错误代码值，且phSem为CSUDI_NULL
@note 
- pcName并不影响创建信号量成功与否，即可以为CSUDI_NULL，可以重复,但不推荐这样用
- 有多个任务等待同一个信号量时,一旦该信号量被释放,不确定哪个任务可以成功获得信号量
- 信号量名称应允许空格
@todo 目前大多数平台上nMaxCount无效，请不要使用，nMaxCount为不限大小

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSSemCreate(const char * pcName,int nInitialCount,int nMaxCount,CSUDI_HANDLE * phSemaphore)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	DWORD dwNameLen = 0;

	CSASSERT(pstrName != NULL);

	if(pcName != NULL)
	{
	    dwNameLen = strlen(pcName);
	}

	CSASSERT((dwNameLen <= 32) && (nMaxCount >= nInitialCount) && (nInitialCount >= 0) && (nMaxCount > 0));
	if ((dwNameLen <= 32) && (nMaxCount >= nInitialCount) && (nInitialCount >= 0) && (nMaxCount > 0))
	{
	    int nRet = 1;

	    sem_t *sem = NULL;

		*phSemaphore = NULL;

	    sem = ( sem_t *)malloc( sizeof( sem_t ) ); /* allocate Memory  */

	    CSASSERT(sem != NULL);

	    if ( sem != NULL )
	    {
	        /*the semaphore is local to the current process( pshared is zero ) */
	        nRet = sem_init( sem, 0,  (DWORD)nInitialCount );
	        /*return 0 on success and -1 on unknown error */
	        if ( nRet == 0 )
	        {
	            *phSemaphore = (CSUDI_HANDLE)sem;
			enRet = CSUDI_SUCCESS;
	        }
	        else
	        {
	            CSASSERT(0);
	            free( sem ); /* free memory.  */
	        }
	    }
	}
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

    return enRet;
}

/**
@brief 销毁信号量

@param[in] hSemaphore 信号量句柄，即CSUDIOSMsgQueueCreate的输出参数phMsgQueue
@return 成功返回CSUDI_SUCCESS;失败返回出错代码
@warning 销毁一个正在被操作（如等待）的信号量是一个不确定的行为

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSSemDestroy(CSUDI_HANDLE hSemaphore)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

	CSASSERT(hSemaphore != NULL);

	if (hSemaphore != NULL)
	{
	    if (sem_destroy((sem_t *)hSemaphore) == 0)
	    {
	        free((sem_t *)hSemaphore); /* free memory.  */
	        enRet = CSUDI_SUCCESS;
	    }
	}
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
}


/**
@brief 等待信号量，相当于P操作(P是请求资源)

@param[in] hSemaphore 信号量句柄，即CSUDIOSMsgQueueCreate的输出参数phMsgQueue
@param[in] uTimeout 等待超时时间，毫秒单位，也可选用:
- CSUDIOS_TIMEOUT_IMMEDIATE	不等待，马上超时
- CSUDIOS_TIMEOUT_INFINITY	永远不超时
@return 成功返回CSUDI_SUCCESS;超时返回CSUDIOS_ERROR_TIMEOUT;失败返回错误代码

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSSemWait(CSUDI_HANDLE hSemaphore,unsigned int uTimeout)
{
   CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

    CSASSERT(hSemaphore != NULL);

    if (hSemaphore != NULL)
    {
        int nRet;

        if(uTimeout == 0xFFFFFFFF)
        {
            nRet = sem_wait((sem_t *)hSemaphore);
            if (nRet == 0)
            {
                enRet = CSUDI_SUCCESS;
            }
            else
            {
                //printf("Wait(%d)nRet is %d\n", uTimeout, nRet);
                enRet = CSUDI_FAILURE;
            }
        }
        else if (uTimeout == 0)
        {
            nRet = sem_trywait((sem_t *)hSemaphore);
            if (nRet == 0)
            {
                enRet = CSUDI_SUCCESS;
            }
            else if(errno == EAGAIN)
            {
                enRet = CSUDIOS_ERROR_TIMEOUT;
            }
            else
            {
                /*printf("Wait(%d)nRet is %d\n", uTimeout, nRet);*/
                enRet = CSUDI_FAILURE;
            }
        }
        else
        {
            struct timespec ts;
            struct timeval tv;

            nRet = gettimeofday (&tv, NULL);
            CSASSERT(nRet == 0);

            ts.tv_sec = tv.tv_sec + (int)(uTimeout/1000);
            ts.tv_nsec = tv.tv_usec*1000 + (int)(uTimeout%1000)*1000000;

            adjusttimespec(&ts);

            nRet = sem_timedwait((sem_t *)hSemaphore, &ts);

            if (nRet == 0)
            {
                enRet = CSUDI_SUCCESS;
            }
            else if(errno == ETIMEDOUT)
            {
                enRet = CSUDIOS_ERROR_TIMEOUT;
            }
            else
            {
                enRet = CSUDI_FAILURE;
                CSDEBUG("CS_OS",ERROR_LEVEL,"CSWaitForSemaphore.CS_OSP_FAILURE.errno=%d",errno);
            }
        }
    }
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

    return enRet;
}

/**
@brief 释放信号量，相当于V操作(V是释放资源）
@param[in] hSemaphore 信号量句柄，即CSUDIOSMsgQueueCreate的输出参数phMsgQueue
@return 成功返回CSUDI_SUCCESS;失败返回出错代码
@note 当信号量被释放次数达到最大个数时，返回CSUDIOS_ERROR_UNKNOWN_ERROR

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/

CSUDI_Error_Code CSUDIOSSemRelease(CSUDI_HANDLE hSemaphore)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

	CSASSERT(hSemaphore != CSUDI_NULL);

	if (hSemaphore != CSUDI_NULL)
	{
	        if (0 == sem_post((sem_t *)hSemaphore))
	        {
			enRet = CSUDI_SUCCESS;
	        }
	}
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);
	return enRet;
}


/*******************************************************************
 **                     Mutex definitions                         **
 *******************************************************************/
/**
@brief 创建可嵌套的互斥量

@param[in] pcName 互斥量名称,长度不能超过32个字节，如果长度超过32个字节则返回CSUDIOS_ERROR_BAD_PARAMETER，并且信号量创建失败
@param[in] uOptions 互斥量选项,目前支持:(各选项可相或)
- CSUDIOS_MUTEX_OBTAIN 创建互斥量后立即获得该锁(该位为0或CSUDIOS_MUTEX_DEFAULT表示不获得该锁)
@param[out] phMutex 互斥量句柄
@return 成功返回CSUDI_SUCCESS;失败返回出错代码,且phMutex为CSUDI_NULL
@note 
- pcName并不影响创建互斥量成功与否,即可以为CSUDI_NULL,可以重复,但不推荐这样用
- 同一任务在已经对一个互斥量加锁后,可再次对其加锁,即使用嵌套锁,但多次加锁后需要多次释放才能释放该锁。
- 一个任务对一个互斥量加锁后,其它任务对其进行开锁将返回失败。
- 以上两点属于高级特性,不建议程序逻辑依赖之
- 互斥量名称应允许空格
- 有多个任务等待同一个互斥量时,一旦该互斥量被释放,不确定哪个任务可以成功加锁

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMutexCreate(const char * pcName, unsigned int uOptions,CSUDI_HANDLE * phMutex)
{
	cs_mutex_t* 			mutex = NULL;
	DWORD 				dwNameLen = 0;
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

	CSSTD_UNUSED(uOptions);

	if(pcName != CSUDI_NULL)
	{
	    dwNameLen = strlen(pcName);
	}

	if(phMutex == CSUDI_NULL)
	{
		CSASSERT(phMutex != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	if(dwNameLen >= 31)
	{
	    return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	*phMutex = CSUDI_NULL;
	mutex =(cs_mutex_t*) CSUDIOSMalloc(sizeof(cs_mutex_t));

	CSASSERT(mutex);

	if( CSUDI_NULL != mutex )
	{
		CSUDI_HANDLE semaphore;
		enRet = CSUDIOSSemCreate (pcName,1,1,&semaphore) ;
		CSASSERT(semaphore);

		if( enRet == CSUDI_SUCCESS)
		{
			if (uOptions == CSUDIOS_MUTEX_OBTAIN)
			{
				enRet = CSUDIOSSemWait(semaphore,CSUDIOS_TIMEOUT_IMMEDIATE);
				CSASSERT(enRet == CSUDI_SUCCESS);
				if (enRet == CSUDI_SUCCESS)
				{
					mutex->count = 1;
					mutex->owner = CSGetThreadId();
				}
			}
			else
			{
				mutex->count = 0;
				mutex->owner = (DWORD)CSUDI_NULL;
			}
			mutex->semaphore = semaphore;
			*phMutex = mutex;
		}

		if (enRet != CSUDI_SUCCESS)
		{
		        if(mutex)
		        {
		            CSUDIOSFree(mutex);
		            mutex = CSUDI_NULL;
		        }
		        CSDEBUG(MODULE_NAME,ERROR_LEVEL,"[USP_OSP] can't create semaphore for a mutex  !\n");
		}
	}
	else
	{
	    CSDEBUG(MODULE_NAME,ERROR_LEVEL,"[USP_OSP] can't malloc memory for  a mutex  !\n");
	}

    return enRet;
}

/**
@brief 销毁互斥量

@param[in] hMutex 互斥量句柄
@return 成功返回CSUDI_SUCCESS;失败返回出错代码
@warning 销毁一个正在被操作（如等待）的互斥量是一个不确定的行为

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMutexDestroy(CSUDI_HANDLE hMutex)
{
    CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

    if(hMutex)
    {
        cs_mutex_t* mutex = (cs_mutex_t*) hMutex;

        if(CSUDIOSSemDestroy( mutex->semaphore) == CSUDI_SUCCESS)
        {
        	CSUDIOSFree(mutex);
              enRet = CSUDI_SUCCESS;
        }
        //CSDEBUG(MODULE_NAME,INFO_LEVEL,"[CS_OS]: DestroyMutex %xh\r\n", hMutex);
    }
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

    return enRet;
}

/**
@brief 等待互斥量,即加锁操作

@param[in] hMutex 互斥量句柄
@param[in] uTimeout 等待超时时间,毫秒单位,也可选用:
- CSUDIOS_TIMEOUT_IMMEDIATE	不等待,马上超时
- CSUDIOS_TIMEOUT_INFINITY	永远不超时
@return 成功返回CSUDI_SUCCESS;超时返回CSUDIOS_ERROR_TIMEOUT;失败返回错误代码。 

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMutexWait(CSUDI_HANDLE hMutex,unsigned int uTimeout)
{
    CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
    CSASSERT(hMutex);
    if(hMutex)
    {
        cs_mutex_t* mutex = (cs_mutex_t*) hMutex;

        DWORD  tid;

        tid = CSGetThreadId();

        if(mutex->owner == tid)
        {
            ++mutex->count;
            enRet = CSUDI_SUCCESS;
        }
        else
        {
            enRet = CSUDIOSSemWait(mutex->semaphore,uTimeout);
            if( enRet == CSUDI_SUCCESS)
            {
                mutex->owner = tid;
                mutex->count = 1;
            }
            else
            {
                CSDEBUG(MODULE_NAME,ERROR_LEVEL,"[CS_OS] can't  acquire semaphore for a mutex %xh, uTimeout=%d !\r\n", mutex, uTimeout);
            }
        }
    }
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

    return enRet;
}


/**
@brief 释放互斥量,即开锁操作

@param[in] hMutex 互斥量句柄
@return 成功返回CSUDI_SUCCESS;失败返回错误代码
@note 试图去释放没有加锁或者已经解锁的互斥量将返回CSUDIOS_ERROR_UNKNOWN_ERROR错误。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSMutexRelease(CSUDI_HANDLE hMutex)
{
     CSUDI_Error_Code 		enRet = CSUDI_FAILURE;

    if(hMutex)
    {
        cs_mutex_t* mutex = (cs_mutex_t*) hMutex;

        DWORD  tid;
        tid = CSGetThreadId();

        if (mutex->owner != tid)
        {
            CSDEBUG(MODULE_NAME,WARN_LEVEL,"[USP_OSP] can't release mutex %08x, count=%d, owner=%x, curtask=%x\n", hMutex, mutex->count, mutex->owner, tid);
            return CSUDI_FAILURE;
        }

        if(mutex->count > 1 )
        {
            enRet = CSUDI_SUCCESS;
            --mutex->count;
            //CSDEBUG(MODULE_NAME,INFO_LEVEL,"[USP_OSP]: Release Mutex %xh  but  Count > 0\r\n", hMutex);
        }
        else if (mutex->count == 1)
        {
            //save
            DWORD pOwner = mutex->owner ;
            int nCount = mutex->count;

            //clear
            mutex->owner = (DWORD)CSUDI_NULL;
            mutex->count = 0;

            //release
            if((enRet=CSUDIOSSemRelease(mutex->semaphore)) != CSUDI_SUCCESS)
            {
                //failed then restore
                mutex->owner = pOwner;
                mutex->count = nCount;
                CSDEBUG(MODULE_NAME,ERROR_LEVEL,"[USP_OSP] can't relase semaphore for a mutex %08X !\n", hMutex);
            }
        }
        else
        {
            CSDEBUG(MODULE_NAME,WARN_LEVEL,"[USP_OSP] can't release mutex %08XH, count=%d!\n", hMutex, mutex->count);
        }
    }
	else
	{
		enRet = CSUDIOS_ERROR_BAD_PARAMETER;
	}

    return enRet;

}

/**
@brief 获取自系统启动后经过的时间

@param[out] psTime 系统启动后经过的时间,详见CSUDIOSTimeVal_S定义。为CSUDI_NULL则返回CSUDIOS_ERROR_BAD_PARAMETER
@return 成功返回CSUDI_SUCCESS;失败返回错误代码
@note 
- 要求精确到操作系统所能提供的最大精度，最少到毫秒,否则可能会对系统产生不可预料的影响!
- 实际上Eastwin仅关心两次使用该接口获得时间的差值，用于获取两次调用的时间间隔，而这个时间的0时刻是否是从开机的一瞬间算起并不重要。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetTime(CSUDIOSTimeVal_S * psTime)
{
	struct timeval stTime;

	if (psTime == CSUDI_NULL)
	{
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	
	gettimeofday(&stTime, NULL);

	psTime->m_nSecond = stTime.tv_sec;
	psTime->m_nMiliSecond = stTime.tv_usec/1000;
	psTime->m_lUSecond = stTime.tv_usec%1000;

	return CSUDI_SUCCESS;
}





/*******************************************************************
 **                     Event definitions                         **
 *******************************************************************/
/**
@brief 创建事件

@param[in] pcName 事件名称,长度不能超过32个字节，如果长度超过32个字节则返回CSUDIOS_ERROR_BAD_PARAMETER，并且创建事件失败
@param[in] uFlags 事件选项,标志可同时使用,也可分别使用。如果此参数为其它值则返回CSUDIOS_ERROR_BAD_PARAMETER。
@note uFlags 的取值包括(各项可相或):
- CSUDIOS_EVENT_INITIAL_STATUS 初始化后事件立即有效，即马上可以Wait到。(若该位为0则表示事件需要Set后方生效)
- CSUDIOS_EVENT_AUTO_RESET CSUDIOSEventWait之后事件自动变为无效。(若该位为0则表示Wait事件后事件仍然有效，除非主动Reset)

@note 这意味着如果uFlags的取值为:
- CSUDIOS_EVENT_AUTO_RESET | CSUDIOS_EVENT_INITIAL_STATUS 初始化后事件立即有效，即马上可以Wait到，Wait之后事件自动变为无效。
- 0 初始化事件之后，需要Set使事件生效；该事件在reset之前永远有效。
@see CSUDIOSEventSet CSUDIOSEventReset
@param[out] phEvent 事件句柄
@return 成功返回CSUDI_SUCCESS;失败返回错误代码,且phEvent为CSUDI_NULL
@note 
- pstrName并不影响创建事件成功与否，即可以为CSUDI_NULL，可以重复,但不推荐这样用
- 有多个任务等待同一个事件时，一旦该事件有效，不确定哪个任务(先)等到事件
- 事件名称应允许空格

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/

typedef struct
{
    pthread_cond_t	condvar;
    pthread_mutex_t	mutex;
    int				signaled;
    int				manual_reset;
}T_CSOSPEvnt;


CSUDI_Error_Code CSUDIOSEventCreate(const char * pcName,unsigned int  uFlags,CSUDI_HANDLE * phEvent)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	T_CSOSPEvnt			*pEvent;
	DWORD 				dwNameLen = 0;
	int 					nRet;

	if(pcName != CSUDI_NULL)
	{
	    dwNameLen = strlen(pcName);
	}

	if (dwNameLen > 32 || phEvent == CSUDI_NULL)
	{
		CSASSERT(dwNameLen <=32);
		CSASSERT(phEvent != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	*phEvent = CSUDI_NULL;
	pEvent = (T_CSOSPEvnt *)malloc(sizeof(T_CSOSPEvnt));
	CSASSERT(pEvent != NULL);
	if (pEvent != NULL)
	{
		if((uFlags&CSUDIOS_EVENT_AUTO_RESET) ==  CSUDIOS_EVENT_AUTO_RESET)
		{
		    pEvent->manual_reset = FALSE;
		}
		else
		{
		    //????????????????????????????????????????????????
		    // always manual reset now, need to be confirmed.
		    //????????????????????????????????????????????????
		    pEvent->manual_reset = TRUE;
		}

		if((uFlags&CSUDIOS_EVENT_INITIAL_STATUS) == CSUDIOS_EVENT_INITIAL_STATUS)
		{
		    pEvent->signaled = TRUE;
		}
		else
		{
		    pEvent->signaled = FALSE;
		}

		nRet = pthread_mutex_init( &pEvent->mutex, NULL );
		CSASSERT(nRet == 0);
		if(nRet == 0) 
		{
	    		nRet = pthread_cond_init( &pEvent->condvar, NULL );
			CSASSERT(nRet == 0);
			if (nRet == 0)
			{
				enRet = CSUDI_SUCCESS;
				*phEvent = (CSUDI_HANDLE)pEvent;
			}
		}
		
		if (enRet != CSUDI_SUCCESS)
		{
			free(pEvent);
		}
	}
	
	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
}


/**
@brief 销毁事件

@param[in] hEvent 事件句柄
@return 如果hEvent有效返回CSUDI_SUCCESS;如果hEvent无效则返回CSUDIOS_ERROR_BAD_PARAMETER
@warning 销毁一个正在被操作（如等待）的事件是一个不确定的行为

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSEventDestroy(CSUDI_HANDLE hEvent)
{
   	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	T_CSOSPEvnt 			*event;
	int 					nRet;

	if (hEvent == CSUDI_NULL)
	{
		CSASSERT(hEvent != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	
        event = (T_CSOSPEvnt *)hEvent;

        nRet = pthread_cond_broadcast( &event->condvar );
        CSASSERT(nRet == 0);
		
        nRet = pthread_cond_destroy( &event->condvar );
        CSASSERT(nRet == 0);

        nRet = pthread_mutex_destroy(&event->mutex);
        CSASSERT(nRet == 0);

   	free(event);
	enRet = CSUDI_SUCCESS;

	return enRet;
}



/**
@brief 设置事件无效

事件无效后，CSUDIOSEventWait将挂起直到事件有效或者超时
@param[in] hEvent 事件句柄
@return 如果成功返回CSUDI_SUCCESS;如果失败返回CSUDI_FAILURE

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSEventReset(CSUDI_HANDLE hEvent)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	T_CSOSPEvnt 			*event;
	int 					nRet;

	if (hEvent == CSUDI_NULL)
	{
		CSASSERT(hEvent != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}

	event = (T_CSOSPEvnt *)hEvent;

	nRet = pthread_mutex_lock(&event->mutex);

	event->signaled = FALSE;

	nRet += pthread_mutex_unlock(&event->mutex);

	CSASSERT(nRet == 0);

	if (nRet == 0)
	{
	    enRet = CSUDI_SUCCESS;
	}

	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
}

/**
@brief 设置事件有效

事件有效后，CSWaitForSingleEvent将立即返回
@param[in] hEvent 事件句柄
@return 如果成功返回CSUDI_SUCCESS;如果失败返回CSUDI_FAILURE

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSEventSet(CSUDI_HANDLE hEvent)
{
	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	T_CSOSPEvnt 			*event;
	int 					nRet;

	if (hEvent == CSUDI_NULL)
	{
		CSASSERT(hEvent != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	
	event = (T_CSOSPEvnt *)hEvent;

	nRet = pthread_mutex_lock(&event->mutex);

	event->signaled = TRUE;
	/* Wake up one or all depending on whether the event is auto-reseting. */
	if( event->manual_reset )
	    nRet += pthread_cond_broadcast(&event->condvar);
	else
	    nRet += pthread_cond_signal(&event->condvar);

	nRet += pthread_mutex_unlock(&event->mutex);

	CSASSERT(nRet == 0);

	if (nRet == 0)
	{
	    enRet = CSUDI_SUCCESS;
	}


	CSASSERT(enRet == CSUDI_SUCCESS);

	return enRet;
}

/**
@brief 等待事件

事件有效时立即返回;无效时挂起任务直到事件有效或者超时
@param[in] hEvent 事件句柄
@param[in] uTimeout 等待超时时间，单位为毫秒，也可选用:
- CSUDIOS_TIMEOUT_IMMEDIATE	不等待，马上超时
- CSUDIOS_TIMEOUT_INFINITY	永远不超时
@return 
	   - 如果成功返回CSUDI_SUCCESS
	   - 如果hEvent错误返回CSUDIOS_ERROR_BAD_PARAMETER;
	   - 在hEvent正确的情况下，如果超时则返回CSUDIOS_ERROR_TIMEOUT。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSEventWait(CSUDI_HANDLE hEvent,unsigned int uTimeout)
{
    	CSUDI_Error_Code 		enRet = CSUDI_FAILURE;
	T_CSOSPEvnt 			*event;
	int 					nRet;

	if (hEvent == CSUDI_NULL)
	{
		CSASSERT(hEvent != CSUDI_NULL);
		return CSUDIOS_ERROR_BAD_PARAMETER;
	}
	
        event = (T_CSOSPEvnt *)hEvent;

        nRet = pthread_mutex_lock(&event->mutex);
        CSASSERT(nRet == 0);

        /* Return immediately if the event is signalled. */
        if(event->signaled)
        {
            if(!event->manual_reset)
            {
                event->signaled = FALSE;
            }

            nRet += pthread_mutex_unlock(&event->mutex);
            CSASSERT(nRet == 0);
            return( CSUDI_SUCCESS);
        }

        /* If just testing the state, return OSAL_TIMEOUT. */
        if( uTimeout == 0 )
        {
            nRet += pthread_mutex_unlock(&event->mutex);
            CSASSERT(nRet == 0);
            enRet = CSUDIOS_ERROR_TIMEOUT;
        }
        else if (uTimeout == 0xFFFFFFFF)
        {
            /* Wait for condition variable to be signaled or broadcast. */
            nRet = pthread_cond_wait(&event->condvar, &event->mutex);
            if (nRet == 0)
            {
                enRet = CSUDI_SUCCESS;
            }
            else
            {
                CSDEBUG(MODULE_NAME,ERROR_LEVEL, "[CSWaitForSingleEvent]ERROR: pthread_cond_wait(%d) return %d\n", uTimeout, nRet);
                enRet = CSUDI_FAILURE;
            }
        }
        else
        {
            struct timespec ts;
            struct timeval tv;

            nRet = gettimeofday (&tv, NULL);
            CSASSERT(nRet == 0);

            ts.tv_sec = tv.tv_sec + (int)(uTimeout/1000);
            ts.tv_nsec = tv.tv_usec*1000 + (int)((uTimeout%1000)*1000000);

            adjusttimespec(&ts);

            nRet = pthread_cond_timedwait(&event->condvar, &event->mutex, &ts);
            //CSDEBUG(MODULE_NAME,3, "nRet=%d, EINVAL:%d, ETIMEDOUT:%d\n", nRet, EINVAL, ETIMEDOUT);
            if (nRet == 0)
            {
                enRet = CSUDI_SUCCESS;
            }
            else if (nRet == ETIMEDOUT)
            {
                enRet = CSUDIOS_ERROR_TIMEOUT;
            }
            else
            {
                CSDEBUG(MODULE_NAME,ERROR_LEVEL, "[CSWaitForSingleEvent]ERROR: pthread_cond_timedwait(%d) return %d\n", uTimeout, nRet);
                enRet = CSUDI_FAILURE;
            }

        }

        if(CSUDI_SUCCESS == enRet)
        {
            if(!event->manual_reset)
            {
                event->signaled = FALSE;
            }
        }
        nRet = pthread_mutex_unlock(&event->mutex);
        CSASSERT(nRet == 0);

    CSASSERT(enRet == CSUDI_SUCCESS || enRet == CSUDIOS_ERROR_TIMEOUT);
    return enRet;
}


/*******************************************************************
 **                     Memory definitions                        **
 *******************************************************************/
/**
@brief 分配内存,但不初始化

相当于malloc,新分配的内存其内容是不确定的
@param[in] uSize 待分配的内存大小
@return 成功返回内存指针;失败返回CSUDI_NULL
@note uSize等于0时，函数分配失败，且返回值为CSUDI_NULL。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
void * CSUDIOSMalloc( unsigned int uSize )
{
		PVOID pvMem =  NULL;
	
		UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);
	
    CSASSERT(uSize > 0);

    if(uSize > 0)
    {
        pvMem =  malloc(uSize);
    }

    CSASSERT(pvMem != NULL);
    if(NULL == pvMem)
    {

        CSDEBUG(MODULE_NAME,ERROR_LEVEL, "[CSMalloc]ERROR[errno=%d]: malloc %x fail\r\n", errno,uSize);
    }

    return pvMem;

}

/**
@brief 重新分配内存

相当于realloc,重新分配内存为新的大小,并拷贝尽量多的内容,如果新内存空间比旧的大,则新增部分的内容是不确定的
@param[in] pvAddr 待重新分配的内存地址
@param[in] uSize 待分配的内存大小
@return 成功返回内存指针;失败返回CSUDI_NULL
@note 
- uSize等于0时该函数执行失败,可以是CSUDI_NULL也可以是一个地址。
- 如果pvAddr不是一个之前CSUDIOSMalloc,CSUDIOSRealloc返回的地址,结果是不确定的
- 如果pvAddr==CSUDI_NULL,功能与CSUDIOSMalloc一样
- 如果分配失败,旧的内存不会自动释放,其内容也不发生变化，返回值可能与pvAddr不一致,也可能一致

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
void* CSUDIOSRealloc( void * pvAddr,unsigned int uSize )
{
		PVOID pvMem =  NULL;
		
		UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

    if(uSize > 0)
    {
        pvMem =  realloc(pvAddr,uSize);
    }

    CSASSERT(pvMem != NULL);
    
    return pvMem;
    
}

/**
@brief 分配内存，并初始化

相当于calloc，分配nElements个nElementSize大小的objects,并将内存全部设为0
@param[in] uElements 待分配的元素个数
@param[in] uElementSize 待分配的每个元素的大小
@return 如果成功返回内存指针;如果失败返回为NULL
@note nElements或nElementSize等于0时的返回值是不确定的，可以是CSUDI_NULL也可以是一个地址。 

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
void* CSUDIOSCalloc(unsigned int uElements, unsigned int uElementSize)
{
	PVOID pvMem =  NULL;
	
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

    CSASSERT(nElements > 0);
    CSASSERT(nElementSize > 0);

    if((uElements>0) && (uElementSize>0))
    {
        pvMem =  calloc(uElements,uElementSize);
    }

    CSASSERT(pvMem != NULL);
    if(NULL == pvMem)
    {
        CSDEBUG(MODULE_NAME,ERROR_LEVEL, "[CSCalloc]ERROR[errno=%d]: calloc %x %x fail\r\n", errno,uElements,uElementSize);
    }

    return pvMem;

}

/**
@brief 释放分配的内存

相当于free,释放CSUDIOSMalloc,CSUDIOSRealloc,CSUDIOSCalloc分配的内存
@param[in] pvAddr 待释放的内存地址
@return 如果成功返回CSUDI_SUCCESS;如果失败返回CSUDI_FAILURE
@note 
- 如果pvAddr不是一个之前CSUDIOSMalloc,CSUDIOSRealloc,CSUDIOSCalloc返回的地址,结果是不确定的
- 多次释放同一段内存将返回失败

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is required for Android DVB-S2 plus OTT project.
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSFree( void * pvAddr )
{
    if (pvAddr != NULL)
    {
        free(pvAddr);
    }

    return CSUDI_SUCCESS;
}


/**
@brief 获取可用的RAM空间
@param[out] pnFreeCount 系统可用的RAM空间
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetFreeRAM(unsigned int * puFreeCount)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;
	
	if(puFreeCount == CSUDI_NULL)
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取CPU个数
@param[out] pnCpuCount CPU的个数
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetCPUCount(unsigned int * puCpuCount)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;
	
	if(puCpuCount == CSUDI_NULL)
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取指定索引CPU的处理器利用率(%)
@param[out] pnUsage CPU处理器利用率，取值范围为0--1000;
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetCPUUsage(unsigned int index, unsigned int * puUsage)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;
	if(puUsage == CSUDI_NULL)
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取系统中所有任务的句柄
@param[out]  hThread 系统中所有任务的句柄数组
@param[in/out]   nSize hThread数组的大小，返回实际Thread的个数
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetAllThreadHandle(CSUDI_HANDLE* phThread, unsigned int*  puSize)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;
	if((phThread == CSUDI_NULL) || (puSize == CSUDI_NULL))
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	
	if(*puSize == 0)
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取指定任务的基本信息，包括任务名称、任务ID、任务优先级
@param[in]   hThread 指定任务的句柄
@param[out]  psThreadInfo 任务的基本信息
@return 获取成功则返回CSUDI_SUCCESS，psThreadInfo不为NULL，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetThreadInfo(CSUDI_HANDLE hThread, CSUDIOSThreadInfo_S* psThreadInfo)
{
		//CSUDI_FAILURE
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;
	
	if((psThreadInfo == CSUDI_NULL) || (hThread == CSUDI_NULL))
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取指定任务的RAM消耗
@param[in]   hThread 指定任务的句柄
@param[out]  pnUsedRam 指定任务的RAM消耗
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetThreadUsedRAM(CSUDI_HANDLE hThread, unsigned int * puUsedRam)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;	
	if((puUsedRam == CSUDI_NULL) || (hThread == CSUDI_NULL))
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}

/**
@brief 获取指定任务的CPU利用率(%)
@param[in]   hThread 指定任务的句柄
@param[out] pnUsage 指定任务的CPU负载，取值范围为:0--1000
@return 获取成功则返回CSUDI_SUCCESS，失败则返回CSUDI_FAILURE。

-------------------------------------------------------------------------------
ENTROPIC COMMENTS ON COSHIP API
	2013/11/06
		This API is NOT required for Android DVB-S2 plus OTT project. 
-------------------------------------------------------------------------------

*/
CSUDI_Error_Code CSUDIOSGetThreadCPUUsage(CSUDI_HANDLE hThread, unsigned int * puUsage)
{
	UDIDRV_LOGI("%s %s begin\n", __FUNCTION__, UDIDRV_IMPLEMENTED);

	CSUDI_Error_Code Retcode = CSUDI_SUCCESS;	
	if((puUsage == CSUDI_NULL) || ((hThread == CSUDI_NULL)))
		Retcode = CSUDIOS_ERROR_BAD_PARAMETER;
	UDIDRV_LOGI("%s (Retcode =%d)end\n", __FUNCTION__, Retcode);    
	return Retcode;
}
