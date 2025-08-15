/**
 * dx11_main.c
 * DirectX11でmicrouiを実行するメインプログラム
 * main.c（DirectX9版）を参考にDirectX11用に実装
 */
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <math.h>
#include "microui.h"
#include "dx11_renderer.h"

// グローバル変数
int width = 1200;
int height = 800;

mu_Context* g_ctx = NULL;
static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[4] = { 90, 95, 100, 105 };

// DirectX11レンダラー用の変数を外部参照に変更
extern ID3D11Device* g_device;
extern ID3D11DeviceContext* g_context;
extern IDXGISwapChain* g_swapChain;
extern ID3D11RenderTargetView* g_renderTargetView;

// uint8_slider関数 - 8ビット値用のスライダー
static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high)
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

// ログに書き込む関数
static void write_log(const char* text)
{
    if (logbuf[0]) { strcat(logbuf, "\n"); }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

// デモウィンドウ表示関数
static void test_window(mu_Context* ctx)
{
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 250)))
    {
        mu_Container* win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        /* window info */
        if (mu_header(ctx, "Window Info"))
        {
            char buf[64];
            int layout[] = { 54, -1 };
            mu_Container* win = mu_get_current_container(ctx);

            mu_layout_row(ctx, 2, layout, 0);
            mu_label(ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED))
        {
            int layout[] = { 86, -110, -1 };
            mu_layout_row(ctx, 3, layout, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
            if (mu_begin_popup(ctx, "Test Popup"))
            {
                mu_button(ctx, "Hello");
                mu_button(ctx, "World");
                mu_end_popup(ctx);
            }
        }

        /* tree */
        if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED))
        {
            int layout[] = { 140, -1 };
            int layout_single[] = { -1 };
            mu_layout_row(ctx, 2, layout, 0);
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode(ctx, "Test 1"))
            {
                if (mu_begin_treenode(ctx, "Test 1a"))
                {
                    mu_label(ctx, "Hello");
                    mu_label(ctx, "world");
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "Test 1b"))
                {
                    if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                    if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "日本語テスト"))
                {
                    mu_label(ctx, "こんにちは世界");
                    mu_label(ctx, "あいうえお");
                    if (mu_button(ctx, "テストボタン")) { write_log("日本語ボタンが押されました"); }
                    mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2"))
            {
                int layout_buttons[] = { 54, 54 };
                mu_layout_row(ctx, 2, layout_buttons, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 3"))
            {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(ctx, "Checkbox 1", &checks[0]);
                mu_checkbox(ctx, "Checkbox 2", &checks[1]);
                mu_checkbox(ctx, "Checkbox 3", &checks[2]);
                mu_end_treenode(ctx);
            }
            mu_layout_end_column(ctx);

            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 1, layout_single, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED))
        {
            char buf[32];
            int sz;
            int layout[] = { -78, -1 };
            int layout_sliders[] = { 46, -1 };
            mu_Rect r;
            mu_Rect resize_rect;
            mu_Container* win;
            mu_layout_row(ctx, 2, layout, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 2, layout_sliders, 0);
            mu_label(ctx, "Red:"); mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:"); mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));

            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
            // サイズ変更エリアの検出
            win = mu_get_current_container(ctx);
            sz = ctx->style->title_height;
            resize_rect = mu_rect(win->rect.x + win->rect.w - sz, win->rect.y + win->rect.h - sz, sz, sz);
            if (ctx->mouse_pos.x >= resize_rect.x && ctx->mouse_pos.x <= resize_rect.x + resize_rect.w &&
                ctx->mouse_pos.y >= resize_rect.y && ctx->mouse_pos.y <= resize_rect.y + resize_rect.h)
            {
                SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
            }
        }

        mu_end_window(ctx);
    }
}

// スタイル編集ウィンドウ
void style_window(mu_Context* ctx)
{
    static struct { const char* label; int idx; } colors[] = {
      { "text:", MU_COLOR_TEXT },
      { "border:", MU_COLOR_BORDER },
      { "windowbg:", MU_COLOR_WINDOWBG },
      { "titlebg:", MU_COLOR_TITLEBG },
      { "titletext:", MU_COLOR_TITLETEXT },
      { "panelbg:", MU_COLOR_PANELBG },
      { "button:", MU_COLOR_BUTTON },
      { "buttonhover:", MU_COLOR_BUTTONHOVER },
      { "buttonfocus:", MU_COLOR_BUTTONFOCUS },
      { "base:", MU_COLOR_BASE },
      { "basehover:", MU_COLOR_BASEHOVER },
      { "basefocus:", MU_COLOR_BASEFOCUS },
      { "scrollbase:", MU_COLOR_SCROLLBASE },
      { "scrollthumb:", MU_COLOR_SCROLLTHUMB },
      { NULL, 0 }
    };

    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 290)))
    {
        int i, sz;
        mu_Container* win;
        mu_Rect resize_rect;
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        int layout[] = { 80, sw, sw, sw, sw, -1 };
        mu_layout_row(ctx, 6, layout, 0);
        for (i = 0; colors[i].label; i++)
        {
            mu_Rect r;
            mu_label(ctx, colors[i].label);
            uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
            r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, ctx->style->colors[i]);
        }
        // サイズ変更エリアの検出
        win = mu_get_current_container(ctx);
        sz = ctx->style->title_height;
        resize_rect = mu_rect(win->rect.x + win->rect.w - sz, win->rect.y + win->rect.h - sz, sz, sz);
        if (ctx->mouse_pos.x >= resize_rect.x && ctx->mouse_pos.x <= resize_rect.x + resize_rect.w &&
            ctx->mouse_pos.y >= resize_rect.y && ctx->mouse_pos.y <= resize_rect.y + resize_rect.h)
        {
            SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
        }

        mu_end_window(ctx);
    }
}

