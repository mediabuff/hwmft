// using Media Foundation API for Video capture
#pragma once


#include "NativeTypes.h"
#include "CamControl.h"

namespace WinRTCSDK{

struct VideoCaptureFormat
{
	VideoCaptureFormat():
		subType4MF(GUID_NULL),subType4dshow(GUID_NULL),
		indexOfMediaType(0), pMFMediaType(nullptr), fsNumerator(0)
		,fsDenominator(0),width(0),height(0)
	{
	}
	std::wstring name; // readable name of the type
	GUID   subType4MF; // subtype of IMFMediaType (attribte MF_MT_SUBTYPE)
	// for DShow media type
	GUID   subType4dshow; // AM_MEDIA_TYPE::subtype
	int    indexOfMediaType; // native type index for MFSourceReader/Dshow Filter
	_com_ptr_IMFMediaType pMFMediaType; // the native media type of MFSourceReader
	UINT32 fsNumerator;
	UINT32 fsDenominator;
	UINT32 width;
	UINT32 height;
};

enum CapFmtTag
{
	TAG_MJPEG ,
	TAG_I420  ,
	TAG_YUY2  ,
	TAG_NV12  ,
	TAG_RGB24 
};

class IVideoCaptureDev
{
public:
	virtual HRESULT OpenDev(std::wstring symbolicLink) = 0;
	virtual HRESULT Start() = 0;
	virtual HRESULT Stop() = 0;
	virtual HRESULT AsyncRead() = 0;//only used for MediaFoundation capture
	virtual HRESULT CloseDev() = 0;
	virtual HRESULT GetFormats(std::map<CapFmtTag, std::vector<VideoCaptureFormat>> & fmtMap) = 0;
	virtual HRESULT SetFormat(VideoCaptureFormat& fmt) = 0;
	virtual HRESULT GetDefaultStride(LONG& stride) = 0; // call this only after a successful SetFormat
	virtual HRESULT InitCameraControl(CameraControl & control) = 0;
};


}// namespace WinRTCSDK