#pragma once
#include <string>
#include <stdint.h>

// global functions
namespace WinRTCSDK{

// hi resolution timestamp
uint64_t GetTimeStempMS();


//wchar to string
std::string WC2string(const wchar_t* buf);
std::wstring _get_lower_case(std::wstring instr);

}