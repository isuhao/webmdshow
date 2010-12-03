#include <new>
#include <windows.h>
#include <windowsx.h>
#include <mfplay.h>
#include <mferror.h>
#include <tchar.h>
#include <strsafe.h>

// strsafe deprecates several functions used in gtest; disable the warnings
#pragma warning(push)
#pragma warning(disable:4995)
#include "gtest/gtest.h"
#pragma warning(pop)

#include "debugutil.hpp"
#include "mfplayer.hpp"
#include "windowutil.hpp"

struct WindowedMfPlayer
{
    WebmMfUtil::MfPlayer* ptr_mfplayer;
    WebmMfUtil::WebmMfWindow* ptr_mfwindow;
};


//-----------------------------------------------------------------------------
// ShowErrorMessage
//
// Error message display utility-- MessageBox wrapper w/HRESULT translation.
//-----------------------------------------------------------------------------
void ShowErrorMessage(PCWSTR format, HRESULT hrErr)
{
    HRESULT hr = S_OK;
    WCHAR msg[MAX_PATH];

    hr = StringCbPrintf(msg, sizeof(msg), L"%s (hr=0x%X)", format, hrErr);

    if (SUCCEEDED(hr))
    {
        MessageBox(NULL, msg, L"Error", MB_ICONERROR);
    }
}

//-----------------------------------------------------------------------------
// OnClose
//
// Handles the WM_CLOSE message.
//-----------------------------------------------------------------------------

void OnClose(HWND hwnd)
{
    LONG_PTR ptr_ourdata = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    assert(ptr_ourdata);

    WindowedMfPlayer* ptr_player =
        reinterpret_cast<WindowedMfPlayer*>(ptr_ourdata);

    if (ptr_player)
    {
        if (ptr_player->ptr_mfplayer)
        {
            ptr_player->ptr_mfplayer->Close();
            ptr_player->ptr_mfplayer = NULL;
        }
    }

    PostQuitMessage(0);
}


//-----------------------------------------------------------------------------
// OnPaint
//
// Handles the WM_PAINT message.
//-----------------------------------------------------------------------------

void OnPaint(HWND hwnd)
{
    PAINTSTRUCT ps;
    HDC hdc = 0;

    hdc = BeginPaint(hwnd, &ps);

    LONG_PTR ptr_ourdata = GetWindowLongPtr(hwnd, GWLP_USERDATA);
    assert(ptr_ourdata);

    WindowedMfPlayer* ptr_player =
        reinterpret_cast<WindowedMfPlayer*>(ptr_ourdata);

    if (ptr_player && ptr_player->ptr_mfplayer)
    {
        ptr_player->ptr_mfplayer->UpdateVideo();
    }
    else
    {
        // There is no video stream, or playback has not started.
        // Paint the entire client area.

        FillRect(hdc, &ps.rcPaint, (HBRUSH) (COLOR_WINDOW+1));
    }

    EndPaint(hwnd, &ps);
}


//-----------------------------------------------------------------------------
// OnSize
//
// Handles the WM_SIZE message.
//-----------------------------------------------------------------------------

void OnSize(HWND hwnd, UINT state, int /*cx*/, int /*cy*/)
{
    if (state == SIZE_RESTORED)
    {
        LONG_PTR ptr_ourdata = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        assert(ptr_ourdata);

        WindowedMfPlayer* ptr_player =
            reinterpret_cast<WindowedMfPlayer*>(ptr_ourdata);

        if (ptr_player && ptr_player->ptr_mfplayer)
        {
            ptr_player->ptr_mfplayer->UpdateVideo();
        }
    }
}


//-----------------------------------------------------------------------------
// OnKeyDown
//
// Handles the WM_KEYDOWN message.
//-----------------------------------------------------------------------------

void OnKeyDown(HWND hwnd, UINT vk, BOOL /*fDown*/, int /*cRepeat*/,
               UINT /*flags*/)
{
    HRESULT hr = S_OK;

    switch (vk)
    {
    case VK_SPACE:
        // Toggle between playback and paused/stopped.
        LONG_PTR ptr_ourdata = GetWindowLongPtr(hwnd, GWLP_USERDATA);
        assert(ptr_ourdata);

        WindowedMfPlayer* ptr_player =
            reinterpret_cast<WindowedMfPlayer*>(ptr_ourdata);

        if (ptr_player->ptr_mfplayer)
        {
            MFP_MEDIAPLAYER_STATE state = ptr_player->ptr_mfplayer->GetState();

            if (state == MFP_MEDIAPLAYER_STATE_PAUSED ||
                state == MFP_MEDIAPLAYER_STATE_STOPPED)
            {
                hr = ptr_player->ptr_mfplayer->Play();
            }
            else if (state == MFP_MEDIAPLAYER_STATE_PLAYING)
            {
                hr = ptr_player->ptr_mfplayer->Pause();
            }
        }

        break;
    }

    if (FAILED(hr))
    {
        ShowErrorMessage(TEXT("Playback Error"), hr);
    }
}

LRESULT CALLBACK window_handler(HWND hwnd, UINT uMsg, WPARAM wParam,
                                LPARAM lParam)
{
    switch (uMsg)
    {
        HANDLE_MSG(hwnd, WM_CLOSE,   OnClose);
        HANDLE_MSG(hwnd, WM_KEYDOWN, OnKeyDown);
        HANDLE_MSG(hwnd, WM_PAINT,   OnPaint);
        HANDLE_MSG(hwnd, WM_SIZE,    OnSize);

    case WM_ERASEBKGND:
        return 1;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        ShowErrorMessage(L"No file specified", E_FAIL);
        return EXIT_FAILURE;
    }

    const DWORD co_init_flags =
        COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE;

    HRESULT hr = CoInitializeEx(NULL, co_init_flags);
    if (FAILED(hr))
    {
        ShowErrorMessage(L"COM init failed", E_FAIL);
        return EXIT_FAILURE;
    }

    std::wstring url_str = argv[1];

    WebmMfUtil::WebmMfWindow mfwindow(window_handler);
    hr = mfwindow.Create();
    assert(SUCCEEDED(hr));

    if (FAILED(hr))
    {
        ShowErrorMessage(L"mfwindow.Create failed!", hr);
        return EXIT_FAILURE;
    }


    WebmMfUtil::MfPlayer mfplayer;

    hr = mfplayer.Open(url_str);
    assert(SUCCEEDED(hr));

    if (SUCCEEDED(hr))
    {
        hr = mfplayer.Play();
        assert(SUCCEEDED(hr));
    }

    CoUninitialize();

    return EXIT_SUCCESS;
}
