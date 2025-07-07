// MetalMicroUI.h
#ifndef MetalMicroUI_h
#define MetalMicroUI_h

#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#include "microui.h"
#include "atlas.inl"  // atlas.inlをインクルード


#include "microui_share.h"  // microui_share.hをインクルード
static float bg[4] = { 90, 95, 100, 105 };

// 頂点構造体
typedef struct {
    vector_float2 position;
    vector_float2 texCoord;
    vector_float4 color;
} MetalVertex;

 void ios_clear_slider_cache(void) { sliderCacheCount = 0; }
 void ios_clear_textbox_cache(void) { textboxCacheCount = 0; }



 mu_Id ios_find_slider_at_point(int x, int y) {
    for (int i = 0; i < sliderCacheCount; i++) {
        mu_Rect r = sliderCache[i].rect;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            return sliderCache[i].id;
        }
    }
    return 0;
}
 mu_Id ios_find_textbox_at_point(int x, int y) {
    for (int i = 0; i < textboxCacheCount; i++) {
        mu_Rect r = textboxCache[i].rect;
        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h) {
            return textboxCache[i].id;
        }
    }
    return 0;
}


// metal_rendererクラス (スネークケース命名規則に合わせる)
@interface metal_renderer : NSObject <MTKViewDelegate>
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


@interface micro_ui_view_controller : UIViewController <UIKeyInput, UITextFieldDelegate>
@property (nonatomic, strong) MTKView *metalView;
@property (nonatomic, strong) metal_renderer *renderer;
@property (nonatomic, strong) CADisplayLink *displayLink;
@property (nonatomic, strong) NSMutableArray *pendingEvents;
@property (nonatomic, strong) NSLock *eventLock;
@property (nonatomic, assign) BOOL needsRedraw;

// --- 不要・重複プロパティ削除 ---
 @property (nonatomic, assign) CGPoint lastHeaderTapPosition; // 使用箇所あり→残す
 @property (nonatomic, assign) BOOL didTapHeader; // 使用箇所あり→残す

// ウィンドウドラッグ関連
@property (nonatomic, assign) CGPoint dragStartTouchPosition;
@property (nonatomic, assign) int dragStartWindowX;
@property (nonatomic, assign) int dragStartWindowY;
@property (nonatomic, assign) BOOL isDraggingWindow;


// @property (nonatomic, strong) NSString *draggingWindowName; // 使っていないので削除
@property (nonatomic, assign) CGPoint dragOffset;
@property (nonatomic, strong) UITextField *hiddenTextField;
@property (nonatomic, assign) char *activeTextBoxBuffer;
@property (nonatomic, assign) int activeTextBoxBufferSize;
- (void)focusTextBox:(char *)buf size:(int)size;
@property (nonatomic, assign) mu_Container *draggingContainer;
- (void)queueEvent:(NSDictionary *)event;
- (mu_Id)calculateTitleId:(mu_Context *)ctx windowName:(const char *)name;
@end


#endif /* MetalMicroUI_h */

// MetalMicroUI.m
#import <simd/simd.h>

// --- mu_textbox用グローバルバッファ定義 ---
char input_buf[128] = {0};


static mu_Id lastHoverId = 0;

/**
 * iOS用のホバー状態管理関数 - タッチデバイスでホバー状態を適切に扱うための最適化版
 * - マウス操作では自動的にcursor位置でのホバーが検出されるが、タッチではそれができない
 * - この関数はタッチでも適切なホバー状態を維持するよう補助する
 */
 void ios_fix_hover_state(mu_Context* ctx) {
    if (!ctx) return;
    
    // 異常な値のチェックと修正（負の値は無効）
    if (ctx->hover < 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
        return;
    }
    
    // 状態が変わっていない場合は処理スキップ（パフォーマンス向上）
    if (ctx->hover == lastHoverId) {
        return;
    }
    
    // 新しいホバーIDを記録
    lastHoverId = ctx->hover;
    
    // タッチ中（mouse_downがON）の場合はフォーカス要素とホバー要素を同期
    if (ctx->mouse_down && ctx->focus != 0 && ctx->hover != ctx->focus) {
        ctx->hover = ctx->focus;
        ctx->prev_hover = ctx->focus;
    }
    // タッチが終了した場合（mouse_downがOFF）はホバー状態をクリア
    else if (!ctx->mouse_down && ctx->hover != 0 && ctx->mouse_pressed == 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
    }
}

/**
 * 安全なホバー状態設定関数 - MicroUIのcore機能を拡張
 * @param ctx MicroUIコンテキスト
 * @param id 設定するホバーID（0の場合はクリア）
 */
static void ios_set_hover(mu_Context* ctx, mu_Id id) {
    if (!ctx) return;
    
    // 現在のホバー状態と同じ場合は何もしない（無駄なログ出力を防止）
    if (ctx->hover == id) return;
    
    if (id == 0) {
        // ホバー状態をクリア
        ctx->hover = 0;
        ctx->prev_hover = 0;
        

    } else {
        // ホバー状態を設定
        ctx->hover = id;
        ctx->prev_hover = id;
        

    }
}

// プルダウン（ヘッダー）の状態を保持するための構造体
typedef struct {
    mu_Id id;
    char label[64];
    BOOL expanded;
} PersistentHeaderState;

// 永続的なヘッダー状態を保持する配列
static PersistentHeaderState persistentHeaders[50];
static int persistentHeaderCount = 0;

// 永続的なヘッダー状態を初期化
static void ios_init_header_states(void) {
    persistentHeaderCount = 0;
    memset(persistentHeaders, 0, sizeof(persistentHeaders));
    write_log("永続的ヘッダー状態管理を初期化しました");
}

// 永続的なヘッダー状態を設定する
static void ios_set_header_state(mu_Id id, const char* label, BOOL expanded) {
    if (id == 0) return;
    
    // 既存のエントリを検索
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            persistentHeaders[i].expanded = expanded;
            return;
        }
    }
    
    // 新しいエントリを追加
    if (persistentHeaderCount < 50) {
        persistentHeaders[persistentHeaderCount].id = id;
        if (label) {
            strncpy(persistentHeaders[persistentHeaderCount].label, label, 63);
            persistentHeaders[persistentHeaderCount].label[63] = '\0';
        }
        persistentHeaders[persistentHeaderCount].expanded = expanded;
        persistentHeaderCount++;
    }
}

