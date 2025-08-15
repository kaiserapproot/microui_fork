#ifndef RENDERER_H
#define RENDERER_H

#include <d3d9.h>
#include "microui.h"

// 関数プロトタイプ
void r_init(LPDIRECT3DDEVICE9 device);
void r_cleanup(void);
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
// 定数
#define MAX_VERTICES 16384

// 頂点構造体
struct Vertex
{
    float x, y, z;    // 位置
    DWORD color;      // 頂点カラー
    float u, v;       // テクスチャ座標
};

// Direct3D関連の関数プロトタイプ
void InitD3D(HWND hwnd);
void CleanD3D();

#endif // RENDERER_H