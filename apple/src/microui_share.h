#import <TargetConditionals.h>
#include <stdio.h>
#include <string.h>
#include "microui.h"

// --- プラットフォームごとのヘッダー・マクロ切り替え ---
#if TARGET_OS_IPHONE
    // iOS: カスタムラッパーを使う
    #define HEADER_FN ios_header
    #define HEADER_FN_EX ios_header_ex
    #define UINT8_SLIDER_FN ios_uint8_slider
    #define TEXTBOX_FN ios_textbox
    // 必要なカスタムラッパーのヘッダーをinclude
#else
    // macOS: 標準MicroUI関数を使う
    #define HEADER_FN mu_header
    #define HEADER_FN_EX mu_header_ex
    #define UINT8_SLIDER_FN uint8_slider
    #define TEXTBOX_FN mu_textbox
#endif
typedef struct {
    mu_Id id;
    mu_Rect rect;
} SliderCache;
static SliderCache sliderCache[32];
static int sliderCacheCount = 0;
typedef struct {
    mu_Id id;
    mu_Rect rect;
} TextboxCache;
static TextboxCache textboxCache[16];
static int textboxCacheCount = 0;

#ifdef __cplusplus
extern "C" {
#endif


// --- iOS用カスタムラッパー関数プロトタイプ宣言 ---
// static実装だが、ここではextern相当で宣言のみ
int ios_header(mu_Context* ctx, const char* label, int opt);
int ios_header_ex(mu_Context* ctx, const char* label, int opt);
int ios_uint8_slider(mu_Context* ctx, unsigned char* value, int min, int max);
int ios_textbox(mu_Context* ctx, char* buf, int bufsz);
void ios_clear_slider_cache(void);
void ios_clear_textbox_cache(void);
mu_Id ios_find_slider_at_point(int x, int y);
mu_Id ios_find_textbox_at_point(int x, int y);
void ios_fix_hover_state(mu_Context* ctx);
mu_Id ios_get_header_id(mu_Context* ctx, const char* label);
 void ios_cache_slider(mu_Context* ctx, mu_Id id, mu_Rect rect);
#if defined(USE_OPENGL)
void ios_toggle_header_state(mu_Context* ctx, mu_Id id, const char* label);
#else
void ios_toggle_header_state(mu_Context* ctx, mu_Id id);
#endif

#ifdef __cplusplus
}
#endif
// --- ログバッファ ---
#define LOGBUF_SIZE 64000
static char logbuf[LOGBUF_SIZE];
static int logbuf_updated = 0;

// ログ書き込み（Xcodeデバッグログに出力）
static void write_log(const char* msg) {
    if (!msg) return;
    NSLog(@"%s", msg);
}

// 標準uint8スライダー
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

// --- 共通ウィジェット描画 ---
void draw_demo_windows(mu_Context* ctx, unsigned char bg[3]) {
    // Demo Window
    if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 250))) {
        mu_Container* win = mu_get_current_container(ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        // Window Info
        if (HEADER_FN(ctx, "Window Info", 0)) {
            mu_layout_row(ctx, 2, (int[]){54, -1}, 0);
            mu_label(ctx, "Position:");
            char buf[32]; snprintf(buf, sizeof(buf), "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
            mu_label(ctx, "Size:");
            snprintf(buf, sizeof(buf), "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
        }

        // Test Buttons
        if (HEADER_FN_EX(ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 3, (int[]){86, -110, -1}, 0);
            mu_label(ctx, "Test buttons 1:");
            if (mu_button(ctx, "Button 1")) write_log("Pressed button 1");
            if (mu_button(ctx, "Button 2")) write_log("Pressed button 2");
        }

        // Background Color
        if (HEADER_FN_EX(ctx, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(ctx, 2, (int[]){-78, -1}, 0);
            mu_label(ctx, "Red:");   UINT8_SLIDER_FN(ctx, &bg[0], 0, 255);
            mu_label(ctx, "Green:"); UINT8_SLIDER_FN(ctx, &bg[1], 0, 255);
            mu_label(ctx, "Blue:");  UINT8_SLIDER_FN(ctx, &bg[2], 0, 255);
        }

        mu_end_window(ctx);
    }

    // Log Window
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
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

        mu_layout_row(ctx, 2, layout2, 0);
        if (TEXTBOX_FN(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) submitted = 1;
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }
        mu_layout_row(ctx, 1, (int[]){-1}, 0);
        if (mu_button(ctx, "Clear")) {
            logbuf[0] = '\0';
        }
        mu_end_window(ctx);
    }
}
 int ios_uint8_slider(mu_Context* ctx, unsigned char* value, int min, int max) {
    int res = mu_slider(ctx, value, min, max);
    ios_cache_slider(ctx, ctx->last_id, ctx->last_rect);
    return res;
}
 void ios_cache_slider(mu_Context* ctx, mu_Id id, mu_Rect rect) {
    if (sliderCacheCount < 32) {
        sliderCache[sliderCacheCount].id = id;
        sliderCache[sliderCacheCount].rect = rect;
        sliderCacheCount++;
    }
}
 int ios_header_ex(mu_Context* ctx, const char* label, int opt) {
    if (!ctx || !label) return 0;
    
    // 標準のヘッダー処理を実行
    return mu_header_ex(ctx, label, opt);
}
static void ios_cache_textbox(mu_Context* ctx, mu_Id id, mu_Rect rect) {
    if (textboxCacheCount < 16) {
        textboxCache[textboxCacheCount].id = id;
        textboxCache[textboxCacheCount].rect = rect;
        textboxCacheCount++;
    }
}
int ios_textbox(mu_Context* ctx, char* buf, int bufsz) {
    int res = mu_textbox(ctx, buf, bufsz);
    ios_cache_textbox(ctx, ctx->last_id, ctx->last_rect);
    return res;
}