// 永続的なヘッダー状態を取得する
static BOOL ios_get_header_state(mu_Id id, BOOL* outExpanded) {
    if (id == 0 || !outExpanded) return NO;
    
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            *outExpanded = persistentHeaders[i].expanded;
            return YES;
        }
    }
    
    *outExpanded = NO;
    return NO;
}

// 永続的なヘッダー状態を切り替える
static void ios_toggle_persistent_header_state(mu_Id id, const char* label) {
    if (id == 0) return;
    
    BOOL currentState = NO;
    BOOL found = ios_get_header_state(id, &currentState);
    
    ios_set_header_state(id, label, !currentState);
    

}

// MicroUI用のヘッダー矩形キャッシュ（描画中にヘッダー矩形情報を保存）
typedef struct {
    mu_Id id;
    mu_Rect rect;
    char label[64];
    BOOL active;
} HeaderCache;

static HeaderCache headerCache[20];  // 最大20個のヘッダーをキャッシュ
static int headerCacheCount = 0;

// ヘッダー情報をキャッシュに保存する
static void ios_cache_header(mu_Context* ctx, const char* label, mu_Rect rect, BOOL active) {
    if (headerCacheCount < 20 && label) {
        mu_Id id = mu_get_id(ctx, label, strlen(label));
        HeaderCache* cache = &headerCache[headerCacheCount++];
        cache->id = id;
        cache->rect = rect;
        strncpy(cache->label, label, 63);
        cache->label[63] = '\0';
        cache->active = active;
    }
}

// キャッシュ情報をクリア（フレーム開始時に呼び出す）
static void ios_clear_header_cache() {
    headerCacheCount = 0;
}


// 直接ヘッダーの状態を切り替える（MicroUIの内部処理をエミュレート）
 void ios_toggle_header_state(mu_Context* ctx, mu_Id id) {
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    
    if (idx >= 0) {
        // ヘッダーが展開されている場合、折りたたむ
        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
    } else {
        // ヘッダーが折りたたまれている場合、展開する
        mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    }
}



// 指定座標にあるヘッダー情報を取得する統合関数
static BOOL ios_get_header_at_position(mu_Context* ctx, CGPoint position, mu_Id* outId, const char** outLabel, BOOL* outActive) {
    // 1. HeaderCacheを優先的に検索
    for (int i = 0; i < headerCacheCount; ++i) {
        HeaderCache *cache = &headerCache[i];
        mu_Rect r = cache->rect;
        if (position.x >= r.x && position.x < r.x + r.w &&
            position.y >= r.y && position.y < r.y + r.h) {
            if (outId) *outId = cache->id;
            if (outLabel) *outLabel = cache->label;
            if (outActive) *outActive = cache->active;
            return YES;
        }
    }
    // 2. キャッシュにない場合はtreenode_poolを全走査
    for (int i = 0; i < MU_TREENODEPOOL_SIZE; ++i) {
        mu_PoolItem *item = &ctx->treenode_pool[i];
        if (item->id == 0) continue;
        // キャッシュにない場合はlabel不明なのでidのみ
        mu_Id id = item->id;
        // last_rectがそのidの矩形か判定
        // ※last_rect/last_idは直近の描画矩形なので、全ウィジェット対応には不十分
        // → コマンドリストをスキャンしてid一致かつ矩形一致を探す
        mu_Command* cmd = NULL;
        ctx->command_list.idx = 0;
        while (mu_next_command(ctx, &cmd)) {
            if (cmd->type == MU_COMMAND_CLIP && ctx->last_id == id) {
                mu_Rect r = cmd->clip.rect;
                if (position.x >= r.x && position.x < r.x + r.w &&
                    position.y >= r.y && position.y < r.y + r.h) {
                    if (outId) *outId = id;
                    if (outLabel) *outLabel = NULL; // ラベルは不明
                    if (outActive) *outActive = 1;
                    return YES;
                }
            }
        }
    }
    return NO;
}

// プリセットされたヘッダーをチェック - レガシー関数（後方互換性のため）
static BOOL ios_check_header_at_position(mu_Context* ctx, CGPoint position) {
    mu_Id headerId = 0;
    const char* headerLabel = NULL;
    BOOL isActive = NO;
    
    // 新しいios_get_header_at_position関数を使用
    if (ios_get_header_at_position(ctx, position, &headerId, &headerLabel, &isActive)) {
      
        return YES;
    }
    
    return NO;
}

// --- 既知ウィンドウ・ヘッダー名リストを一元化 ---
static const char* kKnownHeaders[] = { "Window Info", "Test Buttons", "Background Color", NULL };
static NSArray<NSString *> *kKnownWindows = nil;

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

// MicroUI用のコールバック関数
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


// MicroUIのヘッダー描画をフックするラッパー関数
int ios_header_wrapper(mu_Context* ctx, const char* label, int istreenode, int opt) {

    // オリジナルのヘッダー描画
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    
    // 永続的な状態を取得
    BOOL expanded = NO;
    ios_get_header_state(id, &expanded);
    
    // 永続的な状態に基づいてオプションを修正
    if (expanded) {
        opt |= MU_OPT_EXPANDED;
    } else {
        opt &= ~MU_OPT_EXPANDED;
    }
    
    int result;
    
    // ヘッダーに対する状態変化をキャプチャするためにラップ
    if (istreenode) {
        result = mu_begin_treenode_ex(ctx, label, opt);
    } else {
        result = mu_header_ex(ctx, label, opt);
    }
    // ここでキャッシュ
    ios_cache_header(ctx, label, ctx->last_rect, result);
    // 元の結果を返す
    return result;
}

// プルダウンのラッパーヘルパー（読みやすさのため）
int ios_header(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 0, opt);
}

// ツリーノードのラッパーヘルパー（読みやすさのため）
int ios_begin_treenode(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 1, opt);
}

