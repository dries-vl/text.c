#include <windows.h>
#include <GL/gl.h>

static HWND hWnd;
static HDC hDC;
static HGLRC hGLRC;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_DESTROY) {
        PostQuitMessage(0);
        return 0; // Prevent further processing
    }
    return DefWindowProc(hWnd, message, wParam, lParam);
}

int create_window(int width, int height, const char *title) {
    HINSTANCE hInstance = GetModuleHandle(NULL);
    WNDCLASS wc = {0};
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "OpenGLWindowClass";
    
    if (!RegisterClass(&wc)) return -1;

    hWnd = CreateWindow("OpenGLWindowClass", title, WS_OVERLAPPEDWINDOW,
                        CW_USEDEFAULT, CW_USEDEFAULT, width, height,
                        NULL, NULL, hInstance, NULL);
    if (!hWnd) return -1;

    hDC = GetDC(hWnd);
    ShowWindow(hWnd, SW_SHOWDEFAULT);
    return 0;
}

void initialize_opengl() {
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR), 1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA, 32,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 24, 8, 0,
        PFD_MAIN_PLANE, 0, 0, 0, 0
    };

    int pf = ChoosePixelFormat(hDC, &pfd);
    SetPixelFormat(hDC, pf, &pfd);
    hGLRC = wglCreateContext(hDC);
    wglMakeCurrent(hDC, hGLRC);
}

int poll_events() {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) return 0;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    Sleep(1); // Reduce CPU usage when idle
    return 1;
}

void swap_buffers() {
    SwapBuffers(hDC);
}

void destroy_window() {
    if (hGLRC) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(hGLRC);
    }
    if (hDC) ReleaseDC(hWnd, hDC);
    if (hWnd) DestroyWindow(hWnd);
    UnregisterClass("OpenGLWindowClass", GetModuleHandle(NULL));
}
