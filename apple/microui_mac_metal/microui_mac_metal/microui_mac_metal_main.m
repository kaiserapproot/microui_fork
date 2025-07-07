/*
 * MicroUI Metal版 テキストボックスフォーカス問題 修正履歴 (2025年6月24日)
 * ================================================================================
 * 
 * 【問題】
 * Metal版のMicroUIでテキストボックスをクリックした際、フォーカスが一瞬だけ設定されて
 * すぐに失われてしまい、テキスト入力ができない状態が発生していた。
 * 
 * 症状:
 * - クリック後にカーソルが一瞬現れるが、すぐに消えてしまう
 * - テキスト入力が全くできない
 * - 頻繁にmouseExitedが呼ばれる（ログで確認）
 * - OpenGL版では同じMicroUIライブラリで正常動作
 * 
 * 【根本原因】
 * 1. マウスイベント処理順序の違い
 *    - OpenGL版: mu_input_mousemove() → mu_input_mousedown() の順序
 *    - Metal版:  mu_input_mousedown() のみで、事前のmousemove処理なし
 *    - MicroUIは正確なマウス位置把握後にクリック処理を行う必要がある
 * 
 * 2. フォーカス状態の不適切なリセット
 *    - mouseUpやmouseExitedイベントでフォーカスが意図せずリセットされる
 *    - テキストボックスのフォーカスは他のUI要素クリックまで維持されるべき
 * 
 * 3. マウストラッキングエリアの管理問題
 *    - 重複するトラッキングエリアによりmouseExitedが過度に呼ばれる
 *    - ビューサイズ変更時の適切なトラッキングエリア管理不足
 * 
 * 【修正内容】
 * 1. マウスイベント処理順序の統一（OpenGL版と同じ順序に変更）
 *    - mouseDown: mu_input_mousemove() → mu_input_mousedown() の順序に修正
 *    - mouseUp:   mu_input_mousemove() → mu_input_mouseup() の順序に修正
 * 
 * 2. フォーカス状態の保護
 *    - mouseUpでのフォーカスリセット（focus = 0）をコメントアウト
 *    - mouseExitedでのフォーカスリセットを防止
 *    - テキストボックスのフォーカス維持を優先
 * 
 * 3. 詳細なデバッグログの追加
 *    - マウスイベント前後でのフォーカス状態確認ログ
 *    - フレーム処理前後でのフォーカス状態確認ログ
 *    - 問題特定と解決を支援する詳細情報出力
 * 
 * 4. マウストラッキングエリアの改善
 *    - viewDidLayoutでの適切なトラッキングエリア管理
 *    - 既存エリア削除→新規作成のサイクル改善
 *    - 重複によるmouseExited頻発の防止
 * 
 * 【結果】
 * - Metal版でもOpenGL版と同様のテキスト入力が正常動作
 * - フォーカス維持によりテキストボックスの使用感向上
 * - 不要なmouseExitedイベントの削減
 * - 異なるレンダリングバックエンド間でのUI動作統一
 * 
 * 【学んだ教訓】
 * - UIフレームワーク移植時はイベント処理順序が重要
 * - フォーカス管理は慎重に行い、不必要なリセットを避ける
 * - 十分なデバッグログは問題解決を大幅に加速する
 * - 異なるバックエンド間でもUIロジックは統一すべき
 */

// MetalMicroUI.h
#ifndef MetalMicroUI_h
#define MetalMicroUI_h

#import <MetalKit/MetalKit.h>
#import <Cocoa/Cocoa.h>
#include "microui.h"
#include "atlas.inl"  // atlas.inlをインクルード

// 頂点構造体
typedef struct {
    vector_float2 position;
    vector_float2 texCoord;
    vector_float4 color;
} MetalVertex;

// MetalRendererクラス
@interface MetalRenderer : NSObject <MTKViewDelegate>
@property (nonatomic, strong) id<MTLDevice> device;
@property (nonatomic, strong) id<MTLCommandQueue> commandQueue;
@property (nonatomic, strong) id<MTLRenderPipelineState> pipelineState;
@property (nonatomic, strong) id<MTLBuffer> vertexBuffer;
@property (nonatomic, strong) id<MTLBuffer> indexBuffer;
@property (nonatomic, strong) id<MTLBuffer> uniformBuffer;
@property (nonatomic, strong) id<MTLTexture> atlasTexture;
@property (nonatomic, strong) id<MTLSamplerState> samplerState;
@property (nonatomic, assign) mu_Context *muiContext;
@property (nonatomic, assign) int viewWidth;
@property (nonatomic, assign) int viewHeight;

- (instancetype)initWithMetalKitView:(MTKView *)view;
- (void)setupRenderPipeline;
- (void)createAtlasTexture;
- (void)renderFrame;
@end

// 前方宣言
@class MicroUIMTKView;

// ViewControllerクラス
@interface ViewController : NSViewController
@property (nonatomic, strong) MicroUIMTKView *metalView;
@property (nonatomic, strong) MetalRenderer *renderer;
- (void)handleKeyDown:(NSEvent *)event;
- (void)handleKeyUp:(NSEvent *)event;
@end

// カスタムMTKViewクラス - キーボード入力を受け取るため
@interface MicroUIMTKView : MTKView
@property (nonatomic, weak) ViewController* viewController;
@end

#endif /* MetalMicroUI_h */

