#include "stdafx.h"
#include <string>
#include <stdio.h>
#include <d3dcompiler.h>

#include "D3DPsVideoProcessor.h"
#include "NativeMediaLogger4CPP.h"

//#pragma comment(lib,"d3dcompiler.lib")

namespace WinRTCSDK{

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
#define RESTRAINED_INFO( tag, fmt, ...) \
{ \
	if (__log_threshold < 100 ){ \
		NativeMediaLogger4CPP::log(Native::Info, tag, fmt, ##__VA_ARGS__); \
		__log_threshold++; \
	} \
}

// In order to remove the d3dcompiler_xx.dll in our software package,
//  we load the compiled shader from an object file
class PixelShaderLoader 
{
public:
	PixelShaderLoader( const char * file ):shader_(NULL), len_(0)
	{
		CHAR moduleFileName[512];
		CHAR csoFileName[512]={""};

		CHAR * pFilePos = NULL;
		// get module name of the exe
		DWORD ret = ::GetModuleFileNameA(NULL, moduleFileName, 512);
		if (ret>0){
			::GetFullPathNameA(moduleFileName, 512, csoFileName, &pFilePos);
		}

		if ( pFilePos){
			StrCpyA(pFilePos, file);
		}else{
			StrCpyA(csoFileName, file);
		}

		FILE * ps = fopen (csoFileName, "rb");
		if(ps){
			fseek(ps, 0, SEEK_END);
			len_ = ftell(ps);
			fseek(ps, 0, SEEK_SET);
			shader_ = (DWORD*)::malloc(len_);
			size_t  iRead = fread_s(shader_, len_, len_, 1, ps);
			fclose(ps);
		}
	}
	~PixelShaderLoader(){
		::free(shader_);
	}
	DWORD * GetShaderBuffer(){
		return shader_;
	}
	int GetSharderLen(){
		return len_;
	}

private :
	DWORD * shader_;
	size_t  len_;
};

static PixelShaderLoader sharderLoader_("I420ToARGB.cso");

D3DPsVideoProcessor::D3DPsVideoProcessor()
{
	nWidth_ = 0;
	nHeight_ = 0;
    bFlipHorizontally_ =false;
	bFlipVertically_=false;
}

D3DPsVideoProcessor::~D3DPsVideoProcessor()
{
}

void D3DPsVideoProcessor::UnInit()
{
	// just force the smart pointers to release objects now
	pd3dDevice_=NULL;

	pd3dTexture0_=NULL;
	pd3dTexture1_=NULL;
	pd3dTexture2_=NULL;

	pd3dVertexBuffer_=NULL;
	pd3dVertexBuffer4Icon_=NULL;
	pPixelShader_=NULL;
}

HRESULT D3DPsVideoProcessor::_CompileShader( _In_ LPCVOID  src, SIZE_T size, _In_ LPCSTR entryPoint, _In_ LPCSTR profile, _Outptr_ ID3DBlob** blob )
{
    if ( !src || !entryPoint || !profile || !blob )
       return E_INVALIDARG;

    *blob = nullptr;

    UINT flags = D3DCOMPILE_ENABLE_STRICTNESS| D3DCOMPILE_DEBUG;

    const D3D_SHADER_MACRO defines[] = 
    {
        "EXAMPLE_DEFINE", "1",
        NULL, NULL
    };
	HRESULT hr = S_OK;
    ID3DBlob* shaderBlob = nullptr;
    ID3DBlob* errorBlob = nullptr;
    //hr = ::D3DCompile( src, size, NULL, defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		//entryPoint, profile,
		//flags, 0, &shaderBlob, &errorBlob );

    if ( FAILED(hr) )
    {
        if ( errorBlob )
        {
            OutputDebugStringA( (char*)errorBlob->GetBufferPointer() );
            errorBlob->Release();
        }

        if ( shaderBlob )
           shaderBlob->Release();
        return hr;
    }    

    *blob = shaderBlob;
    return hr;
}


HRESULT D3DPsVideoProcessor::Init(_com_ptr_IDirect3DDevice9Ex pd3dDevice, UINT nWidth, UINT nHeight,  MFRatio par,
								  UINT nBackbufferWidth, UINT nBackbufferHeight, 
								  D3DFORMAT pixelFormat)
{
	HRESULT hr;
	pd3dDevice_ = pd3dDevice;
	nWidth_ = nWidth;
	nHeight_ = nHeight;
	par_ = par;
	nBackbufferWidth_ = nBackbufferWidth;
	nBackbufferHeight_ = nBackbufferHeight;

	pixelFormat_ = pixelFormat;
	bNoYUY2Texture_ = false;
	if ( pd3dDevice_ == NULL) return E_INVALIDARG;


	//ID3DBlob *psBlob = nullptr;
	//hr = _CompileShader(_strPixelShaderCode.c_str(), _strPixelShaderCode.size(),  "main", "ps_2_0", &psBlob);
	//if (FAILED(hr)) goto DONE;

	//hr = pd3dDevice_->CreatePixelShader( (DWORD*)psBlob->GetBufferPointer(), &pPixelShader_ );
	hr = pd3dDevice_->CreatePixelShader( sharderLoader_.GetShaderBuffer(), &pPixelShader_ );
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreatePixelShader hr = %x", hr);

	//psBlob->Release();

	// Texture Addressing mode (for fixed function pipeline)
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_ADDRESSU,  D3DTADDRESS_WRAP );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_ADDRESSV,  D3DTADDRESS_WRAP );

