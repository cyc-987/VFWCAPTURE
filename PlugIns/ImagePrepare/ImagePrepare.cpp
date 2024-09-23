// ImagePrepare.cpp : Defines the initialization routines for the DLL.

#include "stdafx.h"
#include "ImagePrepare.h"
//
#include "BufStruct.h"
#include "ImageProc.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//
/////////////////////////////////////////////////////////////////////////////
// CImagePrepareApp
BEGIN_MESSAGE_MAP(CImagePrepareApp, CWinApp)
	//{{AFX_MSG_MAP(CImagePrepareApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
//
/////////////////////////////////////////////////////////////////////////////
// CImagePrepareApp construction
CImagePrepareApp::CImagePrepareApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}
/////////////////////////////////////////////////////////////////////////////
// The one and only CImagePrepareApp object
CImagePrepareApp theApp;

char sInfo[] = "��������-����ͷ��Ƶ��ͼƬ��ȡ������";
bool bLastPlugin = false;

DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	bLastPlugin = bLast;
}
//�������
DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	return sInfo;
}
//
DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{   
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	//theApp.dlg.Create(IDD_PLUGIN_SETUP);
	//theApp.dlg.ShowWindow(SW_HIDE);
}
DLL_EXP int ON_PLUGINCTRL(int nMode,void* pParameter)
{
//ģ��״̬�л�
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	int nRet = 0;
	return nRet;
}

/****************************************************************************************/
/*                             ����ͷ��Ƶ��ͼƬ��ȡ���ز����ȴ���                       */
/****************************************************************************************/
extern "C" _declspec(dllimport) void myHeapAllocInit(BUF_STRUCT* pBufStruct);

DLL_EXP void ON_PLUGINRUN(int w,int h,BYTE* pYBits,BYTE* pUBits,BYTE* pVBits,BYTE* pBuffer)
{
//pYBits ��СΪw*h
//pUBits �� pVBits �Ĵ�СΪ w*h/2
//pBuffer �Ĵ�СΪ w*h*6
//�����㷨������һ�����裬��w��16�ı���
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
   
	//ShowDebugMessage("��printf�����÷�����,X=%d,Y=%d\n",10,5);

     //���д��Ӧ�������


	//start
	BUF_STRUCT* pBS = ((BUF_STRUCT*)pBuffer); //nick name
	int pixels = w*h;
	int i = 0; //iteration varible

	//first time init part
	if(pBS->bNotInited == TRUE){

		//step1
		pBS->colorBmp = pBuffer + sizeof(BUF_STRUCT);
		pBS->grayBmp = pBS->colorBmp;
		pBS->clrBmp_1d8 = pBS->grayBmp + pixels*2;
		pBS->grayBmp_1d16 = pBS->clrBmp_1d8 + pixels/4;
		pBS->TempImage1d8 = pBS->grayBmp_1d16 + pixels/16;
		pBS->lastImageQueue1d16m8 = pBS->TempImage1d8 + pixels/8;
		pBS->pOtherVars = (OTHER_VARS*)(pBS->lastImageQueue1d16m8 + pixels/2);
		pBS->pOtherData = (aBYTE*)(pBS->pOtherVars) + sizeof(OTHER_VARS);
		for(i=0; i<8; i++){
			pBS->pImageQueue[i] = pBS->lastImageQueue1d16m8 + i*pixels/16;
		}

		//step2
		pBS->W = w;
		pBS->H = h;
		pBS->cur_allocSize = pBS->allocTimes = pBS->cur_maxallocsize = 0;
		pBS->bLastEyeChecked = FALSE;
		pBS->EyeBallConfirm = pBS->EyePosConfirm = TRUE;
		pBS->nImageQueueIndex = pBS->nLastImageIndex = -1;

		//init hist map (skin colors)
		OTHER_VARS* other = (OTHER_VARS*)(pBS->pOtherVars); //nick name
		for(i=0;i<256;i++){
			other->byHistMap_U[i] = other->byHistMap_V[i] = 0;
			if(i>=77 && i<=135){
				other->byHistMap_U[i] = 1;
			}
			if(i>=145 && i<=193){
				other->byHistMap_V[i] = 1;
			}
		}

		//neglect FeaProcBuf

		//calculate max_allocSize
		pBS->max_allocSize = pixels*17/16 - sizeof(BUF_STRUCT) - sizeof(OTHER_VARS);
		//init pBS in ImageProc.lib
		myHeapAllocInit(pBS);
		
		//complete initialization
		pBS->bNotInited = FALSE;

		//debug message
		ShowDebugMessage("ImagePrepare: init success\n");

	}//close if (first time init)

	//set display image pointer
	pBS->displayImage = pYBits;
	//copy the image
	memcpy(pBS->colorBmp, pYBits, pixels);
	memcpy(pBS->colorBmp+pixels, pUBits, pixels/2);
	memcpy(pBS->colorBmp+pixels+pixels/2, pVBits, pixels/2);

	//downscale the image
	ReSample(pBS->colorBmp, w, h, w/2, h/4, false, false, pBS->clrBmp_1d8);
	ReSample(pBS->grayBmp, w, h, w/4, h/4, false, true, pBS->grayBmp_1d16);



	//����Ĳ������ڲ���ͼ������Ľ���Ƿ���ȷ��
	
		if( bLastPlugin )
		{
		
			//����grayBmp_1d16:��grayBmp_1d16���Ƶ���ʾͼƬ�����Ͻ�
			CopyToRect(pBS->grayBmp_1d16, pYBits, w/4, h/4, w, h, 0, 0, true);
			//����colorBmp:��clrBmp_1d8���Ƶ���ʾͼƬ�����Ͻ�
			CopyToRect(pBS->clrBmp_1d8,  pYBits, w/2, h/4, w, h, w/2, 0, false);
		}
    
}



DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	//theApp.dlg.DestroyWindow();
}
