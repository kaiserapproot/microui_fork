/**
 * dx11_renderer.c
 * DirectX11��microui�������_�����O���邽�߂̃����_���[
 * renderer.c�iDirectX9�Łj���Q�l��DirectX11�p�Ɏ���
 */
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdbool.h>
#include <stdio.h>  // sprintf�̂��߂̃C���N���[�h
#include "dx11_renderer.h"
#include "microui.h"

// DirectX11�֘A��GUID�Q��
// dx11_main.obj�Ŋ��ɒ�`����Ă��邽�߁Aextern�Ő錾
extern const GUID IID_ID3D11Texture2D;

// �A�g���X�e�N�X�`����`
#define ATLAS_WIDTH 128
#define ATLAS_HEIGHT 128
#define ATLAS_WHITE MU_ICON_MAX
#define ATLAS_FONT (MU_ICON_MAX + 1)

// �A�g���X�e�N�X�`���iatlas_texture�z��̓��e��atlas.inl����擾�j
// ����: �����ł͋�̃f�[�^���g�p���A��œ���̗̈�𔒐F�ɐݒ肵�܂�
static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = { 0 };

// �A�g���X�̗̈��`
static mu_Rect atlas[] = {
    {0,0,0,0},
    { 88, 68, 16, 16 },  // MU_ICON_CLOSE
    { 0, 0, 18, 18 },    // MU_ICON_CHECK
    { 113, 68, 5, 7 },   // MU_ICON_COLLAPSED
    { 118, 68, 7, 5 },   // MU_ICON_EXPANDED
    { 125, 68, 3, 3 },   // ATLAS_WHITE (MU_ICON_MAX)
    // ATLAS_FONT (MU_ICON_MAX + 1)����n�܂�t�H���g�f�[�^
    { 84, 68, 2, 17 },   // ATLAS_FONT+32 (' ')
    { 39, 68, 3, 17 },   // ATLAS_FONT+33 ('!')
    { 114, 51, 5, 17 },  // ATLAS_FONT+34 ('"')
    { 34, 17, 7, 17 },   // ATLAS_FONT+35 ('#')
    { 28, 34, 6, 17 },   // ATLAS_FONT+36 ('$')
    { 58, 0, 9, 17 },    // ATLAS_FONT+37 ('%')
    { 103, 0, 8, 17 },   // ATLAS_FONT+38 ('&')
    { 86, 68, 2, 17 },   // ATLAS_FONT+39 ('\'')
    { 42, 68, 3, 17 },   // ATLAS_FONT+40 ('(')
    { 45, 68, 3, 17 },   // ATLAS_FONT+41 (')')
    { 34, 34, 6, 17 },   // ATLAS_FONT+42 ('*')
    { 40, 34, 6, 17 },   // ATLAS_FONT+43 ('+')
    { 48, 68, 3, 17 },   // ATLAS_FONT+44 (',')
    { 51, 68, 3, 17 },   // ATLAS_FONT+45 ('-')
    { 54, 68, 3, 17 },   // ATLAS_FONT+46 ('.')
    { 124, 34, 4, 17 },  // ATLAS_FONT+47 ('/')
    { 46, 34, 6, 17 },   // ATLAS_FONT+48 ('0')
    { 52, 34, 6, 17 },   // ATLAS_FONT+49 ('1')
    { 58, 34, 6, 17 },   // ATLAS_FONT+50 ('2')
    { 64, 34, 6, 17 },   // ATLAS_FONT+51 ('3')
    { 70, 34, 6, 17 },   // ATLAS_FONT+52 ('4')
    { 76, 34, 6, 17 },   // ATLAS_FONT+53 ('5')
    { 82, 34, 6, 17 },   // ATLAS_FONT+54 ('6')
    { 88, 34, 6, 17 },   // ATLAS_FONT+55 ('7')
    { 94, 34, 6, 17 },   // ATLAS_FONT+56 ('8')
    { 100, 34, 6, 17 },  // ATLAS_FONT+57 ('9')
    { 57, 68, 3, 17 },   // ATLAS_FONT+58 (':')
    { 60, 68, 3, 17 },   // ATLAS_FONT+59 (';')
    { 106, 34, 6, 17 },  // ATLAS_FONT+60 ('<')
    { 112, 34, 6, 17 },  // ATLAS_FONT+61 ('=')
    { 118, 34, 6, 17 },  // ATLAS_FONT+62 ('>')
    { 119, 51, 5, 17 },  // ATLAS_FONT+63 ('?')
    { 18, 0, 10, 17 },   // ATLAS_FONT+64 ('@')
    { 41, 17, 7, 17 },   // ATLAS_FONT+65 ('A')
    { 48, 17, 7, 17 },   // ATLAS_FONT+66 ('B')
    { 55, 17, 7, 17 },   // ATLAS_FONT+67 ('C')
    { 111, 0, 8, 17 },   // ATLAS_FONT+68 ('D')
    { 0, 35, 6, 17 },    // ATLAS_FONT+69 ('E')
    { 6, 35, 6, 17 },    // ATLAS_FONT+70 ('F')
    { 119, 0, 8, 17 },   // ATLAS_FONT+71 ('G')
    { 18, 17, 8, 17 },   // ATLAS_FONT+72 ('H')
    { 63, 68, 3, 17 },   // ATLAS_FONT+73 ('I')
    { 66, 68, 3, 17 },   // ATLAS_FONT+74 ('J')
    { 62, 17, 7, 17 },   // ATLAS_FONT+75 ('K')
    { 12, 51, 6, 17 },   // ATLAS_FONT+76 ('L')
    { 28, 0, 10, 17 },   // ATLAS_FONT+77 ('M')
    { 67, 0, 9, 17 },    // ATLAS_FONT+78 ('N')
    { 76, 0, 9, 17 },    // ATLAS_FONT+79 ('O')
    { 69, 17, 7, 17 },   // ATLAS_FONT+80 ('P')
    { 85, 0, 9, 17 },    // ATLAS_FONT+81 ('Q')
    { 76, 17, 7, 17 },   // ATLAS_FONT+82 ('R')
    { 18, 51, 6, 17 },   // ATLAS_FONT+83 ('S')
    { 24, 51, 6, 17 },   // ATLAS_FONT+84 ('T')
    { 26, 17, 8, 17 },   // ATLAS_FONT+85 ('U')
    { 83, 17, 7, 17 },   // ATLAS_FONT+86 ('V')
    { 38, 0, 10, 17 },   // ATLAS_FONT+87 ('W')
    { 90, 17, 7, 17 },   // ATLAS_FONT+88 ('X')
    { 30, 51, 6, 17 },   // ATLAS_FONT+89 ('Y')
    { 36, 51, 6, 17 },   // ATLAS_FONT+90 ('Z')
    { 69, 68, 3, 17 },   // ATLAS_FONT+91 ('[')
    { 124, 51, 4, 17 },  // ATLAS_FONT+92 ('\\')
    { 72, 68, 3, 17 },   // ATLAS_FONT+93 (']')
    { 42, 51, 6, 17 },   // ATLAS_FONT+94 ('^')
    { 15, 68, 4, 17 },   // ATLAS_FONT+95 ('_')
    { 48, 51, 6, 17 },   // ATLAS_FONT+96 ('`')
    { 54, 51, 6, 17 },   // ATLAS_FONT+97 ('a')
    { 97, 17, 7, 17 },   // ATLAS_FONT+98 ('b')
    { 0, 52, 5, 17 },    // ATLAS_FONT+99 ('c')
    { 104, 17, 7, 17 },  // ATLAS_FONT+100 ('d')
    { 60, 51, 6, 17 },   // ATLAS_FONT+101 ('e')
    { 19, 68, 4, 17 },   // ATLAS_FONT+102 ('f')
    { 66, 51, 6, 17 },   // ATLAS_FONT+103 ('g')
    { 111, 17, 7, 17 },  // ATLAS_FONT+104 ('h')
    { 75, 68, 3, 17 },   // ATLAS_FONT+105 ('i')
    { 78, 68, 3, 17 },   // ATLAS_FONT+106 ('j')
    { 72, 51, 6, 17 },   // ATLAS_FONT+107 ('k')
    { 81, 68, 3, 17 },   // ATLAS_FONT+108 ('l')
    { 48, 0, 10, 17 },   // ATLAS_FONT+109 ('m')
    { 118, 17, 7, 17 },  // ATLAS_FONT+110 ('n')
    { 0, 18, 7, 17 },    // ATLAS_FONT+111 ('o')
    { 7, 18, 7, 17 },    // ATLAS_FONT+112 ('p')
    { 14, 34, 7, 17 },   // ATLAS_FONT+113 ('q')
    { 23, 68, 4, 17 },   // ATLAS_FONT+114 ('r')
    { 5, 52, 5, 17 },    // ATLAS_FONT+115 ('s')
    { 27, 68, 4, 17 },   // ATLAS_FONT+116 ('t')
    { 21, 34, 7, 17 },   // ATLAS_FONT+117 ('u')
    { 78, 51, 6, 17 },   // ATLAS_FONT+118 ('v')
    { 94, 0, 9, 17 },    // ATLAS_FONT+119 ('w')
    { 84, 51, 6, 17 },   // ATLAS_FONT+120 ('x')
    { 90, 51, 6, 17 },   // ATLAS_FONT+121 ('y')
    { 10, 68, 5, 17 },   // ATLAS_FONT+122 ('z')
    { 31, 68, 4, 17 },   // ATLAS_FONT+123 ('{')
    { 96, 51, 6, 17 },   // ATLAS_FONT+124 ('|')
    { 35, 68, 4, 17 },   // ATLAS_FONT+125 ('}')
    { 102, 51, 6, 17 },  // ATLAS_FONT+126 ('~')
    { 108, 51, 6, 17 }   // ATLAS_FONT+127
};