	// Texture filter (for fixed function pipeline)
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
	pd3dDevice_->SetSamplerState( 0, D3DSAMP_MIPFILTER, D3DTEXF_POINT );

	// Cull mode
	pd3dDevice_->SetRenderState( D3DRS_CULLMODE, D3DCULL_NONE );

	// Enable alpha blending (frame buffer).
	pd3dDevice_->SetRenderState(D3DRS_ALPHABLENDENABLE,TRUE);
	pd3dDevice_->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
	pd3dDevice_->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
	
	switch (pixelFormat_)
	{
	case D3DFMT_NV12:
	case D3DFMT_I420:
		hr = pd3dDevice_->CreateTexture( nWidth, nHeight,     1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture0_, NULL);
		hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture1_, NULL);
		hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture2_, NULL);
		CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreateTexture fmt = D3DFMT_L8, hr = %x", hr);
		break;

	case D3DFMT_YUY2:
		hr = pd3dDevice_->CreateTexture( nWidth, nHeight,     1, D3DUSAGE_DYNAMIC, D3DFMT_YUY2, D3DPOOL_DEFAULT, &pd3dTexture0_, NULL);

		if ( FAILED(hr)) // We don't have YUY2 Texture on some windows 7 box
		{
			RESTRAINED_INFO( "D3DPsVideoProcessor", "Init, CreateTexture fmt = D3DFMT_YUY2, hr = %x", hr);
			RESTRAINED_INFO( "D3DPsVideoProcessor", "Init, Will load YUY2 data to 3 D3DFMT_L8 textures");
			bNoYUY2Texture_ = true;
			hr = pd3dDevice_->CreateTexture( nWidth, nHeight,     1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture0_, NULL);
			hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture1_, NULL);
			hr = pd3dDevice_->CreateTexture( nWidth/2, nHeight/2, 1, D3DUSAGE_DYNAMIC, D3DFMT_L8, D3DPOOL_DEFAULT, &pd3dTexture2_, NULL);
		}
		CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreateTexture fmt = D3DFMT_L8, hr = %x", hr);
		break;

	case D3DFMT_R8G8B8: //Logitec Camera RGB24 format
		hr = pd3dDevice_->CreateTexture( nWidth, nHeight,     1, D3DUSAGE_DYNAMIC, D3DFMT_X8R8G8B8, D3DPOOL_DEFAULT, &pd3dTexture0_, NULL);
		CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreateTexture fmt = D3DFMT_X8R8G8B8, hr = %x", hr);
		break;
	}
	
	CustomVertex* pVertices = NULL;

	// Vertex buffer for video
	hr = pd3dDevice_->CreateVertexBuffer(4*sizeof(CustomVertex), 
		D3DUSAGE_DYNAMIC,  D3DFVF_CUSTOMVERTEX, 
		D3DPOOL_DEFAULT,  &pd3dVertexBuffer_, NULL ); 
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreateVertexBuffer, hr = %x", hr);

	hr = pd3dVertexBuffer_->Lock(0, 4 * sizeof(CustomVertex), (void**)&pVertices, 0); 
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, pd3dVertexBuffer_->Lock hr = %x", hr);
	_MakeQuadVertices( bFlipHorizontally_, bFlipVertically_, pVertices);
	hr = pd3dVertexBuffer_->Unlock();  
	
	// Vertex buffer for icon
	hr = pd3dDevice_->CreateVertexBuffer(4*sizeof(CustomVertex), 
		D3DUSAGE_DYNAMIC,  D3DFVF_CUSTOMVERTEX, 
		D3DPOOL_DEFAULT,  &pd3dVertexBuffer4Icon_, NULL ); 
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "Init, CreateVertexBuffer (for icon), hr = %x", hr);

	// set constants values
	float offsetConsts [] = { 200.0, 200.0, 200.0, 200.0};
	hr = pd3dDevice_->SetPixelShaderConstantF(0, offsetConsts, 1);


