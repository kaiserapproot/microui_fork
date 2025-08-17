#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")

#include <d3dcompiler.h>
#include <stdbool.h>
#include <stdio.h>  // sprintf�̂��߂ɒǉ�
#include "dx11ttfrender.h"
#include "microui.h"

static mu_Rect mu_rect(int x, int y, int w, int h) {
    mu_Rect r;
    r.x = x; r.y = y; r.w = w; r.h = h;
    return r;
}

#define USE_TTF_FONT 1 // 1: TTF, 0: atlas.inl
#if USE_TTF_FONT
#include "ttf_font.h"
extern int g_ui_white_rect[4];
extern mu_Rect* g_ttf_atlas;
#endif
#include "atlas.inl"

#if USE_TTF_FONT
static mu_Rect get_ttf_white_rect(void) {
    mu_Rect r;
    
    // g_ui_white_rect�̍��W���L�����`�F�b�N
    if (g_ui_white_rect[0] >= 0 && g_ui_white_rect[1] >= 0 && 
        g_ui_white_rect[2] > 0 && g_ui_white_rect[3] > 0) {
        
        r.x = g_ui_white_rect[0];
        r.y = g_ui_white_rect[1];
        r.w = g_ui_white_rect[2];
        r.h = g_ui_white_rect[3];
    } else {
        // �����ȏꍇ�͐ÓI�A�g���X��white_patch���g�p
        r = atlas[ATLAS_WHITE];
        
        // �f�o�b�O�o�͕͂p�ɂɂȂ�Ȃ��悤�ɂ���
        static BOOL warned = FALSE;
        if (!warned) {
            OutputDebugStringA("WARNING: Invalid g_ui_white_rect, using static atlas white_patch instead\n");
            warned = TRUE;
        }
    }
    
    return r;
}
#endif

// �O���[�o���ϐ�
static ID3D11Device* g_device = NULL;
static ID3D11DeviceContext* g_context = NULL;
static IDXGISwapChain* g_swapchain = NULL;
static ID3D11RenderTargetView* g_rtv = NULL;

extern int width;
extern int height;
extern mu_Context* g_ctx;

static ID3D11Buffer* g_vertex_buffer = NULL;
static ID3D11Buffer* g_index_buffer = NULL;
static ID3D11Texture2D* g_atlas_texture = NULL;
static ID3D11ShaderResourceView* g_atlas_srv = NULL;
static ID3D11VertexShader* g_vertex_shader = NULL;
static ID3D11PixelShader* g_pixel_shader = NULL;
static ID3D11InputLayout* g_input_layout = NULL;
static ID3D11BlendState* g_blend_state = NULL;
static ID3D11SamplerState* g_sampler_state = NULL;
static ID3D11RasterizerState* g_rasterizer_state = NULL;

static struct Vertex vertices[MAX_VERTICES];
static short indices[MAX_VERTICES * 3 / 2];
#define MAX_INDICES (MAX_VERTICES * 3 / 2)
static int vertex_count = 0;
static int index_count = 0;
static void create_atlas_texture(void);
static void create_shaders(void);
static void create_states(void);
static void flush(void);

void r_init(ID3D11Device* device, ID3D11DeviceContext* context, IDXGISwapChain* swapchain, ID3D11RenderTargetView* rtv)
{
    g_device = device;
    g_context = context;
    g_swapchain = swapchain;
    g_rtv = rtv;
    
    // �f�o�b�O�o��
    OutputDebugStringA("r_init: Starting renderer initialization\n");
    
#if USE_TTF_FONT
    // TTF�t�H���g�̏�����������ǉ�
    OutputDebugStringA("r_init: Initializing TTF font\n");
    mu_font_stash_begin();
    
    // �����̉\�ȃt�H���g�p�X������ - �V�X�e�����̂悭����t�H���g�f�B���N�g�����܂߂�
    BOOL fontLoaded = FALSE;
    const char* fontPaths[] = {
        "MPLUS1p-Regular.ttf",                   // �J�����g�f�B���N�g��
        "fonts\\MPLUS1p-Regular.ttf",            // fonts�T�u�f�B���N�g��
        "..\\fonts\\MPLUS1p-Regular.ttf",        // �e�f�B���N�g����
        "C:\\Windows\\Fonts\\msgothic.ttc",      // MS Gothic�t�H���g�i���{��Windows�j
        "C:\\Windows\\Fonts\\meiryo.ttc",        // ���C���I�t�H���g�i���{��Windows�j
        "C:\\Windows\\Fonts\\arial.ttf",         // Arial�t�H���g�i�p��j
        "C:\\Windows\\Fonts\\segoeui.ttf"        // Segoe UI�i�p��j
    };
    
    for (int i = 0; i < sizeof(fontPaths)/sizeof(fontPaths[0]); i++) {
        char debug[256];
        sprintf(debug, "r_init: Trying to load font from: %s\n", fontPaths[i]);
        OutputDebugStringA(debug);
        
        if (mu_font_add_from_file(fontPaths[i], 16.0f)) {
            sprintf(debug, "r_init: Successfully loaded font from: %s\n", fontPaths[i]);
            OutputDebugStringA(debug);
            fontLoaded = TRUE;
            break;
        }
    }
    
    if (!fontLoaded) {
        OutputDebugStringA("r_init: ERROR: Failed to load any TTF font. UI may not display correctly.\n");
        // Note: Continue anyway - we'll handle this situation in create_atlas_texture
    }
    
    mu_font_stash_end();
    
    // g_ttf_atlas�̏�Ԃ��o��
    if (g_ttf_atlas) {
        char debug[128];
        sprintf(debug, "r_init: g_ttf_atlas available, ATLAS_WHITE: (%d,%d,%d,%d)\n",
            g_ttf_atlas[ATLAS_WHITE].x, g_ttf_atlas[ATLAS_WHITE].y,
            g_ttf_atlas[ATLAS_WHITE].w, g_ttf_atlas[ATLAS_WHITE].h);
        OutputDebugStringA(debug);
    } else {
        OutputDebugStringA("r_init: WARNING: g_ttf_atlas is NULL\n");
    }
#endif

    // ���_�o�b�t�@����
    D3D11_BUFFER_DESC vbdesc = { 0 };
    vbdesc.Usage = D3D11_USAGE_DYNAMIC;
    vbdesc.ByteWidth = sizeof(struct Vertex) * MAX_VERTICES;
    vbdesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr = g_device->lpVtbl->CreateBuffer(g_device, &vbdesc, NULL, &g_vertex_buffer);
    if (FAILED(hr)) {
        OutputDebugStringA("r_init: Failed to create vertex buffer\n");
    }
    
    // �C���f�b�N�X�o�b�t�@����
    D3D11_BUFFER_DESC ibdesc = { 0 };
    ibdesc.Usage = D3D11_USAGE_DYNAMIC;
    ibdesc.ByteWidth = sizeof(short) * MAX_VERTICES * 3 / 2;
    ibdesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibdesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = g_device->lpVtbl->CreateBuffer(g_device, &ibdesc, NULL, &g_index_buffer);
    if (FAILED(hr)) {
        OutputDebugStringA("r_init: Failed to create index buffer\n");
    }
    
    // atlas�e�N�X�`������
    create_atlas_texture();
    
    // �V�F�[�_�[����
    create_shaders();
    
    // �X�e�[�g����
    create_states();
    
    OutputDebugStringA("r_init: Renderer initialization complete\n");
}

