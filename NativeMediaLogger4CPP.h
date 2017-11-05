#pragma once
#include <string>
#include <stdarg.h>

namespace WinRTCSDK
{
#pragma warning (disable:4793)

namespace Native{
enum LogLevelN{
	Error=0, Warning, Info, Debug
};
}

class NativeMediaLogger4CPP
{
	typedef void (*__LOG_THUNK) (int level, std::string tag, char* log);
	typedef void (*__LOG_THUNK_W) (int level, std::wstring tag, wchar_t* log);

public:
	static void setLogDelegate (__LOG_THUNK logger, __LOG_THUNK_W loggerW)
	{
		__log_thunck = logger;
		__log_thunckW = loggerW;
	}
	
	// level : Error=0, Warning, Info, Debug
	static void log (int level, std::string tag, const char * fmt, ...)
	{
		if ( !__log_thunck) return;
		va_list args;
		int     len;
		char    buffer[1024];

		// retrieve the variable arguments
		va_start( args, fmt );

		len = _vscprintf( fmt, args ) + 1;  // _vscprintf doesn't countterminating '\0'	
		if (len > 1024 ){
			__log_thunck(level, tag, "===exceeded logger buffer size(1024)====\n" );
			return ;
		}

		vsprintf_s( buffer, fmt, args ); // C4996

		__log_thunck(level, tag, buffer );

		
	}
	// level : Error=0, Warning, Info, Debug
	static void logW (int level, std::wstring tag, const wchar_t * fmt, ...)
	{
		if ( !__log_thunckW) return;
		va_list args;
		int     len;
		wchar_t buffer[1024];

		// retrieve the variable arguments
		va_start( args, fmt );

		len = _vscwprintf( fmt, args ) + 1;  // _vscprintf doesn't countterminating '\0'	
		if (len > 1024 ){
			__log_thunckW(level, tag, L"===exceeded logger buffer size(1024)====\n" );
			return ;
		}

		vswprintf_s( buffer, fmt, args ); // C4996

		__log_thunckW(level, tag, buffer );

	}

private:
	static __LOG_THUNK __log_thunck;
	static __LOG_THUNK_W __log_thunckW;
};
}