DONE:


	return SUCCEEDED(hr);
}

void _RGB24_TO_RGB32_STRIDE(uint8_t * rgb24, uint8_t * rgb32, int pixelCount)
{
	int i=0;
	for(i=0; i< pixelCount;i++){
		*rgb32++ = *rgb24++;
		*rgb32++ = *rgb24++;
		*rgb32++ = *rgb24++;
		*rgb32++ = 0xff;
	}
}
// Shuffle table for converting RGB24 to ARGB.
typedef __declspec(align(16)) uint8_t uvec8[16];
static const uvec8 kShuffleMaskRGB24ToARGB = {
  0u, 1u, 2u, 12u, 3u, 4u, 5u, 13u, 6u, 7u, 8u, 14u, 9u, 10u, 11u, 15u
};
__declspec(naked)
void _RGB24_TO_RGB32_STRIDE_SSSE3(const uint8_t* src_rgb24, uint8_t* dst_argb, int width) 
{
  __asm {
    mov       eax, [esp + 4]   // src_rgb24
    mov       edx, [esp + 8]   // dst_argb
    mov       ecx, [esp + 12]  // width
    pcmpeqb   xmm5, xmm5       // generate mask 0xff000000
    pslld     xmm5, 24
    movdqa    xmm4, xmmword ptr kShuffleMaskRGB24ToARGB

 convertloop:
    movdqu    xmm0, [eax]
    movdqu    xmm1, [eax + 16]
    movdqu    xmm3, [eax + 32]
    lea       eax, [eax + 48]
    movdqa    xmm2, xmm3
    palignr   xmm2, xmm1, 8    // xmm2 = { xmm3[0:3] xmm1[8:15]}
    pshufb    xmm2, xmm4
    por       xmm2, xmm5
    palignr   xmm1, xmm0, 12   // xmm1 = { xmm3[0:7] xmm0[12:15]}
    pshufb    xmm0, xmm4
    movdqu    [edx + 32], xmm2
    por       xmm0, xmm5
    pshufb    xmm1, xmm4
    movdqu    [edx], xmm0
    por       xmm1, xmm5
    palignr   xmm3, xmm3, 4    // xmm3 = { xmm3[4:15]}
    pshufb    xmm3, xmm4
    movdqu    [edx + 16], xmm1
    por       xmm3, xmm5
    movdqu    [edx + 48], xmm3
    lea       edx, [edx + 64]
    sub       ecx, 16
    jg        convertloop
    ret
  }
}

