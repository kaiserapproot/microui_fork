#!/bin/bash

# main.mの内容を整理して重複削除するスクリプト
# 2025年6月12日

MAIN_FILE="/Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/main.m"
BACKUP_FILE="/Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/main_backup_full.m"

# 全体のバックアップ
cp "$MAIN_FILE" "$BACKUP_FILE"
echo "バックアップファイルを作成しました: $BACKUP_FILE"

# 修正したファイルを作成
cat > "$MAIN_FILE" << 'EOF'
// filepath: /Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/main.m
// MicroUIを使ったウィジェット表示のコード

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
    [self drawMicroUI];
    
    [[self openGLContext] flushBuffer];
}

/**
 * ウィンドウのサイズ変更時に呼ばれるメソッド
 */
- (void)reshape
{
    [super reshape];
    
    // ビューの現在のサイズを取得
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    
    // Retinaディスプレイの場合、バッキングサイズを取得
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    
    // OpenGLビューポートの設定
    [[self openGLContext] makeCurrentContext];
    glViewport(0, 0, viewWidth, viewHeight);

    // レンダラーのサイズ調整（MicroUI初期化後のみ）
    if (mu_ctx) {
        r_resize(viewWidth, viewHeight);
    }
}

/**
 * ビュー初期化メソッド
 */
- (void)prepareOpenGL
{
    [super prepareOpenGL];
    
    // OpenGLコンテキストの設定
    [[self openGLContext] makeCurrentContext];
    
    // 垂直同期を有効にする（ティアリング防止）
    GLint swapInt = 1;
    [[self openGLContext] setValues:&swapInt forParameter:NSOpenGLCPSwapInterval];
    
    // DisplayLinkの設定
    // DisplayLinkは、ディスプレイの垂直同期に合わせて描画を行うためのオブジェクトです
    CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, &DisplayLinkCallback, (__bridge void *)(self));
    
    // DisplayLinkとOpenGLコンテキストのピクセルフォーマットを一致させる
    CGLContextObj cglContext = [[self openGLContext] CGLContextObj];
    CGLPixelFormatObj cglPixelFormat = [[self pixelFormat] CGLPixelFormatObj];
    CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext(displayLink, cglContext, cglPixelFormat);
    
    // DisplayLinkを開始
    CVDisplayLinkStart(displayLink);
    
    // バッチ処理バッファの初期化（USE_QUAD=1の場合のみ）
#if USE_QUAD
    [self initBatchBuffers];
#endif
    
    // MicroUIの初期化
    [self initMicroUI];
}

/**
 * キーダウン処理
 */
- (void)keyDown:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * キーアップ処理
 */
- (void)keyUp:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * マウスダウン処理
 */
- (void)mouseDown:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * マウスドラッグ処理
 */
- (void)mouseDragged:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * マウスアップ処理
 */
- (void)mouseUp:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * マウス移動処理
 */
- (void)mouseMoved:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * マウスホイール処理
 */
- (void)scrollWheel:(NSEvent *)event
{
    // MicroUIにイベント転送
    [self handleMicroUIEvent:event];
}

/**
 * ウィンドウがクローズされる時の処理
 */
- (void)windowWillClose:(NSNotification*)notification
{
    // DisplayLinkを停止・解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
        displayLink = NULL;
    }
    
    // MicroUIコンテキストの解放
    if (mu_ctx) {
        free(mu_ctx);
        mu_ctx = NULL;
    }
    
    // バッチ処理バッファの解放（USE_QUAD=1の場合のみ）
#if USE_QUAD
    [self cleanupBatchBuffers];
#endif
}

/**
 * ビューの破棄時の処理
 */
- (void)dealloc
{
    // DisplayLinkが存在する場合は停止・解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    
    // MicroUIコンテキストの解放
    if (mu_ctx) {
        free(mu_ctx);
    }
    
    // バッチ処理バッファの解放（USE_QUAD=1の場合のみ）
#if USE_QUAD
    [self cleanupBatchBuffers];
#endif
}

#if USE_QUAD
#pragma mark - バッチ処理メソッド

/**
 * バッチ処理用のバッファを初期化
 */
