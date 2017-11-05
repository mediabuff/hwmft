#include "stdafx.h"

#include "MFVidCapture.h"
#include "NativeMediaLogger4CPP.h"


namespace WinRTCSDK{

using namespace Native;

MFVidCapture::MFVidCapture(IMFSourceReaderCallback *callback ):
	callback_(callback),
	pReader_ (NULL),
	symbolicLink_ (),
	format_()
{
}

MFVidCapture::~MFVidCapture(void)
{
}

HRESULT MFVidCapture::Start()
{
	// Currently the SourceReader works in async mode only!
	// Async read the first frame
	HRESULT hr = pReader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,NULL, NULL, NULL, NULL);

	return S_OK;
}

HRESULT MFVidCapture::AsyncRead()
{
	// request next
	HRESULT hr = pReader_->ReadSample((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, 0,NULL, NULL, NULL, NULL);
	return S_OK;
}

HRESULT MFVidCapture::Stop()
{
	// no need to stop
	return S_OK;
}

HRESULT MFVidCapture::OpenDev(std::wstring symbolicLink)
{
	HRESULT hr = S_OK;

	symbolicLink_ = symbolicLink;
	if (symbolicLink_.empty()) return E_INVALIDARG;

	// create source reader from media source
	hr = _CreateSourceReader(FALSE, FALSE);
	if (FAILED(hr)) { 
		NativeMediaLogger4CPP::logW(Error, L"Camera", L"Failed to create MFSourceReader hr=%x",hr);
		return hr; 
	}

	// we need reset source reader for RGB24 format to disable DXVA
	hr = _ResetSourceReader4FinalFormat();

	return hr;
	
}


HRESULT MFVidCapture::CloseDev()
{
	_DestroySourceReader();
	return S_OK;
}
	

HRESULT MFVidCapture::_ResetSourceReader4FinalFormat ()
{
	// disable DXVA for following formats 
	if (format_.subType4MF != MFVideoFormat_RGB24) return S_OK;

	NativeMediaLogger4CPP::logW(Info, L"Camera", L"reset MFSourceReader.");
	
	HRESULT hr = _CreateSourceReader(FALSE/*disable hardware MFT*/, FALSE/*disable DXVA*/);
	if (FAILED(hr)) return hr;
    

	// setup final format again
	hr = SetFormat(format_);

DONE:
	return hr;
}


HRESULT MFVidCapture::_DestroySourceReader ( )
{
	pReader_ = NULL;
	pSource_ = NULL;
   
    return S_OK;
}

HRESULT MFVidCapture::_CreateVideoCaptureDevice(IMFMediaSource **ppSource)
{
    *ppSource = NULL;
    _com_ptr_IMFAttributes pAttributes = NULL;

    HRESULT hr = MFCreateAttributes(&pAttributes, 3);
    if (FAILED(hr)) goto done;

    // Set the device type to video.    
    hr = pAttributes->SetGUID(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
        MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID);
    if (FAILED(hr)) goto done;
    
    // Set the symbolic link.
    hr = pAttributes->SetString(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
            (LPCWSTR)symbolicLink_.c_str());            
    if (FAILED(hr)) goto done;

	// Set the max buffers
	hr = pAttributes->SetUINT32(MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_MAX_BUFFERS, 3);
    if (FAILED(hr)) goto done;
        
	hr = ::MFCreateDeviceSource(pAttributes, ppSource);

done:
    return hr;    
}


HRESULT MFVidCapture::_CreateSourceReader (BOOL useHardwareMFT, BOOL useDXVA)
{
	HRESULT  hr = S_OK;
    _com_ptr_IMFAttributes   pAttributes = NULL;

	//release old ones;
	pReader_ = NULL;
	pSource_ = NULL;

	// create media source from capture device
	hr= _CreateVideoCaptureDevice(&pSource_);
    if (FAILED(hr)){
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"Failed to create MFMediaFource hr=%x",hr);
		return hr;
	}

    // Create an attribute store to hold initialization settings.
	hr = MFCreateAttributes(&pAttributes, 6);
    if (FAILED(hr)) goto DONE;

	// enable async mode
	hr = pAttributes->SetUnknown(MF_SOURCE_READER_ASYNC_CALLBACK , callback_);
	if (FAILED(hr)) goto DONE;

	// enable hardware MFT
	if ( useHardwareMFT ){
		hr = pAttributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS , TRUE);
		if (FAILED(hr)) goto DONE;
	}else{
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"disabled hardware MFT.");
	}
	// enable DXVA
	if ( pD3DManager_ && useDXVA){
		hr = pAttributes->SetUnknown(MF_SOURCE_READER_D3D_MANAGER, (IUnknown*)pD3DManager_);
		if (FAILED(hr)) goto DONE;
	}else{
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"disabled DXVA.");
	}

	// enable low latency
	hr = pAttributes->SetUINT32(MF_LOW_LATENCY , TRUE);
	if (FAILED(hr)) goto DONE;

    hr = ::MFCreateSourceReaderFromMediaSource( pSource_, pAttributes, &pReader_ );
    if (FAILED(hr)) goto DONE;