HRESULT _LoadRGB24ToTexture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int width, int height)
{
	HRESULT hr;
	D3DLOCKED_RECT d3dlr;
	hr = tex->LockRect( 0, &d3dlr, 0, D3DLOCK_DISCARD  );
	if( FAILED(hr)) return hr;
	uint8_t * rgb24line = (uint8_t * ) data;
	uint8_t * rgb32line = (uint8_t * ) d3dlr.pBits;
	for (int i=0; i<height; i++)
	{
		//_RGB24_TO_RGB32_STRIDE(rgb24line, rgb32line, width);
		_RGB24_TO_RGB32_STRIDE_SSSE3(rgb24line, rgb32line, width);
		rgb24line += stride;
		rgb32line += d3dlr.Pitch;
	}
	hr = tex->UnlockRect(0);
	return hr;
}

void _COPY_PLANE(uint8_t * destp, int dest_stride, uint8_t * srcp, int src_stride, int height)
{
	for (int i=0; i<height; i++)
	{
		::memcpy(destp, srcp, src_stride);
		destp += dest_stride;
		srcp += src_stride;
	}
}

HRESULT _Load_I420_To_YV12_Texture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int width, int height)
{

	uint8_t * lpY = (uint8_t *)data;	
	uint8_t * lpU = (uint8_t *)data + stride * height;
	uint8_t * lpV = (uint8_t *)data + stride * height * 5 / 4;

	HRESULT hr;
	D3DLOCKED_RECT d3dlr;
	hr = tex->LockRect( 0, &d3dlr, 0, D3DLOCK_DISCARD  );
	if( FAILED(hr)) return hr;

	uint8_t * destY = (uint8_t * ) d3dlr.pBits;
	uint8_t * destU = (uint8_t * ) d3dlr.pBits + d3dlr.Pitch * height;
	uint8_t * destV = (uint8_t * ) d3dlr.pBits + d3dlr.Pitch * height * 5 / 4;
	
	_COPY_PLANE(destY, d3dlr.Pitch, lpY, stride, height);
	_COPY_PLANE(destU, d3dlr.Pitch/2, lpU, stride/2, height/2);
	_COPY_PLANE(destV, d3dlr.Pitch/2, lpV, stride/2, height/2);
	
	hr = tex->UnlockRect(0);
	return hr;
}

HRESULT D3DPsVideoProcessor::_LoadTexture(_com_ptr_IDirect3DTexture9 tex, PVOID data, int stride, int height)
{
	HRESULT hr;
	D3DLOCKED_RECT d3dlr;
	hr = tex->LockRect( 0, &d3dlr, 0, D3DLOCK_DISCARD  );
	if( FAILED(hr)) return hr;
	for (int i=0; i<height; i++){
		::memcpy((BYTE*)d3dlr.pBits+i*d3dlr.Pitch, (BYTE*)data+i*stride, stride);
	}
	hr = tex->UnlockRect(0);
	return hr;
}


__declspec(naked)
	static void _DEINTERLEAVE_UV_SSE2_ROW(const uint8_t* src_uv,
	uint8_t* dst_u, uint8_t* dst_v, int width) 
{
	__asm
	{
		push       edi
		mov        eax, [esp + 4 + 4]    // src_uv
		mov        edx, [esp + 4 + 8]    // dst_u
		mov        edi, [esp + 4 + 12]   // dst_v
		mov        ecx, [esp + 4 + 16]   // width
		pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
		psrlw      xmm5, 8
		sub        edi, edx

convertloop:
		movdqu     xmm0, [eax]
		movdqu     xmm1, [eax + 16]
		lea        eax,  [eax + 32]
		movdqa     xmm2, xmm0
		movdqa     xmm3, xmm1
		pand       xmm0, xmm5   // even bytes
		pand       xmm1, xmm5
		packuswb   xmm0, xmm1
		psrlw      xmm2, 8      // odd bytes
		psrlw      xmm3, 8
		packuswb   xmm2, xmm3
		movdqu     [edx], xmm0
		movdqu     [edx + edi], xmm2
		lea        edx, [edx + 16]
		sub        ecx, 16
		jg         convertloop

		pop        edi
		ret
	}
}