// プルダウンヘッダーの検出用構造体
typedef struct {
    mu_Id id;
    mu_Rect rect;
    const char* label;
    int poolIndex;
} HeaderInfo;
// 指定座標にあるヘッダーをキャッシュから検出する汎用関数
static BOOL ios_detect_header_at_point(mu_Context* ctx, CGPoint point, HeaderInfo* outInfo) {
    if (!ctx || !outInfo) return NO;
    mu_Id id = 0;
    const char* label = NULL;
    BOOL active = NO;
    if (ios_get_header_at_position(ctx, point, &id, &label, &active)) {
        outInfo->id = id;
        outInfo->rect = (label != NULL)
            ? headerCache[0].rect // キャッシュから取得できる場合
            : ctx->last_rect;     // コマンドリストから取得した場合
        outInfo->label = label;
        outInfo->poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
        return YES;
    }
    return NO;
}


// metal_renderer実装 (スネークケース命名規則に合わせる)
@implementation metal_renderer

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
    
    // ブレンド設定
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

}

- (void)createAtlasTexture {
    MTLTextureDescriptor *textureDescriptor = [[MTLTextureDescriptor alloc] init];
    textureDescriptor.pixelFormat = MTLPixelFormatRGBA8Unorm;
    textureDescriptor.width = ATLAS_WIDTH;
    textureDescriptor.height = ATLAS_HEIGHT;
    textureDescriptor.usage = MTLTextureUsageShaderRead;
    
    self.atlasTexture = [self.device newTextureWithDescriptor:textureDescriptor];
    
    // atlas.inlのデータをRGBA形式に変換
    uint32_t *rgbaData = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        uint8_t alpha = atlas_texture[i];
        // RGBを白に、アルファを設定
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
    
    // 頂点データの設定
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
    
    // インデックスデータの設定
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
    mu_Rect src = atlas[iconId];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    mu_Rect dst = { x, y, src.w, src.h };
    [self pushQuad:dst src:src color:color];
}

- (void)setClipRect:(mu_Rect)rect {
    // Metalでのシザー矩形設定（描画時に適用）
}

