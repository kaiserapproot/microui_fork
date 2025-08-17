#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <d3dcompiler.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define USE_TTF_FONT 0 // 1: TTF, 0: atlas.inl
#include "dx11_new2_render.h"

#if USE_TTF_FONT
#include "ttf_font.h"
#include "stb_truetype.h"
#include "stb_rect_pack.h"
// TTFフォント使用時に必要な定数を定義
enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };
// UIアイコンパッチのextern宣言
extern unsigned char white_patch[3 * 3];
extern unsigned char close_patch[16 * 16];
extern unsigned char check_patch[18 * 18];
extern unsigned char collapsed_patch[5 * 7];
extern unsigned char expanded_patch[7 * 5];
#else
#include "atlas.inl" // 静的ビットマップフォント
#endif

// グローバル変数
static ID3D11Device* g_device = NULL;
static ID3D11DeviceContext* g_context = NULL;
static IDXGISwapChain* g_swapchain = NULL;
static ID3D11RenderTargetView* g_rtv = NULL;

extern int width;
extern int height;
extern mu_Context* g_ctx;

static ID3D11Buffer* g_vertex_buffer = NULL;
static ID3D11Buffer* g_index_buffer = NULL;
static ID3D11VertexShader* g_vertex_shader = NULL;
static ID3D11PixelShader* g_pixel_shader = NULL;
static ID3D11InputLayout* g_input_layout = NULL;
static ID3D11BlendState* g_blend_state = NULL;
static ID3D11SamplerState* g_sampler_state = NULL;
static ID3D11RasterizerState* g_rasterizer_state = NULL;

#if USE_TTF_FONT
// TTFフォント用DirectX11リソース
static ID3D11Texture2D* g_font_texture_dx11 = NULL;
static ID3D11ShaderResourceView* g_font_srv_dx11 = NULL;
extern int g_ui_white_rect[4]; // {x, y, w, h}
extern int g_ui_icon_rect[20];  // {x, y, w, h}
extern mu_Rect* g_ttf_atlas;   // TTFモード用アトラス配列
extern mu_font_atlas g_font_atlas; // ttf_font.cで定義
#else
// 静的アトラス用DirectX11リソース
static ID3D11Texture2D* g_atlas_texture = NULL;
static ID3D11ShaderResourceView* g_atlas_srv = NULL;
#endif

static struct Vertex vertices[MAX_VERTICES];
static short indices[MAX_VERTICES * 3 / 2];
#define MAX_INDICES (MAX_VERTICES * 3 / 2)
static int vertex_count = 0;
static int index_count = 0;

// 前方宣言
static void create_atlas_texture(void);
static void create_shaders(void);
static void create_states(void);
static void flush(void);
static int update_vertex_buffer(void);
static int update_index_buffer(void);
static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color);

// ヘルパー関数
//static int mu_min(int a, int b) {
//    return a < b ? a : b;
//}

static mu_Rect mu_rect(int x, int y, int w, int h) {
    mu_Rect r = { x, y, w, h };
    return r;
}

#if USE_TTF_FONT
static void create_font_texture_dx11(void) {
    if (!g_device || !g_font_atlas.pixel) return;
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = g_font_atlas.width;
    desc.Height = g_font_atlas.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    unsigned char* rgba = (unsigned char*)malloc(g_font_atlas.width * g_font_atlas.height * 4);
    for (int i = 0; i < g_font_atlas.width * g_font_atlas.height; i++) {
        rgba[i*4+0] = 255;
        rgba[i*4+1] = 255;
        rgba[i*4+2] = 255;
        rgba[i*4+3] = ((unsigned char*)g_font_atlas.pixel)[i];
    }
    D3D11_SUBRESOURCE_DATA data = {0};
    data.pSysMem = rgba;
    data.SysMemPitch = g_font_atlas.width * 4;
    HRESULT hr = g_device->lpVtbl->CreateTexture2D(g_device, &desc, &data, &g_font_texture_dx11);
    free(rgba);
    if (FAILED(hr)) return;
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_font_texture_dx11, &srvDesc, &g_font_srv_dx11);
    if (FAILED(hr)) return;
}
#endif

