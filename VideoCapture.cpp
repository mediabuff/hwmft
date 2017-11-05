#include "stdafx.h"
// stl headers
#include <algorithm>
// local headers
#include "GlobalFun.h"
#include "VideoCapture.h"
#include "MFMediaBufferHelper.h"
#include "NativeMediaLogger4CPP.h"
#include "KsClient.h"

#define INITGUID
#include <devpkey.h>
#include "DevSetupInfo.h"

namespace WinRTCSDK{

using namespace Native;

VideoCapture::VideoCapture(IVideoCaptureCallback *callback, 
						   std::shared_ptr<D3D9Renderer> pRenderer, 
						   VideoCaptureOrdinal ordinal):
	pVidCapDev_(NULL),
	isUsingDshow_(false),	
	pRenderer_(pRenderer),
	nRefCount_(1),
	callback_(callback),
	maxCapability_(1920*2, 1080*2, 60, false),
	ordinal_(ordinal),
	symbolicLink_ (),
	lock_(),
	defaultStride_(0),
	format_(),
	dispatcher_("CameraRunner", true),
	cameraRunningFlag_(false),
	dispatchMFSample_(false),
	cameraControl_(),
	bFlipPreview_(FALSE)
{
}

VideoCapture::~VideoCapture(void)
{
	// must ensure the dispatching queue is cleared before dtors called.
	dispatcher_.stopAndJoin();
}

HRESULT VideoCapture::Choose(std::wstring symbolicLink)
{
	// same device
	if (symbolicLink_ == symbolicLink) return S_OK;

	// try MediaFoundation first
	isUsingDshow_ = true;

	symbolicLink_ = symbolicLink;
	if (symbolicLink_.empty()) 
		return E_INVALIDARG;

	return Reset();
}

// if camera runs into error state, we will try to restart it (the video manager)
HRESULT VideoCapture::Reset()
{
	// check if running
	if ( !cameraRunningFlag_ ) return S_OK;
	// camera is running, need to restart!
	Close();
	return Open();
}

HRESULT VideoCapture::_OpenAndSetFormat ()
{
	if (isUsingDshow_){
		pVidCapDev_ = new DShowVidCapture(this);
	}else{
		pVidCapDev_ = new MFVidCapture(this);
	}

	HRESULT hr = pVidCapDev_->OpenDev(symbolicLink_);
	if (FAILED(hr)) {
		NativeMediaLogger4CPP::logW(Error, L"Camera", L"Failed to open capture dev hr=%x",hr);
		return hr; 
	}

	// setup output format
	hr = _SetFormat();
	if (FAILED(hr)) { 
		NativeMediaLogger4CPP::logW(Error, L"Camera", L"Failed to _SetFormat hr=%x",hr);
		return hr; 
	}
}

HRESULT VideoCapture::Open()
{
	HRESULT hr = S_OK;
	AutoLock l (lock_);
	if ( cameraRunningFlag_ == true) return S_OK;

	// retrieve D3DManager from render
	pRenderer_->Clear(0,0,0); // clear the frame

	std::vector<MediaDevInfo>  devList;
	// update current device list
	hr = VideoCapture::GetDevList(devList);

	// check if the specified device exists.
	auto result = std::find_if(devList.begin(), devList.end(), 
		[=](MediaDevInfo &info)->bool{ return info.symbolicLink == symbolicLink_;} );
	if ( result == devList.end() ){
		NativeMediaLogger4CPP::logW(Error, L"Camera", L"device doesn't exist:%s", symbolicLink_.c_str());
		return E_INVALIDARG;
	}

	// log current device
	NativeMediaLogger4CPP::logW(Info, L"Camera", L"Current Camera: %s", result->devFriendlyName.c_str());
	NativeMediaLogger4CPP::logW(Info, L"Camera", L"Current Camera ID: %s", result->symbolicLink.c_str());

	// create source reader from media source
	hr = _OpenAndSetFormat();
	if (FAILED(hr)){
		if (pVidCapDev_){
			pVidCapDev_->CloseDev();
			delete pVidCapDev_;
			pVidCapDev_ = nullptr;
		}

		// fallback to dshow
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"failed to open MFSourceReader, will try dshow pipeline.");
		isUsingDshow_ = true;
		hr = _OpenAndSetFormat();
	}
	