// �O���[�o���ϐ��iDirectX11���\�[�X�j
ID3D11Device* g_device = NULL;
ID3D11DeviceContext* g_context = NULL;
ID3D11RenderTargetView* g_renderTargetView = NULL;
ID3D11Texture2D* g_atlasTexture = NULL;
ID3D11ShaderResourceView* g_atlasSRV = NULL;
ID3D11Buffer* g_vertexBuffer = NULL;
ID3D11Buffer* g_indexBuffer = NULL;
ID3D11VertexShader* g_vertexShader = NULL;
ID3D11PixelShader* g_pixelShader = NULL;
ID3D11InputLayout* g_inputLayout = NULL;
ID3D11BlendState* g_blendState = NULL;
ID3D11SamplerState* g_samplerState = NULL;
ID3D11RasterizerState* g_rasterizerState = NULL; // �V���ɒǉ�
IDXGISwapChain* g_swapChain = NULL;

// ���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@�̊Ǘ�
static DX11Vertex vertices[MAX_VERTICES];
static WORD indices[MAX_VERTICES * 3 / 2];
static int vertex_count = 0;
static int index_count = 0;

// �R�}���h���X�g
static SimpleCommand commands[1024];
static int command_count = 0;

// �E�B���h�E�T�C�Y
static int window_width = 0;
static int window_height = 0;