// レンダリングプロセスにもログを追加
- (void)renderFrame {

}
- (void)processFrame {
    if (mu_begin_window(self.muiContext, "Demo Window", mu_rect(40, 40, 300, 250))) {
        mu_Container* win = mu_get_current_container(self.muiContext);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);
        
        // ウィンドウ情報 - カスタムラッパーを使用
        if (ios_header(self.muiContext, "Window Info", 0)) {
            mu_layout_row(self.muiContext, 2, (int[]){54, -1}, 0);
            mu_label(self.muiContext, "Position:");
            
            // 動的に現在の位置を表示
            char pos[32];
            snprintf(pos, sizeof(pos), "X: %d, Y: %d", win->rect.x, win->rect.y);
            mu_label(self.muiContext, pos);
            
            mu_label(self.muiContext, "Size:");
            mu_label(self.muiContext, "W: 300, H: 250");
            
            // タイトルバーをドラッグできることを表示
            mu_layout_row(self.muiContext, 1, (int[]){-1}, 0);
            mu_label(self.muiContext, "タイトルバーをドラッグで移動");
        }
        
        // テストボタン - カスタムラッパーを使用
        if (ios_header(self.muiContext, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(self.muiContext, 3, (int[]){86, -110, -1}, 0);
            mu_label(self.muiContext, "Test buttons 1:");
            if (mu_button(self.muiContext, "Button 1")) {
                NSLog(@"[MicroUI Widget] Button 1 clicked - hover=%d, focus=%d",
                    self.muiContext->hover, self.muiContext->focus);
                write_log("Pressed button 1");
            }
            if (mu_button(self.muiContext, "Button 2")) {
                write_log("Pressed button 2");
            }
        }
        
        // 背景色スライダー - カスタムラッパーを使用
        if (ios_header(self.muiContext, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(self.muiContext, 2, (int[]){-78, -1}, 0);
            
            mu_label(self.muiContext, "Red:");   ios_uint8_slider(self.muiContext, (unsigned char*)&bg[0], 0, 255);
            mu_label(self.muiContext, "Green:"); ios_uint8_slider(self.muiContext, (unsigned char*)&bg[1], 0, 255);
            mu_label(self.muiContext, "Blue:");  ios_uint8_slider(self.muiContext, (unsigned char*)&bg[2], 0, 255);
        }
        
        mu_end_window(self.muiContext);
    }
    
    // ログウィンドウ
    if (mu_begin_window(self.muiContext, "Log Window", mu_rect(40, 10, 300, 200))) {
        mu_layout_row(self.muiContext, 1, (int[]){-1}, -25);
        mu_begin_panel(self.muiContext, "Log Output");
        mu_Container* panel = mu_get_current_container(self.muiContext);
        mu_layout_row(self.muiContext, 1, (int[]){-1}, -1);
        mu_text(self.muiContext, logbuf);
        mu_end_panel(self.muiContext);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }
        // --- ここから追加 ---
        static char input_buf[128];
        int submitted = 0;
        mu_layout_row(self.muiContext, 2, (int[]){-70, -1}, 0);
        if (ios_textbox(self.muiContext, input_buf, sizeof(input_buf)) & MU_RES_SUBMIT) {
            mu_set_focus(self.muiContext, self.muiContext->last_id);
            submitted = 1;
        }
        if (mu_button(self.muiContext, "Submit")) { submitted = 1; }
        if (submitted) {
            write_log(input_buf);
            input_buf[0] = '\0';
        }
        // --- ここまで追加 ---
        mu_layout_row(self.muiContext, 2, (int[]){-1, -1}, 0);
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
// クリッピング矩形をMetalのシザー矩形として設定する関数
static void set_microui_clip(id<MTLRenderCommandEncoder> encoder, mu_Rect rect, CGSize drawableSize) {
    // クリップなし（MU_CLIP_PART）の場合
    if (rect.w == 16777216 || rect.h == 16777216) {
        MTLScissorRect scissor = {0, 0, (NSUInteger)drawableSize.width, (NSUInteger)drawableSize.height};
        [encoder setScissorRect:scissor];
        return;
    }
    // 負値や範囲外の補正
    if (rect.x < 0) rect.x = 0;
    if (rect.y < 0) rect.y = 0;
    if (rect.w <= 0) rect.w = 1;
    if (rect.h <= 0) rect.h = 1;
    if (rect.x >= drawableSize.width) rect.x = drawableSize.width - 1;
    if (rect.y >= drawableSize.height) rect.y = drawableSize.height - 1;
    if (rect.x + rect.w > drawableSize.width) rect.w = drawableSize.width - rect.x;
    if (rect.y + rect.h > drawableSize.height) rect.h = drawableSize.height - rect.y;
    if (rect.w <= 0) rect.w = 1;
    if (rect.h <= 0) rect.h = 1;
    MTLScissorRect scissor = { (NSUInteger)rect.x, (NSUInteger)rect.y, (NSUInteger)rect.w, (NSUInteger)rect.h };
    [encoder setScissorRect:scissor];
}
- (void)drawInMTKView:(MTKView *)view {
    // MicroUIの状態更新は行わない（これはupdateDisplayで実行済み）
    // ★ここでrenderFrameは呼び出さない★
    
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
        
        // デフォルトのシザー矩形を設定
        MTLScissorRect defaultScissor = {0, 0, view.drawableSize.width, view.drawableSize.height};
        [renderEncoder setScissorRect:defaultScissor];
        
        // ★MicroUIコマンドの処理 - この部分はそのまま維持★
        mu_Command* cmd = NULL;
        while (mu_next_command(self.muiContext, &cmd)) {
            switch (cmd->type) {
                case MU_COMMAND_CLIP: {
                    mu_Rect clipRect = cmd->clip.rect;
                    set_microui_clip(renderEncoder, clipRect, view.drawableSize);
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
    // displayLinkの参照を削除し、Metal関連リソースを適切に解放
    if (self.muiContext) {
        free(self.muiContext);
        self.muiContext = NULL;
    }
    
    // その他のMetalリソースの解放
    self.vertexBuffer = nil;
    self.indexBuffer = nil;
    self.uniformBuffer = nil;
    self.atlasTexture = nil;
    self.samplerState = nil;
    self.pipelineState = nil;
    self.commandQueue = nil;
    self.device = nil;
}

@end

// --- ログ共通化: write_logをmicro_ui_logに統一 ---
// まずログ関数を定義
#include <stdarg.h>
static void micro_ui_log(const char *format, ...) {
#ifdef DEBUG
    va_list args;
    va_start(args, format);
    char buf[256];
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    NSLog(@"[MicroUI] %s", buf);
#endif
}

// より詳細なログ出力関数を追加


// micro_ui_view_controller実装 (スネークケース命名規則に合わせる)
@implementation micro_ui_view_controller

- (void)queueEvent:(NSDictionary *)event {
    [self.eventLock lock];
    [self.pendingEvents addObject:event];
    self.needsRedraw = YES;
    [self.eventLock unlock];
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.metalView setNeedsDisplay];
    });
}
- (void)focusTextBox:(char *)buf size:(int)size {
    self.activeTextBoxBuffer = buf;
    self.activeTextBoxBufferSize = size;
    [self.hiddenTextField becomeFirstResponder];
}

// コンテナのドラッグ状態を判定するヘルパーメソッドを修正
- (void)logContainerState:(NSString *)prefix {
    mu_Container *win = mu_get_container(self.renderer.muiContext, "Demo Window");
    if (win) {
        BOOL isDragging = (self.renderer.muiContext->mouse_down &&
                          self.renderer.muiContext->focus != 0);
    }
}

// 強化版状態ログ出力関数
- (void)logMicroUIState:(NSString*)eventType location:(CGPoint)location {
    mu_Context *ctx = self.renderer.muiContext;
    // コンテナ状態もログ
    [self logContainerState:eventType];
}


// 指定座標がどのウィンドウのタイトルバーにあるか、そのウィンドウ名を返す
- (mu_Container *)containerAtTitleBarPoint:(CGPoint)point {
    mu_Context *ctx = self.renderer.muiContext;
    if (!ctx) return NULL;

    for (int i = 0; i < MU_CONTAINERPOOL_SIZE; ++i) {
        mu_Container *container = &ctx->containers[i];
        if (!container->open) continue;
        if (container->rect.w == 0 || container->rect.h == 0) continue;

        mu_Rect titleBar = container->rect;
        int titleHeight = ctx->style ? ctx->style->title_height : 24;
        titleBar.h = titleHeight;

        if (point.x >= titleBar.x && point.x < titleBar.x + titleBar.w &&
            point.y >= titleBar.y && point.y < titleBar.y + titleBar.h) {
            return container;
        }
    }
    return NULL;
}

- (void)viewDidLoad {
    [super viewDidLoad];
    
    // イベントキュー関連の初期化
    self.pendingEvents = [NSMutableArray array];
    self.eventLock = [[NSLock alloc] init];
    self.needsRedraw = YES;
    
    // iOS用の背景色設定
    self.view.backgroundColor = [UIColor colorWithRed:bg[0]/255.0 green:bg[1]/255.0 blue:bg[2]/255.0 alpha:1.0];
    
    // MetalViewの設定
    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds];
    self.metalView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

     // メインスレッドレンダリング用の重要な設定
    self.metalView.enableSetNeedsDisplay = YES;  // 明示的な描画要求のみに応答
    self.metalView.paused = YES;                // 自動レンダリングを停止
    [self.view addSubview:self.metalView];
    
    // レンダラーの初期化
    self.renderer = [[metal_renderer alloc] initWithMetalKitView:self.metalView];
    
    if (!self.renderer) {
        NSLog(@"Failed to initialize Metal renderer");
        return;
    }
    
    // MicroUIコンテキストが利用可能になった後、永続的ヘッダー状態を初期化
    ios_init_header_states();
    
    // デフォルトでいくつかのヘッダーを展開状態に設定
    mu_Id testButtonsId = mu_get_id(self.renderer.muiContext, "Test Buttons", strlen("Test Buttons"));
    ios_set_header_state(testButtonsId, "Test Buttons", YES);
    
    mu_Id colorHeaderId = mu_get_id(self.renderer.muiContext, "Background Color", strlen("Background Color"));
    ios_set_header_state(colorHeaderId, "Background Color", YES);
    
    // マルチタッチを有効化
    self.metalView.multipleTouchEnabled = YES;
    
    // CADisplayLinkの設定 - メインスレッドでのレンダリングループ用
    self.displayLink = [CADisplayLink displayLinkWithTarget:self

                                                selector:@selector(updateDisplay:)];
    self.displayLink.preferredFramesPerSecond = 60;  // 60FPSに設定
    [self.displayLink addToRunLoop:[NSRunLoop mainRunLoop]
                         forMode:NSRunLoopCommonModes];
    
    // 初期ログメッセージ
    write_log("MicroUI Metal iOS デモ開始 - イベントキュー実装版");
    
    NSString *sizeStr = [NSString stringWithFormat:@"Screen size: %.0fx%.0f",
                       self.view.bounds.size.width, self.view.bounds.size.height];
    write_log([sizeStr UTF8String]);
    
    write_log("タッチ入力有効 - イベントキュー使用");
    
    // --- テキスト入力用の不可視UITextFieldを追加 ---
    self.hiddenTextField = [[UITextField alloc] initWithFrame:CGRectZero];
    self.hiddenTextField.hidden = YES;
    self.hiddenTextField.delegate = self;
    self.hiddenTextField.autocorrectionType = UITextAutocorrectionTypeNo;
    self.hiddenTextField.autocapitalizationType = UITextAutocapitalizationTypeNone;
    self.hiddenTextField.spellCheckingType = UITextSpellCheckingTypeNo;
    [self.view addSubview:self.hiddenTextField];
    
    // --- 初期化 ---
    self.activeTextBoxBuffer = NULL;
    self.activeTextBoxBufferSize = 0;
}

// メインスレッドでのレンダリング処理とイベント処理
- (void)updateDisplay:(CADisplayLink *)displayLink {
    mu_Context *ctx = self.renderer.muiContext;
    

        if (self.activeTextBoxBuffer && ctx->focus != ios_find_textbox_at_point(ctx->mouse_pos.x, ctx->mouse_pos.y)) {
        // テキストボックス以外にフォーカスが移った場合
        [self.hiddenTextField resignFirstResponder];
        self.activeTextBoxBuffer = NULL;
        self.activeTextBoxBufferSize = 0;
    }
    // iOS向けのMicroUI状態修正（毎フレーム実行）
    ios_fix_hover_state(ctx);
    
    mu_begin(ctx);
    
    // 新しいフレームの開始時にヘッダーキャッシュをクリア
    ios_clear_header_cache();
    ios_clear_slider_cache();
    ios_clear_textbox_cache();
    
    // イベントキューからイベントを処理
    NSArray *events;
    [self.eventLock lock];
    events = [self.pendingEvents copy];
    [self.pendingEvents removeAllObjects];
    self.needsRedraw = NO;
    [self.eventLock unlock];
    
    // プルダウン（ヘッダー）タップ後の処理 - 状態リセットのみ
    if (self.didTapHeader) {
        // 状態を安定化させるためにマウス状態を明示的にリセット
        ctx->mouse_down = 0;
        ctx->mouse_pressed = 0;
        
        // フラグをリセット
        self.didTapHeader = NO;
    }
    // イベントを処理
    BOOL hadEvents = (events.count > 0);
    mu_Id lastFocusedId = ctx->focus;  // 現在のフォーカス状態を保存
    
    for (NSDictionary *event in events) {
        NSString *type = event[@"type"];
        // --- 追加: location変数を宣言し、positionから取得 ---
        CGPoint location = CGPointZero;
        if (event[@"position"]) {
            location = [event[@"position"] CGPointValue];
        }
        // 安全確認: 座標が有効範囲内か
        int x = (int)location.x;
        int y = (int)location.y;
        
        // イベントタイプに応じた処理
        if ([type isEqualToString:@"move"]) {
            // マウス移動イベント（ドラッグ処理はtouchesMoved:メソッドで実行するため、ここでは位置更新のみ）
            mu_input_mousemove(ctx, x, y);
            
            // ドラッグ中はフォーカスとホバーの一致を確保
            if (ctx->mouse_down == MU_MOUSE_LEFT && ctx->focus != 0) {
                if (ctx->hover != ctx->focus && ctx->focus != 0) {
                    ios_set_hover(ctx, ctx->focus);
                }
            }
        }
        else if ([type isEqualToString:@"down"]) {
            // マウスダウンイベント - タッチ開始
            mu_input_mousemove(ctx, x, y);
            mu_input_mousedown(ctx, x, y, MU_MOUSE_LEFT);
            
            // 特殊要素のクリック処理
            BOOL isTitleBarTouch = [event[@"isTitleBar"] boolValue];
            
            HeaderInfo headerInfo = {0};
            BOOL isHeaderTouch = ios_detect_header_at_point(ctx, location, &headerInfo);
            if (isHeaderTouch) {
                self.didTapHeader = YES;
                self.lastHeaderTapPosition = location;
                write_log("ヘッダークリック処理準備開始");
                if (headerInfo.id != 0) {
                    mu_set_focus(ctx, headerInfo.id);
                    ios_set_hover(ctx, headerInfo.id);
                    ctx->mouse_pressed = MU_MOUSE_LEFT;
                    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerInfo.id);
                    if (idx >= 0) {
                        write_log("プルダウン: アクティブ → 非アクティブに変更");
                        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
                    } else {
                        write_log("プルダウン: 非アクティブ → アクティブに変更");
                        mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerInfo.id);
                    }
                } else {
                    write_log("ヘッダー要素の検出に失敗しました");
                }
            }
            else if (isHeaderTouch) {
                // ヘッダークリック処理の準備
                self.didTapHeader = YES;
                self.lastHeaderTapPosition = location;
                
                write_log("ヘッダークリック処理準備開始");
                
                // 改善: ヘッダー情報の取得
                mu_Id headerId = 0;
                const char* headerLabel = NULL;
                
                BOOL isActive = NO;
                if (ios_get_header_at_position(ctx, location, &headerId, &headerLabel, &isActive)) {
                    // ヘッダーID設定とフォーカスの設定
                    mu_set_focus(ctx, headerId);
                    ios_set_hover(ctx, headerId);
                    
                    // mouse_pressedを設定して、ヘッダーロジックをトリガー
                    ctx->mouse_pressed = MU_MOUSE_LEFT;
                    
                    // 現在のヘッダー状態をログに記録
                    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerId);

                    
                    // アクティブ状態を反転させる試み（プルダウン動作のエミュレート）
                    if (idx >= 0) {
                        write_log("プルダウン: アクティブ → 非アクティブに変更");
                        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem)); // 非アクティブに
                    } else {
                        write_log("プルダウン: 非アクティブ → アクティブに変更");
                        mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerId); // アクティブに
                    }
                } else {
                    write_log("ヘッダー要素の検出に失敗しました");
                }
            } else {
                // 通常のタッチ処理
                write_log("通常領域タッチ開始");
                // 通常領域タッチ処理
                int tx = (int)location.x, ty = (int)location.y;
                mu_Id sliderId = ios_find_slider_at_point(tx, ty);
                mu_Id textboxId = ios_find_textbox_at_point(tx, ty);
                if (sliderId) {
                    mu_set_focus(ctx, sliderId);
                    ios_set_hover(ctx, sliderId);
                    ctx->mouse_down = 1;
                    ctx->mouse_pressed = 1;
                    write_log("スライダーにタッチ: IDセット");
                } else if (textboxId) {
                    mu_set_focus(ctx, textboxId);
                    ios_set_hover(ctx, textboxId);
                    ctx->mouse_down = 1;
                    ctx->mouse_pressed = 1;
                    write_log("テキストボックスにタッチ: IDセット");
                    // --- ここでバッファを取得しfocusTextBoxを呼ぶ ---
                    // textboxIdに対応するバッファを取得する必要あり
                    // 例: input_bufなど
                    extern char input_buf[128];
                    [self focusTextBox:input_buf size:128];
                } else {
                    ctx->mouse_down = 1;
                    ctx->mouse_pressed = 1;
                }
            }
        }
        else if ([type isEqualToString:@"up"]) {
            // マウスアップイベント - タッチ終了
            BOOL wasDragging = [event[@"wasDragging"] boolValue];
            
            mu_input_mousemove(ctx, x, y);
            mu_input_mouseup(ctx, x, y, MU_MOUSE_LEFT);
            
            // タッチ終了後の状態を安定化
            ios_set_hover(ctx, 0);

        }
    }
    
    // UIを更新
    [self.renderer processFrame];
    
    // フォーカス状態が変化した場合のログ出力
    if (hadEvents && lastFocusedId != ctx->focus) {
        char msg[128];
        snprintf(msg, sizeof(msg), "フォーカス変更: %u → %u",
                lastFocusedId, ctx->focus);
        write_log(msg);
    }
    
    mu_end(ctx);
    
    // フレーム終了時の状態チェックと修正（減らしたログ表示でヘッダー処理を優先）
    ios_fix_hover_state(ctx);
    

    
    // ドラッグ状態だけはホバー状態を維持
    if (ctx->mouse_down && ctx->focus != 0 && ctx->hover != ctx->focus) {
        ios_set_hover(ctx, ctx->focus);
        
    }
    
    // タッチがない場合は確実にホバーをクリア（安定化）
    if (!ctx->mouse_down && ctx->hover != 0) {
        ios_set_hover(ctx, 0);
    }
    
    // 異常値のチェック（フレーム終了時）
    if (ctx->hover <  0 || ctx->focus < 0) {

        // 強制リセット
        ctx->hover = 0;
        ctx->prev_hover = 0;
        
        if (ctx->focus < 0) {
            ctx->focus = 0;
            ctx->prev_focus = 0;
        }
    }
    

    // 描画要求
    [self.metalView setNeedsDisplay];
}
// タッチ座標をMicroUI座標系に変換するヘルパーメソッド（左上原点）
- (CGPoint)getTouchLocation:(UITouch *)touch {
    CGPoint location = [touch locationInView:self.metalView];
    
    // iOS座標系（左上原点）をそのまま使用（MicroUIも左上原点）
    return location;
}