void r_init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain, ID3D11RenderTargetView* rtv) {
    g_device = device;
    g_context = context;
    g_swapchain = swapchain;
    g_rtv = rtv;
    
    // 頂点バッファ生成
    D3D11_BUFFER_DESC vbdesc = {0};
    vbdesc.Usage = D3D11_USAGE_DYNAMIC;
    vbdesc.ByteWidth = sizeof(struct Vertex) * MAX_VERTICES;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_device->lpVtbl->CreateBuffer(g_device, &vbdesc, NULL, &g_vertex_buffer);
    
    // インデックスバッファ生成
    D3D11_BUFFER_DESC ibdesc = {0};
    ibdesc.Usage = D3D11_USAGE_DYNAMIC;
    ibdesc.ByteWidth = sizeof(short) * MAX_VERTICES * 3 / 2;
    ibdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    g_device->lpVtbl->CreateBuffer(g_device, &ibdesc, NULL, &g_index_buffer);
    
#if USE_TTF_FONT
    // フォントファイルの存在確認
    FILE* font_fp = fopen("MPLUS1p-Light.ttf", "rb");
    if (font_fp) {
        OutputDebugStringA("[DEBUG] Font file 'MPLUS1p-Light.ttf' found.\n");
        fclose(font_fp);
    } else {
        OutputDebugStringA("[DEBUG] Font file 'MPLUS1p-Light.ttf' NOT found!\n");
    }
    OutputDebugStringA("[DEBUG] Calling mu_font_stash_begin()\n");
    mu_font_stash_begin();
    OutputDebugStringA("[DEBUG] Calling mu_font_add_from_file()\n");
    int font_ok = mu_font_add_from_file("MPLUS1p-Light.ttf", 16.0f);
    char buf[128];
    sprintf(buf, "[DEBUG] mu_font_add_from_file() returned %d\n", font_ok);
    OutputDebugStringA(buf);
    OutputDebugStringA("[DEBUG] Calling mu_font_stash_end()\n");
    mu_font_stash_end();
    sprintf(buf, "[DEBUG] g_font_atlas.glyph_count = %d\n", g_font_atlas.glyph_count);
    OutputDebugStringA(buf);
#else
    create_atlas_texture();
#endif
    
    // シェーダー生成
    create_shaders();
    // ステート生成
    create_states();
}

// --- テクスチャ生成（atlas.inlのビットマップをDirectX11テクスチャへ） ---
static void create_atlas_texture(void) {
#if USE_TTF_FONT
    // TTFモードでは何もしない（mu_font_stash_end_dx11で処理）
    return;
#else
    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = ATLAS_WIDTH;
    desc.Height = ATLAS_HEIGHT;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    unsigned char* rgba = (unsigned char*)malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        unsigned char alpha = atlas_texture[i];
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha;
    }
    
    // alpha値の一部をログ出力
    char logbuf[128];
    sprintf(logbuf, "[DEBUG] atlas_texture[0]=%d atlas_texture[100]=%d atlas_texture[1000]=%d\n", atlas_texture[0], atlas_texture[100], atlas_texture[1000]);
    OutputDebugStringA(logbuf);
    
    D3D11_SUBRESOURCE_DATA data = {0};
    data.pSysMem = rgba;
    data.SysMemPitch = ATLAS_WIDTH * 4;
    HRESULT hr = g_device->lpVtbl->CreateTexture2D(g_device, &desc, &data, &g_atlas_texture);
    sprintf(logbuf, "[DEBUG] create_atlas_texture: CreateTexture2D hr=0x%08X\n", (unsigned int)hr);
    OutputDebugStringA(logbuf);
    free(rgba);
    
    if (g_atlas_texture) {
        hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_atlas_texture, NULL, &g_atlas_srv);
        sprintf(logbuf, "[DEBUG] create_atlas_texture: CreateShaderResourceView hr=0x%08X g_atlas_srv=%p\n", (unsigned int)hr, g_atlas_srv);
        OutputDebugStringA(logbuf);
    } else {
        OutputDebugStringA("[DEBUG] create_atlas_texture: g_atlas_texture is NULL!\n");
    }
#endif
}

void r_cleanup(void) {
    if (g_blend_state) g_blend_state->lpVtbl->Release(g_blend_state);
    if (g_sampler_state) g_sampler_state->lpVtbl->Release(g_sampler_state);
    
#if USE_TTF_FONT
    if (g_font_srv_dx11) g_font_srv_dx11->lpVtbl->Release(g_font_srv_dx11);
    if (g_font_texture_dx11) g_font_texture_dx11->lpVtbl->Release(g_font_texture_dx11);
#else
    if (g_atlas_srv) g_atlas_srv->lpVtbl->Release(g_atlas_srv);
    if (g_atlas_texture) g_atlas_texture->lpVtbl->Release(g_atlas_texture);
#endif
    
    if (g_index_buffer) g_index_buffer->lpVtbl->Release(g_index_buffer);
    if (g_vertex_buffer) g_vertex_buffer->lpVtbl->Release(g_vertex_buffer);
    if (g_input_layout) g_input_layout->lpVtbl->Release(g_input_layout);
    if (g_pixel_shader) g_pixel_shader->lpVtbl->Release(g_pixel_shader);
    if (g_vertex_shader) g_vertex_shader->lpVtbl->Release(g_vertex_shader);
    if (g_rasterizer_state) g_rasterizer_state->lpVtbl->Release(g_rasterizer_state);
    if (g_rtv) g_rtv->lpVtbl->Release(g_rtv);
    if (g_context) g_context->lpVtbl->Release(g_context);
    if (g_swapchain) g_swapchain->lpVtbl->Release(g_swapchain);
    if (g_device) g_device->lpVtbl->Release(g_device);
}