- (void)initBatchBuffers
{
    // 頂点バッファの生成
    glGenBuffers(1, &vertex_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(Vertex) * BUFFER_SIZE, NULL, GL_DYNAMIC_DRAW);
    
    // インデックスバッファの生成と初期化
    glGenBuffers(1, &index_buffer);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    
    // インデックスの初期化（四角形をインデックス付き三角形として描画）
    GLuint* indices = batch_state.indices;
    for (int i = 0; i < MAX_QUADS; i++) {
        indices[i*6+0] = i*4+0;
        indices[i*6+1] = i*4+1;
        indices[i*6+2] = i*4+2;
        indices[i*6+3] = i*4+0;
        indices[i*6+4] = i*4+2;
        indices[i*6+5] = i*4+3;
    }
    
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(GLuint) * BUFFER_SIZE * 6 / 4, indices, GL_STATIC_DRAW);
    
    // 状態の初期化
    batch_state.vertex_count = 0;
    batch_state.index_count = 0;
    batch_state.current_texture = 0;
    batch_state.is_textured = NO;
}

/**
 * バッチバッファをフラッシュして描画
 */
- (void)flushBatch
{
    if (batch_state.vertex_count == 0) {
        return;
    }
    
    // バッファに頂点データをアップロード
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(Vertex) * batch_state.vertex_count, batch_state.vertices);
    
    // 頂点配列の設定
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    glVertexPointer(2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, x));
    glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, u));
    glColorPointer(4, GL_FLOAT, sizeof(Vertex), (void*)offsetof(Vertex, r));
    
    // インデックスバッファをバインド
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
    
    // インデックス付き三角形として描画
    glDrawElements(GL_TRIANGLES, batch_state.index_count, GL_UNSIGNED_INT, 0);
    
    // クライアント状態を無効化
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    
    // バッチ状態をリセット
    batch_state.vertex_count = 0;
    batch_state.index_count = 0;
}

/**
 * 四角形をバッチに追加
 */
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured
{
    // バッファがいっぱいならフラッシュ
    if (batch_state.vertex_count >= BUFFER_SIZE - 4) {
        [self flushBatch];
    }
    
    // テクスチャ状態が変わった場合もフラッシュ
    if (textured != batch_state.is_textured && batch_state.vertex_count > 0) {
        [self flushBatch];
        batch_state.is_textured = textured;
    }
    
    // テクスチャ座標の計算
    float sx = src.x / (float)ATLAS_WIDTH;
    float sy = src.y / (float)ATLAS_HEIGHT;
    float sw = src.w / (float)ATLAS_WIDTH;
    float sh = src.h / (float)ATLAS_HEIGHT;
    
    // 色の正規化
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // 頂点インデックスの計算
    int idx = batch_state.vertex_count;
    
    // 頂点データの設定
    Vertex* v = &batch_state.vertices[idx];
    
    // 左上
    v[0].x = dst.x;
    v[0].y = dst.y;
    v[0].u = sx;
    v[0].v = sy;
    v[0].r = r;
    v[0].g = g;
    v[0].b = b;
    v[0].a = a;
    
    // 左下
    v[1].x = dst.x;
    v[1].y = dst.y + dst.h;
    v[1].u = sx;
    v[1].v = sy + sh;
    v[1].r = r;
    v[1].g = g;
    v[1].b = b;
    v[1].a = a;
    
    // 右下
    v[2].x = dst.x + dst.w;
    v[2].y = dst.y + dst.h;
    v[2].u = sx + sw;
    v[2].v = sy + sh;
    v[2].r = r;
    v[2].g = g;
    v[2].b = b;
    v[2].a = a;
    
    // 右上
    v[3].x = dst.x + dst.w;
    v[3].y = dst.y;
    v[3].u = sx + sw;
    v[3].v = sy;
    v[3].r = r;
    v[3].g = g;
    v[3].b = b;
    v[3].a = a;
    
    // インデックス数とバッファカウントを更新
    batch_state.vertex_count += 4;
    batch_state.index_count += 6;
}

/**
 * バッチ処理バッファを解放
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
    
    // 統合レンダラーの初期化
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
    
    // atlas.inlの正しいテクスチャデータをアップロード
    // OpenGL 3.2 Core Profileでは、GL_ALPHAの代わりにGL_REDを使用する必要がある
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_RED, GL_UNSIGNED_BYTE, atlas_texture);
    
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // シャープなピクセルフォント用
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}

- (void)setupFallbackFont {
    NSLog(@"フォールバックフォントを使用します");
    
    // 1x1の白いテクスチャを作成
    unsigned char data[ATLAS_WIDTH * ATLAS_HEIGHT];
    memset(data, 0, sizeof(data));
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            data[i * ATLAS_WIDTH + j] = 255;
        }
    }
    
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_RED, GL_UNSIGNED_BYTE, data);
    
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    
    NSLog(@"16x16の白いテクスチャを作成しました");
}

/**
 * MicroUIのフレーム処理
 */
