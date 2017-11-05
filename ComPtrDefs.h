#pragma once
#include <comip.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

namespace WinRTCSDK{
 	
typedef _com_ptr_t < _com_IIID<IMFMediaEvent, &__uuidof(IMFMediaEvent) > > _com_ptr_IMFMediaEvent;
typedef _com_ptr_t < _com_IIID<IMFMediaEventGenerator, &__uuidof(IMFMediaEventGenerator) > > _com_ptr_IMFMediaEventGenerator;
typedef _com_ptr_t < _com_IIID<IMFSample, &__uuidof(IMFSample) > > _com_ptr_IMFSample;
typedef _com_ptr_t < _com_IIID<IMFMediaType, &__uuidof(IMFMediaType) > > _com_ptr_IMFMediaType;
typedef _com_ptr_t < _com_IIID<IMFMediaBuffer, &__uuidof(IMFMediaBuffer) > > _com_ptr_IMFMediaBuffer;
typedef _com_ptr_t < _com_IIID<IMFMediaSource, &__uuidof(IMFMediaSource) > > _com_ptr_IMFMediaSource;
typedef _com_ptr_t < _com_IIID<IMFAttributes, &__uuidof(IMFAttributes) > > _com_ptr_IMFAttributes;
typedef _com_ptr_t < _com_IIID<IMFSourceReader, &__uuidof(IMFSourceReader) > > _com_ptr_IMFSourceReader;
typedef _com_ptr_t < _com_IIID<IMFPresentationDescriptor, &__uuidof(IMFPresentationDescriptor) > > _com_ptr_IMFPresentationDescriptor;
typedef _com_ptr_t < _com_IIID<IMFMediaBuffer, &__uuidof(IMFMediaBuffer) > > _com_ptr_IMFMediaBuffer;
typedef _com_ptr_t < _com_IIID<IMF2DBuffer, &__uuidof(IMF2DBuffer) > > _com_ptr_IMF2DBuffer;

// D3D9Ex

typedef _com_ptr_t < _com_IIID<IDirect3D9Ex, &__uuidof(IDirect3D9Ex) > > _com_ptr_IDirect3D9Ex;
typedef _com_ptr_t < _com_IIID<IDirect3DVertexBuffer9, &__uuidof(IDirect3DVertexBuffer9) > > _com_ptr_IDirect3DVertexBuffer9;
typedef _com_ptr_t < _com_IIID<IDirect3DPixelShader9, &__uuidof(IDirect3DPixelShader9) > > _com_ptr_IDirect3DPixelShader9;
typedef _com_ptr_t < _com_IIID<IDirect3DTexture9, &__uuidof(IDirect3DTexture9) > > _com_ptr_IDirect3DTexture9;
typedef _com_ptr_t < _com_IIID<IDirect3DDevice9, &__uuidof(IDirect3DDevice9) > > _com_ptr_IDirect3DDevice9;
typedef _com_ptr_t < _com_IIID<IDirect3DDevice9Ex, &__uuidof(IDirect3DDevice9Ex) > > _com_ptr_IDirect3DDevice9Ex;
typedef _com_ptr_t < _com_IIID<IDirect3DSurface9, &__uuidof(IDirect3DSurface9) > > _com_ptr_IDirect3DSurface9;
typedef _com_ptr_t < _com_IIID<IDirect3DSwapChain9, &__uuidof(IDirect3DSwapChain9) > > _com_ptr_IDirect3DSwapChain9;
typedef _com_ptr_t < _com_IIID<IDirect3DDeviceManager9, &__uuidof(IDirect3DDeviceManager9) > > _com_ptr_IDirect3DDeviceManager9;
typedef _com_ptr_t < _com_IIID<IDirectXVideoDecoderService, &__uuidof(IDirectXVideoDecoderService) > > _com_ptr_IDirectXVideoDecoderService;
typedef _com_ptr_t < _com_IIID<IDirectXVideoProcessorService, &__uuidof(IDirectXVideoProcessorService) > > _com_ptr_IDirectXVideoProcessorService;
typedef _com_ptr_t < _com_IIID<IDirectXVideoProcessor, &__uuidof(IDirectXVideoProcessor) > > _com_ptr_IDirectXVideoProcessor;

// DShow

typedef _com_ptr_t < _com_IIID<IAMCrossbar, &__uuidof(IAMCrossbar) > > _com_ptr_IAMCrossbar;
typedef _com_ptr_t < _com_IIID<ICaptureGraphBuilder2, &__uuidof(ICaptureGraphBuilder2) > > _com_ptr_ICaptureGraphBuilder2;
typedef _com_ptr_t < _com_IIID<ICaptureGraphBuilder, &__uuidof(ICaptureGraphBuilder) > > _com_ptr_ICaptureGraphBuilder;
typedef _com_ptr_t < _com_IIID<IGraphBuilder, &__uuidof(IGraphBuilder) > > _com_ptr_IGraphBuilder;
typedef _com_ptr_t < _com_IIID<IMediaFilter, &__uuidof(IMediaFilter) > > _com_ptr_IMediaFilter;
typedef _com_ptr_t < _com_IIID<IAMStreamConfig, &__uuidof(IAMStreamConfig) > > _com_ptr_IAMStreamConfig;
typedef _com_ptr_t < _com_IIID<IMediaControl, &__uuidof(IMediaControl) > > _com_ptr_IMediaControl;

typedef _com_ptr_t < _com_IIID<ICreateDevEnum, &__uuidof(ICreateDevEnum) > > _com_ptr_ICreateDevEnum;
typedef _com_ptr_t < _com_IIID<IEnumMoniker, &__uuidof(IEnumMoniker) > > _com_ptr_IEnumMoniker;
typedef _com_ptr_t < _com_IIID<IMoniker, &__uuidof(IMoniker) > > _com_ptr_IMoniker;
typedef _com_ptr_t < _com_IIID<IPropertyBag, &__uuidof(IPropertyBag) > > _com_ptr_IPropertyBag;
typedef _com_ptr_t < _com_IIID<IBaseFilter, &__uuidof(IBaseFilter) > > _com_ptr_IBaseFilter;
typedef _com_ptr_t < _com_IIID<IAMCameraControl, &__uuidof(IAMCameraControl) > > _com_ptr_IAMCameraControl;
typedef _com_ptr_t < _com_IIID<IAMVideoProcAmp, &__uuidof(IAMVideoProcAmp) > > _com_ptr_IAMVideoProcAmp;
typedef _com_ptr_t < _com_IIID<IAMVideoControl, &__uuidof(IAMVideoControl) > > _com_ptr_IAMVideoControl;
typedef _com_ptr_t < _com_IIID<IPin, &__uuidof(IPin) > > _com_ptr_IPin;
typedef _com_ptr_t < _com_IIID<IEnumPins, &__uuidof(IEnumPins) > > _com_ptr_IEnumPins;
typedef _com_ptr_t < _com_IIID<IKsPropertySet, &__uuidof(IKsPropertySet) > > _com_ptr_IKsPropertySet;
typedef _com_ptr_t < _com_IIID<IKsControl, &__uuidof(IKsControl) > > _com_ptr_IKsControl;
typedef _com_ptr_t < _com_IIID<IKsTopologyInfo, &__uuidof(IKsTopologyInfo) > > _com_ptr_IKsTopologyInfo;
  
}