static void flush(void) {
    if (vertex_count == 0) return;
    
#if USE_TTF_FONT
    if (!g_font_srv_dx11) return;
#else
    if (!g_atlas_srv) return;
#endif
    
    if (!update_vertex_buffer() || !update_index_buffer()) return;
    
    UINT stride = sizeof(struct Vertex);
    UINT offset = 0;
    g_context->lpVtbl->IASetVertexBuffers(g_context, 0, 1, &g_vertex_buffer, &stride, &offset);
    g_context->lpVtbl->IASetIndexBuffer(g_context, g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
    g_context->lpVtbl->IASetInputLayout(g_context, g_input_layout);
    g_context->lpVtbl->VSSetShader(g_context, g_vertex_shader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixel_shader, NULL, 0);
    
#if USE_TTF_FONT
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_font_srv_dx11);
#else
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlas_srv);
#endif
    
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_sampler_state);
    g_context->lpVtbl->OMSetBlendState(g_context, g_blend_state, NULL, 0xffffffff);
    g_context->lpVtbl->RSSetState(g_context, g_rasterizer_state);
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->lpVtbl->DrawIndexed(g_context, index_count, 0, 0);
    
    vertex_count = 0;
    index_count = 0;
#if !USE_TTF_FONT
    char logbuf[128];
    sprintf(logbuf, "[DEBUG] flush: g_atlas_srv=%p g_atlas_texture=%p\n", g_atlas_srv, g_atlas_texture);
    OutputDebugStringA(logbuf);
#endif
}

int r_get_text_width(const char* text, int len) {
#if USE_TTF_FONT
    int res = 0;
    const unsigned char* p;
    
    // nullポインタチェックを追加
    if (!text) return 0;
    
    if (len < 0) len = (int)strlen(text);
    
    // textが空文字列の場合もチェック
    if (len == 0) return 0;
    
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
                res += 8; // デフォルトの文字幅
            }
        }
    }
    return res;
#else
    int res = 0;
    if (!text) return 0;
    const unsigned char* p;
    if (len < 0) len = (int)strlen(text);
    for (p = (const unsigned char*)text; *p && len > 0; p++, len--) {
        if ((*p & 0xc0) == 0x80) continue;
        int idx;
        if (*p >= 32 && *p <= 126) {
            idx = ATLAS_FONT + (*p - 32);
        } else {
            idx = ATLAS_FONT + ('?' - 32);
        }
        // 範囲チェック: atlas配列サイズはATLAS_SIZEで仮定
        if (idx >= 0 && idx < (int)(sizeof(atlas)/sizeof(atlas[0]))) {
            res += atlas[idx].w;
        }
    }
    return res;
#endif
}

int r_get_text_height(void) {
#if USE_TTF_FONT
    return 15; // TTFモードでは少し小さめの値
#else
    return 18;
#endif
}

void r_draw_rect(mu_Rect rect, mu_Color color) {
#if USE_TTF_FONT
    push_quad(rect, g_ttf_atlas[ATLAS_WHITE], color);
#else
    push_quad(rect, atlas[ATLAS_WHITE], color);
#endif
}

