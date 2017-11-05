//////////////////////////////////////////////////////////////////////////////
// This is an auto-reset event. Only one waiting thread will be released 
//  on the SetEvent call.
#pragma once
#include "stdafx.h"
namespace WinRTCSDK{

class NoCopy {
public:
	NoCopy(){}
private:
	NoCopy(const NoCopy& r);
	const NoCopy& operator = (const NoCopy& r);
};

class  Win32Event : NoCopy {
public:
	// default auto-reset
	Win32Event(BOOL manualRest=FALSE)
	{ 
		manualRest_ = manualRest;
		hEvent_ = ::CreateEventEx(NULL, NULL, 
			manualRest?CREATE_EVENT_MANUAL_RESET:0
			, EVENT_MODIFY_STATE | SYNCHRONIZE); 
	}
	~Win32Event() { ::CloseHandle(hEvent_); }

	void Wait () { ::WaitForSingleObject(hEvent_, INFINITE) ; }
	bool Wait (int milliSec) {
		DWORD dwRet = ::WaitForSingleObject(hEvent_, milliSec) ; 
		return ( WAIT_OBJECT_0 == dwRet);
	}

	void SetEvent () { ::SetEvent(hEvent_) ; }
	void Reset(){::ResetEvent(hEvent_);}
	bool IsAutoRest(){ return manualRest_ == FALSE;}

private:
	BOOL   manualRest_;
	HANDLE hEvent_;
};

class Win32Semaphore
{
public:
	Win32Semaphore(int count)
	{
		hSemphore_ = ::CreateSemaphore(NULL, count, LONG_MAX, 0);
	}

	~Win32Semaphore(void)
	{
		::CloseHandle(hSemphore_);
	}

	bool Post(void)
	{
		return (::ReleaseSemaphore(hSemphore_, 1, NULL) == TRUE);
	}

	bool Wait(void)
	{
		return (::WaitForSingleObject(hSemphore_, INFINITE) == WAIT_OBJECT_0) ;
	}
private:
	HANDLE  hSemphore_;
private:
	Win32Semaphore(const Win32Semaphore&);
	void operator=(const Win32Semaphore&);
};

struct WorkingBuffer : public NoCopy
{
	WorkingBuffer():size_(0), buffer_(NULL){}
	~WorkingBuffer(){
		if (buffer_!=NULL) free (buffer_);
	}
	// just ensure a working buffer, the call doesn't need to free this buffer.
	// please don't change frame size too frequently, otherwise the performance could be degraded.
	BYTE* EnsureBuffer(int size)
	{
		if ( size<=0 ) return NULL;
		if ( size > 1920*1088*4*4 ) return NULL;// 4k buffer at most

		// need to resize the buffer, this is a costly operation.
		if ( (buffer_ == NULL) || (size > size_) ){
			size_ = size;
			if ( buffer_ != NULL) ::free(buffer_);
			buffer_ = NULL;
			buffer_ = (BYTE*)::malloc(size_);
		}
		
		return buffer_;

	}
		
	BYTE* GetBuffer() const{
		return buffer_;
	}

private:
	int size_;
	BYTE* buffer_;
};

}