// MetalMicroUI.m
#import <simd/simd.h>

// グローバル変数
static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[4] = { 90, 95, 100, 105 };

// Metal用のシェーダー文字列
static NSString *vertexShaderSource = @"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"struct VertexIn {\n"
"    float2 position [[attribute(0)]];\n"
"    float2 texCoord [[attribute(1)]];\n"
"    float4 color [[attribute(2)]];\n"
"};\n"
"\n"
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texCoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"struct Uniforms {\n"
"    float4x4 projectionMatrix;\n"
"};\n"
"\n"
"vertex VertexOut vertex_main(VertexIn in [[stage_in]],\n"
"                           constant Uniforms& uniforms [[buffer(1)]]) {\n"
"    VertexOut out;\n"
"    out.position = uniforms.projectionMatrix * float4(in.position, 0.0, 1.0);\n"
"    out.texCoord = in.texCoord;\n"
"    out.color = in.color;\n"
"    return out;\n"
"}\n";

static NSString *fragmentShaderSource = @"#include <metal_stdlib>\n"
"using namespace metal;\n"
"\n"
"fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
"                            texture2d<half> colorTexture [[texture(0)]],\n"
"                            sampler textureSampler [[sampler(0)]]) {\n"
"    half4 colorSample = colorTexture.sample(textureSampler, in.texCoord);\n"
"    return float4(colorSample) * in.color;\n"
"}\n";

// 描画用のバッファ
#define MAX_VERTICES 16384
static MetalVertex vertices[MAX_VERTICES];
static uint16_t indices[MAX_VERTICES * 3 / 2];
static int vertex_count = 0;
static int index_count = 0;

// MicroUI用のコールバック関数（元のDirectX版と同じロジック）
static int text_width(mu_Font font, const char* text, int len) {
    int res = 0;
    const char* p;
    int chr;
    
    if (len < 0) len = (int)strlen(text);
    
    for (p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

static int text_height(mu_Font font) {
    return 18;
}

// 安全なwrite_log関数に修正
// static void write_log(const char* text) {
//     if (!text) return;  // NULLポインタチェック
    
//     // NSLogに出力
//     NSLog(@"[MicroUI] %s", text);
    
//     // 既存のlogbufにも記録（互換性のため）
//     size_t text_len = strlen(text);
//     size_t current_len = strlen(logbuf);
    
//     // バッファオーバーフローを防ぐ
//     if (current_len + text_len + 2 >= sizeof(logbuf)) {
//         // バッファがフルの場合、古いログを削除して新しいログを追加
//         memmove(logbuf, logbuf + 1000, current_len - 1000);
//         logbuf[current_len - 1000] = '\0';
//         current_len = strlen(logbuf);
//     }
    
//     // 改行を追加（最初のエントリでない場合）
//     if (logbuf[0] != '\0') {
//         strcat(logbuf, "\n");
//     }
    
//     // テキストを安全に追加
//     strncat(logbuf, text, sizeof(logbuf) - strlen(logbuf) - 1);
    
//     logbuf_updated = 1;
// }

// より安全なuint8_slider関数
// static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
//     if (!ctx || !value) return 0;  // NULLポインタチェック
    
//     int res;
//     static float tmp;
//     mu_push_id(ctx, &value, sizeof(value));
//     tmp = *value;
//     res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
//     *value = (unsigned char)tmp;  // 明示的なキャスト
//     mu_pop_id(ctx);
//     return res;
// }

// MetalRenderer実装
@implementation MetalRenderer

// MicroUIコンテキストの初期化を強化
- (instancetype)initWithMetalKitView:(MTKView *)view {
    if (self = [super init]) {
        self.device = MTLCreateSystemDefaultDevice();
        if (!self.device) {
            NSLog(@"Metal is not supported on this device");
            return nil;
        }
        
        self.commandQueue = [self.device newCommandQueue];
        view.device = self.device;
        view.delegate = self;
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.clearColor = MTLClearColorMake(bg[0]/255.0, bg[1]/255.0, bg[2]/255.0, 1.0);
        
        // MicroUIコンテキストの初期化を強化
        self.muiContext = malloc(sizeof(mu_Context));
        memset(self.muiContext, 0, sizeof(mu_Context));
        mu_init(self.muiContext);
        self.muiContext->text_width = text_width;
        self.muiContext->text_height = text_height;
        
        // 初期ビューサイズを設定
        self.viewWidth = view.bounds.size.width;
        self.viewHeight = view.bounds.size.height;
        
        [self setupRenderPipeline];
        [self createAtlasTexture];
        [self createBuffers];
        
        // サンプラーステートの作成
        MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
        samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
        samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
        samplerDescriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
        samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
        self.samplerState = [self.device newSamplerStateWithDescriptor:samplerDescriptor];
        
        // 初期プロジェクション行列を設定
        [self updateProjectionMatrix:view.bounds.size];
    }
    return self;
}


- (void)setupRenderPipeline {
    NSError *error = nil;
    
    // シェーダーライブラリの作成を修正
    NSString *shaderSource = [NSString stringWithFormat:@"%@\n%@", vertexShaderSource, fragmentShaderSource];
    
    id<MTLLibrary> library = [self.device newLibraryWithSource:shaderSource
                                                       options:nil
                                                         error:&error];
    if (!library) {
        NSLog(@"Error creating shader library: %@", error);
        NSLog(@"Shader source: %@", shaderSource);
        return;
    }
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];
    
    if (!vertexFunction) {
        NSLog(@"Error: vertex_main function not found");
        return;
    }
    
    if (!fragmentFunction) {
        NSLog(@"Error: fragment_main function not found");
        return;
    }
    
    // パイプライン記述子の作成
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
    
    // ブレンド設定（DirectX版と同じ）
    pipelineDescriptor.colorAttachments[0].blendingEnabled = YES;
    pipelineDescriptor.colorAttachments[0].rgbBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].alphaBlendOperation = MTLBlendOperationAdd;
    pipelineDescriptor.colorAttachments[0].sourceRGBBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].sourceAlphaBlendFactor = MTLBlendFactorSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationRGBBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    pipelineDescriptor.colorAttachments[0].destinationAlphaBlendFactor = MTLBlendFactorOneMinusSourceAlpha;
    
    // 頂点記述子の設定
    MTLVertexDescriptor *vertexDescriptor = [[MTLVertexDescriptor alloc] init];
    vertexDescriptor.attributes[0].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[0].offset = 0;
    vertexDescriptor.attributes[0].bufferIndex = 0;
    
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = 8;
    vertexDescriptor.attributes[1].bufferIndex = 0;
    
    vertexDescriptor.attributes[2].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[2].offset = 16;
    vertexDescriptor.attributes[2].bufferIndex = 0;
    
    vertexDescriptor.layouts[0].stride = sizeof(MetalVertex);
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    
    self.pipelineState = [self.device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!self.pipelineState) {
        NSLog(@"Error creating pipeline state: %@", error);
    }
}