void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color) {
#if USE_TTF_FONT
    if (!text) return;
    char logbuf[256];
    // --- Shift-JIS→UTF-8変換 ---
//    char utf8[1024];
    char utf8[1024];
//    int wlen = MultiByteToWideChar(CP_ACP, 0, text, -1, NULL, 0);
    int wlen = MultiByteToWideChar(CP932, 0, text, -1, NULL, 0);
    if (wlen <= 0) return;
//    wchar_t* wbuf = (wchar_t*)malloc(wlen * sizeof(wchar_t));
    wchar_t wbuf[1024];
    if (!wbuf) return;
//    MultiByteToWideChar(CP_ACP, 0, text, -1, wbuf, wlen);
    MultiByteToWideChar(CP932, 0, text, -1, wbuf, wlen);
//    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8, sizeof(utf8), NULL, NULL);
    WideCharToMultiByte(CP_UTF8, 0, wbuf, -1, utf8, sizeof(utf8), NULL, NULL);
//    free(wbuf);
    sprintf(logbuf, "[DEBUG] r_draw_text: '%s'\n", utf8);
    OutputDebugStringA(logbuf);
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
        sprintf(logbuf, "[DEBUG] codepoint: %u, glyph: %p\n", codepoint, glyph);
        OutputDebugStringA(logbuf);
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
    // nullポインタチェックを追加
    if (!text) return;
    
    mu_Rect src;
    const unsigned char* p;
    int chr;
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (p = (const unsigned char*)text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        chr = (*p < 127) ? *p : 127; // mu_minの代わりに三項演算子を使用
        src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
#endif
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
#if USE_TTF_FONT
    if (id >= MU_ICON_CLOSE && id < MU_ICON_MAX) {
        // ttf_atlas配列を使用（ビットマップモードと同様）
//        mu_Rect src = g_ttf_atlas[id];
        mu_Rect src = g_ttf_atlas[id];
//        int x = rect.x + (rect.w - src.w) / 2;
//        int y = rect.y + (rect.h - src.h) / 2;
        int x = rect.x;
        int y = rect.y;
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

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    char logbuf[256];
#if USE_TTF_FONT
    sprintf(logbuf, "[DEBUG] push_quad: dst=(%d,%d,%d,%d) src=(%d,%d,%d,%d) color.a=%d\n", dst.x, dst.y, dst.w, dst.h, src.x, src.y, src.w, src.h, color.a);
    OutputDebugStringA(logbuf);
#endif
    if (vertex_count >= MAX_VERTICES - 4 || index_count >= MAX_INDICES - 6) { 
        flush(); 
    }
    float x, y, w, h;
#if USE_TTF_FONT
    if (g_font_atlas.width > 0 && g_font_atlas.height > 0) {
        x = (float)src.x / (float)g_font_atlas.width;
        y = (float)src.y / (float)g_font_atlas.height;
        w = (float)src.w / (float)g_font_atlas.width;
        h = (float)src.h / (float)g_font_atlas.height;
        sprintf(logbuf, "[DEBUG] push_quad UV: x=%.3f y=%.3f w=%.3f h=%.3f\n", x, y, w, h);
        OutputDebugStringA(logbuf);
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
    
    // 座標変換を正規化座標で行う
    float x0 = 2.0f * dst.x / width - 1.0f;
    float y0 = 1.0f - 2.0f * dst.y / height;
    float x1 = 2.0f * (dst.x + dst.w) / width - 1.0f;
    float y1 = 1.0f - 2.0f * (dst.y + dst.h) / height;
    
    // 頂点0（左上）
    vertices[vertex_count + 0].pos[0] = x0;
    vertices[vertex_count + 0].pos[1] = y0;
    vertices[vertex_count + 0].uv[0] = x;
    vertices[vertex_count + 0].uv[1] = y;
    vertices[vertex_count + 0].color[0] = color.r;
    vertices[vertex_count + 0].color[1] = color.g;
    vertices[vertex_count + 0].color[2] = color.b;
    vertices[vertex_count + 0].color[3] = color.a;
    
    // 頂点1（右上）
    vertices[vertex_count + 1].pos[0] = x1;
    vertices[vertex_count + 1].pos[1] = y0;
    vertices[vertex_count + 1].uv[0] = x + w;
    vertices[vertex_count + 1].uv[1] = y;
    vertices[vertex_count + 1].color[0] = color.r;
    vertices[vertex_count + 1].color[1] = color.g;
    vertices[vertex_count + 1].color[2] = color.b;
    vertices[vertex_count + 1].color[3] = color.a;
    
    // 頂点2（左下）
    vertices[vertex_count + 2].pos[0] = x0;
    vertices[vertex_count + 2].pos[1] = y1;
    vertices[vertex_count + 2].uv[0] = x;
    vertices[vertex_count + 2].uv[1] = y + h;
    vertices[vertex_count + 2].color[0] = color.r;
    vertices[vertex_count + 2].color[1] = color.g;
    vertices[vertex_count + 2].color[2] = color.b;
    vertices[vertex_count + 2].color[3] = color.a;
    
    // 頂点3（右下）
    vertices[vertex_count + 3].pos[0] = x1;
    vertices[vertex_count + 3].pos[1] = y1;
    vertices[vertex_count + 3].uv[0] = x + w;
    vertices[vertex_count + 3].uv[1] = y + h;
    vertices[vertex_count + 3].color[0] = color.r;
    vertices[vertex_count + 3].color[1] = color.g;
    vertices[vertex_count + 3].color[2] = color.b;
    vertices[vertex_count + 3].color[3] = color.a;
    
    // インデックス（三角形2つで四角形）
    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;
    
    vertex_count += 4;
    index_count += 6;
}

#if USE_TTF_FONT
// DirectX11用TTFフォント関数

/* TTFフォントステート初期化 (DirectX11版) */
void mu_font_stash_begin_dx11(ID3D11Device* device) {
    g_device = device;
    // アトラス全体をゼロ初期化
    memset(&g_font_atlas, 0, sizeof(g_font_atlas));
    
    // コードポイント検索のための配列にデフォルト値を設定
    for (int i = 0; i < 0xFFFF; i++) {
        g_font_atlas.glyphs[i].codepoint = 0; // 未使用状態
    }
}

/* TTFファイルからフォントを追加 (DirectX11版) */
int mu_font_add_from_file_dx11(const char* path, float size) {
    int i, glyph_count;
    unsigned char* ttf_data;
    stbtt_pack_context spc;
    stbtt_packedchar* pc;
    stbtt_fontinfo info;
    FILE* fp;
    long file_size;

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

    pc = (stbtt_packedchar*)malloc(sizeof(stbtt_packedchar) * 0x10000);
    if (!pc) {
        free(ttf_data);
        return 0;
    }

    g_font_atlas.width = 1024;
    g_font_atlas.height = 1024;
    g_font_atlas.pixel = malloc(g_font_atlas.width * g_font_atlas.height);
    if (!g_font_atlas.pixel) {
        free(ttf_data);
        free(pc);
        return 0;
    }

    stbtt_PackBegin(&spc, g_font_atlas.pixel, g_font_atlas.width, g_font_atlas.height, 0, 1, NULL);
    stbtt_PackSetOversampling(&spc, 1, 1);
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 32, 95, pc); // ASCII
    glyph_count = 95;
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x3040, 0x309F - 0x3040 + 1, pc + glyph_count); // ひらがな
    glyph_count += (0x309F - 0x3040 + 1);
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x30A0, 0x30FF - 0x30A0 + 1, pc + glyph_count); // カタカナ
    glyph_count += (0x30FF - 0x30A0 + 1);
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0xFF00, 0xFFEF - 0xFF00 + 1, pc + glyph_count); // 全角英数
    glyph_count += (0xFFEF - 0xFF00 + 1);
    stbtt_PackFontRange(&spc, ttf_data, 0, size, 0x4E00, 0x5200 - 0x4E00, pc + glyph_count); // 漢字
    glyph_count += (0x5200 - 0x4E00);
    stbtt_PackEnd(&spc);

    stbtt_InitFont(&info, ttf_data, stbtt_GetFontOffsetForIndex(ttf_data, 0));
    float scale = stbtt_ScaleForPixelHeight(&info, size);
    int ascent, descent, lineGap;
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &lineGap);
    g_font_atlas.line_height = (int)((ascent - descent + lineGap) * scale);
    g_font_atlas.line_height = 18;
    g_font_atlas.glyph_count = glyph_count;
    
    for (i = 0; i < glyph_count; i++) {
        int codepoint;
        if (i < 95) codepoint = i + 32;
        else if (i < 95 + (0x309F - 0x3040 + 1)) codepoint = 0x3040 + (i - 95);
        else if (i < 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1)) codepoint = 0x30A0 + (i - 95 - (0x309F - 0x3040 + 1));
        else if (i < 95 + (0x309F - 0x3040 + 1) + (0x30FF - 0x30A0 + 1) + (0xFFEF - 0xFF00 + 1)) codepoint = 0xFF00 + (i - 95 - (0x309F - 0x3040 + 1) - (0x30FF - 0x30A0 + 1));
        else codepoint = 0x4E00 + (i - 95 - (0x309F - 0x3040 + 1) - (0x30FF - 0x30A0 + 1) - (0xFFEF - 0xFF00 + 1));
        
        stbtt_packedchar* glyph = &pc[i];
        g_font_atlas.glyphs[i].codepoint = codepoint;
        g_font_atlas.glyphs[i].x = glyph->x0;
        g_font_atlas.glyphs[i].y = glyph->y0;
        g_font_atlas.glyphs[i].w = glyph->x1 - glyph->x0;
        g_font_atlas.glyphs[i].h = glyph->y1 - glyph->y0;
        g_font_atlas.glyphs[i].xoff = glyph->xoff;
        g_font_atlas.glyphs[i].yoff = glyph->yoff;
        g_font_atlas.glyphs[i].xadvance = glyph->xadvance;
    }
    
    free(pc);
    free(ttf_data);
    return 1;
}

