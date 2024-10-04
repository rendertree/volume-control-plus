// Copyright (c) 2024 Wildan R Wijanarko
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include <chrono>
#include <thread>
#include <string>
#include <stdint.h>
#include <windows.h>
#include <commctrl.h>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys_devpkey.h>

// Window size
constexpr int windowWidth{580};
constexpr int windowHeight{380};

// Window position
static struct { float x; float y; } windowPos;

// Volume lock/unlock status
static bool isVolumeLocked{false};

// Volume mute status
static bool isMuted{false};

// Lock the mute toggle
static bool muteLock{true};

// Max volume
static float maxVolume{0.4f};

// PIN textbox
HWND pinTextBox{};

// Max volume textbox
HWND maxVolumeTextBox{};

// Mute lock checkbox
HWND hMuteToggleCheckbox{};

// Mute checkbox
HWND hMuteCheckbox{};

// PIN string
static std::string strText{};
static std::string strPIN{};

// Max volume string
static std::string strMaxVolume{};

constexpr float minVolume{0.0f};
constexpr uint8_t x{30};

// Set master volume
static HRESULT SetMasterVolume(float volume)
{
    // Initialize COM library
    HRESULT hr{CoInitialize(NULL)};

    // Create device enumerator
    IMMDeviceEnumerator* pEnumerator{};
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // Get the default audio endpoint
    IMMDevice* pDevice{};
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // Activate the audio endpoint volume interface
    IAudioEndpointVolume* pEndpointVolume{};
    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);

    // Set the master volume level
    hr = pEndpointVolume->SetMasterVolumeLevelScalar(volume, NULL);

    // Release resources
    pEndpointVolume->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return hr;
}

// Get master volume
static float GetMasterVolume()
{
    float volume{0.0f};

    // Initialize COM library
    HRESULT hr{CoInitialize(NULL)};

    // Create device enumerator
    IMMDeviceEnumerator* pEnumerator{};
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // Get the default audio endpoint
    IMMDevice* pDevice{};
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // Activate the audio endpoint volume interface
    IAudioEndpointVolume* pEndpointVolume{};
    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);

    // Get the master volume level
    hr = pEndpointVolume->GetMasterVolumeLevelScalar(&volume);

    // Release resources
    pEndpointVolume->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    return volume;
}

// Check collision between mouse and rectangle
static bool CheckCollisionMouseRect(POINT mousePos, RECT rect) 
{
    return PtInRect(&rect, mousePos);
}

// Returns true if system audio is muted
static bool IsMuted()
{
    // Initialize COM library
    HRESULT hr{CoInitialize(NULL)};

    // Create device enumerator
    IMMDeviceEnumerator* pEnumerator{};
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // Get the default audio endpoint
    IMMDevice* pDevice{};
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // Activate the audio endpoint volume interface
    IAudioEndpointVolume* pEndpointVolume{};
    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);

    // Get mute state
    BOOL mute{};
    hr = pEndpointVolume->GetMute(&mute);

    // Release resources
    pEndpointVolume->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();

    // Return mute state as bool
    return mute == TRUE;
}

// Mutes/unmutes system audio based on the mute parameter
static void SetMute(bool mute)
{
    // Initialize COM library
    HRESULT hr{CoInitialize(NULL)};

    // Create device enumerator
    IMMDeviceEnumerator* pEnumerator{};
    hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);

    // Get the default audio endpoint
    IMMDevice* pDevice{};
    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);

    // Activate the audio endpoint volume interface
    IAudioEndpointVolume* pEndpointVolume{};
    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pEndpointVolume);

    // Set mute state
    hr = pEndpointVolume->SetMute(mute ? TRUE : FALSE, NULL);

    // Release resources
    pEndpointVolume->Release();
    pDevice->Release();
    pEnumerator->Release();
    CoUninitialize();
}

// Declare the window procedure
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Windows API that contains information for creating and manipulating an icon in the system tray (notification area)
NOTIFYICONDATA nid{};