- (void)createAtlasTexture {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
    textureDescriptor.width = ATLAS_WIDTH;
    textureDescriptor.height = ATLAS_HEIGHT;
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    self.atlasTexture = [self.device newTextureWithDescriptor:textureDescriptor];
    
    // atlas.inlのデータをRGBA形式に変換（DirectX版と同じロジック）
    uint32_t *rgbaData = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        uint8_t alpha = atlas_texture[i];
        // DirectX版と同じ：RGBを白に、アルファを設定
        rgbaData[i] = (alpha << 24) | 0x00FFFFFF;
    }
    
    MTLRegion region = MTLRegionMake2D(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT);
    [self.atlasTexture replaceRegion:region
                         mipmapLevel:0
                           withBytes:rgbaData
                         bytesPerRow:ATLAS_WIDTH * 4];
    
    free(rgbaData);
}

- (void)createBuffers {
    self.vertexBuffer = [self.device newBufferWithLength:MAX_VERTICES * sizeof(MetalVertex)
                                                 options:MTLResourceStorageModeShared];
    
    self.indexBuffer = [self.device newBufferWithLength:(MAX_VERTICES * 3 / 2) * sizeof(uint16_t)
                                                options:MTLResourceStorageModeShared];
    
    self.uniformBuffer = [self.device newBufferWithLength:sizeof(matrix_float4x4)
                                                  options:MTLResourceStorageModeShared];
}

- (void)updateProjectionMatrix:(CGSize)size {
    // MicroUIの座標系に合わせたプロジェクション行列
    // 左上原点、Y軸下向きの2D座標系
    matrix_float4x4 projectionMatrix = {
        .columns[0] = { 2.0f / size.width, 0.0f, 0.0f, 0.0f },
        .columns[1] = { 0.0f, -2.0f / size.height, 0.0f, 0.0f },  // Y軸反転
        .columns[2] = { 0.0f, 0.0f, 1.0f, 0.0f },
        .columns[3] = { -1.0f, 1.0f, 0.0f, 1.0f }  // 原点を左上に移動
    };
    
    memcpy([self.uniformBuffer contents], &projectionMatrix, sizeof(matrix_float4x4));
}

- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color {
    // DirectX版のpush_quad関数と同じロジック
    if (vertex_count >= MAX_VERTICES - 4 || index_count >= MAX_VERTICES * 3 / 2 - 6) {
        [self flush];
    }
    
    // テクスチャ座標の計算
    float x = (float)src.x / ATLAS_WIDTH;
    float y = (float)src.y / ATLAS_HEIGHT;
    float w = (float)src.w / ATLAS_WIDTH;
    float h = (float)src.h / ATLAS_HEIGHT;
    
    vector_float4 vertexColor = {
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f
    };
    
    // 頂点データの設定（DirectX版と同じ順序）
    vertices[vertex_count + 0] = (MetalVertex){
        .position = { dst.x, dst.y },
        .texCoord = { x, y },
        .color = vertexColor
    };
    
    vertices[vertex_count + 1] = (MetalVertex){
        .position = { dst.x + dst.w, dst.y },
        .texCoord = { x + w, y },
        .color = vertexColor
    };
    
    vertices[vertex_count + 2] = (MetalVertex){
        .position = { dst.x, dst.y + dst.h },
        .texCoord = { x, y + h },
        .color = vertexColor
    };
    
    vertices[vertex_count + 3] = (MetalVertex){
        .position = { dst.x + dst.w, dst.y + dst.h },
        .texCoord = { x + w, y + h },
        .color = vertexColor
    };
    
    // インデックスデータの設定（DirectX版と同じ）
    indices[index_count + 0] = vertex_count + 0;
    indices[index_count + 1] = vertex_count + 1;
    indices[index_count + 2] = vertex_count + 2;
    indices[index_count + 3] = vertex_count + 2;
    indices[index_count + 4] = vertex_count + 1;
    indices[index_count + 5] = vertex_count + 3;
    
    vertex_count += 4;
    index_count += 6;
}

