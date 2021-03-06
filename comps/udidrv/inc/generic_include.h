/****************************************************************************/
/*
 * Filename:        generic_include.h
 *
 *
 * Description:     API implementation for COSHIP interface layer .
 *
 *
 * Author:          Trident Design-In Team
 *
 ****************************************************************************/

#ifndef _GENERICINCLUDE_H_
#define _GENERICINCLUDE_H_

/* Linux Includes*/
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <kal.h>

/* Edwards/Pipeline Includes*/
#include "pipeline.h"
#include "pipeline_manager.h"
#include "stbcfg.h"
#include "kal.h"
#include "presentation.h"
//#include "presentation_internal.h" //commented by frank.zhou
#include "pr_audio.h"
#include "graphics_gen.h"
#include "image.h"
#include "demux.h"
#include "drm.h"
#include "tvenc.h"
// 2011-01-17 TerenceZhang add for real standby feature.
#include "tmscsScd.h"

/*Middleware includes*/
#include "tm_video.h"
#include "tm_audio.h"
#include "tm_inject.h"
#include "tm_pcm_priv.h"

/*Coship-Udi includes*/
#include "udi2_os.h"
#include "udiplus_debug.h"
#include "udi2_audio.h"
#include "udi2_aout.h"
#include "udi2_vout.h"
#include "udi2_screen.h"
#include "udi2_inject.h"
#include "udi2_osg.h"
#include "udi2_video.h"
#include "udi2_player.h"
#include "udi2_toolset.h"
#include "udi2_demux.h"
#include "udi2_section.h"
#include "udi2_public.h"
#include "udiplus_typedef.h"




//2011-01-30 TerenceZhang begin: added for dual video surface support,it could fix the freeze frame issue. 
#define DUAL_VIDEO_SURFACE
#ifdef DUAL_VIDEO_SURFACE
//#define VDOBJ_MALLOC
//#define OSG_USEIMAGE
#define LOOPINJECT
#define SHOWINDECODE

//2011-02-19 TerenceZhang added:
//Since the iframe surfcace could not set windows size after video object stoped,
//so we adjust that only stop it before a new decoding started.
#define STATIC_IFRAME_DECODE
#endif

#define MAX_PIPE_OBJECTS  6
#define MAX_TUNER_OBJECTS 1
#define MAX_VIDEO_OBJECTS 2 
#define MAX_STILL_VIDEO_OBJECTS 1
#define MAX_AUDIO_OBJECTS 2
#define MAX_NUM_DMA_DEMUX 1
#define MAX_NUM_TSI_DEMUX 2   /*As of now, we support only 2 demuxes with TSIn caps*/
#define MAX_DEMUX_OBJECTS (MAX_NUM_TSI_DEMUX + MAX_NUM_DMA_DEMUX)

#define MAX_VIDEO_SUBSYSTEM 1
#define MAX_COSHIP_SCREEN_CALLBACK 32
#define MAX_SECTION_FILTERS 64
#define DEMUX_SECTION_MAX_SIZE 4*1024
#define MAX_PLAYER_OBJECTS 5
#define MAX_PCM_INSTANCES 1
#define MAX_DISPLAY_HANDLES 1 

#define ES_VIDEO_OBJECT 3 /*Currently we are restricting it to 3 & 4...TBD: Need this dynamic*/
#define ES_AUDIO_OBJECT 5

#define PCM_INJECT_INDEX	(2)
//video section
#define CS_TRID_MAX_DEC (2)
//audio section
#define CS_TRID_MAX_AUDIO 2
typedef enum _CNXTStopMode	
{	
	CNXT_STOPMODE_BLACK,	/**< 黑屏*/
	CNXT_STOPMODE_FREEZE,	/**< 静帧*/	
	CNXT_STOPMODE_FIFO	    /**< 淡入淡出*/
}CNXTStopMode;
typedef enum _CNXTServiceType
{	
	CNXT_SERVICE_TS_HW,	 	/**< TS硬件注入，一般为Tuner*/
	CNXT_SERVICE_TS_SW,		/**< TS使用注入接口软件注入,如IPTV、PVR*/
	CNXT_SERVICE_ES_HW,		/**< ES硬件注入，一般为Tuner*/
	CNXT_SERVICE_ES_SW	 	    /**< ES使用注入接口软件注入,如IPTV、PVR*/
}CNXTServiceType;
#ifdef DUAL_VIDEO_SURFACE  
typedef enum CNXTAttachpoint
{
	 CNXT_IDLE_IMG_ATTACH,
        CNXT_STILL_IMG_ATTACH,
        CNXT_VIDEO_IMG_ATTACH
}CNXTAttachpoint;
#endif
typedef struct
{
     CSUDISCREENCallback_F pFnCallBack;
     void *pUserData;
     bool   bUsed;
}CS_ScreenCallback;

