// BlinkEyeCheck.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "BlinkEyeCheck.h"
#include "BufStruct.h"
#include "ImageProc.h"
//self includes
#include "math.h"
//endof self includes

//
#define MAX_EYE_AMP         1
#define MAX_EYE_Y_DIFF      (3*MAX_EYE_AMP)//5
#define MIN_EYE_X_DIFF      (15*MAX_EYE_AMP)//4
#define MAX_EYE_X_DIFF      (40*MAX_EYE_AMP)//30
#define MAX_EYE_SIZE        (150*MAX_EYE_AMP)//200
//#define MIN_EYE_SIZE        (20*MAX_EYE_AMP)//200
//
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
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
// CBlinkEyeCheckApp

BEGIN_MESSAGE_MAP(CBlinkEyeCheckApp, CWinApp)
	//{{AFX_MSG_MAP(CBlinkEyeCheckApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CBlinkEyeCheckApp construction
CBlinkEyeCheckApp::CBlinkEyeCheckApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CBlinkEyeCheckApp object

CBlinkEyeCheckApp theApp;
//
#define PC_MODE
/***************************************************************
这些变量只在PC版中存在，方便调试
在DSP版本中不用引入
***************************************************************/
#ifdef PC_MODE
aBYTE open_eye_left[32*24*2];
aBYTE open_eye_right[32*24*2];
aBYTE close_eye_left[32*24];
aBYTE close_eye_right[32*24];
aBYTE st_nose[32*48*2];
#endif



char sInfo[] = "人脸跟踪-眨眼检测人脸定位处理插件";

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
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
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






/*******************************************************************/
//眨眼检测与眼睛定位插件
/*******************************************************************/

//self defined functions



//gauss filter func
DLL_EXP void gaussFilter(int w, int h, BYTE* grayBmp)
{
	int gauss_matrix[3][3] = {{1,2,1},{2,4,2},{1,2,1}};
    int divide_num = 16;
    
    BYTE* tempBmp = (BYTE*)malloc(w * h * sizeof(BYTE));
    if (tempBmp == NULL) {
		ShowDebugMessage("Error: mem alloc failure.");
    }
    memcpy(tempBmp, grayBmp, w * h * sizeof(BYTE));

    // apply Gauss filter
	int i,j,x,y;
    for (y = 1; y < h - 1; y++){
        for (x = 1; x < w - 1; x++){
            int sum = 0;
            for (j = -1; j <= 1; j++){
                for (i = -1; i <= 1; i++){
                    sum += grayBmp[(y + j) * w + (x + i)] * gauss_matrix[j + 1][i + 1];
                }
            }
            tempBmp[y * w + x] = (BYTE)(sum / divide_num);
        }
    }

    //copy back
    memcpy(grayBmp, tempBmp, w * h * sizeof(BYTE));
    free(tempBmp);
}

//update image sequence
DLL_EXP void updateHistoryImage(BUF_STRUCT* pBS)
{
	unsigned int img_size = pBS->W/4 * pBS->H/4;
	int offset = 4;
	BYTE* current_img = pBS->grayBmp_1d16;
	BYTE** img_pointer = pBS->pImageQueue;//0 to 7

	if(pBS->nImageQueueIndex != -1){ //old image
		pBS->nImageQueueIndex = (pBS->nImageQueueIndex+1)%8;
		pBS->nLastImageIndex = (pBS->nLastImageIndex+1)%8;
		memcpy(img_pointer[pBS->nImageQueueIndex], current_img, img_size);
	}else{ // new image
		pBS->nImageQueueIndex = 0;
		pBS->nLastImageIndex = 8-offset;
		int i = 0;
		for(i=0;i<8;i++){
			memcpy(img_pointer[i], current_img, img_size);
		}
		//ShowDebugMessage("here");
	}
}

//diff image
DLL_EXP void ImamgeDiff(BUF_STRUCT* pBS)
{
	BYTE* current_img = pBS->pImageQueue[pBS->nImageQueueIndex];
	BYTE* last_img = pBS->pImageQueue[pBS->nLastImageIndex];

	BYTE* diff_img = pBS->TempImage1d8;
	unsigned int img_size = pBS->W/4 * pBS->H/4;

	unsigned int i;
	for(i=0;i<img_size;i++){
		diff_img[i] = (last_img[i] - current_img[i] >= 0 ? last_img[i] - current_img[i] : 0);
	}

}

