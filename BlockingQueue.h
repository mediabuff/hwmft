/////////////////////////////////////////////////////////////
// A common task queue to perform thread-switching 
#pragma once
#include "stdafx.h"
#include <queue>

namespace WinRTCSDK{

template < typename T> 
class BlockingQueue {
public:
	BlockingQueue(){
		hSemaphore_ =  ::CreateSemaphore(NULL, 0, 1024, NULL);
		::InitializeCriticalSection(&cs_);
	};
	~BlockingQueue(){
		::CloseHandle(hSemaphore_);
		::DeleteCriticalSection(&cs_);
	}
	void put (T m ){
		::EnterCriticalSection(&cs_);
		queue_.push(m);
		::LeaveCriticalSection(&cs_);
		::ReleaseSemaphore(hSemaphore_, 1, NULL);
	}
	// This is an blocking operation
	bool get (T& m){
		bool ret;
		::WaitForSingleObject(hSemaphore_, INFINITE);
		::EnterCriticalSection(&cs_);
		ret = !queue_.empty();
		if (ret){
			m = queue_.front();
			queue_.pop();
		}
		::LeaveCriticalSection(&cs_);
		return ret;
	}
	bool peek(T& m){
		::EnterCriticalSection(&cs_);
		ret = !queue_.empty();
		if (ret){
			m = queue_.front();
		}
		::LeaveCriticalSection(&cs_);
		return ret;
	}
private:
	std::queue<T> queue_;
	HANDLE hSemaphore_;
	CRITICAL_SECTION  cs_;
};
} // namespace WinRTCSDK