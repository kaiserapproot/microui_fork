/**
 * dx11_renderer.c
 * DirectX11でmicrouiをレンダリングするためのレンダラー
 * renderer.c（DirectX9版）を参考にDirectX11用に実装
 */
#include <Windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <stdbool.h>
#include <stdio.h>  // sprintfのためのインクルード
#include "dx11_renderer.h"
#include "microui.h"

// DirectX11関連のGUID参照
// IID_ID3D11Texture2D: DirectX11のテクスチャ2DインターフェースのGUID。GetBufferで必要。
EXTERN_C const GUID IID_ID3D11Texture2D =
{ 0x6f15aaf2, 0xd208, 0x4e89, {0x9a, 0xb4, 0x48, 0x95, 0x35, 0xd3, 0x4f, 0x9c} };

// atlas.inlをインクルード（フォントデータ）
#include "atlas.inl"

// DirectX11関連のグローバル変数
ID3D11Device* g_device = NULL; // DirectX11デバイス本体。リソース生成・管理に使用。
ID3D11DeviceContext* g_context = NULL; // 描画コマンド発行用のデバイスコンテキスト。
ID3D11RenderTargetView* g_renderTargetView = NULL; // 描画先のレンダーターゲット。
ID3D11Texture2D* g_atlasTexture = NULL; // microui用フォント/アイコンのテクスチャ。
ID3D11ShaderResourceView* g_atlasSRV = NULL; // シェーダーからテクスチャ参照用SRV。
ID3D11Buffer* g_vertexBuffer = NULL; // 頂点バッファ。UI描画用頂点データ格納。
ID3D11Buffer* g_indexBuffer = NULL; // インデックスバッファ。三角形描画用。
ID3D11VertexShader* g_vertexShader = NULL; // 頂点シェーダー。
ID3D11PixelShader* g_pixelShader = NULL; // ピクセルシェーダー。
ID3D11InputLayout* g_inputLayout = NULL; // 頂点レイアウト定義。
ID3D11BlendState* g_blendState = NULL; // アルファブレンド用ステート。
ID3D11SamplerState* g_samplerState = NULL; // テクスチャサンプラー。
ID3D11RasterizerState* g_rasterizerState = NULL; // シザー矩形有効化用ラスタライザ。
IDXGISwapChain* g_swapChain = NULL; // フレームバッファ切り替え用スワップチェーン。

// 頂点バッファとインデックスバッファの管理
static DX11Vertex vertices[MAX_VERTICES]; // UI描画用頂点データ配列。
static WORD indices[MAX_VERTICES * 3 / 2]; // 三角形描画用インデックス配列。
static int vertex_count = 0; // 現在の頂点数。
static int index_count = 0; // 現在のインデックス数。

// コマンドリスト
static SimpleCommand commands[1024]; // microuiコマンドを一時的に格納する配列。
static int command_count = 0; // コマンドリストの現在の数。

// ウィンドウサイズ
static int window_width = 0; // 現在の描画ウィンドウ幅。
static int window_height = 0; // 現在の描画ウィンドウ高さ。

// シェーダーコード（HLSL）
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
"PSInput main(VSInput input) {\n"  // エントリーポイントをmainに変更
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
"float4 main(PSInput input) : SV_TARGET {\n"  // エントリーポイントをmainに変更
"    float4 texColor = tex.Sample(samp, input.uv);\n"
"    return texColor * input.col;\n"
"}\n";

/**
 * CompileShaderFromString
 * HLSLソースコード文字列からシェーダーをコンパイルする。
 * @param shaderCode シェーダーソース文字列
 * @param entryPoint エントリーポイント関数名
 * @param shaderModel シェーダーモデル（例: vs_4_0）
 * @param ppBlob コンパイル済みバイナリ格納先
 * @return HRESULT（成功/失敗）
 */
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

/**
 * dx11_get_device
 * DirectX11デバイス本体へのポインタを返す。
 */
ID3D11Device* dx11_get_device(void) {
    return g_device;
}

/**
 * dx11_get_context
 * DirectX11デバイスコンテキストへのポインタを返す。
 */
ID3D11DeviceContext* dx11_get_context(void) {
    return g_context;
}

/**
 * dx11_get_commands
 * microuiコマンドバッファへのポインタを返す。
 */
SimpleCommand* dx11_get_commands(void) {
    return commands;
}

/**
 * dx11_get_command_count
 * microuiコマンドバッファのコマンド数を返す。
 */
int dx11_get_command_count(void) {
    return command_count;
}

