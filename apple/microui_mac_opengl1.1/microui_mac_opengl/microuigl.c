#undef main
#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#endif
#if defined(__APPLE__) && defined(__MACH__)
#import <OpenGL/gl3.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <windowsx.h>  // GET_X_LPARAM, GET_Y_LPARAM用
#include "renderer.h"
#include "microui.h"

static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };
extern int width;
extern int height;
extern int BUFFER_SIZE;
void resize_buffers(int new_width, int new_height);

#ifndef WM_MOUSEWHEEL
#define WM_MOUSEWHEEL 0x020A
#endif

#ifndef WHEEL_DELTA
#define WHEEL_DELTA 120
#endif

#ifndef CLEARTYPE_QUALITY
#define CLEARTYPE_QUALITY 5
#endif

#ifndef AC_SRC_ALPHA
#define AC_SRC_ALPHA 1
#endif

#ifndef DC_PEN
#define DC_PEN 19
#endif

#ifndef DC_BRUSH
#define DC_BRUSH 18
#endif

#ifndef SetDCPenColor
#define SetDCPenColor(dc, color) SetTextColor(dc, color)
#endif

#ifndef SetDCBrushColor
#define SetDCBrushColor(dc, color) SetBkColor(dc, color)
#endif

#ifndef GdiGradientFill
#define GdiGradientFill(dc, pVertex, nVertex, pMesh, nMesh, mode) GradientFill(dc, pVertex, nVertex, pMesh, nMesh, mode)
#endif
#ifndef GET_WHEEL_DELTA_WPARAM
#define GET_WHEEL_DELTA_WPARAM(wParam)  ((short)HIWORD(wParam))
#endif
static void write_log(const char* text) {
    if (logbuf[0]) { strcat(logbuf, "\n"); }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

static void test_window(mu_Context* ctx)
{
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450)))
    {
        mu_Container* win;
        char buf[64];
        int layout1[2] = { 54, -1 };
        int layout2[3] = { 86, -110, -1 };
        int layout3[2] = { 140, -1 };
        int layout4[2] = { 54, 54 };
        int layout5[1] = { -1 };
        int layout6[2] = { -78, -1 };
        int layout7[2] = { 46, -1 };
        mu_Rect r;

        win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        /* window info */
        if (mu_header(ctx, "Window Info"))
        {
            win = mu_get_current_container(ctx);
            mu_layout_row(ctx, 2, layout1, 0);
            mu_label(ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* labels + buttons */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED))
        {
            mu_layout_row(ctx, 3, layout2, 0);
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
            mu_layout_row(ctx, 2, layout3, 0);
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
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2"))
            {
                mu_layout_row(ctx, 2, layout4, 0);
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
            mu_layout_row(ctx, 1, layout5, 0);
            mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(ctx);
        }

        /* background color sliders */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED))
        {
            mu_layout_row(ctx, 2, layout6, 74);
            /* sliders */
            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 2, layout7, 0);
            mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* color preview */
            r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}

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


static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high)
{
    static float tmp;
    int res;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}

static void style_window(mu_Context* ctx)
{
    static struct { const char* label; int idx; } colors[] = {
      { "text:",         MU_COLOR_TEXT        },
      { "border:",       MU_COLOR_BORDER      },
      { "windowbg:",     MU_COLOR_WINDOWBG    },
      { "titlebg:",      MU_COLOR_TITLEBG     },
      { "titletext:",    MU_COLOR_TITLETEXT   },
      { "panelbg:",      MU_COLOR_PANELBG     },
      { "button:",       MU_COLOR_BUTTON      },
      { "buttonhover:",  MU_COLOR_BUTTONHOVER },
      { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
      { "base:",         MU_COLOR_BASE        },
      { "basehover:",    MU_COLOR_BASEHOVER   },
      { "basefocus:",    MU_COLOR_BASEFOCUS   },
      { "scrollbase:",   MU_COLOR_SCROLLBASE  },
      { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
      { NULL }
    };

    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240)))
    {
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        int layout[6] = { 80, sw, sw, sw, sw, -1 };
        int i;

        mu_layout_row(ctx, 6, layout, 0);
        for (i = 0; colors[i].label; i++)
        {
            mu_label(ctx, colors[i].label);
            uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
            mu_draw_rect(ctx, mu_layout_next(ctx), ctx->style->colors[i]);
        }
        mu_end_window(ctx);
    }
}


static void process_frame(mu_Context* ctx) {
    mu_begin(ctx);
    style_window(ctx);
    log_window(ctx);
    test_window(ctx);
    mu_end(ctx);
}

static char button_map[256];
static char key_map[256];

void initialize_maps()
{
    button_map[VK_LBUTTON & 0xff] = MU_MOUSE_LEFT;
    button_map[VK_RBUTTON & 0xff] = MU_MOUSE_RIGHT;
    button_map[VK_MBUTTON & 0xff] = MU_MOUSE_MIDDLE;

    key_map[VK_SHIFT & 0xff] = MU_KEY_SHIFT;
    key_map[VK_CONTROL & 0xff] = MU_KEY_CTRL;
    key_map[VK_MENU & 0xff] = MU_KEY_ALT;
    key_map[VK_RETURN & 0xff] = MU_KEY_RETURN;
    key_map[VK_BACK & 0xff] = MU_KEY_BACKSPACE;
}

static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { len = strlen(text); }
    return r_get_text_width(text, len);
}

static int text_height(mu_Font font) {
    return r_get_text_height();
}