// 指定座標がタイトルバー内にあるかをチェック
- (BOOL)isPointInTitleBar:(CGPoint)point {
    mu_Context *ctx = self.renderer.muiContext;
    mu_Container *win = mu_get_container(ctx, "Demo Window");
    int titleHeight = ctx && ctx->style ? ctx->style->title_height : -1;
    mu_Rect title_rect = { win ? win->rect.x : -1, win ? win->rect.y : -1, win ? win->rect.w : -1, win ? win->rect.h : -1, titleHeight };

    return [self pointInRect:title_rect point:mu_vec2(point.x, point.y)];
}

// タイトルバーの当たり判定用の関数（microuiのrect_overlaps_vec2と同等）
- (BOOL)pointInRect:(mu_Rect)rect point:(mu_Vec2)point {
    return point.x >= rect.x && point.x < rect.x + rect.w &&
           point.y >= rect.y && point.y < rect.y + rect.h;
}

// タイトルバーのIDを計算するヘルパーメソッド
- (mu_Id)calculateTitleId:(mu_Context *)ctx windowName:(const char *)name {
    // MicroUI内部のID計算方法をエミュレート
    // タイトルバーはウィンドウ名+"!title"でIDを計算している
    mu_Id windowId = mu_get_id(ctx, name, strlen(name));
    
    // タイトルIDの計算
    char titleBuf[64];
    snprintf(titleBuf, sizeof(titleBuf), "%s!title", name);
    return mu_get_id(ctx, titleBuf, strlen(titleBuf));
}