// --- �e�N�X�`�������iatlas.inl�̃r�b�g�}�b�v��DirectX11�e�N�X�`���ցj ---
static void create_atlas_texture(void)
{
#if USE_TTF_FONT
    // TTF�A�g���X�ig_font_atlas.pixel�j����DirectX11�e�N�X�`������
    if (g_font_atlas.pixel && g_font_atlas.width > 0 && g_font_atlas.height > 0) {
        int w = g_font_atlas.width;
        int h = g_font_atlas.height;
        
        // �f�o�b�O�o�́FTTF�A�g���X���
        char debug_info[256];
        sprintf(debug_info, "TTF Atlas: size=%dx%d, pixel data available\n", w, h);
        OutputDebugStringA(debug_info);
        
        // --- white_patch���W�̏������imu_font_stash_end�����j ---
        int white_w = 3, white_h = 3;
        int white_x = w - white_w - 1;
        int white_y = h - white_h - 1;
        
        // white_patch�̈�𔒂œh��Ԃ�
        for (int py = 0; py < white_h; ++py) {
            for (int px = 0; px < white_w; ++px) {
                int dst_x = white_x + px;
                int dst_y = white_y + py;
                if (dst_x < 0 || dst_x >= w || dst_y < 0 || dst_y >= h) continue;
                ((unsigned char*)g_font_atlas.pixel)[dst_y * w + dst_x] = 255;
            }
        }
        
        // ���W���̐ݒ�
        g_ui_white_rect[0] = white_x;
        g_ui_white_rect[1] = white_y;
        g_ui_white_rect[2] = white_w;
        g_ui_white_rect[3] = white_h;
        
        if (g_ttf_atlas) {
            g_ttf_atlas[ATLAS_WHITE].x = white_x;
            g_ttf_atlas[ATLAS_WHITE].y = white_y;
            g_ttf_atlas[ATLAS_WHITE].w = white_w;
            g_ttf_atlas[ATLAS_WHITE].h = white_h;
            
            // �f�o�b�O�o�́Fwhite_patch���W
            sprintf(debug_info, "White patch: x=%d, y=%d, w=%d, h=%d\n", 
                white_x, white_y, white_w, white_h);
            OutputDebugStringA(debug_info);
        } else {
            OutputDebugStringA("ERROR: g_ttf_atlas is NULL, can't set white_patch coordinates\n");
        }
        
        // DirectX11�e�N�X�`���쐬
        D3D11_TEXTURE2D_DESC desc = { 0 };
        desc.Width = w;
        desc.Height = h;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        
        // �A���t�@�l�݂̂̃e�N�X�`����RGBA�ɕϊ�
        unsigned char* rgba = (unsigned char*)malloc(w * h * 4);
        if (!rgba) {
            OutputDebugStringA("ERROR: Failed to allocate memory for RGBA texture\n");
            goto use_static_atlas; // �������m�ێ��s���͐ÓI�A�g���X�Ƀt�H�[���o�b�N
        }
        
        for (int i = 0; i < w * h; i++) {
            unsigned char alpha = ((unsigned char*)g_font_atlas.pixel)[i];
            rgba[i * 4 + 0] = 255;  // R
            rgba[i * 4 + 1] = 255;  // G
            rgba[i * 4 + 2] = 255;  // B
            rgba[i * 4 + 3] = alpha; // A
        }
        
        D3D11_SUBRESOURCE_DATA data = { 0 };
        data.pSysMem = rgba;
        data.SysMemPitch = w * 4;
        
        HRESULT hr = g_device->lpVtbl->CreateTexture2D(g_device, &desc, &data, &g_atlas_texture);
        free(rgba);
        
        if (SUCCEEDED(hr)) {
            hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_atlas_texture, NULL, &g_atlas_srv);
            if (SUCCEEDED(hr)) {
                OutputDebugStringA("TTF texture created successfully\n");
                
                // g_ttf_atlas�̃f�o�b�O�o��
                if (g_ttf_atlas) {
                    // ATLAS_WHITE
                    sprintf(debug_info, "g_ttf_atlas[ATLAS_WHITE]: x=%d, y=%d, w=%d, h=%d\n",
                        g_ttf_atlas[ATLAS_WHITE].x, g_ttf_atlas[ATLAS_WHITE].y,
                        g_ttf_atlas[ATLAS_WHITE].w, g_ttf_atlas[ATLAS_WHITE].h);
                    OutputDebugStringA(debug_info);
                    
                    // ASCII 'A'�̃O���t���i�������݂���΁j
                    if (ATLAS_FONT + ('A' - 32) < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
                        sprintf(debug_info, "g_ttf_atlas[ATLAS_FONT+'A'-32]: x=%d, y=%d, w=%d, h=%d\n",
                            g_ttf_atlas[ATLAS_FONT+('A'-32)].x, g_ttf_atlas[ATLAS_FONT+('A'-32)].y,
                            g_ttf_atlas[ATLAS_FONT+('A'-32)].w, g_ttf_atlas[ATLAS_FONT+('A'-32)].h);
                        OutputDebugStringA(debug_info);
                    }
                }
                return; // ���������̂ł����ŏI��
            } else {
                OutputDebugStringA("ERROR: Failed to create shader resource view\n");
                if (g_atlas_texture) {
                    g_atlas_texture->lpVtbl->Release(g_atlas_texture);
                    g_atlas_texture = NULL;
                }
            }
        } else {
            OutputDebugStringA("ERROR: Failed to create TTF texture\n");
        }
    } else {
        OutputDebugStringA("ERROR: TTF font atlas pixel data is NULL or invalid dimensions\n");
        OutputDebugStringA("       Did you call mu_font_stash_begin() and mu_font_add_from_file()?\n");
    }
    
    // TTF�e�N�X�`�������Ɏ��s�����ꍇ�͐ÓI�r�b�g�}�b�v�A�g���X�Ƀt�H�[���o�b�N
