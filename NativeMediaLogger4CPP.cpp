#include "Stdafx.h"
#include "NativeMediaLogger4CPP.h"
#include "time.h"
#include "conio.h"
namespace WinRTCSDK {

// default output to console
static void __log_thunck_default (int level, std::string tag, char* log)
{
	time_t now;	
	::time(&now);
	tm * ptm =::localtime(&now);
	char tmstr[26];
	strftime(tmstr, 26, "%m-%d %H:%M:%S", ptm);
	_cprintf("%s [%s][LVL%02d] - %s\r\n", tmstr, tag.c_str(), level, log);
}

static void __log_thunckW_default (int level, std::wstring tag, wchar_t* log)
{
	time_t now;	
	::time(&now);
	tm * ptm =::localtime(&now);
	char tmstr[26];
	strftime(tmstr, 26, "%m-%d %H:%M:%S", ptm);
	_cprintf("%s [%S][LVL%02d] - %S\r\n", tmstr, tag.c_str(), level, log);
}

NativeMediaLogger4CPP::__LOG_THUNK NativeMediaLogger4CPP::__log_thunck = __log_thunck_default;
NativeMediaLogger4CPP::__LOG_THUNK_W NativeMediaLogger4CPP::__log_thunckW = __log_thunckW_default;
}
