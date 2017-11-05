#pragma once
#include "stdafx.h"
namespace WinRTCSDK{
class Mutex{
public:
	Mutex() {
		::InitializeCriticalSection(&mutex);
	}

	~Mutex(){
		::DeleteCriticalSection(&mutex);
	}

	void lock(){
		::EnterCriticalSection(&mutex); // This function has no return value
	}

	bool trylock(){
		return (TryEnterCriticalSection(&mutex) == TRUE);
	}

	void unlock(){
		::LeaveCriticalSection(&mutex);
	}
	CRITICAL_SECTION mutex;
private: // mutex using CRITICAL_SECTION, copying is forbidden
	Mutex& operator = (const Mutex& R);
	Mutex (const Mutex& R);
};

class AutoLock
{
public:
	AutoLock(Mutex &lock) : _lock(lock){
		_lock.lock();
	}
	virtual ~AutoLock(){
		_lock.unlock();
	}

private:
	Mutex &_lock;
};

template <typename T> 
class AtomicVar {
public:
	AtomicVar <T> (const T &t){
		_val = t;
	}

	 operator T& (){
		 return _val;
	 }

	 T& operator = (T& rval){
		 // we use spin lock the set the value
        while (::InterlockedCompareExchange(&_opFlag, 1, 0));
		 _val = rval;
        ::InterlockedCompareExchange((long *)&_opFlag, 0, 1);
		 return _val;
	 }

private:
    volatile long _opFlag;
	T     _val;
};
} // namespace Nemo