	// failed to open device
	if (FAILED(hr)){
		if (pVidCapDev_){
			pVidCapDev_->CloseDev();
			delete pVidCapDev_;
			pVidCapDev_ = nullptr;
		}
		return hr;
	}

	// get default stride
	pVidCapDev_->GetDefaultStride(defaultStride_);

	// also initialize the camera control
	pVidCapDev_->InitCameraControl(cameraControl_);
	
	// start the handler thread
	firstSample_ = true;
	cameraRunningFlag_ = true;
	dispatcher_.startUp(); // ensure dispatcher thread
	
	// start the capture dev
	hr = pVidCapDev_->Start();

	// statistics
	frameCnt_ = 0;
	LARGE_INTEGER li;
	::QueryPerformanceCounter(&li);
	startTimeQPC_ = li.QuadPart;

	return S_OK;
}

HRESULT VideoCapture::Close()
{
	AutoLock l (lock_);

	if ( cameraRunningFlag_ == false) return S_OK;
	if(pVidCapDev_==NULL) return S_OK;

	pVidCapDev_->Stop();
	// shut down internal thread !
	cameraRunningFlag_ = false;
	dispatcher_.stopAndJoin();

	// destroy camera control 
	cameraControl_.Destroy();
	// destroy the capture device
	pVidCapDev_->CloseDev();
	delete pVidCapDev_;
	pVidCapDev_ =NULL;

	return S_OK;
}
	
VOID VideoCapture::SetMaxCapability(VideoCaptureCapbaility cap)
{
	maxCapability_ = cap;
	// restart camera if needed
	Reset();
}
	
VOID VideoCapture::SetLocalPreviewFlipping(BOOL bFlip)
{
	bFlipPreview_ = bFlip;
}

VOID VideoCapture::GetStatistics(uint64_t& frameCnt, double&fps)
{
	LARGE_INTEGER liCurrent;
	LARGE_INTEGER freq;
	::QueryPerformanceCounter(&liCurrent);
	::QueryPerformanceFrequency(&freq);

	frameCnt = frameCnt_;
	double ms_elapsed =  (liCurrent.QuadPart - startTimeQPC_) * 1000 / freq.QuadPart;
	fps = (frameCnt)*1.0 / ms_elapsed * 1000.0;
}


static D3DFORMAT __MFMediaTypeToD3DFMT(GUID& type)
{
	if ( type == MFVideoFormat_RGB24) return D3DFMT_R8G8B8;
	if ( type == MFVideoFormat_NV12) return D3DFMT_NV12;
	if ( type == MFVideoFormat_YUY2) return D3DFMT_YUY2;
	if ( type == MFVideoFormat_I420) return D3DFMT_I420;
	return D3DFMT_UNKNOWN;
}

