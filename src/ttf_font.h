/**
 * TTFフォント読み込み・レンダリングモジュール (microui用)
 * Nuklearのフォント機能からの移植
 */
#ifndef TTF_FONT_H
#define TTF_FONT_H

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

/* フォント管理用グローバルコンテキスト */
extern mu_font_atlas g_font_atlas;
extern void create_ttf_font_texture(void);

/* TTFフォント読み込み・レンダリング関数群 */
void mu_font_stash_begin(void);
int mu_font_add_from_file(const char* path, float size);
void mu_font_stash_end(void);

/* グリフ検索関数 */
struct mu_font_glyph* mu_font_find_glyph(unsigned int codepoint);

extern const unsigned char white_patch[3 * 3];
extern const unsigned char close_patch[16 * 16];
extern const unsigned char check_patch[18 * 18];
extern const unsigned char expanded_patch[7 * 5];
extern const unsigned char collapsed_patch[5 * 7];

#endif /* TTF_FONT_H */
