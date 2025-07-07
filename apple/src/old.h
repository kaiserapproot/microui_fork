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

#ifdef __cplusplus
extern "C" {
#endif

// MicroUI用のヘッダー矩形キャッシュ（描画中にヘッダー矩形情報を保存）
typedef struct {
   mu_Id id;
   mu_Rect rect;
   char label[64];
   int active;
} HeaderCache;

// --- iOS拡張プロトタイプ宣言 ---
void ios_clear_slider_cache(void);
void ios_clear_textbox_cache(void);
void ios_cache_slider(mu_Context* ctx, mu_Id id, mu_Rect rect);
void ios_cache_textbox(mu_Context* ctx, mu_Id id, mu_Rect rect);
mu_Id ios_find_slider_at_point(int x, int y);
mu_Id ios_find_textbox_at_point(int x, int y);
void ios_fix_hover_state(mu_Context* ctx);
void ios_set_hover(mu_Context* ctx, mu_Id id);
void ios_init_header_states(void);
void ios_set_header_state(mu_Id id, const char* label, int expanded);
int  ios_get_header_state(mu_Id id, int* outExpanded);
void ios_clear_header_cache(void);
void ios_cache_header(mu_Context* ctx, const char* label, mu_Rect rect, int active);
int  ios_find_header_at_position(int x, int y, HeaderCache** outCache);
mu_Id ios_get_header_id(mu_Context* ctx, const char* label);
int  ios_header_wrapper(mu_Context* ctx, const char* label, int istreenode, int opt);
int  ios_header(mu_Context* ctx, const char* label, int opt);
int  ios_header_ex(mu_Context* ctx, const char* label, int opt);
int  ios_uint8_slider(mu_Context* ctx, unsigned char* value, int min, int max);
int  ios_textbox(mu_Context* ctx, char* buf, int bufsz);
#ifdef USE_OPENGL
void ios_toggle_header_state(mu_Context* ctx, mu_Id id, const char* label);
#else
void ios_toggle_header_state(mu_Context* ctx, mu_Id id);
#endif

#ifdef __cplusplus
}
#endif

// --- iOS拡張用構造体・変数宣言 ---
typedef struct { mu_Id id; mu_Rect rect; } SliderCache;
typedef struct { mu_Id id; mu_Rect rect; } TextboxCache;
typedef struct { mu_Id id; char label[64]; int expanded; } PersistentHeaderState;

SliderCache sliderCache[32];
int sliderCacheCount;
TextboxCache textboxCache[16];
int textboxCacheCount;
PersistentHeaderState persistentHeaders[50];
int persistentHeaderCount;
HeaderCache headerCache[20];
int headerCacheCount;