/**
 * ウィンドウのドラッグ状態を設定するヘルパーメソッド（最適化版）
 * @param ctx MicroUIコンテキスト
 * @param name ドラッグするウィンドウの名前
 * @param position タッチ/クリック位置
 */
- (void)setupWindowDrag:(mu_Context *)ctx windowName:(const char *)name position:(CGPoint)position {
    // 引数の検証
    if (!ctx || !name || !*name) return;
    
    mu_Container *win = mu_get_container(ctx, name);
    if (!win) return;
    
    // タイトルバー領域を効率的に計算
    const mu_Rect title_rect = {
        win->rect.x,
        win->rect.y,
        win->rect.w,
        ctx->style && ctx->style->title_height > 0 ? ctx->style->title_height : 24 // デフォルト値も提供
    };
    
    // タイトルバー内のクリックか簡潔に確認
    if (position.x >= title_rect.x && position.x < title_rect.x + title_rect.w &&
        position.y >= title_rect.y && position.y < title_rect.y + title_rect.h) {
        
        // ウィンドウを最前面に
        mu_bring_to_front(ctx, win);
        
        // タイトルバーのIDを計算
        mu_Id titleId = [self calculateTitleId:ctx windowName:name];
        
        // フォーカスとホバー状態をセット（アトミック操作）
        mu_set_focus(ctx, titleId);
        ios_set_hover(ctx, titleId);
        
        // ドラッグの初期状態を保存（高精度のためにプロパティに直接格納）
        self.dragStartWindowX = win->rect.x;
        self.dragStartWindowY = win->rect.y;
        
        #ifdef DEBUG
        write_log([NSString stringWithFormat:@"タイトルバードラッグ開始: %s @ (%d,%d)",
                  name, win->rect.x, win->rect.y].UTF8String);
        #endif
    }
}

