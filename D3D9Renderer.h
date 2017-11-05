#pragma once

#include <map>
#include <memory>
#include <d3dcompiler.h>


#include "BaseWnd.h"
#include "Looper.h"
#include "ComPtrDefs.h"
#include "D3DPsVideoProcessor.h"
#include "DXVAVideoProcessor.h"


using namespace std;

namespace WinRTCSDK{

#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2')
#define D3DFMT_I420 (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0')
#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2')

struct VideoFrameInfo
{
	VideoFrameInfo() : pSample(NULL), 
		format(D3DFMT_UNKNOWN), image(NULL), width(0), height(0),
		stride(0), rotation(0), bFlipHorizontally(FALSE), 
		bFlipVertically(FALSE), bMicMuteIcon(FALSE)
	{
		par.Numerator=1;
		par.Denominator=1;
	}

	// for DXVA
	_com_ptr_IDirect3DSurface9 pSample;
	// for software decoder
	D3DFORMAT format;
	BYTE     *image; 
	int       width;
	int       height;
	int       stride;
	MFRatio   par; // pixel aspect ratio
	int       rotation; // 0:0, 1:90, 2:180, 3:270
	// common info
	BOOL      bFlipHorizontally;
	BOOL      bFlipVertically;
	BOOL      bMicMuteIcon;
};

struct IconData
{
	IconData(){argb = NULL; width=0; height=0;}
	~IconData(){ if ( argb ) free (argb);}
	
	VOID Set(BYTE* icon, int w, int h)
	{
		if (argb) free (argb); // free old one
		width = w;
		height = h;
		argb = (BYTE*) ::malloc(w * h * 4);
		::memcpy(argb, icon, w*h*4);
	}
	BYTE* argb; int width; int height ;
private : 
	IconData & operator = (IconData & r);
	IconData (IconData & r);
};

class D3D9Video
{
private:
	_com_ptr_IDirect3D9Ex            pD3DEx_; // this is a passed in param

	_com_ptr_IDirect3DSwapChain9     pSwapChain_;
    _com_ptr_IDirect3DDevice9Ex      pDevice_;
    _com_ptr_IDirect3DDeviceManager9 pD3DDevManager_;

	INT                              width_;
    INT                              height_;
	HWND                             hwnd_; // for windowed render
	_com_ptr_IDirect3DTexture9       micMuteIcon_;
	// video processing
	D3DPsVideoProcessor             *pPsVideoProcessor_;
	DXVAVideoProcessorPtr            pDXVAProcessor_;
	D3DFORMAT                        d3dformat_;
	UINT                             resetToken_;

private:
	HRESULT   _Destroy ();
    HRESULT   _DrawVideoPS(VideoFrameInfo &frame);
    HRESULT   _DrawVideoDXVA(VideoFrameInfo &frame);

public:
    ~D3D9Video ();
    D3D9Video (_com_ptr_IDirect3D9Ex pD3DEx, HWND hwnd);

public:
	D3DPRESENT_PARAMETERS            d3dpp_ ;
	_com_ptr_IDirect3DDeviceManager9 GetD3DDeviceManager9();
	HRESULT   Create (const IconData * iconData);
	HRESULT   Reset(UINT width, UINT height, MFRatio par, D3DFORMAT fmt); // reset the video on first frame!
	HRESULT   DrawVideo(VideoFrameInfo &frame);
	HRESULT   Clear(BYTE r, BYTE g, BYTE b);
	HRESULT   GetD3DFormat(int &w, int& h, D3DFORMAT& d3dfmt);
};

// this is thread-unsafe class. methods must be called by one thread simultaneously.
class D3D9Renderer 
{
private:
	HWND                           hwnd_;
	Looper&	                       dispatcher_;
	D3D9Video                     *pD3DVideo_;   
	_com_ptr_IDirect3D9Ex          pD3DEx_;
	IconData                       muteIconData_;
	bool                           isMute_;

private:
    void       _Destroy();
	HRESULT    _CheckAndResetD3DVideo(int width, int height, MFRatio par, D3DFORMAT fmt);

public:
    D3D9Renderer(Looper& dispatcher, HWND hwnd, const IconData * icon );
    ~D3D9Renderer(void);

public:
    HRESULT  Create(_com_ptr_IDirect3D9Ex pD3DEx);
	// draw video frame
	HRESULT  DrawVideo(VideoFrameInfo &frame);
	// clear video frame
	void     Clear(BYTE r, BYTE g, BYTE b);
	void     SetMute(bool isMute);
	// reset video
	HRESULT  ResetVideo();
	// provide DXVA for other modules
	_com_ptr_IDirect3DDeviceManager9 GetD3DDeviceManager9();

	HWND GetWindow();



};

} // namespace WinRTCSDK