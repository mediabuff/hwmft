#include "stdafx.h"
#include "d3d9renderer.h"
#include <sstream>  
#include "NativeMediaLogger4CPP.h"

namespace WinRTCSDK{

using namespace Native;


static int __log_threshold = 0;

#define CHECK_FAILED_GOTO_DONE(hr, tag, fmt, ...) \
{ \
	if ( FAILED(hr)) { \
		if (__log_threshold < 100 ){ \
			NativeMediaLogger4CPP::log(Native::Error, tag, fmt, ##__VA_ARGS__); \
			__log_threshold++; \
		} \
		goto DONE; \
	} \
}

D3D9Video::D3D9Video ( _com_ptr_IDirect3D9Ex pD3DEx,  HWND hwnd)
	:pDXVAProcessor_(nullptr)
{
    width_ = 1;
    height_ = 1;  
    pD3DEx_ = pD3DEx;
	hwnd_ = hwnd;
	d3dformat_ = D3DFMT_UNKNOWN;
	pPsVideoProcessor_ = new D3DPsVideoProcessor();
}

D3D9Video::~D3D9Video ()
{
	_Destroy();
	delete pPsVideoProcessor_;
}

HRESULT D3D9Video::_Destroy ()
{
	pSwapChain_ = NULL;
    pDevice_ = NULL;
    pD3DDevManager_ = NULL;
	resetToken_ = 0;
	return S_OK;
}

HRESULT D3D9Video::Create(const IconData * iconData)
{
	HRESULT hr;

    memset (&d3dpp_, 0, sizeof (D3DPRESENT_PARAMETERS));
    d3dpp_.BackBufferFormat     = D3DFMT_X8R8G8B8;
    d3dpp_.SwapEffect           = D3DSWAPEFFECT_FLIPEX;
    d3dpp_.PresentationInterval = D3DPRESENT_INTERVAL_ONE;
    d3dpp_.Windowed             = TRUE;
    d3dpp_.hDeviceWindow        = hwnd_;
	d3dpp_.BackBufferHeight     = 1;
	d3dpp_.BackBufferWidth      = 1;
	d3dpp_.BackBufferCount      = 4;
	d3dpp_.Flags                = D3DPRESENTFLAG_VIDEO;

	//Get Adapter Information
	D3DDISPLAYMODE mode = { 0 };
    hr = pD3DEx_->GetAdapterDisplayMode( D3DADAPTER_DEFAULT,  &mode );
	if (FAILED(hr)) goto DONE;

	D3DCAPS9 caps;
	hr = pD3DEx_->GetDeviceCaps(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,&caps);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, pD3DEx_->GetDeviceCaps hr = %x", hr);

	DWORD vertexProcessing = D3DCREATE_HARDWARE_VERTEXPROCESSING;
	if (!(caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT))
	{
		CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, D3DCREATE_HARDWARE_VERTEXPROCESSING not supported.");
		vertexProcessing = D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

    hr = pD3DEx_->CreateDeviceEx(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		hwnd_, vertexProcessing |D3DCREATE_MULTITHREADED|
		D3DCREATE_ENABLE_PRESENTSTATS | D3DCREATE_FPU_PRESERVE,
        &d3dpp_, NULL, &pDevice_ );

	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, pD3DEx_->CreateDeviceEx hr = %x", hr);
	
	// get the swapchain
	hr = pDevice_->GetSwapChain(0, &pSwapChain_);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, pDevice_->GetSwapChain hr = %x", hr);

	// create d3d manager, for DXVA decoders
    hr = ::DXVA2CreateDirect3DDeviceManager9(&resetToken_, &pD3DDevManager_);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, DXVA2CreateDirect3DDeviceManager9 hr = %x", hr);

    hr = pD3DDevManager_->ResetDevice(pDevice_, resetToken_);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Create, pD3DDevManager_->ResetDevice hr = %x", hr);

	// create DXVA processor
	pDXVAProcessor_=std::make_shared<DXVAVideoProcessor>(pD3DDevManager_);

	// create the mute icon
	if ( iconData != NULL && iconData->argb!=NULL)
	{
		HRESULT hr = pDevice_->CreateTexture(iconData->width, iconData->height, 1, D3DUSAGE_DYNAMIC,
			D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, &micMuteIcon_, NULL );
		if (FAILED(hr)) { goto DONE;}
		D3DLOCKED_RECT lr;
		hr = micMuteIcon_->LockRect(0, &lr, NULL, D3DLOCK_DISCARD );
		if (FAILED(hr)) { goto DONE;}
		BYTE * srcline = iconData->argb;
		BYTE * destline = (BYTE*)lr.pBits;
		for (int j=0; j<iconData->height; j++)
		{
			::memcpy(destline, srcline, iconData->width*4);
			srcline += iconData->width*4;
			destline += lr.Pitch;
		}
		hr = micMuteIcon_->UnlockRect(0);
	}

DONE:
	if (FAILED(hr)){
		NativeMediaLogger4CPP::log(Error, "D3D9Video", "Failed to create VideoRender hr=%x", hr); 
	}
	return hr;

}

static void _Make16To9Backbuffer(UINT w, UINT h, UINT& ow, UINT& oh)
{
	double r = (w * 1.0) / (h * 1.0);
	double r16_9 = 16.0/9.0;
	double threshold = 0.1;
	if ( w < 400 ){ // increase the threshold for resolution below 360p (for example:320 * 176 )
		threshold = 0.2;
	}

	if ( ::abs( r16_9 - r) < threshold){ // treat the image as 16:9
		ow = w;
		oh = h;
		return;
	}else{
		if ( r > r16_9){ // letter-boxing
			ow = w;
			oh = (double)ow/r16_9;
		}else{
			oh = h;
			ow =  (double)oh * r16_9;
		}
	}
}

HRESULT D3D9Video::Reset(UINT width, UINT height, MFRatio par, D3DFORMAT fmt)
{
	HRESULT hr;
    width_ = width;
    height_ = height;  
	d3dformat_ = fmt;
	UINT backBufferW;
	UINT backBufferH;

	if ( pDevice_ == NULL) return E_FAIL;

	// calculate backbuffer size based on image size
//	_Make16To9Backbuffer(width_, height_, backBufferW, backBufferH);
	RECT rc;
	GetWindowRect(hwnd_, &rc);
	bool windowsSizedChanged = false;
	backBufferW = rc.right - rc.left;
	backBufferH = rc.bottom - rc.top;

	d3dpp_.BackBufferWidth = backBufferW;
	d3dpp_.BackBufferHeight = backBufferH;

	hr = pDevice_->ResetEx(&d3dpp_, NULL);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Reset, pDevice_->ResetEx hr = %x, backBufferW=%d, backBufferH=%d", 
		hr, backBufferW, backBufferH);

	hr = pD3DDevManager_->ResetDevice(pDevice_, resetToken_);
	CHECK_FAILED_GOTO_DONE(hr, "D3D9Video", "Reset, pD3DDevManager_->ResetDevice hr = %x, backBufferW=%d, backBufferH=%d", 
		hr, backBufferW, backBufferH);

	pPsVideoProcessor_->UnInit();
	pPsVideoProcessor_->Init(pDevice_, width, height, par, backBufferW, backBufferH, fmt);
	
DONE:
	return hr;
}

// for software decoder, using shader
HRESULT D3D9Video::_DrawVideoPS(VideoFrameInfo &frame)
{
	HRESULT hr;
	if ( frame.image == NULL) return E_INVALIDARG;
	
	RECT rcIcon;
	// calculate icon position
	if(frame.bMicMuteIcon && micMuteIcon_!=NULL){
		RECT rcHwnd;
		::GetWindowRect(hwnd_, &rcHwnd);
		D3DSURFACE_DESC surf_desc;
		hr = micMuteIcon_->GetLevelDesc(0, &surf_desc);
		if ( FAILED(hr)) goto done;

		float r = 1.0;
		if ( rcHwnd.right - rcHwnd.left > 0){
			r =  0.75f *(1.0f * width_)/(1.0f * (rcHwnd.right - rcHwnd.left)) ; 
		}
		::SetRect(&rcIcon, 0, 0, surf_desc.Width * r, surf_desc.Height * r);//Target rect of the icon
	}
		
	// begin drawing ...
	hr = pDevice_->BeginScene();
	if ( FAILED(hr)) goto done;
	hr = pPsVideoProcessor_->DrawVideo(frame.image, frame.stride, frame.bFlipHorizontally, frame.bFlipVertically);
	if ( FAILED(hr)){
		pDevice_->EndScene();
		goto done;
	}
	 // need to blending an icon...
	if(frame.bMicMuteIcon && micMuteIcon_!=NULL){
		hr = pPsVideoProcessor_->AlphaBlendingIcon(micMuteIcon_, rcIcon);
	}
	if ( FAILED(hr)){
		pDevice_->EndScene();
		goto done;
	}

	hr = pDevice_->EndScene();
	if ( FAILED(hr)) goto done;
	// present now!
	hr = pDevice_->PresentEx(NULL, NULL, NULL, NULL, 0);

done:
	return hr;
}

// for DXVA
HRESULT D3D9Video::_DrawVideoDXVA(VideoFrameInfo &frame)
{
	HRESULT hr;	
	if (frame.pSample == NULL)
		return E_INVALIDARG;

	_com_ptr_IDirect3DSurface9 pRT = NULL;
	hr = pSwapChain_->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO , &pRT);
	if (FAILED(hr)) { goto DONE;}

	RECT srcRC, dstRC;
	// aspect ratio correction:
	// rationale: currently the video window is always displayed as 16:9
	//  so we will stretch the video based on the backbuffer dimension. 
	D3DSURFACE_DESC surf_desc;
	hr = pRT->GetDesc(&surf_desc);
	if (FAILED(hr)) { goto DONE;}

	// setup src rect
	// we use the full image
	::SetRect(&srcRC, 0, 0, frame.width, frame.height);

	// setup dst rect
	double r16to9 = 16.0/9.0;
	double dar =  (double) frame.par.Numerator * (double) frame.width / 
		( (double) frame.par.Denominator * (double) frame.height );

	double v_scale = 1.0;
	double h_scale = 1.0;
	if(dar < r16to9){
		h_scale = dar / r16to9;
	}else{
		v_scale = r16to9/dar;
	}

	int dw, dh;
	dw =  surf_desc.Width * h_scale;
	dh = surf_desc.Height * v_scale;

	int left =  (surf_desc.Width - dw)/2;
	int top  =  (surf_desc.Height- dh)/2;
	::SetRect(&dstRC, left, top, left+dw, top+dh); 

	hr = pDevice_->StretchRect(frame.pSample, &srcRC, pRT, &dstRC,  D3DTEXF_NONE);
	if (FAILED(hr)) { goto DONE;}

	// we will use DXVA later for flexibility

//	hr = pDXVAProcessor_->ProcessVideo(frame.pSample, nullptr, pRT);

	hr = pDevice_->PresentEx(NULL, NULL, NULL, NULL, 0);

DONE:
	return hr;
}

HRESULT D3D9Video::DrawVideo(VideoFrameInfo &frame)
{
	HRESULT hr;	
	if ( pDevice_ == NULL) return E_INVALIDARG;

	if (frame.pSample){// DXVA decoder
		hr = _DrawVideoDXVA(frame);
	}else{// software decoder
		hr = _DrawVideoPS(frame);
	}
	return hr;
}

HRESULT D3D9Video::GetD3DFormat(int& w, int& h, D3DFORMAT& d3dfmt)
{
	w = width_;
	h = height_;
	d3dfmt = d3dformat_;
	return S_OK;
}

_com_ptr_IDirect3DDeviceManager9 D3D9Video::GetD3DDeviceManager9()
{
	return pD3DDevManager_;
}
	
HRESULT D3D9Video::Clear(BYTE r, BYTE g, BYTE b)
{
	if ( pDevice_ == NULL) return E_INVALIDARG;
	_com_ptr_IDirect3DSurface9 pRT = NULL;
	HRESULT hr = pSwapChain_->GetBackBuffer(0, D3DBACKBUFFER_TYPE_MONO , &pRT);
	if (FAILED(hr)) { goto DONE;}
	hr = pDevice_->ColorFill(pRT, NULL, D3DCOLOR_XRGB(r, g, b));
	if (FAILED(hr)) { goto DONE;}
	hr = pDevice_->PresentEx(NULL, NULL, NULL, NULL, 0);
DONE:
	return hr;
}

//-------------------------------------------------------------------
// D3D9Renderer  Class
//-------------------------------------------------------------------
D3D9Renderer::D3D9Renderer(Looper& dispatcher, HWND hwnd, const IconData * icon ):
	pD3DVideo_(NULL),dispatcher_(dispatcher),hwnd_(hwnd),
    pD3DEx_(NULL),isMute_(false)
{
	////// make a fake icon
	//DWORD *data = new DWORD[100 * 100];
	//DWORD *pixel = data;
	//for (int j=0; j<100 * 100; j++){
	//	*pixel = D3DCOLOR_ARGB(100, 70, 80, 90);
	//	pixel++;
	//}
	//muteIconData_.Set((BYTE*)data, 100, 100);
	//delete [] data;
	////// fake icon for test


	if ( icon ){
		muteIconData_.Set(icon->argb, icon->width, icon->height);
	}

    return ;
}

D3D9Renderer::~D3D9Renderer()
{
    _Destroy();
}

HRESULT D3D9Renderer::Create(_com_ptr_IDirect3D9Ex  pD3DEx)
{
	HRESULT hr = S_OK;

    // Create the Direct3D object.
	if ( pD3DEx_ == NULL){
		if (pD3DEx == NULL){// use outer D3d9 obj
			HRESULT hr = Direct3DCreate9Ex(D3D_SDK_VERSION, &pD3DEx_);
			if (FAILED(hr) ){
				return hr;
			}
		}else{
			pD3DEx_ = pD3DEx;
		}
	}

	if (pD3DVideo_ != NULL) return hr;

	// create new one
	pD3DVideo_ = new  D3D9Video (pD3DEx_, hwnd_); 
	// d3d device need to be created/rest by a same thread.
	LooperTask t = [=, &hr]{ hr = pD3DVideo_->Create( &muteIconData_);};
	dispatcher_.runSyncTask(t);


    return hr;
}

void D3D9Renderer::_Destroy()
{
	if ( pD3DVideo_ ){ 
		delete pD3DVideo_;
		pD3DVideo_ = NULL;
	}


}

HRESULT D3D9Renderer::_CheckAndResetD3DVideo(int width, int height, MFRatio par, D3DFORMAT fmt)
{
	HRESULT hr = S_OK;

	int w, h;
	D3DFORMAT d3dfmt;
	pD3DVideo_->GetD3DFormat(w, h, d3dfmt);
	bool videoChanged = (w!=width || h!= height || d3dfmt!=fmt);

	RECT rc;
	GetWindowRect(hwnd_, &rc);
	bool windowsSizedChanged = false;
	if ( pD3DVideo_->d3dpp_.BackBufferWidth != rc.right-rc.left || 
		pD3DVideo_->d3dpp_.BackBufferHeight != rc.bottom-rc.top)
	{
		windowsSizedChanged = true;
	}

	if (videoChanged||windowsSizedChanged)
	{ 
		// video resolution changed, resize the device
		// d3d device need to be created/rest by a same thread.
		LooperTask t = [=,&hr]{ hr = pD3DVideo_->Reset(width, height, par, fmt);};
		dispatcher_.runSyncTask(t);
	}

	return hr;
}

// HW codec can allocate a little bigger buffer (which is 16 pixels aligned at width and height)
//  if the video size is not set, we need to deduce it .
#define _MY_ALIGN16(value)  (((value + 15) >> 4) << 4) // round up to a multiple of 16
static void _fix_video_size( VideoFrameInfo &frame )
{
	if ( frame.pSample && (frame.width == 0 || frame.height == 0) )
	{
		D3DSURFACE_DESC surf_desc;
		frame.pSample->GetDesc(&surf_desc);

		// we assume width is always 16 aligned
		frame.width = surf_desc.Width;
		switch (surf_desc.Height) // fixup height
		{
		case _MY_ALIGN16(180):
			frame.height = 180;
			break;
		case _MY_ALIGN16 (360):
			frame.height = 360;
			break;
		case _MY_ALIGN16(1080):
			frame.height = 1080;
			break;
		default:
			frame.height = surf_desc.Height;
		}
	}

}

HRESULT D3D9Renderer::DrawVideo(VideoFrameInfo &frame)
{
	HRESULT hr;
	if ( pD3DVideo_ == NULL) return E_ILLEGAL_METHOD_CALL;

	// fix video size for HW decoder
	_fix_video_size(frame);

	if ( frame.pSample ) // DXVA decoder
	{
		D3DSURFACE_DESC surf_desc;
		frame.pSample->GetDesc(&surf_desc);
		hr = _CheckAndResetD3DVideo(frame.width, frame.height, frame.par, surf_desc.Format);
	}
	else // software decoder
	{
		hr = _CheckAndResetD3DVideo(frame.width, frame.height, frame.par, frame.format);
	}

	if (FAILED(hr))  goto DONE;
	hr =  pD3DVideo_->DrawVideo(frame);

DONE:
	return hr;
}

void D3D9Renderer::Clear(BYTE r, BYTE g, BYTE b)
{
	if ( pD3DVideo_ == NULL) return;
	pD3DVideo_->Clear(r, g, b);
}

void D3D9Renderer::SetMute(bool isMute)
{
	isMute_ = isMute;
}

// provide DXVA for other modules
_com_ptr_IDirect3DDeviceManager9 D3D9Renderer::GetD3DDeviceManager9()
{
	if ( pD3DVideo_ == NULL) return NULL;

	return pD3DVideo_->GetD3DDeviceManager9();
}

HRESULT D3D9Renderer::ResetVideo()
{
	// TODO: reset individual devices
	return S_OK;
}

HWND D3D9Renderer::GetWindow() 
{
	return hwnd_ ;
}


} // namespace WinRTCSDK

