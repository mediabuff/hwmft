////////////////////////////////////////////////////////
// define native types only here
//     These types are used for Window platrorm only,
//     these types are NOT replications from RTCSDK,
//
//   because these native types has their managed counterparts, 
//   so we define them in this separate header!
//
//   If a native types only used in native code (has not managed counterpart), 
//   then it not need to be defined here!
//
#pragma once

#include <stdint.h>
#include <string>

namespace WinRTCSDK {


enum VideoCaptureOrdinal 
{
	None, First, Second, Third
};

enum class VideoStreamType
{
	People,
	Content
};

// values must be same as values of MediaDevTypeManaged
enum MediaDevTypeN
{
	eAudioCapture,
	eAudioRender,
	eVideoCapture,
	eVideoRender,
	eSerialPort,
	eUsbVolume,
	eOtherDevice
};

////////////////////////////////////////////////////////
// Native types only used in c++ code in this module
struct MediaDevInfo
{
	std::wstring  symbolicLink; // dev symbolic link
	std::wstring  devFriendlyName;
    std::wstring  busReportedDeviceDesc;
	std::wstring  location; // the device location on the bus.	
	std::wstring  deviceId; // 'symbolicLink' is used for endpoint ID, this is the device(adapter) instance ID	
	unsigned long xylinkFPGAVersion; // only available for xylink VideoCapCard
	bool          videoInputLocked;
	MediaDevTypeN devType;
	struct {
		std::wstring devInterfaceName; // the audio adapter name, used to check homologous device	
		std::wstring containerId; // dev enclosure ID, for audio devices, used to check homologous device
		uint32_t     formFactor; // please refer to EndpointFormFactor in Mmdeviceapi.h
		bool         bConsoleRole;
		bool         bMultimediaRole;
		bool         bCommunicationsRole;
	} audioDevInfo;

	MediaDevInfo(){
		videoInputLocked = false;
		xylinkFPGAVersion = 0;
		audioDevInfo.formFactor = 0;
		audioDevInfo.bConsoleRole = false;
		audioDevInfo.bMultimediaRole = false;
		audioDevInfo.bCommunicationsRole = false;
	}
};

struct VideoCaptureCapbaility
{
	VideoCaptureCapbaility(int w, int h, float fr, int isInterlaced):
		width(w),height(h),fameRate(fr),interlaced(isInterlaced){}
	VideoCaptureCapbaility():width(0),height(0),fameRate(0),interlaced(false){}

	int   width;
	int   height;
	float fameRate;
	bool  interlaced;
};

struct DisplayDevInfo
{
	DisplayDevInfo ():
		hdmiPort(0) {}
	std::wstring viewGdiDeviceName; // such as \\.\DISPLAY1 ..
	int          hdmiPort; // 1 -based
	int          edidManufactureId;
	int          edidProductCodeId;
	std::wstring monitorFriendlyDeviceName; // such as DELL U2414H 
	std::wstring monitorDevicePath;
};

} // namespace WinRTCSDK