// フラッシュ処理を強化
- (void)flush {
    if (vertex_count == 0) return;
    
    // バッファオーバーフローのチェック
    if (vertex_count > MAX_VERTICES || index_count > MAX_VERTICES * 3 / 2) {
        char error_msg[128];
        sprintf(error_msg, "Buffer overflow: vertices=%d, indices=%d", vertex_count, index_count);
        write_log(error_msg);
        vertex_count = 0;
        index_count = 0;
        return;
    }
    
    // 頂点データをバッファにコピー
    memcpy([self.vertexBuffer contents], vertices, vertex_count * sizeof(MetalVertex));
    memcpy([self.indexBuffer contents], indices, index_count * sizeof(uint16_t));
    
    vertex_count = 0;
    index_count = 0;
}

- (void)drawRect:(mu_Rect)rect color:(mu_Color)color {
    [self pushQuad:rect src:atlas[ATLAS_WHITE] color:color];
}

- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color {
    // DirectX版のr_draw_text関数と同じロジック
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    
    for (const char* p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        [self pushQuad:dst src:src color:color];
        dst.x += dst.w;
    }
}

- (void)drawIcon:(int)iconId rect:(mu_Rect)rect color:(mu_Color)color {
    // DirectX版のr_draw_icon関数と同じロジック
    mu_Rect src = atlas[iconId];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    mu_Rect dst = { x, y, src.w, src.h };
    [self pushQuad:dst src:src color:color];
}

- (void)setClipRect:(mu_Rect)rect {
    // Metalでのシザー矩形設定（描画時に適用）
}

- (void)renderFrame {
    // フォーカス状態を詳しくログ（開始前）
    NSLog(@"[MicroUI Render] BEGIN Frame - hover=%d, focus=%d, mouse_down=%d, last_id=%d",
        self.muiContext->hover, self.muiContext->focus, self.muiContext->mouse_down, self.muiContext->last_id);

    mu_begin(self.muiContext);
    [self processFrame];
    mu_end(self.muiContext);
    
    // フォーカス状態を詳しくログ（終了後）
    NSLog(@"[MicroUI Render] END Frame - hover=%d, focus=%d, mouse_down=%d, last_id=%d",
          self.muiContext->hover, self.muiContext->focus, self.muiContext->mouse_down, self.muiContext->last_id);
}