// ドラッグ状態を詳細にログ出力するメソッド
- (void)logDraggingState:(NSString *)eventName {
    mu_Context *ctx = self.renderer.muiContext;
    mu_Container *win = mu_get_container(ctx, "Demo Window");
    
    // ドラッグ状態の判定：タイトルがフォーカスされていて、マウスボタンが押されている状態
    BOOL isDragging = (ctx->focus != 0 && ctx->mouse_down == MU_MOUSE_LEFT);
    
    // ドラッグに関連する状態をすべてログに出力
    char msg[256];
    snprintf(msg, sizeof(msg),
             "[%s] ドラッグ状態: focus=%d, hover=%d, mouse_down=%d, mouse_pressed=%d, "
             "状態=%s, ウィンドウ位置=(%d,%d)",
             [eventName UTF8String], ctx->focus, ctx->hover, ctx->mouse_down, ctx->mouse_pressed,
             isDragging ? "ドラッグ中" : "通常",
             win ? win->rect.x : -1, win ? win->rect.y : -1);
    write_log(msg);
}

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    mu_Context *ctx = self.renderer.muiContext;
    mu_Container *win = [self containerAtTitleBarPoint:location];

    #ifdef DEBUG
    write_log([NSString stringWithFormat:@"touchesBegan: (%.1f,%.1f)", location.x, location.y].UTF8String);
    #endif

    if (!ctx || location.x < 0 || location.x >= self.metalView.bounds.size.width ||
        location.y < 0 || location.y >= self.metalView.bounds.size.height) {
        return;
    }

    ios_set_hover(ctx, 0);
    self.didTapHeader = NO;

    // タイトルバー判定（ウィンドウ名ベース）
     //mu_Container *win = [self containerAtTitleBarPoint:location];
    HeaderInfo headerInfo = {0};
    BOOL isHeaderTouch = ios_detect_header_at_point(ctx, location, &headerInfo);

    if (win) {
        self.draggingContainer = win;
        self.isDraggingWindow = YES;
        self.dragStartTouchPosition = location;
        self.dragStartWindowX = win->rect.x;
        self.dragStartWindowY = win->rect.y;
        self.dragOffset = CGPointMake(location.x - win->rect.x, location.y - win->rect.y);
        write_log("タイトルバータッチ: ドラッグモード開始");
        ctx->mouse_down = 1;
        ctx->mouse_pressed = 1;
        // フォーカスIDは必要ならmu_get_idで生成
        // mu_set_focus(ctx, ...);
        // ios_set_hover(ctx, ...);
        return;
    } else if (isHeaderTouch) {
        self.didTapHeader = YES;
        self.lastHeaderTapPosition = location;
        write_log("ヘッダークリック処理準備開始");
        if (headerInfo.id != 0) {
            mu_set_focus(ctx, headerInfo.id);
            ios_set_hover(ctx, headerInfo.id);
            ctx->mouse_pressed = MU_MOUSE_LEFT;
            int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerInfo.id);
            if (idx >= 0) {
                write_log("プルダウン: アクティブ → 非アクティブに変更");
                memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
            } else {
                write_log("プルダウン: 非アクティブ → アクティブに変更");
                mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerInfo.id);
            }
        } else {
            write_log("ヘッダー要素の検出に失敗しました");
        }
    } else {
        write_log("通常領域タッチ開始");
        int tx = (int)location.x, ty = (int)location.y;
        mu_Id sliderId = ios_find_slider_at_point(tx, ty);
        mu_Id textboxId = ios_find_textbox_at_point(tx, ty);
        if (sliderId) {
            mu_set_focus(ctx, sliderId);
            ios_set_hover(ctx, sliderId);
            ctx->mouse_down = 1;
            ctx->mouse_pressed = 1;
            write_log("スライダーにタッチ: IDセット");
        } else if (textboxId) {
            mu_set_focus(ctx, textboxId);
            ios_set_hover(ctx, textboxId);
            ctx->mouse_down = 1;
            ctx->mouse_pressed = 1;
            write_log("テキストボックスにタッチ: IDセット");
            extern char input_buf[128];
            [self focusTextBox:input_buf size:128];
        } else {
            ctx->mouse_down = 1;
            ctx->mouse_pressed = 1;
        }
    }

    ctx->mouse_pos = mu_vec2(location.x, location.y);
    self.needsRedraw = YES;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    mu_Context *ctx = self.renderer.muiContext;
    if (self.isDraggingWindow && self.draggingContainer) {
        mu_Container *win = self.draggingContainer;
        CGFloat newX = location.x - self.dragOffset.x;
        CGFloat newY = location.y - self.dragOffset.y;
        win->rect.x = (int)roundf(newX);
        win->rect.y = (int)roundf(newY);
        #ifdef DEBUG
        static int lastLoggedX = 0, lastLoggedY = 0;
        if (abs(lastLoggedX - win->rect.x) > 5 || abs(lastLoggedY - win->rect.y) > 5) {
            lastLoggedX = win->rect.x;
            lastLoggedY = win->rect.y;
            write_log([NSString stringWithFormat:@"ウィンドウ移動: %p -> (%d,%d)",
                      win, win->rect.x, win->rect.y].UTF8String);
        }
        #endif
        ctx->mouse_pos = mu_vec2(location.x, location.y);
        self.needsRedraw = YES;
               return;
    }

    [self queueEvent:@{
        @"type": @"move",
        @"position": [NSValue valueWithCGPoint:location],
        @"isDragging": @(NO),
        @"previousPosition": [NSValue valueWithCGPoint:[touch previousLocationInView:self.metalView]]
    }];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    mu_Context *ctx = self.renderer.muiContext;

    #ifdef DEBUG
    if (self.isDraggingWindow && self.draggingContainer) {
        mu_Container *win = self.draggingContainer;
        write_log([NSString stringWithFormat:@"タッチ終了: (%.1f,%.1f), ウィンドウ位置=(%d,%d)",
                  location.x, location.y, win ? win->rect.x : -1, win ? win->rect.y : -1].UTF8String);
    }
    #endif

    // テキストボックスIDを取得
    mu_Id textboxId = ios_find_textbox_at_point((int)location.x, (int)location.y);
    BOOL isTextboxFocus = NO;
    for (int i = 0; i < textboxCacheCount; ++i) {
        if (ctx->focus == textboxCache[i].id) {
            isTextboxFocus = YES;
            break;
        }
    }
    // wasDraggingの判定を修正
    BOOL wasDragging = (ctx->focus != 0 && ctx->mouse_down && !isTextboxFocus);
    BOOL wasHeaderTap = self.didTapHeader;


    BOOL shouldClearFocus = NO;

    if (wasDragging) {
        // ドラッグ操作の終了処理
        mu_Container *win = mu_get_container(ctx, "Demo Window");

        shouldClearFocus = YES;
    } else if (wasHeaderTap) {
        // ヘッダークリック処理
        HeaderInfo headerInfo = {0};
        if (ios_detect_header_at_point(ctx, self.lastHeaderTapPosition, &headerInfo)) {
            if (headerInfo.id != 0) {
                ios_toggle_header_state(ctx, headerInfo.id);
            }
        }
        shouldClearFocus = YES;
    } else {

        if (textboxId == 0) {
            shouldClearFocus = YES;
        }
    }
    if (shouldClearFocus) {
        NSLog(@"touchesEnded: ctx->focus をクリアします (元の値=%u)", ctx->focus);
        ctx->focus = 0;
    }

    // 状態のクリーンアップ
    ctx->mouse_down = 0;
    ctx->mouse_pressed = 0;
    ios_set_hover(ctx, 0);
    self.didTapHeader = NO;

    ctx->mouse_pos = mu_vec2(location.x, location.y);

    [self queueEvent:@{
        @"type": @"up",
        @"position": [NSValue valueWithCGPoint:location],
        @"wasDragging": @(wasDragging),
        @"wasHeaderTap": @(wasHeaderTap)
    }];



    self.isDraggingWindow = NO;
    self.needsRedraw = YES;
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    
    // キャンセルイベントも明示的にアップイベントとしてキューに追加
    [self queueEvent:@{
        @"type": @"up",
        @"position": [NSValue valueWithCGPoint:location]
    }];
    
    NSLog(@"タッチキャンセル: (%.1f, %.1f) -> イベントキューに追加", location.x, location.y);
    
    // 状態のリセット（ユーザー操作の中断に対応）
    mu_Context *ctx = self.renderer.muiContext;
    if (ctx) {
        self.isDraggingWindow = NO;
        ctx->focus = 0;
        ctx->hover = 0;
        ctx->mouse_down = 0;
        ctx->mouse_pressed = 0;
        ctx->active_drag_id = 0;
        mu_input_mouseup(ctx, location.x, location.y, MU_MOUSE_LEFT);
        
        // 状態をクリア（安全対策）
        ctx->mouse_down = 0;
    }
}