// �V�F�[�_�[�R�[�h�iHLSL�j
const char* vertex_shader_code = 
"struct VSInput {\n"
"    float2 pos : POSITION;\n"
"    float2 uv : TEXCOORD;\n"
"    float4 col : COLOR;\n"
"};\n"
"\n"
"struct PSInput {\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD;\n"
"    float4 col : COLOR;\n"
"};\n"
"\n"
"PSInput main(VSInput input) {\n"  // �G���g���[�|�C���g��main�ɕύX
"    PSInput output;\n"
"    output.pos = float4(input.pos.x, input.pos.y, 0.0f, 1.0f);\n"
"    output.uv = input.uv;\n"
"    output.col = input.col;\n"
"    return output;\n"
"}\n";

const char* pixel_shader_code =
"Texture2D tex : register(t0);\n"
"SamplerState samp : register(s0);\n"
"\n"
"struct PSInput {\n"
"    float4 pos : SV_POSITION;\n"
"    float2 uv : TEXCOORD;\n"
"    float4 col : COLOR;\n"
"};\n"
"\n"
"float4 main(PSInput input) : SV_TARGET {\n"  // �G���g���[�|�C���g��main�ɕύX
"    float4 texColor = tex.Sample(samp, input.uv);\n"
"    return texColor * input.col;\n"
"}\n";

// �V�F�[�_�[�R���p�C���֐�
HRESULT CompileShaderFromString(const char* shaderCode, const char* entryPoint, 
                               const char* shaderModel, ID3DBlob** ppBlob) {
    HRESULT hr = S_OK;
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined(DEBUG) || defined(_DEBUG)
    dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif

    ID3DBlob* pErrorBlob = NULL;
    hr = D3DCompile(shaderCode, strlen(shaderCode), NULL, NULL, NULL, 
                    entryPoint, shaderModel, dwShaderFlags, 0, ppBlob, &pErrorBlob);
    if (FAILED(hr)) {
        if (pErrorBlob != NULL) {
            OutputDebugStringA((char*)ID3D10Blob_GetBufferPointer(pErrorBlob));
            ID3D10Blob_Release(pErrorBlob);
        }
        return hr;
    }
    if (pErrorBlob) ID3D10Blob_Release(pErrorBlob);

    return S_OK;
}

// �A�N�Z�T�֐�
ID3D11Device* dx11_get_device(void) {
    return g_device;
}

ID3D11DeviceContext* dx11_get_context(void) {
    return g_context;
}

SimpleCommand* dx11_get_commands(void) {
    return commands;
}

int dx11_get_command_count(void) {
    return command_count;
}

