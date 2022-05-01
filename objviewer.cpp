#include <stdio.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <string.h>
#include <stdint.h>

#if DEBUG
#define ASSERT(x) if(x) {} else { __debugbreak(); }
#else
#define ASSERT(x)
#endif

#define ARRAY_LEN(x) sizeof(x) / sizeof((x)[0])

#define CHECK_CBUFFER_ALIGNMENT(x) static_assert(sizeof(x) % 16 == 0, "constant buffer data must be 16-byte aligned")


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

enum class Vkey
{
    Z = 'Z',
    Q = 'Q',
    S = 'S',
    D = 'D',
    A = 'A',
    Space = VK_SPACE
};

struct Keybind
{
    Vkey key;
    int keyDownTransitionCount;
    int keyUpTransitionCount;
    bool isKeyDown;
};

struct Input
{
    Keybind moveForward;
    Keybind moveBackward;
    Keybind moveLeft;
    Keybind moveRight;
    Keybind moveDown;
    Keybind moveUp;
    int mousePosX;
    int mousePosY;
    int mouseMoveX;
    int mouseMoveY;
};

void HandleKeyUpForBind(Keybind* keybind, MSG* event)
{
    if((int)keybind->key == event->wParam)
    {
        keybind->isKeyDown = false;
        keybind->keyUpTransitionCount += 1;
    }
}

void HandleKeyDownForBind(Keybind* keybind, MSG* event)
{
    if((int)keybind->key == event->wParam)
    {
        keybind->isKeyDown = true;
        if((event->lParam & (1 << 30)) == 0)
            keybind->keyDownTransitionCount += 1;
    }
}

void ResetKeyTransitions(Keybind* keybind)
{
    keybind->keyUpTransitionCount = 0;
    keybind->keyDownTransitionCount = 0;
}

void ResetInputKeyTransitions(Input* input)
{
    ResetKeyTransitions(&input->moveForward);
    ResetKeyTransitions(&input->moveBackward);
    ResetKeyTransitions(&input->moveLeft);
    ResetKeyTransitions(&input->moveRight);
    ResetKeyTransitions(&input->moveDown);
    ResetKeyTransitions(&input->moveUp);
}

void ResetRelativeInputMouseData(Input* input)
{
    input->mouseMoveX = 0;
    input->mouseMoveY = 0;
}

void GetMousePositionInWindow(Input* input, HWND window)
{
    POINT mousePos = {};
    GetCursorPos(&mousePos);
    ScreenToClient(window, &mousePos);
    input->mousePosX = mousePos.x;
    input->mousePosY = mousePos.y;
}

void SetupRawMouseInput()
{
    RAWINPUTDEVICE rawMouse = {
        .usUsagePage = 0x0001,
        .usUsage = 0x0002
    };

    BOOL res = RegisterRawInputDevices(&rawMouse, 1, sizeof(RAWINPUTDEVICE));
    ASSERT(res);
}

