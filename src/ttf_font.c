// TTFアトラス内のUI用領域（右下に確保）
// UIパッチ座標は関数内ローカル変数で管理
#define UI_PATCH_W 32
#define UI_PATCH_H 32
// UI用UV座標（外部参照用）
float g_ui_white_uv[4] = {0};
float g_ui_icon_uv[4] = {0};
int g_ui_white_rect[4] = {0}; // {x, y, w, h}

// アイコン定数の定義（microui.hと同じ）
#define MU_ICON_CLOSE 1
#define MU_ICON_CHECK 2
#define MU_ICON_COLLAPSED 3
#define MU_ICON_EXPANDED 4
#define MU_ICON_MAX 5
#define ATLAS_WHITE MU_ICON_MAX
/**
 * TTFフォント読み込み・レンダリングモジュール (microui用)
 * Nuklearのフォント機能からの移植
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <Windows.h>
//#include "d3d9.h"
#include "ttf_font.h"
//#pragma comment(lib, "d3d9.lib")
//#pragma comment(lib, "d3dx9d.lib")

typedef struct { int x, y, w, h; } mu_Rect;
extern mu_Rect* g_ttf_atlas;
// ビットマップアトラスと同じようにアトラス配列を定義
// ビットマップモードと同じサイズに設定
static mu_Rect ttf_atlas[ATLAS_WHITE + 1] = {
    {0, 0, 0, 0},              // 未使用
    {0, 0, 16, 16},            // MU_ICON_CLOSE (ビットマップモードと同じサイズ)
    {0, 0, 18, 18},            // MU_ICON_CHECK (ビットマップモードと同じサイズ)
    {0, 0, 5, 7},              // MU_ICON_COLLAPSED (ビットマップモードと同じサイズ)
    {0, 0, 7, 5},              // MU_ICON_EXPANDED (ビットマップモードと同じサイズ)
    {0, 0, 3, 3}               // ATLAS_WHITE
};

// 外部参照用に配列をエクスポート
mu_Rect* g_ttf_atlas = ttf_atlas;

// アイコン定数の定義（microui.hと同じ）
#define MU_ICON_CLOSE 1
#define MU_ICON_CHECK 2
#define MU_ICON_COLLAPSED 3
#define MU_ICON_EXPANDED 4
#define MU_ICON_MAX 5
#define UI_PATCH_X (g_font_atlas.width - 16)
#define UI_PATCH_Y (g_font_atlas.height - 16)
mu_font_atlas g_font_atlas;
// 白い3x3
 const unsigned char white_patch[3 * 3] = {
    255,255,255,
    255,255,255,
    255,255,255
};

// 9x9の×アイコン (MU_ICON_CLOSE)
 const unsigned char close_patch[16 * 16] = {
    255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255,
    0,255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255, 0,
    0, 0,255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255, 0, 0,
    0, 0, 0,255, 0, 0, 0, 0, 0, 0, 0, 0,255, 0, 0, 0,
    0, 0, 0, 0,255, 0, 0, 0, 0, 0, 0,255, 0, 0, 0, 0,
    0, 0, 0, 0, 0,255, 0, 0, 0, 0,255, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,255, 0, 0,255, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,255,255, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0,255,255, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0,255, 0, 0,255, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0,255, 0, 0, 0, 0,255, 0, 0, 0, 0, 0,
    0, 0, 0, 0,255, 0, 0, 0, 0, 0, 0,255, 0, 0, 0, 0,
    0, 0, 0,255, 0, 0, 0, 0, 0, 0, 0, 0,255, 0, 0, 0,
    0, 0,255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255, 0, 0,
    0,255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255, 0,
    255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,255,
};

// 18x18のチェックアイコン (MU_ICON_CHECK) - ビットマップモードと同じサイズ
 const unsigned char check_patch[18 * 18] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,255,255,  0,  0,  0,  0,  0,
    0,  0,  0,255,255,  0,  0,  0,  0,  0,255,255,  0,  0,  0,  0,  0,  0,
    0,  0,255,255,  0,  0,  0,  0,255,255,255,  0,  0,  0,  0,  0,  0,  0,
    0,255,255,  0,  0,  0,  0,255,255,255,  0,  0,  0,  0,  0,  0,  0,  0,
    255,255,  0,  0,  0,  0,255,255,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,255,255,  0,  0,255,255,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,255,255,255,255,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,255,255,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
};

// ▼アイコン (MU_ICON_EXPANDED) - ビットマップモードと同じサイズ 7x5
 const unsigned char expanded_patch[7 * 5] = {
    255, 255, 255, 255, 255, 255, 255,
    0, 255, 255, 255, 255, 255, 0,
    0, 0, 255, 255, 255, 0, 0,
    0, 0, 0, 255, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0
};

// ▶アイコン (MU_ICON_COLLAPSED) - ビットマップモードと同じサイズ 5x7
 const unsigned char collapsed_patch[5 * 7] = {
    0, 0,255, 0, 0,
    0, 0,255,255, 0,
    0, 0,255,255,255,
    0, 0,255,255,255,
    0, 0,255,255,255,
    0, 0,255,255, 0,
    0, 0,255, 0, 0
};

// ピクセル座標（他ファイルからexternで参照）
int g_ui_white_rect[4]; // {x, y, w, h}

int g_ui_icon_rect[20]; // {x, y, w, h} * アイコン数

// TTFアトラス生成後に呼ぶ
// patch_ui_icons_to_atlasは不要。mu_font_stash_endでUIパッチ転写を行うため削除。

/* stb_truetype と stb_rect_pack の実装を含める */
#define STB_RECT_PACK_IMPLEMENTATION
#include "stb_rect_pack.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

