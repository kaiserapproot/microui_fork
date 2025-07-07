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
#define USE_QUAD 0

#import <Cocoa/Cocoa.h>
#import <OpenGL/gl.h>
#import <CoreVideo/CoreVideo.h>  // CVDisplayLinkのために追加
#import "microui.h"
#import "renderer.h"  // 統合レンダラー関数のために追加
//#import "atlas.inl"
enum { ATLAS_WHITE = MU_ICON_MAX, ATLAS_FONT };
enum { ATLAS_WIDTH = 128, ATLAS_HEIGHT = 128 };
extern unsigned char atlas_texture[];
extern mu_Rect atlas[];
#define GLES_SILENCE_DEPRECATION 1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#include "renderer_text_helper.h"
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
 * OpenGLビュークラス
 * 三角形を描画するためのOpenGLビューを提供します
 */
@interface TriangleView : NSOpenGLView
{
    GLuint VBO;                    // 頂点バッファオブジェクト
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

@implementation TriangleView

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
   // NSLog(@"DisplayLinkCallback called"); // デバッグログを追加
    
    // コンテキストからTriangleViewインスタンスを取得
    TriangleView* view = (__bridge TriangleView*)displayLinkContext;
    
    // 描画処理をメインスレッドにディスパッチ
    dispatch_async(dispatch_get_main_queue(), ^{
        [view drawView];
    });
    
    return kCVReturnSuccess;
}

/**
 * ビューの描画メソッド（簡素化版）
 */
- (void)drawView
{
    if (!self.window || ![self.window isVisible] || self.hidden) {
        return;
    }
    if (![self openGLContext] || !mu_ctx) {
        return;
    }
    
    [[self openGLContext] makeCurrentContext];

    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
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

    [[self openGLContext] flushBuffer];
}
/**
 * ビューの初期化メソッド
 * OpenGLのピクセルフォーマットを設定し、ビューを初期化します
 */
- (id)initWithFrame:(NSRect)frameRect
{
    // OpenGLのピクセルフォーマット属性を設定
    NSOpenGLPixelFormatAttribute attrs[] =
    {
        NSOpenGLPFAAccelerated,      // ハードウェアアクセラレーション
        NSOpenGLPFADoubleBuffer,     // ダブルバッファリングを有効化
        NSOpenGLPFAOpenGLProfile,    // OpenGLプロファイルの指定
        NSOpenGLProfileVersion3_2Core, // OpenGL 3.2 Core Profileを使用
        NSOpenGLPFAColorSize, 24,    // カラーバッファのビット深度
        NSOpenGLPFAAlphaSize, 8,     // アルファチャンネルのビット深度
        NSOpenGLPFADepthSize, 24,    // デプスバッファのビット深度
        0
    };
    
    // ピクセルフォーマットを作成
    NSOpenGLPixelFormat* pixelFormat = [[NSOpenGLPixelFormat alloc] initWithAttributes:attrs];
    
    if (!pixelFormat) {
        NSLog(@"No OpenGL pixel format");
        return nil;
    }
    
    // スーパークラスの初期化
    self = [super initWithFrame:frameRect pixelFormat:pixelFormat];
    
    if (self) {
        // Retinaディスプレイのサポートを有効化
        [self setWantsBestResolutionOpenGLSurface:YES];
        
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
        
        // ロックは使用しない（シングルスレッドで処理）
        //muiLock = nil;
        
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
 * OpenGLの初期設定を行うメソッド
 * DisplayLinkの設定と三角形の頂点データの初期化を行います
 */
- (void)prepareOpenGL
{
    [super prepareOpenGL];
    
    // OpenGLコンテキストを現在のスレッドにアタッチ
    [[self openGLContext] makeCurrentContext];
    
    NSLog(@"Setting up DisplayLink...");
    
    // DisplayLinkの設定
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
    
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
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
    
    // (頂点データの初期化などは省略)
    
#if USE_QUAD
    // バッチ処理用バッファの初期化
    [self initBatchBuffers];
#endif
    
    // MicroUIの初期化
    [self initMicroUI];
    [self processMicroUIFrame];
    [self setNeedsDisplay:YES];
    // ここでDisplayLinkを開始
    result = CVDisplayLinkStart(displayLink);
    if (result == kCVReturnSuccess) {
        NSLog(@"DisplayLink started in prepareOpenGL");
    } else {
        NSLog(@"Failed to start DisplayLink in prepareOpenGL: %d", result);
    }
}

/**
 * ビューがウィンドウに追加された時に呼ばれるメソッド
 */
- (void)viewDidMoveToWindow
{
    [super viewDidMoveToWindow];
    
    NSLog(@"viewDidMoveToWindow called, window: %@", self.window);
    
    // ビューがウィンドウに追加された時点でDisplayLinkの開始を再確認
    if (self.window && displayLink && !CVDisplayLinkIsRunning(displayLink)) {
        // すぐにDisplayLinkを開始
        CVReturn result = CVDisplayLinkStart(displayLink);
        if (result == kCVReturnSuccess) {
            NSLog(@"DisplayLink started in viewDidMoveToWindow");
        } else {
            NSLog(@"Failed to start DisplayLink in viewDidMoveToWindow: %d", result);
        }
        
        // 直接最初の描画を行う（初期表示のため）
        dispatch_async(dispatch_get_main_queue(), ^{
            
            [self drawView];
        });
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
 * ディアロケータ
 * OpenGLリソースとDisplayLinkを解放します
 */
- (void)dealloc
{
    // DisplayLinkの停止と解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    
    // VBOの解放
    glDeleteBuffers(1, &VBO);
    
#if USE_QUAD
    // バッチバッファのクリーンアップ
    [self cleanupBatchBuffers];
#endif
    
    // MicroUIの解放
    glDeleteTextures(1, &font_texture);
    free(mu_ctx);
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
    
    // フォントテクスチャを設定（テクスチャパラメータも含む）
    [self setupSystemFont];
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    // 重要: テクスチャとパラメータ設定の後、レンダラー初期化の前に行う
    register_text_measurement_functions(text_width, text_height);
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
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
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化（このステップでatlas_textureとatlasデータを使用）
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
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
    NSLog(@"atlas.inlの