- (void)processFrame {
    // DirectX版のtest_window関数と同等の処理
    if (mu_begin_window(self.muiContext, "Demo Window", mu_rect(40, 40, 300, 250))) {
        mu_Container* win = mu_get_current_container(self.muiContext);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);
        
        // ウィンドウ情報
        if (mu_header(self.muiContext, "Window Info")) {
            mu_layout_row(self.muiContext, 2, (int[]){54, -1}, 0);
            mu_label(self.muiContext, "Position:");
            mu_label(self.muiContext, "X: 40, Y: 40");
            mu_label(self.muiContext, "Size:");
            mu_label(self.muiContext, "W: 300, H: 250");
        }
        
        // テストボタン
        if (mu_header_ex(self.muiContext, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(self.muiContext, 3, (int[]){86, -110, -1}, 0);
            mu_label(self.muiContext, "Test buttons 1:");
            // ボタンやスライダー処理時にログを追加 (processFrameメソッド内)
            if (mu_button(self.muiContext, "Button 1")) {
                NSLog(@"[MicroUI Widget] Button 1 clicked - hover=%d, focus=%d",
                    self.muiContext->hover, self.muiContext->focus);
                write_log("Pressed button 1");
            }
            if (mu_button(self.muiContext, "Button 2")) {
                write_log("Pressed button 2");
            }
        }
        
        // 背景色スライダー
        if (mu_header_ex(self.muiContext, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(self.muiContext, 2, (int[]){-78, -1}, 0);
            
            mu_label(self.muiContext, "Red:");   uint8_slider(self.muiContext, (unsigned char*)&bg[0], 0, 255);
            mu_label(self.muiContext, "Green:"); uint8_slider(self.muiContext, (unsigned char*)&bg[1], 0, 255);
            mu_label(self.muiContext, "Blue:");  uint8_slider(self.muiContext, (unsigned char*)&bg[2], 0, 255);
        }
        
        mu_end_window(self.muiContext);
    }
    
    // ログウィンドウ
    if (mu_begin_window(self.muiContext, "Log Window", mu_rect(350, 40, 300, 200))) {
        // テキスト出力パネル
        mu_Container* panel;
        static char buf[128];
        int submitted = 0;
        int layout1[1] = { -1 };
        int layout2[2] = { -70, -1 };

        mu_layout_row(self.muiContext, 1, layout1, -25);
        mu_begin_panel(self.muiContext, "Log Output");
        panel = mu_get_current_container(self.muiContext);
        mu_layout_row(self.muiContext, 1, layout1, -1);
        mu_text(self.muiContext, logbuf);
        mu_end_panel(self.muiContext);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        // テキストボックスとサブミットボタン
        mu_layout_row(self.muiContext, 2, layout2, 0);
        
        // テキストボックスの処理にデバッグログを追加
        int textbox_result = mu_textbox(self.muiContext, buf, sizeof(buf));
        if (textbox_result & MU_RES_SUBMIT) {
            mu_set_focus(self.muiContext, self.muiContext->last_id);
            submitted = 1;
            write_log("Text submitted via Enter key");
        }
        if (textbox_result & MU_RES_CHANGE) {
            // テキストが変更された時のログ（デバッグ用）
            char debug_msg[256];
            snprintf(debug_msg, sizeof(debug_msg), "Text changed: '%s'", buf);
            // write_log(debug_msg);  // コメントアウト（ログが多すぎるため）
        }
        
        // フォーカス状態をログ
        static int focus_log_counter = 0;
        if (++focus_log_counter % 60 == 0) {  // 60フレームに1回
            char focus_msg[128];
            snprintf(focus_msg, sizeof(focus_msg), "Focus: %d, LastID: %d", 
                     self.muiContext->focus, self.muiContext->last_id);
            write_log(focus_msg);
        }
        
        if (mu_button(self.muiContext, "Submit")) { 
            submitted = 1; 
            write_log("Text submitted via Submit button");
        }
        if (submitted) {
            write_log(buf);
            buf[0] = '\0';
        }
        
        // クリアボタン
        mu_layout_row(self.muiContext, 1, (int[]){-1}, 0);
        if (mu_button(self.muiContext, "Clear")) {
            logbuf[0] = '\0';
        }
        mu_end_window(self.muiContext);
    }
}

#pragma mark - MTKViewDelegate

- (void)mtkView:(MTKView *)view drawableSizeWillChange:(CGSize)size {
    self.viewWidth = size.width;
    self.viewHeight = size.height;
    [self updateProjectionMatrix:size];
}

- (void)drawInMTKView:(MTKView *)view {
    [self renderFrame];
    
    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    
    if (renderPassDescriptor != nil) {
        // 背景色の設定
        renderPassDescriptor.colorAttachments[0].clearColor =
            MTLClearColorMake(bg[0]/255.0, bg[1]/255.0, bg[2]/255.0, 1.0);
        
        id<MTLRenderCommandEncoder> renderEncoder =
            [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        
        [renderEncoder setRenderPipelineState:self.pipelineState];
        [renderEncoder setVertexBuffer:self.uniformBuffer offset:0 atIndex:1];
        [renderEncoder setFragmentTexture:self.atlasTexture atIndex:0];
        [renderEncoder setFragmentSamplerState:self.samplerState atIndex:0];
        
        // デフォルトのシザー矩形を設定（全画面）
        MTLScissorRect defaultScissor;
        defaultScissor.x = 0;
        defaultScissor.y = 0;
        defaultScissor.width = view.drawableSize.width;
        defaultScissor.height = view.drawableSize.height;
        [renderEncoder setScissorRect:defaultScissor];
        
        // MicroUIコマンドの処理（修正版）
        mu_Command* cmd = NULL;
        while (mu_next_command(self.muiContext, &cmd)) {
            switch (cmd->type) {
                case MU_COMMAND_CLIP: {
                    // MicroUIのクリップ矩形処理を修正
                    mu_Rect clipRect = cmd->clip.rect;
                    
                    // MU_CLIP_PART (16777216) をチェック - これは「クリップなし」を意味
                    if (clipRect.w == 16777216 || clipRect.h == 16777216) {
                        // クリップなしの場合は全画面を設定
                        MTLScissorRect noClipScissor;
                        noClipScissor.x = 0;
                        noClipScissor.y = 0;
                        noClipScissor.width = view.drawableSize.width;
                        noClipScissor.height = view.drawableSize.height;
                        [renderEncoder setScissorRect:noClipScissor];
                    } else {
                        // 通常のクリップ矩形処理
                        // 負の値をチェック
                        if (clipRect.x < 0) clipRect.x = 0;
                        if (clipRect.y < 0) clipRect.y = 0;
                        if (clipRect.w <= 0) clipRect.w = 1;  // 最小値1に設定
                        if (clipRect.h <= 0) clipRect.h = 1;  // 最小値1に設定
                        
                        // 画面境界内にクリップ
                        if (clipRect.x >= view.drawableSize.width) clipRect.x = view.drawableSize.width - 1;
                        if (clipRect.y >= view.drawableSize.height) clipRect.y = view.drawableSize.height - 1;
                        if (clipRect.x + clipRect.w > view.drawableSize.width) {
                            clipRect.w = view.drawableSize.width - clipRect.x;
                        }
                        if (clipRect.y + clipRect.h > view.drawableSize.height) {
                            clipRect.h = view.drawableSize.height - clipRect.y;
                        }
                        
                        // 最終チェック：幅と高さが0以下の場合は最小値に設定
                        if (clipRect.w <= 0) clipRect.w = 1;
                        if (clipRect.h <= 0) clipRect.h = 1;
                        
                        MTLScissorRect scissorRect;
                        scissorRect.x = clipRect.x;
                        scissorRect.y = clipRect.y;
                        scissorRect.width = clipRect.w;
                        scissorRect.height = clipRect.h;
                        
                        [renderEncoder setScissorRect:scissorRect];
                    }
                    break;
                }
                case MU_COMMAND_RECT:
                    [self drawRect:cmd->rect.rect color:cmd->rect.color];
                    break;
                case MU_COMMAND_TEXT: {
                    mu_Vec2 text_pos = { cmd->text.pos.x, cmd->text.pos.y };
                    [self drawText:cmd->text.str pos:text_pos color:cmd->text.color];
                    break;
                }
                case MU_COMMAND_ICON:
                    [self drawIcon:cmd->icon.id rect:cmd->icon.rect color:cmd->icon.color];
                    break;
            }
        }
        
        // 最終的な描画
        if (vertex_count > 0) {
            [renderEncoder setVertexBuffer:self.vertexBuffer offset:0 atIndex:0];
            [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                      indexCount:index_count
                                       indexType:MTLIndexTypeUInt16
                                     indexBuffer:self.indexBuffer
                               indexBufferOffset:0];
        }
        
        [self flush];
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:view.currentDrawable];
    }
    
    [commandBuffer commit];
}

- (void)dealloc {
    if (self.muiContext) {
        free(self.muiContext);
    }
}

@end


@implementation MicroUIMTKView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    [self.viewController handleKeyDown:event];
}