/* フォント管理用グローバル変数 */
mu_font_atlas g_font_atlas;

/* フォントステート初期化 */
void mu_font_stash_begin(void)
{
    // アトラス全体をゼロ初期化
    memset(&g_font_atlas, 0, sizeof(g_font_atlas));
    
    // コードポイント検索のための配列にデフォルト値を設定
    for (int i = 0; i < 0xFFFF; i++) {
        g_font_atlas.glyphs[i].codepoint = 0; // 未使用状態
    }
}

/* TTFファイルからフォントを追加 */
int mu_font_add_from_file(const char* path, float size)
{
    int i, glyph_count;
    unsigned char* ttf_data;
    stbtt_pack_context spc;
    stbtt_packedchar* pc;
    stbtt_fontinfo info;
    FILE* fp;
    long file_size;

    /* TTFファイル読み込み */
    fp = fopen(path, "rb");
    if (!fp) return 0;
    
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    
    ttf_data = (unsigned char*)malloc(file_size);
    if (!ttf_data) {
        fclose(fp);
        return 0;
    }
    
    fread(ttf_data, 1, file_size, fp);
    fclose(fp);

    /* グリフ情報の初期化 */
    pc = (stbtt_packedchar*)malloc(sizeof(stbtt_packedchar) * 0x10000);
    if (!pc) {
        free(ttf_data);
        return 0;
    }

    /* アトラステクスチャサイズの決定（固定サイズ1024x1024） */
    g_font_atlas.width = 1024;
    g_font_atlas.height = 1024;
    g_font_atlas.pixel = malloc(g_font_atlas.width * g_font_atlas.height);
    if (!g_font_atlas.pixel) {
        free(ttf_data);
        free(pc);
        return 0;
    }

    /* stb_rect_packを使ってグリフを配置 */
    stbtt_PackBegin(&spc, g_font_atlas.pixel, g_font_atlas.width, g_font_atlas.height, 0, 1, NULL);

    /* 日本語を含む文字範囲をパック */
    stbtt_PackSetOversampling(&spc, 1, 1);
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 32, 95, pc); /* ASCII範囲 (32-126) */
    glyph_count = 95; /* ASCII文字数 */
    
    /* 日本語の平仮名範囲を追加 (U+3040-U+309F) */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x3040, 0x309F - 0x3040 + 1, pc + glyph_count);
    glyph_count += (0x309F - 0x3040 + 1);
    
    /* 日本語の片仮名範囲を追加 (U+30A0-U+30FF) */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x30A0, 0x30FF - 0x30A0 + 1, pc + glyph_count);
    
    glyph_count += (0x30FF - 0x30A0 + 1);

    /* 全角英数字と記号 (U+FF00-U+FFEF) */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0xFF00, 0xFFEF - 0xFF00 + 1, pc + glyph_count);
    glyph_count += (0xFFEF - 0xFF00 + 1);
    
    /* 基本漢字範囲を追加 (U+4E00-U+9FBF) - JIS第1・第2水準相当の一部 */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x4E00, 0x9FBF - 0x4E00 + 1, pc + glyph_count);
    glyph_count += (0x9FBF - 0x4E00 + 1);
    
    /* よく使われる漢字の追加範囲 */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5200, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5300, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5400, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5900, 100, pc + glyph_count);
    glyph_count += 100;
    

    stbtt_PackEnd(&spc);

    /* フォント情報取得 */
    stbtt_InitFont(&info, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0));

    /* フォントの行の高さを計算 */
    {
        float scale = stbtt_ScaleForPixelHeight(&info, size);
        int ascent, descent, lineGap;
        stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
        g_font_atlas.line_height = (int)((ascent - descent + lineGap) * scale);
        
        /* ビットマップフォントと同じライン高さに強制的に設定 */
        g_font_atlas.line_height = 18; /* ビットマップモードと同じライン高さ */
    }

    /* グリフカウントを設定 */
    g_font_atlas.glyph_count = glyph_count;
    
    /* グリフ情報をmicroui用に変換 */
    typedef struct {
        int start_idx;
        int start_codepoint;
        int count;
    } GlyphRange;
    GlyphRange ranges[] = {
        { 0, 32, 95 }, // ASCII
        { 95, 0x3040, 0x309F - 0x3040 + 1 }, // ひらがな
        { 95 + (0x309F - 0x3040 + 1), 0x30A0, 0x30FF - 0x30A0 + 1 }, // カタカナ
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1), 0xFF00, 0xFFEF - 0xFF00 + 1 }, // 全角英数字
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1), 0x4E00, 0x9FBF - 0x4E00 + 1 }, // 漢字（第1・第2水準）
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1) + (0x9FBF - 0x4E00 + 1), 0x5200, 100 }, // 追加漢字1
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1) + (0x9FBF - 0x4E00 + 1) + 100, 0x5300, 100 }, // 追加漢字2
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1) + (0x9FBF - 0x4E00 + 1) + 100 + 100, 0x5400, 100 }, // 追加漢字3
        { 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1) + (0x9FBF - 0x4E00 + 1) + 100 + 100 + 100, 0x5900, 100 }, // 追加漢字4
    };
    int range_count = sizeof(ranges) / sizeof(ranges[0]);
    for (int r = 0; r < range_count; ++r) {
        for (i = 0; i < ranges[r].count; ++i) {
            int atlas_idx = ranges[r].start_idx + i;
            int codepoint = ranges[r].start_codepoint + i;
            stbtt_packedchar* glyph = &pc[atlas_idx];
            g_font_atlas.glyphs[atlas_idx].codepoint = codepoint;
            g_font_atlas.glyphs[atlas_idx].x = glyph->x0;
            g_font_atlas.glyphs[atlas_idx].y = glyph->y0;
            g_font_atlas.glyphs[atlas_idx].w = glyph->x1 - glyph->x0;
            g_font_atlas.glyphs[atlas_idx].h = glyph->y1 - glyph->y0;
            g_font_atlas.glyphs[atlas_idx].xoff = glyph->xoff;
            g_font_atlas.glyphs[atlas_idx].yoff = glyph->yoff;
            g_font_atlas.glyphs[atlas_idx].xadvance = glyph->xadvance;
        }
    }

    g_font_atlas.glyph_count = glyph_count;
    
    //// カタカナの「ア」と「イ」のグリフを修正
    //{
    //    int hiragana_count = 0x309F - 0x3040 + 1;
    //    int base_idx = 95; // ASCII文字数（32-126）
    //    int a_idx = base_idx + hiragana_count + (0x30A2 - 0x30A0); // 「ア」のインデックス
    //    int i_idx = base_idx + hiragana_count + (0x30A4 - 0x30A0); // 「イ」のインデックス
    //    
    //    // 修正前の状態を出力
    //    {
    //    }
    //    
    //    // グリフ内容を入れ替え
    //    struct mu_font_glyph temp;
    //    temp = g_font_atlas.glyphs[a_idx];
    //    g_font_atlas.glyphs[a_idx] = g_font_atlas.glyphs[i_idx];
    //    g_font_atlas.glyphs[i_idx] = temp;
    //    
    //    // コードポイントを元に戻す
    //    g_font_atlas.glyphs[a_idx].codepoint = 0x30A2;
    //    g_font_atlas.glyphs[i_idx].codepoint = 0x30A4;
    //    
    //    // 修正後の状態を出力
    //    {
    //    }
    //}
    //
    free(pc);
    free(ttf_data);
    return 1;
}

/* フォントアトラス生成完了通知のみ（D3D9依存を除去） */
void mu_font_stash_end(void)
{
    // 何もしない（D3D9テクスチャ生成はrenderer.cで行う）
    // 必要ならアトラスバッファの後処理のみ
}

/* コードポイントからグリフ情報を取得 */
struct mu_font_glyph* mu_font_find_glyph(unsigned int codepoint)
{
    int i;

    // ASCII範囲の場合、インデックスを直接計算
    if (codepoint >= 32 && codepoint < 127) {
        // ASCIIの場合はインデックスが32を引いた値
        int idx = codepoint - 32;
        struct mu_font_glyph* glyph = &g_font_atlas.glyphs[idx];
        // グリフの実際のコードポイントが一致するか確認
        if (glyph->codepoint == codepoint) {
            return glyph;
        }
    }
    // ASCIIでない場合は線形探索
    for (i = 0; i < g_font_atlas.glyph_count; i++) {
        if (g_font_atlas.glyphs[i].codepoint == codepoint) {
            struct mu_font_glyph* glyph = &g_font_atlas.glyphs[i];
            return glyph;
        }
    }
    return NULL;
}
