#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include "microui.h"
#include "dx11_new_render.h"

#pragma comment(lib, "d3dcompiler.lib")

// �O���[�o���ϐ��錾(���t�@�C������Q�Ɖ\��)
int width = 1200;
int height = 800;
mu_Context* g_ctx = NULL;
char logbuf[64000] = { 0 };
int logbuf_updated = 0;
float bg[4] = { 90, 95, 100, 105 };

extern void style_window(mu_Context* ctx);
extern void log_window(mu_Context* ctx);
extern void write_log(const char* text);
extern int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high);
extern void r_init(ID3D11Device*, ID3D11DeviceContext*, IDXGISwapChain*, ID3D11RenderTargetView*);
extern void r_cleanup(void);
extern void process_frame(mu_Context* ctx);
extern void r_clear(mu_Color clr);
extern void r_present(void);
extern void r_draw(void);

void write_log(const char* text)
{
    if (logbuf[0]) { strcat(logbuf, "\n"); }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high)
{
    int res;
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (!g_ctx) return DefWindowProc(hwnd, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_MOUSEMOVE: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        RECT rect;
        GetClientRect(hwnd, &rect);
        float scale_x = (float)width / (float)rect.right;
        float scale_y = (float)height / (float)rect.bottom;
        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mousemove(g_ctx, x, y);
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // WM_PAINT�ł͕`�揈�����s��Ȃ��i���C�����[�v�ŏ����j
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        RECT rect;
        GetClientRect(hwnd, &rect);
        float scale_x = (float)width / (float)rect.right;
        float scale_y = (float)height / (float)rect.bottom;
        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mousedown(g_ctx, x, y, MU_MOUSE_LEFT);
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP: {
        int x = LOWORD(lParam), y = HIWORD(lParam);
        RECT rect;
        GetClientRect(hwnd, &rect);
        float scale_x = (float)width / (float)rect.right;
        float scale_y = (float)height / (float)rect.bottom;
        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mouseup(g_ctx, x, y, MU_MOUSE_LEFT);
        ReleaseCapture();
        return 0;
    }
    case WM_KEYDOWN:
        mu_input_keydown(g_ctx, wParam);
        return 0;
    case WM_CHAR: {
        char text[2] = { (char)wParam, '\0' };
        mu_input_text(g_ctx, text);
        return 0;
    }
    case WM_SIZE: {
        // �E�B���h�E�T�C�Y���ύX���ꂽ���̏���
        if (wParam != SIZE_MINIMIZED)
        {
            RECT rect;
            GetClientRect(hwnd, &rect);
            // �K�v�ɉ����ăo�b�t�@�����T�C�Y
            // resize_buffers(rect.right, rect.bottom);
        }
        return 0;
    }
    case WM_SETCURSOR:
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void process_frame(mu_Context* ctx)
{
    mu_begin(ctx);
    style_window(ctx);
    log_window(ctx);
    // ���ɂ��E�B�W�F�b�g��ǉ��������ꍇ�͂����ɌĂяo��
    mu_end(ctx);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    WNDCLASSW wc = { 0 };
    HWND hwnd;
    const wchar_t CLASS_NAME[] = L"DX11NewWindowClass";
    MSG msg = { 0 };
    DWORD lastTime = GetTickCount();

    // microUI�R���e�L�X�g������
    g_ctx = malloc(sizeof(mu_Context));
    if (!g_ctx)
    {
        MessageBoxW(NULL, L"Failed to allocate microUI context", L"Error", MB_OK);
        return 1;
    }
    memset(g_ctx, 0, sizeof(mu_Context));
    mu_init(g_ctx);
    g_ctx->text_width = r_get_text_width;
    g_ctx->text_height = r_get_text_height;

    // �E�B���h�E�N���X�o�^
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    // �E�B���h�E�쐬
    hwnd = CreateWindowExW(0, CLASS_NAME, L"DirectX11 MicroUI Application", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK);
        free(g_ctx);
        return 0;
    }

    // DirectX11������
    InitD3D(hwnd);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd); // ����`����m���ɍs��

    // ���O�ɏ������b�Z�[�W��ǉ����ăe�X�g
    write_log("Application started successfully!");
    write_log("DirectX11 and MicroUI initialized.");

    // ���C�����[�v
    while (TRUE)
    {
        // ���b�Z�[�W����
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // �t���[�������i��60FPS�j
        DWORD currentTime = GetTickCount();
        if (currentTime - lastTime > 16)
        {
            // �`�揈��
            r_draw(); // ���̒���process_frame���Ă΂��
            lastTime = currentTime;
        }
        else
        {
            Sleep(1); // CPU�𑼂̃v���Z�X�ɏ���
        }
    }

cleanup:
    CleanD3D();
    free(g_ctx);
    return (int)msg.wParam;
}