- (void)keyUp:(NSEvent *)event {
    [self.viewController handleKeyUp:event];
}

- (void)mouseDown:(NSEvent *)event {
    [self.viewController mouseDown:event];
}

- (void)mouseUp:(NSEvent *)event {
    [self.viewController mouseUp:event];
}

- (void)mouseMoved:(NSEvent *)event {
    [self.viewController mouseMoved:event];
}

- (void)mouseDragged:(NSEvent *)event {
    [self.viewController mouseDragged:event];
}

- (void)rightMouseDown:(NSEvent *)event {
    [self.viewController rightMouseDown:event];
}

- (void)rightMouseUp:(NSEvent *)event {
    [self.viewController rightMouseUp:event];
}

- (void)scrollWheel:(NSEvent *)event {
    [self.viewController scrollWheel:event];
}

- (void)mouseExited:(NSEvent *)event {
    [self.viewController mouseExited:event];
}

@end

// ViewController実装
@implementation ViewController
// マウスの状態をログに出力する関数を追加
- (void)logMicroUIState:(NSString*)eventType location:(NSPoint)location {
    mu_Context *ctx = self.renderer.muiContext;
    NSLog(@"[MicroUI State] %@ - Position: (%.1f, %.1f)", eventType, location.x, location.y);
    NSLog(@"[MicroUI State] hover=%d, focus=%d, mouse_down=%d", ctx->hover, ctx->focus, ctx->mouse_down);
    NSLog(@"[MicroUI State] mouse_pressed=%d, updated_focus=%d", ctx->mouse_pressed, ctx->updated_focus);
}
- (void)viewDidLoad {
    [super viewDidLoad];
    
    // ウィンドウサイズを最初から大きく設定
    NSRect windowFrame = NSMakeRect(100, 100, 1024, 768);
    [self.view.window setFrame:windowFrame display:YES];
    [self.view.window setMinSize:NSMakeSize(800, 600)];
    [self.view.window setTitle:@"MicroUI Metal Demo"];
    
    // MetalViewの設定
    self.metalView = [[MicroUIMTKView alloc] initWithFrame:self.view.bounds];
    self.metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    self.metalView.viewController = self;
    [self.view addSubview:self.metalView];
    
    // レンダラーの初期化
    self.renderer = [[MetalRenderer alloc] initWithMetalKitView:self.metalView];
    
    if (!self.renderer) {
        NSLog(@"Failed to initialize Metal renderer");
        return;
    }
    
    // イベント処理の設定を強化
    // MTKViewをファーストレスポンダーに設定
    [self.view.window makeFirstResponder:self.metalView];
    [self.view.window setAcceptsMouseMovedEvents:YES];
    
    // 既存のトラッキングエリアを削除してから新しく追加
    for (NSTrackingArea *area in [self.metalView trackingAreas]) {
        [self.metalView removeTrackingArea:area];
    }
    
    // トラッキングエリアを追加（マウス離脱検知のため）
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.metalView.bounds
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
        owner:self
        userInfo:nil];
    [self.metalView addTrackingArea:trackingArea];
    
    // 初期ログメッセージ
    write_log("MicroUI Metal Demo Started");
    write_log("Window size: 1024x768");
    write_log("Mouse tracking enabled");
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

// マウス座標を正確に取得するヘルパーメソッド
- (NSPoint)getMouseLocation:(NSEvent *)event {
    NSPoint location = [event locationInWindow];
    NSPoint viewLocation = [self.metalView convertPoint:location fromView:nil];
    
    // Y座標を反転（MicroUIは左上原点、Cocoaは左下原点）
    viewLocation.y = self.metalView.bounds.size.height - viewLocation.y;
    
    return viewLocation;
}

