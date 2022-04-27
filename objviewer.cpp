#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>

#if DEBUG
#define ASSERT(x) if(x) {} else { __debugbreak(); }
#else
#define ASSERT(x)
#endif

#define ARRAY_LEN(x) sizeof(x) / sizeof((x)[0])

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

struct Dx11
{
    IDXGISwapChain* swapchain;
    ID3D11Device* device;
    ID3D11DeviceContext* context;
};

bool InitDx11(Dx11* dx, int windowWidth, int windowHeight, HWND window)
{
    *dx = {};

    UINT deviceCreationFlags = 0;
#if DEBUG
    deviceCreationFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featureLevels[] = {
        D3D_FEATURE_LEVEL_11_0
    };

    DXGI_SWAP_CHAIN_DESC swapchainDesc = {
        .BufferDesc = {
            .Width = (UINT)windowWidth,
            .Height = (UINT)windowHeight,
            .RefreshRate = {
                .Numerator = 1,
                .Denominator = 144
            },
            .Format = DXGI_FORMAT_R8G8B8A8_UNORM
        },
        .SampleDesc = {
            .Count = 1,
            .Quality = 0
        },
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 2,
        .OutputWindow = window,
        .Windowed = TRUE,
        .SwapEffect = DXGI_SWAP_EFFECT_DISCARD
    };

    IDXGISwapChain* swapchain = nullptr;
    ID3D11Device* device = nullptr;
    ID3D11DeviceContext* context = nullptr;

    HRESULT res = D3D11CreateDeviceAndSwapChain(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        deviceCreationFlags,
        featureLevels,
        ARRAY_LEN(featureLevels),
        D3D11_SDK_VERSION,
        &swapchainDesc,
        &swapchain,
        &device,
        nullptr,
        &context
    );
    if(res != S_OK) 
    {
        ASSERT(false);
        return false;
    }

    *dx = {
        .swapchain = swapchain,
        .device = device,
        .context = context
    };
    return true;
}

void FreeDx11(Dx11* dx)
{
    dx->swapchain->Release();
    dx->device->Release();
    dx->context->Release();

    *dx = {};
}

struct Dx11Backbuffer
{
    ID3D11Texture2D* buffer;
    ID3D11RenderTargetView* view;
};

Dx11Backbuffer InitDx11Backbuffer(Dx11* dx)
{
    Dx11Backbuffer backbuffer = {};
    dx->swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backbuffer.buffer);
    HRESULT res = dx->device->CreateRenderTargetView(backbuffer.buffer, nullptr, &backbuffer.view);
    ASSERT(res == S_OK);
    return backbuffer;
}

void FreeDx11Backbuffer(Dx11Backbuffer* backbuffer)
{
    backbuffer->buffer->Release();
    backbuffer->view->Release();
    *backbuffer = {};
}

D3D11_VIEWPORT InitDx11ViewportForWindow(HWND window)
{
    RECT windowRect = {};
    GetClientRect(window, &windowRect);
    return {
        .Width = (float)(windowRect.right - windowRect.left),
        .Height = (float)(windowRect.bottom - windowRect.top),
        .MinDepth = 0.0f,
        .MaxDepth = 1.0f
    };
}

ID3D11RasterizerState* InitDx11RasterizerState(Dx11* dx)
{
    D3D11_RASTERIZER_DESC rasterizerDesc = {
        .FillMode = D3D11_FILL_SOLID,
        .CullMode = D3D11_CULL_BACK,
        .FrontCounterClockwise = TRUE
    };
    ID3D11RasterizerState* rasterizerState = nullptr;
    dx->device->CreateRasterizerState(&rasterizerDesc, &rasterizerState);

    return rasterizerState;
}

// GOAL: 
// --------
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// --------
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

    Dx11 dx = {};
    if(!InitDx11(&dx, 1280, 720, window))
        return -1;

    Dx11Backbuffer backbuffer = InitDx11Backbuffer(&dx);
    float clearColor[] = { 0.3f, 0.4f, 0.9f, 1.0f };

    D3D11_VIEWPORT viewport = InitDx11ViewportForWindow(window);

    ID3D11RasterizerState* rasterizerState = InitDx11RasterizerState(&dx);

    ShowWindow(window);
    while(true)
    {
        if(!ProcessWindowEvents())
            break;

        dx.context->RSSetViewports(1, &viewport);
        dx.context->RSSetState(rasterizerState);
        dx.context->OMSetRenderTargets(1, &backbuffer.view, nullptr);
        dx.context->ClearRenderTargetView(backbuffer.view, clearColor);
        dx.swapchain->Present(1, 0);
    }    

    rasterizerState->Release();
    FreeDx11Backbuffer(&backbuffer);
    FreeDx11(&dx);
    DestroyWindow(window);
    return 0;
}