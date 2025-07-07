#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <GL/glx.h>
#include <unistd.h>
#include <sys/time.h>
#include <stdint.h> 
#include "microui.h"

// atlas.inlをインクルードしてフォントデータとアトラスを使用
#include "atlas.inl"

// ウィンドウとUI関連のグローバル変数
static int width = 800;
static int height = 600;
static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[3] = { 90, 95, 100 };

// テクスチャID
static GLuint font_texture;

// テキスト幅計算関数も修正 - スペースの処理を適切に行う
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { 
        len = strlen(text); 
    }
    
    int res = 0;
    for (int i = 0; i < len; i++) {
        int chr = (unsigned char) text[i];
        if (chr == ' ') {
            // スペースの幅を明示的に設定
            res += 6; // スペースの幅（上のr_draw_textと一致させる）
        } else if (chr < 32 || chr >= 127) {
            // 未対応文字の処理
            res += atlas[ATLAS_FONT].w; // デフォルト文字幅
        } else {
            chr -= 32;
            res += atlas[ATLAS_FONT + chr].w;
        }
    }
    return res;
}

static int text_height(mu_Font font) {
    return 16; // 18から16に調整することも検討する
}

// 描画コマンド関連の関数
static void r_init(void);
static void r_draw_rect(mu_Rect rect, mu_Color color);
static void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color);
static void r_draw_icon(int id, mu_Rect rect, mu_Color color);
static void r_set_clip_rect(mu_Rect rect);
static void r_clear(mu_Color color);
static void r_present(void);
static void r_shutdown(void);

// X11とOpenGLのグローバル変数
static Display* display;
static Window window;
static GLXContext gl_context;
static Colormap colormap;
static XVisualInfo* visual_info;
static Atom wm_delete_window;

// イベント処理用の変数
static int key_map[256];
static int quit = 0;

// ログウィンドウに文字列を追加
static void write_log(const char* text) {
    if (logbuf[0]) { 
        strcat(logbuf, "\n"); 
    }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

// テストウィンドウの描画
static void test_window(mu_Context* ctx) {
    /* do window */
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
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

        /* ウィンドウ情報 */
        if (mu_header(ctx, "Window Info")) {
            win = mu_get_current_container(ctx);
            mu_layout_row(ctx, 2, layout1, 0);
            mu_label(ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        /* ラベルとボタン */
        if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 3, layout2, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
            if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
            mu_label(ctx, "Test buttons 2:");
            if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
            if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
            if (mu_begin_popup(ctx, "Test Popup")) {
                mu_button(ctx, "Hello");
                mu_button(ctx, "World");
                mu_end_popup(ctx);
            }
        }

        /* ツリーとテキスト */
        if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 2, layout3, 0);
            mu_layout_begin_column(ctx);
            if (mu_begin_treenode(ctx, "Test 1")) {
                if (mu_begin_treenode(ctx, "Test 1a")) {
                    mu_label(ctx, "Hello");
                    mu_label(ctx, "world");
                    mu_end_treenode(ctx);
                }
                if (mu_begin_treenode(ctx, "Test 1b")) {
                    if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
                    if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
                    mu_end_treenode(ctx);
                }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 2")) {
                mu_layout_row(ctx, 2, layout4, 0);
                if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
                if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
                if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
                if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
                mu_end_treenode(ctx);
            }
            if (mu_begin_treenode(ctx, "Test 3")) {
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

        /* 背景色スライダー */
        if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 2, layout6, 74);
            /* スライダー */
            mu_layout_begin_column(ctx);
            mu_layout_row(ctx, 2, layout7, 0);
            mu_label(ctx, "Red:"); mu_slider(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:"); mu_slider(ctx, &bg[2], 0, 255);
            mu_layout_end_column(ctx);
            /* カラープレビュー */
            r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(ctx);
    }
}

// ログウィンドウの描画
static void log_window(mu_Context* ctx) {
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        /* テキスト出力パネル */
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
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        /* テキストボックスとサブミットボタン */
        mu_layout_row(ctx, 2, layout2, 0);
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }

        mu_end_window(ctx);
    }
}

// スタイルエディタウィンドウの描画
static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
    static float tmp;
    int res;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
}

