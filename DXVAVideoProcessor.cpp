#include "Stdafx.h"

#include "ComPtrDefs.h"

#include "DXVAVideoProcessor.h"

const UINT VIDEO_FPS     = 60;
const UINT VIDEO_MSPF    = (1000 + VIDEO_FPS / 2) / VIDEO_FPS;
const UINT VIDEO_100NSPF = VIDEO_MSPF * 10000;

#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2') 
#define D3DFMT_I420 (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0') 
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2') 

namespace WinRTCSDK{

////////////////////////////////////////////////////
// Helper inline functions.
static BOOL operator != (const DXVA2_ValueRange& x, const DXVA2_ValueRange& y)
{
    return memcmp(&x, &y, sizeof(x)) ? TRUE : FALSE;
}

static DWORD _RGBtoYUV(const D3DCOLOR rgb)
{
    const INT A = HIBYTE(HIWORD(rgb));
    const INT R = LOBYTE(HIWORD(rgb)) - 16;
    const INT G = HIBYTE(LOWORD(rgb)) - 16;
    const INT B = LOBYTE(LOWORD(rgb)) - 16;

    //
    // studio RGB [16...235] to SDTV ITU-R BT.601 YCbCr
    //
    INT Y = ( 77 * R + 150 * G +  29 * B + 128) / 256 + 16;
    INT U = (-44 * R -  87 * G + 131 * B + 128) / 256 + 128;
    INT V = (131 * R - 110 * G -  21 * B + 128) / 256 + 128;

    return D3DCOLOR_AYUV(A, Y, U, V);
}

static DXVA2_AYUVSample16 _GetBackgroundColor(D3DCOLOR rgbclr)
{
    const D3DCOLOR yuv = _RGBtoYUV(rgbclr);

    const BYTE Y = LOBYTE(HIWORD(yuv));
    const BYTE U = HIBYTE(LOWORD(yuv));
    const BYTE V = LOBYTE(LOWORD(yuv));

    DXVA2_AYUVSample16 color;

    color.Cr    = V * 0x100;
    color.Cb    = U * 0x100;
    color.Y     = Y * 0x100;
    color.Alpha = 0xFFFF;

    return color;
}


static INT _ComputeLongSteps(DXVA2_ValueRange &range)
{
    float f_step = DXVA2FixedToFloat(range.StepSize);

    if (f_step == 0.0)
    {
        return 0;
    }

    float f_max = DXVA2FixedToFloat(range.MaxValue);
    float f_min = DXVA2FixedToFloat(range.MinValue);
    INT steps = INT((f_max - f_min) / f_step / 32);

    return std::max(steps, 1);
}


///////////////////////////////////////////
// class DXVAVideoProcessor
DXVAVideoProcessor::DXVAVideoProcessor(_com_ptr_IDirect3DDeviceManager9 pMan):
	pDevManager_(pMan),  
	pDXVAVPS_(NULL),
	pDXVAVP_(NULL),
	sourceFormat_(D3DFMT_UNKNOWN),
	targetFormat_(D3DFMT_UNKNOWN),
	targetWidth_(0),
	targetHeight_(0)
{

	::memset(&vpGuid_,        0, sizeof(vpGuid_));
	::memset(&videoDesc_,     0, sizeof(videoDesc_));
	::memset(&vpCaps_,        0, sizeof(vpCaps_)); 
	::memset(&procAmpSteps_,  0, sizeof(procAmpSteps_));
	::memset(&procAmpRanges_, 0, sizeof(procAmpRanges_));
	::memset(&nFilterRanges_, 0, sizeof(nFilterRanges_));
	::memset(&dFilterRanges_, 0, sizeof(dFilterRanges_));
	::memset(&procAmpValues_, 0, sizeof(procAmpValues_));
	::memset(&nFilterValues_, 0, sizeof(nFilterValues_));
	::memset(&dFilterValues_, 0, sizeof(dFilterValues_));
}

DXVAVideoProcessor::~DXVAVideoProcessor()
{
}

HRESULT DXVAVideoProcessor::CreateDXVA2 (D3DFORMAT sourceFormat, D3DFORMAT targetFormat, int w, int h)
{
	return _CreateDXVA2(sourceFormat, targetFormat, w, h);
}

HRESULT DXVAVideoProcessor::CreateProcessorTarget (D3DFORMAT fmt, int w, int h, IDirect3DSurface9 ** pTarget)
{
	return pDXVAVPS_->CreateSurface(w, h, 1,fmt, D3DPOOL_DEFAULT, 0, DXVA2_VideoProcessorRenderTarget, pTarget, NULL);
}

HRESULT DXVAVideoProcessor::_CreateDXVA2VPDevice(REFGUID vpGuid, D3DFORMAT targetFormat)
{
    HRESULT                     hr;
    UINT                        i;
	UINT                        count;
    D3DFORMAT                  *formats = NULL;
    DXVA2_ValueRange            range;
	_com_ptr_IDirect3DSurface9  workTarget;

	const UINT required_op = DXVA2_VideoProcess_YUV2RGB |
                              DXVA2_VideoProcess_StretchX |
                              DXVA2_VideoProcess_StretchY |
                              DXVA2_VideoProcess_SubRects |
                              DXVA2_VideoProcess_SubStreams;

    hr = pDXVAVPS_->GetVideoProcessorRenderTargets(vpGuid, &videoDesc_, &count, &formats);
	if (FAILED(hr)) goto DONE;
    
    for (i = 0; i < count; i++)
    {
        if (formats[i] == targetFormat)
        {
            break;
        }
    }

    CoTaskMemFree(formats);

	if (i >= count) return E_FAIL;

    // Query video processor capabilities.
    hr = pDXVAVPS_->GetVideoProcessorCaps(vpGuid, &videoDesc_, targetFormat, &vpCaps_);

	if (FAILED(hr)) goto DONE;

    // Check to see if the device is software device.
    if ( !(vpCaps_.DeviceCaps & DXVA2_VPDev_HardwareDevice))
    {
        return E_FAIL; // we use Hardware device only
    }

    // Check to see if the device supports all the VP operations we want.
    if ((vpCaps_.VideoProcessorOperations & required_op) != required_op)
    {
        return E_FAIL;
    }

    // Query ProcAmp ranges.
    for (i = 0; i < ARRAYSIZE(procAmpRanges_); i++)
    {
        if (vpCaps_.ProcAmpControlCaps & (1 << i))
        {
            hr = pDXVAVPS_->GetProcAmpRange(vpGuid, &videoDesc_, targetFormat,
                                             1 << i,  &range);
		    if (FAILED(hr)) goto DONE;
            // Reset to default value if the range is changed.
            if (range != procAmpRanges_[i])
            {
                procAmpRanges_[i] = range;
                procAmpValues_[i] = range.DefaultValue;
                procAmpSteps_[i]  = _ComputeLongSteps(range);
            }
        }
    }

    // Query Noise Filter ranges.
    if (vpCaps_.VideoProcessorOperations & DXVA2_VideoProcess_NoiseFilter)
    {
        for (i = 0; i < ARRAYSIZE(nFilterRanges_); i++)
        {
            hr = pDXVAVPS_->GetFilterPropertyRange(vpGuid, &videoDesc_, targetFormat,
                                                    DXVA2_NoiseFilterLumaLevel + i, &range);
		    if (FAILED(hr)) goto DONE;

            // Reset to default value if the range is changed.
            if (range != nFilterRanges_[i])
            {
                nFilterRanges_[i] = range;
                nFilterValues_[i] = range.DefaultValue;
            }
        }
    }

    // Query Detail Filter ranges.
    if (vpCaps_.VideoProcessorOperations & DXVA2_VideoProcess_DetailFilter)
    {
        for (i = 0; i < ARRAYSIZE(dFilterRanges_); i++)
        {
            hr = pDXVAVPS_->GetFilterPropertyRange(vpGuid, &videoDesc_, targetFormat,
                                                    DXVA2_DetailFilterLumaLevel + i, &range);
		    if (FAILED(hr)) goto DONE;

            if (range != dFilterRanges_[i])
            {
                dFilterRanges_[i] = range;
                dFilterValues_[i] = range.DefaultValue;
            }
        }
    }

    // Finally create a video processor device.
    hr = pDXVAVPS_->CreateVideoProcessor(vpGuid, &videoDesc_, targetFormat, 1, &pDXVAVP_);

    if (FAILED(hr)) goto DONE;

	vpGuid_ = vpGuid;

DONE:

    return hr;
}

HRESULT DXVAVideoProcessor::_CreateDXVA2(D3DFORMAT sourceFormat, D3DFORMAT targetFormat, int w, int h)
{
    HRESULT hr;
	HANDLE hDevice;

	// new target size , need to recreate device!
	if (sourceFormat_ != sourceFormat ||
		targetFormat_!= targetFormat ||
		targetWidth_ != w ||
		targetHeight_ != h)
	{
		_DestroyDXVA2();
	}

	sourceFormat_ = sourceFormat;
	targetFormat_ = targetFormat;
	targetWidth_  = w;
	targetHeight_ = h;

	// device already created!
	if (pDXVAVP_) return S_OK;


	hr = pDevManager_->OpenDeviceHandle(&hDevice);
    if (FAILED(hr)) goto DONE;

	hr = pDevManager_->GetVideoService(hDevice, __uuidof(IDirectXVideoProcessorService),  (VOID**)&pDXVAVPS_);
    if (FAILED(hr)){
		pDevManager_->CloseDeviceHandle(hDevice);
	    goto DONE;
	}

	hr = pDevManager_->CloseDeviceHandle(hDevice);

    if (FAILED(hr)) goto DONE;

    // Initialize the video descriptor.
    videoDesc_.SampleWidth                         = w;
    videoDesc_.SampleHeight                        = h;
    videoDesc_.SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    videoDesc_.SampleFormat.NominalRange           = DXVA2_NominalRange_16_235;
    videoDesc_.SampleFormat.VideoTransferMatrix    = DXVA2_VideoTransferMatrix_BT601;
    videoDesc_.SampleFormat.VideoLighting          = DXVA2_VideoLighting_dim;
    videoDesc_.SampleFormat.VideoPrimaries         = DXVA2_VideoPrimaries_BT709;
    videoDesc_.SampleFormat.VideoTransferFunction  = DXVA2_VideoTransFunc_709;
    videoDesc_.SampleFormat.SampleFormat           = DXVA2_SampleProgressiveFrame;
    videoDesc_.Format                              = sourceFormat;
    videoDesc_.InputSampleFreq.Numerator           = VIDEO_FPS;
    videoDesc_.InputSampleFreq.Denominator         = 1;
    videoDesc_.OutputFrameFreq.Numerator           = VIDEO_FPS;
    videoDesc_.OutputFrameFreq.Denominator         = 1;

    // Query the video processor GUID.
    UINT count;
    GUID* guids = NULL;

    hr = pDXVAVPS_->GetVideoProcessorDeviceGuids(&videoDesc_, &count, &guids);

    if (FAILED(hr)) goto DONE;

    // Create a DXVA2 device.
	UINT i;
    for ( i = 0; i < count; i++)
    {
		hr = _CreateDXVA2VPDevice(guids[i], targetFormat);
        if (SUCCEEDED( hr) )
        {
            break;
        }
    }

    CoTaskMemFree(guids);

DONE:
	return hr;
}


HRESULT DXVAVideoProcessor::_DestroyDXVA2()
{
	pDXVAVP_ = NULL;
	pDXVAVPS_ = NULL;
	return S_OK;
}


HRESULT DXVAVideoProcessor::ProcessVideo(_com_ptr_IDirect3DSurface9 pMainStream,
		_com_ptr_IDirect3DSurface9 pSubStream,
		_com_ptr_IDirect3DSurface9 pRenderTarget)
{
    HRESULT hr = S_OK;

    DXVA2_VideoProcessBltParams blt = {0};
    DXVA2_VideoSample samples[2] = {0};

	RECT srcRect = {0};
	RECT dstRect = {0};
    RECT targetRect = {0};

	if ( pMainStream ==NULL || pRenderTarget==NULL) return E_INVALIDARG;

	D3DSURFACE_DESC surf_desc_src;
	D3DSURFACE_DESC dst_surf_desc;

	pMainStream->GetDesc(&surf_desc_src);
	pRenderTarget->GetDesc(&dst_surf_desc);

	// check source surface type
	switch (surf_desc_src.Format)
	{
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_YUY2:
	case D3DFMT_NV12:
	case D3DFMT_I420:
		break;
	default:
		return E_INVALIDARG;
	}

	// check dest surface type
	switch (dst_surf_desc.Format)
	{
		// only following render targets are supported by DXVA2
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_YUY2:
		break;
	default:
		return E_INVALIDARG;
	}

	// setup source format
	D3DFORMAT sourceFormat = surf_desc_src.Format;
	// setup targetRect format
	D3DFORMAT targetFormat = dst_surf_desc.Format;

	// create processing device
	hr = _CreateDXVA2(sourceFormat, targetFormat, dst_surf_desc.Width, dst_surf_desc.Height);
	if (FAILED(hr)) goto DONE;


	// setup source rect
    SetRect(&srcRect, 0, 0, surf_desc_src.Width, surf_desc_src.Height);

	// setup dest rect of source
	int targetW, targetH;
	if ( surf_desc_src.Width*dst_surf_desc.Height > dst_surf_desc.Width* surf_desc_src.Height) // do letter boxing
	{
		targetW = dst_surf_desc.Width;
		targetH = targetW * surf_desc_src.Height/surf_desc_src.Width;
	}
	else // pillar-boxing
	{
		targetH = dst_surf_desc.Height;
		targetW = targetH * surf_desc_src.Width/surf_desc_src.Height;
	}
	SetRect(&dstRect, (dst_surf_desc.Width - targetW)/2, 
		(dst_surf_desc.Height - targetH)/2, targetW, targetH);

	// setup targetRect rect
    SetRect(&targetRect, 0, 0, dst_surf_desc.Width, dst_surf_desc.Height);

    LONGLONG start_100ns = 1 * LONGLONG(VIDEO_100NSPF);
    LONGLONG end_100ns   = start_100ns + LONGLONG(VIDEO_100NSPF);

    // Initialize VPBlt parameters.
    blt.TargetFrame = start_100ns;
    blt.TargetRect  = targetRect;

    // DXVA2_VideoProcess_Constriction
    blt.ConstrictionSize.cx = targetRect.right - targetRect.left;
    blt.ConstrictionSize.cy = targetRect.bottom - targetRect.top;
	
	D3DCOLOR rgb_bg  = D3DCOLOR_XRGB(0x10, 0x10, 0xEB);
    blt.BackgroundColor = _GetBackgroundColor(rgb_bg);

    // DXVA2_VideoProcess_YUV2RGBExtended
    blt.DestFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    blt.DestFormat.NominalRange           = DXVA2_NominalRange_Unknown;
    blt.DestFormat.VideoTransferMatrix    = DXVA2_VideoTransferMatrix_Unknown;
    blt.DestFormat.VideoLighting          = DXVA2_VideoLighting_dim;
    blt.DestFormat.VideoPrimaries         = DXVA2_VideoPrimaries_BT709;
    blt.DestFormat.VideoTransferFunction  = DXVA2_VideoTransFunc_709;
    blt.DestFormat.SampleFormat           = DXVA2_SampleProgressiveFrame;

    // DXVA2_ProcAmp_XXX
    blt.ProcAmpValues.Brightness = procAmpValues_[0];
    blt.ProcAmpValues.Contrast = procAmpValues_[1];
    blt.ProcAmpValues.Hue = procAmpValues_[2];
    blt.ProcAmpValues.Saturation = procAmpValues_[3];

    // DXVA2_VideoProcess_AlphaBlend
    blt.Alpha = DXVA2_Fixed32OpaqueAlpha();

    // DXVA2_VideoProcess_NoiseFilter
    blt.NoiseFilterLuma.Level       = nFilterValues_[0];
    blt.NoiseFilterLuma.Threshold   = nFilterValues_[1];
    blt.NoiseFilterLuma.Radius      = nFilterValues_[2];
    blt.NoiseFilterChroma.Level     = nFilterValues_[3];
    blt.NoiseFilterChroma.Threshold = nFilterValues_[4];
    blt.NoiseFilterChroma.Radius    = nFilterValues_[5];

    // DXVA2_VideoProcess_DetailFilter
    blt.DetailFilterLuma.Level       = dFilterValues_[0];
    blt.DetailFilterLuma.Threshold   = dFilterValues_[1];
    blt.DetailFilterLuma.Radius      = dFilterValues_[2];
    blt.DetailFilterChroma.Level     = dFilterValues_[3];
    blt.DetailFilterChroma.Threshold = dFilterValues_[4];
    blt.DetailFilterChroma.Radius    = dFilterValues_[5];

    // Initialize main stream video sample.
    samples[0].Start = start_100ns;
    samples[0].End   = end_100ns;

    // DXVA2_VideoProcess_YUV2RGBExtended
    samples[0].SampleFormat.VideoChromaSubsampling = DXVA2_VideoChromaSubsampling_MPEG2;
    samples[0].SampleFormat.NominalRange           = DXVA2_NominalRange_16_235;
    samples[0].SampleFormat.VideoTransferMatrix    = DXVA2_VideoTransferMatrix_BT601;
    samples[0].SampleFormat.VideoLighting          = DXVA2_VideoLighting_dim;
    samples[0].SampleFormat.VideoPrimaries         = DXVA2_VideoPrimaries_BT709;
    samples[0].SampleFormat.VideoTransferFunction  = DXVA2_VideoTransFunc_709;

    samples[0].SampleFormat.SampleFormat = DXVA2_SampleProgressiveFrame;
    samples[0].SrcSurface = pMainStream;

    // DXVA2_VideoProcess_SubRects
    samples[0].SrcRect = srcRect;

    // DXVA2_VideoProcess_StretchX, Y
    samples[0].DstRect = dstRect;

    // DXVA2_VideoProcess_PlanarAlpha
    samples[0].PlanarAlpha = DXVA2FloatToFixed( 0xFF / 0xFF);

    //// Initialize sub stream video sample.
    //samples[1] = samples[0];
    //// DXVA2_VideoProcess_SubStreamsExtended
    //samples[1].SampleFormat = samples[0].SampleFormat;
    //// DXVA2_VideoProcess_SubStreams
    //samples[1].SampleFormat.SampleFormat = DXVA2_SampleSubStream;
    //samples[1].SrcSurface = pSubStream_;
    //samples[1].SrcRect = VIDEO_SUB_RECT;
    //samples[1].DstRect = ScaleRectangle(ssdest, VIDEO_MAIN_RECT, client);


	// do processing
    hr = pDXVAVP_->VideoProcessBlt(pRenderTarget, &blt,  samples,   1,   NULL);

DONE:
    return hr;
}

}