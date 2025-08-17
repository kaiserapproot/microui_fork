
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
#include "ttf_font2.h"
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
static const unsigned char white_patch[3 * 3] = {
    255,255,255,
    255,255,255,
    255,255,255
};

// 9x9の×アイコン (MU_ICON_CLOSE)
static const unsigned char close_patch[16 * 16] = {
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
static const unsigned char check_patch[18 * 18] = {
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
static const unsigned char expanded_patch[7 * 5] = {
    255, 255, 255, 255, 255, 255, 255,
    0, 255, 255, 255, 255, 255, 0,
    0, 0, 255, 255, 255, 0, 0,
    0, 0, 0, 255, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0
};

// ▶アイコン (MU_ICON_COLLAPSED) - ビットマップモードと同じサイズ 5x7
static const unsigned char collapsed_patch[5 * 7] = {
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
IDirect3DTexture9* g_font_texture = NULL;
static IDirect3DDevice9* g_device = NULL;

/* フォントステート初期化 */
void mu_font_stash_begin(IDirect3DDevice9* device)
{
    g_device = device;
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
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x4E00, 0x5200 - 0x4E00, pc + glyph_count);
    glyph_count += (0x5200 - 0x4E00);
    
    /* よく使われる漢字の追加範囲 */
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5200, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5300, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5400, 100, pc + glyph_count);
    glyph_count += 100;
    
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x5900, 100, pc + glyph_count);
    glyph_count += 100;
    
    /* 「世」と「界」の文字を特別に追加 */
    {
        unsigned int special_chars[] = {0x4E16, 0x754C}; // 世, 界
        stbtt_PackFontRange(&spc, ttf_data, 0, size, special_chars[0], 1, pc + glyph_count);
        glyph_count += 1;
        stbtt_PackFontRange(&spc, ttf_data, 0, size, special_chars[1], 1, pc + glyph_count);
        glyph_count += 1;
    }

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
    for (i = 0; i < 95; i++) {
        /* ASCII範囲 (32-126) */
        stbtt_packedchar* glyph = &pc[i];
        int codepoint = i + 32; /* ASCIIはコードポイント32から */
        
        g_font_atlas.glyphs[i].codepoint = codepoint;
        g_font_atlas.glyphs[i].x = glyph->x0;
        g_font_atlas.glyphs[i].y = glyph->y0;
        g_font_atlas.glyphs[i].w = glyph->x1 - glyph->x0;
        g_font_atlas.glyphs[i].h = glyph->y1 - glyph->y0;
        g_font_atlas.glyphs[i].xoff = glyph->xoff;
        g_font_atlas.glyphs[i].yoff = glyph->yoff;
        g_font_atlas.glyphs[i].xadvance = glyph->xadvance;
    }
    
    /* 日本語文字のグリフ情報を設定 */
    {
        int base_idx = 95; // ASCII以降のインデックス基点
        int hiragana_count = 0x309F - 0x3040 + 1;
        int katakana_count = 0x30FF - 0x30A0 + 1;
        int zenwidth_count = 0xFFEF - 0xFF00 + 1;
        int kanji1_count = 0x5200 - 0x4E00;
        int kanji2_count = 100;
        int kanji3_count = 100;
        int kanji4_count = 100;
        int kanji5_count = 100;
        
        // 1. 平仮名範囲 (U+3040-U+309F)
        for (i = 0; i < hiragana_count; i++) {
            int atlas_idx = base_idx + i;
            int codepoint = 0x3040 + i;
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
        
        // 2. 片仮名範囲 (U+30A0-U+30FF)
        base_idx += hiragana_count;
        for (i = 0; i < katakana_count; i++) {
            int atlas_idx = base_idx + i;
            int codepoint = 0x30A0 + i;
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
        
        // 3. 全角英数字と記号 (U+FF00-U+FFEF)
        base_idx += katakana_count;
        for (i = 0; i < zenwidth_count; i++) {
            int atlas_idx = base_idx + i;
            int codepoint = 0xFF00 + i;
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
        
        // 4. 残りの漢字範囲（簡略化のため省略）
        base_idx += zenwidth_count;
        
        // 残りの漢字範囲のグリフを設定
        for (i = base_idx; i < glyph_count; i++) {
            stbtt_packedchar* glyph = &pc[i];
            int remaining_idx = i - base_idx;
            int codepoint;
            
            if (remaining_idx < kanji1_count) {
                codepoint = 0x4E00 + remaining_idx;
                
                // 「世」の文字がこの範囲に含まれるか確認（デバッグ）
                if (codepoint == 0x4E16) {
                }
            }
            else if (remaining_idx < kanji1_count + kanji2_count) {
                codepoint = 0x5200 + (remaining_idx - kanji1_count);
            }
            else if (remaining_idx < kanji1_count + kanji2_count + kanji3_count) {
                codepoint = 0x5300 + (remaining_idx - kanji1_count - kanji2_count);
            }
            else if (remaining_idx < kanji1_count + kanji2_count + kanji3_count + kanji4_count) {
                codepoint = 0x5400 + (remaining_idx - kanji1_count - kanji2_count - kanji3_count);
            }
            else if (remaining_idx < kanji1_count + kanji2_count + kanji3_count + kanji4_count + kanji5_count) {
                codepoint = 0x5900 + (remaining_idx - kanji1_count - kanji2_count - kanji3_count - kanji4_count);
            }
            // 特別文字「世」「界」の処理
            else if (remaining_idx == kanji1_count + kanji2_count + kanji3_count + kanji4_count + kanji5_count) {
                codepoint = 0x4E16; // 世
            }
            else {
                codepoint = 0x754C; // 界
            }
            
            g_font_atlas.glyphs[i].codepoint = codepoint;
            g_font_atlas.glyphs[i].x = glyph->x0;
            g_font_atlas.glyphs[i].y = glyph->y0;
            g_font_atlas.glyphs[i].w = glyph->x1 - glyph->x0;
            g_font_atlas.glyphs[i].h = glyph->y1 - glyph->y0;
            g_font_atlas.glyphs[i].xoff = glyph->xoff;
            g_font_atlas.glyphs[i].yoff = glyph->yoff;
            g_font_atlas.glyphs[i].xadvance = glyph->xadvance;
        }
    }
    
    g_font_atlas.glyph_count = glyph_count;
    
    // カタカナの「ア」と「イ」のグリフを修正
    {
        int hiragana_count = 0x309F - 0x3040 + 1;
        int base_idx = 95; // ASCII文字数（32-126）
        int a_idx = base_idx + hiragana_count + (0x30A2 - 0x30A0); // 「ア」のインデックス
        int i_idx = base_idx + hiragana_count + (0x30A4 - 0x30A0); // 「イ」のインデックス
        
        // 修正前の状態を出力
        {
        }
        
        // グリフ内容を入れ替え
        struct mu_font_glyph temp;
        temp = g_font_atlas.glyphs[a_idx];
        g_font_atlas.glyphs[a_idx] = g_font_atlas.glyphs[i_idx];
        g_font_atlas.glyphs[i_idx] = temp;
        
        // コードポイントを元に戻す
        g_font_atlas.glyphs[a_idx].codepoint = 0x30A2;
        g_font_atlas.glyphs[i_idx].codepoint = 0x30A4;
        
        // 修正後の状態を出力
        {
        }
    }
    
    free(pc);
    free(ttf_data);
    return 1;
}

/* フォントアトラスをD3Dテクスチャに転送 */
void mu_font_stash_end(void)
{
    int i, x, y;
    D3DLOCKED_RECT locked;
    HRESULT hr;
    unsigned char* src;
    unsigned int* dst;

    if (!g_device || !g_font_atlas.pixel) return;

    /* テクスチャ作成 */
    hr = g_device->lpVtbl->CreateTexture(g_device, 
        g_font_atlas.width, g_font_atlas.height, 1, 
        D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT,
        &g_font_texture, NULL);
    
    if (FAILED(hr)) {
        return;
    }

    /* テクスチャにフォントアトラスデータをコピー */
    hr = g_font_texture->lpVtbl->LockRect(g_font_texture, 0, &locked, NULL, 0);
    if (SUCCEEDED(hr)) {
        src = (unsigned char*)g_font_atlas.pixel;
        for (y = 0; y < g_font_atlas.height; y++) {
            dst = (unsigned int*)((unsigned char*)locked.pBits + y * locked.Pitch);
            for (x = 0; x < g_font_atlas.width; x++) {
                unsigned char alpha = *src++;
                *dst++ = (alpha << 24) | 0x00FFFFFF;
            }
        }
        // --- UIパッチ転写 ---
        // UIパッチ領域を64x64で確保
        int patch_size = 64;
        int ui_patch_x = g_font_atlas.width - patch_size;
        int ui_patch_y = g_font_atlas.height - patch_size;
        // 領域全体を透明で初期化
        for (int py = 0; py < patch_size; ++py) {
            for (int px = 0; px < patch_size; ++px) {
                int dst_x = ui_patch_x + px;
                int dst_y = ui_patch_y + py;
                if (dst_x < 0 || dst_x >= g_font_atlas.width || dst_y < 0 || dst_y >= g_font_atlas.height) continue;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = 0x00000000;
            }
        }
        // 白パッチ（上部に配置）- ビットマップモードと同じサイズ
        int white_w = 3, white_h = 3;
        int white_x = ui_patch_x + (patch_size - white_w) / 2;
        int white_y = ui_patch_y + 4; // 上部に配置
        for (int py = 0; py < white_h; ++py) {
            for (int px = 0; px < white_w; ++px) {
                int dst_x = white_x + px;
                int dst_y = white_y + py;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = 0xFFFFFFFF; // 完全な白（アルファ255）
            }
        }
        
        // ×アイコン (MU_ICON_CLOSE) - ビットマップモードでは16x16
        int icon_w = 16, icon_h = 16;
        int icon_x = ui_patch_x + 4; // 左上に配置
        int icon_y = ui_patch_y + 12;
        for (int py = 0; py < icon_h; ++py) {
            for (int px = 0; px < icon_w; ++px) {
                int dst_x = icon_x + px;
                int dst_y = icon_y + py;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = (close_patch[py * 16 + px] << 24) | 0x00FFFFFF;
            }
        }
        
        // チェックアイコン (MU_ICON_CHECK) - ビットマップモードでは18x18
        int check_w = 18, check_h = 18;
        int check_x = ui_patch_x + 20; // 中央左に配置
        int check_y = ui_patch_y + 12;
        for (int py = 0; py < check_h; ++py) {
            for (int px = 0; px < check_w; ++px) {
                int dst_x = check_x + px;
                int dst_y = check_y + py;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = (check_patch[py * 18 + px] << 24) | 0x00FFFFFF;
            }
        }
        
        // 折りたたみアイコン▶ (MU_ICON_COLLAPSED) - ビットマップモードでは5x7
        int collapsed_w = 5, collapsed_h = 7;
        int collapsed_x = ui_patch_x + 36; // 右側に配置
        int collapsed_y = ui_patch_y + 12;
        for (int py = 0; py < collapsed_h; ++py) {
            for (int px = 0; px < collapsed_w; ++px) {
                int dst_x = collapsed_x + px;
                int dst_y = collapsed_y + py;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = (collapsed_patch[py * collapsed_w + px] << 24) | 0x00FFFFFF;
            }
        }
        
        // 展開アイコン▼ (MU_ICON_EXPANDED) - ビットマップモードでは7x5
        int expanded_w = 7, expanded_h = 5;
        int expanded_x = ui_patch_x + 52; // 右端に配置
        int expanded_y = ui_patch_y + 12;
        for (int py = 0; py < expanded_h; ++py) {
            for (int px = 0; px < expanded_w; ++px) {
                int dst_x = expanded_x + px;
                int dst_y = expanded_y + py;
                unsigned int* dst_row = (unsigned int*)((unsigned char*)locked.pBits + dst_y * locked.Pitch);
                unsigned int* dst_px = dst_row + dst_x;
                *dst_px = (expanded_patch[py * expanded_w + px] << 24) | 0x00FFFFFF;
            }
        }
        
        // ピクセル座標をセット
        g_ui_white_rect[0] = white_x;
        g_ui_white_rect[1] = white_y;
        g_ui_white_rect[2] = white_w;
        g_ui_white_rect[3] = white_h;
        
        // 各アイコン座標をセット (MU_ICON_CLOSE)
        g_ui_icon_rect[0*4+0] = icon_x;
        g_ui_icon_rect[0*4+1] = icon_y;
        g_ui_icon_rect[0*4+2] = icon_w;
        g_ui_icon_rect[0*4+3] = icon_h;
        
        // MU_ICON_CHECK座標
        g_ui_icon_rect[1*4+0] = check_x;
        g_ui_icon_rect[1*4+1] = check_y;
        g_ui_icon_rect[1*4+2] = icon_w;
        g_ui_icon_rect[1*4+3] = icon_h;
        
        // MU_ICON_COLLAPSED座標
        g_ui_icon_rect[2*4+0] = collapsed_x;
        g_ui_icon_rect[2*4+1] = collapsed_y;
        g_ui_icon_rect[2*4+2] = collapsed_w;
        g_ui_icon_rect[2*4+3] = collapsed_h;
        
        // MU_ICON_EXPANDED座標
        g_ui_icon_rect[3*4+0] = expanded_x;
        g_ui_icon_rect[3*4+1] = expanded_y;
        g_ui_icon_rect[3*4+2] = expanded_w;
        g_ui_icon_rect[3*4+3] = expanded_h;
        
        // ttf_atlas配列を更新（ビットマップモードと同じサイズを設定）
        ttf_atlas[MU_ICON_CLOSE].x = icon_x;
        ttf_atlas[MU_ICON_CLOSE].y = icon_y;
        ttf_atlas[MU_ICON_CLOSE].w = icon_w; // 16
        ttf_atlas[MU_ICON_CLOSE].h = icon_h; // 16
        
        ttf_atlas[MU_ICON_CHECK].x = check_x;
        ttf_atlas[MU_ICON_CHECK].y = check_y;
        ttf_atlas[MU_ICON_CHECK].w = check_w; // 18
        ttf_atlas[MU_ICON_CHECK].h = check_h; // 18
        
        ttf_atlas[MU_ICON_COLLAPSED].x = collapsed_x;
        ttf_atlas[MU_ICON_COLLAPSED].y = collapsed_y;
        ttf_atlas[MU_ICON_COLLAPSED].w = collapsed_w; // 5
        ttf_atlas[MU_ICON_COLLAPSED].h = collapsed_h; // 7
        
        ttf_atlas[MU_ICON_EXPANDED].x = expanded_x;
        ttf_atlas[MU_ICON_EXPANDED].y = expanded_y;
        ttf_atlas[MU_ICON_EXPANDED].w = expanded_w; // 7
        ttf_atlas[MU_ICON_EXPANDED].h = expanded_h; // 5
        
        ttf_atlas[ATLAS_WHITE].x = white_x;
        ttf_atlas[ATLAS_WHITE].y = white_y;
        ttf_atlas[ATLAS_WHITE].w = white_w; // 3
        ttf_atlas[ATLAS_WHITE].h = white_h; // 3
        
        g_font_texture->lpVtbl->UnlockRect(g_font_texture, 0);
    } else {
    }
    free(g_font_atlas.pixel);
    g_font_atlas.pixel = NULL;
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