// レイアウト変更時の処理を追加（iOSでの処理）
- (void)viewDidLayoutSubviews {
    [super viewDidLayoutSubviews];
    
    // レイアウト変更時に状態をリセットして安全に保つ
    if (self.renderer && self.renderer.muiContext) {
        // ドラッグ中のレイアウト変更は状態を解除
        if (self.renderer.muiContext->mouse_down) {
            self.renderer.muiContext->mouse_down = 0;
            self.renderer.muiContext->focus = 0;
            write_log("レイアウト変更によりドラッグ状態をリセット");
        }
        
        // ビューサイズが変更された場合はプロジェクション行列を更新
        [self.renderer updateProjectionMatrix:self.metalView.bounds.size];
        [self.metalView setNeedsDisplay];
    }
}

// アプリのステータス管理
- (void)viewWillAppear:(BOOL)animated {
    [super viewWillAppear:animated];
    // 表示時にDisplayLinkを開始/再開
    self.displayLink.paused = NO;
    write_log("View will appear - DisplayLink activated");
}

- (void)viewDidAppear:(BOOL)animated {
    [super viewDidAppear:animated];
    write_log("View did appear");
}

// アプリケーション状態変化時のリセット処理を追加
- (void)viewWillDisappear:(BOOL)animated {
    [super viewWillDisappear:animated];
    // 非表示時にDisplayLinkを一時停止
    self.displayLink.paused = YES;
    write_log("View will disappear - DisplayLink paused");
    
    // 画面離脱時は完全にリセット
    if (self.renderer && self.renderer.muiContext) {
        self.renderer.muiContext->mouse_down = 0;
        self.renderer.muiContext->mouse_pressed = 0;
        self.renderer.muiContext->focus = 0;
        self.renderer.muiContext->hover = 0;
    }
}
// アプリがバックグラウンドに移行するときの処理
- (void)applicationWillResignActive:(UIApplication *)application {
    // アプリがバックグラウンドに移行する時も状態をリセット
    if (self.renderer && self.renderer.muiContext) {
        self.renderer.muiContext->mouse_down = 0;
        self.renderer.muiContext->mouse_pressed = 0;
        self.renderer.muiContext->focus = 0;
        self.renderer.muiContext->hover = 0;
        write_log("Reset state on app resign active");
    }
}
@end
// app_delegate実装 (スネークケース命名規則に合わせる)
@interface app_delegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation app_delegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    micro_ui_view_controller *viewController = [[micro_ui_view_controller alloc] init];
    self.window.rootViewController = viewController;
    [self.window makeKeyAndVisible];
    return YES;
}

@end
// main関数
int main(int argc, char * argv[]) {
    NSString * appDelegateClassName;
    @autoreleasepool {
        // Setup code that might create autoreleased objects goes here.
        appDelegateClassName = NSStringFromClass([app_delegate class]);
        return UIApplicationMain(argc, argv, nil, appDelegateClassName);
    }
}