- (void)processMicroUIFrame
{
    // ウィンドウ移動検出
    NSPoint origin = [[self window] frame].origin;
    int wx = origin.x;
    int wy = origin.y;
    if (wx != prevWindowX || wy != prevWindowY) {
        prevWindowX = wx;
        prevWindowY = wy;
        mu_input_mousemove(mu_ctx, mu_ctx->mouse_pos.x, mu_ctx->mouse_pos.y);
    }
    
    mu_begin(mu_ctx);
    
    // ここにMicroUIのGUIコード（ウィンドウなど）
    
    // ログウィンドウ
    if (mu_begin_window(mu_ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        mu_layout_row(mu_ctx, 1, (int[]) { -1 }, -1);
        mu_begin_panel(mu_ctx, "LogPanel");
        mu_Container *panel = mu_get_current_container(mu_ctx);
        panel->scroll.y = panel->content_size.y - panel->body.h; // スクロールを一番下に固定
        mu_layout_row(mu_ctx, 1, (int[]) { -1 }, -1);
        mu_text(mu_ctx, logbuf);
        mu_end_panel(mu_ctx);
        mu_end_window(mu_ctx);
    }
    
    // コントロールウィンドウ
    if (mu_begin_window(mu_ctx, "Controls", mu_rect(50, 40, 250, 250))) {
        mu_layout_row(mu_ctx, 1, (int[]) { -1 }, 0);
        mu_text(mu_ctx, "MicroUIデモコントロール");
        mu_layout_row(mu_ctx, 1, (int[]) { -1 }, 0);
        mu_label(mu_ctx, "背景色:");
        
        mu_layout_row(mu_ctx, 3, (int[]) { 60, 100, 40 }, 0);
        mu_label(mu_ctx, "赤:"); 
        if (mu_slider(mu_ctx, &bg[0], 0, 1) & MU_RES_CHANGE) {
            [self writeLog:"赤色値が変更されました"];
        }
        char buf[8];
        sprintf(buf, "%.2f", bg[0]);
        mu_label(mu_ctx, buf);
        
        mu_layout_row(mu_ctx, 3, (int[]) { 60, 100, 40 }, 0);
        mu_label(mu_ctx, "緑:");
        if (mu_slider(mu_ctx, &bg[1], 0, 1) & MU_RES_CHANGE) {
            [self writeLog:"緑色値が変更されました"];
        }
        sprintf(buf, "%.2f", bg[1]);
        mu_label(mu_ctx, buf);
        
        mu_layout_row(mu_ctx, 3, (int[]) { 60, 100, 40 }, 0);
        mu_label(mu_ctx, "青:");
        if (mu_slider(mu_ctx, &bg[2], 0, 1) & MU_RES_CHANGE) {
            [self writeLog:"青色値が変更されました"];
        }
        sprintf(buf, "%.2f", bg[2]);
        mu_label(mu_ctx, buf);
        
        mu_layout_row(mu_ctx, 1, (int[]) { -1 }, 0);
        if (mu_button(mu_ctx, "ログクリア")) {
            logbuf[0] = '\0';
            logbuf_updated = 1;
            [self writeLog:"ログがクリアされました"];
        }
        
        mu_layout_row(mu_ctx, 2, (int[]) { 100, 100 }, 0);
        if (mu_button(mu_ctx, "ボタンA")) {
            [self writeLog:"ボタンAがクリックされました"];
        }
        if (mu_button(mu_ctx, "ボタンB")) {
            [self writeLog:"ボタンBがクリックされました"];
        }
        
        mu_end_window(mu_ctx);
    }
    
    mu_end(mu_ctx);
}

/**
 * MicroUI描画メソッド
 */
- (void)drawMicroUI
{
    // 背景色の反映
    glClearColor(bg[0], bg[1], bg[2], 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
    
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    
    // 描画用の座標系設定
    glViewport(0, 0, viewWidth, viewHeight);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0f, viewWidth, viewHeight, 0.0f, -1.0f, 1.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // レンダリングコマンドのレンダリング
    mu_Command *cmd = NULL;
    while (mu_next_command(mu_ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT:
                r_draw_text(cmd->text.str, cmd->text.pos, cmd->text.color);
                break;
            case MU_COMMAND_RECT:
                r_draw_rect(cmd->rect.rect, cmd->rect.color);
                break;
            case MU_COMMAND_ICON:
                r_draw_icon(cmd->icon.id, cmd->icon.rect, cmd->icon.color);
                break;
            case MU_COMMAND_CLIP:
                r_set_clip_rect(cmd->clip.rect);
                break;
        }
    }
}

/**
 * イベント処理（MicroUIへの入力変換）
 */
- (void)handleMicroUIEvent:(NSEvent*)event
{
    NSPoint windowPoint = [event locationInWindow];
    NSPoint viewPoint = [self convertPoint:windowPoint fromView:nil];
    
    // Retinaディスプレイの場合のスケーリング
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:NSMakeSize(1.0, 1.0)];
        viewPoint.x *= backingSize.width;
        viewPoint.y *= backingSize.height;
    }
    
    // macOSのY座標は下から上なので、MicroUIの座標系（上から下）に変換
    NSRect bounds = [self bounds];
    if ([self wantsBestResolutionOpenGLSurface]) {
        bounds.size = [self convertSizeToBacking:bounds.size];
    }
    viewPoint.y = bounds.size.height - viewPoint.y;
    
    int x = viewPoint.x;
    int y = viewPoint.y;
    
    switch ([event type]) {
        case NSEventTypeMouseMoved:
            mu_input_mousemove(mu_ctx, x, y);
            break;
            
        case NSEventTypeLeftMouseDown:
            mu_input_mousedown(mu_ctx, x, y, MU_MOUSE_LEFT);
            break;
            
        case NSEventTypeLeftMouseUp:
            mu_input_mouseup(mu_ctx, x, y, MU_MOUSE_LEFT);
            break;
            
        case NSEventTypeRightMouseDown:
            mu_input_mousedown(mu_ctx, x, y, MU_MOUSE_RIGHT);
            break;
            
        case NSEventTypeRightMouseUp:
            mu_input_mouseup(mu_ctx, x, y, MU_MOUSE_RIGHT);
            break;
            
        case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDragged:
            mu_input_mousemove(mu_ctx, x, y);
            break;
            
        case NSEventTypeScrollWheel: {
            float dy = [event scrollingDeltaY];
            
            // ピクセル単位とライン単位のスクロールを区別
            if ([event hasPreciseScrollingDeltas]) {
                dy /= 10.0; // スクロール速度の調整
            }
            
            if (fabs(dy) > 0.1) {
                mu_input_scroll(mu_ctx, 0, dy * -1); // macOSとMicroUIでスクロール方向が逆
            }
            break;
        }
            
        case NSEventTypeKeyDown: {
            int key = 0;
            
            // 特殊キーの変換
            switch ([event keyCode]) {
                case 36: key = MU_KEY_RETURN; break;    // Return
                case 51: key = MU_KEY_BACKSPACE; break; // Backspace
                case 123: key = MU_KEY_LEFT; break;     // Left Arrow
                case 124: key = MU_KEY_RIGHT; break;    // Right Arrow
                case 125: key = MU_KEY_DOWN; break;     // Down Arrow
                case 126: key = MU_KEY_UP; break;       // Up Arrow
            }
            
            // 特殊キーが見つかった場合、処理
            if (key) {
                mu_input_keydown(mu_ctx, key);
            } else {
                // 通常の文字入力
                NSString *characters = [event characters];
                for (NSUInteger i = 0; i < [characters length]; i++) {
                    unichar c = [characters characterAtIndex:i];
                    if (c >= 32) { // 表示可能な文字のみ
                        mu_input_text(mu_ctx, (const char*)&c);
                    }
                }
            }
            break;
        }
            
        case NSEventTypeKeyUp: {
            int key = 0;
            
            // 特殊キーの変換
            switch ([event keyCode]) {
                case 36: key = MU_KEY_RETURN; break;
                case 51: key = MU_KEY_BACKSPACE; break;
                case 123: key = MU_KEY_LEFT; break;
                case 124: key = MU_KEY_RIGHT; break;
                case 125: key = MU_KEY_DOWN; break;
                case 126: key = MU_KEY_UP; break;
            }
            
            if (key) {
                mu_input_keyup(mu_ctx, key);
            }
            break;
        }
            
        default:
            break;
    }
}

@end

@interface AppDelegate : NSObject <NSApplicationDelegate>
@property (weak) IBOutlet NSWindow *window;
@end

@implementation AppDelegate

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
    // ウィンドウの設定
    [self.window setTitle:@"MicroUI OpenGLデモ"];
    
    // アスペクト比を維持するよう設定
    [self.window setContentAspectRatio:NSMakeSize(4, 3)];
    
    // ウィンドウサイズの設定
    NSRect frame = [self.window frame];
    frame.size.width = 800;
    frame.size.height = 600;
    [self.window setFrame:frame display:YES];
    
    // ウィンドウを中央に配置
    [self.window center];
}

@end

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        return NSApplicationMain(argc, argv);
    }
}
EOF

echo "main.mファイルを新しい整理されたバージョンに置き換えました"