use_static_atlas:
    OutputDebugStringA("FALLBACK: Using static bitmap atlas instead of TTF font\n");
#endif

    // atlas.inl�̃r�b�g�}�b�v�i�]�������j
    OutputDebugStringA("Creating static bitmap atlas texture\n");
    
    D3D11_TEXTURE2D_DESC desc = { 0 };
    desc.Width = ATLAS_WIDTH;
    desc.Height = ATLAS_HEIGHT;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    
    unsigned char* rgba = (unsigned char*)malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    if (!rgba) {
        OutputDebugStringA("ERROR: Failed to allocate memory for static atlas texture\n");
        return;
    }
    
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        unsigned char alpha = atlas_texture[i];
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = alpha;
    }
    
    D3D11_SUBRESOURCE_DATA data = { 0 };
    data.pSysMem = rgba;
    data.SysMemPitch = ATLAS_WIDTH * 4;
    
    HRESULT hr = g_device->lpVtbl->CreateTexture2D(g_device, &desc, &data, &g_atlas_texture);
    free(rgba);
    
    if (SUCCEEDED(hr)) {
        hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_atlas_texture, NULL, &g_atlas_srv);
        if (SUCCEEDED(hr)) {
            OutputDebugStringA("Static atlas texture created successfully\n");
            
            // TTF���[�h���ɂ��ÓI�A�g���X���g�p���邽�߂̓��ʏ���
#if USE_TTF_FONT
            if (g_ttf_atlas) {
                // �ÓI�A�g���X���g�p���邪�ATTF���[�h������g_ttf_atlas�z����X�V
                for (int i = 0; i <= ATLAS_WHITE; i++) {
                    g_ttf_atlas[i] = atlas[i];
                }
                g_ui_white_rect[0] = atlas[ATLAS_WHITE].x;
                g_ui_white_rect[1] = atlas[ATLAS_WHITE].y;
                g_ui_white_rect[2] = atlas[ATLAS_WHITE].w;
                g_ui_white_rect[3] = atlas[ATLAS_WHITE].h;
                OutputDebugStringA("Updated g_ttf_atlas with static atlas coordinates\n");
            }
#endif
        } else {
            OutputDebugStringA("ERROR: Failed to create shader resource view for static atlas\n");
        }
    } else {
        OutputDebugStringA("ERROR: Failed to create static atlas texture\n");
    }
}