/* フォントアトラスをDirectX11テクスチャに転送 */
void mu_font_stash_end_dx11(void) {
    if (!g_device || !g_font_atlas.pixel) return;

    // UIアイコン/白矩形の座標設定
    int patch_size = 64;
    int ui_patch_x = g_font_atlas.width - patch_size;
    int ui_patch_y = g_font_atlas.height - patch_size;
    
    // 白パッチの描画
    int white_w = 3, white_h = 3;
    int white_x = ui_patch_x + (patch_size - white_w) / 2;
    int white_y = ui_patch_y + 4;
    for (int y = 0; y < white_h; y++) {
        for (int x = 0; x < white_w; x++) {
            ((unsigned char*)g_font_atlas.pixel)[(white_y + y) * g_font_atlas.width + (white_x + x)] = 255;
        }
    }
    
    // ×アイコンの描画 (MU_ICON_CLOSE)
    extern unsigned char close_patch[16 * 16];
    int icon_w = 16, icon_h = 16;
    int icon_x = ui_patch_x + 4;
    int icon_y = ui_patch_y + 12;
    for (int y = 0; y < icon_h; y++) {
        for (int x = 0; x < icon_w; x++) {
            ((unsigned char*)g_font_atlas.pixel)[(icon_y + y) * g_font_atlas.width + (icon_x + x)] = close_patch[y * icon_w + x];
        }
    }
    
    // チェックアイコンの描画 (MU_ICON_CHECK)
    extern unsigned char check_patch[18 * 18];
    int check_w = 18, check_h = 18;
    int check_x = ui_patch_x + 20;
    int check_y = ui_patch_y + 12;
    for (int y = 0; y < check_h; y++) {
        for (int x = 0; x < check_w; x++) {
            ((unsigned char*)g_font_atlas.pixel)[(check_y + y) * g_font_atlas.width + (check_x + x)] = check_patch[y * check_w + x];
        }
    }
    
    // 折りたたみアイコンの描画 (MU_ICON_COLLAPSED)
    extern unsigned char collapsed_patch[5 * 7];
    int collapsed_w = 5, collapsed_h = 7;
    int collapsed_x = ui_patch_x + 36;
    int collapsed_y = ui_patch_y + 12;
    for (int y = 0; y < collapsed_h; y++) {
        for (int x = 0; x < collapsed_w; x++) {
            ((unsigned char*)g_font_atlas.pixel)[(collapsed_y + y) * g_font_atlas.width + (collapsed_x + x)] = collapsed_patch[y * collapsed_w + x];
        }
    }
    
    // 展開アイコンの描画 (MU_ICON_EXPANDED)
    extern unsigned char expanded_patch[7 * 5];
    int expanded_w = 7, expanded_h = 5;
    int expanded_x = ui_patch_x + 52;
    int expanded_y = ui_patch_y + 12;
    for (int y = 0; y < expanded_h; y++) {
        for (int x = 0; x < expanded_w; x++) {
            ((unsigned char*)g_font_atlas.pixel)[(expanded_y + y) * g_font_atlas.width + (expanded_x + x)] = expanded_patch[y * expanded_w + x];
        }
    }

    D3D11_TEXTURE2D_DESC desc = {0};
    desc.Width = g_font_atlas.width;
    desc.Height = g_font_atlas.height;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // RGBA変換
    unsigned char* rgba = (unsigned char*)malloc(g_font_atlas.width * g_font_atlas.height * 4);
    for (int i = 0; i < g_font_atlas.width * g_font_atlas.height; i++) {
        rgba[i*4+0] = 255;
        rgba[i*4+1] = 255;
        rgba[i*4+2] = 255;
        rgba[i*4+3] = ((unsigned char*)g_font_atlas.pixel)[i];
    }

    D3D11_SUBRESOURCE_DATA data = {0};
    data.pSysMem = rgba;
    data.SysMemPitch = g_font_atlas.width * 4;

    HRESULT hr = g_device->lpVtbl->CreateTexture2D(g_device, &desc, &data, &g_font_texture_dx11);
    free(rgba);
    if (FAILED(hr)) return;

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {0};
    srvDesc.Format = desc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_font_texture_dx11, &srvDesc, &g_font_srv_dx11);
    if (FAILED(hr)) return;

    // g_ttf_atlas配列を更新
    g_ttf_atlas[MU_ICON_CLOSE] = mu_rect(icon_x, icon_y, icon_w, icon_h);
    g_ttf_atlas[MU_ICON_CHECK] = mu_rect(check_x, check_y, check_w, check_h);
    g_ttf_atlas[MU_ICON_COLLAPSED] = mu_rect(collapsed_x, collapsed_y, collapsed_w, collapsed_h);
    g_ttf_atlas[MU_ICON_EXPANDED] = mu_rect(expanded_x, expanded_y, expanded_w, expanded_h);
    g_ttf_atlas[ATLAS_WHITE] = mu_rect(white_x, white_y, white_w, white_h);
    
    // DirectX11テクスチャ生成（ここで呼ぶ）
    create_font_texture_dx11();
    
    free(g_font_atlas.pixel);
    g_font_atlas.pixel = NULL;
}

