/////////////////////////////////////////////////////////////////////////
// A simple looper to execute tasks synchronously or asynchronously
//  This is typically used to switch thread-context.
#pragma once
#include "stdafx.h"
#include <queue>
#include <functional>
#include <memory>

#include "AutoLock.h"
#include "Win32Primitives.h"
#include "BlockingQueue.h"

namespace WinRTCSDK{

static void SetThreadName(DWORD dwThreadID, const char *threadName);

typedef std::function<void ()> LooperTask;

class Looper {
	struct LooperMsg{
		LooperMsg() { stopLooper = false ;}
		bool stopLooper;
		LooperTask t;
		std::shared_ptr<Win32Event> notif;
	};

public:

	Looper (std::string name, bool needMediaFundation = false):
		hThread_(NULL),lock_(),
		dwThreadID_(0),
		name_(name),
		needMediaFundation_(needMediaFundation), // need Media Foundation?
		isRunning_(false)
	{
		// kick off the task handler
		startUp();
	}

	~Looper () {
		stopAndJoin(); 
	}

	bool startUp() {
		AutoLock l (lock_);
		if ( hThread_ == NULL) hThread_ = ::CreateThread(NULL, 0, ThreadProcEntry, this, 0, &dwThreadID_);
		if ( hThread_!=NULL) isRunning_ = true;
		return true;
	}
	// This method try to stop the looper by posting a stop message to the task queue of the looper.
	// if the thread is running a lengthy task, 
	// the user of this looper should provide appropriate means to cancel the task! 
	bool stopAndJoin() 
	{ 
		HANDLE hThread = NULL;

		// setting the 'isRunning_' flag and posting the 'stopMsg' message
		// are combined as an atomic operation
		{
			AutoLock l (lock_);
			// to avoid stopAndJoin being called multiple times
			if (isRunning_ == false) return true; 

			isRunning_ = false;
			hThread = hThread_; // copy to thread stack

			if ( hThread ) {
				LooperMsg msg;
				msg.stopLooper = true;
				msgQueue_.put(msg);
			}
		}

		if ( hThread ) {
			::WaitForSingleObject(hThread, INFINITE);
			::CloseHandle(hThread);
		}
		// reset data members
		{
			AutoLock l (lock_);
			hThread_ = NULL;
			dwThreadID_ = 0;
		}
		return true;
	}

	bool runAsyncTask (LooperTask &t)
	{
		LooperMsg msg;
		msg.t = t;

		// checking isRunning_ and putting an item are combined as an atomic
		{
			AutoLock l (lock_);
			if (isRunning_ == false) return false;
			msgQueue_.put(msg);
		}
		return true;
	}

	// Be carefull when using this method to run a task.
	// The calling thread can be blocked for long time, when the looper thread is very busy!
	bool runSyncTask (LooperTask &t)
	{
		// The calling thread is the looper itself
		if ( ::GetCurrentThreadId() == dwThreadID_) {
			t(); // just run the task
			return true;
		} 

		LooperMsg msg;
		msg.t = t;
		msg.notif = std::make_shared<Win32Event>();
		// checking isRunning_ and putting an item are combined as an atomic
		{
			AutoLock l (lock_);
			if (isRunning_ == false) return false;
			msgQueue_.put(msg);
		}

		// await the task to be done.
		msg.notif->Wait();
		return true;
	}

	DWORD threadID(){
		return dwThreadID_;
	}

private:
	static DWORD WINAPI ThreadProcEntry(LPVOID pContext){ // Win32 Thread Entry
		Looper * pThis = (Looper*) pContext;
		return pThis->loop();
	}

	DWORD loop()
	{
		// for easy debugging
		SetThreadName(::GetCurrentThreadId(), name_.c_str());
		// initialize COM
		HRESULT hr = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);

		// startup media foundation
		if( needMediaFundation_ ) { 
			::MFStartup(MF_VERSION);
		}

		LooperMsg msg;
		while (true) {
			if ( msgQueue_.get(msg))
			{
				if (msg.stopLooper) break;
				msg.t(); // run the task;
				if (msg.notif != nullptr) msg.notif->SetEvent();
			}
		}

		// tear down media foundation
		if ( needMediaFundation_){
			::MFShutdown();
		}

		::CoUninitialize();
		return 0;
	}

private:
	Mutex                   lock_; // to synchronize looper startup/Shutdown
	HANDLE                  hThread_;
	DWORD                   dwThreadID_;
	BlockingQueue<LooperMsg> msgQueue_;
	std::string             name_;
	bool                    needMediaFundation_;
	bool                    isRunning_;
private: // no copy
	Looper( const Looper& r);
	Looper& operator =(const Looper&r);
};

//////////////////////////////////////////////////////////
// we have undocumented Win32 APIs to set thread name.
const DWORD MS_VC_EXCEPTION = 0x406D1388;
#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType; // Must be 0x1000.
    LPCSTR szName; // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags; // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
static void SetThreadName(DWORD dwThreadID, const char *threadName)
{
    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = threadName;
    info.dwThreadID = dwThreadID;
    info.dwFlags = 0;
#pragma warning(push)
#pragma warning(disable: 6320 6322)

    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR *)&info);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
    }

#pragma warning(pop)
}

} // namespace WinRTCSDK