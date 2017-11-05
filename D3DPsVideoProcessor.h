#pragma once
/////////////////////////////////////////////////////////
// Processing video by using D3D pipeline
#include "ComPtrDefs.h"

#define D3DFMT_I420 (D3DFORMAT)MAKEFOURCC('I', '4', '2', '0') 
#define D3DFMT_YV12 (D3DFORMAT)MAKEFOURCC('Y', 'V', '1', '2') 
#define D3DFMT_NV12 (D3DFORMAT)MAKEFOURCC('N', 'V', '1', '2') 

namespace WinRTCSDK{

//Flexible Vertex Format, FVF
struct CustomVertex
{
	FLOAT       x, y, z, rhw;   
	D3DCOLOR    diffuse;    
	FLOAT       tu, tv; // texture coordinates
};

// coustom vertex format
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)

class D3DPsVideoProcessor
{
public:
	D3DPsVideoProcessor(void);
	~D3DPsVideoProcessor(void);
	HRESULT   Init(_com_ptr_IDirect3DDevice9Ex pd3dDevice_, UINT nWidth, UINT nHeight, MFRatio par,
		UINT nBackbufferWidth, UINT nBackbufferHeight, 
		D3DFORMAT nPixelFormat);
	void      UnInit();
	HRESULT   AlphaBlendingIcon(_com_ptr_IDirect3DTexture9  pIcon, RECT destRC);
	HRESULT   DrawVideo(BYTE* pImage, int stride, bool flipHorizontally, bool flipVertically); // need to be called between a BeginScene/EndScene pair 
private:
	HRESULT   _UpdateTexture(BYTE* pImage, int stride);
	HRESULT   _UpdateVertices(bool flipHorizontally, bool flipVertically);
	void      _MakeQuadVertices( bool bFlipHorizontally, bool bFlipVertically, CustomVertex * pVertex);
	HRESULT   _CompileShader( _In_ LPCVOID  src, SIZE_T size, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob );
	HRESULT   _LoadTexture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int height);
private:
	_com_ptr_IDirect3DDevice9Ex     pd3dDevice_;
	
	_com_ptr_IDirect3DTexture9      pd3dTexture0_;
	_com_ptr_IDirect3DTexture9      pd3dTexture1_; // for U plane for I420 data
	_com_ptr_IDirect3DTexture9      pd3dTexture2_; // for V plane for I420 data

	_com_ptr_IDirect3DVertexBuffer9 pd3dVertexBuffer_;
	_com_ptr_IDirect3DVertexBuffer9 pd3dVertexBuffer4Icon_;
	_com_ptr_IDirect3DPixelShader9  pPixelShader_;

	// dimension of the image
	UINT         nWidth_;
	UINT         nHeight_;
	MFRatio      par_; //pixel aspect ratio
	// dimensioin of the  swapchain
	UINT         nBackbufferWidth_;
	UINT         nBackbufferHeight_;

	D3DFORMAT    pixelFormat_;
	bool         bFlipHorizontally_;
	bool         bFlipVertically_;
	bool         bNoYUY2Texture_;
};

}