// �G���[�R�[�h���f�o�b�O���O�ɏo�͂���w���p�[�֐�
void dx11_log_error(HRESULT hr, const char* operation) {
    char errorMsg[256];
    sprintf(errorMsg, "DirectX11 Error: %s failed with HRESULT: 0x%08X", operation, (unsigned int)hr);
    OutputDebugStringA(errorMsg);
    
    // �G���[�R�[�h�̏ڍא���
    switch(hr) {
        case E_INVALIDARG:
            OutputDebugStringA(" (E_INVALIDARG: Invalid parameter)");
            break;
        case E_OUTOFMEMORY:
            OutputDebugStringA(" (E_OUTOFMEMORY: Out of memory)");
            break;
        case E_NOTIMPL:
            OutputDebugStringA(" (E_NOTIMPL: Not implemented)");
            break;
        case E_FAIL:
            OutputDebugStringA(" (E_FAIL: Unknown failure)");
            break;
    }
    OutputDebugStringA("\n");
}

// DirectX 11������
HRESULT dx11_init(HWND hWnd, int width, int height) {
    HRESULT hr = S_OK;
    window_width = width;
    window_height = height;

    // �X���b�v�`�F�[���̐ݒ�
    DXGI_SWAP_CHAIN_DESC swapChainDesc = { 0 };
    swapChainDesc.BufferCount = 1;
    swapChainDesc.BufferDesc.Width = width;
    swapChainDesc.BufferDesc.Height = height;
    swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
    swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.OutputWindow = hWnd;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Windowed = TRUE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    // �f�o�C�X�A�f�o�C�X�R���e�L�X�g�A�X���b�v�`�F�[���̍쐬
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
                                      D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain, &g_device, NULL, &g_context);
    if (FAILED(hr)) {
        dx11_log_error(hr, "D3D11CreateDeviceAndSwapChain");
        return hr;
    }

    // �����_�[�^�[�Q�b�g�r���[�̍쐬
    ID3D11Texture2D* backBuffer = NULL;
    hr = g_swapChain->lpVtbl->GetBuffer(g_swapChain, 0, &IID_ID3D11Texture2D, (void**)&backBuffer);
    if (FAILED(hr)) {
        dx11_log_error(hr, "GetBuffer");
        return hr;
    }

    hr = g_device->lpVtbl->CreateRenderTargetView(g_device, (ID3D11Resource*)backBuffer, NULL, &g_renderTargetView);
    backBuffer->lpVtbl->Release(backBuffer);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateRenderTargetView");
        return hr;
    }

    // ���X�^���C�U�X�e�[�g�̍쐬�i�V�U�[��`��L���ɂ���j
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = TRUE; // �V�U�[��`��L����
    
    hr = g_device->lpVtbl->CreateRasterizerState(g_device, &rasterizerDesc, &g_rasterizerState);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateRasterizerState");
        return hr;
    }
    
    // ���X�^���C�U�X�e�[�g��ݒ�
    g_context->lpVtbl->RSSetState(g_context, g_rasterizerState);

    // �V�F�[�_�[���R���p�C��
    ID3DBlob* vsBlob = NULL;
    ID3DBlob* psBlob = NULL;
    
    // ���_�V�F�[�_�[���R���p�C��
    hr = CompileShaderFromString(vertex_shader_code, "main", "vs_4_0", &vsBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CompileShaderFromString (VS)");
        return hr;
    }
    
    // ���_�V�F�[�_�[�̍쐬
    hr = g_device->lpVtbl->CreateVertexShader(g_device, 
                                           ID3D10Blob_GetBufferPointer(vsBlob),
                                           ID3D10Blob_GetBufferSize(vsBlob), 
                                           NULL, &g_vertexShader);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateVertexShader");
        ID3D10Blob_Release(vsBlob);
        return hr;
    }

    // ���̓��C�A�E�g�̐ݒ�
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    // ���̓��C�A�E�g�̍쐬
    hr = g_device->lpVtbl->CreateInputLayout(g_device, layout, numElements, 
                                          ID3D10Blob_GetBufferPointer(vsBlob), 
                                          ID3D10Blob_GetBufferSize(vsBlob),
                                          &g_inputLayout);
    ID3D10Blob_Release(vsBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateInputLayout");
        return hr;
    }

    // �s�N�Z���V�F�[�_�[���R���p�C��
    hr = CompileShaderFromString(pixel_shader_code, "main", "ps_4_0", &psBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CompileShaderFromString (PS)");
        return hr;
    }
    
    // �s�N�Z���V�F�[�_�[�̍쐬
    hr = g_device->lpVtbl->CreatePixelShader(g_device, 
                                          ID3D10Blob_GetBufferPointer(psBlob),
                                          ID3D10Blob_GetBufferSize(psBlob), 
                                          NULL, &g_pixelShader);
    ID3D10Blob_Release(psBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreatePixelShader");
        return hr;
    }

    // ���_�o�b�t�@�̍쐬
    D3D11_BUFFER_DESC vertexBufferDesc = { 0 };
    vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    vertexBufferDesc.ByteWidth = sizeof(DX11Vertex) * MAX_VERTICES;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_device->lpVtbl->CreateBuffer(g_device, &vertexBufferDesc, NULL, &g_vertexBuffer);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateBuffer (Vertex Buffer)");
        return hr;
    }

    // �C���f�b�N�X�o�b�t�@�̍쐬
    D3D11_BUFFER_DESC indexBufferDesc = { 0 };
    indexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
    indexBufferDesc.ByteWidth = sizeof(WORD) * MAX_VERTICES * 3 / 2;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
    indexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_device->lpVtbl->CreateBuffer(g_device, &indexBufferDesc, NULL, &g_indexBuffer);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateBuffer (Index Buffer)");
        return hr;
    }

    // atlas.inl����e�N�X�`�����쐬
    D3D11_TEXTURE2D_DESC textureDesc = { 0 };
    textureDesc.Width = ATLAS_WIDTH;
    textureDesc.Height = ATLAS_HEIGHT;
    textureDesc.MipLevels = 1;
    textureDesc.ArraySize = 1;
    textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    textureDesc.SampleDesc.Count = 1;
    textureDesc.SampleDesc.Quality = 0;
    textureDesc.Usage = D3D11_USAGE_DEFAULT;
    textureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // �������m�ۂ̃f�o�b�O�o��
    OutputDebugStringA("Allocating texture memory\n");
    
    // ??�d�v: ���F�p�b�`�̈�𔒐F�ɐݒ�iATLAS_WHITE�̈ʒu�j
    mu_Rect white = atlas[ATLAS_WHITE]; // { 125, 68, 3, 3 }
    for (int y = 0; y < white.h; y++) {
        for (int x = 0; x < white.w; x++) {
            int idx = (white.y + y) * ATLAS_WIDTH + (white.x + x);
            atlas_texture[idx] = 255; // ���S�s����
        }
    }

    // �f�o�b�O: ���F�p�b�`�̏����o��
    char whitePatchInfo[256];
    sprintf(whitePatchInfo, "White patch at: %d,%d,%d,%d set to opaque\n", 
            white.x, white.y, white.w, white.h);
    OutputDebugStringA(whitePatchInfo);
    
    // �e�X�g�p�F�N���[�Y�A�C�R���̈���ݒ�
    mu_Rect closeIcon = atlas[MU_ICON_CLOSE];  // { 88, 68, 16, 16 }
    for (int y = 0; y < closeIcon.h; y++) {
        for (int x = 0; x < closeIcon.w; x++) {
            int idx = (closeIcon.y + y) * ATLAS_WIDTH + (closeIcon.x + x);
            // �N���[�Y�A�C�R���iX�`�j���쐬
            int centerX = closeIcon.w / 2;
            int centerY = closeIcon.h / 2;
            int dx = x - centerX;
            int dy = y - centerY;
            if (abs(dx) == abs(dy) || abs(dx) == abs(dy) - 1 || abs(dx) - 1 == abs(dy)) {
                atlas_texture[idx] = 255; // X�̌`�𔒂ŕ`��
            }
        }
    }

    // ??�d�v: ���S�ɔ����e�N�X�`�����쐬�i�S�s�N�Z����RGBA=(255,255,255,255)�Ɂj
    unsigned char* rgbaData = (unsigned char*)malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    if (rgbaData == NULL) {
        OutputDebugStringA("Failed to allocate memory for texture data\n");
        return E_OUTOFMEMORY;
    }
    
    // RGBA�`���ɕϊ��iR=G=B=255, A=atlas_texture�l�j
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        rgbaData[i * 4 + 0] = 255;  // R
        rgbaData[i * 4 + 1] = 255;  // G
        rgbaData[i * 4 + 2] = 255;  // B
        rgbaData[i * 4 + 3] = atlas_texture[i];  // A
    }

    D3D11_SUBRESOURCE_DATA textureData = { 0 };
    textureData.pSysMem = rgbaData;
    textureData.SysMemPitch = ATLAS_WIDTH * 4;
    textureData.SysMemSlicePitch = 0;

    // �e�N�X�`���쐬���f�o�b�O
    char debugMsg[256];
    sprintf(debugMsg, "Creating texture: %dx%d, Format=%d, MipLevels=%d\n", 
            textureDesc.Width, textureDesc.Height, textureDesc.Format, textureDesc.MipLevels);
    OutputDebugStringA(debugMsg);
    
    hr = g_device->lpVtbl->CreateTexture2D(g_device, &textureDesc, &textureData, &g_atlasTexture);
    free(rgbaData); // �f�[�^�̃R�s�[��ɉ��

    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateTexture2D");
        return hr;
    }

    // �e�N�X�`������V�F�[�_�[���\�[�X�r���[���쐬
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { 0 };
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_atlasTexture, &srvDesc, &g_atlasSRV);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateShaderResourceView");
        return hr;
    }

    // �T���v���[�X�e�[�g�̍쐬
    D3D11_SAMPLER_DESC samplerDesc = { 0 };
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // �ŋߖT�T���v�����O�ŃN���A�ȃt�H���g��
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = g_device->lpVtbl->CreateSamplerState(g_device, &samplerDesc, &g_samplerState);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateSamplerState");
        return hr;
    }

    // �u�����h�X�e�[�g�̍쐬
    D3D11_BLEND_DESC blendDesc = { 0 };
    blendDesc.AlphaToCoverageEnable = FALSE;
    blendDesc.IndependentBlendEnable = FALSE;
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = g_device->lpVtbl->CreateBlendState(g_device, &blendDesc, &g_blendState);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateBlendState");
        return hr;
    }

    // �r���[�|�[�g�̐ݒ�
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)width;
    vp.Height = (FLOAT)height;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    g_context->lpVtbl->RSSetViewports(g_context, 1, &vp);

    OutputDebugStringA("DirectX11 initialization completed successfully\n");
    return S_OK;
}

