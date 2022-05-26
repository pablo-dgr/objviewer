#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#if DEBUG
#define ASSERT(x) if(x) {} else { __debugbreak(); }
#else
#define ASSERT(x)
#endif

#define ARRAY_LEN(x) sizeof(x) / sizeof((x)[0])

#define CHECK_CBUFFER_ALIGNMENT(x) static_assert(sizeof(x) % 16 == 0, "constant buffer data must be 16-byte aligned")

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

struct Vec4
{
    float x;
    float y;
    float z;
    float w;
};

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

// TODO: remove old version when not needed anymore
ID3D11Buffer* CreateStaticDx11VertexBuffer(const Dx11& dx, void* data, size_t byteSize)
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
    HRESULT res = dx.device->CreateBuffer(&vertexBufferDesc, &vertexBufData, &vertexBuffer);
    ASSERT(res == S_OK);

    return vertexBuffer;
}

enum class InputElType
{
    Position,
    TexCoord,
    Normal,
    Matrix
};

D3D11_INPUT_ELEMENT_DESC CreateDx11InputElDesc(InputElType type, UINT typeIndex, UINT slot, UINT byteOffset, 
    bool instanced, UINT instanceStepRate)
{
    auto inputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
    if(instanced)
        inputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;

    switch(type) {
        case InputElType::Position:
        {
            return {
                .SemanticName = "POSITION",
                .SemanticIndex = typeIndex,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = slot,
                .AlignedByteOffset = byteOffset,
                .InputSlotClass = inputSlotClass,
                .InstanceDataStepRate = instanceStepRate
            };
        }
        case InputElType::TexCoord:
        {
            return {
                .SemanticName = "TEXCOORD",
                .SemanticIndex = typeIndex,
                .Format = DXGI_FORMAT_R32G32_FLOAT,
                .InputSlot = slot,
                .AlignedByteOffset = byteOffset,
                .InputSlotClass = inputSlotClass,
                .InstanceDataStepRate = instanceStepRate
            };
        }
        case InputElType::Normal:
        {
            return {
                .SemanticName = "NORMAL",
                .SemanticIndex = typeIndex,
                .Format = DXGI_FORMAT_R32G32B32_FLOAT,
                .InputSlot = slot,
                .AlignedByteOffset = byteOffset,
                .InputSlotClass = inputSlotClass,
                .InstanceDataStepRate = instanceStepRate
            };
        }
        case InputElType::Matrix:
        {
            return {
                .SemanticName = "MATRIX",
                .SemanticIndex = typeIndex,
                .Format = DXGI_FORMAT_R32G32B32A32_FLOAT,
                .InputSlot = slot,
                .AlignedByteOffset = byteOffset,
                .InputSlotClass = inputSlotClass,
                .InstanceDataStepRate = instanceStepRate
            };
        }
        default:
            ASSERT(false);
            return {};
    }
}

