#pragma once
///////////////////////////////////////////////////////////////////////
//Provides access to following interfaces of a WDM Video Capture.
//    IAMCameraControl, IAMVideoProcAmp , IAMVideoControl 


#include "ComPtrDefs.h"

namespace WinRTCSDK{

class CameraControl 
{
public:
	CameraControl(){};
	~CameraControl(){};
	// Query control interface by dshow capture filter
	HRESULT Create (_com_ptr_IBaseFilter pCapFilter)
	{
		HRESULT hr;
		// Get IAMVideoProcAmp
		hr = pCapFilter->QueryInterface(IID_IAMVideoProcAmp, (void**)&pAMVideoProcAmp_);
		if ( FAILED (hr) ) goto done;

		// Get IAMCameraControl
		hr = pCapFilter->QueryInterface(IID_IAMCameraControl, (void**)&pAMCameraControl_);
		if ( FAILED (hr) ) goto done;

done:
		return hr;	
	}

	// We are still able to query following DShow interfaces from MFMediaSource
	HRESULT Create( _com_ptr_IMFMediaSource pMediaSource )
	{
		HRESULT hr;
		// Get IAMVideoProcAmp
		hr = pMediaSource->QueryInterface(IID_IAMVideoProcAmp, (void**)&pAMVideoProcAmp_);
		if ( FAILED (hr) ) goto done;

		// Get IAMCameraControl
		hr = pMediaSource->QueryInterface(IID_IAMCameraControl, (void**)&pAMCameraControl_);
		if ( FAILED (hr) ) goto done;

done:
		return hr;	
	}

	// just release the COM interfaces
	void Destroy () 
	{
		pAMCameraControl_=NULL;
		pAMVideoProcAmp_ =NULL;
	}

	///////////////////////////////////////////////////////////
	// camera control
	bool CameraControl_GetRange(long camCtrlProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto)
	{
		long Flags=0;
		if (pAMCameraControl_ == NULL) return false;
		HRESULT hr = pAMCameraControl_->GetRange(camCtrlProperty, &minVal, &maxVal, &Step,&defaultVal, &Flags);
		supportAuto = (Flags & CameraControl_Flags_Auto);
		return (hr==S_OK);
	}

	bool CameraControl_GetValue(long camCtrlProperty, long &val, bool &isAuto)
	{
		long Flags=0;
		if (pAMCameraControl_ == NULL) return false;
		HRESULT hr = pAMCameraControl_->Get(camCtrlProperty, &val, &Flags);
		isAuto =  (Flags & CameraControl_Flags_Auto);
		return (hr==S_OK);
	}

	bool CameraControl_SetValue(long camCtrlProperty, long val, bool isAuto)
	{
		long Flags = isAuto?CameraControl_Flags_Auto:0;
		if (pAMCameraControl_ == NULL) return false;
		HRESULT hr = pAMCameraControl_->Set(camCtrlProperty, val, Flags);
		return (hr==S_OK);
	}

	///////////////////////////////////////////////////
	// VideoProcAmp
	bool VideoProcAmp_GetRange(long procAmpProperty, long &minVal, long &maxVal, long &Step, long &defaultVal, bool &supportAuto)
	{
		long Flags=0;
		if (pAMVideoProcAmp_ == NULL) return false;
		HRESULT hr = pAMVideoProcAmp_->GetRange(procAmpProperty, &minVal, &maxVal, &Step,&defaultVal, &Flags);
		supportAuto = (Flags & VideoProcAmp_Flags_Auto);
		return (hr==S_OK);
	}

	bool VideoProcAmp_GetValue(long procAmpProperty, long &val, bool &isAuto)
	{
		long Flags=0;
		if (pAMVideoProcAmp_ == NULL) return false;
		HRESULT hr = pAMVideoProcAmp_->Get(procAmpProperty, &val, &Flags);
		isAuto =  (Flags & VideoProcAmp_Flags_Auto);
		return (hr==S_OK);
	}

	bool VideoProcAmp_SetValue(long procAmpProperty, long val, bool isAuto)
	{
		long Flags = isAuto?VideoProcAmp_Flags_Auto:0;
		if (pAMVideoProcAmp_ == NULL) return false;
		HRESULT hr = pAMVideoProcAmp_->Set(procAmpProperty, val, Flags);
		return (hr==S_OK);
	}

private:
	_com_ptr_IAMCameraControl pAMCameraControl_;
	_com_ptr_IAMVideoProcAmp  pAMVideoProcAmp_;
};

} // namesapce WinRTCSDK