static void create_shaders(void)
{
    // ���_�V�F�[�_�[�iCOLOR���K�����폜�j
    const char* vs_code =
        "struct VS_IN { float2 pos : POSITION; float2 uv : TEXCOORD; float4 color : COLOR; };\n"
        "struct VS_OUT { float4 pos : SV_POSITION; float4 color : COLOR; float2 uv : TEXCOORD; };\n"
        "VS_OUT main(VS_IN input) {\n"
        "    VS_OUT output;\n"
        "    output.pos = float4(input.pos.xy, 0.0f, 1.0f);\n"
        "    output.color = input.color;\n"  // ���K�����폜
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

        // ���̓��C�A�E�g
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

    // �s�N�Z���V�F�[�_�[
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

    if (ps_blob)
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

// --- �X�e�[�g�����i�u�����h�E���X�^���C�U�E�T���v���[�j ---
static void create_states(void)
{
    // �u�����h�X�e�[�g�i�A���t�@�u�����h�j
    D3D11_BLEND_DESC blendDesc = { 0 };
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    g_device->lpVtbl->CreateBlendState(g_device, &blendDesc, &g_blend_state);
    // ���X�^���C�U�X�e�[�g
    D3D11_RASTERIZER_DESC rastDesc = { 0 };
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.ScissorEnable = TRUE;
    rastDesc.DepthClipEnable = TRUE;
    g_device->lpVtbl->CreateRasterizerState(g_device, &rastDesc, &g_rasterizer_state);
    // �T���v���[�X�e�[�g
    D3D11_SAMPLER_DESC sampDesc = { 0 };
    sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sampDesc.MinLOD = 0;
    sampDesc.MaxLOD = D3D11_FLOAT32_MAX;
    g_device->lpVtbl->CreateSamplerState(g_device, &sampDesc, &g_sampler_state);
}

void r_cleanup(void)
{
    if (g_blend_state) g_blend_state->lpVtbl->Release(g_blend_state);
    if (g_sampler_state) g_sampler_state->lpVtbl->Release(g_sampler_state);
    if (g_atlas_srv) g_atlas_srv->lpVtbl->Release(g_atlas_srv);
    if (g_atlas_texture) g_atlas_texture->lpVtbl->Release(g_atlas_texture);
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


void r_set_clip_rect(mu_Rect rect)
{
    // �ُ�l���E�B���h�E�T�C�Y�Ő���
    if (rect.w > width || rect.w <= 0) rect.w = width;
    if (rect.h > height || rect.h <= 0) rect.h = height;
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    //char buf[128];
    //sprintf(buf, "r_set_clip_rect: x=%d y=%d w=%d h=%d\n", rect.x, rect.y, rect.w, rect.h);
    //OutputDebugStringA(buf);
    D3D11_RECT scissor_rect = {
        rect.x,
        rect.y,
        rect.x + rect.w,
        rect.y + rect.h
    };
    g_context->lpVtbl->RSSetScissorRects(g_context, 1, &scissor_rect);
}

int r_get_text_width(const char* text, int len)
{
    if (!text) return 0;
    if (len < 0) len = (int)strlen(text);
    
#if USE_TTF_FONT
    // TTF���[�h���L������g_font_atlas.pixel���L���ł���΁ATTF�̃e�L�X�g���v�Z���g�p
    BOOL use_static_fallback = (g_font_atlas.pixel == NULL || g_font_atlas.width <= 0 || g_font_atlas.height <= 0);
    
    if (!use_static_fallback) {
        // --- UTF-8�Ή��e�L�X�g���v�Z ---
        int res = 0;
        const unsigned char* p = (const unsigned char*)text;
        
        while (*p && len > 0) {
            unsigned int codepoint = 0;
            int bytes_read = 0;
            
            // UTF-8�f�R�[�h����
            if ((*p & 0x80) == 0) { 
                // ASCII�����i7�r�b�g�j
                codepoint = *p++; 
                bytes_read = 1;
            }
            else if ((*p & 0xE0) == 0xC0) { 
                // 2�o�C�g����
                codepoint = (*p++ & 0x1F) << 6; 
                bytes_read = 2;
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
                else { p++; continue; }
            }
            else if ((*p & 0xF0) == 0xE0) { 
                // 3�o�C�g�����i���{��Ȃǁj
                codepoint = (*p++ & 0x0F) << 12; 
                bytes_read = 3;
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6; 
                else { p++; continue; }
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
                else { p++; continue; }
            }
            else if ((*p & 0xF8) == 0xF0) { 
                // 4�o�C�g�����i�G�����Ȃǁj
                codepoint = (*p++ & 0x07) << 18; 
                bytes_read = 4;
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 12; 
                else { p++; continue; }
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F) << 6; 
                else { p++; continue; }
                if (*p && (*p & 0xC0) == 0x80) codepoint |= (*p++ & 0x3F); 
                else { p++; continue; }
            }
            else { 
                // �s����UTF-8�V�[�P���X
                p++; 
                bytes_read = 1;
                continue; 
            }
            
            len -= bytes_read;
            
            // �O���t���������ĕ������Z
            struct mu_font_glyph* glyph = mu_font_find_glyph(codepoint);
            if (glyph && glyph->w > 0) {
                res += glyph->xadvance;
            }
            else if (codepoint >= 32 && codepoint <= 126) {
                // ASCII�̏ꍇ�ŁATTF�t�H���g�Ɍ�����Ȃ��ꍇ�͐ÓI�r�b�g�}�b�v����
                int idx = ATLAS_FONT + (codepoint - 32);
                if (idx >= 0 && idx < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
                    res += atlas[idx].w;
                }
            }
            else {
                // �t�H�[���o�b�N�F�f�t�H���g��
                res += 8; // �f�t�H���g�̕�����
            }
        }
        
        return res;
    }
#endif

    // �ÓI�r�b�g�}�b�v�t�H���g���g�p�������v�Z�i�t�H�[���o�b�N�܂��͒ʏ탂�[�h�j
    int res = 0;
    const unsigned char* p;
    
    for (p = (const unsigned char*)text; *p && len > 0; p++, len--) {
        if ((*p & 0xc0) == 0x80) continue;
        
        int idx;
        if (*p >= 32 && *p <= 126) {
            idx = ATLAS_FONT + (*p - 32);
        }
        else {
            idx = ATLAS_FONT + ('?' - 32);
        }
        
        if (idx >= 0 && idx < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
            res += atlas[idx].w;
        }
    }
    
    return res;
}

int r_get_text_height(void)
{
    return 18;
}

void r_clear(mu_Color clr)
{
    float color[4] = { clr.r / 255.0f, clr.g / 255.0f, clr.b / 255.0f, clr.a / 255.0f };
    if (g_context && g_rtv)
    {
        g_context->lpVtbl->ClearRenderTargetView(g_context, g_rtv, color);
    }
}

void r_present(void)
{
    if (g_swapchain)
    {
        g_swapchain->lpVtbl->Present(g_swapchain, 1, 0);
    }
}

void resize_buffers(int new_width, int new_height)
{
    // �o�b�t�@���T�C�Y�����i�ȈՁj
    width = new_width;
    height = new_height;
    // TODO: �K�v�Ȃ�RTV��o�b�t�@�Đ���
}

// --- log_window, style_window �̐��`���� ---
void log_window(mu_Context* ctx)
{
    extern char logbuf[];
    extern int logbuf_updated;
    if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200)))
    {
        int layout1[1] = { -1 };
        int layout2[2] = { -70, -1 };
        mu_layout_row(ctx, 1, layout1, -25);
        mu_begin_panel(ctx, "Log Output");
        mu_Container* panel = mu_get_current_container(ctx);
        mu_layout_row(ctx, 1, layout1, -1);
        mu_text(ctx, logbuf);
        mu_end_panel(ctx);
        if (logbuf_updated)
        {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }
        mu_layout_row(ctx, 2, layout2, 0);
        static char buf[128];
        int submitted = 0;
        if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT)
        {
            mu_set_focus(ctx, ctx->last_id);
            submitted = 1;
        }
        if (mu_button(ctx, "Submit")) { submitted = 1; }
        if (submitted)
        {
            extern void write_log(const char* text);
            write_log(buf);
            buf[0] = '\0';
        }
        mu_end_window(ctx);
    }
}

