#define COBJMACROS    // Needed for direct COM interface access
#include <windows.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <stdint.h>
#include <initguid.h>

// Define necessary GUIDs
DEFINE_GUID(IID_ID3D11Texture2D,
    0x6f15aaf2, 0xd208, 0x4e89, 0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c);
DEFINE_GUID(IID_ID3D11RenderTargetView,
    0xdfdba067, 0x0c64, 0x4b92, 0x80, 0x65, 0xbc, 0x88, 0x9c, 0x5f, 0xdc, 0x98);

ID3D11Device* device;
ID3D11DeviceContext* context;
IDXGISwapChain* swapchain;
ID3D11RenderTargetView* rtv;
ID3D11Texture2D* texture;
const int WIDTH = 800;
const int HEIGHT = 600;

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    // Register window class
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "D3DWindow";  // Removed L prefix
    RegisterClassEx(&wc);

    // Create window
    HWND hwnd = CreateWindow("D3DWindow", "Red Square",  // Removed L prefix
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
        WIDTH, HEIGHT, NULL, NULL, hInstance, NULL);

    // Setup D3D
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = WIDTH;
    scd.BufferDesc.Height = HEIGHT;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.Windowed = TRUE;

    D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
        NULL, 0, D3D11_SDK_VERSION, &scd, &swapchain, &device, NULL, &context);

    // Create texture for our pixel buffer
    D3D11_TEXTURE2D_DESC td = {0};
    td.Width = WIDTH;
    td.Height = HEIGHT;
    td.MipLevels = 1;
    td.ArraySize = 1;
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage = D3D11_USAGE_DYNAMIC;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    ID3D11Device_CreateTexture2D(device, &td, NULL, &texture);

    // Get backbuffer and create render target view
    ID3D11Texture2D* backbuffer;
    IDXGISwapChain_GetBuffer(swapchain, 0, &IID_ID3D11Texture2D, (void**)&backbuffer);
    ID3D11Device_CreateRenderTargetView(device, (ID3D11Resource*)backbuffer, NULL, &rtv);
    ID3D11Texture2D_Release(backbuffer);

    ShowWindow(hwnd, nCmdShow);

    // Main loop
    MSG msg = {0};
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        } else {
            // Map texture for writing
            D3D11_MAPPED_SUBRESOURCE mapped;
            ID3D11DeviceContext_Map(context, (ID3D11Resource*)texture, 0,
                                  D3D11_MAP_WRITE_DISCARD, 0, &mapped);
            uint32_t* data = (uint32_t*)mapped.pData;

            // Draw red square
            for (int y = 0; y < HEIGHT; y++) {
                for (int x = 0; x < WIDTH; x++) {
                    if (x >= WIDTH/4 && x < WIDTH*3/4 &&
                        y >= HEIGHT/4 && y < HEIGHT*3/4) {
                        data[y * mapped.RowPitch/4 + x] = 0xFFFF0000; // Red
                    } else {
                        data[y * mapped.RowPitch/4 + x] = 0xFF000000; // Black
                    }
                }
            }
            ID3D11DeviceContext_Unmap(context, (ID3D11Resource*)texture, 0);

            // Copy texture to backbuffer and present
            ID3D11DeviceContext_CopyResource(context, (ID3D11Resource*)backbuffer,
                                           (ID3D11Resource*)texture);
            IDXGISwapChain_Present(swapchain, 1, 0);
        }
    }

    // Cleanup
    ID3D11Texture2D_Release(texture);
    ID3D11RenderTargetView_Release(rtv);
    IDXGISwapChain_Release(swapchain);
    ID3D11DeviceContext_Release(context);
    ID3D11Device_Release(device);

    return 0;
}