typedef  struct COSHIP_Setup
{
    CSUDIVIDEOMatchMethod_E          ARTypeSD; /* aspect ratio handling mode */
	CSUDIVIDEOMatchMethod_E	         ARTypeHD; /* aspect ratio handling mode */
    CSUDIVIDEOAspectRatioType_E      ARSD;
	CSUDIVIDEOAspectRatioType_E      ARHD;
    CSUDIVIDEOStopMode_E             eStopMode;
//    CSUDIAOUTDevices_E eAoutDevice;
    CSUDIAOUTDevices_E eDecAoutDevice;
    CSUDIAOUTDevices_E eBypassAoutDevice;
    CSUDIAOUTMode_E eAoutMode;
//    CSUDIWinRect_S                   RectSD;
//	CSUDIWinRect_S                   RectHD;
//    CSUDISCREENResolution_E          uHDResolution;
//    CSUDISCREENResolution_E          uSDResolution;    
    u_int8    nBrightness;
    u_int8    nSaturation;
    u_int8    nContrast;
    CSUDIAUDIOChannelType_E eChannelType; /*this is for live audio*/
    bool AudioMute;   /*this is for live audio*/
    bool bPcmMute[MAX_PCM_INSTANCES]; /*this is for PCM inject channel*/
    unsigned int unPcmVolume[MAX_PCM_INSTANCES];/*this is for PCM inject channel and note: the volume use trident scope*/
    unsigned int unLRMap[MAX_PCM_INSTANCES];
    bool AoutRCAmute;
    bool AoutHDMImute;
    bool Aoutspdifmute;
    
}COSHIP_Setup;
typedef  struct CS_CNXT_Video_SubSystem
{
    u_int32             uIndex;
    u_int32           uVidPid;
    CSUDIWinRect_S          RectSD;
	CSUDIWinRect_S          RectHD;
    CSUDIWinRect_S	 SrcRectScaleSD;
	CSUDIWinRect_S	 SrcRectScaleHD;
    bool bVidFullScreenSD;
	bool bVidFullScreenHD;
    bool bVidFullImageSD;
    bool bVidFullImageHD;
    CNXTStopMode     StopMode;
    CNXT_VIDEO_FORMAT VideoFormat;
    VidState      VideoState;
    CNXTServiceType ServiceType;
    /*All other parameters to be filled as required */
    void *pfNewFrameCallabck;
    u_int32 CSHandle;
    void *pNewFrameUserData;
    u_int32 	deviceno;
    bool VidRgnShow;
    bool bJpeg;
    CNXT_VIDEO_PICTURE_INFO   PictureInfo;
    CNXT_VIDEO_PICTURE_INFO   StillPictureInfo;
    u_int32                   uPTSdelay;
 }CS_CNXT_Video_SubSystem;

typedef  struct CS_CNXT_Display_SubSystem
{
      
      u_int8    nBrightness;
      u_int8    nSaturation;
      u_int8    nContrast;
      u_int32   nDefaultPALResolution;
      u_int32   nDefaultNTSCResolution;
      bool       bAutoResolution;
      u_int32   uHDVidStandard;
      u_int32   uSDVidStandard;
      CSUDISCREENResolution_E uHDResolution;
      CSUDISCREENResolution_E uSDResolution;
//      CNXT_AR   AspectRatio;
      u_int32   nOutputDevices;
      u_int8  nAlpha;
//      CSUDIVIDEOMatchMethod_E          ARTypeSD; /* aspect ratio handling mode */
//	  CSUDIVIDEOMatchMethod_E	          ARTypeHD; /* aspect ratio handling mode */
//      CSUDIVIDEOAspectRatioType_E ARSD;
//	  CSUDIVIDEOAspectRatioType_E ARHD;
//      bool      bPlaneVisible[CNXT_DRM_PLANE_LAST];
      /*All other parameters to be filled as required */
      u_int32 	deviceno;
      int8 Xoffset;
      int8 Yoffset;
//      CS_ScreenCallback  ScreenCallback[32];
      void  *pUserData;      
//      CNXT_CC_HANDLE              hCCHandle;
//      CNXT_VBI_HANDLE             hVBIHandle;
      u_int32                     uVBITypes; 
 }CS_CNXT_Display_SubSystem;

