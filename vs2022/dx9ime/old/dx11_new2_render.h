#pragma once
#include <Windows.h>
#include <d3d11.h>
#include "microui.h"



#if USE_TTF_FONT
#include "ttf_font.h"
// DirectX11用TTF関数プロトタイプ
void mu_font_stash_begin_dx11(ID3D11Device* device);
int mu_font_add_from_file_dx11(const char* path, float size);
void mu_font_stash_end_dx11(void);
struct mu_font_glyph* mu_font_find_glyph(unsigned int codepoint);
#endif

// 関数プロトタイプ（renderer.hに準拠、DirectX11用）
void r_init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain, ID3D11RenderTargetView* rtv);
void r_cleanup(void);
void r_draw(void);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
void r_set_clip_rect(mu_Rect rect);
int r_get_text_width(const char* text, int len);
int r_get_text_height(void);
void r_clear(mu_Color clr);
void r_present(void);
void resize_buffers(int new_width, int new_height);
void process_frame(mu_Context* ctx);
void log_window(mu_Context* ctx);
void style_window(mu_Context* ctx);
void UpdateProjectionMatrix();

#define MAX_VERTICES 16384

// 頂点構造体をDX11Rendererサンプルに合わせて修正
struct Vertex
{
    float pos[2];      // offset 0, size 8
    float uv[2];       // offset 8, size 8
    unsigned char color[4]; // offset 16, size 4
    unsigned char _pad[12]; // offset 20, size 12 (for 32-byte alignment)
};
void InitD3D(HWND hwnd);
void CleanD3D();
