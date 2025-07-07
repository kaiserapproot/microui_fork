// MicroUIを使ったウィジェット表示のコードを追加

/*
 * フォントレンダリング修正履歴 (2025年6月11日)
 * ===============================================
 * 
 * 問題: スペース文字とその他の文字のatlas座標が(0,0,0,0)になる問題
 * 
 * 原因: インデックス計算が間違っていた
 * - 誤: atlas[ATLAS_FONT + (chr - 32)]  ← スペース文字で atlas[6 + 0] = atlas[6]
 * - 正: atlas[ATLAS_FONT + chr]         ← スペース文字で atlas[6 + 32] = atlas[38]
 * 
 * 修正内容:
 * 1. text_width関数とdrawText関数のインデックス計算を修正
 * 2. atlas.inlに詳細なコメントと構造説明を追加
 * 3. デバッグログでスペース文字の座標確認機能を追加
 * 4. バッチ処理システムの実装（USE_QUAD=1で有効）
 *    - pushQuad関数によるクアッド蓄積
 *    - flushBatch関数による一括描画
 *    - drawText, drawRect, drawIcon関数のバッチ処理対応
 *    - VBO/IBOを使用した高性能レンダリング
 * 
 * 結果: 
 * - スペース文字が正しく { 84, 68, 2, 17 } の座標データを取得
 * - 2ピクセル幅で正常にレンダリング
 * - バッチ処理により大幅なパフォーマンス向上
 * - #define USE_QUAD で即座描画とバッチ処理を切り替え可能
 * 
 * 関連ファイル:
 * - atlas.inl: フォントアトラスデータと詳細なコメント
 * - main.m: text_width, drawText関数の修正、完全なバッチ処理システム
 */

// バッチ処理モード切り替え（1でバッチ処理、0で従来の即座描画）
#define USE_QUAD 1

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <CoreVideo/CoreVideo.h>  // CVDisplayLinkのために追加
#import <QuartzCore/QuartzCore.h>  // Core Animation用
#import "microui.h"
#import "atlas.inl"

#define GLES_SILENCE_DEPRECATION 1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

// バッチ処理用の定数とバッファ（USE_QUAD=1の場合のみ）
#if USE_QUAD
#define BUFFER_SIZE 16384  // バッファサイズ
#define MAX_QUADS (BUFFER_SIZE / 4)  // 最大クアッド数

// バッチ処理用の構造体
typedef struct {
    float x, y;       // 位置
    float u, v;       // テクスチャ座標
    float r, g, b, a; // 色
} Vertex;

// バッチ描画用の状態管理
typedef struct {
    Vertex vertices[BUFFER_SIZE];
    GLuint indices[BUFFER_SIZE * 6 / 4]; // クアッドあたり6インデックス
    int vertex_count;
    int index_count;
    GLuint current_texture;
    BOOL is_textured;
} BatchState;

#endif

/**
 * NSViewベースのOpenGLビュークラス
 * NSViewの上でOpenGLコンテキストを管理し、三角形とMicroUIを描画します
 */
@interface MicroUIMacView : NSView
{
    NSOpenGLContext* openGLContext;    // OpenGLコンテキスト
    NSOpenGLPixelFormat* pixelFormat;  // ピクセルフォーマット
    GLuint VBO;                        // 頂点バッファオブジェクト
    
    /**
     * ディスプレイの垂直同期に合わせて描画を行うためのDisplayLink
     *
     * CVDisplayLinkは以下の特徴を持ちます：
     * - ディスプレイのリフレッシュレート（一般的に60Hz）に同期して描画を行う
     * - 効率的な描画タイミング制御により、ティアリング（画面の引き裂き）を防ぐ
     * - GPUとディスプレイの同期を取ることで、スムーズなアニメーションを実現
     * - バッファの入れ替えタイミングを最適化し、フレームのスキップを最小限に抑える
     *
     * 注意：
     * - DisplayLinkは別スレッドで動作するため、OpenGLコンテキストの扱いに注意が必要
     * - アプリケーション終了時には適切に停止・解放する必要がある
     */
    CVDisplayLinkRef displayLink;
    
    // MicroUI関連
    mu_Context* mu_ctx;
    GLuint font_texture;
    char logbuf[64000];
    int logbuf_updated;
    float bg[3];
    
    // ウィンドウ移動検出用
    int prevWindowX;
    int prevWindowY;
    
#if USE_QUAD
    // バッチ処理用メンバー
    BatchState batch_state;
    GLuint vertex_buffer;
    GLuint index_buffer;
#endif
}

// OpenGL関連のメソッド
- (void)setupOpenGL;
- (void)setupPixelFormat;
- (NSOpenGLContext*)openGLContext;

// MicroUI関連のメソッド
- (void)initMicroUI;
- (void)processMicroUIFrame;
- (void)drawMicroUI;
- (void)handleMicroUIEvent:(NSEvent*)event;

#if USE_QUAD
// バッチ処理用メソッド
- (void)initBatchBuffers;
- (void)flushBatch;
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured;
- (void)cleanupBatchBuffers;
#endif

@end

@implementation MicroUIMacView

/**
 * DisplayLinkのコールバック関数
 * ディスプレイの垂直同期のタイミングで呼び出され、描画を更新します
 */
static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink,
                                  const CVTimeStamp* now,
                                  const CVTimeStamp* outputTime,
                                  CVOptionFlags flagsIn,
                                  CVOptionFlags* flagsOut,
                                  void* displayLinkContext)
{
    // コンテキストからMicroUIMacViewインスタンスを取得
    MicroUIMacView* view = (__bridge MicroUIMacView*)displayLinkContext;
    
    // 描画処理をメインスレッドにディスパッチ
    dispatch_async(dispatch_get_main_queue(), ^{
        [view drawView];
    });
    
    return kCVReturnSuccess;
}

/**
 * ビューの描画メソッド
 */
- (void)drawView
{
    if (!self.window || ![self.window isVisible] || self.hidden) {
        return;
    }
    if (!openGLContext || !mu_ctx) {
        return;
    }
    
    [openGLContext makeCurrentContext];

    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    
    // Retinaディスプレイ対応
    if ([self.window respondsToSelector:@selector(backingScaleFactor)]) {
        CGFloat scaleFactor = [self.window backingScaleFactor];
        viewWidth *= scaleFactor;
        viewHeight *= scaleFactor;
    }

    // MicroUIフレーム処理
    [self processMicroUIFrame];

    // 背景クリア
    glViewport(0, 0, viewWidth, viewHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // 三角形の描画
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.5f, 0.0f);
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glEnd();

    // MicroUIの描画
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, viewWidth, 0, viewHeight, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glScalef(1.0f, -1.0f, 1.0f);
    glTranslatef(0.0f, -viewHeight, 0.0f);

    [self drawMicroUI];

    [openGLContext flushBuffer];
}