// ���\�[�X�̃N���[���A�b�v
void dx11_cleanup(void) {
    if (g_blendState) g_blendState->lpVtbl->Release(g_blendState);
    if (g_samplerState) g_samplerState->lpVtbl->Release(g_samplerState);
    if (g_atlasSRV) g_atlasSRV->lpVtbl->Release(g_atlasSRV);
    if (g_atlasTexture) g_atlasTexture->lpVtbl->Release(g_atlasTexture);
    if (g_indexBuffer) g_indexBuffer->lpVtbl->Release(g_indexBuffer);
    if (g_vertexBuffer) g_vertexBuffer->lpVtbl->Release(g_vertexBuffer);
    if (g_inputLayout) g_inputLayout->lpVtbl->Release(g_inputLayout);
    if (g_pixelShader) g_pixelShader->lpVtbl->Release(g_pixelShader);
    if (g_vertexShader) g_vertexShader->lpVtbl->Release(g_vertexShader);
    if (g_rasterizerState) g_rasterizerState->lpVtbl->Release(g_rasterizerState); // �ǉ�
    if (g_renderTargetView) g_renderTargetView->lpVtbl->Release(g_renderTargetView);
    if (g_context) g_context->lpVtbl->Release(g_context);
    if (g_swapChain) g_swapChain->lpVtbl->Release(g_swapChain);
    if (g_device) g_device->lpVtbl->Release(g_device);
}

