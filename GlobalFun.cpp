// audio/video buffer related global functions
//  and other utility functions
#include "Stdafx.h"
#include <map>
#include <memory>


#include "GlobalFun.h"

// global funcions to access data source managers, used by NativeMedia only
namespace WinRTCSDK{

// we use same algo to calculate the time stamp for video as the WASAPI api
// please refer to IAudioCaptureClient::GetBuffer method
uint64_t GetTimeStempMS()
{
	LARGE_INTEGER t;
	LARGE_INTEGER f;
	QueryPerformanceCounter(&t);
	QueryPerformanceFrequency (&f);
	double fdou = f.QuadPart / (double)1000;
	return (uint64_t)(t.QuadPart /fdou);
}

std::string WC2string(const wchar_t* buf)
{
	int len = WideCharToMultiByte(CP_UTF8, 0, buf, -1, NULL, 0, NULL, NULL);
    if (len == 0)
	{
		return "";
	}
 
    std::vector<char> utf8(len);
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, &utf8[0], len, NULL, NULL);
 
	return std::string(&utf8[0]);
}

std::wstring _get_lower_case(std::wstring instr)
{
	wchar_t * ws1 = new wchar_t[instr.size()+1];
	wcscpy_s(ws1, instr.size()+1, instr.c_str());
	_wcslwr_s(ws1,instr.size()+1);
	std::wstring outstr = ws1;
	delete [] ws1;
	return outstr;
}


}//namespace WinRTCSDK