/**
 * ビューの初期化メソッド
 */
- (id)initWithFrame:(NSRect)frameRect
{
    self = [super initWithFrame:frameRect];
    
    if (self) {
        // レイヤーサポートを有効化
        [self setWantsLayer:YES];
        
        // OpenGLの設定
        [self setupPixelFormat];
        [self setupOpenGL];
        
        // マウストラッキングエリアを設定
        NSTrackingArea *trackingArea = [[NSTrackingArea alloc] 
            initWithRect:frameRect
                 options:(NSTrackingMouseMoved | NSTrackingActiveInKeyWindow | NSTrackingInVisibleRect)
                   owner:self
                userInfo:nil];
        [self addTrackingArea:trackingArea];
        
        // MicroUI関連のデフォルト値設定
        bg[0] = 90;
        bg[1] = 95;
        bg[2] = 100;
        logbuf[0] = '\0';
        logbuf_updated = 0;
        
        // ウィンドウ移動検出用の初期位置設定
        prevWindowX = 40;
        prevWindowY = 40;
        
#if USE_QUAD
        // バッチ処理用の初期化
        memset(&batch_state, 0, sizeof(BatchState));
        NSLog(@"Using QUAD batch processing mode");
#else
        NSLog(@"Using immediate drawing mode");
#endif
    }
    
    return self;
}

/**
 * ピクセルフォーマットの設定
 */
- (void)setupPixelFormat
{
    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAAccelerated,      // ハードウェアアクセラレーション
        NSOpenGLPFADoubleBuffer,     // ダブルバッファリングを有効化
        NSOpenGLPFAColorSize, 24,    // カラーバッファのビット深度
        NSOpenGLPFAAlphaSize, 8,     // アルファチャンネルのビット深度
        NSOpenGLPFADepthSize, 24,    // デプスバッファのビット深度
        0
    };
    
    pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    
    if (!pixelFormat) {
        NSLog(@"No OpenGL pixel format");
    }
}

/**
 * OpenGLコンテキストの設定
 */
- (void)setupOpenGL
{
    if (!pixelFormat) {
        NSLog(@"Pixel format not initialized");
        return;
    }
    
    openGLContext = [[NSOpenGLContext alloc] initWithFormat:pixelFormat shareContext:nil];
    
    if (!openGLContext) {
        NSLog(@"Failed to create OpenGL context");
        return;
    }
    
    [openGLContext makeCurrentContext];
    
    // DisplayLinkの設定
    NSLog(@"Setting up DisplayLink...");
    
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
    
    CGLContextObj cglContext = [openGLContext CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [pixelFormat CGLPixelFormatObj];
    result = CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
    if (result != kCVReturnSuccess) {
        NSLog(@"Failed to set DisplayLink context: %d", result);
        return;
    }
    
    NSLog(@"DisplayLink setup completed successfully");
    
    // 基本的なOpenGL設定
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // エラーをクリア
    while(glGetError() != GL_NO_ERROR);
    
#if USE_QUAD
    // バッチ処理用バッファの初期化
    [self initBatchBuffers];
#endif
    
    // MicroUIの初期化
    [self initMicroUI];
    [self processMicroUIFrame];
}

/**
 * ビューがウィンドウに追加された時に呼ばれるメソッド
 */
- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    NSLog(@"viewDidMoveToWindow called, window: %@", self.window);
    
    if (self.window) {
        // OpenGLコンテキストをウィンドウのビューに設定
        if (openGLContext) {
            [openGLContext setView:self];
        }
        
        // DisplayLinkの開始
        if (displayLink && !CVDisplayLinkIsRunning(displayLink)) {
            CVReturn result = CVDisplayLinkStart(displayLink);
            if (result == kCVReturnSuccess) {
                NSLog(@"DisplayLink started in viewDidMoveToWindow");
            } else {
                NSLog(@"Failed to start DisplayLink in viewDidMoveToWindow: %d", result);
            }
            
            // 初期描画
            dispatch_async(dispatch_get_main_queue(), ^{
                [self drawView];
            });
        }
    }
}

/**
 * ビューがウィンドウから削除された時に呼ばれるメソッド
 */
- (void)viewWillMoveToWindow:(NSWindow *)newWindow
{
    [super viewWillMoveToWindow:newWindow];
    
    // ビューがウィンドウから削除される場合はDisplayLinkを停止
    if (newWindow == nil && displayLink && CVDisplayLinkIsRunning(displayLink)) {
        CVDisplayLinkStop(displayLink);
    }
}

/**
 * ビューのサイズが変更された時の処理
 */
- (void)setFrameSize:(NSSize)newSize
{
    [super setFrameSize:newSize];
    
    if (openGLContext) {
        [openGLContext makeCurrentContext];
        [openGLContext update];
    }
}

/**
 * OpenGLコンテキストのゲッター
 */
- (NSOpenGLContext*)openGLContext
{
    return openGLContext;
}

/**
 * 描画が必要かどうかを判定
 */
- (BOOL)isOpaque
{
    return YES;
}

/**
 * デアロケータ
 * OpenGLリソースとDisplayLinkを解放します
 */
- (void)dealloc
{
    // DisplayLinkの停止と解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    
    // OpenGLコンテキストをクリア
    if (openGLContext) {
        [openGLContext makeCurrentContext];
        
        // VBOの解放
        if (VBO) {
            glDeleteBuffers(1, &VBO);
        }
        
#if USE_QUAD
        // バッチバッファのクリーンアップ
        [self cleanupBatchBuffers];
#endif
        
        // MicroUIの解放
        if (font_texture) {
            glDeleteTextures(1, &font_texture);
        }
        if (mu_ctx) {
            free(mu_ctx);
        }
        
        [NSOpenGLContext clearCurrentContext];
        openGLContext = nil;
    }
    
    pixelFormat = nil;
}

#if USE_QUAD

/**
 * バッチ処理用バッファの初期化
 */