void style_window(mu_Context* ctx)
{
    static struct { const char* label; int idx; } colors[] = {
      { "text:", MU_COLOR_TEXT },
      { "border:", MU_COLOR_BORDER },
      { "windowbg:", MU_COLOR_WINDOWBG },
      { "titlebg:", MU_COLOR_TITLEBG },
      { "titletext:", MU_COLOR_TITLETEXT },
      { "panelbg:", MU_COLOR_PANELBG },
      { "button:", MU_COLOR_BUTTON },
      { "buttonhover:", MU_COLOR_BUTTONHOVER },
      { "buttonfocus:", MU_COLOR_BUTTONFOCUS },
      { "base:", MU_COLOR_BASE },
      { "basehover:", MU_COLOR_BASEHOVER },
      { "basefocus:", MU_COLOR_BASEFOCUS },
      { "scrollbase:", MU_COLOR_SCROLLBASE },
      { "scrollthumb:", MU_COLOR_SCROLLTHUMB },
      { NULL, 0 }
    };
    if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 290)))
    {
        int i, sz;
        mu_Container* win;
        mu_Rect resize_rect;
        int sw = mu_get_current_container(ctx)->body.w * 0.14;
        int layout[] = { 80, sw, sw, sw, sw, -1 };
        mu_layout_row(ctx, 6, layout, 0);
        for (i = 0; colors[i].label; i++)
        {
            mu_Rect r;
            mu_label(ctx, colors[i].label);
            extern int uint8_slider(mu_Context * ctx, unsigned char* value, int low, int high);
            uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
            uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
            r = mu_layout_next(ctx);
            mu_draw_rect(ctx, r, ctx->style->colors[i]);
        }
        win = mu_get_current_container(ctx);
        sz = ctx->style->title_height;
        resize_rect = mu_rect(win->rect.x + win->rect.w - sz, win->rect.y + win->rect.h - sz, sz, sz);
        if (ctx->mouse_pos.x >= resize_rect.x && ctx->mouse_pos.x <= resize_rect.x + resize_rect.w &&
            ctx->mouse_pos.y >= resize_rect.y && ctx->mouse_pos.y <= resize_rect.y + resize_rect.h)
        {
            SetCursor(LoadCursorW(NULL, IDC_SIZENWSE));
        }
        mu_end_window(ctx);
    }
}

void UpdateProjectionMatrix()
{
    if (!g_context) return;

    // �r���[�|�[�g�ݒ�
    D3D11_VIEWPORT vp = { 0.0f, 0.0f, (FLOAT)width, (FLOAT)height, 0.0f, 1.0f };
    g_context->lpVtbl->RSSetViewports(g_context, 1, &vp);

    // �����_�[�^�[�Q�b�g�ݒ�
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);

    // �v���~�e�B�u�g�|���W�[�ݒ�
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

void InitD3D(HWND hwnd)
{
    DXGI_SWAP_CHAIN_DESC scd = { 0 };
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
    if (FAILED(hr))
    {
        MessageBoxA(hwnd, "DirectX11 device creation failed", "Error", MB_OK);
        return;
    }

    ID3D11Texture2D* backBuffer = NULL;
    hr = g_swapchain->lpVtbl->GetBuffer(g_swapchain, 0, &IID_ID3D11Texture2D, (void**)&backBuffer);
    if (FAILED(hr))
    {
        MessageBoxA(hwnd, "Failed to get back buffer", "Error", MB_OK);
        return;
    }
    hr = g_device->lpVtbl->CreateRenderTargetView(g_device, (ID3D11Resource*)backBuffer, NULL, &g_rtv);
    backBuffer->lpVtbl->Release(backBuffer);
    if (FAILED(hr))
    {
        MessageBoxA(hwnd, "Failed to create render target view", "Error", MB_OK);
        return;
    }
    r_init(g_device, g_context, g_swapchain, g_rtv);
}

void CleanD3D()
{
    r_cleanup();
}

// ���_�o�b�t�@�E�C���f�b�N�X�o�b�t�@�X�V�iDX11�p�j
static int update_vertex_buffer(void)
{
    if (!g_vertex_buffer || vertex_count == 0) return 0;
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) return 0;
    memcpy(resource.pData, vertices, vertex_count * sizeof(struct Vertex));
    g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_vertex_buffer, 0);
    return 1;
}
static int update_index_buffer(void)
{
    if (!g_index_buffer || index_count == 0) return 0;
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_index_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (FAILED(hr)) return 0;
    memcpy(resource.pData, indices, index_count * sizeof(short));
    g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_index_buffer, 0);
    return 1;
}

