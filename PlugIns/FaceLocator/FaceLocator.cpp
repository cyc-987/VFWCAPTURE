// FaceLocator.cpp : Defines the initialization routines for the DLL.
#include "stdafx.h"
#include "FaceLocator.h"
#include "BufStruct.h"
#include "ImageProc.h"

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
// CFaceLocatorApp
BEGIN_MESSAGE_MAP(CFaceLocatorApp, CWinApp)
	//{{AFX_MSG_MAP(CFaceLocatorApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
/////////////////////////////////////////////////////////////////////////////
// CFaceLocatorApp construction
CFaceLocatorApp::CFaceLocatorApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}
/////////////////////////////////////////////////////////////////////////////
// The one and only CFaceLocatorApp object
CFaceLocatorApp theApp;
char sInfo[] = "人脸跟踪-基于彩色信息的人脸分割处理插件";
bool bLastPlugin = false;
DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	bLastPlugin = bLast;
}
DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	return sInfo;
}
DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//theApp.dlg.Create(IDD_PLUGIN_SETUP);
	//theApp.dlg.ShowWindow(SW_HIDE);
}
DLL_EXP int ON_PLUGINCTRL(int nMode,void* pParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	int nRet = 0;
	switch(nMode)
	{
	case 0:
		{
			//theApp.dlg.ShowWindow(SW_SHOWNORMAL);
			//theApp.dlg.SetWindowPos(NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED);
		}
		break;
	}
	return nRet;
}
/****************************************************************************************/
/*                             人脸检测与定位                                           */
/****************************************************************************************/
//self defined functions

//function to set the skin color
DLL_EXP void skinModeling(int w, int h, BYTE* clrBmp_1d8, BUF_STRUCT* pBS, BYTE* pDst)
{
	int pixels = w*h;
	BYTE* Ustart = clrBmp_1d8 + pixels*2;
	BYTE* Vstart = Ustart + pixels;
	BYTE* tempImage = pDst;

	//judge
	int i,j;
	aBYTE* mapU = pBS->pOtherVars->byHistMap_U;
	aBYTE* mapV = pBS->pOtherVars->byHistMap_V;

	for(i=0;i<h;i++){
		for(j=0;j<w;j++){
			tempImage[i*w+j] = (mapU[Ustart[i*w+j]] && mapV[Vstart[i*w+j]])? 255 : 0; // if U & V both in range, set to white
		}
	}
}

DLL_EXP int outrange(int x, int y, int w, int h){ // judge if the point is out of range
		return (x<0 || x>=w || y<0 || y>=h);
}
// function to erode or dilate
// mode: 0 for erode 腐蚀, 1 for dilate 膨胀
DLL_EXP void erodeORdilate(bool mode, int w_actual, int h_actual, int kernelsizeN, BYTE* pSrc, BYTE* pDst)
{
	int i,j,m,n;
	int w = w_actual;
	int h = h_actual;
	int kernelsize = kernelsizeN;
	int halfkernelsize = kernelsize/2;


	for(i=0;i<h;i++){
		for(j=0;j<w;j++){
			// decide the value of the point
			int flag_set = 0;
			for(m=-halfkernelsize;m<=halfkernelsize;m++){
				int flag_ok = 0;
				for(n=-halfkernelsize;n<=halfkernelsize;n++){
					if(!outrange(j+m, i+n, w, h)){
						if(mode == 0){ // erode
							if(pSrc[(i+n)*w+j+m] == 0){
								pDst[i*w+j] = 0;
								flag_ok = 1;
								break;
							}
						}else{ // dilate
							if(pSrc[(i+n)*w+j+m] == 255){
								pDst[i*w+j] = 255;
								flag_ok = 1;
								break;
							}
						}
					}
				}
				if(flag_ok) {
					flag_set = 1;
					break;
				}
			}
			if(!flag_set){
				if(mode == 0) pDst[i*w+j] = 255;
				else pDst[i*w+j] = 0;
			}
		}
	}	
}