static void style_window(mu_Context* ctx) {
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

    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        int layout[6] = { 80, sw, sw, sw, sw, -1 };
        int i;

        mu_layout_row(ctx, 6, layout, 0);
        for (i = 0; colors[i].label; i++) {
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

// フレーム処理
static void process_frame(mu_Context* ctx) {
    mu_begin(ctx);
    style_window(ctx);
    log_window(ctx);
    test_window(ctx);
    mu_end(ctx);
}

// OpenGLの初期化
static void r_init(void) {
    // main関数内のr_init()の直後に追加
    // フォントテクスチャのデバッグ情報
    printf("フォントテクスチャID: %u\n", font_texture);
    printf("アトラス幅: %d, 高さ: %d\n", ATLAS_WIDTH, ATLAS_HEIGHT);
    printf("最初の文字のアトラス情報 - x:%d y:%d w:%d h:%d\n", 
       atlas[ATLAS_FONT].x, atlas[ATLAS_FONT].y, 
       atlas[ATLAS_FONT].w, atlas[ATLAS_FONT].h);
    // OpenGLの初期設定
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    // フォントテクスチャの初期化
    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    
    // atlas.inlからフォントテクスチャデータをロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
                
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
}

static void r_clear(mu_Color color) {
    glClearColor(color.r / 255.0, color.g / 255.0, color.b / 255.0, color.a / 255.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void r_draw_rect(mu_Rect rect, mu_Color color) {
    float x = rect.x;
    float y = rect.y;
    float w = rect.w;
    float h = rect.h;
    float r = color.r / 255.0;
    float g = color.g / 255.0;
    float b = color.b / 255.0;
    float a = color.a / 255.0;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// テキスト描画関数を renderer.c と同様のアプローチで実装
static void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color) {
    float r = color.r / 255.0;
    float g = color.g / 255.0;
    float b = color.b / 255.0;
    float a = color.a / 255.0;
    
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    const char* p;
    int chr;
    mu_Rect src;
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    
    for (p = text; *p; p++) {
        // UTF-8対応（マルチバイト文字の2バイト目以降をスキップ）
        if ((*p & 0xc0) == 0x80) { continue; }
        
        // ASCII範囲に制限
        chr = mu_min((unsigned char)*p, 127);
        
        // アトラスから文字の矩形情報を取得
        src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        
        // 一文字ずつ直接描画（push_quadの内部処理を展開）
        float x, y, w, h;
        
        // テクスチャ座標の計算
        x = (float)src.x / ATLAS_WIDTH;
        y = (float)src.y / ATLAS_HEIGHT;
        w = (float)src.w / ATLAS_WIDTH;
        h = (float)src.h / ATLAS_HEIGHT;
        
        // 頂点とテクスチャ座標を設定してクアッドを描画
        glBegin(GL_QUADS);
        glColor4f(r, g, b, a);
        
        // 左上
        glTexCoord2f(x, y);
        glVertex2f(dst.x, dst.y);
        
        // 右上
        glTexCoord2f(x + w, y);
        glVertex2f(dst.x + dst.w, dst.y);
        
        // 右下
        glTexCoord2f(x + w, y + h);
        glVertex2f(dst.x + dst.w, dst.y + dst.h);
        
        // 左下
        glTexCoord2f(x, y + h);
        glVertex2f(dst.x, dst.y + dst.h);
        
        glEnd();
        
        // 次の文字位置へ移動
        dst.x += dst.w;
    }
    
    glDisable(GL_TEXTURE_2D);
}

// アイコン描画関数も同様に調整
static void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    if (id < 0) return; // 無効なアイコンIDをチェック
    
    float r = color.r / 255.0;
    float g = color.g / 255.0;
    float b = color.b / 255.0;
    float a = color.a / 255.0;
    
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    mu_Rect dst = { x, y, src.w, src.h };
    
    // テクスチャ座標の計算
    float tx = (float)src.x / ATLAS_WIDTH;
    float ty = (float)src.y / ATLAS_HEIGHT;
    float tw = (float)src.w / ATLAS_WIDTH;
    float th = (float)src.h / ATLAS_HEIGHT;
    
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    
    // 左上
    glTexCoord2f(tx, ty);
    glVertex2f(dst.x, dst.y);
    
    // 右上
    glTexCoord2f(tx + tw, ty);
    glVertex2f(dst.x + dst.w, dst.y);
    
    // 右下
    glTexCoord2f(tx + tw, ty + th);
    glVertex2f(dst.x + dst.w, dst.y + dst.h);
    
    // 左下
    glTexCoord2f(tx, ty + th);
    glVertex2f(dst.x, dst.y + dst.h);
    
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}
static void r_set_clip_rect(mu_Rect rect) {
    glScissor(rect.x, height - (rect.y + rect.h), rect.w, rect.h);
    glEnable(GL_SCISSOR_TEST);
}

static void r_present(void) {
    glDisable(GL_SCISSOR_TEST);
    glXSwapBuffers(display, window);
}

static void r_shutdown(void) {
    // テクスチャの解放
    glDeleteTextures(1, &font_texture);
}

// キーマップの初期化
static void init_keymap(void) {
    key_map[XK_Shift_L & 0xff] = MU_KEY_SHIFT;
    key_map[XK_Shift_R & 0xff] = MU_KEY_SHIFT;
    key_map[XK_Control_L & 0xff] = MU_KEY_CTRL;
    key_map[XK_Control_R & 0xff] = MU_KEY_CTRL;
    key_map[XK_Alt_L & 0xff] = MU_KEY_ALT;
    key_map[XK_Alt_R & 0xff] = MU_KEY_ALT;
    key_map[XK_Return & 0xff] = MU_KEY_RETURN;
    key_map[XK_BackSpace & 0xff] = MU_KEY_BACKSPACE;
}

// ミリ秒単位の時間取得
static unsigned long get_time_ms(void) {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000) + (tv.tv_usec / 1000);
}

// GLXのバージョンチェック関数を追加
static int check_glx_version(Display* display) {
    int glx_major, glx_minor;
    
    // GLXバージョンの取得
    if (!glXQueryVersion(display, &glx_major, &glx_minor)) {
        fprintf(stderr, "GLX not supported\n");
        return 0;
    }
    
    printf("GLX version: %d.%d\n", glx_major, glx_minor);
    return (glx_major > 1) || (glx_major == 1 && glx_minor >= 3);
}

int main(int argc, char** argv) {
    mu_Context* ctx;
    XSetWindowAttributes window_attr;
    XEvent event;
    unsigned long last_time;
    unsigned long current_time;
    int glx_attr[] = {
        GLX_RGBA,
        GLX_DOUBLEBUFFER,
        GLX_DEPTH_SIZE, 24,
        GLX_RED_SIZE, 8,
        GLX_GREEN_SIZE, 8,
        GLX_BLUE_SIZE, 8,
        None
    };

    // X11の初期化
    display = XOpenDisplay(NULL);
    if (!display) {
        fprintf(stderr, "Error: XOpenDisplay failed\n");
        return 1;
    }

    // GLXバージョンを確認
    check_glx_version(display);

    // GLX VisualInfoの取得
    visual_info = glXChooseVisual(display, DefaultScreen(display), glx_attr);
    if (!visual_info) {
        fprintf(stderr, "Error: glXChooseVisual failed\n");
        XCloseDisplay(display);
        return 1;
    }

    // カラーマップの作成
    colormap = XCreateColormap(display, RootWindow(display, visual_info->screen),
                              visual_info->visual, AllocNone);

    // ウィンドウ属性の設定
    window_attr.colormap = colormap;
    window_attr.border_pixel = 0;
    window_attr.event_mask = ExposureMask | KeyPressMask | KeyReleaseMask |
                           ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                           StructureNotifyMask;

    // ウィンドウの作成
    window = XCreateWindow(display, RootWindow(display, visual_info->screen),
                          0, 0, width, height, 0, visual_info->depth, InputOutput,
                          visual_info->visual, CWBorderPixel | CWColormap | CWEventMask, &window_attr);

    // ウィンドウが表示されるのを待つ
    XStoreName(display, window, "MicroUI X11 Demo");
    XMapWindow(display, window);
    XFlush(display);
    
    // WM_DELETE_WINDOWプロトコルの設定
    wm_delete_window = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(display, window, &wm_delete_window, 1);

    // OpenGLコンテキストの作成 - 修正部分
    gl_context = glXCreateContext(display, visual_info, NULL, True);
    if (!gl_context) {
        fprintf(stderr, "Error: Failed to create OpenGL context\n");
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return 1;
    }

    // コンテキストをウィンドウに関連付け
    if (!glXMakeCurrent(display, window, gl_context)) {
        fprintf(stderr, "Error: Failed to make context current\n");
        glXDestroyContext(display, gl_context);
        XDestroyWindow(display, window);
        XFreeColormap(display, colormap);
        XCloseDisplay(display);
        return 1;
    }

    // OpenGLのビューポート設定
    glViewport(0, 0, width, height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, width, height, 0, -1.0, 1.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // MicroUIとレンダラの初期化
    r_init();
    ctx = (mu_Context*)malloc(sizeof(mu_Context));
    mu_init(ctx);
    ctx->text_width = text_width;
    ctx->text_height = text_height;

    // キーマップの初期化
    init_keymap();

    // メインループ
    last_time = get_time_ms();
    while (!quit) {
        // イベント処理
        while (XPending(display)) {
            XNextEvent(display, &event);

            switch (event.type) {
                case Expose: {
                    XWindowAttributes gwa;
                    XGetWindowAttributes(display, window, &gwa);
                    width = gwa.width;
                    height = gwa.height;
                    glViewport(0, 0, width, height);
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glOrtho(0, width, height, 0, -1.0, 1.0);
                    break;
                }

                case ConfigureNotify: {
                    width = event.xconfigure.width;
                    height = event.xconfigure.height;
                    glViewport(0, 0, width, height);
                    glMatrixMode(GL_PROJECTION);
                    glLoadIdentity();
                    glOrtho(0, width, height, 0, -1.0, 1.0);
                    break;
                }

                case MotionNotify:
                    mu_input_mousemove(ctx, event.xmotion.x, event.xmotion.y);
                    break;

                case ButtonPress:
                    if (event.xbutton.button == Button1) {
                        mu_input_mousedown(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_LEFT);
                    } else if (event.xbutton.button == Button3) {
                        mu_input_mousedown(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_RIGHT);
                    } else if (event.xbutton.button == Button2) {
                        mu_input_mousedown(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_MIDDLE);
                    } else if (event.xbutton.button == Button4) {
                        mu_input_scroll(ctx, 0, -10); // スクロールアップ
                    } else if (event.xbutton.button == Button5) {
                        mu_input_scroll(ctx, 0, 10);  // スクロールダウン
                    }
                    break;

                case ButtonRelease:
                    if (event.xbutton.button == Button1) {
                        mu_input_mouseup(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_LEFT);
                    } else if (event.xbutton.button == Button3) {
                        mu_input_mouseup(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_RIGHT);
                    } else if (event.xbutton.button == Button2) {
                        mu_input_mouseup(ctx, event.xbutton.x, event.xbutton.y, MU_MOUSE_MIDDLE);
                    }
                    break;

                case KeyPress: {
                    char buf[32];
                    KeySym sym;
                    int len = XLookupString(&event.xkey, buf, sizeof(buf), &sym, NULL);
                    
                    // マップされたキーがあれば処理
                    int key = key_map[sym & 0xff];
                    if (key) {
                        mu_input_keydown(ctx, key);
                    }
                    
                    // テキスト入力処理
                    if (len > 0) {
                        buf[len] = '\0';
                        mu_input_text(ctx, buf);
                    }
                    
                    // ESCキーで終了
                    if (sym == XK_Escape) {
                        quit = 1;
                    }
                    break;
                }

                case KeyRelease: {
                    KeySym sym = XLookupKeysym(&event.xkey, 0);
                    int key = key_map[sym & 0xff];
                    if (key) {
                        mu_input_keyup(ctx, key);
                    }
                    break;
                }

                case ClientMessage:
                    if ((Atom)event.xclient.data.l[0] == wm_delete_window) {
                        quit = 1;
                    }
                    break;
            }
        }

        // フレームレート制御
        current_time = get_time_ms();
        if (current_time - last_time >= 16) {  // 約60FPS
            last_time = current_time;

            // UIの更新と描画
            process_frame(ctx);

            // レンダリング
            r_clear(mu_color(bg[0], bg[1], bg[2], 255));
            
            // microuiコマンドの処理
            mu_Command* cmd = NULL;
            while (mu_next_command(ctx, &cmd)) {
                switch (cmd->type) {
                    case MU_COMMAND_TEXT: r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color); break;
                    case MU_COMMAND_RECT: r_draw_rect(cmd->rect.rect, cmd->rect.color); break;
                    case MU_COMMAND_ICON: r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color); break;
                    case MU_COMMAND_CLIP: r_set_clip_rect(cmd->clip.rect); break;
                }
            }

            r_present();
        } else {
            // CPUを占有しないようにスリープ
            usleep(1000);
        }
    }

    // クリーンアップ
    r_shutdown();
    free(ctx);
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, gl_context);
    XDestroyWindow(display, window);
    XFreeColormap(display, colormap);
    XCloseDisplay(display);

    return 0;
}