//calculate ret and convert to two-value image
DLL_EXP void retConvertDiffImg(BUF_STRUCT* pBS)
{
	unsigned int img_size = pBS->W/4 * pBS->H/4;
	unsigned int ret_count[256] = {0};
	BYTE* img = pBS->TempImage1d8;
	
	//count
	unsigned int i;
	for(i=0;i<img_size;i++) ret_count[img[i]]++;

	//calculate ret
	BYTE ret = 255;
	BYTE limit = 40; //between 20 and 60
	unsigned int sum = 0;
	do{
		sum += ret_count[ret];
		if(sum >= limit){break;}
		ret--;
	}while(ret>0);

	//reset value
	for(i=0;i<img_size;i++){img[i] = img[i]>ret ? 255 : 0;}

	//print debug message
	//ShowDebugMessage("ret:%d",ret);
}

//open operation
DLL_EXP void open_operation_3x1(BYTE* input, BYTE* output, int width, int height) 
{
    BYTE* temp = (unsigned char*)malloc(width * height);
	int x,y;
    
    // erode
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            unsigned char min_val = 255;
            for (int i = -1; i <= 1; i++) {
                int nx = x + i;
                if (nx >= 0 && nx < width) {
                    unsigned char pixel = input[y * width + nx];
                    if (pixel < min_val) {
                        min_val = pixel;
                    }
                }
            }
            temp[y * width + x] = min_val;
        }
    }
    
    // dilate
    for (y = 0; y < height; y++) {
        for (x = 0; x < width; x++) {
            unsigned char max_val = 0;
            for (int i = -1; i <= 1; i++) {
                int nx = x + i;
                if (nx >= 0 && nx < width) {
                    unsigned char pixel = temp[y * width + nx];
                    if (pixel > max_val) {
                        max_val = pixel;
                    }
                }
            }
            output[y * width + x] = max_val;
        }
    }
    
    // free temp
    free(temp);
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
DLL_EXP int markConnectedDomain(int w, int h, BYTE* pImg)
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

	return myclass;
}

//get size and center
DLL_EXP void getSizeAndCenterOfeachClass(int w, int h, BYTE* tempImg, int myclass, int* size, int* centerX, int* centerY, int* widthX, int* widthY)
{
	//init arrays
    int* pixelCount = (int*)malloc(myclass * sizeof(int));
    int* minX = (int*)malloc(myclass * sizeof(int));
    int* maxX = (int*)malloc(myclass * sizeof(int));
    int* minY = (int*)malloc(myclass * sizeof(int));
    int* maxY = (int*)malloc(myclass * sizeof(int));
	
	int i;
    for (i = 0; i < myclass; i++) {
        minX[i] = w - 1;
		minY[i] = h - 1;
        maxX[i] = maxY[i] = centerX[i] = centerY[i] = widthX[i] = widthY[i] = pixelCount[i] = 0;
    }

    // First pass: count pixels, sum up coordinates, and find bounding box
	int x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            int index = y * w + x;
            int label = tempImg[index];
            if (label > 0 && label < myclass) {
                pixelCount[label]++;
                centerX[label] += x;
                centerY[label] += y;
                
                // Update bounding box
                if (x < minX[label]) minX[label] = x;
                if (x > maxX[label]) maxX[label] = x;
                if (y < minY[label]) minY[label] = y;
                if (y > maxY[label]) maxY[label] = y;
            }
        }
    }

    // Second pass: calculate size and center coordinates
    for (i = 1; i < myclass; i++) {
        if (pixelCount[i] > 0) {
            // Calculate size as area of bounding box
 			widthX[i] = maxX[i] - minX[i] + 1;
			widthY[i] = maxY[i] - minY[i] + 1;
			size[i] = widthX[i]*widthX[i] + widthY[i]*widthY[i];
            
            // Calculate center coordinates
            centerX[i] /= pixelCount[i];
            centerY[i] /= pixelCount[i];
        } else {
            size[i] = 0;
        }
    }

    //free mem
    free(pixelCount);
    free(minX);
    free(maxX);
    free(minY);
    free(maxY);
}