static HRESULT  _Split_UV_TO_Texture(_com_ptr_IDirect3DTexture9 texU,
		_com_ptr_IDirect3DTexture9 texV, LPBYTE uv, int stride, int width, int height)
{
	HRESULT hr;

	D3DLOCKED_RECT d3dlr_U, d3dlr_V;

	// lock texture for U plane
	hr = texU->LockRect( 0, &d3dlr_U, 0, D3DLOCK_DISCARD );
	if( FAILED(hr)) return hr;

	// lock texture for V plane
	hr = texV->LockRect( 0, &d3dlr_V, 0, D3DLOCK_DISCARD );
	if( FAILED(hr)) {
		texU->UnlockRect(0);
		return hr;
	}

	LPBYTE scan_u = (LPBYTE)d3dlr_U.pBits;
	LPBYTE scan_v = (LPBYTE)d3dlr_V.pBits;

	for (int i=0; i<height; i++){
		_DEINTERLEAVE_UV_SSE2_ROW(uv, scan_u, scan_v, width);
		uv+= stride;
		scan_u += d3dlr_U.Pitch;
		scan_v += d3dlr_V.Pitch;
	}
	hr = texU->UnlockRect(0);
	hr = texV->UnlockRect(0);

	return hr;
}

////////////////////////////////////////////////////////////////////
// We don't have YUY2 texture on some video adapters (on some VM) host, 
//  so we have to load YUY2 data into three textures fo L8 type
static __declspec(naked)
void _YUY2_To_Y_STRIDE_SSE2(const uint8_t* src_yuy2,
	uint8_t* dst_y, int width)
{
	__asm {
		mov        eax, [esp + 4]    // src_yuy2
		mov        edx, [esp + 8]    // dst_y
		mov        ecx, [esp + 12]   // width
		pcmpeqb    xmm5, xmm5        // generate mask 0x00ff00ff
		psrlw      xmm5, 8

convertloop:
		movdqu     xmm0, [eax]
		movdqu     xmm1, [eax + 16]
		lea        eax,  [eax + 32]
		pand       xmm0, xmm5   // even bytes are Y
		pand       xmm1, xmm5
		packuswb   xmm0, xmm1
		movdqu     [edx], xmm0
		lea        edx, [edx + 16]
		sub        ecx, 16
		jg         convertloop
		ret
	}
}