// �X�N���[���N���A
void dx11_clear_screen(float r, float g, float b, float a) {
    float color[4] = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    g_context->lpVtbl->ClearRenderTargetView(g_context, g_renderTargetView, color);
}

// �����_�����O�J�n
void dx11_begin_frame(void) {
    // �����_�[�^�[�Q�b�g�̐ݒ�
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_renderTargetView, NULL);

    // �C���v�b�g���C�A�E�g�̐ݒ�
    g_context->lpVtbl->IASetInputLayout(g_context, g_inputLayout);

    // �V�F�[�_�[�̐ݒ�
    g_context->lpVtbl->VSSetShader(g_context, g_vertexShader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixelShader, NULL, 0);

    // �T���v���[�ƃe�N�X�`���̐ݒ�
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_samplerState);
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlasSRV);

    // �u�����h�X�e�[�g�̐ݒ�
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_context->lpVtbl->OMSetBlendState(g_context, g_blendState, blendFactor, 0xffffffff);

    // ���X�^���C�U�X�e�[�g���Đݒ�i�O�̂��߁j
    g_context->lpVtbl->RSSetState(g_context, g_rasterizerState);

    // �v���~�e�B�u�g�|���W�[�̐ݒ�
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // ���_�J�E���g�ƃC���f�b�N�X�J�E���g�����Z�b�g
    vertex_count = 0;
    index_count = 0;
}

// �����_�����O�I��
void dx11_end_frame(void) {
    // �o�b�t�@�̃t���b�V��
    dx11_flush_vertices();
}

// ��ʂɕ\��
void dx11_present(void) {
    // �X���b�v�`�F�[���̃v���[���g
    if (g_swapChain) {
        HRESULT hr = g_swapChain->lpVtbl->Present(g_swapChain, 1, 0);
        if (FAILED(hr)) {
            dx11_log_error(hr, "Present");
        }
    }
}

// ���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@���X�V���ă����_�����O
void dx11_flush_vertices(void) {
    if (vertex_count == 0) {
        return;
    }

    // ���_�o�b�t�@�̍X�V
    D3D11_MAPPED_SUBRESOURCE resource;
    HRESULT hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_vertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (SUCCEEDED(hr)) {
        memcpy(resource.pData, vertices, vertex_count * sizeof(DX11Vertex));
        g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_vertexBuffer, 0);
    }
    else {
        dx11_log_error(hr, "Map (Vertex Buffer)");
        return;
    }

    // �C���f�b�N�X�o�b�t�@�̍X�V
    hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (SUCCEEDED(hr)) {
        memcpy(resource.pData, indices, index_count * sizeof(WORD));
        g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_indexBuffer, 0);
    }
    else {
        dx11_log_error(hr, "Map (Index Buffer)");
        return;
    }

    // ���_�o�b�t�@�ƃC���f�b�N�X�o�b�t�@��ݒ�
    UINT stride = sizeof(DX11Vertex);
    UINT offset = 0;
    g_context->lpVtbl->IASetVertexBuffers(g_context, 0, 1, &g_vertexBuffer, &stride, &offset);
    g_context->lpVtbl->IASetIndexBuffer(g_context, g_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // �`��
    g_context->lpVtbl->DrawIndexed(g_context, index_count, 0, 0);

    // �J�E���g�����Z�b�g
    vertex_count = 0;
    index_count = 0;
}

// �l�p�`�𒸓_�o�b�t�@�ɒǉ�����iUV���W�w��Ȃ� - ��{�I�Ɏg�p���Ȃ��j
void dx11_push_quad(mu_Rect dst, mu_Color color) {
    // �A�g���X�̔��F�������g�p���ĒP�F�l�p�`��`��
    // �f�o�b�O����ǉ�
    char debugMsg[256];
    sprintf(debugMsg, "dx11_push_quad: rect=%d,%d,%d,%d color=%d,%d,%d,%d\n", 
            dst.x, dst.y, dst.w, dst.h, color.r, color.g, color.b, color.a);
    OutputDebugStringA(debugMsg);
    
    dx11_push_quad_atlas(dst, atlas[ATLAS_WHITE], color);
}

// �l�p�`�𒸓_�o�b�t�@�ɒǉ�����i�A�g���X��UV���W���g�p�j
void dx11_push_quad_atlas(mu_Rect dst, mu_Rect src, mu_Color color) {
    if (vertex_count + 4 > MAX_VERTICES || index_count + 6 > MAX_VERTICES * 3 / 2) {
        dx11_flush_vertices();
    }

    // UV���W�̌v�Z�i�A�g���X�̃T�C�Y�Ő��K���j
    float u0 = (float)src.x / ATLAS_WIDTH;
    float v0 = (float)src.y / ATLAS_HEIGHT;
    float u1 = (float)(src.x + src.w) / ATLAS_WIDTH;
    float v1 = (float)(src.y + src.h) / ATLAS_HEIGHT;

    // �X�N���[�����W�𐳋K���i-1.0�`1.0�͈̔͂ɕϊ��j
    float x0 = 2.0f * dst.x / window_width - 1.0f;
    float y0 = 1.0f - 2.0f * dst.y / window_height;
    float x1 = 2.0f * (dst.x + dst.w) / window_width - 1.0f;
    float y1 = 1.0f - 2.0f * (dst.y + dst.h) / window_height;

    // ���_�̐ݒ�
    DX11Vertex* v = &vertices[vertex_count];

    // ����
    v[0].pos[0] = x0;
    v[0].pos[1] = y0;
    v[0].uv[0] = u0;
    v[0].uv[1] = v0;
    v[0].color[0] = color.r;
    v[0].color[1] = color.g;
    v[0].color[2] = color.b;
    v[0].color[3] = color.a;

    // �E��
    v[1].pos[0] = x1;
    v[1].pos[1] = y0;
    v[1].uv[0] = u1;
    v[1].uv[1] = v0;
    v[1].color[0] = color.r;
    v[1].color[1] = color.g;
    v[1].color[2] = color.b;
    v[1].color[3] = color.a;

    // ����
    v[2].pos[0] = x0;
    v[2].pos[1] = y1;
    v[2].uv[0] = u0;
    v[2].uv[1] = v1;
    v[2].color[0] = color.r;
    v[2].color[1] = color.g;
    v[2].color[2] = color.b;
    v[2].color[3] = color.a;

    // �E��
    v[3].pos[0] = x1;
    v[3].pos[1] = y1;
    v[3].uv[0] = u1;
    v[3].uv[1] = v1;
    v[3].color[0] = color.r;
    v[3].color[1] = color.g;
    v[3].color[2] = color.b;
    v[3].color[3] = color.a;

    // �C���f�b�N�X�̐ݒ�i�O�p�`2�Ŏl�p�`��\���j
    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;

    // �J�E���^�̍X�V
    vertex_count += 4;
    index_count += 6;
}

