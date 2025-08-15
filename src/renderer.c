#include <Windows.h>
#include "d3d9.h"
#include "d3dx9.h"
#include <stdbool.h>  // bool型用
//#include <stdint.h>  // uint8_t用
#include "renderer.h"

#define USE_TTF_FONT 0 // 1: TTF, 0: atlas.inl
#if USE_TTF_FONT
#include "ttf_font.h"  // TTFフォント読み込み用ヘッダー
// TTFフォント使用時に必要な定数を定義
enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };
#else
#include "atlas.inl" // 静的ビットマップフォント
#endif
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "d3dx9d.lib")
// グローバル変数の定義
static LPDIRECT3D9 d3d = NULL;
static LPDIRECT3DDEVICE9 d3d_device = NULL;

IDirect3DVertexBuffer9* g_vertex_buffer = NULL;
IDirect3DIndexBuffer9* g_index_buffer = NULL;
IDirect3DTexture9* white_texture = NULL;  // 白色テクスチャ (ATLAS_WHITE代替)

#if USE_TTF_FONT
extern IDirect3DTexture9* g_font_texture; // ttf_font.cで定義
static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color);
extern int g_ui_white_rect[4]; // {x, y, w, h}
extern int g_ui_icon_rect[20];  // {x, y, w, h}
extern mu_Rect* g_ttf_atlas;   // TTFモード用アトラス配列
#else
IDirect3DTexture9* atlas_texture_d3d = NULL; // 静的フォント用
static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color);
#endif

// flush関数の定義はこの後に記述してください（または既存の正しい位置に移動）

extern int height;
extern int width;
extern mu_Context* g_ctx;
// インデックスバッファを使う場合TRUE
int buf_on = TRUE;
// レイアウト定数
#define LAYOUT_WIDTH 300  // 定数をマクロとして定義
#define LAYOUT_HEIGHT 30
#define MAX_INDICES (MAX_VERTICES * 3 / 2)

static struct Vertex vertices[MAX_VERTICES];
static short indices[MAX_INDICES];
static int vertex_count = 0;
static int index_count = 0;

// デバイス設定関数
void r_set_device(LPDIRECT3DDEVICE9 device)
{
    d3d_device = device;
}

/*
* DirectXのクリーンアップ処理
* - デバイスとD3Dインターフェースの解放
*/
void CleanD3D()
{
    if (d3d_device) d3d_device->lpVtbl->Release(d3d_device);
    if (d3d) d3d->lpVtbl->Release(d3d);
}
void UpdateProjectionMatrix()
{
    D3DMATRIX proj;
    if (!d3d_device) return;

    ZeroMemory(&proj, sizeof(proj));
    proj._11 = 2.0f / width;   // widthではなくWINDOW_WIDTHを使用
    proj._22 = -2.0f / height; // heightではなくWINDOW_HEIGHTを使用
    proj._41 = -1.0f;
    proj._42 = 1.0f;
    proj._44 = 1.0f;
    d3d_device->lpVtbl->SetTransform(d3d_device, D3DTS_PROJECTION, &proj);
}

