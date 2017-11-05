#include "stdafx.h"
#include "BaseWnd.h"

namespace WinRTCSDK{

std::map<HWND, BaseWnd*> BaseWnd::wndmap_;
Mutex BaseWnd::wndmaplock_;
ATOM  BaseWnd::wndcls_;
TCHAR BaseWnd::wndclsname_ []=L"_vulture_base_wndcls" ;			// the main window class name

BOOL BaseWnd::Create( DWORD dwExStyle, LPCWSTR lpWindowName, DWORD dwStyle, int X,	int Y,	
					 int nWidth, int nHeight, HWND hWndParent, HMENU hMenu, HINSTANCE hInstance )
{
    AutoLock autolock (wndmaplock_);
    if ( wndcls_ == 0) 
    {
        WNDCLASSEX wcex;
        wcex.cbSize = sizeof(WNDCLASSEX);
        wcex.style			= CS_HREDRAW | CS_VREDRAW;
        wcex.lpfnWndProc	= WndProcThunk;
        wcex.cbClsExtra		= 0;
        wcex.cbWndExtra		= 0;
        wcex.hInstance		= hInstance;
        wcex.hIcon			= LoadIcon(NULL, MAKEINTRESOURCE(IDI_APPLICATION));
        wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
		wcex.hbrBackground	= (HBRUSH)::GetStockObject(BLACK_BRUSH);
        wcex.lpszMenuName	= NULL;
        wcex.lpszClassName	= wndclsname_;
        wcex.hIconSm		= LoadIcon(NULL, MAKEINTRESOURCE(IDI_APPLICATION));

        wndcls_ = RegisterClassEx(&wcex);
    }

    hwnd_ = ::CreateWindowEx(dwExStyle, wndclsname_, lpWindowName, dwStyle,
        X, Y, nWidth, nHeight, hWndParent, hMenu, hInstance, (LPVOID)this);

    if (!hwnd_)
    {
        return FALSE;
    }

    return TRUE;
}

LRESULT BaseWnd::WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam){
    // if it is the main window of the UI thread, quit the message loop
    if (msg == WM_CLOSE && bMainWnd_ ){
        ::PostQuitMessage (0);
        return 0;
    }else{
        return ::DefWindowProc (hWnd, msg, wParam, lParam);
    }
}

// static functions to maitain a global map;
LRESULT CALLBACK BaseWnd::WndProcThunk(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if ( msg == WM_CREATE ) { // we map hWnd to this pointer in this special message
        LPCREATESTRUCT lpcs = (LPCREATESTRUCT)lParam;
        BaseWnd * pThis = (BaseWnd *) lpcs->lpCreateParams;
        AutoLock autolock (wndmaplock_);
        wndmap_[hWnd] = pThis;
        return pThis->WndProc( hWnd,  msg,  wParam,  lParam );
    }else if ( msg == WM_DESTROY ) {
        AutoLock autolock (wndmaplock_);
        BaseWnd * pThis = wndmap_[hWnd];
        wndmap_.erase(hWnd);
        if ( pThis ) return pThis->WndProc( hWnd,  msg,  wParam,  lParam );
    }else{
        BaseWnd * pThis = wndmap_[hWnd];
        if ( pThis ) {
            return pThis->WndProc( hWnd,  msg,  wParam,  lParam );
        } else{
            return ::DefWindowProc( hWnd,  msg,  wParam,  lParam);
        }
    }
    return 0;
}
}// namespace WinRTCSDK