static __declspec(naked)
void _YUY2_TO_UV_STRIDE_SSE2(const uint8_t* src_yuy2, int stride_yuy2,
	uint8_t* dst_u, uint8_t* dst_v, int width) 
{
	__asm {
		push       esi
		push       edi
		mov        eax, [esp + 8 + 4]    // src_yuy2
		mov        esi, [esp + 8 + 8]    // stride_yuy2
		mov        edx, [esp + 8 + 12]   // dst_u
		mov        edi, [esp + 8 + 16]   // dst_v
		mov        ecx, [esp + 8 + 20]   // width
		pcmpeqb    xmm5, xmm5            // generate mask 0x00ff00ff
		psrlw      xmm5, 8
		sub        edi, edx

convertloop:
		movdqu     xmm0, [eax]
		movdqu     xmm1, [eax + 16]
		movdqu     xmm2, [eax + esi]
		movdqu     xmm3, [eax + esi + 16]
		lea        eax,  [eax + 32]
		pavgb      xmm0, xmm2
		pavgb      xmm1, xmm3
		psrlw      xmm0, 8      // YUYV -> UVUV
		psrlw      xmm1, 8
		packuswb   xmm0, xmm1
		movdqa     xmm1, xmm0
		pand       xmm0, xmm5  // U
		packuswb   xmm0, xmm0
		psrlw      xmm1, 8     // V
		packuswb   xmm1, xmm1
		movq       qword ptr [edx], xmm0
		movq       qword ptr [edx + edi], xmm1
		lea        edx, [edx + 8]
		sub        ecx, 16
		jg         convertloop

		pop        edi
		pop        esi
		ret
	}
}
static void _Load_YUY2_TO_3_Textures(_com_ptr_IDirect3DTexture9 texY, _com_ptr_IDirect3DTexture9 texU,
		_com_ptr_IDirect3DTexture9 texV,  unsigned char *src, int srcStride, int width, int src_height)
{
	HRESULT hr;

	D3DLOCKED_RECT d3dlr_Y, d3dlr_U, d3dlr_V;

	// lock texture for Y plane
	hr = texY->LockRect( 0, &d3dlr_Y, 0, D3DLOCK_DISCARD );
	if( FAILED(hr)) return ;
	// lock texture for U plane
	hr = texU->LockRect( 0, &d3dlr_U, 0, D3DLOCK_DISCARD );
	if( FAILED(hr)) return ;
	// lock texture for V plane
	hr = texV->LockRect( 0, &d3dlr_V, 0, D3DLOCK_DISCARD );
	if( FAILED(hr)) {
		texU->UnlockRect(0);
		return ;
	}

	uint8_t * src_yuy2 = src;
	uint8_t * dst_y = (uint8_t *)d3dlr_Y.pBits;
	uint8_t * dst_u = (uint8_t *)d3dlr_U.pBits;
	uint8_t * dst_v = (uint8_t *)d3dlr_V.pBits;
	int dstY_stride = d3dlr_Y.Pitch;
	int dstU_stride = d3dlr_U.Pitch;
	int dstV_stride = d3dlr_V.Pitch;

	for (int y = 0; y < src_height - 1; y += 2)
	{
		_YUY2_TO_UV_STRIDE_SSE2( src_yuy2, srcStride, dst_u, dst_v, width);
		_YUY2_To_Y_STRIDE_SSE2( src_yuy2, dst_y, width);
		_YUY2_To_Y_STRIDE_SSE2( src_yuy2 + srcStride, dst_y + dstY_stride, width);
		src_yuy2 += srcStride * 2;
		dst_y  += dstY_stride * 2;
		dst_u += dstU_stride;
		dst_v += dstV_stride;
	}
	
	if (src_height & 1) 
	{
		_YUY2_TO_UV_STRIDE_SSE2( src_yuy2, srcStride, dst_u, dst_v, width);
		_YUY2_To_Y_STRIDE_SSE2( src_yuy2, dst_y, width);
	}

	hr = texY->UnlockRect(0);
	hr = texU->UnlockRect(0);
	hr = texV->UnlockRect(0);

	return ;
}


HRESULT D3DPsVideoProcessor::_UpdateTexture(BYTE* pImage, int stride)
{

	HRESULT hr;
	LPBYTE lpY = NULL;
	LPBYTE lpU = NULL;
	LPBYTE lpV = NULL;
	LPBYTE lpUV = NULL;

	if ( pImage == NULL) return E_INVALIDARG;
	// texture not created correctly
	if (pd3dTexture0_ == NULL) return E_FAIL;

	switch ( pixelFormat_)
	{
	case D3DFMT_I420:
		if ( stride == 0) stride = nWidth_;
		lpY = pImage;
		lpU = pImage + stride * nHeight_;
		lpV = pImage + stride * nHeight_ * 5 / 4;
		hr = _LoadTexture(pd3dTexture0_, lpY, stride, nHeight_);
		if( FAILED(hr)) goto DONE;
		hr = _LoadTexture(pd3dTexture1_, lpU, stride/2, nHeight_/2);
		if( FAILED(hr)) goto DONE;
		hr = _LoadTexture(pd3dTexture2_, lpV, stride/2, nHeight_/2);
		if( FAILED(hr)) goto DONE;
		break;

	case D3DFMT_NV12:
		if ( stride == 0) stride = nWidth_;
		lpY = pImage;
		lpUV = pImage + stride * nHeight_;
		hr = _LoadTexture(pd3dTexture0_, lpY, stride, nHeight_);
		if( FAILED(hr)) goto DONE;
		hr = _Split_UV_TO_Texture(pd3dTexture1_, pd3dTexture2_, lpUV, stride, nWidth_/2, nHeight_/2);
		break;

	case D3DFMT_YUY2:
		if ( stride == 0) stride = nWidth_*2;
		if ( bNoYUY2Texture_ ){ // we don't have YUY2 Texture, so must load YUY2 to 3 textures
			_Load_YUY2_TO_3_Textures(pd3dTexture0_, pd3dTexture1_, pd3dTexture2_, pImage, stride, nWidth_, nHeight_);
		}else{
			hr = _LoadTexture(pd3dTexture0_, pImage, stride, nHeight_);
		}
		break;
	case D3DFMT_R8G8B8:
		if ( stride == 0) stride = nWidth_*3;
		hr = _LoadRGB24ToTexture(pd3dTexture0_, pImage, stride, nWidth_, nHeight_);
		break;
	}
 
DONE:
	return hr;
}