// �e�L�X�g�`��i�������ƂɎl�p�`�𐶐��j
void dx11_draw_text(const char* text, int x, int y, mu_Color color) {
    mu_Rect dst = { x, y, 0, 0 };
    mu_Rect src;
    
    for (const char* p = text; *p; p++) {
        // ASCII�����̂݃T�|�[�g
        if (*p < 32 || *p > 127) {
            continue;
        }
        
        // atlas.inl�̃t�H���g�O���t�����擾
        src = atlas[ATLAS_FONT + (*p - 32)];
        dst.w = src.w;
        dst.h = src.h;
        
        // ������`��
        dx11_push_quad_atlas(dst, src, color);
        
        // ���̕����ʒu��
        dst.x += dst.w;
    }
}

// �A�C�R���`��
void dx11_draw_icon(int icon_id, mu_Rect rect, mu_Color color) {
    // �L���ȃA�C�R��ID���`�F�b�N
    if (icon_id < MU_ICON_CLOSE || icon_id > MU_ICON_MAX) {
        return;
    }

    // �A�C�R���̃\�[�X��`���擾
    mu_Rect src = atlas[icon_id];
    
    // �A�C�R���𒆉��ɔz�u���邽�߂̈ʒu����
    mu_Rect dst = rect;
    dst.x += (rect.w - src.w) / 2;
    dst.y += (rect.h - src.h) / 2;
    dst.w = src.w;
    dst.h = src.h;
    
    // �A�C�R����`��
    dx11_push_quad_atlas(dst, src, color);
}

// �N���b�v��`�̐ݒ�
void dx11_set_clip_rect(mu_Rect rect) {
    // �o�b�t�@���t���b�V�����Č��݂̕`����R�~�b�g
    dx11_flush_vertices();
    
    // DirectX11�̃V�U�[��`��ݒ�
    D3D11_RECT scissorRect;
    scissorRect.left = rect.x;
    scissorRect.top = rect.y;
    scissorRect.right = rect.x + rect.w;
    scissorRect.bottom = rect.y + rect.h;
    
    // �f�o�b�O�o��: �V�U�[��`��ݒ�
    char debugMsg[256];
    sprintf(debugMsg, "Setting scissor rect: %d,%d,%d,%d\n", 
            rect.x, rect.y, rect.w, rect.h);
    OutputDebugStringA(debugMsg);
    
    g_context->lpVtbl->RSSetScissorRects(g_context, 1, &scissorRect);
}

// �R�}���h�o�b�t�@����
void dx11_begin_commands(void) {
    command_count = 0;
}

void dx11_add_rect_command(mu_Rect rect, mu_Color color) {
    if (command_count < sizeof(commands) / sizeof(commands[0])) {
        commands[command_count].type = MU_COMMAND_RECT;
        commands[command_count].rect = rect;
        commands[command_count].color = color;
        command_count++;
    }
}

void dx11_add_text_command(const char* text, int x, int y, mu_Color color) {
    if (command_count < sizeof(commands) / sizeof(commands[0])) {
        commands[command_count].type = MU_COMMAND_TEXT;
        commands[command_count].rect.x = x;
        commands[command_count].rect.y = y;
        commands[command_count].color = color;
        strncpy(commands[command_count].text, text, sizeof(commands[command_count].text) - 1);
        commands[command_count].text[sizeof(commands[command_count].text) - 1] = '\0';
        command_count++;
    }
}

void dx11_end_commands(void) {
    // �R�}���h���X�g������������Ԃł̃t�b�N�|�C���g
}

// �R�}���h�o�b�t�@����̕`��
void dx11_render_commands(void) {
    for (int i = 0; i < command_count; i++) {
        SimpleCommand* cmd = &commands[i];
        
        switch (cmd->type) {
            case MU_COMMAND_RECT:
                // �d�v: �P����dx11_push_quad�ł͂Ȃ��Aatlas[ATLAS_WHITE]���g�p����悤�C��
                dx11_push_quad_atlas(cmd->rect, atlas[ATLAS_WHITE], cmd->color);
                break;
                
            case MU_COMMAND_TEXT:
                dx11_draw_text(cmd->text, cmd->rect.x, cmd->rect.y, cmd->color);
                break;
                
            case MU_COMMAND_CLIP:
                dx11_set_clip_rect(cmd->rect);
                break;
                
            case MU_COMMAND_ICON:
                dx11_draw_icon(cmd->rect.w, cmd->rect, cmd->color); // rect.w�ɃA�C�R��ID���i�[
                break;
        }
    }
    
    // ���ׂẴR�}���h��`�悵����Ƀt���b�V��
    dx11_flush_vertices();
}