bool ProcessWindowEvents(Input* input)
{
    MSG event = {};
    BOOL peekRes = 0;
    while((peekRes = PeekMessageA(&event, nullptr, 0, 0, PM_REMOVE)))
    {
        if(peekRes < 0)
            continue;
        
        if(event.message == WM_INPUT)
        {
            RAWINPUT rawInput = {};
            UINT rawInputDataSize = sizeof(RAWINPUT);
            UINT res = GetRawInputData(
                (HRAWINPUT)event.lParam,
                RID_INPUT,
                &rawInput,
                &rawInputDataSize,
                sizeof(RAWINPUTHEADER)
            );
            ASSERT(res == sizeof(RAWINPUT));

            if(rawInput.header.dwType == RIM_TYPEMOUSE && rawInput.data.mouse.usFlags == MOUSE_MOVE_RELATIVE)
            {
                input->mouseMoveX += rawInput.data.mouse.lLastX;
                input->mouseMoveY += rawInput.data.mouse.lLastY;
            }
        }
        else if(event.message == WM_KEYUP)
        {
            HandleKeyUpForBind(&input->moveForward, &event);
            HandleKeyUpForBind(&input->moveBackward, &event);
            HandleKeyUpForBind(&input->moveLeft, &event);
            HandleKeyUpForBind(&input->moveRight, &event);
            HandleKeyUpForBind(&input->moveDown, &event);
            HandleKeyUpForBind(&input->moveUp, &event);
        }
        else if(event.message == WM_KEYDOWN)
        {
            HandleKeyDownForBind(&input->moveForward, &event);
            HandleKeyDownForBind(&input->moveBackward, &event);
            HandleKeyDownForBind(&input->moveLeft, &event);
            HandleKeyDownForBind(&input->moveRight, &event);
            HandleKeyDownForBind(&input->moveDown, &event);
            HandleKeyDownForBind(&input->moveUp, &event);
        }
        else if(event.message == WM_QUIT)
        {
            return false;
        }

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

D3D11_VIEWPORT GetDx11ViewportForWindow(HWND window)
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

bool UpdateDx11ViewportForWindow(D3D11_VIEWPORT* oldViewport, HWND window)
{
    D3D11_VIEWPORT newViewport = GetDx11ViewportForWindow(window);
    if(oldViewport->Width != newViewport.Width || oldViewport->Height != newViewport.Height)
    {
        *oldViewport = newViewport;
        return true;
    }
    return false;
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

struct Dx11VertexBuffer
{
    ID3D11Buffer* buffer;
    UINT stride;
    UINT byteOffset;
};

void FreeDx11VertexBuffer(Dx11VertexBuffer* vertexBuffer)
{
    vertexBuffer->buffer->Release();
    *vertexBuffer = {};
}

Dx11VertexBuffer CreateStaticDx11VertexBuffer(void* data, size_t byteSize, UINT stride, UINT byteOffset, Dx11* dx)
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
    return {
        .buffer = vertexBuffer,
        .stride = stride,
        .byteOffset = byteOffset
    };
}

ID3D11InputLayout* CreateDx11InputLayout(Dx11* dx, ID3DBlob* vsByteCode)
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
        vsByteCode->GetBufferPointer(),
        vsByteCode->GetBufferSize(),
        &inputLayout
    );
    ASSERT(res == S_OK);
    return inputLayout;
}

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
    ASSERT(res == S_OK);

    if(compileErrors != nullptr)
    {
        printf("Failed to compile shader: %s\n", (const char*)compileErrors->GetBufferPointer());
        compileErrors->Release();
        return nullptr;
    }

    return compiledCode;
}


struct Vec3
{
    float x;
    float y;
    float z;
};

Vec3 operator + (const Vec3& one, const Vec3& other)
{
    return {
        .x = one.x + other.x,
        .y = one.y + other.y,
        .z = one.z + other.z,
    };
}

Vec3 operator - (const Vec3& one, const Vec3& other)
{
    return {
        .x = one.x - other.x,
        .y = one.y - other.y,
        .z = one.z - other.z,
    };
}

Vec3 operator * (const Vec3& vec, float scalar)
{
    return {
        .x = vec.x * scalar,
        .y = vec.y * scalar,
        .z = vec.z * scalar,
    };
}

Vec3 operator / (const Vec3& vec, float scalar)
{
    return {
        .x = vec.x / scalar,
        .y = vec.y / scalar,
        .z = vec.z / scalar,
    };
}

struct Vec4
{
    float x;
    float y;
    float z;
    float w;
};

Vec4 operator + (const Vec4& one, const Vec4& other)
{
    return {
        .x = one.x + other.x,
        .y = one.y + other.y,
        .z = one.z + other.z,
        .w = one.w + other.w,
    };
}

Vec4 operator - (const Vec4& one, const Vec4& other)
{
    return {
        .x = one.x - other.x,
        .y = one.y - other.y,
        .z = one.z - other.z,
        .w = one.w - other.w,
    };
}

Vec4 operator * (const Vec4& vec, float scalar)
{
    return {
        .x = vec.x * scalar,
        .y = vec.y * scalar,
        .z = vec.z * scalar,
        .w = vec.w * scalar,
    };
}

Vec4 operator / (const Vec4& vec, float scalar)
{
    return {
        .x = vec.x / scalar,
        .y = vec.y / scalar,
        .z = vec.z / scalar,
        .w = vec.w / scalar,
    };
}

struct Mat4
{
    float data[4][4];
};