- (void)initBatchBuffers
{
    // インデックスバッファを事前計算
    for (int i = 0; i < MAX_QUADS; i++) {
        int quad_index = i * 6;
        int vertex_index = i * 4;
        
        // 各クアッドのインデックス（時計回り）
        batch_state.indices[quad_index + 0] = vertex_index + 0; // 左上
        batch_state.indices[quad_index + 1] = vertex_index + 1; // 右上
        batch_state.indices[quad_index + 2] = vertex_index + 2; // 右下
        batch_state.indices[quad_index + 3] = vertex_index + 2; // 右下
        batch_state.indices[quad_index + 4] = vertex_index + 3; // 左下
        batch_state.indices[quad_index + 5] = vertex_index + 0; // 左上
    }
    
    // VBOとIBOの生成
    glGenBuffers(1, &vertex_buffer);
    glGenBuffers(1, &index_buffer);
    
    // インデックスバッファにデータをアップロード（一度だけ）
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
                 sizeof(GLuint) * MAX_QUADS * 6, 
                 batch_state.indices, 
                 GL_STATIC_DRAW);
    
    NSLog(@"Batch buffers initialized: max_quads=%d", MAX_QUADS);
}

/**
 * バッチにクアッドを追加
 */
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured
{
    // バッファが満杯の場合はフラッシュ
    if (batch_state.vertex_count >= BUFFER_SIZE - 4) {
        [self flushBatch];
    }
    
    // テクスチャ状態が変わった場合もフラッシュ
    if (batch_state.vertex_count > 0 && batch_state.is_textured != textured) {
        [self flushBatch];
    }
    
    batch_state.is_textured = textured;
    
    // 色を正規化
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // テクスチャ座標を計算
    float u1 = 0.0f, v1 = 0.0f, u2 = 1.0f, v2 = 1.0f;
    if (textured) {
        u1 = (float)src.x / ATLAS_WIDTH;
        v1 = (float)src.y / ATLAS_HEIGHT;
        u2 = (float)(src.x + src.w) / ATLAS_WIDTH;
        v2 = (float)(src.y + src.h) / ATLAS_HEIGHT;
    }
    
    // 4つの頂点を追加
    int base_index = batch_state.vertex_count;
    
    // 左上
    batch_state.vertices[base_index + 0] = (Vertex){
        dst.x, dst.y, u1, v1, r, g, b, a
    };
    
    // 右上
    batch_state.vertices[base_index + 1] = (Vertex){
        dst.x + dst.w, dst.y, u2, v1, r, g, b, a
    };
    
    // 右下
    batch_state.vertices[base_index + 2] = (Vertex){
        dst.x + dst.w, dst.y + dst.h, u2, v2, r, g, b, a
    };
    
    // 左下
    batch_state.vertices[base_index + 3] = (Vertex){
        dst.x, dst.y + dst.h, u1, v2, r, g, b, a
    };
    
    batch_state.vertex_count += 4;
    batch_state.index_count += 6;
}

/**
 * バッチをフラッシュして描画
 */
- (void)flushBatch
{
    if (batch_state.vertex_count == 0) {
        return;
    }
    
    // テクスチャの設定
    if (batch_state.is_textured) {
        glEnable(GL_TEXTURE_2D);
        glBindTexture(GL_TEXTURE_2D, font_texture);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    
    // 頂点バッファにデータをアップロード
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, 
                 sizeof(Vertex) * batch_state.vertex_count, 
                 batch_state.vertices, 
                 GL_DYNAMIC_DRAW);
    
    // 頂点配列の設定
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    if (batch_state.is_textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    
    // 頂点ポインタの設定
    glVertexPointer(2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glColorPointer(4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, r));
    if (batch_state.is_textured) {
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, u));
    }
    
    // インデックスバッファをバインド
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    
    // 描画実行
    int quad_count = batch_state.vertex_count / 4;
    glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_INT, 0);
    
    // 状態をリセット
    batch_state.vertex_count = 0;
    batch_state.index_count = 0;
    
    // バッファをアンバインド
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    
    // クライアント状態を無効化
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

/**
 * バッチバッファのクリーンアップ
 */
- (void)cleanupBatchBuffers
{
    if (vertex_buffer) {
        glDeleteBuffers(1, &vertex_buffer);
        vertex_buffer = 0;
    }
    if (index_buffer) {
        glDeleteBuffers(1, &index_buffer);
        index_buffer = 0;
    }
}

#endif

#pragma mark - MicroUI関連メソッド
/**
 * MicroUIの初期化
 */
- (void)initMicroUI
{
    // エラーをクリア
    while(glGetError() != GL_NO_ERROR);
    
    // OpenGLの初期設定
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    
    // クライアント状態を有効にする
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    // atlas.inlデータをロード前にATLAS定数を表示
    NSLog(@"ATLAS constants: ATLAS_FONT=%d, ATLAS_WIDTH=%d, ATLAS_HEIGHT=%d", 
          ATLAS_FONT, ATLAS_WIDTH, ATLAS_HEIGHT);
    
    // アトラス内の文字情報をデバッグ出力
    for (int i = 0; i < 10; i++) {
        mu_Rect r = atlas[ATLAS_FONT + i];
        NSLog(@"Atlas[%d]: x=%d, y=%d, w=%d, h=%d", 
              ATLAS_FONT + i, r.x, r.y, r.w, r.h);
    }
    
    // フォントテクスチャの初期化
    glGenTextures(1, &font_texture);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"OpenGL Error (glGenTextures): %04X", error);
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, font_texture);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"OpenGL Error (glBindTexture): %04X", error);
        return;
    }
    
    // atlas.inlのデータを直接使用
    // atlas配列とatlas_textureは既にatlas.inlで定義されている
    [self setupSystemFont];
    
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // シャープなピクセルフォント用
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    // MicroUIコンテキストの初期化
    mu_ctx = (mu_Context*)malloc(sizeof(mu_Context));
    if (!mu_ctx) {
        NSLog(@"Failed to allocate MicroUI context");
        return;
    }
    
    mu_init(mu_ctx);
    
    // テキスト幅と高さの計算関数を設定
    mu_ctx->text_width = text_width;
    mu_ctx->text_height = text_height;
    
    // 初期のログメッセージ
    [self writeLog:"MicroUI initialized successfully"];
    logbuf_updated = 1;
    
    NSLog(@"MicroUI initialization completed");
}