/* Structure to hold all objects in PIPE*/

typedef struct CS_TM_Pipe_Objects
{

   PIPE_PIPELINE_OBJECT     *hPipeObj[MAX_PIPE_OBJECTS];
   PIPE_TUNER_OBJECT        *hTunerObj[MAX_TUNER_OBJECTS];
   PIPE_DEMUX_OBJECT        *hDemuxObj[MAX_PIPE_OBJECTS];/*MAX_DEMUX_OBJECTS*/
   PIPE_AUDIO_OBJECT        *hAudioObj[MAX_AUDIO_OBJECTS];
   PIPE_VIDEO_OBJECT        *hVideoObj[MAX_VIDEO_OBJECTS];
   CS_CNXT_Video_SubSystem   TridVideoSubSystem[MAX_VIDEO_SUBSYSTEM];
   CS_CNXT_Display_SubSystem  TmDisplaySubSys[MAX_VIDEO_SUBSYSTEM];
   COSHIP_Setup TridCoshipSetup[MAX_PIPE_OBJECTS];
}CS_TM_PIPE_OBJECTS;

typedef struct CS_TM_VP_Device_Objects
{

   PIPE_VP_DEVICE_OBJECT  *pHDDevice;
   PIPE_VP_DEVICE_OBJECT  *pSDDevice;
   PIPE_VP_SURFACE_OBJECT *pHDVideoSurface[2];
   PIPE_VP_SURFACE_OBJECT *pSDVideoSurface[2];  
   CS_ScreenCallback  ScreenCallback[MAX_COSHIP_SCREEN_CALLBACK];
#ifdef DUAL_VIDEO_SURFACE  
   CNXTAttachpoint        eImgattachpoint;	//indicated which surface should be enable.
#endif
   bool     bShowVideo[2];
}CS_TM_VP_DEVICE_OBJECTS;

typedef struct CS_TM_AP_Device_Objects
{

   PIPE_AUDIO_PRESENTATION_OBJECT *pAudioPresentation[1]; //according to pipeline driver we have only one apm
}CS_TM_AP_DEVICE_OBJECTS;
typedef enum
{
    SF_INIT=0,
    SF_READY,
    SF_FILTERING,
    SF_STOP
} SF_State;
/* State structure used to track each table request */
typedef struct 
{
   bool                       InUseFlag;
   u_int8                     uDemuxId;
   PIPE_DEMUX_FILTER_CFG      DmxFilterCfg ;   
   u_int16                    PID;
   int                        nBufSize;
   CSUDISECTIONCallback_F     fnSectionCallback;
   u_int16                    *hCbuf;
   void                       *pvUserData;
   SF_State                   state;
   void*                      pTag;					
} SECTION_REQUEST;

typedef struct UDI_SECTION_HNDL
{
   u_int8 nFilter;
   u_int8 nDemuxId;
}UDI_SECTION_HNDL;

typedef struct
{
    /* in milli dB */
    int32                                nVolumeLeft;
    int32                                nVolumeRight;
}CSUDI_Volume_Cfg_S;


extern CS_TM_PIPE_OBJECTS gTmPipeObject;
extern CS_TM_VP_DEVICE_OBJECTS gTmVpDeviceObj;
extern CS_TM_AP_DEVICE_OBJECTS gTmApDeviceObj;
/* Forward Function Declaration*/
CSUDI_Error_Code tm_section_init(void);
CSUDI_Error_Code tm_desc_init(void);

void Section_filter_event_notifier(struct pipe_pipeline_obj *pPipe,
                                   void                     *pUserData,
                                   PIPE_NOTIFY_EVENT         Event,
                                   void                     *pData,
                                    void                     *pTag );
#endif


