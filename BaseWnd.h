#pragma once
#include <map>
#include "AutoLock.h"

namespace WinRTCSDK{

class BaseWnd
{

private:
	HWND hwnd_;
	BOOL bMainWnd_;
public:
	BaseWnd(BOOL bMainWnd=FALSE) : hwnd_(NULL), bMainWnd_(bMainWnd)
	{
	}
    operator  HWND (){
        return hwnd_;
    }
	~BaseWnd(void)
	{
		if ( hwnd_ && IsWindow(hwnd_) ) {
			::DestroyWindow (hwnd_);
		}
	}

	BOOL Create ( DWORD dwExStyle, LPCWSTR lpWindowName, DWORD dwStyle, int X,	int Y,	
		int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance );
	
	virtual LRESULT WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	// static functions to maitain a global map;
	static	LRESULT CALLBACK WndProcThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static  std::map<HWND, BaseWnd*> wndmap_;
	static  Mutex wndmaplock_;
	static  ATOM  wndcls_;
	static  TCHAR wndclsname_ [] ;			// the main window class name

};

}// namespace WinRTCSDK