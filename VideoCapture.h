// using Media Foundation API for Video capture
#pragma once

#include "AutoLock.h"
#include "ComPtrDefs.h"
#include "NativeTypes.h"
#include "Looper.h"
#include "D3D9Renderer.h"

#include "CamControl.h"
#include "DShowVidCapture.h"
#include "MFVidCapture.h"

using namespace std;

namespace WinRTCSDK {

struct CaptureFrameInfo
{
	CaptureFrameInfo() : pImage(NULL),pSample(NULL), 
		format(D3DFMT_UNKNOWN), width(0), height(0),
		stride(0){}
	// data is already locked.
	LPBYTE pImage;
	// data is in MediaFoundation sample
	_com_ptr_IMFSample pSample;
	D3DFORMAT format;
	int       width;
	int       height;
	int       stride;
};

class IVideoCaptureCallback
{
public:
	virtual void OnVideoCaptureData(CaptureFrameInfo & pSample, VideoCaptureOrdinal ordinal, bool firstFrame) = 0;
	virtual void OnVideoCaptureError(HRESULT hr, VideoCaptureOrdinal ordinal) {}
};

class VideoCapture : 
	public IMFSourceReaderCallback,
	public IDShowGrabberCallbacks
{
public:
	class Deleter {
	public :
		Deleter(){};
		// refCount of std::shared_ptr reached zero, lets decrease the COM reference
		void operator()(VideoCapture* ptr){ 
			ptr->Release(); // call IUnKnown::Release
		}
	};

	// for easy object management
	static std::shared_ptr<VideoCapture> make_shared(IVideoCaptureCallback *callback, std::shared_ptr<D3D9Renderer> pRenderer, VideoCaptureOrdinal ordinal)
	{
		return std::shared_ptr<VideoCapture>( new VideoCapture(callback, pRenderer, ordinal), Deleter());
	}

	friend class  Deleter;
	// life time is managed by COM pointers and std::shared_ptr pointers automatically!
private:
	VideoCapture(IVideoCaptureCallback *callback, std::shared_ptr<D3D9Renderer> pRenderer, VideoCaptureOrdinal ordinal);
public:
	~VideoCapture(void);

public:
    // IUnknown methods
    STDMETHODIMP QueryInterface(REFIID iid, void** ppv){
		static const QITAB qit[] = { QITABENT(VideoCapture, IMFSourceReaderCallback), { 0 } };
		return ::QISearch(this, qit, iid, ppv);
	}
	STDMETHODIMP_(ULONG) AddRef(){
	   return InterlockedIncrement(&nRefCount_);
	}
    STDMETHODIMP_(ULONG) Release(){
		ULONG uCount = InterlockedDecrement(&nRefCount_);
		if (uCount == 0) delete this; 
		return uCount;
	}

    // IMFSourceReaderCallback methods
    STDMETHODIMP VideoCapture::OnReadSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
		LONGLONG llTimestamp, IMFSample *pSample );
    STDMETHODIMP VideoCapture::OnEvent(DWORD, IMFMediaEvent *);
    STDMETHODIMP VideoCapture::OnFlush(DWORD);

	// IDShowGrabberCallbacks methods
	HRESULT OnMediaSample(IMediaSample* pSample, CMediaType* pMediaType);

public:
	static  HRESULT GetDevList (std::vector<MediaDevInfo>& devList);
	HRESULT        CheckDeviceLost(std::wstring removedId, BOOL *pbDeviceLost);
	HRESULT        Choose(std::wstring symbolicLink);
	HRESULT        Open();
	HRESULT        Close();
	HRESULT        Reset();
	VOID           GetCurrentFormat(VideoCaptureFormat&fmt);
	VOID           GetStatistics(uint64_t& frameCnt, double&fps);
	VOID           SetMaxCapability(VideoCaptureCapbaility cap);
	VOID           SetLocalPreviewFlipping(BOOL bFlip);

public: 	
	// camera control
	bool CameraControl_GetRange(long camCtrlProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto);
	bool CameraControl_GetValue(long camCtrlProperty, long &val, bool &isAuto);
	bool CameraControl_SetValue(long camCtrlProperty, long val, bool isAuto);

	// VideoProcAmp
	bool VideoProcAmp_GetRange(long procAmpProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto);
	bool VideoProcAmp_GetValue(long procAmpProperty, long &val, bool &isAuto);
	bool VideoProcAmp_SetValue(long procAmpProperty, long val, bool isAuto);

private:
	HRESULT        _OpenAndSetFormat ();
	HRESULT        _SetFormat ();
	HRESULT        _ChooseFormat(std::map<CapFmtTag, std::vector<VideoCaptureFormat>> & fmtMap, VideoCaptureFormat&out_fmt);
	HRESULT        _HandleCameraSample(HRESULT hrStatus, DWORD dwStreamIndex, DWORD dwStreamFlags,
						LONGLONG llTimestamp, IMFSample * pSample);
	HRESULT		   _HandleMediaSample(IMediaSample* pSample, CMediaType* pMediaType);

private:
	IVideoCaptureDev                   *pVidCapDev_;
	bool                                isUsingDshow_;  // if failed to use MFRourceReader pipeline, Dshow graph will be used...
	std::shared_ptr<D3D9Renderer>       pRenderer_;
	IVideoCaptureCallback              *callback_;
    long                                nRefCount_;        // Reference count.
	VideoCaptureCapbaility              maxCapability_;
	VideoCaptureOrdinal                 ordinal_;

	// symboliclink is used to check the device lost 
	std::wstring                        symbolicLink_ ; // for current device
	Mutex                               lock_;
	long                                defaultStride_;
	VideoCaptureFormat                  format_;
	Looper                              dispatcher_;
	AtomicVar<bool>                     cameraRunningFlag_;
	bool                                dispatchMFSample_ ; // async dispatch mode.
	CameraControl                       cameraControl_;

	// diagnostics
	bool                                firstSample_;
	HRESULT                             lastError_;
	BOOL                                bFlipPreview_;

	// staticstic
	uint64_t                            frameCnt_;
	uint64_t                            startTimeQPC_; // start time 


};


}// namespace WinRTCSDK