/**
 * dx11_log_error
 * DirectX11 APIのエラーコードをデバッグ出力する。
 * @param hr HRESULTエラーコード
 * @param operation 操作名（文字列）
 */
void dx11_log_error(HRESULT hr, const char* operation) {
    char errorMsg[256];
    sprintf(errorMsg, "DirectX11 Error: %s failed with HRESULT: 0x%08X", operation, (unsigned int)hr);
    OutputDebugStringA(errorMsg);
    
    // エラーコードの詳細説明
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

/**
 * dx11_init
 * DirectX11の初期化処理。デバイス・スワップチェーン・各種リソース生成。
 * @param hWnd ウィンドウハンドル
 * @param width ウィンドウ幅
 * @param height ウィンドウ高さ
 * @return HRESULT（成功/失敗）
 */
HRESULT dx11_init(HWND hWnd, int width, int height) {
    HRESULT hr = S_OK;
    window_width = width;
    window_height = height;

    // スワップチェーンの設定
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

    // デバイス、デバイスコンテキスト、スワップチェーンの作成
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    hr = D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, &featureLevel, 1,
                                      D3D11_SDK_VERSION, &swapChainDesc, &g_swapChain, &g_device, NULL, &g_context);
    if (FAILED(hr)) {
        dx11_log_error(hr, "D3D11CreateDeviceAndSwapChain");
        return hr;
    }

    // レンダーターゲットビューの作成
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

    // ラスタライザステートの作成（シザー矩形を有効にする）
    D3D11_RASTERIZER_DESC rasterizerDesc;
    ZeroMemory(&rasterizerDesc, sizeof(rasterizerDesc));
    rasterizerDesc.FillMode = D3D11_FILL_SOLID;
    rasterizerDesc.CullMode = D3D11_CULL_NONE;
    rasterizerDesc.FrontCounterClockwise = FALSE;
    rasterizerDesc.DepthClipEnable = TRUE;
    rasterizerDesc.ScissorEnable = TRUE; // シザー矩形を有効化
    
    hr = g_device->lpVtbl->CreateRasterizerState(g_device, &rasterizerDesc, &g_rasterizerState);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateRasterizerState");
        return hr;
    }
    
    // ラスタライザステートを設定
    g_context->lpVtbl->RSSetState(g_context, g_rasterizerState);

    // シェーダーをコンパイル
    ID3DBlob* vsBlob = NULL;
    ID3DBlob* psBlob = NULL;
    
    // 頂点シェーダーをコンパイル
    hr = CompileShaderFromString(vertex_shader_code, "main", "vs_4_0", &vsBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CompileShaderFromString (VS)");
        return hr;
    }
    
    // 頂点シェーダーの作成
    hr = g_device->lpVtbl->CreateVertexShader(g_device, 
                                           ID3D10Blob_GetBufferPointer(vsBlob),
                                           ID3D10Blob_GetBufferSize(vsBlob), 
                                           NULL, &g_vertexShader);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateVertexShader");
        ID3D10Blob_Release(vsBlob);
        return hr;
    }

    // 入力レイアウトの設定
    D3D11_INPUT_ELEMENT_DESC layout[] = {
        { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(layout);

    // 入力レイアウトの作成
    hr = g_device->lpVtbl->CreateInputLayout(g_device, layout, numElements, 
                                          ID3D10Blob_GetBufferPointer(vsBlob), 
                                          ID3D10Blob_GetBufferSize(vsBlob),
                                          &g_inputLayout);
    ID3D10Blob_Release(vsBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateInputLayout");
        return hr;
    }

    // ピクセルシェーダーをコンパイル
    hr = CompileShaderFromString(pixel_shader_code, "main", "ps_4_0", &psBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CompileShaderFromString (PS)");
        return hr;
    }
    
    // ピクセルシェーダーの作成
    hr = g_device->lpVtbl->CreatePixelShader(g_device, 
                                          ID3D10Blob_GetBufferPointer(psBlob),
                                          ID3D10Blob_GetBufferSize(psBlob), 
                                          NULL, &g_pixelShader);
    ID3D10Blob_Release(psBlob);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreatePixelShader");
        return hr;
    }

    // 頂点バッファの作成
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

    // インデックスバッファの作成
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

    // RGBA形式に変換（R=G=B=255, A=atlas_texture値）
    unsigned char* rgbaData = (unsigned char*)malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    if (rgbaData == NULL) {
        OutputDebugStringA("Failed to allocate memory for texture data\n");
        return E_OUTOFMEMORY;
    }

    // atlas.inlからテクスチャデータを変換
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        rgbaData[i * 4 + 0] = 255;  // R
        rgbaData[i * 4 + 1] = 255;  // G
        rgbaData[i * 4 + 2] = 255;  // B
        rgbaData[i * 4 + 3] = atlas_texture[i];  // A - 透明度
    }

    // atlas.inlからテクスチャを作成
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

    D3D11_SUBRESOURCE_DATA textureData = { 0 };
    textureData.pSysMem = rgbaData;
    textureData.SysMemPitch = ATLAS_WIDTH * 4;
    textureData.SysMemSlicePitch = 0;

    hr = g_device->lpVtbl->CreateTexture2D(g_device, &textureDesc, &textureData, &g_atlasTexture);
    free(rgbaData);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateTexture2D");
        return hr;
    }

    // テクスチャからシェーダーリソースビューを作成
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = { 0 };
    srvDesc.Format = textureDesc.Format;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = g_device->lpVtbl->CreateShaderResourceView(g_device, (ID3D11Resource*)g_atlasTexture, &srvDesc, &g_atlasSRV);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateShaderResourceView");
        return hr;
    }

    // サンプラーステートの作成
    D3D11_SAMPLER_DESC samplerDesc = { 0 };
    samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT; // ピクセル化を防ぐためにポイントフィルタリング
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
    samplerDesc.MinLOD = 0;
    samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

    hr = g_device->lpVtbl->CreateSamplerState(g_device, &samplerDesc, &g_samplerState);
    if (FAILED(hr)) {
        dx11_log_error(hr, "CreateSamplerState");
        return hr;
    }

    // ブレンドステートの作成
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

    // ビューポートの設定
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