// ログウィンドウ
static void log_window(mu_Context* ctx)
{
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200)))
    {
        /* output text panel */
        mu_Container* panel;
        static char buf[128];
        int submitted = 0;
        int layout1[1] = { -1 };
        int layout2[2] = { -70, -1 };

        mu_layout_row(ctx, 1, layout1, -25);
        mu_begin_panel(ctx, "Log Output");
        panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, layout1, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated)
        {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        /* input textbox + submit button */
        mu_layout_row(ctx, 2, layout2, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT)
        {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted)
        {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}

// UTF-8文字列を生成するヘルパー関数
static const char* create_utf8_string(int codepoint) {
    static char buffer[8]; // UTF-8は最大4バイト + 終端NUL

    if (codepoint <= 0x7F) {
        // 1バイト (ASCII)
        buffer[0] = (char)codepoint;
        buffer[1] = 0;
    }
    else if (codepoint <= 0x7FF) {
        // 2バイト
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        buffer[2] = 0;
    }
    else if (codepoint <= 0xFFFF) {
        // 3バイト (日本語はここ)
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        buffer[3] = 0;
    }
    else {
        // 4バイト (絵文字など)
        buffer[0] = (char)(0xF0 | (codepoint >> 18));
        buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        buffer[4] = 0;
    }

    return buffer;
}

// 空のウィジェット用関数
static void empty_window(mu_Context* ctx) {
    static int init = 0;
    static const char* katakana_a = NULL;
    static const char* katakana_i = NULL;
    if (!katakana_a) {
        katakana_a = create_utf8_string(0x30A2);
        katakana_i = create_utf8_string(0x30A4);
    }
    // カタカナ「ア」を表示する
    if (mu_begin_window(ctx, "こんにちは世界", mu_rect(150, 150, 300, 200))) {
        wchar_t* wstr = L"こんにちは世界";
        char utf8str[64];
        WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8str, sizeof(utf8str), NULL, NULL);
        mu_label(ctx, utf8str);
        mu_label(ctx, "日本語テスト");
        mu_end_window(ctx);
    }
}

// フレーム処理関数
void process_frame(mu_Context* ctx)
{
    mu_begin(ctx);
    // 既存のウィジェットをコメントアウト
    style_window(ctx);
    log_window(ctx);
    test_window(ctx);

    // 代わりに空のウィンドウを表示
    //empty_window(ctx);

    mu_end(ctx);
}

// テキスト関連関数 (DirectX11用に修正)
static int text_width(mu_Font font, const char* text, int len)
{
    // 単純な幅計算（各文字のピクセル幅の合計）
    if (len < 0) len = (int)strlen(text);
    
    // 簡易的な実装 - 1文字あたり8ピクセルと仮定
    int width = 0;
    for (int i = 0; i < len; i++) {
        // ASCII文字なら8ピクセル、それ以外なら16ピクセル（簡易的な処理）
        if ((unsigned char)text[i] < 128) {
            width += 8; // ASCII
        }
        else {
            // マルチバイト文字（日本語など）
            width += 16;
            // UTF-8の追加バイトをスキップ
            if ((text[i] & 0xE0) == 0xC0) i += 1;      // 2バイト文字
            else if ((text[i] & 0xF0) == 0xE0) i += 2; // 3バイト文字
            else if ((text[i] & 0xF8) == 0xF0) i += 3; // 4バイト文字
        }
    }
    
    return width;
}

static int text_height(mu_Font font)
{
    // 単純な高さ計算
    return 18; // フォントの高さは固定18ピクセル
}

