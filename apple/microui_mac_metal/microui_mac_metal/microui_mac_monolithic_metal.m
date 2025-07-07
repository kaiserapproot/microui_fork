/*
 * MicroUI macOS Metal モノリシック実装 (2025年6月24日)
 * =====================================================
 * 
 * 【アーキテクチャ設計】
 * macOS OpenGL実装のモノリシックスタイルを参考にし、単一の大きなビュークラスで
 * すべての機能を管理するアプローチを採用。
 * 
 * 【特徴】
 * - NSViewのサブクラス（MicroUIMetalView）内ですべてを管理
 * - Metal レンダリング、MicroUI処理、イベント処理をすべて統合
 * - バッチ処理システムを標準実装
 * - CVDisplayLinkによる垂直同期対応
 * - CamelCase命名規則でApple開発慣習に準拠
 * 
 * 【参考実装】
 * - アーキテクチャ: microui_mac_opengl_main.m のモノリシックスタイル
 * - Metalレンダリング: microui_mac_metal_main.m の技術
 * - イベント処理: 両実装のベストプラクティスを統合
 * 
 * 【技術的改善】
 * - テキストボックスフォーカス問題の修正を事前適用
 * - 統一されたイベント処理順序 (mousemove → mousedown)
 * - 詳細なデバッグログとエラーハンドリング
 */

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#import <CoreVideo/CoreVideo.h>
#import <QuartzCore/QuartzCore.h>
#import <simd/simd.h>
#include "microui.h"
#include "atlas.inl"

// 定数定義
#define MAX_VERTICES 16384
#define GLES_SILENCE_DEPRECATION 1

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// Metal用頂点構造体
typedef struct {
    vector_float2 position;
    vector_float2 texCoord;
    vector_float4 color;
} MetalVertex;

// プロジェクション行列用のUniform構造体
typedef struct {
    matrix_float4x4 projectionMatrix;
} Uniforms;

/**
 * モノリシックMetal MicroUIビュークラス
 * 
 * OpenGL版のMicroUIMacViewと同様の設計思想で、NSViewのサブクラスとして
 * Metal レンダリング、MicroUI管理、イベント処理をすべて統合
 */
@interface MicroUIMetalView : NSView
{
    // Metal レンダリング関連
    id<MTLDevice> device;
    id<MTLCommandQueue> commandQueue;
    id<MTLRenderPipelineState> pipelineState;
    id<MTLBuffer> vertexBuffer;
    id<MTLBuffer> indexBuffer;
    id<MTLBuffer> uniformBuffer;
    id<MTLTexture> atlasTexture;
    id<MTLSamplerState> samplerState;
    
    // MetalKit関連
    MTKView *metalView;
    
    // ディスプレイ同期
    CVDisplayLinkRef displayLink;
    
    // MicroUI関連
    mu_Context* muContext;
    char logbuf[64000];
    int logbufUpdated;
    float bg[4];
    
    // ウィンドウ移動検出用
    int prevWindowX;
    int prevWindowY;
    
    // バッチレンダリング用
    MetalVertex vertices[MAX_VERTICES];
    uint16_t indices[MAX_VERTICES * 3 / 2];
    int vertexCount;
    int indexCount;
    
    // ビューサイズ
    int viewWidth;
    int viewHeight;
}

// 初期化とセットアップ
- (void)setupMetal;
- (void)setupRenderPipeline;
- (void)createAtlasTexture;
- (void)createBuffers;
- (void)setupDisplayLink;

// MicroUI関連
- (void)initMicroUI;
- (void)processMicroUIFrame;
- (void)drawTestWindow;
- (void)drawLogWindow;
- (void)drawStyleWindow;

// レンダリング
- (void)renderFrame;
- (void)drawMicroUI;
- (void)flushBatch;
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color;

// 描画プリミティブ
- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color;
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color;
- (void)drawIcon:(int)iconId rect:(mu_Rect)rect color:(mu_Color)color;
- (void)setClipRect:(mu_Rect)rect;

// イベント処理
- (NSPoint)normalizeMousePoint:(NSEvent *)event;

// ユーティリティ
- (void)writeLog:(const char*)text;
- (void)updateProjectionMatrix:(NSSize)size;

@end