DONE:
    return hr;
}
	
HRESULT MFVidCapture::_VideoCapFormatFromMediaType( VideoCaptureFormat& fmt, _com_ptr_IMFMediaType type)
{
	HRESULT hr = type->GetGUID(MF_MT_SUBTYPE, &fmt.subType4MF);
	fmt.name = _GetFmtName(fmt.subType4MF);
	fmt.pMFMediaType = type;
	hr = MFGetAttributeRatio(type, MF_MT_FRAME_RATE, &fmt.fsNumerator, &fmt.fsDenominator );
	hr = MFGetAttributeSize(type, MF_MT_FRAME_SIZE, &fmt.width, &fmt.height);
	return hr;
}

static float __FR(VideoCaptureFormat&fmt)
{
	if ( fmt.fsDenominator ==0) return 0.0;
	return (fmt.fsNumerator*1.0f )/(fmt.fsDenominator*1.0f );
}
	
HRESULT MFVidCapture::GetFormats(std::map<CapFmtTag, std::vector<VideoCaptureFormat>> & fmtMap)
{
	if (pReader_ == NULL) return E_INVALIDARG;
	int     i;
	HRESULT hr;
    GUID    majorType;
    _com_ptr_IMFMediaType pNativeType = NULL;
	VideoCaptureFormat fmt;
    // Find the native formats of the stream.
	for (i=0; /* keep trying get formats until failed*/ ;i++)
	{
		// the GetNativeMediaType method returns a copy of underlying mediatype
		// so we can modify it safely
		hr = pReader_->GetNativeMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, i, &pNativeType);
		if ( SUCCEEDED(hr))
		{
		    hr = pNativeType->GetGUID(MF_MT_MAJOR_TYPE, &majorType);
			if ( majorType != MFMediaType_Video) continue; // care only video type

			_VideoCapFormatFromMediaType(fmt, pNativeType);

			NativeMediaLogger4CPP::logW(Info, L"Camera", L"Native VideoFmt: %s, resolution=%dx%d, fr=%d/%d(%ffps)\n",
				fmt.name.c_str(), fmt.width, fmt.height, fmt.fsNumerator, fmt.fsDenominator, fmt.fsNumerator *1.0/fmt.fsDenominator);

			if (fmt.subType4MF == MFVideoFormat_NV12 ) 
				fmtMap[TAG_NV12].push_back(fmt); // Windows 10 (build 1607) can provide NV12 for nearly all UVC cameras 

			if (fmt.subType4MF == MFVideoFormat_MJPG && fmt.width <= 1920) 
				fmtMap[TAG_MJPEG].push_back(fmt); // UVC cameras with USB 2.0 interface always report this format

			if (fmt.subType4MF == MFVideoFormat_YUY2 ) 
				fmtMap[TAG_YUY2].push_back(fmt); // Nearly all capture devices provide this format on all windows versions

			if (fmt.subType4MF == MFVideoFormat_RGB24 ) 
				fmtMap[TAG_RGB24].push_back(fmt); // Some brands' ( Logitech) cameras support rgb24 on windows 7 only

			if (fmt.subType4MF == MFVideoFormat_I420  ) 
				fmtMap[TAG_I420].push_back(fmt); // D3D9 surface of I420 type only works well on windows 10
		}
		else
		{
			break;
		}
	}
}

HRESULT MFVidCapture::InitCameraControl(CameraControl & control)
{
	if(pSource_==nullptr) return E_FAIL;
	HRESULT hr = control.Create(pSource_); // create camera control by media source
	return hr;
}
	