static void flush(void)
{
    if (vertex_count == 0) return;
    if (!g_atlas_srv) return;
    if (!update_vertex_buffer() || !update_index_buffer()) return;
    UINT stride = sizeof(struct Vertex);
    UINT offset = 0;
    g_context->lpVtbl->IASetVertexBuffers(g_context, 0, 1, &g_vertex_buffer, &stride, &offset);
    g_context->lpVtbl->IASetIndexBuffer(g_context, g_index_buffer, DXGI_FORMAT_R16_UINT, 0);
    g_context->lpVtbl->IASetInputLayout(g_context, g_input_layout);
    g_context->lpVtbl->VSSetShader(g_context, g_vertex_shader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixel_shader, NULL, 0);
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlas_srv);
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_sampler_state);
    g_context->lpVtbl->OMSetBlendState(g_context, g_blend_state, NULL, 0xffffffff);
    g_context->lpVtbl->RSSetState(g_context, g_rasterizer_state);
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    g_context->lpVtbl->DrawIndexed(g_context, index_count, 0, 0);
    vertex_count = 0;
    index_count = 0;
}

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color)
{
    float x, y, w, h;
    if (vertex_count >= MAX_VERTICES - 4 || index_count >= MAX_INDICES - 6) { flush(); }
    
    // �\�[�X���W�i�f�o�b�O�p�j
    static DWORD last_debug_time = 0;
    DWORD current_time = GetTickCount();
    static int debug_counter = 0;
    
#if USE_TTF_FONT
    // TTF�A�g���X�g�p���͓K�؂ȃT�C�Y��UV�v�Z
    if (g_font_atlas.width > 0 && g_font_atlas.height > 0) {
        x = (float)src.x / g_font_atlas.width;
        y = (float)src.y / g_font_atlas.height;
        w = (float)src.w / g_font_atlas.width;
        h = (float)src.h / g_font_atlas.height;
        
        // ����I�ȃf�o�b�O�o��
        if (current_time - last_debug_time > 5000 && debug_counter < 5) { // 5�b��1�� * 5��܂�
            char debug[256];
            sprintf(debug, "TTF push_quad: src=(%d,%d,%d,%d) dst=(%d,%d,%d,%d) uv=(%.4f,%.4f,%.4f,%.4f)\n", 
                    src.x, src.y, src.w, src.h,
                    dst.x, dst.y, dst.w, dst.h,
                    x, y, w, h);
            OutputDebugStringA(debug);
            last_debug_time = current_time;
            debug_counter++;
        }
    } else {
        // �t�H�[���o�b�N
        x = (float)src.x / ATLAS_WIDTH;
        y = (float)src.y / ATLAS_HEIGHT;
        w = (float)src.w / ATLAS_WIDTH;
        h = (float)src.h / ATLAS_HEIGHT;
    }
#else
    x = (float)src.x / ATLAS_WIDTH;
    y = (float)src.y / ATLAS_HEIGHT;
    w = (float)src.w / ATLAS_WIDTH;
    h = (float)src.h / ATLAS_HEIGHT;
#endif
    
    float x0 = 2.0f * dst.x / width - 1.0f;
    float y0 = 1.0f - 2.0f * dst.y / height;
    float x1 = 2.0f * (dst.x + dst.w) / width - 1.0f;
    float y1 = 1.0f - 2.0f * (dst.y + dst.h) / height;
    
    // ���_�f�[�^�𐳂������C�A�E�g�ifloat2 pos, float2 uv, uchar4 color�j�Ŋi�[
    vertices[vertex_count + 0].pos[0] = x0;
    vertices[vertex_count + 0].pos[1] = y0;
    vertices[vertex_count + 0].uv[0] = x;
    vertices[vertex_count + 0].uv[1] = y;
    vertices[vertex_count + 0].color[0] = color.r;
    vertices[vertex_count + 0].color[1] = color.g;
    vertices[vertex_count + 0].color[2] = color.b;
    vertices[vertex_count + 0].color[3] = color.a;

    vertices[vertex_count + 1].pos[0] = x1;
    vertices[vertex_count + 1].pos[1] = y0;
    vertices[vertex_count + 1].uv[0] = x + w;
    vertices[vertex_count + 1].uv[1] = y;
    vertices[vertex_count + 1].color[0] = color.r;
    vertices[vertex_count + 1].color[1] = color.g;
    vertices[vertex_count + 1].color[2] = color.b;
    vertices[vertex_count + 1].color[3] = color.a;

    vertices[vertex_count + 2].pos[0] = x0;
    vertices[vertex_count + 2].pos[1] = y1;
    vertices[vertex_count + 2].uv[0] = x;
    vertices[vertex_count + 2].uv[1] = y + h;
    vertices[vertex_count + 2].color[0] = color.r;
    vertices[vertex_count + 2].color[1] = color.g;
    vertices[vertex_count + 2].color[2] = color.b;
    vertices[vertex_count + 2].color[3] = color.a;

    vertices[vertex_count + 3].pos[0] = x1;
    vertices[vertex_count + 3].pos[1] = y1;
    vertices[vertex_count + 3].uv[0] = x + w;
    vertices[vertex_count + 3].uv[1] = y + h;
    vertices[vertex_count + 3].color[0] = color.r;
    vertices[vertex_count + 3].color[1] = color.g;
    vertices[vertex_count + 3].color[2] = color.b;
    vertices[vertex_count + 3].color[3] = color.a;

    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;
    vertex_count += 4;
    index_count += 6;
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color)
{
#if USE_TTF_FONT
    // g_ttf_atlas��g_font_atlas.pixel���L���ł����TTF�A�C�R�����g�p
    BOOL valid_ttf = (g_ttf_atlas != NULL && g_font_atlas.pixel != NULL);
    
    if (valid_ttf) {
        if (id == ATLAS_WHITE) {
            mu_Rect src = get_ttf_white_rect();
            // ���W���L�����m�F
            if (src.w <= 0 || src.h <= 0) {
                // �����Ȃ�t�H�[���o�b�N
                src = atlas[ATLAS_WHITE];
            }
            int x = rect.x + (rect.w - src.w) / 2;
            int y = rect.y + (rect.h - src.h) / 2;
            push_quad(mu_rect(x, y, src.w, src.h), src, color);
            return;
        }
        if (id >= 0 && id <= ATLAS_WHITE) {
            mu_Rect src = g_ttf_atlas[id];
            // ���W���L�����m�F
            if (src.w <= 0 || src.h <= 0) {
                // �����Ȃ�t�H�[���o�b�N
                src = atlas[id];
            }
            int x = rect.x + (rect.w - src.w) / 2;
            int y = rect.y + (rect.h - src.h) / 2;
            push_quad(mu_rect(x, y, src.w, src.h), src, color);
            return;
        }
    }
#endif

    // �t�H�[���o�b�N�F�ÓI�A�g���X�̏ꍇ�܂���TTF�������ȏꍇ
    if (id >= 0 && id < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
        mu_Rect src = atlas[id];
        int x = rect.x + (rect.w - src.w) / 2;
        int y = rect.y + (rect.h - src.h) / 2;
        push_quad(mu_rect(x, y, src.w, src.h), src, color);
    }
}