/**
 * テキスト幅計算関数
 */
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { 
        len = strlen(text); 
    }
    
    // 最初の数回だけデバッグ情報を出力（成功確認のため10回に増加）
    static int debug_call_count = 0;
    BOOL shouldDebug = (debug_call_count < 10);
    debug_call_count++;
    
    if (shouldDebug) {
        NSLog(@"Calculating width for text: '%.*s' (len=%d)", len, text, len);
    }
    
    int res = 0;
    for (int i = 0; i < len; i++) {
        unsigned char chr = (unsigned char) text[i];
        
        // 制御文字またはASCII範囲外の文字はスキップ
        if (chr < 32 || chr >= 127) {
            if (shouldDebug) {
                NSLog(@"  Skipping character with ASCII %d", chr);
            }
            continue;
        }
        
        // 文字インデックスの計算
        // atlas配列では実際のフォントデータはATLAS_FONT+chrの位置にある
        int idx = ATLAS_FONT + chr;
        int char_width = atlas[idx].w;
        res += char_width;
        
        // 最後の文字以外はスペーシングを追加
        if (i < len - 1) {
            res += 1;
        }
        
        if (shouldDebug) {
            NSLog(@"  Character '%c' (ASCII %d): atlas_idx=%d, width=%d, total_width=%d", 
                  chr, chr, idx, char_width, res);
            
            // スペース文字の特別ログ
            if (chr == ' ') {
                NSLog(@"  *** SPACE CHARACTER: Expected atlas data { 84, 68, 2, 17 }, Got { %d, %d, %d, %d }", 
                      atlas[idx].x, atlas[idx].y, atlas[idx].w, atlas[idx].h);
            }
        }
    }
    
    if (shouldDebug) {
        NSLog(@"Final text width: %d", res);
    }
    
    return res;
}

/**
 * テキスト高さ計算関数
 */
static int text_height(mu_Font font) {
    return 17; // atlas.inlのフォントは17ピクセル高
}

/**
 * ログウィンドウにテキストを追加
 */
- (void)writeLog:(const char*)text {
    if (logbuf[0]) { 
        strcat(logbuf, "\n"); 
    }
    strcat(logbuf, text);
    logbuf_updated = 1;
}

- (void)setupSystemFont {
    // atlas.inlの正しいフォントテクスチャを使用
    NSLog(@"atlas.inlのフォントテクスチャを使用します");
    
    // アトラステクスチャが正しく定義されているかチェック
    BOOL hasValidData = NO;
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            break;
        }
    }
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
        return;
    }
    
    // atlas.inlの正しいテクスチャデータをアップロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
    
    // 重要：スペース文字（ASCII 32）を含む最初の数文字の詳細情報を出力
    NSLog(@"Atlas font data loaded successfully");
    NSLog(@"ATLAS_FONT constant value: %d", ATLAS_FONT);
    
    for (int i = 0; i < 15; i++) {
        mu_Rect r = atlas[ATLAS_FONT + i];
        char c = i + 32;
        NSLog(@"Character '%c' (ASCII %d, idx=%d): x=%d, y=%d, w=%d, h=%d", 
              c, (int)c, ATLAS_FONT + i, r.x, r.y, r.w, r.h);
    }
}