HRESULT MFVidCapture::SetFormat (VideoCaptureFormat& /*in out*/fmt)
{
	HRESULT hr = S_OK;
	_com_ptr_IMFMediaType pUsedType; 

	if (fmt.subType4MF == MFVideoFormat_MJPG){
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"Decode MJPG to YUY2");
		fmt.subType4MF = MFVideoFormat_YUY2; // require the MFReader the decode the image
		fmt.pMFMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_YUY2);
	}

	// set the selected media type
	hr = pReader_->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, NULL, fmt.pMFMediaType);
	
	if (FAILED(hr)) {
		NativeMediaLogger4CPP::logW(Info, L"Camera", L"failed to set format!\n");
	}

done:
	// retrieve current format
	hr = pReader_->GetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_VIDEO_STREAM, &pUsedType);
	_VideoCapFormatFromMediaType (format_, pUsedType);

	NativeMediaLogger4CPP::logW(Info, L"Camera", L"Final format: %s, resolution=%dx%d, fr=%d/%d\n",
		format_.name.c_str(), format_.width, format_.height, format_.fsNumerator, format_.fsDenominator);
	// write back
	fmt = format_;
    return hr;
}

HRESULT MFVidCapture::GetDefaultStride(LONG& stride)
{
    // Try to get the default stride from the media type.
	HRESULT hr = format_.pMFMediaType->GetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32*)&stride);
    if (FAILED(hr))
    {
        // Attribute not set. Try to calculate the default stride.
        GUID subtype = GUID_NULL;

        UINT32 width = 0;
        UINT32 height = 0;

        // Get the subtype and the image size.
        hr = format_.pMFMediaType->GetGUID(MF_MT_SUBTYPE, &subtype);
        if (SUCCEEDED(hr))
        {
            hr = MFGetAttributeSize(format_.pMFMediaType, MF_MT_FRAME_SIZE, &width, &height);
        }
        if (SUCCEEDED(hr))
        {
            hr = MFGetStrideForBitmapInfoHeader(subtype.Data1, width, &stride);
        }

        // Set the attribute for later reference.
        if (SUCCEEDED(hr))
        {
            (void)format_.pMFMediaType->SetUINT32(MF_MT_DEFAULT_STRIDE, UINT32(stride));
        }
    }

    return hr;
}

std::wstring MFVidCapture::_GetFmtName( GUID fmt)
{
	static struct { GUID guid;  std::wstring name; } fmtList [] = 
	{
		MFVideoFormat_RGB32,L"MFVideoFormat_RGB32",
		MFVideoFormat_ARGB32,L"MFVideoFormat_ARGB32",
		MFVideoFormat_RGB24,L"MFVideoFormat_RGB24",
		MFVideoFormat_RGB555,L"MFVideoFormat_RGB555",
		MFVideoFormat_RGB565,L"MFVideoFormat_RGB565",
		MFVideoFormat_RGB8, L"MFVideoFormat_RGB8", 
		MFVideoFormat_AI44, L"MFVideoFormat_AI44", 
		MFVideoFormat_AYUV, L"MFVideoFormat_AYUV", 
		MFVideoFormat_YUY2, L"MFVideoFormat_YUY2", 
		MFVideoFormat_YVYU, L"MFVideoFormat_YVYU", 
		MFVideoFormat_YVU9, L"MFVideoFormat_YVU9", 
		MFVideoFormat_UYVY, L"MFVideoFormat_UYVY", 
		MFVideoFormat_NV11, L"MFVideoFormat_NV11", 
		MFVideoFormat_NV12, L"MFVideoFormat_NV12", 
		MFVideoFormat_YV12, L"MFVideoFormat_YV12", 
		MFVideoFormat_I420, L"MFVideoFormat_I420", 
		MFVideoFormat_IYUV, L"MFVideoFormat_IYUV", 
		MFVideoFormat_DVC,  L"MFVideoFormat_DVC",  
		MFVideoFormat_H264, L"MFVideoFormat_H264", 
		MFVideoFormat_MJPG, L"MFVideoFormat_MJPG", 
		MFVideoFormat_420O, L"MFVideoFormat_420O", 
		GUID_NULL, L""
	};

	for(int i=0; fmtList [i].guid != GUID_NULL ;i++)
	{
		if ( fmtList [i].guid == fmt ) return fmtList [i].name;
	}
	return L"";
}

} // namespace WinRTCSDK