// DirectXの初期化
void InitD3D(HWND hwnd)
{
    D3DMATRIX proj;
    D3DPRESENT_PARAMETERS d3dpp;
    d3d = Direct3DCreate9(D3D_SDK_VERSION);
    ZeroMemory(&d3dpp, sizeof(d3dpp));
    d3dpp.Windowed = TRUE;
    d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow = hwnd;

    d3d->lpVtbl->CreateDevice(d3d, D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING, &d3dpp, &d3d_device);
    // レンダリング設定
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_LIGHTING, FALSE);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    // プロジェクション行列の設定
    ZeroMemory(&proj, sizeof(proj));
    proj._11 = 2.0f / width;
    proj._22 = -2.0f / height;
    proj._41 = -1.0f;
    proj._42 = 1.0f;
    proj._44 = 1.0f;
    d3d_device->lpVtbl->SetTransform(d3d_device, D3DTS_PROJECTION, &proj);

    // テクスチャステージの設定
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_COLOROP, D3DTOP_MODULATE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);
    if (buf_on)
    {
        d3d_device->lpVtbl->SetSamplerState(d3d_device, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
        d3d_device->lpVtbl->SetSamplerState(d3d_device, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
        d3d_device->lpVtbl->SetSamplerState(d3d_device, 0, D3DSAMP_MIPFILTER, D3DTEXF_NONE);
    }
    r_init(d3d_device);
    r_set_device(d3d_device);
    if (d3d_device == NULL)
    {
        return;
    }
#if USE_TTF_FONT
    // 白色テクスチャ作成 (ATLAS_WHITE代替)
    {
        D3DLOCKED_RECT locked;
        HRESULT hr = d3d_device->lpVtbl->CreateTexture(d3d_device, 1, 1, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &white_texture, NULL);
        if (SUCCEEDED(hr) && white_texture) {
            hr = white_texture->lpVtbl->LockRect(white_texture, 0, &locked, NULL, 0);
            if (SUCCEEDED(hr)) {
                *(DWORD*)locked.pBits = 0xFFFFFFFF;  // 白色（ARGB）
                white_texture->lpVtbl->UnlockRect(white_texture, 0);
            }
        }
    }

    // TTFフォントの読み込み
    mu_font_stash_begin(d3d_device);
    mu_font_add_from_file("MPLUS1p-Light.ttf", 16.0f);  // フォントサイズを大きくする
    mu_font_stash_end();
#else
    // atlas.inl用テクスチャ生成
    D3DLOCKED_RECT locked;
    d3d_device->lpVtbl->CreateTexture(d3d_device, ATLAS_WIDTH, ATLAS_HEIGHT, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &atlas_texture_d3d, NULL);
    if (atlas_texture_d3d && SUCCEEDED(atlas_texture_d3d->lpVtbl->LockRect(atlas_texture_d3d, 0, &locked, NULL, 0))) {
        DWORD* dst = (DWORD*)locked.pBits;
        for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
            BYTE alpha = atlas_texture[i];
            dst[i] = (alpha << 24) | 0x00FFFFFF;
        }
        atlas_texture_d3d->lpVtbl->UnlockRect(atlas_texture_d3d, 0);
    }
#endif
}

// r_create_atlas_textureは必要なくなりました - 代わりにmu_font_stash_beginとmu_font_stash_endを使用
// ダミー実装: 旧inl方式の互換用
int r_create_atlas_texture(void) { return 1; }

static BOOL r_validate_device(void)
{
    if (!d3d_device)
    {
        return FALSE;
    }
    return TRUE;
}

static void r_cleanup_resources(void)
{
    if (g_vertex_buffer)
    {
        g_vertex_buffer->lpVtbl->Release(g_vertex_buffer);
        g_vertex_buffer = NULL;
    }

    if (buf_on && g_index_buffer)
    {
        g_index_buffer->lpVtbl->Release(g_index_buffer);
        g_index_buffer = NULL;
    }

#if USE_TTF_FONT
    if (g_font_texture) {
        g_font_texture->lpVtbl->Release(g_font_texture);
        g_font_texture = NULL;
    }
#else
    if (atlas_texture_d3d) {
        atlas_texture_d3d->lpVtbl->Release(atlas_texture_d3d);
        atlas_texture_d3d = NULL;
    }
#endif

    if (white_texture) {
        white_texture->lpVtbl->Release(white_texture);
        white_texture = NULL;
    }

    d3d_device = NULL;
}

void r_cleanup(void)
{
    if (!r_validate_device()) return;
    r_cleanup_resources();
}

static BOOL r_setup_render_states(void)
{
    if (!r_validate_device()) return FALSE;

    // 基本レンダリング設定
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_LIGHTING, FALSE);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_ALPHABLENDENABLE, TRUE);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);

    // テクスチャブレンド設定
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAOP, D3DTOP_MODULATE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE);
    d3d_device->lpVtbl->SetTextureStageState(d3d_device, 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE);

    return TRUE;
}