- (void)setupFallbackFont {
    // 文字サイズを大きくする（8x8から10x16に）
    int charWidth = 12;
    int charHeight = 18;
    int charsPerRow = 16;
    
    // フォントアトラス情報を作成（ASCII 32-126）
    for (int i = 0; i < 95; i++) {
        int idx = ATLAS_FONT + i;
        int row = i / charsPerRow;
        int col = i % charsPerRow;
        
        atlas[idx].x = col * charWidth;
        atlas[idx].y = row * charHeight;
        atlas[idx].w = charWidth;
        atlas[idx].h = charHeight;
    }
    
    NSLog(@"フォールバックフォント情報を設定しました (サイズ: %dx%d)", charWidth, charHeight);
    
    // テクスチャサイズを計算（16列 x 6行でASCII文字をカバー）
    int texWidth = charsPerRow * charWidth;  // 160
    int texHeight = 6 * charHeight;         // 96
    
    // 簡易フォントテクスチャの生成
    unsigned char* fontTexture = calloc(texWidth * texHeight, sizeof(unsigned char));
    if (!fontTexture) {
        NSLog(@"フォントテクスチャのメモリ確保に失敗しました");
        return;
    }
    
    // 各文字に異なるパターンを生成
    for (int charIdx = 0; charIdx < 95; charIdx++) {
        char c = charIdx + 32; // ASCII 32から開始
        int row = charIdx / charsPerRow;
        int col = charIdx % charsPerRow;
        int baseX = col * charWidth;
        int baseY = row * charHeight;
        
        // 文字の形状をより認識しやすく生成
        for (int y = 0; y < charHeight; y++) {
            for (int x = 0; x < charWidth; x++) {
                int pixelValue = 0;
                int texPos = (baseY + y) * texWidth + baseX + x;
                
                if (texPos >= 0 && texPos < texWidth * texHeight) {
                    // 基本文字形状を作成
                    switch (c) {
                        case ' ':  // スペース
                            // スペースは何も描画しない
                            break;
                            
                        case '!':  // 感嘆符
                            if (x >= 4 && x <= 5 && ((y >= 2 && y <= 10) || (y >= 12 && y <= 13))) {
                                pixelValue = 255;
                            }
                            break;
                            
                        case '.':  // ピリオド
                            if (x >= 4 && x <= 5 && y >= 12 && y <= 13) {
                                pixelValue = 255;
                            }
                            break;
                            
                        case ',':  // カンマ
                            if (x >= 4 && x <= 5 && y >= 12 && y <= 14) {
                                pixelValue = 255;
                            }
                            break;
                            
                        case ':':  // コロン
                            if (x >= 4 && x <= 5 && ((y >= 5 && y <= 6) || (y >= 12 && y <= 13))) {
                                pixelValue = 255;
                            }
                            break;
                            
                        case '0': case '1': case '2': case '3': case '4':
                        case '5': case '6': case '7': case '8': case '9':
                            // 数字の場合
                            if (c == '0') {
                                // 数字0は楕円形
                                if ((x == 2 || x == charWidth-3) && (y >= 3 && y <= charHeight-4) ||
                                    (y == 2 || y == charHeight-3) && (x >= 3 && x <= charWidth-4)) {
                                    pixelValue = 255;
                                }
                            } else if (c == '1') {
                                // 数字1は縦線と斜め線
                                if ((x == 5 && y >= 2 && y <= charHeight-3) ||
                                    (y == charHeight-3 && x >= 3 && x <= 7) ||
                                    (x == 4 && y == 3) || (x == 3 && y == 4)) {
                                    pixelValue = 255;
                                }
                            } else {
                                // その他の数字は枠+内部パターン
                                if ((x == 2 || x == charWidth-3) && (y >= 3 && y <= charHeight-4) ||
                                    (y == 2 || y == charHeight-3) && (x >= 3 && x <= charWidth-4)) {
                                    pixelValue = 200;
                                }
                                
                                // 中央に数字ごとに異なるパターンを追加
                                int digit = c - '0';
                                if ((digit % 3 == 0 && x == 5 && (y == 5 || y == 10)) ||
                                    (digit % 3 == 1 && y == 8 && (x == 3 || x == 7)) ||
                                    (digit % 3 == 2 && ((x == 3 && y == 5) || (x == 7 && y == 10)))) {
                                    pixelValue = 255;
                                }
                            }
                            break;
                            
                        default:
                            // アルファベットなど他の文字
                            if (c >= 'A' && c <= 'Z') {
                                // 大文字アルファベット
                                int letterIndex = c - 'A';
                                
                                // すべての文字に枠を描画
                                if ((x == 2 || x == charWidth-3) && (y >= 3 && y <= charHeight-4) ||
                                    (y == 2) && (x >= 3 && x <= charWidth-4)) {
                                    pixelValue = 200;
                                }
                                
                                // 文字ごとに特徴的なパターンを追加
                                switch (letterIndex % 6) {
                                    case 0: // A, G, M, S, Y
                                        if ((y == 7 && x >= 3 && x <= 7) || 
                                            (y == charHeight-3 && x >= 3 && x <= charWidth-4)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 1: // B, H, N, T, Z
                                        if ((x == 5 && y >= 3 && y <= charHeight-4) || 
                                            (y == 7 && x >= 3 && x <= 7)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 2: // C, I, O, U
                                        if ((y == 7 && x >= 3 && x <= 7) || 
                                            (y == charHeight-3 && x >= 3 && x <= charWidth-4)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 3: // D, J, P, V
                                        if ((x == 3 && y >= 3 && y <= charHeight-4) || 
                                            (y == 5 && x >= 4 && x <= 6)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 4: // E, K, Q, W
                                        if ((y == 4 && x >= 3 && x <= 7) || 
                                            (y == 8 && x >= 3 && x <= 6) ||
                                            (y == 12 && x >= 3 && x <= 7)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 5: // F, L, R, X
                                        if ((x == 3 && y >= 3 && y <= charHeight-4) || 
                                            (y == 5 && x >= 4 && x <= 7)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                }
                            } else if (c >= 'a' && c <= 'z') {
                                // 小文字アルファベット
                                int letterIndex = c - 'a';
                                
                                // すべての小文字に基本形状
                                if ((x == 2 && y >= 6 && y <= charHeight-4) ||
                                    (y == charHeight-3 && x >= 3 && x <= 6)) {
                                    pixelValue = 200;
                                }
                                
                                // 文字ごとに特徴的なパターンを追加
                                switch (letterIndex % 5) {
                                    case 0: // a, f, k, p, u
                                        if (y == 8 && x >= 3 && x <= 6) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 1: // b, g, l, q, v
                                        if (x == 5 && y >= 7 && y <= 12) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 2: // c, h, m, r, w
                                        if ((x == 3 && y == 9) || (x == 6 && y == 9)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 3: // d, i, n, s, x
                                        if (y == 10 && x >= 3 && x <= 6) {
                                            pixelValue = 255;
                                        }
                                        break;
                                        
                                    case 4: // e, j, o, t, y, z
                                        if ((x == 4 && y >= 7 && y <= 12) || (y == 9 && x == 6)) {
                                            pixelValue = 255;
                                        }
                                        break;
                                }
                            } else {
                                // その他の記号
                                if ((x == 2 || x == charWidth-3) && (y == 5 || y == charHeight-6) ||
                                    (y == 2 || y == charHeight-3) && (x == 4 || x == charWidth-5)) {
                                    pixelValue = 200;
                                }
                                
                                // 記号ごとに特徴を追加
                                int symbolIdx = (int)c % 4;
                                switch (symbolIdx) {
                                    case 0:
                                        if (x == 5 && y == 8) pixelValue = 255;
                                        break;
                                    case 1:
                                        if (y == 8 && x >= 3 && x <= 7) pixelValue = 255;
                                        break;
                                    case 2:
                                        if (x == 5 && y >= 6 && y <= 10) pixelValue = 255;
                                        break;
                                    case 3:
                                        if ((x == 3 && y == 6) || (x == 7 && y == 10)) pixelValue = 255;
                                        break;
                                }
                            }
                            break;
                    }
                    
                    fontTexture[texPos] = pixelValue;
                }
            }
        }
    }
    
    // テクスチャをアップロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, texWidth, texHeight, 0,
                GL_ALPHA, GL_UNSIGNED_BYTE, fontTexture);
    
    // メモリ解放
    free(fontTexture);
    
    NSLog(@"詳細なフォールバックフォントを生成しました");
}

/**
 * MicroUIフレームの処理（簡素化版）
 */
- (void)processMicroUIFrame
{
    // MicroUIの開始
    mu_begin(mu_ctx);
    
    // テストウィンドウの描画
    [self drawTestWindow];
    
    // ログウィンドウの描画
    [self drawLogWindow];
    [self drawStyleWindow];
    // MicroUIの終了
    mu_end(mu_ctx);
}
/**
 * テストウィンドウの描画
 */
- (void)drawTestWindow
{
    // ウィンドウの開始
    if (mu_begin_window(mu_ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
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

        win = mu_get_current_container(mu_ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);

        // ウィンドウ位置の変更を検出してログ出力
        if (win->rect.x != prevWindowX || win->rect.y != prevWindowY) {
            NSLog(@"[ウィンドウ移動] 移動後の座標: x=%d, y=%d (前回: x=%d, y=%d)", 
                  win->rect.x, win->rect.y, prevWindowX, prevWindowY);
            
            // ログバッファにも記録
            char logMessage[128];
            sprintf(logMessage, "Window moved to x=%d, y=%d", win->rect.x, win->rect.y);
            [self writeLog:logMessage];
            
            // 前回の位置を更新
            prevWindowX = win->rect.x;
            prevWindowY = win->rect.y;
        }

        // ウィンドウ情報
        if (mu_header(mu_ctx, "Window Info")) {
            win = mu_get_current_container(mu_ctx);
            mu_layout_row(mu_ctx, 2, layout1, 0);
            mu_label(mu_ctx, "Position:");
            sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(mu_ctx, buf);
            mu_label(mu_ctx, "Size:");
            sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(mu_ctx, buf);
        }

        // ラベルとボタン
        if (mu_header_ex(mu_ctx, "Test Buttons", MU_OPT_EXPANDED)) {
            mu_layout_row(mu_ctx, 3, layout2, 0);
            mu_label(mu_ctx, "Test buttons 1:");
            if (mu_button(mu_ctx, "Button 1")) { [self writeLog:"Pressed button 1"]; }
            if (mu_button(mu_ctx, "Button 2")) { [self writeLog:"Pressed button 2"]; }
            mu_label(mu_ctx, "Test buttons 2:");
            if (mu_button(mu_ctx, "Button 3")) { [self writeLog:"Pressed button 3"]; }
            if (mu_button(mu_ctx, "Popup")) { mu_open_popup(mu_ctx, "Test Popup"); }
            if (mu_begin_popup(mu_ctx, "Test Popup")) {
                mu_button(mu_ctx, "Hello");
                mu_button(mu_ctx, "World");
                mu_end_popup(mu_ctx);
            }
        }

        // ツリーとテキスト
        if (mu_header_ex(mu_ctx, "Tree and Text", MU_OPT_EXPANDED)) {
            mu_layout_row(mu_ctx, 2, layout3, 0);
            mu_layout_begin_column(mu_ctx);
            if (mu_begin_treenode(mu_ctx, "Test 1")) {
                if (mu_begin_treenode(mu_ctx, "Test 1a")) {
                    mu_label(mu_ctx, "Hello");
                    mu_label(mu_ctx, "world");
                    mu_end_treenode(mu_ctx);
                }
                if (mu_begin_treenode(mu_ctx, "Test 1b")) {
                    if (mu_button(mu_ctx, "Button 1")) { [self writeLog:"Pressed button 1"]; }
                    if (mu_button(mu_ctx, "Button 2")) { [self writeLog:"Pressed button 2"]; }
                    mu_end_treenode(mu_ctx);
                }
                mu_end_treenode(mu_ctx);
            }
            if (mu_begin_treenode(mu_ctx, "Test 2")) {
                mu_layout_row(mu_ctx, 2, layout4, 0);
                if (mu_button(mu_ctx, "Button 3")) { [self writeLog:"Pressed button 3"]; }
                if (mu_button(mu_ctx, "Button 4")) { [self writeLog:"Pressed button 4"]; }
                if (mu_button(mu_ctx, "Button 5")) { [self writeLog:"Pressed button 5"]; }
                if (mu_button(mu_ctx, "Button 6")) { [self writeLog:"Pressed button 6"]; }
                mu_end_treenode(mu_ctx);
            }
            if (mu_begin_treenode(mu_ctx, "Test 3")) {
                static int checks[3] = { 1, 0, 1 };
                mu_checkbox(mu_ctx, "Checkbox 1", &checks[0]);
                mu_checkbox(mu_ctx, "Checkbox 2", &checks[1]);
                mu_checkbox(mu_ctx, "Checkbox 3", &checks[2]);
                mu_end_treenode(mu_ctx);
            }
            mu_layout_end_column(mu_ctx);

            mu_layout_begin_column(mu_ctx);
            mu_layout_row(mu_ctx, 1, layout5, 0);
            mu_text(mu_ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
                "elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
                "ipsum, eu varius magna felis a nulla.");
            mu_layout_end_column(mu_ctx);
        }

        // 背景色スライダー
        if (mu_header_ex(mu_ctx, "Background Color", MU_OPT_EXPANDED)) {
            mu_layout_row(mu_ctx, 2, layout6, 74);
            // スライダー
            mu_layout_begin_column(mu_ctx);
            mu_layout_row(mu_ctx, 2, layout7, 0);
            mu_label(mu_ctx, "Red:"); mu_slider(mu_ctx, &bg[0], 0, 255);
            mu_label(mu_ctx, "Green:"); mu_slider(mu_ctx, &bg[1], 0, 255);
            mu_label(mu_ctx, "Blue:"); mu_slider(mu_ctx, &bg[2], 0, 255);
            mu_layout_end_column(mu_ctx);
            // カラープレビュー
            r = mu_layout_next(mu_ctx);
            mu_draw_rect(mu_ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(mu_ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }

        mu_end_window(mu_ctx);
    }
}

/**
 * ログウィンドウの描画
 */
- (void)drawLogWindow
{
    if (mu_begin_window(mu_ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        // テキスト出力パネル
        mu_Container* panel;
        static char buf[128];
        int submitted = 0;
        int layout1[1] = { -1 };
        int layout2[2] = { -70, -1 };

        mu_layout_row(mu_ctx, 1, layout1, -25);
        mu_begin_panel(mu_ctx, "Log Output");
        panel = mu_get_current_container(mu_ctx);
        mu_layout_row(mu_ctx, 1, layout1, -1);
        mu_text(mu_ctx, logbuf);
        mu_end_panel(mu_ctx);
        if (logbuf_updated) {
            panel->scroll.y = panel->content_size.y;
            logbuf_updated = 0;
        }

        // テキストボックスとサブミットボタン
        mu_layout_row(mu_ctx, 2, layout2, 0);
        if (mu_textbox(mu_ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) {
            mu_set_focus(mu_ctx, mu_ctx->last_id);
            submitted = 1;
        }
        if (mu_button(mu_ctx, "Submit")) { submitted = 1; }
        if (submitted) {
            [self writeLog:buf];
            buf[0] = '\0';
        }

        mu_end_window(mu_ctx);
    }
}

/**
 * uint8スライダーの描画ヘルパー関数
 */
static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
    static float tmp;
    int res;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = tmp;
    mu_pop_id(ctx);
    return res;
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

    if (mu_begin_window(mu_ctx, "Style Editor", mu_rect(350, 250, 300, 240))) {
        int sw = mu_get_current_container(mu_ctx)->body.w * 0.14;
        int layout[6] = { 80, sw, sw, sw, sw, -1 };
        int i;

        mu_layout_row(mu_ctx, 6, layout, 0);
        for (i = 0; colors[i].label; i++) {
            mu_label(mu_ctx, colors[i].label);
            uint8_slider(mu_ctx, &mu_ctx->style->colors[i].r, 0, 255);
            uint8_slider(mu_ctx, &mu_ctx->style->colors[i].g, 0, 255);
            uint8_slider(mu_ctx, &mu_ctx->style->colors[i].b, 0, 255);
            uint8_slider(mu_ctx, &mu_ctx->style->colors[i].a, 0, 255);
            mu_draw_rect(mu_ctx, mu_layout_next(mu_ctx), mu_ctx->style->colors[i]);
        }
        mu_end_window(mu_ctx);
    }
}

/**
 * MicroUI描画関数
 * MicroUIのコマンドを処理して描画します
 */
- (void)drawMicroUI
{
    // MicroUIのコンテキストがあるかチェック
    if (!mu_ctx) {
        NSLog(@"MicroUI context is not initialized");
        return;
    }

    NSRect bounds = [self bounds];
    int viewWidth = bounds.size.width;
    int viewHeight = bounds.size.height;
    
    // Retinaディスプレイ対応
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    // OpenGL状態の設定（プロジェクション行列はdrawViewで設定済み）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    
    // MicroUIコマンドの処理
    mu_Command* cmd = NULL;
    while (mu_next_command(mu_ctx, &cmd)) {
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
                [self setClipRect:cmd->clip.rect viewHeight:viewHeight];
                break;
                
            default:
                NSLog(@"Unknown MicroUI command type: %d", cmd->type);
                break;
        }
    }
    
#if USE_QUAD
    // 最後にバッチをフラッシュ
    [self flushBatch];
#endif
    
    // クリッピングを解除
    glDisable(GL_SCISSOR_TEST);
}

/**
 * テキストの描画
 */
// drawTextメソッドの修正
- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color
{
    if (!text || !text[0]) return;
    
#if USE_QUAD
    // バッチ処理版
    const char* p;
    unsigned char chr;
    mu_Rect src, dst = { pos.x, pos.y, 0, 0 };
    
    int char_count = 0;
    int total_chars = strlen(text);
    
    for (p = text; *p; p++, char_count++) {
        chr = (unsigned char)*p;
        
        if (chr < 32 || chr >= 127) {
            continue;
        }
        
        int idx = ATLAS_FONT + chr;
        src = atlas[idx];
        dst.w = src.w;
        dst.h = src.h;
        
        if (chr == ' ') {
            // スペースは描画せずに位置だけ移動
            dst.x += dst.w;
            if (char_count < total_chars - 1) {
                dst.x += 1;
            }
            continue;
        }
        
        // バッチにクアッドを追加
        [self pushQuad:dst src:src color:color textured:YES];
        
        // 次の文字位置へ移動
        dst.x += dst.w;
        if (char_count < total_chars - 1) {
            dst.x += 1;
        }
    }
    
#else
    // 従来の即座描画版
    static int debug_call_count = 0;
    BOOL shouldDebug = (debug_call_count < 10);
    debug_call_count++;
    
    if (shouldDebug) {
        NSLog(@"Drawing text: '%s' at position (%d, %d)", text, pos.x, pos.y);
    }
    
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // フォントテクスチャをバインド
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    // アンチエイリアスを有効化
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    const char* p;
    unsigned char chr;
    mu_Rect src;
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    
    int char_count = 0;
    int total_chars = strlen(text);
    
    for (p = text; *p; p++, char_count++) {
        // ASCII値を取得
        chr = (unsigned char)*p;
        
        // 制御文字またはASCII範囲外の文字はスキップ
        if (chr < 32 || chr >= 127) {
            if (shouldDebug) {
                NSLog(@"Skipping character with ASCII %d (out of range)", chr);
            }
            continue;
        }
        
        // アトラスインデックスを計算
        // atlas配列では実際のフォントデータはATLAS_FONT+chrの位置にある
        int idx = ATLAS_FONT + chr;
        
        // アトラスから文字の矩形情報を取得
        src = atlas[idx];
        dst.w = src.w;
        dst.h = src.h;
        
        if (shouldDebug) {
            NSLog(@"Character '%c' (ASCII %d): atlas_idx=%d, src=(%d,%d,%d,%d), dst_pos=(%d,%d)", 
                  chr, chr, idx, src.x, src.y, src.w, src.h, dst.x, dst.y);
            
            // スペース文字の特別ログ
            if (chr == ' ') {
                NSLog(@"*** SPACE CHARACTER: Expected { 84, 68, 2, 17 }, Got { %d, %d, %d, %d }", 
                      src.x, src.y, src.w, src.h);
            }
        }
        
        // スペース文字の特別処理
        if (chr == ' ') {
            // スペースは描画せずに位置だけ移動
            dst.x += dst.w;
            // 最後の文字以外はスペーシングを追加
            if (char_count < total_chars - 1) {
                dst.x += 1;
            }
            if (shouldDebug) {
                NSLog(@"Space character: moving position by %d pixels", dst.w + (char_count < total_chars - 1 ? 1 : 0));
            }
            continue;
        }
        
        // テクスチャ座標の計算（ATLAS_WIDTHとATLAS_HEIGHTを使用）
        float tx = (float)src.x / ATLAS_WIDTH;
        float ty = (float)src.y / ATLAS_HEIGHT;
        float tw = (float)src.w / ATLAS_WIDTH;
        float th = (float)src.h / ATLAS_HEIGHT;
        
        // 頂点とテクスチャ座標を設定してクアッドを描画
        glBegin(GL_QUADS);
        glColor4f(r, g, b, a);
        
        // 左上
        glTexCoord2f(tx, ty);
        glVertex2f(dst.x, dst.y);
        
        // 右上
        glTexCoord2f(tx + tw, ty);
        glVertex2f(dst.x + dst.w, dst.y);
        
        // 右下
        glTexCoord2f(tx + tw, ty + th);
        glVertex2f(dst.x + dst.w, dst.y + dst.h);
        
        // 左下
        glTexCoord2f(tx, ty + th);
        glVertex2f(dst.x, dst.y + dst.h);
        
        glEnd();
        
        // 次の文字位置へ移動
        dst.x += dst.w;
        // 最後の文字以外はスペーシングを追加
        if (char_count < total_chars - 1) {
            dst.x += 1;
        }
    }
    
    glDisable(GL_TEXTURE_2D);
#endif
}

/**
 * 矩形の描画
 */
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color
{
#if USE_QUAD
    // バッチ処理版
    mu_Rect src = {0, 0, 1, 1}; // ダミーのテクスチャ座標
    [self pushQuad:rect src:src color:color textured:NO];
    
#else
    // 従来の即座描画版
    float x = rect.x;
    float y = rect.y;
    float w = rect.w;
    float h = rect.h;
    float r = color.r / 255.0;
    float g = color.g / 255.0;
    float b = color.b / 255.0;
    float a = color.a / 255.0;

    glDisable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
#endif
}

/**
 * アイコンの描画
 */
- (void)drawIcon:(int)id rect:(mu_Rect)rect color:(mu_Color)color
{
    if (id < 0) return;
    
    // アイコンの情報を取得（アイコンIDを直接使用）
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    mu_Rect dst = { x, y, src.w, src.h };
    
#if USE_QUAD
    // バッチ処理版
    [self pushQuad:dst src:src color:color textured:YES];
    
#else
    // 従来の即座描画版
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // テクスチャ座標の計算
    float tx = (float)src.x / 128.0f; // ATLAS_WIDTH
    float ty = (float)src.y / 128.0f; // ATLAS_HEIGHT
    float tw = (float)src.w / 128.0f;
    float th = (float)src.h / 128.0f;
    
    // テクスチャをバインド
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    
    // 左上
    glTexCoord2f(tx, ty);
    glVertex2f(dst.x, dst.y);
    
    // 右上
    glTexCoord2f(tx + tw, ty);
    glVertex2f(dst.x + dst.w, dst.y);
    
    // 右下
    glTexCoord2f(tx + tw, ty + th);
    glVertex2f(dst.x + dst.w, dst.y + dst.h);
    
    // 左下
    glTexCoord2f(tx, ty + th);
    glVertex2f(dst.x, dst.y + dst.h);
    
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
#endif
}

/**
 * クリップ矩形の設定
 */
- (void)setClipRect:(mu_Rect)rect viewHeight:(int)viewHeight
{
    // OpenGLの座標系に合わせてクリップを設定
    glScissor(rect.x, viewHeight - rect.y - rect.h, rect.w, rect.h);
    glEnable(GL_SCISSOR_TEST);
    
    // デバッグ情報
    static int debug_counter = 0;
    // if (debug_counter++ % 100 == 0) {
    //     NSLog(@"Setting clip rect: x=%d, y=%d, w=%d, h=%d", 
    //           rect.x, viewHeight - rect.y - rect.h, rect.w, rect.h);
    // }
}

/**
 * マウスイベント処理 - X11のシンプルな実装を参考にする
 */
- (void)mouseDown:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    
    // MicroUIにイベントを送信
    mu_input_mousemove(mu_ctx, (int)point.x, (int)point.y);
    mu_input_mousedown(mu_ctx, (int)point.x, (int)point.y, MU_MOUSE_LEFT);
    
    NSLog(@"[mouseDown] 座標: (%.1f, %.1f)", point.x, point.y);
    
    // 即座に次のフレームを要求
    [self setNeedsDisplay:YES];
}

- (void)mouseUp:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    
    // MicroUIにイベントを送信
    mu_input_mousemove(mu_ctx, (int)point.x, (int)point.y);
    mu_input_mouseup(mu_ctx, (int)point.x, (int)point.y, MU_MOUSE_LEFT);
    
    NSLog(@"[mouseUp] 座標: (%.1f, %.1f)", point.x, point.y);
    
    // 即座に次のフレームを要求
    [self setNeedsDisplay:YES];
}

- (void)mouseMoved:(NSEvent *)event
{
    NSPoint point = [self normalizeMousePoint:event];
    
    // MicroUIにマウス移動を送信
    mu_input_mousemove(mu_ctx, (int)point.x, (int)point.y);
}

- (void)mouseDragged:(NSEvent *)event
{
    // mouseDraggedはmouseMovedと同じ処理
    [self mouseMoved:event];
    
    // ドラッグ中なので描画更新
    [self setNeedsDisplay:YES];
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
        mu_input_keydown(mu_ctx, MU_KEY_SHIFT);
    }
    if ([event modifierFlags] & NSEventModifierFlagControl) {
        mu_input_keydown(mu_ctx, MU_KEY_CTRL);
    }
    if ([event modifierFlags] & NSEventModifierFlagOption) {
        mu_input_keydown(mu_ctx, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keydown(mu_ctx, MU_KEY_RETURN);
    } else if (keyCode == 51) { // Backspace
        mu_input_keydown(mu_ctx, MU_KEY_BACKSPACE);
    }
    
    // テキスト入力
    mu_input_text(mu_ctx, [characters UTF8String]);
}

- (void)keyUp:(NSEvent *)event
{
    unsigned short keyCode = [event keyCode];
    
    // 修飾キーのチェック
    if (!([event modifierFlags] & NSEventModifierFlagShift)) {
        mu_input_keyup(mu_ctx, MU_KEY_SHIFT);
    }
    if (!([event modifierFlags] & NSEventModifierFlagControl)) {
        mu_input_keyup(mu_ctx, MU_KEY_CTRL);
    }
    if (!([event modifierFlags] & NSEventModifierFlagOption)) {
        mu_input_keyup(mu_ctx, MU_KEY_ALT);
    }
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keyup(mu_ctx, MU_KEY_RETURN);
    } else if (keyCode == 51) { // Backspace
        mu_input_keyup(mu_ctx, MU_KEY_BACKSPACE);
    }
}

/**
 * イベントを処理するメソッド
 * キーボードイベントを処理するためにファーストレスポンダーになります
 */
- (BOOL)acceptsFirstResponder
{
    return YES;
}

/**
 * マウス座標の正規化（共通処理）
 */
- (NSPoint)normalizeMousePoint:(NSEvent *)event
{
    NSPoint point = [self convertPoint:[event locationInWindow] fromView:nil];
    NSRect bounds = [self bounds];
    
    // Retinaディスプレイ対応
    CGFloat scaleFactor = 1.0;
    if ([self wantsBestResolutionOpenGLSurface]) {
        scaleFactor = [[self window] backingScaleFactor];
    }
    
    // スケールファクターを適用
    point.x *= scaleFactor;
    point.y *= scaleFactor;
    
    // Y座標を反転（OpenGL座標系）
    CGFloat viewHeight = bounds.size.height * scaleFactor;
    point.y = viewHeight - point.y;
    
    return point;
}

@end

// アプリケーションデリゲート
@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (strong) NSWindow *window;
@property (strong) MicroUIMacView *glView;
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
    
    [self.window setTitle:@"MicroUI OpenGL Demo"];
    [self.window setMinSize:NSMakeSize(200, 200)];
    
    // OpenGLビューの作成
    self.glView = [[MicroUIMacView alloc] initWithFrame:[[self.window contentView] bounds]];
    [self.glView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    
    // ビューをウィンドウに追加
    [self.window setContentView:self.glView];
    
    // ウィンドウの表示
    [self.window center];
    [self.window makeKeyAndOrderFront:nil];
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender
{
    return YES;
}

@end

// メイン関数
int main(int argc, const char * argv[]) {
    @autoreleasepool {
        NSApplication *application = [NSApplication sharedApplication];
        AppDelegate *delegate = [[AppDelegate alloc] init];
        [application setDelegate:delegate];
        [application run];
    }
    return 0;
}