// グリフ検索はttf_font.cのmu_font_find_glyph()を使用
#endif

static void create_shaders(void)
{
    // 頂点シェーダー（COLOR正規化を削除）
    const char* vs_code =
        "struct VS_IN { float2 pos : POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };\n"
        "struct VS_OUT { float4 pos : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };\n"
        "VS_OUT main(VS_IN input) {\n"
        "    VS_OUT output;\n"
        "    output.pos = float4(input.pos.xy, 0.0f, 1.0f);\n"
        "    output.color = input.color;\n"  // 正規化を削除
        "    output.uv = input.uv;\n"
        "    return output;\n"
        "}\n";

    ID3DBlob* vs_blob = NULL;
    ID3DBlob* error_blob = NULL;
    HRESULT hr = D3DCompile(vs_code, strlen(vs_code), NULL, NULL, NULL, "main", "vs_4_0", 0, 0, &vs_blob, &error_blob);

    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA("Vertex Shader Compile Error:\n");
            OutputDebugStringA((char*)error_blob->lpVtbl->GetBufferPointer(error_blob));
            MessageBoxA(NULL, (char*)error_blob->lpVtbl->GetBufferPointer(error_blob), "Vertex Shader Compile Error", MB_OK);
            error_blob->lpVtbl->Release(error_blob);
        }
        else
        {
            OutputDebugStringA("Failed to compile vertex shader (no error blob)\n");
            MessageBoxA(NULL, "Failed to compile vertex shader", "Error", MB_OK);
        }
        return;
    }

    if (vs_blob)
    {
        hr = g_device->lpVtbl->CreateVertexShader(g_device,
            vs_blob->lpVtbl->GetBufferPointer(vs_blob),
            vs_blob->lpVtbl->GetBufferSize(vs_blob),
            NULL, &g_vertex_shader);

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create vertex shader\n");
            MessageBoxA(NULL, "Failed to create vertex shader", "Error", MB_OK);
        }

        // 入力レイアウト
        D3D11_INPUT_ELEMENT_DESC layout[] = {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = g_device->lpVtbl->CreateInputLayout(g_device, layout, 3,
            vs_blob->lpVtbl->GetBufferPointer(vs_blob),
            vs_blob->lpVtbl->GetBufferSize(vs_blob),
            &g_input_layout);

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create input layout\n");
            MessageBoxA(NULL, "Failed to create input layout", "Error", MB_OK);
        }

        vs_blob->lpVtbl->Release(vs_blob);
    }

    // ピクセルシェーダー
    const char* ps_code =
        "Texture2D tex : register(t0);\n"
        "SamplerState smp : register(s0);\n"
        "struct PS_IN { float4 pos : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };\n"
        "float4 main(PS_IN input) : SV_TARGET {\n"
        "    float4 texcol = tex.Sample(smp, input.uv);\n"
        "    return texcol * input.color;\n"
        "}\n";

    ID3DBlob* ps_blob = NULL;
    error_blob = NULL;
    hr = D3DCompile(ps_code, strlen(ps_code), NULL, NULL, NULL, "main", "ps_4_0", 0, 0, &ps_blob, &error_blob);

    if (FAILED(hr))
    {
        if (error_blob)
        {
            OutputDebugStringA("Pixel Shader Compile Error:\n");
            OutputDebugStringA((char*)error_blob->lpVtbl->GetBufferPointer(error_blob));
            MessageBoxA(NULL, (char*)error_blob->lpVtbl->GetBufferPointer(error_blob), "Pixel Shader Compile Error", MB_OK);
            error_blob->lpVtbl->Release(error_blob);
        }
        else
        {
            OutputDebugStringA("Failed to compile pixel shader (no error blob)\n");
            MessageBoxA(NULL, "Failed to compile pixel shader", "Error", MB_OK);
        }
        return;
    }
    {
        hr = g_device->lpVtbl->CreatePixelShader(g_device,
            ps_blob->lpVtbl->GetBufferPointer(ps_blob),
            ps_blob->lpVtbl->GetBufferSize(ps_blob),
            NULL, &g_pixel_shader);

        if (FAILED(hr))
        {
            OutputDebugStringA("Failed to create pixel shader\n");
            MessageBoxA(NULL, "Failed to create pixel shader", "Error", MB_OK);
        }

        ps_blob->lpVtbl->Release(ps_blob);
    }
}

