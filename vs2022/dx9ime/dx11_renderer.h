#pragma once

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include "microui.h"
#include "dx11_atlas.h"

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxgi.lib")

// C言語でID3D10Blobを扱うためのヘルパーマクロ
#define ID3D10Blob_GetBufferPointer(This) ((This)->lpVtbl->GetBufferPointer((This)))
#define ID3D10Blob_GetBufferSize(This) ((This)->lpVtbl->GetBufferSize((This)))
#define ID3D10Blob_Release(This) ((This)->lpVtbl->Release((This)))

// 定数
#define MAX_VERTICES 65536  // 頂点バッファの最大サイズ

// DX11頂点構造体
typedef struct {
    float pos[2];    // 位置 (x, y)
    float uv[2];     // テクスチャ座標 (u, v)
    unsigned char color[4]; // 色 (RGBA)
} DX11Vertex;

// 簡易microuiコマンドリスト
typedef struct {
    int type;
    mu_Rect rect;
    mu_Color color;
    char text[256];
} SimpleCommand;

// 関数プロトタイプ
HRESULT dx11_init(HWND hWnd, int width, int height);
void dx11_cleanup(void);
void dx11_clear_screen(float r, float g, float b, float a);
void dx11_present(void);
void dx11_begin_frame(void);
void dx11_end_frame(void);
void dx11_flush_vertices(void);
void dx11_push_quad(mu_Rect dst, mu_Color color);
void dx11_push_quad_atlas(mu_Rect dst, mu_Rect src, mu_Color color);
void dx11_draw_text(const char* text, int x, int y, mu_Color color);
void dx11_draw_icon(int icon_id, mu_Rect rect, mu_Color color);
void dx11_set_clip_rect(mu_Rect rect);

// コマンドバッファ操作
void dx11_begin_commands(void);
void dx11_add_rect_command(mu_Rect rect, mu_Color color);
void dx11_add_text_command(const char* text, int x, int y, mu_Color color);
void dx11_end_commands(void);
void dx11_render_commands(void);

// アクセサ関数
ID3D11Device* dx11_get_device(void);
ID3D11DeviceContext* dx11_get_context(void);
SimpleCommand* dx11_get_commands(void);
int dx11_get_command_count(void);

// エラーログ関数
void dx11_log_error(HRESULT hr, const char* operation);

// シェーダーコンパイル関数
HRESULT CompileShaderFromString(const char* shaderCode, const char* entryPoint, 
                               const char* shaderModel, ID3DBlob** ppBlob);