// DirectX11でmicrouiコマンドを描画する関数
void render_microui(mu_Context* ctx) {
    // DirectX11レンダリング開始
    dx11_begin_frame();
    
    // 背景色でクリア
    dx11_clear_screen(bg[0], bg[1], bg[2], 255);
    
    // ⚠️重要: デバッグ情報を出力

    
    // microuiのコマンド処理
    mu_Command* cmd = NULL;
    mu_Rect lastClip = {0};
    int command_counter = 0;
    
    while (mu_next_command(ctx, &cmd)) {
        command_counter++;
        
        switch (cmd->type) {
            case MU_COMMAND_CLIP:
                dx11_set_clip_rect(cmd->clip.rect);
                lastClip = cmd->clip.rect;

                break;
                
            case MU_COMMAND_RECT:

                dx11_push_quad(cmd->rect.rect, cmd->rect.color);
                break;
                
            case MU_COMMAND_TEXT:

                dx11_draw_text(cmd->text.str, cmd->text.pos.x, cmd->text.pos.y, cmd->text.color);
                break;
                
            case MU_COMMAND_ICON:

                dx11_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
                break;
                
            default:

                break;
        }
    }

    // DirectX11レンダリング終了
    dx11_end_frame();
    dx11_present();
}

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    static DWORD lastDebugTime = 0;
    DWORD currentTime = GetTickCount();
    if (g_ctx == NULL)
    {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    switch (uMsg)
    {
    case WM_MOUSEMOVE: {
        float scale_x;
        float scale_y;
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        // 座標変換
        RECT rect;
        GetClientRect(hwnd, &rect);
        scale_x = (float)width / (float)rect.right;
        scale_y = (float)height / (float)rect.bottom;

        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mousemove(g_ctx, x, y);
        InvalidateRect(hwnd, NULL, TRUE);  // 再描画要求
        return 0;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        process_frame(g_ctx);  // microuiフレーム処理
        render_microui(g_ctx); // DirectX11でmicrouiを描画
        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_LBUTTONDOWN: {
        float scale_x;
        float scale_y;
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        // 座標変換
        RECT rect;
        GetClientRect(hwnd, &rect);
        scale_x = (float)width / (float)rect.right;
        scale_y = (float)height / (float)rect.bottom;

        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mousedown(g_ctx, x, y, MU_MOUSE_LEFT);
        SetCapture(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_LBUTTONUP: {
        RECT rect;
        float scale_x;
        float scale_y;

        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        // 座標変換
        GetClientRect(hwnd, &rect);
        scale_x = (float)width / (float)rect.right;
        scale_y = (float)height / (float)rect.bottom;

        x = (int)(x * scale_x);
        y = (int)(y * scale_y);
        mu_input_mouseup(g_ctx, x, y, MU_MOUSE_LEFT);
        ReleaseCapture();
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_KEYDOWN: {
        mu_input_keydown(g_ctx, wParam);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_CHAR: {
        char text[2] = { (char)wParam, '\0' };
        mu_input_text(g_ctx, text);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }
    case WM_SETCURSOR: {
        // マウスカーソルの変更
        if (LOWORD(lParam) == HTBOTTOMRIGHT)
        {
            SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
            return TRUE;
        }
        SetCursor(LoadCursorW(NULL, IDC_ARROW));
        return TRUE;
    }

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
    WNDCLASSW wc;
    HWND hwnd;
    const wchar_t CLASS_NAME[] = L"DirectX11 Sample Window Class";
    MSG msg = { 0 };  // メッセージ構造体を初期化
    // メッセージループ
    DWORD lastTime = GetTickCount();

    // microUIコンテキストの初期化
    g_ctx = malloc(sizeof(mu_Context));
    if (!g_ctx)
    {
        MessageBoxW(NULL, L"Failed to allocate microUI context", L"Error", MB_OK);
        return 1;
    }

    memset(g_ctx, 0, sizeof(mu_Context));
    mu_init(g_ctx);

    g_ctx->text_width = text_width;
    g_ctx->text_height = text_height;
    
    // ウィンドウクラスの設定
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.style = CS_HREDRAW | CS_VREDRAW;

    RegisterClassW(&wc);

    // ウィンドウの作成
    hwnd = CreateWindowExW(
        0,                      // オプションのウィンドウスタイル
        CLASS_NAME,             // ウィンドウクラス
        L"DirectX11 MicroUI Application",  // ウィンドウタイトル
        WS_OVERLAPPEDWINDOW,   // ウィンドウスタイル
        CW_USEDEFAULT, CW_USEDEFAULT, // 位置
        width, height,   // サイズ
        NULL,       // 親ウィンドウ
        NULL,       // メニュー
        hInstance,  // インスタンスハンドル
        NULL        // 追加のアプリケーションデータ
    );

    if (hwnd == NULL)
    {
        MessageBoxW(NULL, L"Window Creation Failed!", L"Error", MB_OK);
        return 0;
    }

    // DirectX11の初期化
    HRESULT hr = dx11_init(hwnd, width, height);
    if (FAILED(hr))
    {
        wchar_t errorMsg[256];
        swprintf(errorMsg, 256, L"DirectX11 Initialization Failed! HRESULT: 0x%08X", (unsigned int)hr);
        MessageBoxW(NULL, errorMsg, L"Error", MB_OK);
        return 0;
    }

    // ウィンドウの表示
    ShowWindow(hwnd, nCmdShow);

    // メッセージループ
    while (TRUE)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            DWORD currentTime = GetTickCount();
            if (currentTime - lastTime > 16)
            {  // ~60FPS
                process_frame(g_ctx);
                render_microui(g_ctx);
                lastTime = currentTime;
            }
        }
    }

    // クリーンアップ処理
    dx11_cleanup();
    free(g_ctx);

    return 0;
}