Mat4 operator * (const Mat4& one, const Mat4& other)
{
    Mat4 res = {};

    for(int x = 0; x < 4; x++) {
        for(int y = 0; y < 4; y++) {
            res.data[y][x] = 
                (one.data[0][x] * other.data[y][0]) + 
                (one.data[1][x] * other.data[y][1]) + 
                (one.data[2][x] * other.data[y][2]) + 
                (one.data[3][x] * other.data[y][3]);
        }
    }

    return res;
}

Mat4 IdentityMat4()
{
    Mat4 res = {};
    for(int i = 0; i < 4; i++)
        res.data[i][i] = 1.0f;
    return res;
}

Mat4 TranslateMat4(Vec3 position)
{
    Mat4 res = IdentityMat4();
    res.data[3][0] = position.x;
    res.data[3][1] = position.y;
    res.data[3][2] = position.z;
    return res;
}

Mat4 ScaleMat4(Vec3 scale)
{
    Mat4 res = IdentityMat4();
    res.data[0][0] = scale.x;
    res.data[1][1] = scale.y;
    res.data[2][2] = scale.z;
    return res;
}

Mat4 OrthoProjMat4(float left, float right, float bot, float top, float nearClip, float farClip)
{
    Mat4 res = IdentityMat4();
    res.data[0][0] = 2.0f / (right - left);
    res.data[1][1] = 2.0f / (top - bot);
    res.data[2][2] = 1.0f / (nearClip - farClip);
    res.data[3][0] = (left + right) / (left - right);
    res.data[3][1] = (top + bot) / (bot - top);
    res.data[3][2] = nearClip / (nearClip - farClip);
    return res;
}

struct BasicColorShaderData
{
    Mat4 xformMat;
};
CHECK_CBUFFER_ALIGNMENT(BasicColorShaderData);

ID3D11Buffer* CreateDx11ConstantBuffer(UINT dataByteSize, Dx11* dx)
{
    D3D11_BUFFER_DESC cBufferDesc = {
        .ByteWidth = dataByteSize,
        .Usage = D3D11_USAGE_DYNAMIC,
        .BindFlags = D3D11_BIND_CONSTANT_BUFFER,
        .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE
    };

    ID3D11Buffer* cBuffer = nullptr;
    HRESULT res = dx->device->CreateBuffer(
        &cBufferDesc,
        nullptr,
        &cBuffer
    );
    ASSERT(res == S_OK);

    return cBuffer;
}

void UploadDataToConstantBuffer(ID3D11Buffer* cBuffer, void* data, UINT dataByteSize, Dx11* dx)
{
    D3D11_MAPPED_SUBRESOURCE mappedRes = {};
    HRESULT res = dx->context->Map(cBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedRes);
    ASSERT(res == S_OK);
    memcpy(mappedRes.pData, data, dataByteSize);
    dx->context->Unmap(cBuffer, 0);
}

void ResizeDx11Backbuffer(Dx11Backbuffer* backbuffer, int newWidth, int newHeight, Dx11* dx)
{
    dx->context->OMSetRenderTargets(0, 0, 0);
    FreeDx11Backbuffer(backbuffer);
    HRESULT res = dx->swapchain->ResizeBuffers(
        2,
        (UINT)newWidth,
        (UINT)newHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0
    );
    ASSERT(res == S_OK);
    *backbuffer = InitDx11Backbuffer(dx);
}