ID3D11InputLayout* CreateBasicColorDx11InputLayout(Dx11* dx, ID3DBlob* vsByteCode)
{
    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        CreateDx11InputElDesc(InputElType::Position, 0, 0, 0, false, 0)
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

ID3D11InputLayout* CreatePhongDx11InputLayout(Dx11* dx, ID3DBlob* vsByteCode)
{
    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        CreateDx11InputElDesc(InputElType::Position, 0, 0, 0, false, 0),
        CreateDx11InputElDesc(InputElType::Normal, 0, 1, 0, false, 0)
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

ID3D11InputLayout* CreateTextDx11InputLayout(Dx11* dx, ID3DBlob* vsByteCode)
{
    UINT instanceStepRate = 1;

    D3D11_INPUT_ELEMENT_DESC inputElements[] = {
        CreateDx11InputElDesc(InputElType::Position, 0, 0, 0, false, 0),
        
        CreateDx11InputElDesc(InputElType::Matrix, 0, 1, 0, true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::Matrix, 1, 1, 1 * sizeof(Vec4), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::Matrix, 2, 1, 2 * sizeof(Vec4), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::Matrix, 3, 1, 3 * sizeof(Vec4), true, instanceStepRate),

        CreateDx11InputElDesc(InputElType::TexCoord, 0, 1, 0, true, 1),
        CreateDx11InputElDesc(InputElType::TexCoord, 1, 1, (4 * sizeof(Vec4)) + (1 * sizeof(Vec2)), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::TexCoord, 2, 1, (4 * sizeof(Vec4)) + (2 * sizeof(Vec2)), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::TexCoord, 3, 1, (4 * sizeof(Vec4)) + (3 * sizeof(Vec2)), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::TexCoord, 4, 1, (4 * sizeof(Vec4)) + (4 * sizeof(Vec2)), true, instanceStepRate),
        CreateDx11InputElDesc(InputElType::TexCoord, 5, 1, (4 * sizeof(Vec4)) + (5 * sizeof(Vec2)), true, instanceStepRate),
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

struct ByteBuffer
{
    unsigned char* data;
    size_t len;
};

ByteBuffer ReadAllBytesFromFile(const char* filename, size_t extraBytesToAllocate)
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
    void* buffer = calloc(1, fileSize + extraBytesToAllocate);
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
        .data = (unsigned char*)buffer,
        .len = fileSize + extraBytesToAllocate
    };
}

String ReadAllTextFromFile(const char* filename)
{
    ByteBuffer bytes = ReadAllBytesFromFile(filename, 1);
    return {
        .data = (char*)bytes.data,
        .len = bytes.len - 1
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

Mat4 operator * (const Mat4& mat, float scalar)
{
    Mat4 res = {};

    for(int x = 0; x < 4; x++) {
        for(int y = 0; y < 4; y++) {
            res.data[y][x] = mat.data[y][x] * scalar;
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
    res.data[2][0] =  sine;
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

struct Mat2
{
    float data[2][2];
};

struct Mat3
{
    float data[3][3];
};

float Determ(const Mat2& mat)
{
    return 
        (mat.data[0][0] * mat.data[1][1]) - 
        (mat.data[1][0] * mat.data[0][1]);
}

float Determ(const Mat3& mat)
{
    float s1 = mat.data[0][0];
    Mat2 m1 = {
        .data = {
            { mat.data[1][1], mat.data[1][2] },
            { mat.data[2][1], mat.data[2][2] },
        }
    };
    float s2 = -mat.data[0][1];
    Mat2 m2 = {
        .data = {
            { mat.data[1][0], mat.data[1][2] },
            { mat.data[2][0], mat.data[2][2] },
        }
    };
    float s3 = mat.data[0][2];
    Mat2 m3 = {
        .data = {
            { mat.data[1][0], mat.data[1][1] },
            { mat.data[2][0], mat.data[2][1] },
        }
    };

    return (s1 * Determ(m1)) + (s2 * Determ(m2)) + (s3 * Determ(m3));
}

float Determ(const Mat4& mat)
{
    float s1 = mat.data[0][0];
    Mat3 m1 = {
        .data = {
            { mat.data[1][1], mat.data[1][2], mat.data[1][3] },
            { mat.data[2][1], mat.data[2][2], mat.data[2][3] },
            { mat.data[3][1], mat.data[3][2], mat.data[3][3] }
        }
    };
    float s2 = -mat.data[0][1];
    Mat3 m2 = {
        .data = {
            { mat.data[1][0], mat.data[1][2], mat.data[1][3] },
            { mat.data[2][0], mat.data[2][2], mat.data[2][3] },
            { mat.data[3][0], mat.data[3][2], mat.data[3][3] }
        }
    };
    float s3 = mat.data[0][2];
    Mat3 m3 = {
        .data = {
            { mat.data[1][0], mat.data[1][1], mat.data[1][3] },
            { mat.data[2][0], mat.data[2][1], mat.data[2][3] },
            { mat.data[3][0], mat.data[3][1], mat.data[3][3] }
        }
    };
    float s4 = -mat.data[0][3];
    Mat3 m4 = {
        .data = {
            { mat.data[1][0], mat.data[1][1], mat.data[1][2] },
            { mat.data[2][0], mat.data[2][1], mat.data[2][2] },
            { mat.data[3][0], mat.data[3][1], mat.data[3][2] }
        }
    };

    return (s1 * Determ(m1)) + (s2 * Determ(m2)) + (s3 * Determ(m3)) + (s4 * Determ(m4));
}

Mat3 GetSubMat(const Mat4& mat, int exceptX, int exceptY)
{
    Mat3 res = {};
    
    int writeX = 0;
    int writeY = 0;

    for(int y = 0; y < 4; y++) {
        if(y == exceptY)
            continue;
        for(int x = 0; x < 4; x++) {
            if(x == exceptX)
                continue;
            res.data[writeY][writeX] = mat.data[y][x];
            writeX++;
        }
        writeX = 0;
        writeY++;
    }

    return res;
}

Mat4 Transpose(const Mat4& mat)
{
    Mat4 res = {};

    for(int y = 0; y < 4; y++) {
        for(int x = 0; x < 4; x++) {
            res.data[x][y] = mat.data[y][x];
        }
    }

    return res;
}

Mat4 Adjugate(const Mat4& mat)
{
    // + - + -
    // - + - +
    // + - + -
    // - + - +
    Mat4 cofactorMat = {};
    for(int y = 0; y < 4; y++) {
        for(int x = 0; x < 4; x++) {
            float determ = Determ(GetSubMat(mat, x, y));
            if(y % 2 == 0) {
                if(x % 2 != 0)
                    determ = -determ;
            }
            else {
                if(x % 2 == 0)
                    determ = -determ;
            }
            cofactorMat.data[y][x] = determ;
        }
    }

    return Transpose(cofactorMat);
}

Mat4 Inverse(const Mat4& mat) 
{
    float determ = Determ(mat);
    Mat4 adjugateMat = Adjugate(mat);
    return adjugateMat * (1.0f / determ);
}

Mat4 NormalMat4FromModelMat(const Mat4& modelMat)
{
    return Transpose(Inverse(modelMat));
}

struct BasicColorShaderData
{
    Mat4 xformMat;
    Vec4 color;
};
CHECK_CBUFFER_ALIGNMENT(BasicColorShaderData);

struct PhongShaderData
{
    Mat4 projViewMat;
    Mat4 modelMat;
    Mat4 normalMat;
    Vec4 color;
    alignas(16) Vec3 lightPosition;
    alignas(16) Vec3 camPosition;
};
CHECK_CBUFFER_ALIGNMENT(PhongShaderData);

struct TextShaderData
{
    Vec4 color;
};
CHECK_CBUFFER_ALIGNMENT(TextShaderData);

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
    Mat4 projMat;
    Mat4 viewMat;
    Vec3 position;
    Vec3 front;
    Vec3 up;
    float moveSpeed;
    float lookSpeed;
    float pitch;
    float yaw;
    bool isControlOn;
};

FpsCam CreateFpsCam(Vec3 position, float moveSpeed, float lookSpeed, const Mat4& projMat)
{
    return {
        .projMat = projMat,
        .viewMat = IdentityMat4(),
        .position = position,
        .front = { 0.0f, 0.0f, -1.0f },
        .up = { 0.0f, 1.0f, 0.0f },
        .moveSpeed = moveSpeed,
        .lookSpeed = lookSpeed,
        .yaw = -89.0f // 0 would start with the cam looking to the right
    };
}

Mat4 CreateViewMatForFpsCam(FpsCam* cam)
{
    return LookatMat4(cam->position, cam->position + cam->front, cam->up);
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

    cam->viewMat = CreateViewMatForFpsCam(cam);
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

struct Dx11Program
{
    ID3D11VertexShader* vs;
    ID3DBlob* vsByteCode;
    ID3D11PixelShader* ps;
    ID3DBlob* psByteCode;
    ID3D11Buffer* cBuffer;
};

void FreeDx11Program(Dx11Program* program)
{
    program->vs->Release();
    program->vsByteCode->Release();
    program->ps->Release();
    program->psByteCode->Release();
    program->cBuffer->Release();
    *program = {};
}

Dx11Program CreateDx11ProgramFromFiles(const char* vsFilename, const char* psFilename, UINT cBufferByteSize, Dx11* dx)
{
    String vsCode = ReadAllTextFromFile(vsFilename);
    ID3DBlob* vsByteCode = CompileShaderCode(ShaderType::Vertex, vsCode);
    ID3D11VertexShader* vs = nullptr;
    dx->device->CreateVertexShader(
        vsByteCode->GetBufferPointer(), 
        vsByteCode->GetBufferSize(), 
        nullptr, 
        &vs
    );
    
    String psCode = ReadAllTextFromFile(psFilename);
    ID3DBlob* psByteCode = CompileShaderCode(ShaderType::Pixel, psCode);
    ID3D11PixelShader* ps = nullptr;
    dx->device->CreatePixelShader(
        psByteCode->GetBufferPointer(), 
        psByteCode->GetBufferSize(), 
        nullptr, 
        &ps
    );

    FreeString(&psCode);
    FreeString(&vsCode);

    ID3D11Buffer* cBuffer = CreateDx11ConstantBuffer(cBufferByteSize, dx);

    return {
        .vs = vs,
        .vsByteCode = vsByteCode,
        .ps = ps,
        .psByteCode = psByteCode,
        .cBuffer = cBuffer
    };
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

struct ObjModel
{
    Vec3* positions;
    Vec2* texCoords;
    Vec3* normals;
    unsigned int vertexCount;
};

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

constexpr int InvalidObjIndex = 0;

struct ObjStats
{
    int positionCount;
    int texCoordCount;
    int normalCount;
    unsigned int faceCount;
    unsigned int vertexCount;
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
                stats.faceCount++;
                break;
        }
    }

    stats.vertexCount = stats.faceCount * 3;
    return stats;
}

struct ObjVertex
{
    int positionId;
    int texCoordId;
    int normalId;
};

struct ObjData
{
    Vec3* positions;
    Vec2* texCoords;
    Vec3* normals;
    ObjVertex* vertices;
};

void FreeObjData(ObjData* objData)
{
    if(objData->positions != nullptr)
        free(objData->positions);
    if(objData->texCoords != nullptr)
        free(objData->texCoords);
    if(objData->normals != nullptr)
        free(objData->normals);

    *objData = {};
}

ObjData AllocateObjData(ObjStats stats)
{
    ObjData data = {
        .positions = (Vec3*)calloc(1, stats.positionCount * sizeof(Vec3)),
        .vertices = (ObjVertex*)calloc(1, stats.vertexCount * sizeof(ObjVertex))
    };

    ASSERT(data.positions != nullptr);
    ASSERT(data.vertices != nullptr);

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

int SplitStringOnChar(StringView line, char delimiter, bool collapseRepeatedDelimiters, StringView* dest)
{
    int writeIndex = 0;
    const char* nextStart = line.start;
    size_t nextLen = 0;
    for(int i = 0; i < line.len; i++) {
        char c = line.start[i];
        if(c == delimiter) {
            dest[writeIndex++] = { .start = nextStart, .len = nextLen };
            
            if(collapseRepeatedDelimiters) {
                while(line.start[(i + 1)] == delimiter && (i + 1) < line.len)
                    i++;
            }

        nextStart = line.start + (i + 1);
            nextLen = 0;
        } else {
            nextLen++;
        }
    }
    
    dest[writeIndex++] = { .start = nextStart, .len = nextLen };

    return writeIndex;
}

void GetVerticesFromObjLine(StringView line, ObjVertex* vertices, int* vertexWriteIndex)
{
    line = SkipObjLineStart(line);

    StringView vertexParts[3] = {};
    int vertexPartCount = SplitStringOnChar(line, ' ', true, vertexParts);
    for(int i = 0; i < vertexPartCount; i++) {
        StringView vertexPart = vertexParts[i];
        StringView indexParts[3] = {};
        ObjVertex vertex = {};
        SplitStringOnChar(vertexPart, '/', false, indexParts);
        char* parseAt = nullptr;
        if(indexParts[0].len > 0) {
            parseAt = (char*)indexParts[0].start;
            vertex.positionId = strtol(parseAt, &parseAt, 10);
        }
        if(indexParts[1].len > 0) {
            parseAt = (char*)indexParts[1].start;
            vertex.texCoordId = strtol(parseAt, &parseAt, 10);
        }
        if(indexParts[2].len > 0) {
            parseAt = (char*)indexParts[2].start;
            vertex.normalId = strtol(parseAt, &parseAt, 10);
        }
        vertices[(*vertexWriteIndex)++] = vertex;
    }
}

size_t GetArrayIndexFromObjIndex(int objIndex, size_t arrayLen)
{
    if(objIndex > 0) {
        return objIndex - 1;
    }
    else {
        return arrayLen - objIndex;
    }
}

void FreeObjModel(ObjModel* model)
{
    if(model->positions != nullptr)
        free(model->positions);
    if(model->texCoords != nullptr)
        free(model->texCoords);
    if(model->normals != nullptr)
        free(model->normals);

    *model = {};
}

ObjModel LoadModelFromObjFile(const char* filename)
{
    String objText = ReadAllTextFromFile(filename);

    StringReader reader = { .string = { .start = objText.data, .len = objText.len } };
    StringView line = {};

    ObjStats stats = GetObjStats(objText);
    ObjData data = AllocateObjData(stats);

    int vertexWriteIndex = 0;
    int texCoordWriteIndex = 0;
    int normalWriteIndex = 0;
    int verticesWriteIndex = 0;

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
                GetVerticesFromObjLine(line, data.vertices, &verticesWriteIndex);
                break;
        }
    }

    bool hasTexCoords = data.vertices[0].texCoordId != InvalidObjIndex;
    bool hasNormals = data.vertices[0].normalId != InvalidObjIndex;

    ObjModel model = { .vertexCount = stats.vertexCount };
    model.positions = (Vec3*)calloc(1, stats.vertexCount * sizeof(Vec3));
    ASSERT(model.positions != nullptr);

    if(hasTexCoords) {
        model.texCoords = (Vec2*)calloc(1, stats.vertexCount * sizeof(Vec2));
        ASSERT(model.texCoords != nullptr);
    }

    if(hasNormals) {
        model.normals = (Vec3*)calloc(1, stats.vertexCount * sizeof(Vec3));
        ASSERT(model.normals != nullptr);
    }

    for(int i = 0; i < stats.vertexCount; i++) {
        ObjVertex vertex = data.vertices[i];
        model.positions[i] = data.positions[GetArrayIndexFromObjIndex(vertex.positionId, stats.vertexCount)];
        if(hasTexCoords)
            model.texCoords[i] = data.texCoords[GetArrayIndexFromObjIndex(vertex.texCoordId, stats.vertexCount)];
        if(hasNormals)
            model.normals[i] = data.normals[GetArrayIndexFromObjIndex(vertex.normalId, stats.vertexCount)];
    }

    FreeObjData(&data);
    FreeString(&objText);

    return model;
}

struct Transform
{
    Vec3 position;
    Vec3 scale;
    Vec3 rotation;
};

Mat4 GetModelMatFromTransform(const Transform& transform)
{
    // TODO: support rotation on all axis
    return TranslateMat4(transform.position) * 
        ScaleMat4(transform.scale) * 
        RotateEulerXMat4(transform.rotation.x);
}

struct Dx11ModelData
{
    ID3D11Buffer** vertexBuffers;
    UINT vertexBufferCount;
    UINT* vertexBufferStrides;
    UINT* vertexBufferOffsets;
    UINT vertexCount;
};

Dx11ModelData CreateDx11ModelDataFromObjModel(const Dx11& dx, const ObjModel& objModel)
{
    Dx11ModelData modelData = {
        .vertexBufferCount = 2,
        .vertexCount = objModel.vertexCount
    };

    modelData.vertexBuffers = (ID3D11Buffer**)calloc(1, modelData.vertexBufferCount * sizeof(ID3D11Buffer*));
    ASSERT(modelData.vertexBuffers != nullptr);
    modelData.vertexBufferStrides = (UINT*)calloc(1, modelData.vertexBufferCount * sizeof(UINT*));
    ASSERT(modelData.vertexBufferStrides != nullptr);
    modelData.vertexBufferOffsets = (UINT*)calloc(1, modelData.vertexBufferCount * sizeof(UINT*));
    ASSERT(modelData.vertexBufferOffsets != nullptr);

    modelData.vertexBuffers[0] = CreateStaticDx11VertexBuffer(dx, objModel.positions, sizeof(Vec3) * objModel.vertexCount);
    modelData.vertexBuffers[1] = CreateStaticDx11VertexBuffer(dx, objModel.normals, sizeof(Vec3) * objModel.vertexCount);

    modelData.vertexBufferStrides[0] = sizeof(Vec3);
    modelData.vertexBufferStrides[1] = sizeof(Vec3);

    modelData.vertexBufferOffsets[0] = 0;
    modelData.vertexBufferOffsets[1] = 0;

    return modelData;
}

Dx11ModelData CreateDx11ModelDataForCube(const Dx11& dx, Vec3* vertexPositions, UINT vertexCount)
{
    Dx11ModelData modelData = {
        .vertexBufferCount = 1,
        .vertexCount = vertexCount
    };

    modelData.vertexBuffers = (ID3D11Buffer**)calloc(1, modelData.vertexBufferCount * sizeof(ID3D11Buffer*));
    ASSERT(modelData.vertexBuffers != nullptr);
    modelData.vertexBufferStrides = (UINT*)calloc(1, modelData.vertexBufferCount * sizeof(UINT*));
    ASSERT(modelData.vertexBufferStrides != nullptr);
    modelData.vertexBufferOffsets = (UINT*)calloc(1, modelData.vertexBufferCount * sizeof(UINT*));
    ASSERT(modelData.vertexBufferOffsets != nullptr);

    modelData.vertexBuffers[0] = CreateStaticDx11VertexBuffer(dx, vertexPositions, sizeof(Vec3) * vertexCount);

    modelData.vertexBufferStrides[0] = sizeof(Vec3);

    modelData.vertexBufferOffsets[0] = 0;

    return modelData;
}

void FreeDx11ModelData(Dx11ModelData* modelData)
{
    for(int i = 0; i < modelData->vertexBufferCount; i++)
        modelData->vertexBuffers[i]->Release();

    free(modelData->vertexBuffers);
    free(modelData->vertexBufferStrides);
    free(modelData->vertexBufferOffsets);

    *modelData = {};
}

void DrawLine(Vec3 position, Vec3 scale, float yRotation, ID3D11Buffer* cBuffer, Dx11* dx, const FpsCam& cam)
{
    BasicColorShaderData shaderData = { .color = { 0.0f, 0.0f, 0.0f, 1.0f } };
    Mat4 modelMat = TranslateMat4(position) * ScaleMat4(scale) * RotateEulerYMat4(toRadians(yRotation));
    shaderData.xformMat = cam.projMat * cam.viewMat * modelMat;
    UploadDataToConstantBuffer(cBuffer, &shaderData, sizeof(shaderData), dx);
    dx->context->VSSetConstantBuffers(0, 1, &cBuffer);
    dx->context->Draw(2, 0);
}

void DrawLineGrid(int xSquaresHalf, int zSquaresHalf, Dx11* dx, Dx11VertexBuffer* vertexBuffer, 
    ID3D11InputLayout* inputLayout, Dx11Program* program, const FpsCam& cam)
{
    dx->context->IASetVertexBuffers(0, 1, &vertexBuffer->buffer, &vertexBuffer->stride, &vertexBuffer->byteOffset);
    dx->context->IASetInputLayout(inputLayout);
    dx->context->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
    dx->context->VSSetShader(program->vs, nullptr, 0);
    dx->context->PSSetShader(program->ps, nullptr, 0);

    int nrOfXLines = 1 + (zSquaresHalf * 2);
    int nrOfZLines = 1 + (xSquaresHalf * 2);

    // draw lines parallel to x-axis
    for(int i = 0; i < nrOfXLines; i++) {
        Vec3 position = {};
        position.z = (float)i;
        position.z -= (float)((nrOfXLines - 1) / 2);
        float xLineLen = (float)(nrOfZLines - 1);
        DrawLine(position, { xLineLen, 1.0f, 0.0f }, 0.0f, program->cBuffer, dx, cam);
    }

    // draw lines parallel to z-axis
    for(int i = 0; i < nrOfZLines; i++) {
        Vec3 position = {};
        position.x = (float)i;
        position.x -= (float)((nrOfZLines - 1) / 2);
        float zLineLen = (float)(nrOfXLines - 1);
        DrawLine(position, { 1.0f, 1.0f, zLineLen }, 90.0f, program->cBuffer, dx, cam);
    }
}

void DrawDx11Model(Dx11* dx, Dx11ModelData& model, ID3D11InputLayout* inputLayout, const Dx11Program& program, 
    void* programData, UINT programDataByteSize)
{
    dx->context->IASetVertexBuffers(0, model.vertexBufferCount, model.vertexBuffers, model.vertexBufferStrides, model.vertexBufferOffsets);
    dx->context->IASetInputLayout(inputLayout);
    dx->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dx->context->VSSetShader(program.vs, nullptr, 0);
    dx->context->PSSetShader(program.ps, nullptr, 0);

    UploadDataToConstantBuffer(program.cBuffer, programData, programDataByteSize, dx);
    dx->context->VSSetConstantBuffers(0, 1, &program.cBuffer);
    dx->context->Draw(model.vertexCount, 0);
}

void DrawText(Dx11* dx, UINT textLen, Dx11VertexBuffer& positionVertexBuffer, Dx11VertexBuffer& instanceVertexBuffer, 
    ID3D11InputLayout* inputLayout, const Dx11Program& program, 
    void* programData, UINT programDataByteSize)
{
    UINT dummyOffset = 0;
    dx->context->IASetVertexBuffers(0, 1, &positionVertexBuffer.buffer, &positionVertexBuffer.stride, &dummyOffset);
    dx->context->IASetVertexBuffers(1, 1, &instanceVertexBuffer.buffer, &instanceVertexBuffer.stride, &dummyOffset);
    dx->context->IASetInputLayout(inputLayout);
    dx->context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    dx->context->VSSetShader(program.vs, nullptr, 0);
    dx->context->PSSetShader(program.ps, nullptr, 0);

    UploadDataToConstantBuffer(program.cBuffer, programData, programDataByteSize, dx);
    dx->context->VSSetConstantBuffers(0, 1, &program.cBuffer);
    dx->context->DrawInstanced(6, textLen, 0, 0);
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

static Vec3 cubeVertices[] = {
    // back face
    {  0.5f,  0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f, -0.5f },

    { -0.5f,  0.5f, -0.5f },
    {  0.5f,  0.5f, -0.5f },
    { -0.5f, -0.5f, -0.5f },

    // front face
    { -0.5f, -0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    {  0.5f,  0.5f,  0.5f },

    {  0.5f,  0.5f,  0.5f },
    { -0.5f,  0.5f,  0.5f },
    { -0.5f, -0.5f,  0.5f },

    // left face
    { -0.5f,  0.5f,  0.5f },
    { -0.5f,  0.5f, -0.5f },
    { -0.5f, -0.5f, -0.5f },

    { -0.5f, -0.5f, -0.5f },
    { -0.5f, -0.5f,  0.5f },
    { -0.5f,  0.5f,  0.5f },

    // right face
    {  0.5f, -0.5f, -0.5f },
    {  0.5f,  0.5f, -0.5f },
    {  0.5f,  0.5f,  0.5f },

    {  0.5f,  0.5f,  0.5f },
    {  0.5f, -0.5f,  0.5f },
    {  0.5f, -0.5f, -0.5f },

    // bottom face    
    { -0.5f, -0.5f, -0.5f },
    {  0.5f, -0.5f, -0.5f },
    {  0.5f, -0.5f,  0.5f },

    {  0.5f, -0.5f,  0.5f },
    { -0.5f, -0.5f,  0.5f },
    { -0.5f, -0.5f, -0.5f },

    // top face
    {  0.5f,  0.5f,  0.5f },
    {  0.5f,  0.5f, -0.5f },
    { -0.5f,  0.5f, -0.5f },

    { -0.5f,  0.5f, -0.5f },
    { -0.5f,  0.5f,  0.5f },
    {  0.5f,  0.5f,  0.5f }
};

static Vec3 lineVertices[] = {
    { -0.5f, 0.0f, 0.0f },
    {  0.5f, 0.0f, 0.0f }
};

static Vec3 quadVertices[] = {
    { 0.0f, 0.0f, 0.0f },
    { 0.0f, -1.0f, 0.0f },
    { 1.0f, -1.0f, 0.0f },
    { 1.0f, -1.0f, 0.0f },
    { 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f }
};

struct BakedCharMap
{
    ByteBuffer ttf;
    unsigned char* fontBitmap;
    int fontBitmapWidth;
    int fontBitmapHeight;
    int startChar;
    stbtt_bakedchar* bakedChars;
};

void FreeBakedCharMap(BakedCharMap* bakedCharMap)
{
    free(bakedCharMap->ttf.data);
    free(bakedCharMap->fontBitmap);
    free(bakedCharMap->bakedChars);
    *bakedCharMap = {};
}

BakedCharMap BakeCharMapForFont(const char* fontName, float fontHeight)
{
    ByteBuffer ttfBuffer = ReadAllBytesFromFile(fontName, 0);
    
    int fontBitmapWidth = 1024;
    int fontBitmapHeight = 1024;
    // one-channel bitmap
    unsigned char* fontBitmapBuffer = (unsigned char*)calloc(1, fontBitmapWidth * fontBitmapHeight);
    ASSERT(fontBitmapBuffer != nullptr);
    
    // from space to end of ASCII
    int startChar = 32;
    int nrOfChars = 96;
    stbtt_bakedchar* bakedChars = (stbtt_bakedchar*)calloc(1, nrOfChars * sizeof(stbtt_bakedchar));
    ASSERT(bakedChars != nullptr);

    int bakeRes = stbtt_BakeFontBitmap(
        ttfBuffer.data,
        0,
        fontHeight,
        fontBitmapBuffer,
        fontBitmapWidth,
        fontBitmapHeight,
        startChar,
        nrOfChars,
        bakedChars
    );
    ASSERT(bakeRes > 0);

    return {
        .ttf = ttfBuffer,
        .fontBitmap = fontBitmapBuffer,
        .fontBitmapWidth = fontBitmapWidth,
        .fontBitmapHeight = fontBitmapHeight,
        .startChar = startChar,
        .bakedChars = bakedChars
    };
}

struct CharQuadInstanceData
{
    Mat4 xformMat;
    Vec2 texCoords[6];
};

void GenerateQuadInstanceDataForStringAt(BakedCharMap& bakedCharMap, String text, Vec2 position, 
    Mat4& orthoProjMat, CharQuadInstanceData* instanceData)
{
    for(int i = 0; i < text.len; i++) {
        int c = (int)text.data[i];
        stbtt_aligned_quad quad = {};
        stbtt_GetBakedQuad(
            bakedCharMap.bakedChars,
            bakedCharMap.fontBitmapWidth,
            bakedCharMap.fontBitmapHeight,
            c - bakedCharMap.startChar,
            &position.x, 
            &position.y,
            &quad,
            1
        );

        // quad origin = top left
        Transform transform = {
            .position = { quad.x0, quad.y0, 0.0f },
            .scale =    { quad.x1 - quad.x0, quad.y1 - quad.y0, 1.0f }
        };

        Vec2 topLeftTexCoord =      { quad.s0, quad.t0 };
        Vec2 bottomLeftTexCoord =   { quad.s0, quad.t1 };
        Vec2 bottomRightTexCoord =  { quad.s1, quad.t1 };
        Vec2 topRightTexCoord =     { quad.s1, quad.t0 };

        instanceData[i] = {
            .xformMat = orthoProjMat * GetModelMatFromTransform(transform),
            .texCoords = {
                topLeftTexCoord,
                bottomLeftTexCoord,
                bottomRightTexCoord,
                bottomRightTexCoord,
                topRightTexCoord,
                topLeftTexCoord
            }
        };
    }
}

// GOAL: 
// =============================================
// Load a textured 3D model from an .obj file 
// with reference grid at 0.0.0, some info stats in corner and mouse drag controls and keyboard movement
// =============================================
// TODO: text rendering
// TODO: draw fps & model stats text on screen
// TODO: mouse click + drag controls for model rotation
// TODO: optimize grid drawing
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

    Dx11Program basicColorProgram = CreateDx11ProgramFromFiles("res/basiccolorvs.hlsl", "res/basiccolorps.hlsl", sizeof(BasicColorShaderData), &dx);
    ID3D11InputLayout* basicColorInputLayout = CreateBasicColorDx11InputLayout(&dx, basicColorProgram.vsByteCode);

    Dx11Program phongProgram = CreateDx11ProgramFromFiles("res/phongvs.hlsl", "res/phongps.hlsl", sizeof(PhongShaderData), &dx);
    ID3D11InputLayout* phongInputLayout = CreatePhongDx11InputLayout(&dx, phongProgram.vsByteCode);

    Dx11Program textProgram = CreateDx11ProgramFromFiles("res/textvs.hlsl", "res/textps.hlsl", sizeof(TextShaderData), &dx);
    ID3D11InputLayout* textInputLayout = CreateTextDx11InputLayout(&dx, textProgram.vsByteCode);

    Transform cubeTransform = {
        .position = { 1.5f, 4.5f, -1.5f },
        .scale = { 0.4f, 0.4f, 0.4f }
    };
    Dx11ModelData cubeDx11Model = CreateDx11ModelDataForCube(dx, cubeVertices, ARRAY_LEN(cubeVertices));

    ObjModel monkeyObjModel = LoadModelFromObjFile("res/monkey.obj");
    Transform monkeyTransform = {
        .position = { 0.0f, 0.0f, 0.0f },
        .scale = { 1.0f, 1.0f, 1.0f },
        .rotation = { toRadians(-90.0f), 0.0f, 0.0f }
    };
    Dx11ModelData monkeyDx11Model = CreateDx11ModelDataFromObjModel(dx, monkeyObjModel);

    Dx11VertexBuffer lineVertexBuffer = CreateStaticDx11VertexBuffer(lineVertices, sizeof(lineVertices), 3 * sizeof(float), 0, &dx);

    Dx11VertexBuffer textPositionVertexBuffer = CreateStaticDx11VertexBuffer(quadVertices, sizeof(quadVertices), 3 * sizeof(float), 0, &dx);

    FpsCam cam = CreateFpsCam({ 0.0f, 0.0f, 2.0f }, 5.0f, 6.0f, PerspectiveProjMat4(toRadians(45.0f), viewport.Width, viewport.Height, 0.1f, 100.0f));
    ToggleCamControl(&cam, true);

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

    Mat4 orthoProjMat = OrthoProjMat4(0.0f, viewport.Width, 0.0f, viewport.Height, 0.1f, 100.0f);
    BakedCharMap bakedCharMap = BakeCharMapForFont("res/CourierPrime-Regular.ttf", 64.0f);
    char text[] = "Hello";
    int maxQuadInstances = 16;
    CharQuadInstanceData* charQuadInstanceData = (CharQuadInstanceData*)calloc(1, maxQuadInstances * sizeof(CharQuadInstanceData));
    ASSERT(charQuadInstanceData != nullptr);
    GenerateQuadInstanceDataForStringAt(bakedCharMap, { text, ARRAY_LEN(text) - 1 }, { 100.0f, 100.0f }, orthoProjMat, charQuadInstanceData);
    Dx11VertexBuffer textInstanceVertexBuffer = CreateStaticDx11VertexBuffer(charQuadInstanceData, 
        maxQuadInstances * sizeof(CharQuadInstanceData), sizeof(CharQuadInstanceData), 0, &dx);

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

        dx.context->RSSetViewports(1, &viewport);
        dx.context->RSSetState(rasterizerState);
        dx.context->OMSetRenderTargets(1, &backbuffer.view, dsBuffer.view);
        dx.context->OMSetDepthStencilState(dsState, 1);
        dx.context->ClearRenderTargetView(backbuffer.view, clearColor);
        dx.context->ClearDepthStencilView(dsBuffer.view, D3D11_CLEAR_DEPTH, 1.0f, 0.0f);

        Mat4 monkeyModelMat = GetModelMatFromTransform(monkeyTransform);
        Mat4 monkeyNormalMat = NormalMat4FromModelMat(monkeyModelMat);
        PhongShaderData phongShaderData = {
            .projViewMat = cam.projMat * cam.viewMat,
            .modelMat = monkeyModelMat,
            .normalMat = monkeyNormalMat,
            .color = { 0.0f, 0.9f, 0.1f, 1.0f },
            .lightPosition = cubeTransform.position,
            .camPosition = cam.position
        };
        DrawDx11Model(&dx, monkeyDx11Model, phongInputLayout, phongProgram, &phongShaderData, sizeof(phongShaderData));

        Mat4 cubeModelMat = GetModelMatFromTransform(cubeTransform);
        BasicColorShaderData basicColorShaderData = {
            .xformMat = cam.projMat * cam.viewMat * cubeModelMat,
            .color = { 1.0f, 1.0f, 1.0f, 1.0f } 
        };
        DrawDx11Model(&dx, cubeDx11Model, basicColorInputLayout, basicColorProgram, &basicColorShaderData, sizeof(basicColorShaderData));

        DrawLineGrid(6, 6, &dx, &lineVertexBuffer, basicColorInputLayout, &basicColorProgram, cam);

        Transform quadTransform = {
            .position = { 0.0f, 100.0f, 0.0f },
            .scale = { 100.0f, 100.0f, 1.0f }
        };

        TextShaderData textShaderData = {
            .color = { 1.0f, 0.0f, 0.0f, 0.0f }
        };
        DrawText(&dx, ARRAY_LEN(text) - 1, textPositionVertexBuffer, textInstanceVertexBuffer, textInputLayout, textProgram,
            &textShaderData, sizeof(textShaderData));

        dx.swapchain->Present(1, 0);
        UpdateTimer(&timer);
        printf("delta time: %f\n", timer.deltaTime);
    }

    free(charQuadInstanceData);
    FreeBakedCharMap(&bakedCharMap);

    FreeDx11ModelData(&monkeyDx11Model);
    FreeObjModel(&monkeyObjModel);
    
    FreeDx11ModelData(&cubeDx11Model);

    textInputLayout->Release();
    basicColorInputLayout->Release();
    phongInputLayout->Release();

    FreeDx11Program(&textProgram);
    FreeDx11Program(&basicColorProgram);
    FreeDx11Program(&phongProgram);

    FreeDx11VertexBuffer(&textInstanceVertexBuffer);
    FreeDx11VertexBuffer(&textPositionVertexBuffer);
    FreeDx11VertexBuffer(&lineVertexBuffer);
    
    rasterizerState->Release();
    dsState->Release();
    FreeDx11DepthStencilBuffer(&dsBuffer);
    FreeDx11Backbuffer(&backbuffer);
    FreeDx11(&dx);
    DestroyWindow(window);
    return 0;
}