// hwmft.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <initguid.h>
#include <wmcodecdsp.h>
#include <Codecapi.h>

#include <comip.h>

#include "Looper.h"
#include "ComPtrDefs.h"

using namespace WinRTCSDK;

#pragma comment (lib, "comsuppw.lib ")
#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mf.lib")
#pragma comment (lib, "Mfuuid.lib")


typedef _com_ptr_t < _com_IIID<IMFTransform, &__uuidof(IMFTransform) > > _com_ptr_IMFTransform;


void SetupMFT(_com_ptr_IMFTransform mft)
{
	DWORD maxIn, minIn, maxOut, minOut, inCnt, outCnt;
	HRESULT hr;
	hr = mft->GetStreamLimits(&minIn, &maxIn, &minOut, &maxOut);
	hr = mft->GetStreamCount(&inCnt, &outCnt);
	DWORD inID, outID;
	hr = mft->GetStreamIDs(1, &inID, 1, &outID);

	if(FAILED(hr))
		inID = 0;
	// setup In format
	_com_ptr_IMFAttributes attr; 
	hr = mft->GetAttributes(&attr);

	if (attr != NULL){
		UINT32 useDXVA;
		hr = attr->GetUINT32(CODECAPI_AVDecVideoAcceleration_H264, &useDXVA);
		UINT32 d3dAware;
		hr = attr->GetUINT32(MF_SA_D3D_AWARE , &d3dAware);
		if( d3dAware ){
//			hr = attr->SetUINT32(CODECAPI_AVDecVideoAcceleration_H264, 1);
		}
	}

}

HRESULT CreateMSH264Decoder ( _com_ptr_IMFTransform &mft)
{
	HRESULT hr;
	hr = ::CoCreateInstance(__uuidof(CMSH264DecoderMFT), 
		NULL,CLSCTX_INPROC_SERVER, IID_IMFTransform, (VOID**)&mft);

	return hr;
}
void EnumCodecs()
{
	HRESULT hr;

	MFT_REGISTER_TYPE_INFO regOutType;
	regOutType.guidMajorType = MFMediaType_Video;
	regOutType.guidSubtype = MFVideoFormat_H264;

	IMFActivate **ppActivate;
	UINT32 num;
	// enum mft
	hr = MFTEnumEx(MFT_CATEGORY_VIDEO_ENCODER, 
		  MFT_ENUM_FLAG_ALL,
		  NULL, 
		  &regOutType,
		  &ppActivate,
		  &num);
	WCHAR buff [1024];
	UINT32 len;
	UINT32 d3daware;
	UINT32 flags;
	UINT32 async;

	if (SUCCEEDED(hr)){
		IMFTransform * mft;
		for (int i = 0; i<num; i++){
			hr = ppActivate[i]->GetString(MFT_FRIENDLY_NAME_Attribute, buff, 1024, &len);
			hr = ppActivate[i]->GetUINT32(MF_TRANSFORM_FLAGS_Attribute, &flags);
			hr = ppActivate[i]->ActivateObject(__uuidof(IMFTransform), (void**)&mft);
			ppActivate[i]->Release();
			if (SUCCEEDED(hr))
			{
				IMFAttributes * attr;
				hr = mft->GetAttributes(&attr);
				if(SUCCEEDED(hr))
				{

					hr = attr->GetUINT32(MF_SA_D3D_AWARE, &d3daware);
					hr = attr->GetUINT32(MF_TRANSFORM_ASYNC, &async);
					
					attr->Release();
				}

				mft->Release();
			}
		}

		// free the array
		::CoTaskMemFree(ppActivate);
		hr = S_OK;
	}
}

int _tmain(int argc, _TCHAR* argv[])
{
	::MFStartup(MF_VERSION);
	{
	_com_ptr_IMFTransform mft;

	HRESULT hr = CreateMSH264Decoder(mft);
	if(SUCCEEDED(hr)){
		SetupMFT (mft);
	}
	}
	::MFShutdown();
	return 0;
}
