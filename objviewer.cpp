#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

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
    Space = VK_SPACE,
    F1 = VK_F1
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
    Keybind devToggle;
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
    ResetKeyTransitions(&input->devToggle);
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
            HandleKeyUpForBind(&input->devToggle, &event);
        }
        else if(event.message == WM_KEYDOWN)
        {
            HandleKeyDownForBind(&input->moveForward, &event);
            HandleKeyDownForBind(&input->moveBackward, &event);
            HandleKeyDownForBind(&input->moveLeft, &event);
            HandleKeyDownForBind(&input->moveRight, &event);
            HandleKeyDownForBind(&input->moveDown, &event);
            HandleKeyDownForBind(&input->moveUp, &event);
            HandleKeyDownForBind(&input->devToggle, &event);
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

// pi rad = 180*
constexpr double pi = 3.141592653589793238462643383279502;
constexpr double radiansToDegreesFactor = 180.0 / pi;
constexpr double degreesToRadiansFactor = pi / 180.0f;

float toDegrees(float radians)
{
    return radians * (float)radiansToDegreesFactor;
}

float toRadians(float degrees)
{
    return degrees * (float)degreesToRadiansFactor;
}

float Clamp(float min, float max, float value)
{
    if(value < min)
        return min;
    else if(value > max)
        return max;
    else
        return value;
}

struct Vec2
{
    float x;
    float y;
};

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

float Dot(const Vec3& one, const Vec3& other)
{
    return (one.x * other.x) + (one.y * other.y) + (one.z * other.z);
}

float Len(const Vec3& vec)
{
    return sqrtf(Dot(vec, vec));
}

Vec3 Normalize(const Vec3& vec)
{
    return vec / Len(vec);
}

Vec3 Cross(const Vec3& one, const Vec3& other)
{
    return {
        .x = one.y * other.z - one.z * other.y,
        .y = one.z * other.x - one.x * other.z,
        .z = one.x * other.y - one.y * other.x 
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
    // based on D3DXMatrixOrthoOffCenterRH
    Mat4 res = IdentityMat4();
    res.data[0][0] = 2.0f / (right - left);
    res.data[1][1] = 2.0f / (top - bot);
    res.data[2][2] = 1.0f / (nearClip - farClip);
    res.data[3][0] = (left + right) / (left - right);
    res.data[3][1] = (top + bot) / (bot - top);
    res.data[3][2] = nearClip / (nearClip - farClip);
    return res;
}

Mat4 PerspectiveProjMat4(float fovY, float width, float height, float nearClip, float farClip)
{
    // based on D3DXMatrixPerspectiveFovRH
    float aspectRatio = width / height;
    // 1 / tan = cotangent
    float yScale = 1.0f / tanf(fovY / 2.0f);
    float xScale = yScale / aspectRatio;

    Mat4 res = {};
    res.data[0][0] = xScale;
    res.data[1][1] = yScale;
    res.data[2][2] = farClip / (nearClip - farClip);
    res.data[2][3] = -1;
    res.data[3][2] = nearClip * farClip / (nearClip - farClip);
    return res;
}

Mat4 LookatMat4(Vec3 eye, Vec3 at, Vec3 up)
{
    // based on LearnOpenGL/Getting started/Camera
    Vec3 zAxis = Normalize(eye - at);
    Vec3 xAxis = Normalize(Cross(up, zAxis));
    Vec3 yAxis = Cross(zAxis, xAxis);

    Mat4 res = {};
    res.data[0][0] = xAxis.x;
    res.data[0][1] = yAxis.x;
    res.data[0][2] = zAxis.x;
    
    res.data[1][0] = xAxis.y;
    res.data[1][1] = yAxis.y;
    res.data[1][2] = zAxis.y;
    
    res.data[2][0] = xAxis.z;
    res.data[2][1] = yAxis.z;
    res.data[2][2] = zAxis.z;

    res.data[3][0] = -Dot(xAxis, eye);
    res.data[3][1] = -Dot(yAxis, eye);
    res.data[3][2] = -Dot(zAxis, eye);
    res.data[3][3] = 1.0f;
    return res;
}

Mat4 RotateEulerYMat4(float angleRads)
{
    float cosine = cosf(angleRads);
    float sine = sinf(angleRads);
    
    Mat4 res = IdentityMat4();
    res.data[0][0] =  cosine;
    res.data[0][2] =  -sine;
    res.data[0][2] =  sine;
    res.data[2][2] =  cosine;
    return res;
}

Mat4 RotateEulerXMat4(float angleRads)
{
    float cosine = cosf(angleRads);
    float sine = sinf(angleRads);
    
    Mat4 res = IdentityMat4();
    res.data[1][1] =  cosine;
    res.data[1][2] =  sine;
    res.data[2][1] =  -sine;
    res.data[2][2] =  cosine;
    return res;
}

Mat4 RotateEulerZMat4(float angleRads)
{
    float cosine = cosf(angleRads);
    float sine = sinf(angleRads);
    
    Mat4 res = IdentityMat4();
    res.data[0][0] =  cosine;
    res.data[0][1] =  sine;
    res.data[1][0] =  -sine;
    res.data[1][1] =  cosine;
    return res;
}

struct BasicColorShaderData
{
    Mat4 xformMat;
    Vec4 color;
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

void ResizeDx11Backbuffer(Dx11Backbuffer* backbuffer, UINT newWidth, UINT newHeight, Dx11* dx)
{
    dx->context->OMSetRenderTargets(0, 0, 0);
    FreeDx11Backbuffer(backbuffer);
    HRESULT res = dx->swapchain->ResizeBuffers(
        2,
        newWidth,
        newHeight,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0
    );
    ASSERT(res == S_OK);
    *backbuffer = InitDx11Backbuffer(dx);
}

uint64_t GetTicks()
{
    LARGE_INTEGER counter = {};
    QueryPerformanceCounter(&counter);
    return counter.QuadPart;
}

uint64_t GetTickFrequency()
{
    LARGE_INTEGER freq = {};
    QueryPerformanceFrequency(&freq);
    return freq.QuadPart;
}

double TicksToSeconds(uint64_t ticks)
{
    static uint64_t freq = GetTickFrequency();
    return (double)ticks / (double)freq;
}

struct Timer
{
    uint64_t startTicks;
    uint64_t lastTicks;
    double elapsedTime;
    double deltaTime;
};

Timer CreateTimer()
{
    uint64_t currentTicks = GetTicks();
    return {
        .startTicks = currentTicks,
        .lastTicks = currentTicks
    };
}

void UpdateTimer(Timer* timer)
{
    uint64_t currentTicks = GetTicks();
    uint64_t deltaTicks = currentTicks - timer->lastTicks;
    uint64_t elapsedTicks = currentTicks - timer->startTicks;

    timer->deltaTime = TicksToSeconds(deltaTicks);
    timer->elapsedTime = TicksToSeconds(elapsedTicks);
    timer->lastTicks = currentTicks;
}

struct FpsCam
{
    Vec3 position;
    Vec3 front;
    Vec3 up;
    float moveSpeed;
    float lookSpeed;
    float pitch;
    float yaw;
    bool isControlOn;
};

FpsCam CreateFpsCam(Vec3 position, float moveSpeed, float lookSpeed)
{
    return {
        .position = position,
        .front = { 0.0f, 0.0f, -1.0f },
        .up = { 0.0f, 1.0f, 0.0f },
        .moveSpeed = moveSpeed,
        .lookSpeed = lookSpeed,
        .yaw = -89.0f // // 0 would start with the cam looking to the right
    };
}

void UpdateFpsCam(FpsCam* cam, Input* input, float deltaTime)
{
    if(input->moveForward.isKeyDown)
        cam->position = cam->position + (cam->front * cam->moveSpeed * deltaTime);

    if(input->moveBackward.isKeyDown)
        cam->position = cam->position - (cam->front * cam->moveSpeed * deltaTime);

    if(input->moveLeft.isKeyDown)
        cam->position = cam->position - (Normalize(Cross(cam->front, cam->up)) * cam->moveSpeed * deltaTime);

    if(input->moveRight.isKeyDown)
        cam->position = cam->position + (Normalize(Cross(cam->front, cam->up)) * cam->moveSpeed * deltaTime);

    if(input->moveUp.isKeyDown)
        cam->position.y += (cam->moveSpeed * deltaTime);
    
    if(input->moveDown.isKeyDown)
        cam->position.y -= (cam->moveSpeed * deltaTime);

    if(input->mouseMoveX != 0)
        cam->yaw += (input->mouseMoveX * cam->lookSpeed * deltaTime);

    if(input->mouseMoveY != 0)
        cam->pitch -= (input->mouseMoveY * cam->lookSpeed * deltaTime);

    cam->pitch = Clamp(-89.0f, 89.0f, cam->pitch);

    Vec3 dir = {};
    dir.x = cosf(toRadians(cam->yaw)) * cosf(toRadians(cam->pitch));
    dir.y = sin(toRadians(cam->pitch));
    dir.z = sin(toRadians(cam->yaw)) * cosf(toRadians(cam->pitch));
    cam->front = Normalize(dir);
}

Mat4 CreateViewMatForFpsCam(FpsCam* cam)
{
    return LookatMat4(cam->position, cam->position + cam->front, cam->up);
}

void ToggleCamControl(FpsCam* cam, bool isOn)
{
    cam->isControlOn = isOn;
    if(isOn)
        ShowCursor(FALSE);
    else
        ShowCursor(TRUE);
}

void TrapCursorInWindow(HWND window, int windowWidth, int windowHeight)
{
    POINT mouseTrapPoint = {
        .x = windowWidth / 2,
        .y = windowHeight / 2
    };
    ClientToScreen(window, &mouseTrapPoint);
    SetCursorPos(mouseTrapPoint.x, mouseTrapPoint.y);
}

void DrawLine(Vec3 position, Vec3 scale, float yRotation, 
    const Mat4& projMat, const Mat4 viewMat, ID3D11Buffer* cBuffer, Dx11* dx)
{
    BasicColorShaderData shaderData = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } };
    Mat4 modelMat = TranslateMat4(position) * ScaleMat4(scale) * RotateEulerYMat4(toRadians(yRotation));
    shaderData.xformMat = projMat * viewMat * modelMat;
    UploadDataToConstantBuffer(cBuffer, &shaderData, sizeof(shaderData), dx);
    dx->context->VSSetConstantBuffers(0, 1, &cBuffer);
    dx->context->Draw(2, 0);
}

void DrawLineGrid(int xSquaresHalf, int zSquaresHalf, Dx11* dx, Dx11VertexBuffer* vertexBuffer, 
    ID3D11InputLayout* inputLayout, ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11Buffer* cBuffer,
    const Mat4& projMat, const Mat4& viewMat)
{
    dx->context->IASetVertexBuffers(0, 1, &vertexBuffer->buffer, &vertexBuffer->stride, &vertexBuffer->byteOffset);
    dx->context->IASetInputLayout(inputLayout);
    dx->context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    dx->context->VSSetShader(vs, nullptr, 0);
    dx->context->PSSetShader(ps, nullptr, 0);

    int nrOfXLines = 1 + (zSquaresHalf * 2);
    int nrOfZLines = 1 + (xSquaresHalf * 2);

    // draw lines parallel to x-axis
    for(int i = 0; i < nrOfXLines; i++) {
        Vec3 position = {};
        position.z = (float)i;
        position.z -= (float)((nrOfXLines - 1) / 2);
        float xLineLen = (float)(nrOfZLines - 1);
        DrawLine(position, { xLineLen, 1.0f, 0.0f }, 0.0f, projMat, viewMat, cBuffer, dx);
    }

    // draw lines parallel to z-axis
    for(int i = 0; i < nrOfZLines; i++) {
        Vec3 position = {};
        position.x = (float)i;
        position.x -= (float)((nrOfZLines - 1) / 2);
        float zLineLen = (float)(nrOfXLines - 1);
        DrawLine(position, { 1.0f, 1.0f, zLineLen }, 90.0f, projMat, viewMat, cBuffer, dx);
    }

}

void DrawTriangle(Dx11* dx, Dx11VertexBuffer* vertexBuffer, ID3D11InputLayout* inputLayout, 
    ID3D11VertexShader* vs, ID3D11PixelShader* ps, ID3D11Buffer* cBuffer, const Mat4& projMat, const Mat4& viewMat)
{
    Vec3 position = { 0.0f, 0.0f, -0.5f };
    Vec3 scale = { 1.0f, 1.0f, 1.0f };
    Mat4 modelMat = TranslateMat4(position) * ScaleMat4(scale);

    BasicColorShaderData shaderData = {
        .xformMat = projMat * viewMat * modelMat,
        .color = { 0.0f, 0.9f, 0.1f, 1.0f }
    };
    dx->context->IASetVertexBuffers(0, 1, &vertexBuffer->buffer, &vertexBuffer->stride, &vertexBuffer->byteOffset);
    dx->context->IASetInputLayout(inputLayout);
    dx->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dx->context->VSSetShader(vs, nullptr, 0);
    dx->context->PSSetShader(ps, nullptr, 0);

    UploadDataToConstantBuffer(cBuffer, &shaderData, sizeof(shaderData), dx);
    dx->context->VSSetConstantBuffers(0, 1, &cBuffer);
    dx->context->Draw(3, 0);
}

struct Dx11DepthStencilBuffer
{
    ID3D11Texture2D* buffer;
    ID3D11DepthStencilView* view;
};

Dx11DepthStencilBuffer CreateDx11DepthStencilBuffer(UINT width, UINT height, Dx11* dx)
{
    D3D11_TEXTURE2D_DESC dsBufferDesc = {
        .Width = width,
        .Height = height,
        .MipLevels = 1,
        .ArraySize = 1,
        .Format = DXGI_FORMAT_D24_UNORM_S8_UINT,
        .SampleDesc = {
            .Count = 1,
            .Quality = 0
        },
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_DEPTH_STENCIL
    };

    ID3D11Texture2D* dsBuffer = nullptr;
    HRESULT res = dx->device->CreateTexture2D(&dsBufferDesc, nullptr, &dsBuffer);
    ASSERT(res == S_OK);

    ID3D11DepthStencilView* dsBufferView = nullptr;
    res = dx->device->CreateDepthStencilView(dsBuffer, nullptr, &dsBufferView);
    ASSERT(res == S_OK);

    return {
        .buffer = dsBuffer,
        .view = dsBufferView
    };
}

void FreeDx11DepthStencilBuffer(Dx11DepthStencilBuffer* dsBuffer)
{
    dsBuffer->buffer->Release();
    dsBuffer->view->Release();
    *dsBuffer = {};
}

void ResizeDx11DepthStencilBuffer(Dx11DepthStencilBuffer* dsBuffer, UINT newWidth, UINT newHeight, Dx11* dx)
{
    dx->context->OMSetRenderTargets(0, 0, 0);
    FreeDx11DepthStencilBuffer(dsBuffer);
    *dsBuffer = CreateDx11DepthStencilBuffer(newWidth, newHeight, dx);
}

ID3D11DepthStencilState* CreateDx11DepthStencilState(Dx11* dx)
{
    D3D11_DEPTH_STENCIL_DESC stateDesc = {
        .DepthEnable = TRUE,
        .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
        .DepthFunc = D3D11_COMPARISON_LESS,
        .StencilEnable = FALSE,
        .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
        .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK
    };

    ID3D11DepthStencilState* dsState = nullptr;
    HRESULT res = dx->device->CreateDepthStencilState(&stateDesc, &dsState);
    ASSERT(res == S_OK);
    return dsState;
}

struct StringView
{
    const char* start;
    size_t len;
};

struct StringReader
{
    StringView string;
    size_t pos;
};

StringView ReadLine(StringReader* reader)
{
    if(reader->pos == reader->string.len)
        return {};

    const char* start = reader->string.start + reader->pos;
    char c = 0;
    size_t len = 0;
    while(reader->pos + len < reader->string.len && (c = start[len]) != '\0') {
        len++;
        if(c == '\n' || c == '\r\n')
            break;
    }
    reader->pos += len;
    
    return {
        .start = start,
        .len = len
    };
}

enum class ObjLineType
{
    Comment,
    Vertex,
    TexCoord,
    Normal,
    Face,
    Unknown
};

ObjLineType GetObjLineType(StringView line)
{
    char c0 = line.start[0];
    char c1 = line.start[1];

    switch(c0) {
        case '#':
        {
            return ObjLineType::Comment;
        }
        case 'v':
        {
            if(c1 == ' ')
                return ObjLineType::Vertex;
            else if(c1 == 't')
                return ObjLineType::TexCoord;
            else
                return ObjLineType::Normal;
        }
        case 'f':
        {
            return ObjLineType::Face;
        }
        default:
        {
            return ObjLineType::Unknown;
        }
    }
}

struct ObjStats
{
    int positionCount;
    int texCoordCount;
    int normalCount;
    int indicesPerVertexCount;
    int faceCount;
};

StringView SkipObjLineStart(StringView line)
{
    size_t offset = 0;
    while(line.start[offset] == 'v' || line.start[offset] == 't' || line.start[offset] == 'n' || 
        line.start[offset] == 'f' || line.start[offset] == ' ')
    {
        offset++;
    }
    
    return {
        .start = line.start + offset,
        .len = line.len - offset
    };
}

int GetIndicesPerVertexCountFromObjLine(StringView line)
{
    line = SkipObjLineStart(line);
    int count = 1;
    size_t i = 0;
    char c = 0;
    while((c = line.start[i++]) != ' ') {
        if(c == '/')
            count++;
    }
    return count;
}

ObjStats GetObjStats(String objText)
{
    StringReader reader = { .string = { .start = objText.data, .len = objText.len } };
    StringView line = {};
    ObjStats stats = {};

    while((line = ReadLine(&reader)).len > 0) {
        ObjLineType lineType = GetObjLineType(line);
        switch(lineType) {
            case ObjLineType::Vertex:
                stats.positionCount++;
                break;
            case ObjLineType::TexCoord:
                stats.texCoordCount++;
                break;
            case ObjLineType::Normal:
                stats.normalCount++;
                break;
            case ObjLineType::Face:
                if(stats.faceCount == 0)
                    stats.indicesPerVertexCount = GetIndicesPerVertexCountFromObjLine(line);
                stats.faceCount++;
                break;
        }
    }

    return stats;
}

struct ObjData
{
    Vec3* positions;
    Vec2* texCoords;
    Vec3* normals;
    int* indices;
};

ObjData AllocateObjData(ObjStats stats)
{
    ObjData data = {
        .positions = (Vec3*)calloc(1, stats.positionCount * sizeof(Vec3)),
        .indices = (int*)calloc(1, stats.faceCount * 3 * stats.indicesPerVertexCount)
    };

    ASSERT(data.positions != nullptr);
    ASSERT(data.indices != nullptr);

    if(stats.texCoordCount > 0) {
        data.texCoords = (Vec2*)calloc(1, stats.texCoordCount * sizeof(Vec2));
        ASSERT(data.texCoords != nullptr);
    }
    if(stats.normalCount > 0) {
        data.normals = (Vec3*)calloc(1, stats.normalCount * sizeof(Vec3));
        ASSERT(data.normals != nullptr);
    }

    return data;
}

Vec3 GetVec3FromObjLine(StringView line)
{
    line = SkipObjLineStart(line);

    Vec3 vec = {};
    char* parseAt = (char*)line.start;
    vec.x = strtof(parseAt, &parseAt);
    vec.y = strtof(parseAt, &parseAt);
    vec.z = strtof(parseAt, &parseAt);

    return vec;
}

Vec2 GetVec2FromObjLine(StringView line)
{
    line = SkipObjLineStart(line);

    Vec2 vec = {};
    char* parseAt = (char*)line.start;
    vec.x = strtof(parseAt, &parseAt);
    vec.y = strtof(parseAt, &parseAt);

    return vec;
}

void GetIndicesFromObjLine(StringView line, int* indices, int* indicesWriteIndex)
{
    line = SkipObjLineStart(line);

    char* parseAt = (char*)line.start;
    while(parseAt[0] != '\n' && parseAt[0] != '\r\n') {
        indices[(*indicesWriteIndex)++] = strtol(parseAt, &parseAt, 10);
        while(parseAt[0] == '/' || parseAt[0] == ' ')
            parseAt++;
    }
}

void LoadModelFromObjFile(const char* filename)
{
    String objText = ReadAllTextFromFile(filename);

    StringReader reader = { .string = { .start = objText.data, .len = objText.len } };
    StringView line = {};

    ObjStats stats = GetObjStats(objText);
    ObjData data = AllocateObjData(stats);

    int vertexWriteIndex = 0;
    int texCoordWriteIndex = 0;
    int normalWriteIndex = 0;
    int indicesWriteIndex = 0;

    while((line = ReadLine(&reader)).len > 0) {
        ObjLineType lineType = GetObjLineType(line);
        switch(lineType) {
            case ObjLineType::Vertex:
                data.positions[vertexWriteIndex++] = GetVec3FromObjLine(line);
                break;
            case ObjLineType::TexCoord:
                data.texCoords[texCoordWriteIndex++] = GetVec2FromObjLine(line);
                break;
            case ObjLineType::Normal:
                data.normals[normalWriteIndex++] = GetVec3FromObjLine(line);
                break;
            case ObjLineType::Face:
                GetIndicesFromObjLine(line, data.indices, &indicesWriteIndex);
                break;
        }
    }

    // TODO: prep extracted data for rendering using the indices
    // TODO: free unneeded obj data afterwards

    FreeString(&objText);

    // WaveFront Obj file spec important bits:
    // =============================================
    //
    // ref: https://en.wikipedia.org/wiki/Wavefront_.obj_file
    //
    // - line starts with #        = comment
    // - line starts with v        = vertex
    //          -> followed by x,y,z with optional w
    // - line starts with vn       = vertex normal
    // - line starts with vt       = vertex texture coords
    //          -> followed by u,v with optional w
    // - line starts with f        = polygon face
    //          -> defined by a list of indexes into previous v, vt, vn data; in that order
    //          -> index starts at 1
    //          -> index can be negative, indexing must be done from the back
    //          -> example: v/vt/vn = 1/12/-2
    // =============================================
}

// GOAL: 
// =============================================
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// =============================================
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
    D3D11_VIEWPORT viewport = GetDx11ViewportForWindow(window);
    Dx11DepthStencilBuffer dsBuffer = CreateDx11DepthStencilBuffer((UINT)viewport.Width, (UINT)viewport.Height, &dx);
    ID3D11DepthStencilState* dsState = CreateDx11DepthStencilState(&dx);
    float clearColor[] = { 0.3f, 0.4f, 0.9f, 1.0f };

    ID3D11RasterizerState* rasterizerState = InitDx11RasterizerState(&dx);

    Vec3 vertices[] = {
        {  0.0f,  0.5f, 0.0f },
        { -0.5f, -0.5f, 0.0f },
        {  0.5f, -0.5f, 0.0f }
    };
    Dx11VertexBuffer triangleVertexBuffer = CreateStaticDx11VertexBuffer(vertices, sizeof(vertices), 3 * sizeof(float), 0, &dx);

    Vec3 lineVertices[] = {
        { -0.5f, 0.0f, 0.0f },
        {  0.5f, 0.0f, 0.0f }
    };
    Dx11VertexBuffer lineVertexBuffer = CreateStaticDx11VertexBuffer(lineVertices, sizeof(lineVertices), 3 * sizeof(float), 0, &dx);

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

    FpsCam cam = CreateFpsCam({ 0.0f, 0.0f, 2.0f }, 5.0f, 6.0f);
    ToggleCamControl(&cam, true);

    ID3D11Buffer* basicColorCBuffer = CreateDx11ConstantBuffer(sizeof(BasicColorShaderData), &dx);

    SetupRawMouseInput();

    Input input = {
        .moveForward  = { .key = Vkey::Z },
        .moveBackward = { .key = Vkey::S },
        .moveLeft     = { .key = Vkey::Q },
        .moveRight    = { .key = Vkey::D },
        .moveDown     = { .key = Vkey::A },
        .moveUp       = { .key = Vkey::Space },
        .devToggle    = { .key = Vkey::F1 }
    };

    LoadModelFromObjFile("res/teapot.obj");

    Timer timer = CreateTimer();

    ShowWindow(window);
    while(true)
    {
        ResetInputKeyTransitions(&input);
        ResetRelativeInputMouseData(&input);
        GetMousePositionInWindow(&input, window);
        if(!ProcessWindowEvents(&input))
            break;

        if(UpdateDx11ViewportForWindow(&viewport, window))
        {
            ResizeDx11Backbuffer(&backbuffer, (UINT)viewport.Width, (UINT)viewport.Height, &dx);
            ResizeDx11DepthStencilBuffer(&dsBuffer, (UINT)viewport.Width, (UINT)viewport.Height, &dx);
        }

        if(input.devToggle.keyDownTransitionCount)
            ToggleCamControl(&cam, !cam.isControlOn);

        if(cam.isControlOn)
        {
            TrapCursorInWindow(window, (int)viewport.Width, (int)viewport.Height);
            UpdateFpsCam(&cam, &input, (float)timer.deltaTime);
        }

        Mat4 perspectiveProjMat = PerspectiveProjMat4(toRadians(45.0f), viewport.Width, viewport.Height, 0.1f, 100.0f);
        Mat4 viewMat = CreateViewMatForFpsCam(&cam);

        dx.context->RSSetViewports(1, &viewport);
        dx.context->RSSetState(rasterizerState);
        dx.context->OMSetRenderTargets(1, &backbuffer.view, dsBuffer.view);
        dx.context->OMSetDepthStencilState(dsState, 1);
        dx.context->ClearRenderTargetView(backbuffer.view, clearColor);
        dx.context->ClearDepthStencilView(dsBuffer.view, D3D11_CLEAR_DEPTH, 1.0f, 0.0f);

        DrawTriangle(&dx, &triangleVertexBuffer, inputLayout, vs, ps, basicColorCBuffer, perspectiveProjMat, viewMat);

        DrawLineGrid(4, 4, &dx, &lineVertexBuffer, inputLayout, vs, ps, 
            basicColorCBuffer, perspectiveProjMat, viewMat);

        dx.swapchain->Present(1, 0);
        UpdateTimer(&timer);
    }

    basicColorCBuffer->Release();
    inputLayout->Release();

    ps->Release();
    psByteCode->Release();
    FreeString(&psCode);

    vs->Release();
    vsByteCode->Release();
    FreeString(&vsCode);

    FreeDx11VertexBuffer(&lineVertexBuffer);
    FreeDx11VertexBuffer(&triangleVertexBuffer);
    rasterizerState->Release();
    dsState->Release();
    FreeDx11DepthStencilBuffer(&dsBuffer);
    FreeDx11Backbuffer(&backbuffer);
    FreeDx11(&dx);
    DestroyWindow(window);
    return 0;
}