HRESULT VideoCapture::_HandleCameraSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
						LONGLONG llTimestamp, IMFSample * pSample)
{
	HRESULT hr=S_OK;
	DWORD   cnt=0;
	_com_ptr_IMFMediaBuffer pBuffer = NULL;

	if ( FAILED(hrStatus)){
		if (pSample ){
			pSample->Release();//we have increased ref for async processing
		}
		lastError_ = hrStatus;
		NativeMediaLogger4CPP::logW(Error, L"Camera", L"failed to read sample: hr=%x", lastError_);
		// we got failure in MediaFoundation pipeline, lets fallback to dshow pipeline
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"failed to read sample: will try dshow pipeline.");
		isUsingDshow_ = true;// keep ture until next ChooseDev call
		// fire an event to callbacks
		callback_->OnVideoCaptureError(hrStatus, ordinal_);
		return hrStatus;
	}

	//// UT
	//if(frameCnt_ > 0 && (frameCnt_ % 100) ==0){
	//	callback_->OnVideoCaptureError(0, ordinal_);
	//}

	// Async read the next frame
	if ( cameraRunningFlag_ ){	
		pVidCapDev_->AsyncRead();
	}

	if(pSample==NULL) return S_OK;

	// Get the video frame buffer from the sample.
	hr = pSample->GetBufferCount(&cnt);
	if ( cnt ==0 ){
		pSample->Release();
		return S_OK;
	}

	hr = pSample->GetBufferByIndex(0, &pBuffer);
	// output the frame.
	if (SUCCEEDED(hr))   
	{
		// statistics
		frameCnt_++;

		// video frame to dispatch
		VideoFrameInfo frame;

		frame.bFlipHorizontally = bFlipPreview_;

		// lock the media buffer for data accessing.
		MediaBufferLock bufferLock(pBuffer);
		_com_ptr_IDirect3DSurface9 pSurface = bufferLock.GetD3DSurface();
		if ( pSurface != NULL )
		{
			D3DSURFACE_DESC src_surf_desc;
			pSurface->GetDesc(&src_surf_desc);
			frame.format = src_surf_desc.Format;
			frame.height = src_surf_desc.Height;
			frame.width = src_surf_desc.Width;
			frame.pSample = pSurface;
		}
		else // Currently we only disable DXVA capture for RGB24 format
		{
			BYTE  *pbScanLine0 = NULL;
			LONG  lStride = 0;
			hr = bufferLock.LockBuffer(defaultStride_, &pbScanLine0, &lStride, format_.height, TRUE/*try 2d buffer*/);
			frame.image = pbScanLine0;
			frame.width = format_.width;
			frame.height = format_.height;
			frame.stride = lStride;
			frame.format = __MFMediaTypeToD3DFMT(format_.subType4MF);
		}

		bool isFirstFrameLocal = firstSample_;
		// log for first sample
		if ( firstSample_){
			firstSample_ = false;
			NativeMediaLogger4CPP::logW(Info, L"Camera", L"read first sample : width = %d, height = %d", 
				frame.width, frame.height);
		}

		// draw preview
		pRenderer_->DrawVideo(frame);

		if ( dispatchMFSample_) //dispatch IMFSample
		{
		}
		else 
		{
			CaptureFrameInfo capFrame;
			capFrame.format = frame.format;
			capFrame.height = frame.height;
			capFrame.width = frame.width;
			capFrame.stride = frame.stride;
			capFrame.pSample = NULL;
			capFrame.pImage = frame.image;
            callback_->OnVideoCaptureData(capFrame, ordinal_, isFirstFrameLocal);
		}

	}

	pBuffer = NULL; // force free the pointer.
	pSample->Release(); //we have increased ref for async processing

	return S_OK;
}

// IMFSourceReaderCallback methods
STDMETHODIMP VideoCapture::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
	LONGLONG llTimestamp, IMFSample *pSample )
{
	if (pSample!=NULL){
		pSample->AddRef(); // increase ref to switch thread
	}
	LooperTask t = [=]()
	{ 
		_HandleCameraSample(hrStatus, dwStreamIndex, dwStreamFlags, llTimestamp, pSample); 
	};

	dispatcher_.runAsyncTask(t); 
	
	return S_OK;
}

STDMETHODIMP VideoCapture::OnEvent(DWORD, IMFMediaEvent *) 
{
    return S_OK;
}
STDMETHODIMP VideoCapture::OnFlush(DWORD)
{
    return S_OK;
}
// IMFSourceReaderCallback methods end

static D3DFORMAT _AM_Media_SubTypeToD3DFORMAT(const GUID subType)
{
	if (subType== MEDIASUBTYPE_YUY2 ||	
		subType== MEDIASUBTYPE_YUYV) return D3DFMT_YUY2;

	 if (subType== MEDIASUBTYPE_IYUV ||
		subType== MEDIASUBTYPE_I420 ||
		subType== MEDIASUBTYPE_YV12 ) return D3DFMT_I420;

	 if ( subType== MEDIASUBTYPE_NV12) return D3DFMT_NV12;

	 if  ( subType== MEDIASUBTYPE_RGB24) return D3DFMT_R8G8B8;

	 return D3DFMT_UNKNOWN;
}

HRESULT VideoCapture::_HandleMediaSample(IMediaSample* pSample, CMediaType* pMediaType)
{
	BYTE * bufferSample = NULL;
	LONG nSize = -1;
	pSample->GetPointer(&bufferSample);
	nSize=pSample->GetActualDataLength();

	VideoFrameInfo frame;
	frame.format = _AM_Media_SubTypeToD3DFORMAT (pMediaType->subtype);
	VIDEOINFO * vidInfo = (VIDEOINFO *)pMediaType->pbFormat;
	frame.width = vidInfo->bmiHeader.biWidth;
	frame.height = ::abs(vidInfo->bmiHeader.biHeight);
	LONG stride;
	MFGetStrideForBitmapInfoHeader(pMediaType->subtype.Data1, vidInfo->bmiHeader.biWidth, &stride);
	frame.stride = stride;
	if ( vidInfo->bmiHeader.biHeight <0){ // for bottom-up RGB24 image
		frame.stride = -frame.stride;
	}

	frame.bFlipHorizontally = bFlipPreview_;
	frame.image = bufferSample;
	// draw preview
	pRenderer_->DrawVideo(frame);

	bool isFirstFrameLocal = firstSample_;
	// log for first sample
	if ( firstSample_){
		firstSample_ = false;
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"read first sample : width = %d, height = %d", 
			frame.width, frame.height);
	}


	CaptureFrameInfo capFrame;
	capFrame.format = frame.format;
	capFrame.height = frame.height;
	capFrame.width = frame.width;
	capFrame.stride = frame.stride;
	capFrame.pSample = NULL;
	capFrame.pImage = frame.image;
	callback_->OnVideoCaptureData(capFrame, ordinal_, isFirstFrameLocal);


	// after using in runner thread
	pSample->Release();
	
	return S_OK;
}

