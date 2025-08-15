#pragma once
#include <Windows.h>
#include <d3d11.h>
#include "microui.h"

// �֐��v���g�^�C�v�irenderer.h�ɏ����ADirectX11�p�j
void r_init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain, ID3D11RenderTargetView* rtv);
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

#define MAX_VERTICES 16384

// ���_�\���̂�DX11Renderer�T���v���ɍ��킹�ďC��
struct Vertex {
    float pos[2];      // x, y (-1.0�`1.0�ɐ��K��)
    float uv[2];       // u, v (0.0�`1.0)
    unsigned char color[4]; // r, g, b, a (0�`255)
};

void InitD3D(HWND hwnd);
void CleanD3D();
