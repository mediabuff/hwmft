#pragma once

#include "ComPtrDefs.h"

namespace WinRTCSDK{

////////////// This is helper class to access locked media buffer
class MediaBufferLock
{   
public:
    MediaBufferLock(_com_ptr_IMFMediaBuffer pBuffer) :
		m_p2DBuffer(NULL), m_bLocked(FALSE), m_bLocked2D(FALSE)
    {
        m_pBuffer = pBuffer;
        // Query for the 2-D buffer interface. OK if this fails.
        (void)m_pBuffer->QueryInterface(IID_PPV_ARGS(&m_p2DBuffer));

		m_pD3DSurf_ = NULL;
		HRESULT hr = ::MFGetService(pBuffer, MR_BUFFER_SERVICE, __uuidof(m_pD3DSurf_), (LPVOID*)&m_pD3DSurf_);
    }

    ~MediaBufferLock()
    {
        UnlockBuffer();
		m_p2DBuffer = NULL;
		m_pD3DSurf_ = NULL;
    }
	
	_com_ptr_IDirect3DSurface9 GetD3DSurface()
	{
		return m_pD3DSurf_;
	}
    
	// lock2DIfAvailable : lock 2d buffer if available
	HRESULT LockBuffer (LONG  lDefaultStride,  BYTE  **ppbScanLine0, 
		LONG  *plStride, DWORD dwHeightInPixels, BOOL lock2DIfAvailable)
    {
        HRESULT hr = S_OK;
        // Use the 2-D version if available.
        if (m_p2DBuffer && lock2DIfAvailable)
        {
            hr = m_p2DBuffer->Lock2D(ppbScanLine0, plStride);
		    m_bLocked2D = (SUCCEEDED(hr));
        }
        else// Use non-2D version.
        {        
            BYTE *pData = NULL;
            hr = m_pBuffer->Lock(&pData, NULL, NULL);
            if (SUCCEEDED(hr))
            {
                *plStride = lDefaultStride;
                if (lDefaultStride < 0)
                {
                    // Bottom-up orientation. Return a pointer to the start of the
                    // last row *in memory* which is the top row of the image.
                    *ppbScanLine0 = pData + ::abs(lDefaultStride) * (dwHeightInPixels - 1);
                }
                else
                {
                    // Top-down orientation. Return a pointer to the start of the buffer.
                    *ppbScanLine0 = pData;
                }
            }
	        m_bLocked = SUCCEEDED(hr);
        }
        return hr;
    }

    void UnlockBuffer()
    {
		if (m_bLocked2D)
		{
			(void)m_p2DBuffer->Unlock2D();
			m_bLocked2D = FALSE;
		}

		if (m_bLocked)
		{
			(void)m_pBuffer->Unlock();
			m_bLocked = FALSE;
		}
    }

private:
    _com_ptr_IDirect3DSurface9 m_pD3DSurf_;
    _com_ptr_IMFMediaBuffer    m_pBuffer;
    _com_ptr_IMF2DBuffer       m_p2DBuffer;
    BOOL   m_bLocked;
    BOOL   m_bLocked2D;
};
}