HRESULT VideoCapture::OnMediaSample(IMediaSample* pSample, CMediaType* pMediaType)
{
	if(pSample == NULL || pMediaType == NULL) return E_FAIL;
	// addref for thread switching
	pSample->AddRef();
	// switch to runner thread
	LooperTask t = [=] (){
		_HandleMediaSample(pSample, pMediaType);
	};

	dispatcher_.runAsyncTask(t);

	return S_OK;
}

// this is a class static method
HRESULT VideoCapture::GetDevList (std::vector<MediaDevInfo>& devList)
{
	devList.clear();

	IMFActivate  **ppDevices;
    UINT32  devCount;

    _com_ptr_IMFAttributes pAttributes = NULL;
    // Create an attribute store to specify the enumeration parameters.
    HRESULT hr = MFCreateAttributes(&pAttributes, 1);
    if (FAILED(hr)) return hr;

	// Source type: video capture devices
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE, 
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);

    if (FAILED(hr)){ return hr; }

    // Enumerate devices.
    hr = MFEnumDeviceSources(pAttributes, &ppDevices, &devCount);
	if ( devCount<1) return -1; // no capture dev

	for (int i=0; i<(int)devCount;i++)
	{
		MediaDevInfo info;
		WCHAR buff [4096];
		// friendly name
		hr = ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME, buff, 4096, NULL);
		if (SUCCEEDED(hr))info.devFriendlyName = buff;

		// symbolic link
		hr = ppDevices[i]->GetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK, buff, 4096, NULL);
		if (SUCCEEDED(hr))info.symbolicLink = buff;
		info.devType = MediaDevTypeN::eVideoCapture;

		// Get bus location of the camera
		DevSetupInfo setupInfo (info.symbolicLink, TRUE/*need to parse the symboliclink*/);
		BOOL ret = setupInfo.IsSetupDiRetrieved();
		info.location = setupInfo.GetDevLocation();
		info.deviceId = setupInfo.GetDevInstance();
        info.busReportedDeviceDesc = setupInfo.GetBusReportedDeviceDesc();

		std::wstring lowerId = _get_lower_case(info.symbolicLink);
		if (lowerId.find(L"xylink") != std::string::npos && 
			lowerId.find(L"\\global") != std::string::npos ){
				continue; // ignore the one ends with '\global' for xylink card
		}

		// get FPGA version, and input status
		if(lowerId.find(L"xylink") != std::string::npos){
			KsClient ks ;
			ks.Create(info.symbolicLink);
			ULONG locked,  lineNum;
			info.videoInputLocked = false;
			hr = ks.QueryVidCapDecoderStatus(locked, lineNum);// xyLink card store verNum in the DecoderStatus.lineNum field.
			if ( SUCCEEDED(hr)){
				NativeMediaLogger4CPP::log(Native::LogLevelN::Info, "VideoCapture", "QueryVidCapDecoderStatus: locked=%x \n", locked);
				NativeMediaLogger4CPP::log(Native::LogLevelN::Info, "VideoCapture", "QueryVidCapDecoderStatus: lineNum(ver)=%x \n", lineNum);
				info.xylinkFPGAVersion = lineNum;
				info.videoInputLocked = locked;
			}
		}

		// ignore virtaul devices
		if (lowerId.find(L"\\\\?\\root") == std::string::npos){
			devList.push_back(info);
		}

	}

	for (DWORD i = 0; i < devCount; i++)
    {
        SafeRelease(&ppDevices[i]);
    }
    CoTaskMemFree(ppDevices);
	ppDevices = NULL;
	devCount = 0;
	return hr;
}