//find eyes
DLL_EXP BOOL findEyes(int myclass, int* size, int* centerX, int* centerY, BUF_STRUCT* pBS)
{
	int i,j;
	BOOL flag = FALSE;
	for(i=1;i<myclass;i++){
		for(j=i+1;j<myclass;j++){
			if(abs(centerY[i] - centerY[j]) < 4 &&
				abs(centerX[i] - centerX[j]) > 15 &&
				abs(centerX[i] - centerX[j]) < 30 &&
				size[i] < 200 && size[j] < 200){

				//find the eyes
				//store
				if(centerX[i] < centerX[j]){
					pBS->ptTheLeftEye.x = centerX[i];
					pBS->ptTheLeftEye.y = centerY[i];
					pBS->ptTheRightEye.x = centerX[j];
					pBS->ptTheRightEye.y = centerY[j];
				}else{
					pBS->ptTheLeftEye.x = centerX[j];
					pBS->ptTheLeftEye.y = centerY[j];
					pBS->ptTheRightEye.x = centerX[i];
					pBS->ptTheRightEye.y = centerY[i];
				}
				//verify eyes
				pBS->ptTheLeftEye.x *= 4;
				pBS->ptTheLeftEye.y *= 4;
				pBS->ptTheRightEye.x *= 4;
				pBS->ptTheRightEye.y *= 4;

				int nEyeDist = pBS->ptTheRightEye.x - pBS->ptTheLeftEye.x;
				int nEyeWidth = nEyeDist * 2/3;
				int nEyeHeight = nEyeDist/2;

				//verify eye size
				if(nEyeWidth > pBS->W/4 || nEyeHeight > pBS->H/4){
					ShowDebugMessage("eye size too large!");
					flag = FALSE;
					continue;
				}

				//verify eye position
				if(pBS->ptTheLeftEye.x < 0 || pBS->ptTheLeftEye.y < 0 || pBS->ptTheRightEye.x < 0 || pBS->ptTheRightEye.y < 0
				|| pBS->ptTheLeftEye.x > pBS->W || pBS->ptTheLeftEye.y > pBS->H || pBS->ptTheRightEye.x > pBS->W || pBS->ptTheRightEye.y > pBS->H){
					ShowDebugMessage("eye position error!");
					flag = FALSE;
					continue;
				}
				
				// ShowDebugMessage("eye1: %d, %d; eye2: %d, %d", centerX[i], centerY[i], centerX[j], centerY[j]);
				//set true
				// ShowDebugMessage("find eyes!");
				flag = TRUE;
			}
			if(flag) break;
		}
		if(flag) break;
	}
	pBS->EyePosConfirm = flag;//set flag
	return flag;
}

//copy the area to temp memory
DLL_EXP void copyImgAreaToMem(BYTE* source, 
								int source_w, int source_h, 
								int source_area_left, int source_area_top, int source_area_width, int source_area_height,
								BYTE* dest,
								int dest_w, int dest_h,
								bool bGray)
{
	//temp mem
	BYTE* temp;
	if(bGray){
		temp = (BYTE*)malloc(source_area_width * source_area_height);
	}else{
		temp = (BYTE*)malloc(source_area_width * source_area_height * 2);
	}
	//copy to temp mem, Y vector
	int i,j;
	for(i=0;i<source_area_height;i++){
		for(j=0;j<source_area_width;j++){
			temp[i*source_area_width+j] = source[(source_area_top+i)*source_w+source_area_left+j];
		}
	}
	//copy UV vector
	if(!bGray){
		BYTE* tempU = temp + source_area_width * source_area_height;
		BYTE* tempV = tempU + source_area_width * source_area_height / 2;
		BYTE* source_U = source + source_w * source_h;
		BYTE* source_V = source_U + source_w * source_h / 2;
		for(i=0;i<source_area_height;i++){
			for(j=0;j<source_area_width/2;j++){
				tempU[i*source_area_width/2+j] = source_U[(source_area_top+i)*source_w/2+source_area_left/2+j];
				tempV[i*source_area_width/2+j] = source_V[(source_area_top+i)*source_w/2+source_area_left/2+j];
			}
		}
	}
	//resample to dest
	ReSample(temp, source_area_width, source_area_height, dest_w, dest_h, false, bGray, dest);
	//free mem
	free(temp);
}

DLL_EXP BOOL checkCalcStatus(BUF_STRUCT* pBS) //check if the calc is needed
{
	if(!pBS->EyePosConfirm){
		return false; //if not confirmed or previous calc failed, return
	}
	TRACE_OBJECT *nose_obj, *leye_obj, *reye_obj;
	nose_obj = &(pBS->pOtherVars->objNose);
	leye_obj = &(pBS->pOtherVars->objLefteye);
	reye_obj = &(pBS->pOtherVars->objRighteye);
	if(pBS->bLastEyeChecked && !nose_obj->bBrokenTrace && !leye_obj->bBrokenTrace && !reye_obj->bBrokenTrace){
		return false; //if the last eye checked and the trace is not broken, there is no need to calc again
	}
	return true; //then calc
}