// MicroUI コールバック関数
static int textWidth(mu_Font font, const char* text, int len) {
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

static int textHeight(mu_Font font) {
    return 18;
}

// uint8スライダーヘルパー関数
static int uint8Slider(mu_Context* ctx, unsigned char* value, int low, int high) {
    if (!ctx || !value) return 0;
    
    int res;
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = (unsigned char)tmp;
    mu_pop_id(ctx);
    return res;
}

// Metal シェーダー文字列
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
"struct VertexOut {\n"
"    float4 position [[position]];\n"
"    float2 texCoord;\n"
"    float4 color;\n"
"};\n"
"\n"
"fragment float4 fragment_main(VertexOut in [[stage_in]],\n"
"                            texture2d<half> colorTexture [[texture(0)]],\n"
"                            sampler textureSampler [[sampler(0)]]) {\n"
"    half4 colorSample = colorTexture.sample(textureSampler, in.texCoord);\n"
"    return float4(colorSample) * in.color;\n"
"}\n";

@implementation MicroUIMetalView

/**
 * DisplayLinkコールバック関数
 * OpenGL版と同様の垂直同期実装
 */
static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
                                  const CVTimeStamp* now,
                                  const CVTimeStamp* outputTime,
                                  CVOptionFlags flagsIn,
                                  CVOptionFlags* flagsOut,
                                  void* displayLinkContext)
{
    MicroUIMetalView* view = (__bridge MicroUIMetalView*)displayLinkContext;
    
    dispatch_async(dispatch_get_main_queue(), ^{
        [view renderFrame];
    });
    
    return kCVReturnSuccess;
}

/**
 * 初期化メソッド
 * OpenGL版のinitWithFrameと同様の構造
 */
- (instancetype)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    
    if (self) {
        [self setWantsLayer:YES];
        
        // MicroUI関連のデフォルト値設定
        bg[0] = 90.0f / 255.0f;
        bg[1] = 95.0f / 255.0f;
        bg[2] = 100.0f / 255.0f;
        bg[3] = 1.0f;
        logbuf[0] = '\0';
        logbufUpdated = 0;
        
        // ウィンドウ移動検出用
        prevWindowX = 40;
        prevWindowY = 40;
        
        // バッチレンダリング用の初期化
        vertexCount = 0;
        indexCount = 0;
        
        // マウストラッキングエリアを設定
        NSTrackingArea *trackingArea = [[NSTrackingArea alloc] 
            initWithRect:frameRect
                 options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
                   owner:self
                userInfo:nil];
        [self addTrackingArea:trackingArea];
        
        // Metal の初期化
        [self setupMetal];
        
        NSLog(@"MicroUIMetalView initialized with monolithic architecture");
    }
    
    return self;
}

/**
 * Metal の初期化
 */
- (void)setupMetal
{
    // デバイスの作成
    device = MTLCreateSystemDefaultDevice();
    if (!device) {
        NSLog(@"Metal is not supported on this device");
        return;
    }
    
    // コマンドキューの作成
    commandQueue = [device newCommandQueue];
    
    // MTKViewの作成と設定
    metalView = [[MTKView alloc] initWithFrame:self.bounds device:device];
    metalView.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
    metalView.clearColor = MTLClearColorMake(bg[0], bg[1], bg[2], bg[3]);
    metalView.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable;
    
    [self addSubview:metalView];
    
    // 初期ビューサイズを設定
    viewWidth = self.bounds.size.width;
    viewHeight = self.bounds.size.height;
    
    // レンダリングパイプラインの設定
    [self setupRenderPipeline];
    
    // アトラステクスチャの作成
    [self createAtlasTexture];
    
    // バッファの作成
    [self createBuffers];
    
    // サンプラーステートの作成
    MTLSamplerDescriptor *samplerDescriptor = [[MTLSamplerDescriptor alloc] init];
    samplerDescriptor.minFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.magFilter = MTLSamplerMinMagFilterNearest;
    samplerDescriptor.mipFilter = MTLSamplerMipFilterNotMipmapped;
    samplerDescriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerDescriptor.rAddressMode = MTLSamplerAddressModeClampToEdge;
    samplerState = [device newSamplerStateWithDescriptor:samplerDescriptor];
    
    // プロジェクション行列の初期化
    [self updateProjectionMatrix:self.bounds.size];
    
    // DisplayLinkの設定
    [self setupDisplayLink];
    
    // MicroUIの初期化
    [self initMicroUI];
    
    NSLog(@"Metal setup completed successfully");
}

/**
 * レンダリングパイプラインの設定
 */