static float __FR(VideoCaptureFormat&fmt)
{
	if ( fmt.fsDenominator ==0) return 0.0;
	return (fmt.fsNumerator*1.0f )/(fmt.fsDenominator*1.0f );
}

HRESULT VideoCapture::_SetFormat ()
{
	if (pVidCapDev_==NULL) return E_FAIL;
	std::map<CapFmtTag, std::vector<VideoCaptureFormat>> fmtMap;
	HRESULT hr = pVidCapDev_->GetFormats(fmtMap);
	if(FAILED(hr))return hr;

	hr = _ChooseFormat(fmtMap, format_);
	if(FAILED(hr))return hr;

	hr  = pVidCapDev_->SetFormat(format_);
}

HRESULT VideoCapture::_ChooseFormat(std::map<CapFmtTag, std::vector<VideoCaptureFormat>> & fmtMap, VideoCaptureFormat&out_fmt)
{
	bool hasMatched = false;
	std::vector<VideoCaptureFormat> * mjpgFormats = NULL;
	std::vector<VideoCaptureFormat> * yuy2Formats = NULL;
	std::vector<VideoCaptureFormat> * i420Formats = NULL;
	std::vector<VideoCaptureFormat> * nv12Formats = NULL;
	std::vector<VideoCaptureFormat> * rgb24Formats = NULL;

	if(fmtMap.find(TAG_MJPEG)!=fmtMap.end())mjpgFormats=&fmtMap[TAG_MJPEG];
	if(fmtMap.find(TAG_YUY2)!=fmtMap.end())yuy2Formats=&fmtMap[TAG_YUY2];
	if(fmtMap.find(TAG_I420)!=fmtMap.end())i420Formats=&fmtMap[TAG_I420];
	if(fmtMap.find(TAG_NV12)!=fmtMap.end())nv12Formats=&fmtMap[TAG_NV12];
	if(fmtMap.find(TAG_RGB24)!=fmtMap.end())rgb24Formats=&fmtMap[TAG_RGB24];

	// compare resolution and frame speed
	auto comp = [](VideoCaptureFormat& l, VideoCaptureFormat& r)->bool
	{
		// frame speed greater than 30, we will put resolution at priority 
		float lcap = __FR(l) * l.width * l.height;
		float rcap = __FR(r) * r.width * r.height;

		if( lcap > rcap ){
			return true;
		}else if ( ::abs(lcap - rcap ) < 0.1f) { // same cap
			if ( l.width > r.width && l.height > r.height ){
				return true;
			}
		}
		return false;
	};

	if(yuy2Formats)std::sort(yuy2Formats->begin(), yuy2Formats->end(), comp);
	if(i420Formats)std::sort(i420Formats->begin(), i420Formats->end(), comp);
	if(nv12Formats)std::sort(nv12Formats->begin(), nv12Formats->end(), comp);
	if(rgb24Formats)std::sort(rgb24Formats->begin(), rgb24Formats->end(), comp);
	if(mjpgFormats)std::sort(mjpgFormats->begin(), mjpgFormats->end(), comp);

	std::vector<VideoCaptureFormat> * pfmts = nullptr;
	// use uncompressed formats at priority
	// For best optimization, we use camera format at following order:
	//  YUY2 > NV12 > RGB24 > I420 > MJPEG
	if (yuy2Formats && yuy2Formats->size()> 0 && (*yuy2Formats)[0].width >= 1280 && __FR((*yuy2Formats)[0])>29.0 ){
		pfmts = yuy2Formats;
	}else if (nv12Formats && nv12Formats->size()> 0 && (*nv12Formats)[0].width >= 1280 && __FR((*nv12Formats)[0])>29.0 ){
		pfmts = nv12Formats;
	}else if (rgb24Formats && rgb24Formats->size()> 0 && (*rgb24Formats)[0].width >= 1280 && __FR((*rgb24Formats)[0])>29.0 ){
		pfmts = rgb24Formats;
	}else if (i420Formats && i420Formats->size()> 0 && (*i420Formats)[0].width >= 1280 && __FR((*i420Formats)[0])>29.0 ){
		pfmts = i420Formats;
	}else if (mjpgFormats && mjpgFormats->size() > 0&& (*mjpgFormats)[0].width >= 1280 && __FR((*mjpgFormats)[0])>9.0){
		pfmts = mjpgFormats;
	}else {
		// very low spec cameras can support YUY2 VGA/RGB24 VGA
		if (yuy2Formats && yuy2Formats->size()>0){
			pfmts = yuy2Formats;
		}else if (rgb24Formats && rgb24Formats->size()>0){
			pfmts = rgb24Formats;
		}
	}

	// limit the max resolution (we should limit the capability based on PC spec later)
	if (pfmts != nullptr)
	{
		std::vector<VideoCaptureFormat>::iterator i = pfmts->begin();
		while ( i!=pfmts->end())
		{
			float diff = ::abs( __FR(*i) - maxCapability_.fameRate);
			bool  meetTargetFR = ( diff <= 1.0f || __FR(*i) < maxCapability_.fameRate);
			bool  meet16to9Ratio =  ::abs( (i->width*1.0)/(i->height*1.0) - 1.777777) < 0.0001;
			if ( (*i).width <= maxCapability_.width &&
				meetTargetFR &&
				meet16to9Ratio &&
				 (*i).width >=1280){
				out_fmt = *i;
				hasMatched = true;
				break;
			}
			i++;
		}

		// has no 16to9 formats
		if ( i == pfmts->end())
		{
			for ( i = pfmts->begin(); i!=pfmts->end(); i++)
			{
				float diff = ::abs( __FR(*i) - maxCapability_.fameRate);
				bool  meetTargetFR = ( diff <= 1.0f || __FR(*i) < maxCapability_.fameRate);
				if ( (*i).width <= maxCapability_.width && meetTargetFR ){ // do not check aspect ratio
					hasMatched = true;
					out_fmt = *i;
					break;
				}
				i++;
			}
		}
	}

	if ( !hasMatched) return E_NOTFOUND;

	return S_OK;
}

