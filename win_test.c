#include <windows.h>
#include <stdint.h>

int main() {
    // Create a window for displaying (for demonstration purposes)
    HWND hWnd = CreateWindowEx(
        0, "STATIC", "DIB Example", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 200, 200, NULL, NULL, NULL, NULL);

    // Get device context for the window
    HDC hdc = GetDC(hWnd);

    // Set up bitmap info
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = 100;
    bmi.bmiHeader.biHeight = -100;  // Top-down
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    // Create a DIB section and get a pointer to the pixel data
    void* pixels;
    HBITMAP hBitmap = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pixels, NULL, 0);
    if (hBitmap == NULL || pixels == NULL) {
        MessageBox(hWnd, "Failed to create DIB section", "Error", MB_OK);
        return 1;
    }

    // Fill the pixels with red color
    uint32_t* pixel_data = (uint32_t*) pixels;
    for (int i = 0; i < 100; i++) {
        for (int j = 0; j < 100; j++) {
            pixel_data[i * 100 + j] = 0x00FF0000;  // Blue component first for BGRA
        }
    }

    // Create a compatible memory DC and select the bitmap into it
    HDC memDC = CreateCompatibleDC(hdc);
    SelectObject(memDC, hBitmap);

    // Coordinates for where to place the bitmap
    int x = 50;
    int y = 50;

    // Display the DIB section on the window
    while(1) {
        BitBlt(hdc, x, y, 100, 100, memDC, 0, 0, SRCCOPY);
    }
    // Keep the window open to display the result
    MessageBox(hWnd, "DIB Section example complete", "Info", MB_OK);
    DestroyWindow(hWnd);
    return 0;
}