static mu_Context* ctx;

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
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

            // デバッグ出力
            //char debug[256];
            //sprintf(debug, "Transformed Mouse: x=%d, y=%d\n", x, y);
            //OutputDebugStringA(debug);

            mu_input_mousemove(ctx, x, y);
            return 0;
        }

        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN: {
            RECT rect;
            float scale_x;
            float scale_y;
            int button = 0;
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            if (uMsg == WM_LBUTTONDOWN) button = MU_MOUSE_LEFT;
            if (uMsg == WM_RBUTTONDOWN) button = MU_MOUSE_RIGHT;
            if (uMsg == WM_MBUTTONDOWN) button = MU_MOUSE_MIDDLE;
            
            SetCapture(hwnd);

            // 座標変換
            GetClientRect(hwnd, &rect);
            scale_x = (float)width / (float)rect.right;
            scale_y = (float)height / (float)rect.bottom;

            x = (int)(x * scale_x);
            y = (int)(y * scale_y);
            
            mu_input_mousedown(ctx, x, y, button);
            return 0;
        }

        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP: {
            float scale_x;
            float scale_y;
            RECT rect;
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            int button = 0;
            ReleaseCapture();
            if (uMsg == WM_LBUTTONUP) button = MU_MOUSE_LEFT;
            if (uMsg == WM_RBUTTONUP) button = MU_MOUSE_RIGHT;
            if (uMsg == WM_MBUTTONUP) button = MU_MOUSE_MIDDLE;


            // 座標変換
            GetClientRect(hwnd, &rect);
            scale_x = (float)width / (float)rect.right;
            scale_y = (float)height / (float)rect.bottom;

            x = (int)(x * scale_x);
            y = (int)(y * scale_y);
            
            mu_input_mouseup(ctx, x, y, button);
            return 0;
        }

        case WM_SETCURSOR: {
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }
    case WM_DESTROY:
    r_shutdown();  // OpenGLリソースの解放
        PostQuitMessage(0);
        return 0;

    case WM_SIZE: {
        RECT rect;
        int new_width = LOWORD(lParam);
        int new_height = HIWORD(lParam);
        // ウィンドウが最小化された場合
        if (new_width == 0 || new_height == 0)
        {
            return 0;
        }
        GetClientRect(hwnd, &rect);

        // widthとheightを更新
        width = rect.right;
        height = rect.bottom;

        glViewport(0, 0, rect.right, rect.bottom);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0.0f, width, height, 0.0f, -1.0f, 1.0f);
        glMatrixMode(GL_MODELVIEW);
        resize_buffers(new_width, new_height);

        // バッファの再初期化
        r_shutdown();
        r_init();

    }
            return 0;
    case WM_MOUSEWHEEL:
        mu_input_scroll(ctx, 0, GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA * -30);
        return 0;

    case WM_KEYDOWN:
    case WM_KEYUP: {
        int c = key_map[wParam & 0xff];
        if (c && uMsg == WM_KEYDOWN) { mu_input_keydown(ctx, c); }
        if (c && uMsg == WM_KEYUP) { mu_input_keyup(ctx, c); }
        return 0;
    }

    case WM_CHAR:
        mu_input_text(ctx, (const char*)&wParam);
        return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
typedef BOOL(APIENTRY* PFNWGLSWAPINTERVALEXTPROC)(int);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    WNDCLASS wc = { 0 };
    int pf;
    HGLRC hglrc;
    HWND hwnd;
    HDC hdc;
    MSG msg;
    DWORD last_time = GetTickCount();
    PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
    DWORD current_time;
    PIXELFORMATDESCRIPTOR pfd = {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER, // ダブルバッファ有効化
        PFD_TYPE_RGBA,
        32,
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        24,
        8,
        0,
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "MicroUI";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        0, "MicroUI", "MicroUI",
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
        NULL, NULL, hInstance, NULL
    );

    hdc = GetDC(hwnd);

    pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    hglrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hglrc);
    // VSYNCを有効化
    wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");
    if (wglSwapIntervalEXT)
    {
        wglSwapIntervalEXT(1);
    }
    /* init OpenGL */
    r_init();

    /* init microui */
    ctx = malloc(sizeof(mu_Context));
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    /* initialize maps */
    initialize_maps();

    /* main loop */
    while (1)
    {
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT) { return 0; }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }

        /* フレーム処理 */
        current_time = GetTickCount();
        if (current_time - last_time >= 16)
        {
            RECT rect;
			mu_Command* cmd = NULL;
            last_time = current_time;

            /* UI更新 */
            process_frame(ctx);

            /* レンダリング */
            GetClientRect(hwnd, &rect);
            glViewport(0, 0, rect.right, rect.bottom);
            glClearColor(bg[0] / 255.0f, bg[1] / 255.0f, bg[2] / 255.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            r_clear(mu_color(bg[0], bg[1], bg[2], 255));


            while (mu_next_command(ctx, &cmd))
            {
                switch (cmd->type)
                {
                case MU_COMMAND_TEXT:
                    //OutputDebugStringW(L"Processing text command\n"); 
                    r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
                case MU_COMMAND_RECT:
                    //OutputDebugStringW(L"Processing rect command\n"); 
                    r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
                case MU_COMMAND_ICON:
                    //OutputDebugStringW(L"Processing icon command\n"); 
                    r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
                case MU_COMMAND_CLIP:
                    //OutputDebugStringW(L"Processing clip command\n"); 
                    r_set_clip_rect(cmd->clip.rect); break;
                }
            }

            /* フレーム終了処理 */
            r_present();
        }
        else
        {
            Sleep(1);
        }
    }

    return 0;
}