void r_init(LPDIRECT3DDEVICE9 device)
{
    if (!device)
    {
        return;
    }

    // 既存リソースのクリーンアップ
    r_cleanup_resources();
    d3d_device = device;
    if (buf_on)
    {
        // 頂点バッファの作成
        HRESULT hr = d3d_device->lpVtbl->CreateVertexBuffer(d3d_device,
            MAX_VERTICES * sizeof(struct Vertex),
            D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
            D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE,
            D3DPOOL_DEFAULT,
            &g_vertex_buffer,
            NULL);
        if (FAILED(hr))
        {
            r_cleanup_resources();
            return;
        }
        // インデックスバッファの作成
        hr = d3d_device->lpVtbl->CreateIndexBuffer(d3d_device,
            MAX_INDICES * sizeof(short),
            D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY,
            D3DFMT_INDEX16,
            D3DPOOL_DEFAULT,
            &g_index_buffer,
            NULL);
        if (FAILED(hr))
        {
            r_cleanup_resources();
            return;
        }
    }

    // レンダラーの初期化
    if (!r_create_atlas_texture())
    {
        r_cleanup_resources();
        return;
    }

    if (!r_setup_render_states())
    {
        r_cleanup_resources();
        return;
    }
}

static int update_vertex_buffer(void)
{
    void* ptr;
    HRESULT hr;
    if (!g_vertex_buffer)
    {
        return FALSE;
    }

    // 頂点バッファのロック
    hr = g_vertex_buffer->lpVtbl->Lock(g_vertex_buffer, 0, vertex_count * sizeof(struct Vertex), &ptr, 0);
    if (FAILED(hr))
    {
        return FALSE;
    }

    // 頂点データのコピー
    memcpy(ptr, vertices, vertex_count * sizeof(struct Vertex));
    g_vertex_buffer->lpVtbl->Unlock(g_vertex_buffer);

    return TRUE;
}
static int update_index_buffer(void)
{
    void* ptr;
    HRESULT hr;
    if (!g_index_buffer)
    {
        return FALSE;
    }

    // インデックスバッファのロック
    hr = g_index_buffer->lpVtbl->Lock(g_index_buffer, 0, index_count * sizeof(short), &ptr, D3DLOCK_DISCARD);
    if (FAILED(hr))
    {
        return FALSE;
    }
    // インデックスデータのコピー
    memcpy(ptr, indices, index_count * sizeof(short));
    g_index_buffer->lpVtbl->Unlock(g_index_buffer);

    return TRUE;
}
static void flush(void)
{
    if (vertex_count == 0) { return; }
    
#if USE_TTF_FONT
    // TTFフォント使用時: フォントテクスチャを使用（白い部分も含む）
    if (!g_font_texture) { 
        return; 
    }
    if (buf_on && (!update_vertex_buffer() || !update_index_buffer())) { return; }
    
    // フォントテクスチャを使用（UI要素とテキスト両方）
    d3d_device->lpVtbl->SetTexture(d3d_device, 0, (IDirect3DBaseTexture9*)g_font_texture);
    if (buf_on)
        d3d_device->lpVtbl->SetStreamSource(d3d_device, 0, g_vertex_buffer, 0, sizeof(struct Vertex));
    if (buf_on)
        d3d_device->lpVtbl->SetIndices(d3d_device, g_index_buffer);
    d3d_device->lpVtbl->SetFVF(d3d_device, D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    if (buf_on)
        d3d_device->lpVtbl->DrawIndexedPrimitive(d3d_device, D3DPT_TRIANGLELIST, 0, 0, vertex_count, 0, index_count / 3);
    else
        d3d_device->lpVtbl->DrawIndexedPrimitiveUP(d3d_device, D3DPT_TRIANGLELIST, 0, vertex_count, index_count / 3, indices, D3DFMT_INDEX16, vertices, sizeof(struct Vertex));
    vertex_count = 0;
    index_count = 0;

#else
    if (!atlas_texture_d3d) { return; }
    if (buf_on && (!update_vertex_buffer() || !update_index_buffer())) { return; }
    d3d_device->lpVtbl->SetTexture(d3d_device, 0, (IDirect3DBaseTexture9*)atlas_texture_d3d);
    if (buf_on)
        d3d_device->lpVtbl->SetStreamSource(d3d_device, 0, g_vertex_buffer, 0, sizeof(struct Vertex));
    if (buf_on)
        d3d_device->lpVtbl->SetIndices(d3d_device, g_index_buffer);
    d3d_device->lpVtbl->SetFVF(d3d_device, D3DFVF_XYZ | D3DFVF_TEX1 | D3DFVF_DIFFUSE);
    if (buf_on)
        d3d_device->lpVtbl->DrawIndexedPrimitive(d3d_device, D3DPT_TRIANGLELIST, 0, 0, vertex_count, 0, index_count / 3);
    else
        d3d_device->lpVtbl->DrawIndexedPrimitiveUP(d3d_device, D3DPT_TRIANGLELIST, 0, vertex_count, index_count / 3, indices, D3DFMT_INDEX16, vertices, sizeof(struct Vertex));
    vertex_count = 0;
    index_count = 0;
#endif
}

void draw_win()
{
    // バックバッファのクリア
    d3d_device->lpVtbl->Clear(d3d_device, 0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(90, 95, 100), 1.0f, 0);

    // シーンの開始
    d3d_device->lpVtbl->BeginScene(d3d_device);
    // シザー矩形の無効化
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_SCISSORTESTENABLE, FALSE);

    // 描画
    flush();

    // シーンの終了
    d3d_device->lpVtbl->EndScene(d3d_device);
    d3d_device->lpVtbl->Present(d3d_device, NULL, NULL, NULL, NULL);
}