DLL_EXP void copyAndResampleEyeNosePic(BUF_STRUCT* pBS, BYTE* _lefteyeOpen, BYTE* _righteyeOpen, BYTE* _stnose)
{
	//check
	if(!checkCalcStatus(pBS)){
		return;
	}

	TRACE_OBJECT *nose_obj, *leye_obj, *reye_obj;
	nose_obj = &(pBS->pOtherVars->objNose);
	leye_obj = &(pBS->pOtherVars->objLefteye);
	reye_obj = &(pBS->pOtherVars->objRighteye);

	//step1: creat eye and nose object, copy
	int nEyeDist = pBS->ptTheRightEye.x - pBS->ptTheLeftEye.x;
	int nEyeWidth = nEyeDist * 2/3;
	int nEyeHeight = nEyeDist/2;

	int nose_center_x = (pBS->ptTheLeftEye.x + pBS->ptTheRightEye.x) / 2;
	int nose_center_y = (pBS->ptTheLeftEye.y + pBS->ptTheRightEye.y) / 2 - nEyeDist / 2;
	int nose_width = nEyeDist * 3/4;
	int nose_height = nEyeDist;

	//make width and height multiple of 4
	nEyeWidth = ((int)(nEyeWidth/4))*4;
	nEyeHeight = ((int)(nEyeHeight/4))*4;
	nose_width = ((int)(nose_width/4))*4;
	nose_height = ((int)(nose_height/4))*4;


	nose_obj->rcObject.left = nose_center_x - nose_width/2;
	nose_obj->rcObject.top = nose_center_y + nose_height/2;
	nose_obj->rcObject.width = nose_width;
	nose_obj->rcObject.height = nose_height;

	leye_obj->rcObject.left = pBS->ptTheLeftEye.x - nEyeWidth/2;
	leye_obj->rcObject.top = pBS->ptTheLeftEye.y - nEyeHeight/2;
	leye_obj->rcObject.width = nEyeWidth;
	leye_obj->rcObject.height = nEyeHeight;

	reye_obj->rcObject.left = pBS->ptTheRightEye.x - nEyeWidth/2;
	reye_obj->rcObject.top = pBS->ptTheRightEye.y - nEyeHeight/2;
	reye_obj->rcObject.width = nEyeWidth;
	reye_obj->rcObject.height = nEyeHeight;

	//step2: resample and copy
	copyImgAreaToMem(pBS->colorBmp, pBS->W, pBS->H, leye_obj->rcObject.left, leye_obj->rcObject.top, 
					nEyeWidth, nEyeHeight, _lefteyeOpen, 32, 24, false);
	copyImgAreaToMem(pBS->colorBmp, pBS->W, pBS->H, reye_obj->rcObject.left, reye_obj->rcObject.top,
					nEyeWidth, nEyeHeight, _righteyeOpen, 32, 24, false);
	copyImgAreaToMem(pBS->colorBmp, pBS->W, pBS->H, nose_obj->rcObject.left, nose_obj->rcObject.top,
					nose_width, nose_height, _stnose, 32, 48, false);
	
	return;
}