// --- ステート生成（ブレンド・ラスタライザ・サンプラー） ---
static void create_states(void) {
    // ブレンドステート（アルファブレンド）
    D3D11_BLEND_DESC blendDesc = {0};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_device->lpVtbl->CreateBlendState(g_device, &blendDesc, &g_blend_state);
    // ラスタライザステート
    D3D11_RASTERIZER_DESC rastDesc = {0};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.ScissorEnable = TRUE;
    rastDesc.DepthClipEnable = TRUE;
    g_device->lpVtbl->CreateRasterizerState(g_device, &rastDesc, &g_rasterizer_state);
    // サンプラーステート
    D3D11_SAMPLER_DESC sampDesc = {0};
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    g_device->lpVtbl->CreateSamplerState(g_device, &sampDesc, &g_sampler_state);
}

// 頂点バッファ・インデックスバッファ更新（DX11用）
static int update_vertex_buffer(void) {
    if (!g_vertex_buffer || vertex_count == 0) return 0;
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) return 0;
    memcpy(resource.pData, vertices, vertex_count * sizeof(struct Vertex));
    g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_vertex_buffer, 0);
    return 1;
}

static int update_index_buffer(void) {
    if (!g_index_buffer || index_count == 0) return 0;
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_index_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) return 0;
    memcpy(resource.pData, indices, index_count * sizeof(short));
    g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_index_buffer, 0);
    return 1;
}

void r_set_clip_rect(mu_Rect rect)
{
    // 異常値をウィンドウサイズで制限
    if (rect.w > width || rect.w <= 0) rect.w = width;
    if (rect.h > height || rect.h <= 0) rect.h = height;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    char buf[128];
    sprintf(buf, "r_set_clip_rect: x=%d y=%d w=%d h=%d\n", rect.x, rect.y, rect.w, rect.h);
    OutputDebugStringA(buf);
    D3D11_RECT scissor_rect = {
        rect.x,
        rect.y,
        rect.x + rect.w,
        rect.y + rect.h
    };
    g_context->lpVtbl->RSSetScissorRects(g_context, 1, &scissor_rect);
}