- (void)setupRenderPipeline
{
    NSError *error = nil;
    
    // シェーダーライブラリの作成
    NSString *shaderSource = [NSString stringWithFormat:@"%@\n%@", vertexShaderSource, fragmentShaderSource];
    
    id<MTLLibrary> library = [device newLibraryWithSource:shaderSource
                                                   options:nil
                                                     error:&error];
    if (!library) {
        NSLog(@"Error creating shader library: %@", error);
        return;
    }
    
    id<MTLFunction> vertexFunction = [library newFunctionWithName:@"vertex_main"];
    id<MTLFunction> fragmentFunction = [library newFunctionWithName:@"fragment_main"];
    
    if (!vertexFunction || !fragmentFunction) {
        NSLog(@"Error loading shader functions");
        return;
    }
    
    // レンダーパイプライン記述子の作成
    MTLRenderPipelineDescriptor *pipelineDescriptor = [[MTLRenderPipelineDescriptor alloc] init];
    pipelineDescriptor.vertexFunction = vertexFunction;
    pipelineDescriptor.fragmentFunction = fragmentFunction;
    pipelineDescriptor.colorAttachments[0].pixelFormat = metalView.colorPixelFormat;
    
    // ブレンディングの設定
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
    vertexDescriptor.attributes[0].offset = offsetof(MetalVertex, position);
    vertexDescriptor.attributes[0].bufferIndex = 0;
    
    vertexDescriptor.attributes[1].format = MTLVertexFormatFloat2;
    vertexDescriptor.attributes[1].offset = offsetof(MetalVertex, texCoord);
    vertexDescriptor.attributes[1].bufferIndex = 0;
    
    vertexDescriptor.attributes[2].format = MTLVertexFormatFloat4;
    vertexDescriptor.attributes[2].offset = offsetof(MetalVertex, color);
    vertexDescriptor.attributes[2].bufferIndex = 0;
    
    vertexDescriptor.layouts[0].stride = sizeof(MetalVertex);
    vertexDescriptor.layouts[0].stepRate = 1;
    vertexDescriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
    
    pipelineDescriptor.vertexDescriptor = vertexDescriptor;
    
    // パイプラインステートの作成
    pipelineState = [device newRenderPipelineStateWithDescriptor:pipelineDescriptor error:&error];
    if (!pipelineState) {
        NSLog(@"Error creating pipeline state: %@", error);
        return;
    }
    
    NSLog(@"Render pipeline setup completed");
}

/**
 * アトラステクスチャの作成
 */
- (void)createAtlasTexture
{
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
    textureDescriptor.width = ATLAS_WIDTH;
    textureDescriptor.height = ATLAS_HEIGHT;
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    atlasTexture = [device newTextureWithDescriptor:textureDescriptor];
    
    // A8フォーマットからRGBA8へ変換
    uint32_t *rgbaData = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        uint8_t alpha = atlas_texture[i];
        rgbaData[i] = (alpha << 24) | 0x00FFFFFF; // RGB=白、Aはアトラスデータ
    }
    
    [atlasTexture replaceRegion:MTLRegionMake2D(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT)
                    mipmapLevel:0
                      withBytes:rgbaData
                    bytesPerRow:ATLAS_WIDTH * 4];
    
    free(rgbaData);
    
    NSLog(@"Atlas texture created successfully (%dx%d)", ATLAS_WIDTH, ATLAS_HEIGHT);
}

/**
 * バッファの作成
 */
- (void)createBuffers
{
    // 頂点バッファ
    vertexBuffer = [device newBufferWithLength:sizeof(MetalVertex) * MAX_VERTICES
                                       options:MTLResourceStorageModeShared];
    
    // インデックスバッファ
    indexBuffer = [device newBufferWithLength:sizeof(uint16_t) * MAX_VERTICES * 3 / 2
                                      options:MTLResourceStorageModeShared];
    
    // Uniformバッファ
    uniformBuffer = [device newBufferWithLength:sizeof(Uniforms)
                                        options:MTLResourceStorageModeShared];
    
    NSLog(@"Metal buffers created successfully");
}

/**
 * DisplayLinkの設定
 * OpenGL版と同様の実装
 */
- (void)setupDisplayLink
{
    CVReturn result = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    if (result != kCVReturnSuccess) {
        NSLog(@"Failed to create DisplayLink: %d", result);
        return;
    }
    
    result = CVDisplayLinkSetOutputCallback(displayLink, &DisplayLinkCallback, (__bridge void*)self);
    if (result != kCVReturnSuccess) {
        NSLog(@"Failed to set DisplayLink callback: %d", result);
        return;
    }
    
    NSLog(@"DisplayLink setup completed");
}

/**
 * MicroUIの初期化
 */
- (void)initMicroUI
{
    muContext = malloc(sizeof(mu_Context));
    if (!muContext) {
        NSLog(@"Failed to allocate MicroUI context");
        return;
    }
    
    memset(muContext, 0, sizeof(mu_Context));
    mu_init(muContext);
    muContext->text_width = textWidth;
    muContext->text_height = textHeight;
    
    [self writeLog:"MicroUI Metal monolithic implementation initialized"];
    logbufUpdated = 1;
    
    NSLog(@"MicroUI initialization completed");
}