//eye color and pos check
DLL_EXP BOOL checkEyeClrAndPos(BUF_STRUCT* pBS, BYTE* _lefteyeOpen, BYTE* _righteyeOpen)
{
	//check
	if(!checkCalcStatus(pBS)){
		pBS->EyePosConfirm = false; //set again in case of previous calc failed
		return false;
	}
	ShowDebugMessage("checkEyeClrAndPos");

	//step1: check eye color
	BYTE* leye_U = _lefteyeOpen + 32 * 24;
	BYTE* leye_V = _lefteyeOpen + 32 * 24 * 3/2;
	BYTE* reye_U = _righteyeOpen + 32 * 24;
	BYTE* reye_V = _righteyeOpen + 32 * 24 * 3/2;

	int face_count_left, face_count_right, eye_count_left, eye_count_right;
	int eye_pixel_sum_x_left, eye_pixel_sum_y_left, eye_pixel_sum_x_right, eye_pixel_sum_y_right;
	face_count_left = face_count_right = eye_count_left = eye_count_right = 0;
	eye_pixel_sum_x_left = eye_pixel_sum_y_left = eye_pixel_sum_x_right = eye_pixel_sum_y_right = 0;

	aBYTE* histmapU = pBS->pOtherVars->byHistMap_U;
	aBYTE* histmapV = pBS->pOtherVars->byHistMap_V;

	int i,j;
	for(j=0;j<24;j++){ //count pixels
		for(i=0;i<16;i++){
			if(leye_U[j*16+i] >= 115 && leye_U[j*16+i] <= 131 && leye_V[j*16+i] >= 121 && leye_V[j*16+i] <= 140){
				eye_count_left++;
				eye_pixel_sum_x_left += 2*i;
				eye_pixel_sum_y_left += j;
			}
			if(histmapU[leye_U[j*16+i]] && histmapV[leye_V[j*16+i]]){
				face_count_left++;
			}

			if(reye_U[j*16+i] >= 115 && reye_U[j*16+i] <= 131 && reye_V[j*16+i] >= 121 && reye_V[j*16+i] <= 140){
				eye_count_right++;
				eye_pixel_sum_x_right += 2*i;
				eye_pixel_sum_y_right += j;
			}
			if(histmapU[reye_U[j*16+i]] && histmapV[reye_V[j*16+i]]){
				face_count_right++;
			}
		}
	}
	// ShowDebugMessage("eye count: %d, %d", eye_count_left, eye_count_right);
	// ShowDebugMessage("face count: %d, %d", face_count_left, face_count_right);

	//check num
	bool check_flag = false;
	if(face_count_left >= 200 && face_count_right >= 200 && 
		eye_count_left >= 10 && eye_count_left <= 60 &&
		eye_count_right >= 10 && eye_count_right <= 60){
			check_flag = true;
	}else{
		check_flag = false;
		pBS->EyePosConfirm = false;
		return false;
	}
	ShowDebugMessage("check face and eye num pass");

	//calculate centers
	int eye_bias_x_left, eye_bias_y_left, eye_bias_x_right, eye_bias_y_right;
	eye_bias_x_left = eye_pixel_sum_x_left / eye_count_left;
	eye_bias_y_left = eye_pixel_sum_y_left / eye_count_left;
	eye_bias_x_right = eye_pixel_sum_x_right / eye_count_right;
	eye_bias_y_right = eye_pixel_sum_y_right / eye_count_right;

	TRACE_OBJECT *leye_obj, *reye_obj;
	leye_obj = &(pBS->pOtherVars->objLefteye);
	reye_obj = &(pBS->pOtherVars->objRighteye);

	aPOINT ptLeftEye_cfm, ptRightEye_cfm;
	ptLeftEye_cfm.x = leye_obj->rcObject.left + eye_bias_x_left;
	ptLeftEye_cfm.y = leye_obj->rcObject.top + eye_bias_y_left;
	ptRightEye_cfm.x = reye_obj->rcObject.left + eye_bias_x_right;
	ptRightEye_cfm.y = reye_obj->rcObject.top + eye_bias_y_right;

	//step2: check eye position
	BYTE* clrBmp_1d8 = pBS->clrBmp_1d8;
	BYTE* clrBmp_1d8_U = clrBmp_1d8 + pBS->W/2 * pBS->H/4;
	BYTE* clrBmp_1d8_V = clrBmp_1d8_U + pBS->W/4 * pBS->H/4;
	aRect* face_area_1d8 = &(pBS->rcnFace);

	int eyes_center_y = (ptLeftEye_cfm.y + ptRightEye_cfm.y) / 2;

	int face_left = 2*face_area_1d8->left;
	int face_top = 4*face_area_1d8->top;
	int face_width = 2*face_area_1d8->width;
	int face_height = 4*face_area_1d8->height;
	
	int face_right = face_left + face_width;
	int face_bottom = face_top + face_height;

	check_flag = false;
	if(ptLeftEye_cfm.x >= face_left && ptLeftEye_cfm.x <= face_right && 
		ptLeftEye_cfm.y >= face_top && ptLeftEye_cfm.y <= face_bottom &&
		ptRightEye_cfm.x >= face_left && ptRightEye_cfm.x <= face_right && 
		ptRightEye_cfm.y >= face_top && ptRightEye_cfm.y <= face_bottom &&
		ptLeftEye_cfm.x < ptRightEye_cfm.x){
			check_flag = true;
	}else{
		check_flag = false;
		pBS->EyePosConfirm = false;
		return false;
	}
	ShowDebugMessage("check eye position pass");
	
	return true;
}


