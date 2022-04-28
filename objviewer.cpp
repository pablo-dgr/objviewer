#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

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

struct Vec3
{
    float x;
    float y;
    float z;
};

ID3D11Buffer* CreateStaticDx11VertexBuffer(void* data, size_t byteSize, Dx11* dx)
{
    D3D11_BUFFER_DESC vertexBufferDesc = {
        .ByteWidth = (UINT)byteSize,
        .Usage = D3D11_USAGE_IMMUTABLE,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER
    };

    D3D11_SUBRESOURCE_DATA vertexBufData = {
        .pSysMem = data
    };

    ID3D11Buffer* vertexBuffer = nullptr;
    HRESULT res = dx->device->CreateBuffer(&vertexBufferDesc, &vertexBufData, &vertexBuffer);
    ASSERT(res == S_OK);
    return vertexBuffer;
}

ID3D11InputLayout* CreateDx11InputLayout(Dx11* dx, void* vertexShaderBytecode, size_t vertexShaderBytecodeSize)
{
    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        {
            .SemanticName = "POSITION",
            .SemanticIndex = 0,
            .Format = DXGI_FORMAT_R32G32B32_FLOAT,
            .InputSlot = 0,
            .AlignedByteOffset = 0,
            .InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA,
            .InstanceDataStepRate = 0
        }
    };

    ID3D11InputLayout* inputLayout = nullptr;
    HRESULT res = dx->device->CreateInputLayout(
        inputElements,
        ARRAY_LEN(inputElements),
        vertexShaderBytecode,
        vertexShaderBytecodeSize,
        &inputLayout
    );
    ASSERT(res != S_OK);
    return inputLayout;
}

// GOAL: 
// --------
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// --------
// TODO: draw a red triangle using vertex & fragment shaders
// TODO: Vec3/Vec4 and Mat4 structs with basic operators/helpers
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

struct String
{
    const char* data;
    size_t len;
};

void FreeString(String* str)
{
    free((void*)str->data);
    *str = {};
}

String ReadAllTextFromFile(const char* filename)
{
    HANDLE file = CreateFileA(
        filename,
        GENERIC_READ,
        FILE_SHARE_READ,
        nullptr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        nullptr
    );
    if(file == INVALID_HANDLE_VALUE)
    {
        ASSERT(false);
        return {};
    }

    LARGE_INTEGER tmpFileSize = {};
    if(!GetFileSizeEx(file, &tmpFileSize))
    {
        ASSERT(false);
        CloseHandle(file);
        return {};
    }

    size_t fileSize = tmpFileSize.QuadPart;
    void* buffer = calloc(1, fileSize + 1);
    ASSERT(buffer != nullptr);

    ASSERT(fileSize < 0xFFFFFFFF); // big ass files not supported (yet)

    DWORD bytesRead = 0;
    ReadFile(
        file,
        buffer,
        (DWORD)fileSize,
        &bytesRead,
        nullptr
    );
    if(bytesRead == 0)
    {
        ASSERT(false);
        free(buffer);
        CloseHandle(file);
        return {};
    }

    CloseHandle(file);
    return {
        .data = (char*)buffer,
        .len = fileSize
    };
}

enum class ShaderType
{
    Vertex,
    Pixel
};

ID3DBlob* CompileShaderCode(ShaderType type, String code)
{
    const char* target = nullptr;
    if(type == ShaderType::Pixel)
        target = "ps_5_0";
    else if(type == ShaderType::Vertex)
        target = "vs_5_0";
    else
        ASSERT(false);

    UINT compileFlags = 0;
#if DEBUG
    compileFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* compiledCode = nullptr;
    ID3DBlob* compileErrors = nullptr;

    HRESULT res = D3DCompile(
        (void*)code.data,
        code.len,
        nullptr,
        nullptr,
        nullptr,
        "main",
        target,
        compileFlags,
        0,
        &compiledCode,
        &compileErrors
    );
    ASSERT(res);

    if(compileErrors != nullptr)
    {
        printf("Failed to compile shader: %s\n", (const char*)compileErrors->GetBufferPointer());
        compileErrors->Release();
        return nullptr;
    }

    return compiledCode;
}

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

    Vec3 vertices[] = {
        {  0.0f,  0.5f, 0.0f },
        { -0.5f, -0.5f, 0.0f },
        {  0.5f, -0.5f, 0.0f }
    };

    ID3D11Buffer* vertexBuffer = CreateStaticDx11VertexBuffer(vertices, sizeof(vertices), &dx);
    String vsCode = ReadAllTextFromFile("res/basiccolorvs.hlsl");
    ID3DBlob* vsByteCode = CompileShaderCode(ShaderType::Vertex, vsCode);
    ID3D11VertexShader* vertexShader = nullptr;
    dx.device->CreateVertexShader(
        vsByteCode->GetBufferPointer(), 
        vsByteCode->GetBufferSize(), 
        nullptr, 
        &vertexShader
    );

    // TODO: create ps from file here

    // TODO: create input layout

    // TODO: use vertexbuffer, inputlayout, shaders to do draw call

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

    vsByteCode->Release();
    FreeString(&vsCode);

    vertexBuffer->Release();
    rasterizerState->Release();
    FreeDx11Backbuffer(&backbuffer);
    FreeDx11(&dx);
    DestroyWindow(window);
    return 0;
}