/**
 * ビューがウィンドウに追加された時の処理
 * OpenGL版と同様の構造
 */
- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    if (self.window) {
        if (displayLink && !CVDisplayLinkIsRunning(displayLink)) {
            CVReturn result = CVDisplayLinkStart(displayLink);
            if (result == kCVReturnSuccess) {
                NSLog(@"DisplayLink started in viewDidMoveToWindow");
            } else {
                NSLog(@"Failed to start DisplayLink: %d", result);
            }
        }
    }
}

/**
 * ビューがウィンドウから削除される時の処理
 */
- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [super viewWillMoveToWindow:newWindow];
    
    if (newWindow == nil && displayLink && CVDisplayLinkIsRunning(displayLink)) {
        CVDisplayLinkStop(displayLink);
    }
}

/**
 * ビューサイズ変更時の処理
 */
- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    
    viewWidth = newSize.width;
    viewHeight = newSize.height;
    
    [self updateProjectionMatrix:newSize];
}

/**
 * プロジェクション行列の更新
 */
- (void)updateProjectionMatrix:(NSSize)size
{
    Uniforms *uniforms = (Uniforms *)uniformBuffer.contents;
    
    // 正射影行列の作成 (Metal座標系用)
    float left = 0.0f;
    float right = size.width;
    float bottom = size.height;  // Metalでは下が原点
    float top = 0.0f;
    float near = -1.0f;
    float far = 1.0f;
    
    uniforms->projectionMatrix = (matrix_float4x4) {{
        { 2.0f / (right - left), 0.0f, 0.0f, 0.0f },
        { 0.0f, 2.0f / (top - bottom), 0.0f, 0.0f },
        { 0.0f, 0.0f, -2.0f / (far - near), 0.0f },
        { -(right + left) / (right - left), -(top + bottom) / (top - bottom), -(far + near) / (far - near), 1.0f }
    }};
}

/**
 * メインレンダリング関数
 */
- (void)renderFrame
{
    if (!self.window || ![self.window isVisible] || self.hidden) {
        return;
    }
    
    // ビューサイズの更新
    NSRect bounds = [self bounds];
    viewWidth = bounds.size.width;
    viewHeight = bounds.size.height;
    
    // Retinaディスプレイ対応
    if ([self.window respondsToSelector:@selector(backingScaleFactor)]) {
        CGFloat scaleFactor = [self.window backingScaleFactor];
        viewWidth *= scaleFactor;
        viewHeight *= scaleFactor;
    }
    
    [self updateProjectionMatrix:NSMakeSize(viewWidth, viewHeight)];
    
    // MicroUIフレーム処理
    [self processMicroUIFrame];
    
    // Metal描画
    id<MTLCommandBuffer> commandBuffer = [commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = metalView.currentRenderPassDescriptor;
    
    if (renderPassDescriptor != nil) {
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setRenderPipelineState:pipelineState];
        
        [self drawMicroUI];
        
        if (indexCount > 0) {
            // バッファにデータをコピー
            memcpy(vertexBuffer.contents, vertices, vertexCount * sizeof(MetalVertex));
            memcpy(indexBuffer.contents, indices, indexCount * sizeof(uint16_t));
            
            [renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
            [renderEncoder setVertexBuffer:uniformBuffer offset:0 atIndex:1];
            [renderEncoder setFragmentTexture:atlasTexture atIndex:0];
            [renderEncoder setFragmentSamplerState:samplerState atIndex:0];
            
            [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                     indexCount:indexCount
                                      indexType:MTLIndexTypeUInt16
                                    indexBuffer:indexBuffer
                              indexBufferOffset:0];
        }
        
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:metalView.currentDrawable];
    }
    
    [commandBuffer commit];
}

/**
 * MicroUIフレームの処理
 * OpenGL版と同様の構造
 */
- (void)processMicroUIFrame
{
    mu_begin(muContext);
    
    [self drawTestWindow];
    [self drawLogWindow];
    [self drawStyleWindow];
    
    mu_end(muContext);
}

/**
 * テストウィンドウの描画
 * OpenGL版と同様の内容
 */