//morphological operation
DLL_EXP void morphological(int w, int h, BUF_STRUCT* pBS, BYTE* tempImg)
{
	//open operation
	open_operation_3x1(pBS->TempImage1d8, tempImg, w, h);
	//maek connected domain(the function is the same as in FaceLocator but returns the class)
	int myclass = markConnectedDomain(w, h, tempImg);
	//calculate the size and center of each domain
	int* size = (int*)malloc(myclass * sizeof(int));
	int* centerX = (int*)malloc(myclass * sizeof(int));
	int* centerY = (int*)malloc(myclass * sizeof(int));
	int* widthX = (int*)malloc(myclass * sizeof(int));
	int* widthY = (int*)malloc(myclass * sizeof(int));
	getSizeAndCenterOfeachClass(w, h, tempImg, myclass, size, centerX, centerY, widthX, widthY);
	BOOL flag = findEyes(myclass, size, centerX, centerY, pBS);
	//free mem
	free(size);
	free(centerX);
	free(centerY);
	free(widthX);
	free(widthY);

	//draw eyes
	COLORREF clr = TYUV1(250,250,0);
	if(flag){
		// ShowDebugMessage("left: %d, %d, right: %d, %d", pBS->ptTheLeftEye.x, pBS->ptTheLeftEye.y, pBS->ptTheRightEye.x, pBS->ptTheRightEye.y);
	}
		DrawCross(pBS->displayImage, pBS->W, pBS->H, pBS->ptTheLeftEye.x, pBS->ptTheLeftEye.y, 10, clr, FALSE); 
		DrawCross(pBS->displayImage, pBS->W, pBS->H, pBS->ptTheRightEye.x, pBS->ptTheRightEye.y, 10, clr, FALSE);


}

DLL_EXP void verifyingEyes(BUF_STRUCT* pBS)
{
	//malloc memory
	BYTE* _lefteyeOpen = (BYTE*)malloc(32*24*2);
	BYTE* _righteyeOpen = (BYTE*)malloc(32*24*2);
	BYTE* _stnose = (BYTE*)malloc(32*48*2);

	bool flag = true;
	//copy and resample
	copyAndResampleEyeNosePic(pBS, _lefteyeOpen, _righteyeOpen, _stnose);
	//eye color and position check
	flag = checkEyeClrAndPos(pBS, _lefteyeOpen, _righteyeOpen);

	//test
	if(flag){
		ShowDebugMessage("eye verification passed!");
	}
	if(bLastPlugin){
		CopyToRect(_lefteyeOpen, pBS->displayImage, 32, 24, pBS->W, pBS->H, 0, 0, false);
		CopyToRect(_righteyeOpen, pBS->displayImage, 32, 24, pBS->W, pBS->H, 0, 24, false);
		CopyToRect(_stnose, pBS->displayImage, 32, 48, pBS->W, pBS->H, 0, 48, false);
	}

	//mem free
	free(_lefteyeOpen);
	free(_righteyeOpen);
	free(_stnose);
	return;
}