// マウス入力処理も安全に修正
- (void)mouseDown:(NSEvent *)event {
    @try {
        // マウスクリック時にMTKViewがファーストレスポンダーになることを確認
        [self.view.window makeFirstResponder:self.metalView];
        
        NSPoint location = [self getMouseLocation:event];
        [self logMicroUIState:@"BEFORE mouseDown" location:location];
        // 座標の範囲チェック
        if (location.x < 0 || location.x >= self.metalView.bounds.size.width ||
            location.y < 0 || location.y >= self.metalView.bounds.size.height) {
            return; // 範囲外の場合は無視
        }
        
        // デバッグログ
//        char log_msg[128];
//        snprintf(log_msg, sizeof(log_msg), "Mouse Down: (%.1f, %.1f) View: (%.0f, %.0f)",
//                location.x, location.y,
//                self.metalView.bounds.size.width,
//                self.metalView.bounds.size.height);
//        write_log(log_msg);
        
        // MicroUIにマウス入力を伝達（OpenGL版と同じ順序）
        mu_input_mousemove(self.renderer.muiContext, location.x, location.y);
        mu_input_mousedown(self.renderer.muiContext, location.x, location.y, MU_MOUSE_LEFT);
        [self logMicroUIState:@"AFTER mouseDown" location:location];
        
        // フォーカス設定の詳細ログ
        NSLog(@"[Debug] Focus after mousedown: %d, last_id: %d", 
              self.renderer.muiContext->focus, self.renderer.muiContext->last_id);
        // 即時再描画を要求
        [self.metalView setNeedsDisplay:YES];
    }
    @catch (NSException *exception) {
        NSLog(@"Exception in mouseDown: %@", exception);
    }
}

- (void)mouseUp:(NSEvent *)event {
    @try {
        NSPoint location = [self getMouseLocation:event];
        
        // 座標の範囲チェック
        if (location.x < 0 || location.x >= self.metalView.bounds.size.width ||
            location.y < 0 || location.y >= self.metalView.bounds.size.height) {
            // 範囲外でも確実にマウスアップを送信（ドラッグ状態解除のため）
            location.x = fmax(0, fmin(location.x, self.metalView.bounds.size.width - 1));
            location.y = fmax(0, fmin(location.y, self.metalView.bounds.size.height - 1));
        }
        
        // デバッグログ
//        char log_msg[128];
//        snprintf(log_msg, sizeof(log_msg), "Mouse Up: (%.1f, %.1f)", location.x, location.y);
//        write_log(log_msg);
        
        // 全マウスボタンを明示的に解放（ドラッグ状態を確実に解除）
        mu_input_mousemove(self.renderer.muiContext, location.x, location.y);
        mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_LEFT);
        mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_RIGHT);
        mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_MIDDLE);
        
        // フォーカス設定の詳細ログ
        NSLog(@"[Debug] Focus after mouseup: %d, last_id: %d", 
              self.renderer.muiContext->focus, self.renderer.muiContext->last_id);
        
        // MicroUIの内部状態をリセット（ドラッグ状態を強制解除）
        self.renderer.muiContext->mouse_down = 0;
        // フォーカスはリセットしない - テキストボックスのフォーカスを維持するため
        // self.renderer.muiContext->focus = 0;  ← この行をコメントアウト
        // self.renderer.muiContext->hover = 0; ← このリセットを削除
        
        // マウスの現在位置を再送信してホバー状態を更新
        mu_input_mousemove(self.renderer.muiContext, location.x, location.y);
        
        // 即時再描画を要求
        [self.metalView setNeedsDisplay:YES];
    }
    @catch (NSException *exception) {
        NSLog(@"Exception in mouseUp: %@", exception);
    }
}
- (void)viewDidLayout {
    [super viewDidLayout];
    
    // サイズ変更時のトラッキングエリア重複を防ぐためのログ
    NSLog(@"[MicroUI] View layout changed, updating tracking area");
    
    // 既存のトラッキングエリアを削除
    for (NSTrackingArea *area in [self.metalView trackingAreas]) {
        [self.metalView removeTrackingArea:area];
    }
    
    // 新しいトラッキングエリアを追加
    NSTrackingArea *trackingArea = [[NSTrackingArea alloc]
        initWithRect:self.metalView.bounds
        options:(NSTrackingMouseEnteredAndExited | NSTrackingMouseMoved | NSTrackingActiveInKeyWindow)
        owner:self
        userInfo:nil];
    [self.metalView addTrackingArea:trackingArea];
    
    NSLog(@"[MicroUI] Tracking area updated with bounds: %.1fx%.1f", 
          self.metalView.bounds.size.width, self.metalView.bounds.size.height);
}
- (void)mouseMoved:(NSEvent *)event {
    NSPoint location = [self getMouseLocation:event];
    mu_input_mousemove(self.renderer.muiContext, location.x, location.y);
    
    // クリックなしでホバー状態を確実に更新
    self.renderer.muiContext->mouse_pressed = 0;
    
    // マウス移動でも再描画を要求
    [self.metalView setNeedsDisplay:YES];
    
    // // デバッグログ（ホバー状態確認用）
    // static int move_log_counter = 0;
    // if (++move_log_counter % 30 == 0) {  // 30回に1回ログ出力
    //     NSLog(@"[MicroUI Mouse] Move hover=%d, focus=%d",
    //           self.renderer.muiContext->hover,
    //           self.renderer.muiContext->focus);
    //     }
}

- (void)mouseDragged:(NSEvent *)event {
    NSPoint location = [self getMouseLocation:event];
    
    // 座標の範囲制限
    location.x = fmax(0, fmin(location.x, self.metalView.bounds.size.width - 1));
    location.y = fmax(0, fmin(location.y, self.metalView.bounds.size.height - 1));
    
    // ドラッグ時は明示的にマウス移動を送信
    mu_input_mousemove(self.renderer.muiContext, location.x, location.y);
    
    // 再描画を要求
    [self.metalView setNeedsDisplay:YES];
}