void r_draw_rect(mu_Rect rect, mu_Color color)
{
#if USE_TTF_FONT
    // UI��`�̂�TTF white_patch�̈���g��
    // �������Ag_ttf_atlas��NULL�܂���white_patch�������ȏꍇ�͐ÓI�A�g���X���g�p
    if (g_ttf_atlas && 
        g_ttf_atlas[ATLAS_WHITE].w > 0 && g_ttf_atlas[ATLAS_WHITE].h > 0 &&
        g_font_atlas.pixel != NULL) {
        
        push_quad(rect, get_ttf_white_rect(), color);
    } else {
        // �t�H�[���o�b�N�F�ÓI�A�g���X��white_patch���g�p
        push_quad(rect, atlas[ATLAS_WHITE], color);
    }
#else
    push_quad(rect, atlas[ATLAS_WHITE], color);
#endif
}

void r_draw_text(const char* text, mu_Vec2 pos, mu_Color color)
{
    if (!text) return; // �e�L�X�g��NULL�Ȃ牽�����Ȃ�
    
    mu_Rect src;
    const unsigned char* p;
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    
#if USE_TTF_FONT
    // TTF���[�h���L�������Ag_font_atlas.pixel��NULL�̏ꍇ�͐ÓI�A�g���X�Ƀt�H�[���o�b�N
    BOOL use_static_fallback = (g_font_atlas.pixel == NULL || g_font_atlas.width <= 0 || g_font_atlas.height <= 0);
    
    if (!use_static_fallback) {
        // --- UTF-8�Ή� ---
        p = (const unsigned char*)text;
        while (*p) {
            unsigned int codepoint = 0;
            
            // UTF-8�f�R�[�h����
            if ((*p & 0x80) == 0) { 
                // ASCII�����i7�r�b�g�j
                codepoint = *p++; 
            }
            else if ((*p & 0xE0) == 0xC0) { 
                // 2�o�C�g����
                codepoint = (*p++ & 0x1F) << 6; 
                if (*p) codepoint |= (*p++ & 0x3F); 
                else break;
            }
            else if ((*p & 0xF0) == 0xE0) { 
                // 3�o�C�g�����i���{��Ȃǁj
                codepoint = (*p++ & 0x0F) << 12; 
                if (*p) codepoint |= (*p++ & 0x3F) << 6; 
                else break;
                if (*p) codepoint |= (*p++ & 0x3F); 
                else break;
            }
            else if ((*p & 0xF8) == 0xF0) { 
                // 4�o�C�g�����i�G�����Ȃǁj
                codepoint = (*p++ & 0x07) << 18; 
                if (*p) codepoint |= (*p++ & 0x3F) << 12; 
                else break;
                if (*p) codepoint |= (*p++ & 0x3F) << 6; 
                else break;
                if (*p) codepoint |= (*p++ & 0x3F); 
                else break;
            }
            else { 
                // �s����UTF-8�V�[�P���X
                p++; 
                continue; 
            }
            
            // �O���t�̎擾�ƕ`��
            struct mu_font_glyph* glyph = mu_font_find_glyph(codepoint);
            if (glyph && glyph->w > 0 && glyph->h > 0) {
                // �O���t�����������ꍇ
                src.x = glyph->x;
                src.y = glyph->y;
                src.w = glyph->w;
                src.h = glyph->h;
                
                // �O���t�̕`��ʒu����
                mu_Rect glyph_dst = {
                    dst.x + glyph->xoff,
                    dst.y + glyph->yoff,
                    glyph->w,
                    glyph->h
                };
                
                // �O���t�̕`��
                push_quad(glyph_dst, src, color);
                
                // ���̕����ʒu��
                dst.x += glyph->xadvance;
            }
            else if (codepoint >= 32 && codepoint <= 126) {
                // ASCII�̏ꍇ�ŁATTF�t�H���g�Ɍ�����Ȃ��ꍇ�͐ÓI�r�b�g�}�b�v����
                int chr = (int)codepoint;
                int idx = ATLAS_FONT + (chr - 32);
                if (idx >= 0 && idx < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
                    src = atlas[idx];
                    dst.w = src.w;
                    dst.h = src.h;
                    push_quad(dst, src, color);
                    dst.x += src.w;
                }
            }
            else {
                // �t�H�[���o�b�N�F'?'��\��
                struct mu_font_glyph* fallback = mu_font_find_glyph('?');
                if (fallback && fallback->w > 0 && fallback->h > 0) {
                    src.x = fallback->x;
                    src.y = fallback->y;
                    src.w = fallback->w;
                    src.h = fallback->h;
                    
                    mu_Rect fallback_dst = {
                        dst.x + fallback->xoff,
                        dst.y + fallback->yoff,
                        fallback->w,
                        fallback->h
                    };
                    
                    push_quad(fallback_dst, src, color);
                    dst.x += fallback->xadvance;
                }
                else {
                    // �Ō�̎�i�Ƃ��āA�ÓI�r�b�g�}�b�v��'?'���g�p
                    src = atlas[ATLAS_FONT + ('?' - 32)];
                    dst.w = src.w;
                    dst.h = src.h;
                    push_quad(dst, src, color);
                    dst.x += src.w;
                }
            }
        }
    } else {
        // �t�H�[���o�b�N�F�ÓI�r�b�g�}�b�v�ł̕`��
        int chr;
        for (p = (const unsigned char*)text; *p; p++) {
            if ((*p & 0xc0) == 0x80) continue;
            chr = mu_min(*p, 127);
            int idx = ATLAS_FONT + (chr - 32);
            if (idx >= 0 && idx < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
                src = atlas[idx];
                dst.w = src.w;
                dst.h = src.h;
                push_quad(dst, src, color);
                dst.x += dst.w;
            }
        }
    }
#else
    // �]���̏����i�ÓI�r�b�g�}�b�v�t�H���g�j
    int chr;
    for (p = (const unsigned char*)text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        chr = mu_min(*p, 127);
        int idx = ATLAS_FONT + (chr - 32);
        if (idx >= 0 && idx < (int)(sizeof(atlas) / sizeof(atlas[0]))) {
            src = atlas[idx];
            dst.w = src.w;
            dst.h = src.h;
            push_quad(dst, src, color);
            dst.x += dst.w;
        }
    }
#endif
}

