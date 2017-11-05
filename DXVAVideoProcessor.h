///////////////////////////////////////////////////
//  processing video by using DXVA2

#pragma once
#include <map>
#include <memory>

namespace WinRTCSDK{

class DXVAVideoProcessor
{
private:

	_com_ptr_IDirect3DDeviceManager9       pDevManager_;  
	_com_ptr_IDirectXVideoProcessorService pDXVAVPS_ ;
	_com_ptr_IDirectXVideoProcessor        pDXVAVP_ ;

	// current parameter
	D3DFORMAT                sourceFormat_;
	D3DFORMAT                targetFormat_;
	int                      targetWidth_;
	int                      targetHeight_;

	GUID                     vpGuid_    ;
	DXVA2_VideoDesc          videoDesc_ ;
	DXVA2_VideoProcessorCaps vpCaps_    ;

	INT                      procAmpSteps_[4] ;
	DXVA2_ValueRange         procAmpRanges_[4];
	DXVA2_ValueRange         nFilterRanges_[6];
	DXVA2_ValueRange         dFilterRanges_[6];

	DXVA2_Fixed32            procAmpValues_[4];
	DXVA2_Fixed32            nFilterValues_[6];
	DXVA2_Fixed32            dFilterValues_[6];


public:

	DXVAVideoProcessor(_com_ptr_IDirect3DDeviceManager9 pMan);
	~DXVAVideoProcessor();

public:

	HRESULT ProcessVideo(_com_ptr_IDirect3DSurface9 pMainStream,
		_com_ptr_IDirect3DSurface9 pSubStream,
		_com_ptr_IDirect3DSurface9 pRenderTarget);
	
	HRESULT CreateDXVA2 (D3DFORMAT sourceFormat, D3DFORMAT targetFormat, int w, int h);
	HRESULT CreateProcessorTarget (D3DFORMAT fmt, int w, int h, IDirect3DSurface9 ** pTarget);
private:
	HRESULT _CreateDXVA2VPDevice(REFGUID vpGuid, D3DFORMAT targetFormat);
	HRESULT _CreateDXVA2(D3DFORMAT sourceFormat, D3DFORMAT targetFormat, int w, int h);
	HRESULT _DestroyDXVA2();
};

typedef std::shared_ptr<DXVAVideoProcessor> DXVAVideoProcessorPtr;

} // WinRTCSDK