- (void)drawTestWindow
{
    if (mu_begin_window(muContext, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container* win;
        char buf[64];
        int layout1[2] = { 54, -1 };
        int layout2[3] = { 86, -110, -1 };
        int layout3[2] = { 140, -1 };
        int layout4[2] = { 54, 54 };
        int layout5[1] = { -1 };
        int layout6[2] = { -78, -1 };
        int layout7[2] = { 46, -1 };
        mu_Rect r;

        win = mu_get_current_container(muContext);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        // ウィンドウ位置の変更を検出してログ出力
        if (win->rect.x != prevWindowX || win->rect.y != prevWindowY) {
            NSLog(@"[Metal] Window moved to x=%d, y=%d (prev: x=%d, y=%d)", 
                  win->rect.x, win->rect.y, prevWindowX, prevWindowY);
            
            char logMessage[128];
            sprintf(logMessage, "Metal: Window moved to x=%d, y=%d", win->rect.x, win->rect.y);
            [self writeLog:logMessage];
            
            prevWindowX = win->rect.x;
            prevWindowY = win->rect.y;
        }

        // ウィンドウ情報
        if (mu_header(muContext, "Window Info")) {
            win = mu_get_current_container(muContext);
            mu_layout_row(muContext, 2, layout1, 0);
            mu_label(muContext, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(muContext, buf);
            mu_label(muContext, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(muContext, buf);
        }

        // ラベルとボタン
        if (mu_header_ex(muContext, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(muContext, 3, layout2, 0);
            mu_label(muContext, "Test buttons 1:");
            if (mu_button(muContext, "Button 1")) { [self writeLog:"Metal: Pressed button 1"]; }
            if (mu_button(muContext, "Button 2")) { [self writeLog:"Metal: Pressed button 2"]; }
            mu_label(muContext, "Test buttons 2:");
            if (mu_button(muContext, "Button 3")) { [self writeLog:"Metal: Pressed button 3"]; }
            if (mu_button(muContext, "Popup")) { mu_open_popup(muContext, "Test Popup"); }
            if (mu_begin_popup(muContext, "Test Popup")) {
                mu_button(muContext, "Hello");
                mu_button(muContext, "World");
                mu_end_popup(muContext);
            }
        }

        // ツリーとテキスト
        if (mu_header_ex(muContext, "Tree and Text", MU_OPT_EXPANDED)) {
            mu_layout_row(muContext, 2, layout3, 0);
            mu_layout_begin_column(muContext);
            if (mu_begin_treenode(muContext, "Test 1")) {
                if (mu_begin_treenode(muContext, "Test 1a")) {
                    mu_label(muContext, "Hello");
                    mu_label(muContext, "world");
                    mu_end_treenode(muContext);
                }
                if (mu_begin_treenode(muContext, "Test 1b")) {
                    if (mu_button(muContext, "Button 1")) { [self writeLog:"Metal: Pressed button 1"]; }
                    if (mu_button(muContext, "Button 2")) { [self writeLog:"Metal: Pressed button 2"]; }
                    mu_end_treenode(muContext);
                }
                mu_end_treenode(muContext);
            }
            if (mu_begin_treenode(muContext, "Test 2")) {
                mu_layout_row(muContext, 2, layout4, 0);
                if (mu_button(muContext, "Button 3")) { [self writeLog:"Metal: Pressed button 3"]; }
                if (mu_button(muContext, "Button 4")) { [self writeLog:"Metal: Pressed button 4"]; }
                if (mu_button(muContext, "Button 5")) { [self writeLog:"Metal: Pressed button 5"]; }
                if (mu_button(muContext, "Button 6")) { [self writeLog:"Metal: Pressed button 6"]; }
                mu_end_treenode(muContext);
            }
            if (mu_begin_treenode(muContext, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(muContext, "Checkbox 1", &checks[0]);
                mu_checkbox(muContext, "Checkbox 2", &checks[1]);
                mu_checkbox(muContext, "Checkbox 3", &checks[2]);
                mu_end_treenode(muContext);
            }
            mu_layout_end_column(muContext);

            mu_layout_begin_column(muContext);
            mu_layout_row(muContext, 1, layout5, 0);
            mu_text(muContext, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(muContext);
        }

        // 背景色スライダー
        if (mu_header_ex(muContext, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(muContext, 2, layout6, 74);
            mu_layout_begin_column(muContext);
            mu_layout_row(muContext, 2, layout7, 0);
            static unsigned char bgColor[3] = { 90, 95, 100 };
            mu_label(muContext, "Red:"); uint8Slider(muContext, &bgColor[0], 0, 255);
            mu_label(muContext, "Green:"); uint8Slider(muContext, &bgColor[1], 0, 255);
            mu_label(muContext, "Blue:"); uint8Slider(muContext, &bgColor[2], 0, 255);
            
            // 背景色の更新
            bg[0] = bgColor[0] / 255.0f;
            bg[1] = bgColor[1] / 255.0f;
            bg[2] = bgColor[2] / 255.0f;
            metalView.clearColor = MTLClearColorMake(bg[0], bg[1], bg[2], bg[3]);
            
            mu_layout_end_column(muContext);
            
            // カラープレビュー
            r = mu_layout_next(muContext);
            mu_draw_rect(muContext, r, mu_color(bgColor[0], bgColor[1], bgColor[2], 255));
            sprintf(buf, "#%02X%02X%02X", (int)bgColor[0], (int)bgColor[1], (int)bgColor[2]);
            mu_draw_control_text(muContext, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(muContext);
    }
}

/**
 * ログウィンドウの描画
 */
- (void)drawLogWindow
{
    if (mu_begin_window(muContext, "Log Window", mu_rect(350, 40, 300, 200))) {
        mu_Container* panel;
        static char buf[128];
        int submitted = 0;
        int layout1[1] = { -1 };
        int layout2[2] = { -70, -1 };

        mu_layout_row(muContext, 1, layout1, -25);
        mu_begin_panel(muContext, "Log Output");
        panel = mu_get_current_container(muContext);
        mu_layout_row(muContext, 1, layout1, -1);
        mu_text(muContext, logbuf);
        mu_end_panel(muContext);
        if (logbufUpdated) {
            panel->scroll.y = panel->content_size.y;
            logbufUpdated = 0;
        }

        mu_layout_row(muContext, 2, layout2, 0);
        if (mu_textbox(muContext, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(muContext, muContext->last_id);
            submitted = 1;
        }
        if (mu_button(muContext, "Submit")) { submitted = 1; }
        if (submitted) {
            [self writeLog:buf];
            buf[0] = '\0';
        }

        mu_end_window(muContext);
    }
}

/**
 * スタイルエディタウィンドウの描画
 */
- (void)drawStyleWindow
{
    static struct { const char* label; int idx; } colors[] = {
        { "text:",         MU_COLOR_TEXT        },
        { "border:",       MU_COLOR_BORDER      },
        { "windowbg:",     MU_COLOR_WINDOWBG    },
        { "titlebg:",      MU_COLOR_TITLEBG     },
        { "titletext:",    MU_COLOR_TITLETEXT   },
        { "panelbg:",      MU_COLOR_PANELBG     },
        { "button:",       MU_COLOR_BUTTON      },
        { "buttonhover:",  MU_COLOR_BUTTONHOVER },
        { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
        { "base:",         MU_COLOR_BASE        },
        { "basehover:",    MU_COLOR_BASEHOVER   },
        { "basefocus:",    MU_COLOR_BASEFOCUS   },
        { "scrollbase:",   MU_COLOR_SCROLLBASE  },
        { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
        { NULL }
    };

    if (mu_begin_window(muContext, "Style Editor", mu_rect(350, 250, 300, 240))) {
        int sw = mu_get_current_container(muContext)->body.w * 0.14;
        int layout[6] = { 80, sw, sw, sw, sw, -1 };
        mu_layout_row(muContext, 6, layout, 0);
        
        for (int i = 0; colors[i].label; i++) {
            mu_label(muContext, colors[i].label);
            uint8Slider(muContext, &muContext->style->colors[colors[i].idx].r, 0, 255);
            uint8Slider(muContext, &muContext->style->colors[colors[i].idx].g, 0, 255);
            uint8Slider(muContext, &muContext->style->colors[colors[i].idx].b, 0, 255);
            uint8Slider(muContext, &muContext->style->colors[colors[i].idx].a, 0, 255);
            mu_draw_rect(muContext, mu_layout_next(muContext), muContext->style->colors[colors[i].idx]);
        }
        
        mu_end_window(muContext);
    }
}

/**
 * MicroUI描画関数
 * MicroUIコマンドを処理してMetalで描画
 */
- (void)drawMicroUI
{
    if (!muContext) return;
    
    // バッチカウンターをリセット
    vertexCount = 0;
    indexCount = 0;
    
    // MicroUIコマンドの処理
    mu_Command* cmd = NULL;
    while (mu_next_command(muContext, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT:
                [self drawText:cmd->text.str pos:cmd->text.pos color:cmd->text.color];
                break;
            case MU_COMMAND_RECT:
                [self drawRect:cmd->rect.rect color:cmd->rect.color];
                break;
            case MU_COMMAND_ICON:
                [self drawIcon:cmd->icon.id rect:cmd->icon.rect color:cmd->icon.color];
                break;
            case MU_COMMAND_CLIP:
                [self setClipRect:cmd->clip.rect];
                break;
        }
    }
}

/**
 * バッチにクアッドを追加
 */
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color
{
    if (vertexCount >= MAX_VERTICES - 4 || indexCount >= MAX_VERTICES * 3 / 2 - 6) {
        NSLog(@"Warning: Vertex buffer full, skipping quad");
        return;
    }
    
    // テクスチャ座標を計算
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
    
    // 4つの頂点を追加（左上、右上、右下、左下）
    vertices[vertexCount + 0] = (MetalVertex){
        .position = { dst.x, dst.y },
        .texCoord = { x, y },
        .color = vertexColor
    };
    
    vertices[vertexCount + 1] = (MetalVertex){
        .position = { dst.x + dst.w, dst.y },
        .texCoord = { x + w, y },
        .color = vertexColor
    };
    
    vertices[vertexCount + 2] = (MetalVertex){
        .position = { dst.x + dst.w, dst.y + dst.h },
        .texCoord = { x + w, y + h },
        .color = vertexColor
    };
    
    vertices[vertexCount + 3] = (MetalVertex){
        .position = { dst.x, dst.y + dst.h },
        .texCoord = { x, y + h },
        .color = vertexColor
    };
    
    // インデックスを追加（2つの三角形でクアッドを作成）
    indices[indexCount + 0] = vertexCount + 0;
    indices[indexCount + 1] = vertexCount + 1;
    indices[indexCount + 2] = vertexCount + 2;
    indices[indexCount + 3] = vertexCount + 2;
    indices[indexCount + 4] = vertexCount + 3;
    indices[indexCount + 5] = vertexCount + 0;
    
    vertexCount += 4;
    indexCount += 6;
}

/**
 * テキストの描画
 */
- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color
{
    if (!text || !text[0]) return;
    
    const char* p;
    unsigned char chr;
    mu_Rect src, dst = { pos.x, pos.y, 0, 0 };
    
    int charCount = 0;
    int totalChars = strlen(text);
    
    for (p = text; *p; p++, charCount++) {
        chr = (unsigned char)*p;
        
        if (chr < 32 || chr >= 127) {
            continue;
        }
        
        int idx = ATLAS_FONT + chr;
        src = atlas[idx];
        dst.w = src.w;
        dst.h = src.h;
        
        if (chr == ' ') {
            dst.x += dst.w;
            if (charCount < totalChars - 1) {
                dst.x += 1;
            }
            continue;
        }
        
        [self pushQuad:dst src:src color:color];
        
        dst.x += dst.w;
        if (charCount < totalChars - 1) {
            dst.x += 1;
        }
    }
}

/**
 * 矩形の描画
 */
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color
{
    mu_Rect src = {0, 0, 1, 1}; // ダミーのテクスチャ座標
    [self pushQuad:rect src:src color:color];
}

/**
 * アイコンの描画
 */
- (void)drawIcon:(int)iconId rect:(mu_Rect)rect color:(mu_Color)color
{
    if (iconId < 0 || iconId >= sizeof(atlas) / sizeof(atlas[0])) return;
    
    mu_Rect src = atlas[iconId];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    mu_Rect dst = { x, y, src.w, src.h };
    
    [self pushQuad:dst src:src color:color];
}

/**
 * クリップ矩形の設定
 * Metal実装では現在スキップ（将来的にシザーテストで実装可能）
 */
- (void)setClipRect:(mu_Rect)rect
{
    // Metal実装では現在シザーテストは実装していない
    // 将来的にMTLRenderCommandEncoderのsetScissorRectで実装可能
}

/**
 * ログ出力関数
 */
- (void)writeLog:(const char*)text
{
    if (!text) return;
    
    NSLog(@"[MicroUI Metal] %s", text);
    
    size_t textLen = strlen(text);
    size_t currentLen = strlen(logbuf);
    
    if (currentLen + textLen + 2 >= sizeof(logbuf)) {
        memmove(logbuf, logbuf + 1000, currentLen - 1000);
        logbuf[currentLen - 1000] = '\0';
        currentLen = strlen(logbuf);
    }
    
    if (logbuf[0] != '\0') {
        strcat(logbuf, "\n");
    }
    
    strncat(logbuf, text, sizeof(logbuf) - strlen(logbuf) - 1);
    logbufUpdated = 1;
}

/**
 * マウスイベント処理
 * テキストボックスフォーカス問題の修正を事前適用
 */
- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    
    int focusBefore = muContext->focus;
    
    // OpenGL版と同じ順序: mousemove → mousedown
    mu_input_mousemove(muContext, (int)point.x, (int)point.y);
    mu_input_mousedown(muContext, (int)point.x, (int)point.y, MU_MOUSE_LEFT);
    
    NSLog(@"[Metal] mouseDown at (%.1f, %.1f), focus before: %d, after: %d", 
          point.x, point.y, focusBefore, muContext->focus);
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    
    int focusBefore = muContext->focus;
    
    // OpenGL版と同じ順序: mousemove → mouseup
    mu_input_mousemove(muContext, (int)point.x, (int)point.y);
    mu_input_mouseup(muContext, (int)point.x, (int)point.y, MU_MOUSE_LEFT);
    
    NSLog(@"[Metal] mouseUp at (%.1f, %.1f), focus before: %d, after: %d", 
          point.x, point.y, focusBefore, muContext->focus);
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    mu_input_mousemove(muContext, (int)point.x, (int)point.y);
}

- (void)mouseDragged:(NSEvent *)event
{
    [self mouseMoved:event];
}

/**
 * キーボードイベント処理
 */
- (void)keyDown:(NSEvent *)event
{
    NSString *characters = [event charactersIgnoringModifiers];
    unsigned short keyCode = [event keyCode];
    
    // 修飾キーのチェック
    if ([event modifierFlags] & NSEventModifierFlagShift) {
        mu_input_keydown(muContext, MU_KEY_SHIFT);
    }
    if ([event modifierFlags] & NSEventModifierFlagControl) {
        mu_input_keydown(muContext, MU_KEY_CTRL);
    }
    if ([event modifierFlags] & NSEventModifierFlagOption) {
        mu_input_keydown(muContext, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keydown(muContext, MU_KEY_RETURN);
    } else if (keyCode == 51) { // Backspace
        mu_input_keydown(muContext, MU_KEY_BACKSPACE);
    }
    
    // テキスト入力
    mu_input_text(muContext, [characters UTF8String]);
}

- (void)keyUp:(NSEvent *)event
{
    unsigned short keyCode = [event keyCode];
    
    // 修飾キーのチェック
    if (!([event modifierFlags] & NSEventModifierFlagShift)) {
        mu_input_keyup(muContext, MU_KEY_SHIFT);
    }
    if (!([event modifierFlags] & NSEventModifierFlagControl)) {
        mu_input_keyup(muContext, MU_KEY_CTRL);
    }
    if (!([event modifierFlags] & NSEventModifierFlagOption)) {
        mu_input_keyup(muContext, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keyup(muContext, MU_KEY_RETURN);
    } else if (keyCode == 51) { // Backspace
        mu_input_keyup(muContext, MU_KEY_BACKSPACE);
    }
}

/**
 * ファーストレスポンダーの設定
 */
- (BOOL)acceptsFirstResponder
{
    return YES;
}

/**
 * マウス座標の正規化
 */
- (NSPoint)normalizeMousePoint:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect bounds = [self bounds];
    
    // Retinaディスプレイ対応
    CGFloat scaleFactor = 1.0;
    if ([self.window respondsToSelector:@selector(backingScaleFactor)]) {
        scaleFactor = [self.window backingScaleFactor];
    }
    
    point.x *= scaleFactor;
    point.y *= scaleFactor;
    
    // Metal座標系では上が原点なので、Y座標の反転は不要
    // ただし、MicroUIは左上原点なので、適切な座標変換を行う
    CGFloat viewHeight = bounds.size.height * scaleFactor;
    point.y = viewHeight - point.y;
    
    return point;
}

/**
 * デアロケータ
 */
- (void)dealloc
{
    // DisplayLinkの停止と解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    
    // MicroUIコンテキストの解放
    if (muContext) {
        free(muContext);
    }
    
    NSLog(@"MicroUIMetalView deallocated");
}

@end

#pragma mark - アプリケーションデリゲート

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) MicroUIMetalView *metalView;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification
{
    // ウィンドウの作成
    NSRect windowRect = NSMakeRect(100, 100, 800, 600);
    NSWindowStyleMask style = NSWindowStyleMaskTitled |
                             NSWindowStyleMaskClosable |
                             NSWindowStyleMaskMiniaturizable |
                             NSWindowStyleMaskResizable;
    
    self.window = [[NSWindow alloc] initWithContentRect:windowRect
                                            styleMask:style
                                              backing:NSBackingStoreBuffered
                                                defer:NO];
    
    [self.window setTitle:@"MicroUI Metal Monolithic Demo"];
    [self.window setMinSize:NSMakeSize(200, 200)];
    
    // Metalビューの作成
    self.metalView = [[MicroUIMetalView alloc] initWithFrame:[[self.window contentView] bounds]];
    [self.metalView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    // ビューをウィンドウに追加
    [self.window setContentView:self.metalView];
    
    // ウィンドウの表示
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
    
    NSLog(@"MicroUI Metal Monolithic application launched successfully");
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

@end

#pragma mark - メイン関数

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *application = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [application setDelegate:delegate];
        [application run];
    }
    return 0;
}

#pragma clang diagnostic pop