// pVertex must point to a buffer of 4 CustomVertex
void D3DPsVideoProcessor::_MakeQuadVertices( bool bFlipHorizontally, bool bFlipVertically,  CustomVertex * pVertex)
{
	// make texture coordinates
	float u1, u2, v1, v2;
	if ( !bFlipHorizontally){
		u1=0.0f, u2=1.0f;
	}else{
		u1=1.0f, u2=0.0f;
	}

	if ( !bFlipVertically){
		v1=0.0f, v2=1.0f;
	}else{
		v1=1.0f, v2=0.0f;
	}

	double r16to9 = 16.0/9.0;
	double dar =  (double) par_.Numerator * (double) nWidth_ / 
		( (double) par_.Denominator * (double) nHeight_ );

	double v_scale = 1.0;
	double h_scale = 1.0;
	if(dar < r16to9){
		h_scale = dar / r16to9;
	}else{
		v_scale = r16to9/dar;
	}

	int dw, dh;
	dw =  nBackbufferWidth_ * h_scale;
	dh = nBackbufferHeight_ * v_scale;

	// make vertex positions
	float x1, y1, x2, y2;
	// the swapchain is create based on the image size,
	// ( refer to _Make16To9Backbuffer(D3D9Renderer.cpp) )
	x1 = (float) ( ( nBackbufferWidth_ - dw )*1.0f/2.0);
	x2 = x1 + dw;
	y1 = (float) ( ( nBackbufferHeight_ - dh )*1.0f/2.0);
	y2 = y1 + dh;

	CustomVertex vertices[] =
	{
		{x1,	y1,   0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v1},
		{x2,    y1,   0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v1},
		{x2,    y2,   0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v2},
		{x1,	y2,   0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v2}
	};
	::memcpy(pVertex, vertices, sizeof(vertices));	
}

HRESULT D3DPsVideoProcessor::_UpdateVertices(bool flipHorizontally, bool flipVertically)
{
	HRESULT hr = S_OK;
	if(pd3dVertexBuffer_ ==NULL) return E_INVALIDARG;

	if ( bFlipHorizontally_ != flipHorizontally || bFlipVertically_!= flipVertically)
	{
		bFlipHorizontally_ = flipHorizontally;
		bFlipVertically_ = flipVertically;
		CustomVertex *  pVertices;
		hr = pd3dVertexBuffer_->Lock(0, 4 * sizeof(CustomVertex), (void**)&pVertices, 0); 
		if( FAILED(hr)) goto DONE;
		_MakeQuadVertices(bFlipHorizontally_, bFlipVertically_, pVertices);
		hr = pd3dVertexBuffer_->Unlock();  
	}

DONE:
	return hr;
}