/**
 * dx11_cleanup
 * DirectX11リソースの解放処理。全てのCOMオブジェクトをRelease。
 */
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
    if (g_rasterizerState) g_rasterizerState->lpVtbl->Release(g_rasterizerState);
    if (g_renderTargetView) g_renderTargetView->lpVtbl->Release(g_renderTargetView);
    if (g_context) g_context->lpVtbl->Release(g_context);
    if (g_swapChain) g_swapChain->lpVtbl->Release(g_swapChain);
    if (g_device) g_device->lpVtbl->Release(g_device);
}

/**
 * dx11_clear_screen
 * 画面全体を指定色でクリアする。
 * @param r 赤成分（0-255）
 * @param g 緑成分（0-255）
 * @param b 青成分（0-255）
 * @param a アルファ成分（0-255）
 */
void dx11_clear_screen(float r, float g, float b, float a) {
    float color[4] = { r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f };
    g_context->lpVtbl->ClearRenderTargetView(g_context, g_renderTargetView, color);
}

/**
 * dx11_begin_frame
 * 1フレームの描画開始処理。各種ステート・バッファ初期化。
 */
void dx11_begin_frame(void) {
    // レンダーターゲットの設定
    g_context->lpVtbl->OMSetRenderTargets(g_context, 1, &g_renderTargetView, NULL);

    // インプットレイアウトの設定
    g_context->lpVtbl->IASetInputLayout(g_context, g_inputLayout);

    // シェーダーの設定
    g_context->lpVtbl->VSSetShader(g_context, g_vertexShader, NULL, 0);
    g_context->lpVtbl->PSSetShader(g_context, g_pixelShader, NULL, 0);

    // サンプラーとテクスチャの設定
    g_context->lpVtbl->PSSetSamplers(g_context, 0, 1, &g_samplerState);
    g_context->lpVtbl->PSSetShaderResources(g_context, 0, 1, &g_atlasSRV);

    // ブレンドステートの設定
    float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    g_context->lpVtbl->OMSetBlendState(g_context, g_blendState, blendFactor, 0xffffffff);

    // ラスタライザステートを再設定（念のため）
    g_context->lpVtbl->RSSetState(g_context, g_rasterizerState);

    // プリミティブトポロジーの設定
    g_context->lpVtbl->IASetPrimitiveTopology(g_context, D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 頂点カウントとインデックスカウントをリセット
    vertex_count = 0;
    index_count = 0;
}

/**
 * dx11_end_frame
 * 1フレームの描画終了処理。バッファのフラッシュ。
 */
void dx11_end_frame(void) {
    // バッファのフラッシュ
    dx11_flush_vertices();
}

/**
 * dx11_present
 * スワップチェーンをPresentし画面表示を更新。
 */
void dx11_present(void) {
    // スワップチェーンのプレゼント
    if (g_swapChain) {
        HRESULT hr = g_swapChain->lpVtbl->Present(g_swapChain, 1, 0);
        if (FAILED(hr)) {
            dx11_log_error(hr, "Present");
        }
    }
}

/**
 * dx11_flush_vertices
 * 頂点・インデックスバッファをGPUに転送し描画。
 * バッファが満杯時やフレーム終了時に呼ばれる。
 */
void dx11_flush_vertices(void) {
    if (vertex_count == 0) {
        return;
    }

    // 頂点バッファの更新
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

    // インデックスバッファの更新
    hr = g_context->lpVtbl->Map(g_context, (ID3D11Resource*)g_indexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &resource);
    if (SUCCEEDED(hr)) {
        memcpy(resource.pData, indices, index_count * sizeof(WORD));
        g_context->lpVtbl->Unmap(g_context, (ID3D11Resource*)g_indexBuffer, 0);
    }
    else {
        dx11_log_error(hr, "Map (Index Buffer)");
        return;
    }

    // 頂点バッファとインデックスバッファを設定
    UINT stride = sizeof(DX11Vertex);
    UINT offset = 0;
    g_context->lpVtbl->IASetVertexBuffers(g_context, 0, 1, &g_vertexBuffer, &stride, &offset);
    g_context->lpVtbl->IASetIndexBuffer(g_context, g_indexBuffer, DXGI_FORMAT_R16_UINT, 0);

    // 描画
    g_context->lpVtbl->DrawIndexed(g_context, index_count, 0, 0);

    // カウントをリセット
    vertex_count = 0;
    index_count = 0;
}

/**
 * dx11_push_quad
 * 指定矩形領域に単色四角形を描画する。
 * @param dst 描画先矩形
 * @param color 四角形の色
 */
void dx11_push_quad(mu_Rect dst, mu_Color color) {
    // アトラスの白色部分を使用して単色四角形を描画
    dx11_push_quad_atlas(dst, atlas[ATLAS_WHITE], color);
}

/**
 * dx11_push_quad_atlas
 * 指定矩形領域にアトラスUVを使った四角形を描画。
 * @param dst 描画先矩形
 * @param src アトラス上のUV矩形
 * @param color 四角形の色
 */
void dx11_push_quad_atlas(mu_Rect dst, mu_Rect src, mu_Color color) {
    if (vertex_count + 4 > MAX_VERTICES || index_count + 6 > MAX_VERTICES * 3 / 2) {
        dx11_flush_vertices();
    }

    // UV座標の計算（アトラスのサイズで正規化）
    float u0 = (float)src.x / ATLAS_WIDTH;
    float v0 = (float)src.y / ATLAS_HEIGHT;
    float u1 = (float)(src.x + src.w) / ATLAS_WIDTH;
    float v1 = (float)(src.y + src.h) / ATLAS_HEIGHT;

    // スクリーン座標を正規化（-1.0～1.0の範囲に変換）
    float x0 = 2.0f * dst.x / window_width - 1.0f;
    float y0 = 1.0f - 2.0f * dst.y / window_height;
    float x1 = 2.0f * (dst.x + dst.w) / window_width - 1.0f;
    float y1 = 1.0f - 2.0f * (dst.y + dst.h) / window_height;

    // 頂点の設定
    DX11Vertex* v = &vertices[vertex_count];

    // 左上
    v[0].pos[0] = x0;
    v[0].pos[1] = y0;
    v[0].uv[0] = u0;
    v[0].uv[1] = v0;
    v[0].color[0] = color.r;
    v[0].color[1] = color.g;
    v[0].color[2] = color.b;
    v[0].color[3] = color.a;

    // 右上
    v[1].pos[0] = x1;
    v[1].pos[1] = y0;
    v[1].uv[0] = u1;
    v[1].uv[1] = v0;
    v[1].color[0] = color.r;
    v[1].color[1] = color.g;
    v[1].color[2] = color.b;
    v[1].color[3] = color.a;

    // 左下
    v[2].pos[0] = x0;
    v[2].pos[1] = y1;
    v[2].uv[0] = u0;
    v[2].uv[1] = v1;
    v[2].color[0] = color.r;
    v[2].color[1] = color.g;
    v[2].color[2] = color.b;
    v[2].color[3] = color.a;

    // 右下
    v[3].pos[0] = x1;
    v[3].pos[1] = y1;
    v[3].uv[0] = u1;
    v[3].uv[1] = v1;
    v[3].color[0] = color.r;
    v[3].color[1] = color.g;
    v[3].color[2] = color.b;
    v[3].color[3] = color.a;

    // インデックスの設定（三角形2つで四角形を表現）
    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;

    // カウンタの更新
    vertex_count += 4;
    index_count += 6;
}

/**
 * dx11_draw_text
 * 指定座標にテキスト文字列を描画。
 * @param text 描画する文字列
 * @param x X座標
 * @param y Y座標
 * @param color 文字色
 */
void dx11_draw_text(const char* text, int x, int y, mu_Color color) {
    mu_Rect dst = { x, y, 0, 0 };
    mu_Rect src;
    const unsigned char* p;
    for (p = (const unsigned char*)text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue; // UTF-8継続バイトはスキップ
        int chr = mu_min(*p, 127);         // 範囲外はDEL(127)に丸める
        src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        dx11_push_quad_atlas(dst, src, color);
        dst.x += dst.w;
    }
}

/**
 * dx11_draw_icon
 * 指定矩形領域にアイコンを描画。
 * @param icon_id アイコンID
 * @param rect 描画先矩形
 * @param color アイコン色
 */
void dx11_draw_icon(int icon_id, mu_Rect rect, mu_Color color) {
    // 有効なアイコンIDかチェック
    if (icon_id < 0 || icon_id > MU_ICON_MAX) {
        return;
    }

    // アイコンのソース矩形を取得
    mu_Rect src = atlas[icon_id];
    
    // アイコンを中央に配置するための位置調整
    mu_Rect dst = rect;
    dst.x += (rect.w - src.w) / 2;
    dst.y += (rect.h - src.h) / 2;
    dst.w = src.w;
    dst.h = src.h;
    
    // アイコンを描画
    dx11_push_quad_atlas(dst, src, color);
}

/**
 * dx11_set_clip_rect
 * 描画クリップ矩形（シザー矩形）を設定。
 * @param rect クリップ矩形
 */
void dx11_set_clip_rect(mu_Rect rect) {
    // バッファをフラッシュして現在の描画をコミット
    dx11_flush_vertices();
    
    // DirectX11のシザー矩形を設定
    D3D11_RECT scissorRect;
    scissorRect.left = rect.x;
    scissorRect.top = rect.y;
    scissorRect.right = rect.x + rect.w;
    scissorRect.bottom = rect.y + rect.h;
    
    g_context->lpVtbl->RSSetScissorRects(g_context, 1, &scissorRect);
}

/**
 * dx11_begin_commands
 * microuiコマンドバッファの初期化（リセット）。
 */
void dx11_begin_commands(void) {
    command_count = 0;
}

/**
 * dx11_add_rect_command
 * コマンドバッファに矩形描画コマンドを追加。
 * @param rect 描画矩形
 * @param color 描画色
 */
void dx11_add_rect_command(mu_Rect rect, mu_Color color) {
    if (command_count < sizeof(commands) / sizeof(commands[0])) {
        commands[command_count].type = MU_COMMAND_RECT;
        commands[command_count].rect = rect;
        commands[command_count].color = color;
        command_count++;
    }
}

/**
 * dx11_add_text_command
 * コマンドバッファにテキスト描画コマンドを追加。
 * @param text 描画文字列
 * @param x X座標
 * @param y Y座標
 * @param color 文字色
 */
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

/**
 * dx11_end_commands
 * コマンドバッファの終了処理（フックポイント）。
 */
void dx11_end_commands(void) {
    // コマンドリストが完成した状態でのフックポイント
}

/**
 * dx11_render_commands
 * コマンドバッファに格納されたmicrouiコマンドを描画。
 */
void dx11_render_commands(void) {
    for (int i = 0; i < command_count; i++) {
        SimpleCommand* cmd = &commands[i];
        
        switch (cmd->type) {
            case MU_COMMAND_RECT:
                dx11_push_quad_atlas(cmd->rect, atlas[ATLAS_WHITE], cmd->color);
                break;
                
            case MU_COMMAND_TEXT:
                dx11_draw_text(cmd->text, cmd->rect.x, cmd->rect.y, cmd->color);
                break;
                
            case MU_COMMAND_CLIP:
                dx11_set_clip_rect(cmd->rect);
                break;
                
            case MU_COMMAND_ICON:
                dx11_draw_icon(cmd->rect.w, cmd->rect, cmd->color);
                break;
        }
    }
    
    // すべてのコマンドを描画した後にフラッシュ
    dx11_flush_vertices();
}