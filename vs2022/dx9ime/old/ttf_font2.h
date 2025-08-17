/**
 * TTFフォント読み込み・レンダリングモジュール (microui用)
 * Nuklearのフォント機能からの移植
 */
#ifndef TTF_FONT_H
#define TTF_FONT_H

#include <d3d9.h>

/* mu_Rect構造体（microui.hと同じ定義） */

// mu_font_glyph構造体を外部定義
struct mu_font_glyph {
    unsigned int codepoint;   /* 文字コード */
    short x, y, w, h;         /* アトラスでの位置とサイズ */
    short xadvance;           /* 次の文字へのX方向の進み */
    short xoff, yoff;         /* オフセット */
};

// フォントアトラス構造体
typedef struct mu_font_atlas {
    void* pixel;              /* フォントアトラス画像のバッファ */
    int width, height;        /* アトラス画像のサイズ */
    int glyph_count;          /* グリフの数 */
    int line_height;          /* フォントの行の高さ */
    struct mu_font_glyph glyphs[0xFFFF]; /* グリフ情報の配列 */
} mu_font_atlas;
// UI用UV座標（外部参照用）
extern float g_ui_white_uv[4];
extern float g_ui_icon_uv[4];

/* フォント管理用グローバルコンテキスト */
extern mu_font_atlas g_font_atlas;
extern IDirect3DTexture9* g_font_texture;

/* TTFフォント読み込み・レンダリング関数群 */
void mu_font_stash_begin(IDirect3DDevice9* device);
int mu_font_add_from_file(const char* path, float size);
void mu_font_stash_end(void);

/* グリフ検索関数 */
struct mu_font_glyph* mu_font_find_glyph(unsigned int codepoint);

#endif /* TTF_FONT_H */