void r_draw()
{
    // コマンド処理
    mu_Command* cmd = NULL;
    static int frame_count = 0;
    int command_count = 0;
    int text_count = 0;
    int rect_count = 0;

    frame_count++;
    process_frame(g_ctx);

    while (mu_next_command(g_ctx, &cmd))
    {
        command_count++;
        switch (cmd->type)
        {
        case MU_COMMAND_CLIP:
            r_set_clip_rect(cmd->clip.rect);
            break;
        case MU_COMMAND_RECT:
            rect_count++;
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
            break;
        case MU_COMMAND_TEXT:
        {
            mu_Vec2 text_rect = { cmd->text.pos.x, cmd->text.pos.y };
            text_count++;
            r_draw_text(cmd->text.str, text_rect, cmd->text.color);
        }
        break;
        case MU_COMMAND_ICON:
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        }
    }

    draw_win();
}

// --- r_draw_icon: アイコン描画 ---
void r_draw_icon(int id, mu_Rect rect, mu_Color color)
{
#if USE_TTF_FONT
    if (id >= MU_ICON_CLOSE && id < MU_ICON_MAX) {
        // ttf_atlas配列を使用（ビットマップモードと同様）
        mu_Rect src = g_ttf_atlas[id];
        int x = rect.x + (rect.w - src.w) / 2;
        int y = rect.y + (rect.h - src.h) / 2;
        push_quad(mu_rect(x, y, src.w, src.h), src, color);
    } else {
        // 未定義のアイコンの場合は白矩形を描画
        push_quad(rect, g_ttf_atlas[ATLAS_WHITE], color);
    }
#else
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    push_quad(mu_rect(x, y, src.w, src.h), src, color);
#endif
}

void r_draw_rect(mu_Rect rect, mu_Color color)
{
    if (!r_validate_device()) return;
#if USE_TTF_FONT
    // ttf_atlas配列を使用（ビットマップモードと同様）
    push_quad(rect, g_ttf_atlas[ATLAS_WHITE], color);
#else
    push_quad(rect, atlas[ATLAS_WHITE], color);
#endif
}

int r_get_text_height(void)
{
#if USE_TTF_FONT
    // ビットマップモードと同じ高さを返す
    // ただし少し小さめの値にして、中央寄せ計算時に下に寄るようにする
    return 15; // 18よりも少し小さく
#else
    return 18;
#endif
}