// morphological operation
DLL_EXP void morphologicalOperation(int w, int h, BYTE* pImage)
{
	BYTE* tempImage2 = myHeapAlloc(w*h);
	// open operation
	erodeORdilate(0, w, h, 3, pImage, tempImage2);
	erodeORdilate(1, w, h, 3, tempImage2, pImage);
	// close operation
	erodeORdilate(1, w, h, 7, pImage, tempImage2);
	erodeORdilate(0, w, h, 7, tempImage2, pImage);
	myHeapFree(tempImage2);
}

//mark connected domain
DLL_EXP BYTE getAdjacentPixel(int mode, BYTE* pImg, int w, int h, int i, int j)
{
	if(mode == 1){
		//left
		if(j-1<0){
			return 0;
		}else{
			return pImg[i*w+j-1];
		}
	}else{
		//up
		if(i-1<0){
			return 0;
		}else{
			return pImg[(i-1)*w+j];
		}
	}
}
DLL_EXP int findRoot(int x, int* LK) {
    if (LK[x] != x) {
        LK[x] = findRoot(LK[x], LK);  // Path compression
    }
    return LK[x];
}
DLL_EXP void markConnectedDomain(int w, int h, BYTE* pImg)
{
	//init
	int mark[120][160] = {0}; // mark the connected domain
	int LK[1024]; // store the marks
	int i,j;
	
	for(i=0;i<1024;i++){
		LK[i] = i;
	}
	int assign_num = 1;

	//mark 1
	for(i=0;i<h;i++){
		for(j=0;j<w;j++){
			if(pImg[i*w+j] == 255){
				if(getAdjacentPixel(1,pImg,w,h,i,j) == 0 && getAdjacentPixel(0,pImg,w,h,i,j) == 0){
					mark[i][j] = assign_num;
					assign_num++;
				}else if(getAdjacentPixel(1,pImg,w,h,i,j) == 255 && getAdjacentPixel(0,pImg,w,h,i,j) == 0){
					mark[i][j] = mark[i][j-1];
				}else if(getAdjacentPixel(1,pImg,w,h,i,j) == 0 && getAdjacentPixel(0,pImg,w,h,i,j) == 255){
					mark[i][j] = mark[i-1][j];
				}else if(getAdjacentPixel(1,pImg,w,h,i,j) == 255 && getAdjacentPixel(0,pImg,w,h,i,j) == 255){
					if(mark[i-1][j] == mark[i][j-1]){
						mark[i][j] = mark[i][j-1];
					}else{
						int Lmax, Lmin;
						Lmax = Lmin = mark[i][j] = mark[i][j-1];
						//set equal relation
						if(mark[i-1][j] > Lmin){
							Lmax = mark[i-1][j];
						}else{
							Lmin = mark[i-1][j];
						}
						//update equal relation
						Lmax = findRoot(Lmax, LK);
						Lmin = findRoot(Lmin, LK);
						LK[Lmax] = Lmin;
					}
				}
			}else{
				continue;
			}
		}
	}
	//ShowDebugMessage("assign_num: %d",assign_num);

	//reassign mark in LK
	int myclass = 1;
	int LK1[1024] = {0};
	for(i=1;i<assign_num;i++){
		if(LK[i] == i){
			//give new
			LK1[i] = myclass;
			myclass++;
		}else{
			//find its root
			int root = LK[i];
			LK1[i] = LK1[root];
		}
	}
	//ShowDebugMessage("final classes: %d",myclass);

	//copy back
	for(i=0;i<h;i++){
		for(j=0;j<w;j++){
			pImg[i*w+j] = LK1[mark[i][j]];
		}
	}
}