// Entry point of the application
int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
    // Create the window class
    WNDCLASSW wc{};

    wc.lpfnWndProc   = WindowProc;
    wc.hInstance     = hInstance;
    wc.lpszClassName = L"Volume Control Plus";

    // Register the window class
    RegisterClassW(&wc);

    // Create the window
    const HWND hwnd{CreateWindowExW(
        0,
        wc.lpszClassName,
        L"Volume Control Plus",
        WS_OVERLAPPEDWINDOW,

        // Window position and size 
        0, 0, windowWidth, windowHeight,

        NULL,
        NULL,
        hInstance,
        NULL
    )};

    // Create lock/unlock volume button
    const HWND lockUnlockbuttonHwnd{CreateWindow(
        L"BUTTON",
        L"Lock Volume",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,

        // Lock/unlock volume button position and size
        x + 250, 50, 120, 30,

        hwnd,
        (HMENU)1,
        GetModuleHandle(NULL),
        NULL
    )};

    // Create the slider control
    const HWND slider{CreateWindow(
        TRACKBAR_CLASS,
        L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ,

        // Volume slider position and size
        x + 40, 50, 200, 30,

        hwnd,
        NULL,
        hInstance,
        NULL
    )};

    // Create set max volume button
    const HWND setMaxVolumebuttonHwnd{CreateWindow(
        L"BUTTON",
        L"Set Max Volume",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,

        // Set PIN button position and size
        x + 170, 150, 120, 30,

        hwnd,
        (HMENU)2,
        GetModuleHandle(NULL),
        NULL
    )};

    // Create set PIN button
    const HWND setPINbuttonHwnd{CreateWindow(
        L"BUTTON",
        L"Set PIN",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,

        // Set PIN button position and size
        x + 170, 250, 120, 30,

        hwnd,
        (HMENU)2,
        GetModuleHandle(NULL),
        NULL
    )};

    // Current volume
    float currentVolume{GetMasterVolume()};
    
    // Set the range of the slider
    SendMessage(slider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 100));
    
    // Set the initial position of the slider
    SendMessage(slider, TBM_SETPOS, TRUE, static_cast<LPARAM>(currentVolume * 100.0f));

    // Get the monitor size
    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(MONITORINFO);

    // Windows API represents a handle to a physical display monitor
    const HMONITOR hMonitor{MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY)};

    if (hMonitor != NULL)
    {
        GetMonitorInfo(hMonitor, &monitorInfo);

        windowPos.x = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.right / 1.5f - 50.0f;
        windowPos.y = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.bottom / 1.5f - 50.0f;
    }
    else
    {
        return 0;
    }

    if (hwnd == NULL)
    {
        MessageBoxW(NULL, L"Failed to create the window", L"Error", MB_ICONERROR);

        return 0;
    }

    // Load the app icon from the file
    const HICON hCustomIcon{(HICON)LoadImage(NULL, L"lock.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE)};

    // Initialize the NOTIFYICONDATA structure
    memset(&nid, 0, sizeof(NOTIFYICONDATA));
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd   = hwnd;
    nid.uID    = 1;
    nid.uFlags = NIF_ICON | NIF_TIP | NIF_MESSAGE;
    nid.uCallbackMessage = WM_USER + 1;
    nid.hIcon  = hCustomIcon;
    lstrcpy(nid.szTip, L"Volume Control Plus");

    // Set the icon for the application window
    SendMessage(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hCustomIcon);   // Set the large icon
    SendMessage(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hCustomIcon); // Set the small icon

    // Create a white HBITMAP
    const HDC     hdcScreen{GetDC(NULL)};
    const HDC     hdcMem{CreateCompatibleDC(hdcScreen)};
    const HBITMAP hBitmap{CreateCompatibleBitmap(hdcScreen, 500, 300)};
    const HGDIOBJ hOldBitmap{SelectObject(hdcMem, hBitmap)};
    const HBRUSH  hBrush{CreateSolidBrush(RGB(255, 255, 255))};

    // Define the size of the white bitmap
    const RECT rect{ 0, 0, 500, 300 };
    FillRect(hdcMem, &rect, hBrush);

    SelectObject(hdcMem, hOldBitmap);
    DeleteObject(hBrush);
    ReleaseDC(NULL, hdcScreen);

    // Define the blend function for alpha blending (opaque white)
    BLENDFUNCTION blend{};

    blend.BlendOp             = AC_SRC_OVER;
    blend.SourceConstantAlpha = 255; // 255 (opaque)
    blend.AlphaFormat         = AC_SRC_ALPHA;

    // Update the layered window with the white bitmap
    POINT ptZero{};
    SIZE  size{500, 300};
    POINT ptLocation{0, 0};

    // Show and update the window
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // Message loop
    MSG msg{};

    // Get current mute status
    isMuted = IsMuted();

    while (true)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);

            if (msg.message == WM_QUIT)
            {
                return 0;
            }
        }

        if (isVolumeLocked)
        {
            const float sliderValue{static_cast<float>(SendMessage(slider, TBM_GETPOS, 0, 0))};
            currentVolume = sliderValue / 100.0f;

            SetMasterVolume(currentVolume);

            if (muteLock)
            {
                SetMute(isMuted);
            }

            SetWindowText(lockUnlockbuttonHwnd, L"Unlock Volume");
        }
        else if (!isVolumeLocked)
        {
            if (currentVolume >= minVolume && currentVolume <= maxVolume)
            {
                // Get mouse position
                POINT mousePos{};
                GetCursorPos(&mousePos);
                ScreenToClient(hwnd, &mousePos);

                const RECT sliderRect{x + 40, 50, x + 240, 80};

                if (CheckCollisionMouseRect(mousePos, sliderRect))
                {
                    const float sliderValue{static_cast<float>(SendMessage(slider, TBM_GETPOS, 0, 0))};
                    currentVolume = sliderValue / 100.0f;

                    SetMasterVolume(currentVolume);
                }
                else
                {
                    currentVolume = GetMasterVolume();

                    // Assuming currentVolume is a float value between 0.0 and 1.0 representing the volume level
                    // Convert it to an integer value between 0 and 100 for the trackbar
                    const int sliderValue{static_cast<int>(currentVolume * 100)};

                    // Set the position of the slider
                    SendMessage(slider, TBM_SETPOS, TRUE, sliderValue);

                    // Set the range of the slider (0 to 100)
                    SendMessage(slider, TBM_SETRANGEMIN, TRUE, 0);
                    SendMessage(slider, TBM_SETRANGEMAX, TRUE, 100);
                }
            }
            else if (currentVolume > maxVolume)
            {
                currentVolume = maxVolume;
                SetMasterVolume(currentVolume);
            }

            SetWindowText(lockUnlockbuttonHwnd, L"Lock Volume");
        }

        const bool isClickable{(strText == strPIN)};

        // Enable or disable controls based on state
        EnableWindow(slider, !isVolumeLocked);           
        EnableWindow(lockUnlockbuttonHwnd, isClickable); 
        EnableWindow(setPINbuttonHwnd, strPIN.empty());

        UpdateLayeredWindow(hwnd, NULL, &ptLocation, &size, hdcMem, &ptZero, RGB(0, 0, 0), &blend, ULW_ALPHA);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // Clean up resources
    SelectObject(hdcMem, hOldBitmap);
    DeleteDC(hdcMem);
    DeleteObject(hBitmap);
    DestroyIcon(hCustomIcon);

    return msg.wParam;
}

