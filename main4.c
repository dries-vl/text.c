#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Add missing typedefs for TCC
typedef unsigned long RPC_STATUS;
typedef void* RPC_NS_HANDLE;
typedef void* RPC_BINDING_HANDLE;
typedef void* RPC_IF_HANDLE;
typedef void* RPC_AUTH_IDENTITY_HANDLE;
typedef void* RPC_AUTHZ_HANDLE;

#include <d3d11.h>
#include <d3dcompiler.h>

#pragma comment(lib, "user32")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")

static ID3D11Device* device;
static ID3D11DeviceContext* context;
static IDXGISwapChain* swapchain;
static ID3D11RenderTargetView* rtv;

static const char* vs_str = 
    "float4 main(float2 pos : POS) : SV_POSITION { return float4(pos, 0, 1); }";
static const char* ps_str = 
    "float4 main() : SV_TARGET { return float4(1.0, 0.0, 0.0, 1.0); }";

static const float vertices[] = {
    0.0f,  0.5f,
   -0.5f, -0.5f,
    0.5f, -0.5f
};

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_DESTROY) PostQuitMessage(0);
    return DefWindowProcA(hwnd, msg, wparam, lparam);
}

int main() {
    // Create Window
    WNDCLASSA wc = {
        .lpfnWndProc = WndProc,
        .lpszClassName = "d3d11",
        .style = CS_OWNDC
    };
    RegisterClassA(&wc);
    HWND hwnd = CreateWindowA("d3d11", "", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600, 0, 0, 0, 0);
    ShowWindow(hwnd, SW_SHOW);

    // Create D3D11 Device and SwapChain
    DXGI_SWAP_CHAIN_DESC sd = {
        .BufferDesc.Width = 800,
        .BufferDesc.Height = 600,
        .BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM,
        .SampleDesc.Count = 1,
        .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
        .BufferCount = 1,
        .OutputWindow = hwnd,
        .Windowed = TRUE
    };
    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0,
        D3D11_SDK_VERSION, &sd, &swapchain, &device, NULL, &context);

    // Create RTV
    ID3D11Texture2D* backbuffer;
    ID3D11SwapChain_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void**)&backbuffer);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)backbuffer, NULL, &rtv);
    ID3D11Texture2D_Release(backbuffer);

    // Compile and create shaders
    ID3DBlob *vs_blob, *ps_blob, *error_blob;
    D3DCompile(vs_str, strlen(vs_str), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vs_blob, &error_blob);
    D3DCompile(ps_str, strlen(ps_str), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &ps_blob, &error_blob);

    ID3D11VertexShader* vs;
    ID3D11PixelShader* ps;
    ID3D11Device_CreateVertexShader(device, ID3D10Blob_GetBufferPointer(vs_blob),
        ID3D10Blob_GetBufferSize(vs_blob), NULL, &vs);
    ID3D11Device_CreatePixelShader(device, ID3D10Blob_GetBufferPointer(ps_blob),
        ID3D10Blob_GetBufferSize(ps_blob), NULL, &ps);

    // Create vertex buffer
    D3D11_BUFFER_DESC bd = {
        .ByteWidth = sizeof(vertices),
        .Usage = D3D11_USAGE_DEFAULT,
        .BindFlags = D3D11_BIND_VERTEX_BUFFER
    };
    D3D11_SUBRESOURCE_DATA vd = { .pSysMem = vertices };
    ID3D11Buffer* vb;
    ID3D11Device_CreateBuffer(device, &bd, &vd, &vb);

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POS", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    ID3D11InputLayout* input_layout;
    ID3D11Device_CreateInputLayout(device, layout, 1,
        ID3D10Blob_GetBufferPointer(vs_blob),
        ID3D10Blob_GetBufferSize(vs_blob), &input_layout);

    // Main loop
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageA(&msg);
        } else {
            float clear_color[4] = {0.0f, 0.2f, 0.4f, 1.0f};
            ID3D11DeviceContext_ClearRenderTargetView(context, rtv, clear_color);
            ID3D11DeviceContext_OMSetRenderTargets(context, 1, &rtv, NULL);

            D3D11_VIEWPORT viewport = {0, 0, 800, 600, 0, 1};
            ID3D11DeviceContext_RSSetViewports(context, 1, &viewport);
            
            UINT stride = 2 * sizeof(float);
            UINT offset = 0;
            ID3D11DeviceContext_IASetVertexBuffers(context, 0, 1, &vb, &stride, &offset);
            ID3D11DeviceContext_IASetInputLayout(context, input_layout);
            ID3D11DeviceContext_IASetPrimitiveTopology(context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            ID3D11DeviceContext_VSSetShader(context, vs, NULL, 0);
            ID3D11DeviceContext_PSSetShader(context, ps, NULL, 0);
            ID3D11DeviceContext_Draw(context, 3, 0);

            IDXGISwapChain_Present(swapchain, 1, 0);
        }
    }
    return 0;
}