extern float bg[4];

void r_draw(void)
{
    if (!g_ctx || !g_context || !g_rtv) {
        OutputDebugStringA("r_draw: context or rtv is NULL\n");
        return;
    }

    // �t���[���J�n�����idx11_begin_frame�����j
    // �����_�[�^�[�Q�b�g�̐ݒ�
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_rtv, NULL);

    // �C���v�b�g���C�A�E�g�̐ݒ�
    g_context->lpVtbl->IASetInputLayout(g_context, g_input_layout);

    // �V�F�[�_�[�̐ݒ�
    g_context->lpVtbl->VSSetShader(g_context, g_vertex_shader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixel_shader, NULL, 0);

    // �T���v���[�ƃe�N�X�`���̐ݒ�
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_sampler_state);
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlas_srv);

    // �u�����h�X�e�[�g�̐ݒ�
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_context->lpVtbl->OMSetBlendState(g_context, g_blend_state, blendFactor, 0xffffffff);

    // ���X�^���C�U�X�e�[�g�̐ݒ�
    g_context->lpVtbl->RSSetState(g_context, g_rasterizer_state);

    // �v���~�e�B�u�g�|���W�[�̐ݒ�
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // �r���[�|�[�g�ݒ�
    UpdateProjectionMatrix();

    // microui�t���[������
    process_frame(g_ctx); // UI����

    // �o�b�N�o�b�t�@�̃N���A
    r_clear(mu_color((int)bg[0], (int)bg[1], (int)bg[2], 255));

    // ���_�E�C���f�b�N�X�J�E���g�����Z�b�g
    vertex_count = 0;
    index_count = 0;

    // �R�}���h�J�E���^�i�f�o�b�O�p�j
    int cmd_count = 0;
    int rect_count = 0;
    int text_count = 0;
    int icon_count = 0;

    // microui�R�}���h����
    mu_Command* cmd = NULL;
    while (mu_next_command(g_ctx, &cmd))
    {
        cmd_count++;
        switch (cmd->type)
        {
        case MU_COMMAND_CLIP:
            r_set_clip_rect(cmd->clip.rect);
            break;
        case MU_COMMAND_RECT:
            r_draw_rect(cmd->rect.rect, cmd->rect.color);
            rect_count++;
            break;
        case MU_COMMAND_TEXT:
            r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
            text_count++;
            break;
        case MU_COMMAND_ICON:
            r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
            icon_count++;
            break;
        }
    }

    // �c���Ă��钸�_���t���b�V��
    flush();

    // �f�o�b�O�o�́i�p�ɂɂȂ肷���Ȃ��悤�ÓI�ϐ��Ő����j
    static DWORD last_debug_time = 0;
    DWORD current_time = GetTickCount();
    if (current_time - last_debug_time > 1000) { // 1�b��1�񂾂��f�o�b�O�o��
        char debug_info[256];
        sprintf(debug_info, "Draw commands: %d (rect:%d, text:%d, icon:%d)\n", 
            cmd_count, rect_count, text_count, icon_count);
        OutputDebugStringA(debug_info);
        last_debug_time = current_time;
    }

    // ��ʂɕ\��
    r_present();
}

// DirectX11 IID_ID3D11Texture2D ��`�i�����������N�G���[�΍�j
#ifndef INITGUID
#define INITGUID
#endif
#include <initguid.h>
const GUID IID_ID3D11Texture2D = { 0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c} };
