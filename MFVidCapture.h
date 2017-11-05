// using Media Foundation API for Video capture
#pragma once


#include "AutoLock.h"
#include "ComPtrDefs.h"
#include "NativeTypes.h"

#include "IVideoCaptureDev.h"
#include "CamControl.h"

using namespace std;

namespace WinRTCSDK {

class MFVidCapture:
	public IVideoCaptureDev
{
public:
	MFVidCapture(IMFSourceReaderCallback *callback);
public:
	virtual ~MFVidCapture(void);

public:
	virtual HRESULT OpenDev(std::wstring symbolicLink);
	virtual HRESULT Start();
	virtual HRESULT Stop();
	virtual HRESULT AsyncRead();//only used for MediaFoundation capture
	virtual HRESULT CloseDev();
	virtual HRESULT GetFormats(std::map<CapFmtTag, std::vector<VideoCaptureFormat>> & fmtMap);
	virtual HRESULT SetFormat(VideoCaptureFormat& fmt);
	virtual HRESULT GetDefaultStride(LONG& stride); // call this only after a successful SetFormat
	virtual HRESULT InitCameraControl(CameraControl & control);

private:
	static std::wstring _GetFmtName( GUID fmt);
	HRESULT        _VideoCapFormatFromMediaType( VideoCaptureFormat& fmt, _com_ptr_IMFMediaType type);
	HRESULT        _CreateVideoCaptureDevice(IMFMediaSource **ppSource);
	HRESULT        _CreateSourceReader (BOOL useHardwareMFT, BOOL useDXVA);
	HRESULT        _ResetSourceReader4FinalFormat ();
	HRESULT        _DestroySourceReader();

private:
	_com_ptr_IDirect3DDeviceManager9    pD3DManager_;// for HW decoder for MJPEG, H.264 formats
	_com_ptr_IMFMediaSource             pSource_;
	IMFSourceReaderCallback             *callback_;
    _com_ptr_IMFSourceReader            pReader_;
	// symboliclink is used to check the device lost 
	std::wstring                        symbolicLink_ ; // for current device
	VideoCaptureFormat                  format_;

};


}// namespace WinRTCSDK