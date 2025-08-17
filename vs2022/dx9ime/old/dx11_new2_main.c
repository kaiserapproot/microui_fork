#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include "microui.h"
#include "dx11_new2_render.h"

#pragma comment(lib, "d3dcompiler.lib")

// グローバル変数宣言(他ファイルから参照可能に)
int width = 1200;
int height = 800;
mu_Context* g_ctx = NULL;
char logbuf[64000] = { 0 };
int logbuf_updated = 0;
float bg[4] = { 90, 95, 100, 255 };

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
    *value = (unsigned char)tmp;
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
        InvalidateRect(hwnd, NULL, FALSE);  // 再描画要求を追加
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        // 実際に描画処理を呼び出す
        r_draw();
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
        InvalidateRect(hwnd, NULL, FALSE);  // 再描画要求を追加
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
        InvalidateRect(hwnd, NULL, FALSE);  // 再描画要求を追加
        return 0;
    }
    case WM_KEYDOWN:
        mu_input_keydown(g_ctx, (int)wParam);
        InvalidateRect(hwnd, NULL, FALSE);  // 再描画要求を追加
        return 0;
    case WM_CHAR: {
        char text[2] = { (char)wParam, '\0' };
        mu_input_text(g_ctx, text);
        InvalidateRect(hwnd, NULL, FALSE);  // 再描画要求を追加
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

    // 簡単なテストウィンドウ
    int win = mu_begin_window(ctx, "Test Window", mu_rect(50, 50, 300, 200));
    char buf[128];
    sprintf(buf, "mu_begin_window returned %d\n", win);
    OutputDebugStringA(buf);
    //    if (mu_begin_window(ctx, "Test Window", mu_rect(50, 50, 300, 200)))
    if (win)
    {
        mu_layout_row(ctx, 1, (int[]) { -1 }, 0);
        mu_label(ctx, "ああ!");
        if (mu_button(ctx, "じゅべむ寿限無n"))
        {
            write_log("Button clicked!");
        }
        mu_end_window(ctx);
    }

    style_window(ctx);
    log_window(ctx);

    mu_end(ctx);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    WNDCLASSW wc = { 0 };
    HWND hwnd;
    const wchar_t CLASS_NAME[] = L"DX11NewWindowClass";
    MSG msg = { 0 };
    DWORD lastTime = GetTickCount();

    // microUIコンテキスト初期化
    g_ctx = (mu_Context*)malloc(sizeof(mu_Context));
    if (!g_ctx)
    {
        MessageBoxW(NULL, L"Failed to allocate microUI context", L"Error", MB_OK);
        return 1;
    }

    memset(g_ctx, 0, sizeof(mu_Context));
    mu_init(g_ctx);
    g_ctx->text_width = r_get_text_width;
    g_ctx->text_height = r_get_text_height;

    // ウィンドウクラス登録
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    RegisterClassW(&wc);

    // ウィンドウ作成
    hwnd = CreateWindowExW(0, CLASS_NAME, L"DirectX11 MicroUI Application", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, width, height, NULL, NULL, hInstance, NULL);
    if (!hwnd)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK);
        free(g_ctx);
        return 0;
    }

    // DirectX11初期化
    InitD3D(hwnd);
    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    // ログに初期メッセージを追加してテスト
    write_log("Application started successfully!");
    write_log("DirectX11 and MicroUI initialized.");
    write_log("This is a test message.");

    // メインループ
    while (TRUE)
    {
        // メッセージ処理
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) goto cleanup;
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        // フレーム制限は削除し、WM_PAINTメッセージでのみ描画
        Sleep(1);
    }

cleanup:
    CleanD3D();
    free(g_ctx);
    return (int)msg.wParam;
}