// --- iOS拡張インライン実装 ---
  void ios_clear_slider_cache(void) { sliderCacheCount = 0; }
  void ios_clear_textbox_cache(void) { textboxCacheCount = 0; }
  void ios_cache_slider(mu_Context* ctx, mu_Id id, mu_Rect rect) {
    if (sliderCacheCount < 32) {
        sliderCache[sliderCacheCount].id = id;
        sliderCache[sliderCacheCount].rect = rect;
        sliderCacheCount++;
    }
}
  void ios_cache_textbox(mu_Context* ctx, mu_Id id, mu_Rect rect) {
    if (textboxCacheCount < 16) {
        textboxCache[textboxCacheCount].id = id;
        textboxCache[textboxCacheCount].rect = rect;
        textboxCacheCount++;
    }
}
  mu_Id ios_find_slider_at_point(int x, int y) {
    for (int i = 0; i < sliderCacheCount; i++) {
        mu_Rect r = sliderCache[i].rect;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            return sliderCache[i].id;
        }
    }
    return 0;
}
  mu_Id ios_find_textbox_at_point(int x, int y) {
    for (int i = 0; i < textboxCacheCount; i++) {
        mu_Rect r = textboxCache[i].rect;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            return textboxCache[i].id;
        }
    }
    return 0;
}
  void ios_fix_hover_state(mu_Context* ctx) {
    if (!ctx) return;
    if (ctx->hover < 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
        return;
    }
    if (ctx->mouse_down && ctx->focus != 0) {
        ctx->hover = ctx->focus;
        ctx->prev_hover = ctx->focus;
    } else if (!ctx->mouse_down && ctx->mouse_pressed == 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
    }
}
  void ios_set_hover(mu_Context* ctx, mu_Id id) {
    if (!ctx) return;
    if (ctx->hover == id) return;
    if (id == 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
    } else {
        ctx->hover = id;
        ctx->prev_hover = id;
    }
}
  void ios_init_header_states(void) {
    persistentHeaderCount = 0;
    memset(persistentHeaders, 0, sizeof(persistentHeaders));
}
  void ios_set_header_state(mu_Id id, const char* label, int expanded) {
    if (id == 0) return;
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            persistentHeaders[i].expanded = expanded ? 1 : 0;
            return;
        }
    }
    if (persistentHeaderCount < 50) {
        persistentHeaders[persistentHeaderCount].id = id;
        if (label) {
            strncpy(persistentHeaders[persistentHeaderCount].label, label, 63);
            persistentHeaders[persistentHeaderCount].label[63] = '\0';
        }
        persistentHeaders[persistentHeaderCount].expanded = expanded ? 1 : 0;
        persistentHeaderCount++;
    }
}
  int ios_get_header_state(mu_Id id, int* outExpanded) {
    if (id == 0 || !outExpanded) return 0;
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            *outExpanded = persistentHeaders[i].expanded;
            return 1;
        }
    }
    *outExpanded = 0;
    return 0;
}
  void ios_clear_header_cache(void) { headerCacheCount = 0; }
  void ios_cache_header(mu_Context* ctx, const char* label, mu_Rect rect, int active) {
    if (headerCacheCount < 20 && label) {
        mu_Id id = mu_get_id(ctx, label, strlen(label));
        HeaderCache* cache = &headerCache[headerCacheCount++];
        cache->id = id;
        cache->rect = rect;
        strncpy(cache->label, label, 63);
        cache->label[63] = '\0';
        cache->active = active;
    }
}
  int ios_find_header_at_position(int x, int y, HeaderCache** outCache) {
    for (int i = 0; i < headerCacheCount; i++) {
        HeaderCache* cache = &headerCache[i];
        if (x >= cache->rect.x && x < cache->rect.x + cache->rect.w &&
            y >= cache->rect.y && y < cache->rect.y + cache->rect.h) {
            if (outCache) *outCache = cache;
            return 1;
        }
    }
    return 0;
}
  mu_Id ios_get_header_id(mu_Context* ctx, const char* label) {
    return mu_get_id(ctx, label, strlen(label));
}
  int ios_header_wrapper(mu_Context* ctx, const char* label, int istreenode, int opt) {
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    int expanded = 0;
    ios_get_header_state(id, &expanded);
    if (expanded) opt |= MU_OPT_EXPANDED;
    else opt &= ~MU_OPT_EXPANDED;
    int result = istreenode ? mu_begin_treenode_ex(ctx, label, opt) : mu_header_ex(ctx, label, opt);
    ios_cache_header(ctx, label, ctx->last_rect, result);
    if (result != expanded) ios_set_header_state(id, label, result != 0);
    return result;
}
  int ios_header(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 0, opt);
}
  int ios_header_ex(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 0, opt);
}
  int ios_uint8_slider(mu_Context* ctx, unsigned char* value, int min, int max) {
    int res = mu_slider(ctx, value, min, max);
    ios_cache_slider(ctx, ctx->last_id, ctx->last_rect);
    return res;
}
  int ios_textbox(mu_Context* ctx, char* buf, int bufsz) {
    int res = mu_textbox(ctx, buf, bufsz);
    ios_cache_textbox(ctx, ctx->last_id, ctx->last_rect);
    return res;
}
#ifdef USE_OPENGL
  void ios_toggle_header_state(mu_Context* ctx, mu_Id id, const char* label) {
    int expanded = 0;
    ios_get_header_state(id, &expanded);
    int newState = !expanded;
    ios_set_header_state(id, label, newState);
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    if (idx >= 0) {
        ctx->treenode_pool[idx].last_update = newState ? 1 : 0;
    } else {
        idx = mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
        if (idx >= 0) ctx->treenode_pool[idx].last_update = newState ? 1 : 0;
    }
}
#else
  void ios_toggle_header_state(mu_Context* ctx, mu_Id id) {
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    if (idx >= 0) {
        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
    } else {
        mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    }
}
#endif

// --- ログバッファ ---
#define LOGBUF_SIZE 64000
 char logbuf[LOGBUF_SIZE];
 int logbuf_updated = 0;

// ログ書き込み
 void write_log(const char* msg) {
if (!msg) return;
    size_t len = strlen(logbuf);
    size_t msglen = strlen(msg);
    if (len + msglen + 2 >= LOGBUF_SIZE) {
        logbuf[0] = '\0';
        len = 0;
    }
    snprintf(logbuf + len, LOGBUF_SIZE - len, "%s\n", msg);
    logbuf_updated = 1;
}

// 標準uint8スライダー
 int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
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