// compare the removed device id with current device id, 
// to check if our camera disconnected. 
HRESULT VideoCapture::CheckDeviceLost(std::wstring removedId, BOOL *pbDeviceLost)
{
    if (pbDeviceLost == NULL) return E_POINTER;
    
	*pbDeviceLost = FALSE;
	if (removedId.size()==0)return S_OK;

	wchar_t * w1 = new wchar_t[symbolicLink_.size()+1];
	wchar_t * w2 = new wchar_t[removedId.size()+1];
	wcscpy_s(w1, symbolicLink_.size()+1, symbolicLink_.c_str());
	wcscpy_s(w2, removedId.size()+1, removedId.c_str());
	wchar_t * p = wcschr(w1, L'{');// remove guid
	if (p) *p=NULL;
	p = wcschr(w2, L'{');
	if (p) *p=NULL;
    if (_wcsicmp(w1, w2)==0)
    {
        *pbDeviceLost = TRUE;
    }

    return S_OK;
}

VOID VideoCapture::GetCurrentFormat(VideoCaptureFormat&fmt)
{
	fmt = format_;
}

///////////////////////////////////////////////////
// camera control
bool VideoCapture::CameraControl_GetRange(long camCtrlProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto)
{
	return cameraControl_.CameraControl_GetRange(camCtrlProperty, minVal, maxVal, Step, defaultVal,supportAuto);
}
bool VideoCapture::CameraControl_GetValue(long camCtrlProperty, long &val, bool &isAuto)
{
	return cameraControl_.CameraControl_GetValue(camCtrlProperty, val, isAuto);
}
bool VideoCapture::CameraControl_SetValue(long camCtrlProperty, long val, bool isAuto)
{
	return cameraControl_.CameraControl_SetValue(camCtrlProperty, val, isAuto);
}

///////////////////////////////////////////////////
// VideoProcAmp
bool VideoCapture::VideoProcAmp_GetRange(long procAmpProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto)
{
	return cameraControl_.VideoProcAmp_GetRange(procAmpProperty, minVal, maxVal, Step, defaultVal,supportAuto);
}
bool VideoCapture::VideoProcAmp_GetValue(long procAmpProperty, long &val, bool &isAuto)
{
	return cameraControl_.VideoProcAmp_GetValue(procAmpProperty, val, isAuto);
}
bool VideoCapture::VideoProcAmp_SetValue(long procAmpProperty, long val, bool isAuto)
{
	return cameraControl_.VideoProcAmp_SetValue(procAmpProperty, val, isAuto);
}

} // namespace WinRTCSDK