void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color)
{
#if USE_TTF_FONT
    // --- Shift-JIS→UTF-8変換 ---
    char utf8[1024];
    int wlen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    MultiByteToWideChar(CP_ACP, 0, text, -1, wbuf, wlen);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8, sizeof(utf8), NULL, NULL);
    free(wbuf);
    const unsigned char* p = (const unsigned char*)utf8;
    mu_Rect dst = { pos.x, pos.y + 15, 0, 0 };
    while (*p) {
        unsigned int codepoint = 0;
        if ((*p & 0x80) == 0) { codepoint = *p++; }
        else if ((*p & 0xE0) == 0xC0) {
            codepoint = (*p++ & 0x1F) << 6;
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F);
            else { p++; continue; }
        }
        else if ((*p & 0xF0) == 0xE0) {
            codepoint = (*p++ & 0x0F) << 12;
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6;
            else { p++; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F);
            else { p++; continue; }
        }
        else if ((*p & 0xF8) == 0xF0) {
            codepoint = (*p++ & 0x07) << 18;
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 12;
            else { p++; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6;
            else { p++; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F);
            else { p++; continue; }
        }
        else { p++; continue; }
        struct mu_font_glyph* glyph = mu_font_find_glyph(codepoint);
        if (glyph) {
            mu_Rect src = { glyph->x, glyph->y, glyph->w, glyph->h };
            dst.x += glyph->xoff;
            dst.y += glyph->yoff - 8;
            dst.w = glyph->w;
            dst.h = glyph->h;
            push_quad(dst, src, color);
            dst.x += glyph->xadvance - glyph->xoff;
            dst.y -= (glyph->yoff - 8);
        }
    }
#else
    mu_Rect src;
    char* p;
    int chr;
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (p = (char*)text; *p; p++) {
        if ((*p & 0xc0) == 0x80) { continue; }
        chr = mu_min((unsigned char)*p, 127);
        src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
#endif
}