void r_clear(mu_Color clr) {
    float color[4] = { clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f };
    if (g_context && g_rtv) {
        g_context->lpVtbl->ClearRenderTargetView(g_context, g_rtv, color);
    }
}

void r_present(void) {
    if (g_swapchain) {
        g_swapchain->lpVtbl->Present(g_swapchain, 1, 0);
    }
}

void resize_buffers(int new_width, int new_height) {
    // バッファリサイズ処理（簡易）
    width = new_width;
    height = new_height;
    // TODO: 必要ならRTVやバッファ再生成
}

// DirectX11 IID_ID3D11Texture2D 定義（未解決リンクエラー対策）
#ifndef INITGUID
#define INITGUID
#endif
#include <initguid.h>
const GUID IID_ID3D11Texture2D = {0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c}};

void UpdateProjectionMatrix()
{
    if (!g_context) return;

    // ビューポート設定
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f };
    g_context->lpVtbl->RSSetViewports(g_context, 1, &vp);

    // レンダーターゲット設定
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);

    // プリミティブトポロジー設定
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void InitD3D(HWND hwnd) {
    DXGI_SWAP_CHAIN_DESC scd = {0};
    scd.BufferCount = 1;
    scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.Width = width;
    scd.BufferDesc.Height = height;
    scd.BufferDesc.RefreshRate.Numerator = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow = hwnd;
    scd.SampleDesc.Count = 1;
    scd.SampleDesc.Quality = 0;
    scd.Windowed = TRUE;
    scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
        D3D11_SDK_VERSION, &scd, &g_swapchain, &g_device, NULL, &g_context);
    if (FAILED(hr)) {
        MessageBoxA(hwnd, "DirectX11 device creation failed", "Error", MB_OK);
        return;
    }

    ID3D11Texture2D* backBuffer = NULL;
    hr = g_swapchain->lpVtbl->GetBuffer(g_swapchain, 0, &IID_ID3D11Texture2D, (void**)&backBuffer);
    if (FAILED(hr)) {
        MessageBoxA(hwnd, "Failed to get back buffer", "Error", MB_OK);
        return;
    }
    hr = g_device->lpVtbl->CreateRenderTargetView(g_device, (ID3D11Resource*)backBuffer, NULL, &g_rtv);
    backBuffer->lpVtbl->Release(backBuffer);
    if (FAILED(hr)) {
        MessageBoxA(hwnd, "Failed to create render target view", "Error", MB_OK);
        return;
    }
    r_init(g_device, g_context, g_swapchain, g_rtv);
}

void CleanD3D() {
    r_cleanup();
}

extern float bg[4];

void r_draw(void)
{
    if (!g_ctx || !g_context || !g_rtv) return;

    // フレーム開始処理（dx11_begin_frame相当）
    // レンダーターゲットの設定
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);
    
    // インプットレイアウトの設定
    g_context->lpVtbl->IASetInputLayout(g_context, g_input_layout);
    
    // シェーダーの設定
    g_context->lpVtbl->VSSetShader(g_context, g_vertex_shader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixel_shader, NULL, 0);
    
    // サンプラーとテクスチャの設定
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_sampler_state);
#if USE_TTF_FONT
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_font_srv_dx11);
#else
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlas_srv);
#endif
    
    // ブレンドステートの設定
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_context->lpVtbl->OMSetBlendState(g_context, g_blend_state, blendFactor, 0xffffffff);
    
    // ラスタライザステートの設定
    g_context->lpVtbl->RSSetState(g_context, g_rasterizer_state);
    
    // プリミティブトポロジーの設定
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ビューポート設定
    UpdateProjectionMatrix();

    // microuiフレーム処理
    extern void process_frame(mu_Context* ctx);
    process_frame(g_ctx); // UI生成

    // バックバッファのクリア
    r_clear(mu_color((int)bg[0], (int)bg[1], (int)bg[2], 255));

    // 頂点・インデックスカウントをリセット
    vertex_count = 0;
    index_count = 0;

    // microuiコマンド処理
    mu_Command* cmd = NULL;
    while (mu_next_command(g_ctx, &cmd))
    {
        char logbuf[128];
        sprintf(logbuf, "[DEBUG] mu_Command type: %d\n", cmd->type);
        OutputDebugStringA(logbuf);
        switch (cmd->type)
        {
        case MU_COMMAND_CLIP:
            r_set_clip_rect(cmd->clip.rect);
            break;
        case MU_COMMAND_RECT:
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
            break;
        case MU_COMMAND_TEXT:
            r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
            break;
        case MU_COMMAND_ICON:
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            break;
        }
    }

    // 残っている頂点をフラッシュ
    flush();

    // 画面に表示
    r_present();
}