//find the largest connected domain
DLL_EXP void findLargestDomain(int w, int h, BYTE* pImg)
{
	int lookup[255] = {0};
	int i;

	//count
	for(i=0;i<w*h;i++){
		lookup[pImg[i]]++;
	}
	lookup[0] = 0;
	//find max
	i = 2;
	int maxindex = 1;
	int maxcount = lookup[1];
	while(lookup[i] != 0){
		if(lookup[i] > maxcount){
			maxcount = lookup[i];
			maxindex = i;
		}
		i++;
	}
	//ShowDebugMessage("maxindex is %d at %d times", maxindex, maxcount);
	//reset image color
	for(i=0;i<w*h;i++){
		if(pImg[i] == maxindex){
			pImg[i] = 255;
		}else{
			pImg[i] = 0;
		}
	}
}

//update paramaters
DLL_EXP void updateRcnFace(int w, int h, BUF_STRUCT* pBS, BYTE* pImg)
{
	//init
	int i,j;
	int left, right, up, down, count;
	count = right = down = 0;
	left = w;
	up = h;
	
	aBYTE* bmp = (aBYTE*)(pBS->clrBmp_1d8);
	for(i=0;i<h;i++){
		for(j=0;j<w;j++){
			bmp[i*w*2+j*2] = bmp[i*w*2+j*2+1] = pImg[i*w+j]; //upscale
			if(pImg[i*w+j] == 255){
				//find face area
				count++;
				if(j < left) left = j;
				if(j > right) right = j;
				if(i < up) up = i;
				if(i > down) down = i;
			}else{
				continue;
			}
		}
	}

	//write
	aRect* rcnFace = (aRect*)(&(pBS->rcnFace));
	pBS->nFacePixelNum = count*2;
	rcnFace->height = down - up;
	rcnFace->width = (right - left)*2;
	rcnFace->left = left*2;
	rcnFace->top = up;
}

//draw retangle
DLL_EXP void drawFaceArea(BUF_STRUCT* pBS)
{
	int w, h;
	w = pBS->W;
	h = pBS->H;

	aRect area;
	area.height = 4*(pBS->rcnFace.height);
	area.width = 2*(pBS->rcnFace.width);
	area.left = 2*(pBS->rcnFace.left);
	area.top = 4*(pBS->rcnFace.top);
	//ShowDebugMessage("up:%d, height:%d, left:%d, width:%d",area.top,area.height,area.left,area.width);

	COLORREF clr = TYUV1(0,250,250);
	DrawRectangle(pBS->displayImage, w, h, area, clr, false);
}

//end of func

DLL_EXP void ON_PLUGINRUN(int w,int h,BYTE* pYBits,BYTE* pUBits,BYTE* pVBits,BYTE* pBuffer)
{
//pYBits 大小为w*h
//pUBits 和 pVBits 的大小为 w*h/2
//pBuffer 的大小为 w*h*4
//下面算法都基于一个假设，即w是16的倍数

	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	
      //请编写相应处理程序
	//start
	int pixels = w*h;
	BUF_STRUCT* pBS = ((BUF_STRUCT*)pBuffer);
	
	//init dynamic memory
	BYTE* tempImage = myHeapAlloc(pixels/16);

	//mark skin in tempImage
	skinModeling(w/4, h/4, pBS->clrBmp_1d8, pBS, tempImage); 
	//morphological operation in tempImage
	morphologicalOperation(w/4, h/4, tempImage); 
	//mark connected domain in tempImage
	markConnectedDomain(w/4, h/4, tempImage);
	//find the largest connected domain
	findLargestDomain(w/4, h/4, tempImage);
	//update parameters
	updateRcnFace(w/4, h/4, pBS, tempImage);
	//draw face area
	drawFaceArea(pBS);

	//TEST ONLY
	if( bLastPlugin ){
		CopyToRect(tempImage, pYBits, w/4, h/4, w, h, 0, 0, true);
	}

	//free memory
	myHeapFree(tempImage);

}

DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//theApp.dlg.DestroyWindow();
}