int r_get_text_width(const char* text, int len)
{
#if USE_TTF_FONT
    int res = 0;
    const unsigned char* p;
    if (len < 0) len = strlen(text);
    for (p = (const unsigned char*)text; *p && len > 0; ) {
        unsigned int codepoint = 0;
        int bytes_read = 0;
        
        if ((*p & 0x80) == 0) { 
            codepoint = *p++; 
            bytes_read = 1; 
        }
        else if ((*p & 0xE0) == 0xC0) { 
            codepoint = (*p++ & 0x1F) << 6; 
            bytes_read = 2; 
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
            else { p++; bytes_read = 2; continue; }
        }
        else if ((*p & 0xF0) == 0xE0) { 
            codepoint = (*p++ & 0x0F) << 12; 
            bytes_read = 3; 
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6; 
            else { p++; bytes_read = 3; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
            else { p++; bytes_read = 3; continue; }
        }
        else if ((*p & 0xF8) == 0xF0) { 
            codepoint = (*p++ & 0x07) << 18; 
            bytes_read = 4; 
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 12; 
            else { p++; bytes_read = 4; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6; 
            else { p++; bytes_read = 4; continue; }
            if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
            else { p++; bytes_read = 4; continue; }
        }
        else { 
            p++; 
            bytes_read = 1; 
            continue;
        }
        len -= bytes_read;
        struct mu_font_glyph* glyph = mu_font_find_glyph(codepoint);
        if (glyph) {
            res += glyph->xadvance;
        } else {
            // グリフが見つからない場合は'?'の幅を使用
            struct mu_font_glyph* fallback_glyph = mu_font_find_glyph('?');
            if (fallback_glyph) {
                res += fallback_glyph->xadvance;
            } else {
                // '?'のグリフもない場合はデフォルト幅
                res += 8; // デフォルトの文字幅
            }
        }
    }
    return res;
#else
    int res = 0;
    const unsigned char* p;
    if (len < 0) len = strlen(text);
    
    for (p = (const unsigned char*)text; *p && len > 0; p++, len--) {
        // UTF-8の継続バイトをスキップ
        if ((*p & 0xc0) == 0x80) { continue; }
        
        // ASCII文字の範囲のみ処理
        if (*p >= 32 && *p <= 126) {
            res += atlas[ATLAS_FONT + (*p - 32)].w;
        } else {
            // ASCII範囲外は'?'の幅を使用
            res += atlas[ATLAS_FONT + ('?' - 32)].w;
        }
    }
    return res;
#endif
}

/*
* クリッピング領域の設定
* @param rect - クリッピング領域の矩形情報（x,y,w,h）
*
* 処理フロー:
* 1. デバイスの有効性チェック
* 2. ビューポートの設定
* 3. シザー矩形の設定
*/
void r_set_clip_rect(mu_Rect rect)
{
    // シザー矩形の設定
    RECT scissor_rect = {
        rect.x,             // 左端
        rect.y,             // 上端
        rect.x + rect.w,    // 右端
        rect.y + rect.h     // 下端
    };
    // デバイスの有効性チェック
    if (!r_validate_device()) return;


    // シザー矩形の設定
    d3d_device->lpVtbl->SetRenderState(d3d_device, D3DRS_SCISSORTESTENABLE, TRUE);
    d3d_device->lpVtbl->SetScissorRect(d3d_device, &scissor_rect);
}

/*
* バッファのクリア処理
* バックバッファをクリアし新しいフレームの描画準備をする
*/
void r_clear(mu_Color color)
{
    if (!r_validate_device()) return;
    d3d_device->lpVtbl->Clear(d3d_device,
        0,
        NULL,
        D3DCLEAR_TARGET,
        D3DCOLOR_RGBA(color.r, color.g, color.b, color.a),
        1.0f,
        0);
}

/*
* フレームの表示
* バックバッファをフロントバッファに切り替える
*/
void r_present(void)
{
    if (!r_validate_device()) return;
    d3d_device->lpVtbl->Present(d3d_device, NULL, NULL, NULL, NULL);
}

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color)
{
    float x, y, w, h;
    DWORD vertex_color = D3DCOLOR_RGBA(color.r, color.g, color.b, color.a);
    if (!r_validate_device()) return;
    if (vertex_count >= MAX_VERTICES - 4 || index_count >= MAX_INDICES - 6) { flush(); }
    #if USE_TTF_FONT
    if (g_font_atlas.width > 0 && g_font_atlas.height > 0) {
        x = (float)src.x / (float)g_font_atlas.width;
        y = (float)src.y / (float)g_font_atlas.height;
        w = (float)src.w / (float)g_font_atlas.width;
        h = (float)src.h / (float)g_font_atlas.height;
    } else {
        x = 0.99f;
        y = 0.99f;
        w = 0.01f;
        h = 0.01f;
    }
    #else
    x = (float)src.x / ATLAS_WIDTH;
    y = (float)src.y / ATLAS_HEIGHT;
    w = (float)src.w / ATLAS_WIDTH;
    h = (float)src.h / ATLAS_HEIGHT;
    #endif
    vertices[vertex_count + 0].x = dst.x;
    vertices[vertex_count + 0].y = dst.y;
    vertices[vertex_count + 0].z = 0.5f;
    vertices[vertex_count + 0].color = vertex_color;
    vertices[vertex_count + 0].u = x;
    vertices[vertex_count + 0].v = y;
    vertices[vertex_count + 1].x = dst.x + dst.w;
    vertices[vertex_count + 1].y = dst.y;
    vertices[vertex_count + 1].z = 0.5f;
    vertices[vertex_count + 1].color = vertex_color;
    vertices[vertex_count + 1].u = x + w;
    vertices[vertex_count + 1].v = y;
    vertices[vertex_count + 2].x = dst.x;
    vertices[vertex_count + 2].y = dst.y + dst.h;
    vertices[vertex_count + 2].z = 0.5f;
    vertices[vertex_count + 2].color = vertex_color;
    vertices[vertex_count + 2].u = x;
    vertices[vertex_count + 2].v = y + h;
    vertices[vertex_count + 3].x = dst.x + dst.w;
    vertices[vertex_count + 3].y = dst.y + dst.h;
    vertices[vertex_count + 3].z = 0.5f;
    vertices[vertex_count + 3].color = vertex_color;
    vertices[vertex_count + 3].u = x + w;
    vertices[vertex_count + 3].v = y + h;
    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;
    vertex_count += 4;
    index_count += 6;
}