- (void)rightMouseDown:(NSEvent *)event {
    NSPoint location = [self getMouseLocation:event];
    mu_input_mousedown(self.renderer.muiContext, location.x, location.y, MU_MOUSE_RIGHT);
}

- (void)rightMouseUp:(NSEvent *)event {
    NSPoint location = [self getMouseLocation:event];
    mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_RIGHT);
}

- (void)scrollWheel:(NSEvent *)event {
    mu_input_scroll(self.renderer.muiContext, 0, [event scrollingDeltaY] * 10);
}

// マウス離脱時の処理を追加
- (void)mouseExited:(NSEvent *)event {
    // マウスがビュー外に出た時のログを詳細にする
    NSPoint location = [self getMouseLocation:event];
    NSLog(@"[MicroUI] Mouse exited at (%.1f, %.1f)", location.x, location.y);
    
    // すべてのマウスボタンを強制的に離す（フォーカスは維持）
    mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_LEFT);
    mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_RIGHT);
    mu_input_mouseup(self.renderer.muiContext, location.x, location.y, MU_MOUSE_MIDDLE);
    
    // MicroUIの内部状態をリセット（フォーカスは維持）
    self.renderer.muiContext->mouse_down = 0;
    // 注意：フォーカスはリセットしない（テキストボックスのフォーカスを維持）
    
    // 再描画を要求
    [self.metalView setNeedsDisplay:YES];
    
    write_log("Mouse exited - all buttons released");
}

// ウィンドウフォーカス変更時の処理
- (void)windowDidResignKey:(NSNotification *)notification {
    // ウィンドウがフォーカスを失った時、すべての入力状態をリセット
    mu_input_mouseup(self.renderer.muiContext, 0, 0, MU_MOUSE_LEFT);
    mu_input_mouseup(self.renderer.muiContext, 0, 0, MU_MOUSE_RIGHT);
    mu_input_mouseup(self.renderer.muiContext, 0, 0, MU_MOUSE_MIDDLE);
    
    // MicroUIの内部状態をリセット（マウス状態のみ）
    self.renderer.muiContext->mouse_down = 0;
    // テキストボックスのフォーカスはウィンドウフォーカス変更時も維持
    // self.renderer.muiContext->focus = 0;  ← コメントアウト
    self.renderer.muiContext->hover = 0;
    
    // 再描画を要求
    [self.metalView setNeedsDisplay:YES];
    
    write_log("Window lost focus - mouse state reset (focus preserved)");
}

- (void)handleKeyDown:(NSEvent *)event {
    NSLog(@"[MicroUI Keyboard] KeyDown: keyCode=%d, characters='%@'", 
          [event keyCode], [event characters]);
    
    NSString *characters = [event charactersIgnoringModifiers];  // 修飾キーを無視
    unsigned short keyCode = [event keyCode];
    
    // 修飾キーのチェック
    if ([event modifierFlags] & NSEventModifierFlagShift) {
        mu_input_keydown(self.renderer.muiContext, MU_KEY_SHIFT);
    }
    if ([event modifierFlags] & NSEventModifierFlagControl) {
        mu_input_keydown(self.renderer.muiContext, MU_KEY_CTRL);
    }
    if ([event modifierFlags] & NSEventModifierFlagOption) {
        mu_input_keydown(self.renderer.muiContext, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keydown(self.renderer.muiContext, MU_KEY_RETURN);
        NSLog(@"[MicroUI Key] Enter key pressed");
    } else if (keyCode == 51) { // Backspace
        mu_input_keydown(self.renderer.muiContext, MU_KEY_BACKSPACE);
        NSLog(@"[MicroUI Key] Backspace key pressed");
    } else if (keyCode == 53) { // Escape
        // Escapeキーで強制的に状態リセット
        mu_input_mouseup(self.renderer.muiContext, 0, 0, MU_MOUSE_LEFT);
        mu_input_mouseup(self.renderer.muiContext, 0, 0, MU_MOUSE_RIGHT);
        write_log("Escape pressed - input state reset");
        NSLog(@"[MicroUI Key] Escape key pressed - state reset");
    }
    
    // テキスト入力 - OpenGLバージョンと同じ方式
    mu_input_text(self.renderer.muiContext, [characters UTF8String]);
    NSLog(@"[MicroUI Text] Input text: '%@'", characters);
    
    // 再描画を要求
    [self.metalView setNeedsDisplay:YES];
}

- (void)handleKeyUp:(NSEvent *)event {
    unsigned short keyCode = [event keyCode];
    
    // 修飾キーのチェック
    if (!([event modifierFlags] & NSEventModifierFlagShift)) {
        mu_input_keyup(self.renderer.muiContext, MU_KEY_SHIFT);
    }
    if (!([event modifierFlags] & NSEventModifierFlagControl)) {
        mu_input_keyup(self.renderer.muiContext, MU_KEY_CTRL);
    }
    if (!([event modifierFlags] & NSEventModifierFlagOption)) {
        mu_input_keyup(self.renderer.muiContext, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keyup(self.renderer.muiContext, MU_KEY_RETURN);
    } else if (keyCode == 51) { // Backspace
        mu_input_keyup(self.renderer.muiContext, MU_KEY_BACKSPACE);
    }
}

@end

// main関数
int main(int argc, const char * argv[]) {
    return NSApplicationMain(argc, argv);
}