// GOAL: 
// --------
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// --------
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
    int windowWidth = 1280;
    int windowHeight = 720;

    HWND window = InitWindow(windowWidth, windowHeight, "objviewer");
    if(window == nullptr)
        return -1;

    Dx11 dx = {};
    if(!InitDx11(&dx, windowWidth, windowHeight, window))
        return -1;

    Dx11Backbuffer backbuffer = InitDx11Backbuffer(&dx);
    float clearColor[] = { 0.3f, 0.4f, 0.9f, 1.0f };

    ID3D11RasterizerState* rasterizerState = InitDx11RasterizerState(&dx);

    Vec3 vertices[] = {
        {  0.0f,  0.5f, 0.0f },
        { -0.5f, -0.5f, 0.0f },
        {  0.5f, -0.5f, 0.0f }
    };

    Dx11VertexBuffer vertexBuffer = CreateStaticDx11VertexBuffer(vertices, sizeof(vertices), 3 * sizeof(float), 0, &dx);
    String vsCode = ReadAllTextFromFile("res/basiccolorvs.hlsl");
    ID3DBlob* vsByteCode = CompileShaderCode(ShaderType::Vertex, vsCode);
    ID3D11VertexShader* vs = nullptr;
    dx.device->CreateVertexShader(
        vsByteCode->GetBufferPointer(), 
        vsByteCode->GetBufferSize(), 
        nullptr, 
        &vs
    );
    
    String psCode = ReadAllTextFromFile("res/basiccolorps.hlsl");
    ID3DBlob* psByteCode = CompileShaderCode(ShaderType::Pixel, psCode);
    ID3D11PixelShader* ps = nullptr;
    dx.device->CreatePixelShader(
        psByteCode->GetBufferPointer(), 
        psByteCode->GetBufferSize(), 
        nullptr, 
        &ps
    );

    ID3D11InputLayout* inputLayout = CreateDx11InputLayout(&dx, vsByteCode);

    Vec3 position = { 200.0f, 200.0f, -1.0f };
    Vec3 scale = { 100.0f, 100.0f, 1.0f };
    BasicColorShaderData shaderData = {};
    Mat4 modelMat = TranslateMat4(position) * ScaleMat4(scale);

    ID3D11Buffer* basicColorCBuffer = CreateDx11ConstantBuffer(sizeof(shaderData), &dx);
    D3D11_VIEWPORT viewport = GetDx11ViewportForWindow(window);

    SetupRawMouseInput();

    Input input = {
        .moveForward  = { .key = Vkey::Z },
        .moveBackward = { .key = Vkey::S },
        .moveLeft     = { .key = Vkey::Q },
        .moveRight    = { .key = Vkey::D },
        .moveDown     = { .key = Vkey::A },
        .moveUp       = { .key = Vkey::Space }
    };

    ShowWindow(window);
    while(true)
    {
        ResetInputKeyTransitions(&input);
        ResetRelativeInputMouseData(&input);
        GetMousePositionInWindow(&input, window);
        if(!ProcessWindowEvents(&input))
            break;

        if(UpdateDx11ViewportForWindow(&viewport, window))
            ResizeDx11Backbuffer(&backbuffer, (int)viewport.Width, (int)viewport.Height, &dx);

        Mat4 orthoProjMat = OrthoProjMat4(0.0f, viewport.Width, 0.0f, viewport.Height, 0.1f, 100.0f);
        shaderData.xformMat = orthoProjMat * modelMat;

        if(input.moveForward.isKeyDown)
        {
            printf("moving forward\n");
        }

        if(input.moveBackward.isKeyDown)
        {
            printf("moving backward\n");
        }

        if(input.moveUp.keyDownTransitionCount > 0)
        {
            printf("pressed space\n");
        }
        printf("mouse pos: x = %d, y = %d | move: x = %d, y = %d\n", input.mousePosX, input.mousePosY, input.mouseMoveX, input.mouseMoveY);

        dx.context->RSSetViewports(1, &viewport);
        dx.context->RSSetState(rasterizerState);
        dx.context->OMSetRenderTargets(1, &backbuffer.view, nullptr);
        dx.context->ClearRenderTargetView(backbuffer.view, clearColor);

        dx.context->IASetVertexBuffers(0, 1, &vertexBuffer.buffer, &vertexBuffer.stride, &vertexBuffer.byteOffset);
        dx.context->IASetInputLayout(inputLayout);
        dx.context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        dx.context->VSSetShader(vs, nullptr, 0);
        dx.context->PSSetShader(ps, nullptr, 0);

        UploadDataToConstantBuffer(basicColorCBuffer, &shaderData, sizeof(shaderData), &dx);
        dx.context->VSSetConstantBuffers(0, 1, &basicColorCBuffer);

        // TODO: put vertex count somewhere else?
        dx.context->Draw(ARRAY_LEN(vertices), 0);

        dx.swapchain->Present(1, 0);
    }    

    basicColorCBuffer->Release();

    inputLayout->Release();

    ps->Release();
    psByteCode->Release();
    FreeString(&psCode);

    vs->Release();
    vsByteCode->Release();
    FreeString(&vsCode);

    FreeDx11VertexBuffer(&vertexBuffer);
    rasterizerState->Release();
    FreeDx11Backbuffer(&backbuffer);
    FreeDx11(&dx);
    DestroyWindow(window);
    return 0;
}