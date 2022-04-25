#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(uMsg == WM_CLOSE)
    {
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

HWND InitWindow(int width, int height, const char* title)
{
    static WNDCLASSEXA windowClass = {};
    static bool doClassInit = true;
    if(doClassInit)
    {
        windowClass.cbSize = sizeof(WNDCLASSEXA);
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = WindowProc;
        windowClass.lpszClassName = title;

        if(!RegisterClassExA(&windowClass))
            return nullptr;
        
        doClassInit = false;
    }
    
    DWORD windowStyle = WS_OVERLAPPEDWINDOW;
    DWORD exWindowStyle = WS_EX_OVERLAPPEDWINDOW;

    RECT windowRect = {
        .right = width,
        .bottom = height
    };
    if(AdjustWindowRectEx(&windowRect, windowStyle, 0, exWindowStyle))
    {
        width = windowRect.right - windowRect.left;
        height = windowRect.bottom - windowRect.top;
    }

    return CreateWindowExA(
        exWindowStyle,
        windowClass.lpszClassName,
        title,
        windowStyle,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        width,
        height,
        nullptr,
        nullptr,
        nullptr,
        nullptr
    );
}

bool ProcessWindowEvents()
{
    MSG event = {};
    BOOL peekRes = 0;
    while((peekRes = PeekMessageA(&event, nullptr, 0, 0, PM_REMOVE)))
    {
        if(peekRes < 0)
            continue;
        
        if(event.message == WM_QUIT)
            return false;
        
        DispatchMessageA(&event);
    }
    return true;
}

void ShowWindow(HWND windowHandle)
{
    ShowWindow(windowHandle, SW_SHOW);
}

// GOAL: 
// --------
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// --------
// TODO: create D3D11 context and clear screen to blue color
// TODO: draw a red triangle using vertex & fragment shaders
// TODO: vector3/vector4 and mat4 structs with basic operators/helpers
// TODO: triangle with ortho projection
// TODO: mouse and keyboard input handling
// TODO: perspective projection
// TODO: fps flying camera
// TODO: a generated line grid
// TODO: get a sample .obj file
// TODO: load data from .obj file
// TODO: render the loaded model data
// TODO: mouse drag controls for model rotation
// TODO: fps & draw model stats text on screen

int main()
{
    HWND window = InitWindow(1280, 720, "objviewer");
    if(window == nullptr)
        return -1;

    ShowWindow(window);
    while(true)
    {
        if(!ProcessWindowEvents())
            break;
    }    

    DestroyWindow(window);
    return 0;
}