DLL_EXP void calcGrayFourCornerCentroid(BYTE* pImg, int imgW, int imgH, 
										int left, int top, int width, int height,
										aPOINT* Vector)
{
	ShowDebugMessage("calcGrayFourCornerCentroid");
	//step1
	int half_width = width/2;
	int half_height = height/2;
	int* SumX_0 = (int*)malloc(height * sizeof(int));
	int* SumX_1 = (int*)malloc(height * sizeof(int));
	int* SumX_2 = (int*)malloc(height * sizeof(int));
	int* SumX_3 = (int*)malloc(height * sizeof(int));
	int* SumI_0 = (int*)malloc(height * sizeof(int));
	int* SumI_1 = (int*)malloc(height * sizeof(int));
	int* SumI_2 = (int*)malloc(height * sizeof(int));
	int* SumI_3 = (int*)malloc(height * sizeof(int));

	//initialize
	int i;
	for(i=0;i<height;i++){
		SumX_0[i] = SumX_1[i] = SumX_2[i] = SumX_3[i] = 0;
		SumI_0[i] = SumI_1[i] = SumI_2[i] = SumI_3[i] = 0;
	}

	int j;
	for(j=top;j<top+height;j++){
		for(i=left;i<left+width/4;i++){
			SumX_0[j-top] += i * pImg[j*imgW+i];
			SumI_0[j-top] += pImg[j*imgW+i];
			ShowDebugMessage("SumX_0[%d]: %d, SumI_0[%d]: %d", j-top, SumX_0[j-top], j-top, SumI_0[j-top]);
		}
		for(i=left+width/4;i<left+width/2;i++){
			SumX_1[j-top] += i * pImg[j*imgW+i];
			SumI_1[j-top] += pImg[j*imgW+i];
		}
		for(i=left+width/2;i<left+width*3/4;i++){
			SumX_2[j-top] += i * pImg[j*imgW+i];
			SumI_2[j-top] += pImg[j*imgW+i];
		}
		for(i=left+width*3/4;i<left+width;i++){
			SumX_3[j-top] += i * pImg[j*imgW+i];
			SumI_3[j-top] += pImg[j*imgW+i];
		}
	}
	// ShowDebugMessage("step1 done");
	ShowDebugMessage("SumX_0[0]: %d, SumX_1[0]: %d, SumX_2[0]: %d, SumX_3[0]: %d", SumX_0[0], SumX_1[0], SumX_2[0], SumX_3[0]);

	//step2
	int X0, X1, X2, X3, Y0, Y1, Y2, Y3;
	int up_tmp_x, up_tmp_y, down_tmp, y_tmp;
	up_tmp_x = up_tmp_y = down_tmp = 0;
	X0 = X1 = X2 = X3 = Y0 = Y1 = Y2 = Y3 = 0;
	for(j=0;j<half_height;j++){
		y_tmp = top + j;
		up_tmp_x += SumX_0[j] + SumX_1[j];
		up_tmp_y += SumX_0[j]*y_tmp + SumX_1[j]*y_tmp;
		down_tmp += SumI_0[j] + SumI_1[j];
	}
	if(down_tmp != 0){
		X0 = up_tmp_x / down_tmp;
		Y0 = up_tmp_y / down_tmp;
	}
	up_tmp_x = up_tmp_y = down_tmp = 0;
	for(j=0;j<half_height;j++){
		y_tmp = top + j;
		up_tmp_x += SumX_2[j] + SumX_3[j];
		up_tmp_y += SumX_2[j]*y_tmp + SumX_3[j]*y_tmp;
		down_tmp += SumI_2[j] + SumI_3[j];
	}
	if(down_tmp != 0){
		X1 = up_tmp_x / down_tmp;
		Y1 = up_tmp_y / down_tmp;
	}
	up_tmp_x = up_tmp_y = down_tmp = 0;
	for(j=half_height;j<height;j++){
		y_tmp = top + j;
		up_tmp_x += SumX_0[j] + SumX_1[j];
		up_tmp_y += SumX_0[j]*y_tmp + SumX_1[j]*y_tmp;
		down_tmp += SumI_0[j] + SumI_1[j];
	}
	if(down_tmp != 0){
		X2 = up_tmp_x / down_tmp;
		Y2 = up_tmp_y / down_tmp;
	}
	up_tmp_x = up_tmp_y = down_tmp = 0;
	for(j=half_height;j<height;j++){
		y_tmp = top + j;
		up_tmp_x += SumX_2[j] + SumX_3[j];
		up_tmp_y += SumX_2[j]*y_tmp + SumX_3[j]*y_tmp;
		down_tmp += SumI_2[j] + SumI_3[j];
	}
	if(down_tmp != 0){
		X3 = up_tmp_x / down_tmp;
		Y3 = up_tmp_y / down_tmp;
	}
	// ShowDebugMessage("step2 done");
	ShowDebugMessage("X0: %d, Y0: %d, X1: %d, Y1: %d, X2: %d, Y2: %d, X3: %d, Y3: %d", X0, Y0, X1, Y1, X2, Y2, X3, Y3);

	//step3
	X0 = X0 * 1024 / half_width;
	Y0 = Y0 * 1024 / half_height;
	X1 = X1 * 1024 / half_width;
	Y1 = Y1 * 1024 / half_height;
	X2 = X2 * 1024 / half_width;
	Y2 = Y2 * 1024 / half_height;
	X3 = X3 * 1024 / half_width;
	Y3 = Y3 * 1024 / half_height;
	ShowDebugMessage("step3 done");

	//save
	Vector[0].x = X0;
	Vector[0].y = Y0;
	Vector[1].x = X1;
	Vector[1].y = Y1;
	Vector[2].x = X2;
	Vector[2].y = Y2;
	Vector[3].x = X3;
	Vector[3].y = Y3;
	ShowDebugMessage("save done");

	//free mem
	free(SumX_0);
	free(SumX_1);
	free(SumX_2);
	free(SumX_3);
	free(SumI_0);
	free(SumI_1);
	free(SumI_2);
	free(SumI_3);
}