// Window procedure
static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // In Windows programming, those are messages defined by the Windows operating 
    // system that are sent to a window procedure (also known as a window callback function) 
    // to notify it about various events or requests.
    switch (uMsg)
    {
    // WM_CREATE: This message is sent to a window when it is being created. 
    // It allows the window procedure to perform initialization tasks and 
    // allocate resources associated with the window.
    case WM_CREATE:
    {
        // Create the max volume textbox
        maxVolumeTextBox = CreateWindowEx(
            0,
            L"EDIT",
            L"100",
            WS_VISIBLE | WS_CHILD | WS_BORDER,

            // Max Volume textBox position and size
            x + 40, 150, 120, 30,

            hwnd,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );

        // Create the PIN textbox
        pinTextBox = CreateWindowEx(
            0,
            L"EDIT",
            L"Enter PIN",
            WS_VISIBLE | WS_CHILD | WS_BORDER,

            // PIN textbox position and size
            x + 40, 250, 120, 30,

            hwnd,
            NULL,
            GetModuleHandle(NULL),
            NULL
        );

        // Create the mute checkbox
        hMuteCheckbox = CreateWindowEx(
            0, L"BUTTON",
            L"Mute",
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX,

            // Position and size
            340, 150, 60, 30,

            hwnd,
            NULL,
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE),
            NULL
        );

        // Create the toggle mute checkbox
        hMuteToggleCheckbox = CreateWindowEx(
            0, L"BUTTON",  
            L"Lock Mute Toggle",   
            WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_CHECKBOX,

            // Position and size
            340, 250, 140, 30,
            
            hwnd,          
            NULL,          
            (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), 
            NULL          
        );

        SendMessage(hMuteToggleCheckbox, BM_SETCHECK, BST_CHECKED, 0);

    } break;
    // WM_COMMAND: This message is sent to a window when the user selects a menu item, 
    // clicks a button, or performs an action that generates a command from a control or menu.
    case WM_COMMAND:
    {
        // The volume lock button
        if (LOWORD(wParam) == 1 && HIWORD(wParam) == BN_CLICKED)
        {
            isVolumeLocked = !isVolumeLocked;
            
            // Get the current mute status
            isMuted = IsMuted();
        }

        // The button to set the max volume
        if (LOWORD(wParam) == 2 && HIWORD(wParam) == BN_CLICKED)
        {
            if (strMaxVolume.empty())
            {
                strMaxVolume = "100";
            }

            // Get the max volume; it should be more than 0 and less than or equal to 100
            const unsigned int max{static_cast<unsigned int>((atoi(strMaxVolume.c_str()) > 100) ? 100 : atoi(strMaxVolume.c_str()))};

            maxVolume = static_cast<float>(max/100.0f);
        }

        // The button to set the PIN
        if (LOWORD(wParam) == 3 && HIWORD(wParam) == BN_CLICKED)
        {
            strPIN = strText;
        }

        // PIN textbox
        if (reinterpret_cast<HWND>(lParam) == pinTextBox && HIWORD(wParam) == EN_CHANGE)
        {
            // Text has changed in the PIN textbox, update the std::string
            char buffer[256];
            GetWindowTextA(pinTextBox, buffer, sizeof(buffer));
            strText = buffer;
        }

        // Max volume textbox
        if (reinterpret_cast<HWND>(lParam) == maxVolumeTextBox && HIWORD(wParam) == EN_CHANGE)
        {
            // Text has changed in the max volume textbox, update the std::string
            char buffer[256];
            GetWindowTextA(maxVolumeTextBox, buffer, sizeof(buffer));
            strMaxVolume = buffer;
        }

        // Update the mute volume
        if ((HWND)lParam == hMuteCheckbox && !muteLock)
        {
            isMuted = !isMuted;

            SetMute(isMuted);
        }

        // Update the mute checkbox
        SendMessage(hMuteCheckbox, BM_SETCHECK, isMuted ? BST_CHECKED : BST_UNCHECKED, 0);

        // Update the toggle mute and the checkbox
        if ((HWND)lParam == hMuteToggleCheckbox) 
        {
            muteLock = !muteLock;

            SendMessage(hMuteToggleCheckbox, BM_SETCHECK, muteLock ? BST_CHECKED : BST_UNCHECKED, 0);   
        }

    } break;
    // WM_GETMINMAXINFO: This message is sent to a window when its size or position is about to change. 
    // It provides the window procedure with information about the window's minimum and maximum size constraints.
    case WM_GETMINMAXINFO:
    {
        // Prevent resizing to a smaller size
        MINMAXINFO* pMMI{(MINMAXINFO*)lParam};
        pMMI->ptMinTrackSize.x = windowWidth;
        pMMI->ptMinTrackSize.y = windowHeight;

    } break;
    // WM_PAINT: This message is sent to a window when it needs to be repainted. 
    // It typically occurs when a portion of the window is exposed after being obscured 
    // by another window or when the window is invalidated by a call to the InvalidateRect or UpdateWindow function.
    case WM_PAINT:
    {
        // The PAINTSTRUCT structure is a structure defined in 
        // the Windows API that is used in painting operations. 
        // It is used in conjunction with the BeginPaint and EndPaint 
        // functions for handling the painting of a window or client area.
        PAINTSTRUCT ps{};
        
        // The HDC (Handle to Device Context) is a data type in the Windows API 
        // that represents a handle to a device context. A device context is an abstraction 
        // that allows an application to interact with a device, such as a display or printer, 
        // for drawing graphics or performing other operations.
        const HDC hdc{BeginPaint(hwnd, &ps)};

        // Display text labels for volume and PIN settings
        const TCHAR* text0{ L"Set Volume Level:" };
        const TCHAR* text1{ L"Set Max Volume:" };
        const TCHAR* text2{ L"Set PIN:" };

        // Output the text at specified positions in the window
        TextOut(hdc, x + 10, 14, text0, lstrlen(text0));  
        TextOut(hdc, x + 10, 124, text1, lstrlen(text1));  
        TextOut(hdc, x + 10, 224, text2, lstrlen(text2));

        EndPaint(hwnd, &ps);

    } break;
    // WM_SIZE: This message is sent to a window when its size has changed, either due to the user resizing 
    // the window or through programmatic changes. It provides information about the new size and allows 
    // the window procedure to adjust its layout accordingly.
    case WM_SIZE:
    {
        // Update the window position to the center of screen
        SetWindowPos(
            hwnd, 
            NULL,

            // Window position and size
            windowPos.x, windowPos.y, windowWidth, windowHeight, 
            
            SWP_NOZORDER
        );

        if (wParam == SIZE_MINIMIZED)
        {
            // Add the icon to the system tray
            Shell_NotifyIcon(NIM_ADD, &nid);

            // Hide the window (SW_HIDE)
            ShowWindow(hwnd, SW_HIDE);
        }
        else if (wParam == SIZE_RESTORED)
        {
            // Remove the icon from the system tray
            Shell_NotifyIcon(NIM_DELETE, &nid);

            // Show the window (SW_SHOW)
            ShowWindow(hwnd, SW_SHOW);
            SetForegroundWindow(hwnd);
        }

    } break;
    // WM_USER: This message is a user-defined message that can be used by applications 
    // to communicate custom messages between different parts of the program.
    case WM_USER + 1:
    {
        // The system tray icon
        if (lParam == WM_LBUTTONUP)
        {
            // Restore the window when left-clicking on the system tray icon
            Shell_NotifyIcon(NIM_DELETE, &nid);
            ShowWindow(hwnd, SW_RESTORE);
            SetForegroundWindow(hwnd);
        }

    } break;
    // WM_SYSCOMMAND: This message is sent to a window when the user selects a command from the window menu 
    // (also known as the system menu) or when the window's title bar is double-clicked. 
    // It is commonly used to handle system commands such as minimizing, maximizing, or closing the window.
    case WM_SYSCOMMAND:
    {
        // Disable the "Restore Down" (Maximize) functionality
        if (wParam == SC_MAXIMIZE) return 0;

        // Disable the close functionality when the volume is locked
        if (wParam == SC_CLOSE && isVolumeLocked) return 0;

    } break;
    // WM_DESTROY: This message is sent to a window when it is being destroyed. 
    // It allows the window procedure to perform cleanup tasks and release any resources associated with the window.
    case WM_DESTROY:
    {
        // Remove the icon from the system tray before exiting
        Shell_NotifyIcon(NIM_DELETE, &nid);

        // Windows API function used to post a quit message to the message queue of the current thread
        PostQuitMessage(0);

        return 0;
    } break;
    // WM_CLOSE: This message is sent to a window when the user attempts to close it, typically by clicking 
    // the close button in the window's title bar. It provides an opportunity for the window procedure to confirm 
    // the close operation or perform any necessary cleanup.
    case WM_CLOSE:
    {
        DestroyWindow(hwnd);

        return 0;
    }
    default:
        return DefWindowProcW(hwnd, uMsg, wParam, lParam);
    }

    // Handle unhandled messages
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}