// need to be called between a BeginScene/EndScene pair
HRESULT D3DPsVideoProcessor::DrawVideo (BYTE* pImage, int stride, bool flipHorizontally, bool flipVertically)
{
	HRESULT hr;

	// prepare vertices & textures
	hr = _UpdateVertices(flipHorizontally, flipVertically);
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "DrawVideo, _UpdateVertices hr = %x", hr);

	hr = _UpdateTexture( pImage, stride);
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "DrawVideo, _UpdateTexture hr = %x", hr);

	hr = pd3dDevice_->SetStreamSource( 0, pd3dVertexBuffer_, 0, sizeof(CustomVertex) );
	if( FAILED(hr)) goto DONE;
	hr = pd3dDevice_->SetFVF( D3DFVF_CUSTOMVERTEX );
	if( FAILED(hr)) goto DONE;

	hr = pd3dDevice_->SetTexture( 0, pd3dTexture0_ );
	if( FAILED(hr)) goto DONE;
	
	// additional textures and a pixel shader to I420 data
	if ( pixelFormat_ == D3DFMT_I420 ||
		pixelFormat_ == D3DFMT_NV12||
		(pixelFormat_ == D3DFMT_YUY2 && bNoYUY2Texture_) )
	{
		hr = pd3dDevice_->SetTexture( 1, pd3dTexture1_ );
		hr = pd3dDevice_->SetTexture( 2, pd3dTexture2_ );
		hr = pd3dDevice_->SetPixelShader( pPixelShader_ );
	}

	hr = pd3dDevice_->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
	CHECK_FAILED_GOTO_DONE(hr, "D3DPsVideoProcessor", "DrawVideo, pd3dDevice_->DrawPrimitive hr = %x", hr);

	hr = pd3dDevice_->SetTexture( 0, NULL );
	hr = pd3dDevice_->SetTexture( 1, NULL );
	hr = pd3dDevice_->SetTexture( 2, NULL );  	
	hr = pd3dDevice_->SetPixelShader( NULL );
DONE:

	return hr;
}

// This method must be called between a BeginScene/EndScene pair!
HRESULT D3DPsVideoProcessor::AlphaBlendingIcon(_com_ptr_IDirect3DTexture9  pIcon, RECT destRC)
{
	HRESULT hr;
	unsigned long w = destRC.right-destRC.left;
	unsigned long h = destRC.bottom-destRC.top;

	// Fill Vertex Buffer with new position
	CustomVertex *pVertex;
	hr = pd3dVertexBuffer4Icon_->Lock( 0, 4 * sizeof(CustomVertex), (void**)&pVertex, 0 );
	if (FAILED(hr)) { goto done;}

	float u1, u2, v1, v2;
	u1=0.0f, u2=1.0f;
	v1=0.0f, v2=1.0f;
	CustomVertex vertices[] =
	{
		{0.0f,	 0.0f,	 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v1},
		{w*1.0f, 0.0f,	 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v1},
		{w*1.0f, h*1.0f, 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u2, v2},
		{0.0f,	 h*1.0f, 0.0f,  1.0f, D3DCOLOR_ARGB(255, 255, 255, 255), u1, v2}
	};
	::memcpy(pVertex, vertices, sizeof(vertices));	

	hr = pd3dVertexBuffer4Icon_->Unlock();

	// setup texture
	hr = pd3dDevice_->SetTexture( 0, pIcon );
	if (FAILED(hr)) { goto done;}	
	// binds a vertex buffer to a device data stream.
	hr = pd3dDevice_->SetStreamSource( 0, pd3dVertexBuffer4Icon_, 0, sizeof(CustomVertex));
	if (FAILED(hr)) { goto done;}
	// sets the current vertex stream declaration.
	hr = pd3dDevice_->SetFVF( D3DFVF_CUSTOMVERTEX );
	if (FAILED(hr)) { goto done;}
	// draw the primitive
	hr = pd3dDevice_->DrawPrimitive( D3DPT_TRIANGLEFAN, 0, 2 );
	if (FAILED(hr)) { goto done;}

	hr = pd3dDevice_->SetTexture( 0, NULL);

done:

	return hr;
}

}