DLL_EXP void calcTwoLevelCentroid(BUF_STRUCT* pBS, TRACE_OBJECT* obj)
{
	BYTE* grayBmp = pBS->grayBmp;
	FeatureVector4P* fv = (FeatureVector4P*)(obj->fvObject.Vector);
	FeatureVector4P* fvLT = fv + 1;
	FeatureVector4P* fvRT = fv + 2;
	FeatureVector4P* fvLB = fv + 3;
	FeatureVector4P* fvRB = fv + 4;
	calcGrayFourCornerCentroid(grayBmp, pBS->W, pBS->H,
								obj->rcObject.left, obj->rcObject.top,
								obj->rcObject.width, obj->rcObject.height,
								fv->Vector);
	calcGrayFourCornerCentroid(grayBmp, pBS->W, pBS->H,
								obj->rcObject.left, obj->rcObject.top,
								obj->rcObject.width/2, obj->rcObject.height/2,
								fvLT->Vector);
	calcGrayFourCornerCentroid(grayBmp, pBS->W, pBS->H,
								obj->rcObject.left+obj->rcObject.width/2, obj->rcObject.top,
								obj->rcObject.width/2, obj->rcObject.height/2,
								fvRT->Vector);
	calcGrayFourCornerCentroid(grayBmp, pBS->W, pBS->H,
								obj->rcObject.left, obj->rcObject.top+obj->rcObject.height/2,
								obj->rcObject.width/2, obj->rcObject.height/2,
								fvLB->Vector);
	calcGrayFourCornerCentroid(grayBmp, pBS->W, pBS->H,
								obj->rcObject.left+obj->rcObject.width/2, obj->rcObject.top+obj->rcObject.height/2,
								obj->rcObject.width/2, obj->rcObject.height/2,
								fvRB->Vector);

	fv->nLevels = 2;
	fvLT->nLevels = fvRT->nLevels = fvLB->nLevels = fvRB->nLevels = 1;
	fv->pNL_LeftTop = fvLT;
	fv->pNL_RightTop = fvRT;
	fv->pNL_LeftBottom = fvLB;
	fv->pNL_RightBottom = fvRB;
}

DLL_EXP void pickObjFeature(BUF_STRUCT* pBS)
{
	// check
	if(!checkCalcStatus(pBS)){
		return;
	}
	ShowDebugMessage("pickObjFeature");

	TRACE_OBJECT *nose_obj, *leye_obj, *reye_obj;
	nose_obj = &(pBS->pOtherVars->objNose);
	leye_obj = &(pBS->pOtherVars->objLefteye);
	reye_obj = &(pBS->pOtherVars->objRighteye);

	calcTwoLevelCentroid(pBS, leye_obj);
	calcTwoLevelCentroid(pBS, reye_obj);
	calcTwoLevelCentroid(pBS, nose_obj);

	return;
}

//end of self defined functions

DLL_EXP void ON_PLUGINRUN(int w,int h,BYTE* pYBits,BYTE* pUBits,BYTE* pVBits,BYTE* pBuffer)
{
//pYBits 大小为w*h
//pUBits 和 pVBits 的大小为 w*h/2
//pBuffer 的大小为 w*h*4
//下面算法都基于一个假设， 即w是16的倍数
    AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	 //请编写相应处理程序

	//start
	BUF_STRUCT* pBS = ((BUF_STRUCT*)pBuffer);
	//gauss filter
	gaussFilter(w/4,h/4,pBS->grayBmp_1d16);
	//copy to history
	updateHistoryImage(pBS);
	//calculate diff image
	ImamgeDiff(pBS);
	//calculate ret and convert diff
	retConvertDiffImg(pBS);
	//morphological operation
	BYTE* tempImg = myHeapAlloc(w*h/16);
	morphological(w/4, h/4, pBS, tempImg);
	//verifying eyes
	verifyingEyes(pBS);
	//pick obj feature
	pickObjFeature(pBS);

	//free mem
	myHeapFree(tempImg);

	//TEST ONLY
	if( bLastPlugin ){
		// CopyToRect(tempImg, pYBits, w/4, h/4, w, h, 0, 0, true);
		//pBS->TempImage1d8
	}


}
/*******************************************************************/

/*******************************************************************/              
DLL_EXP void ON_PLUGINEXIT()
{
   AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//theApp.dlg.DestroyWindow();
}

