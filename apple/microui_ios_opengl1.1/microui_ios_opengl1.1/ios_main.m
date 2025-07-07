// MicroUIを使ったiOS用ウィジェット表示のコード

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
 * iOS版への移植 (2025年6月11日)
 * - OpenGLからOpenGLESへ変更
 * - NSOpenGLViewからCAEAGLLayerへ変更
 * - マウスイベントをタッチイベントに変換
 * - DisplayLinkをCADisplayLinkに変更
 */

// バッチ処理モード切り替え（1でバッチ処理、0で従来の即座描画）
#define USE_QUAD 0

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <OpenGLES/ES2/gl.h>
#import <QuartzCore/QuartzCore.h>
#import "microui.h"
#import "atlas.inl"
#define MAX_QUADS 4096
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
 * MicroUIビュークラス
 * iOSでOpenGLESを使用して三角形とMicroUIを描画するためのカスタムビュー
 */
@interface MicroUIView : UIView {
    EAGLContext *context;
    GLuint defaultFramebuffer;
    GLuint colorRenderbuffer;
    GLuint depthRenderbuffer;
    
    int viewWidth;
    int viewHeight;
    
    // MicroUI関連
    mu_Context* mu_ctx;
    GLuint font_texture;
    char logbuf[64000];
    int logbuf_updated;
    float bg[3];
    
    // ウィンドウ移動検出用
    int prevWindowX;
    int prevWindowY;
    
    // アニメーション用
    CADisplayLink *displayLink;
    
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
- (void)handleMicroUITouchEvent:(UITouch*)touch withType:(int)type;

#if USE_QUAD
// バッチ処理用メソッド
- (void)initBatchBuffers;
- (void)flushBatch;
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured;
- (void)cleanupBatchBuffers;
#endif

// OpenGLES関連
- (void)setupLayer;
- (void)setupContext;
- (void)setupRenderBuffer;
- (void)setupFrameBuffer;
- (void)render;

@end

@implementation MicroUIView

/**
 * 文字幅を計算する関数
 */
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { 
        len = strlen(text); 
    }
    
    // デバッグ情報出力（最初の数回のみ）
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
 * 文字の高さを返す関数
 */
static int text_height(mu_Font font) {
    return 17; // atlas.inlのフォントは17ピクセル高
}

/**
 * uint8スライダー用のヘルパー関数
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
 * CustomViewの初期化
 */
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        // レイヤーの設定
        [self setupLayer];
        
        // OpenGLESコンテキストの設定
        [self setupContext];
        
        // レンダーバッファとフレームバッファの設定
        [self setupRenderBuffer];
        [self setupFrameBuffer];
        
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
        
        // MicroUIの初期化
        [self initMicroUI];
        
        // タッチイベント有効化
        self.multipleTouchEnabled = NO;
        
        // CADisplayLink設定
        displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    }
    return self;
}

/**
 * レイヤーの設定
 */
- (void)setupLayer {
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    
    // Retinaディスプレイに対応
    self.contentScaleFactor = [[UIScreen mainScreen] scale];
    
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
}

/**
 * OpenGLESコンテキストの設定
 */
- (void)setupContext {
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    
    if (!context || ![EAGLContext setCurrentContext:context]) {
        NSLog(@"Failed to setup OpenGL ES context");
    }
}

/**
 * レンダーバッファの設定
 */
- (void)setupRenderBuffer {
    glGenRenderbuffers(1, &colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];
    
    // ビューのサイズを取得
    GLint width, height;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    
    viewWidth = width;
    viewHeight = height;
    
    NSLog(@"Render buffer created with size: %d x %d", viewWidth, viewHeight);
    
    // 深度バッファ作成 (必要に応じて)
    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, viewWidth, viewHeight);
}

/**
 * フレームバッファの設定
 */
- (void)setupFrameBuffer {
    glGenFramebuffers(1, &defaultFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"Failed to create complete framebuffer object: %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

/**
 * レイアウト変更時の処理
 */
- (void)layoutSubviews {
    [super layoutSubviews];
    
    // フレームバッファとレンダーバッファを解放
    if (defaultFramebuffer) {
        glDeleteFramebuffers(1, &defaultFramebuffer);
        defaultFramebuffer = 0;
    }
    
    if (colorRenderbuffer) {
        glDeleteRenderbuffers(1, &colorRenderbuffer);
        colorRenderbuffer = 0;
    }
    
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
    
    // レンダーバッファとフレームバッファを再作成
    [self setupRenderBuffer];
    [self setupFrameBuffer];
    
    // 描画を要求
    [self render];
}

/**
 * 描画処理
 */
- (void)render {
    if (!context || ![EAGLContext setCurrentContext:context]) {
        return;
    }
    
    // フレームバッファをバインド
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    
    // MicroUIフレーム処理
    [self processMicroUIFrame];
    
    // 背景クリア
    glViewport(0, 0, viewWidth, viewHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 三角形の描画（OpenGLES互換の頂点配列を使用）
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    // 三角形の頂点データ
    GLfloat triangleVertices[] = {
        0.0f, 0.5f,     // 上
        -0.5f, -0.5f,   // 左下
        0.5f, -0.5f     // 右下
    };
    
    // 三角形の色データ
    GLfloat triangleColors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,  // 赤
        0.0f, 1.0f, 0.0f, 1.0f,  // 緑
        0.0f, 0.0f, 1.0f, 1.0f   // 青
    };
    
    // テクスチャを無効化
    glDisable(GL_TEXTURE_2D);
    
    // 頂点配列を有効化
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    // 頂点データを設定
    glVertexPointer(2, GL_FLOAT, 0, triangleVertices);
    glColorPointer(4, GL_FLOAT, 0, triangleColors);
    
    // 三角形を描画
    glDrawArrays(GL_TRIANGLES, 0, 3);
    
    // 頂点配列を無効化
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    
    // MicroUIの描画
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, viewWidth, viewHeight, 0, -1, 1); // iOSでは上下が逆
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    [self drawMicroUI];
    
    // レンダーバッファを表示
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

/**
 * 解放処理
 */
- (void)dealloc {
    // CADisplayLinkを停止
    [displayLink invalidate];
    
    // フレームバッファとレンダーバッファを解放
    if (defaultFramebuffer) {
        glDeleteFramebuffers(1, &defaultFramebuffer);
        defaultFramebuffer = 0;
    }
    
    if (colorRenderbuffer) {
        glDeleteRenderbuffers(1, &colorRenderbuffer);
        colorRenderbuffer = 0;
    }
    
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
    
#if USE_QUAD
    // バッチバッファのクリーンアップ
    [self cleanupBatchBuffers];
#endif
    
    // MicroUIの解放
    glDeleteTextures(1, &font_texture);
    free(mu_ctx);
    
    // コンテキストをnilに設定
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }
}

#pragma mark - MicroUI関連メソッド
/**
 * MicroUIの初期化
 */
- (void)initMicroUI {
    [EAGLContext setCurrentContext:context];
    
    // エラーをクリア
    while(glGetError() != GL_NO_ERROR);
    
    // OpenGLESの初期設定
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
    [self writeLog:"MicroUI iOS版初期化完了"];
    logbuf_updated = 1;
    
    NSLog(@"MicroUI initialization completed");
    
#if USE_QUAD
    // バッチ処理用バッファの初期化
    [self initBatchBuffers];
#endif
}

/**
 * システムフォントの設定
 */
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

/**
 * フォールバックフォントの設定
 */
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
                            // アルファベットなど他の文字はシンプルなデフォルト形状
                            if ((x == 3 || x == charWidth-4) && (y >= 3 && y <= charHeight-4) ||
                                (y == 3 || y == charHeight-4) && (x >= 3 && x <= charWidth-4)) {
                                pixelValue = 200;
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
    
    NSLog(@"フォールバックフォントを生成しました");
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

/**
 * MicroUIフレームの処理
 */
- (void)processMicroUIFrame {
    // MicroUIの開始
    mu_begin(mu_ctx);
    
    // テストウィンドウの描画
    [self drawTestWindow];
    
    // ログウィンドウとスタイルウィンドウの描画
    [self drawLogWindow];
    [self drawStyleWindow];
    
    // MicroUIの終了
    mu_end(mu_ctx);
}

/**
 * テストウィンドウの描画
 */
- (void)drawTestWindow {
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
- (void)drawLogWindow {
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
 * スタイルエディタウィンドウの描画
 */
- (void)drawStyleWindow {
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

#pragma mark - 描画関連メソッド

/**
 * MicroUIの描画
 */
- (void)drawMicroUI {
    // MicroUIのコンテキストがあるかチェック
    if (!mu_ctx) {
        NSLog(@"MicroUI context is not initialized");
        return;
    }
    
    // OpenGL状態の設定
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
- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color {
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
        
        // テクスチャ座標の計算
        float tx = (float)src.x / ATLAS_WIDTH;
        float ty = (float)src.y / ATLAS_HEIGHT;
        float tw = (float)src.w / ATLAS_WIDTH;
        float th = (float)src.h / ATLAS_HEIGHT;
        
        // 頂点とテクスチャ座標を設定してクアッドを描画（OpenGLES用に修正）
        GLfloat vertices[] = {
            dst.x, dst.y,                 // 左上
            dst.x + dst.w, dst.y,         // 右上
            dst.x + dst.w, dst.y + dst.h, // 右下
            dst.x, dst.y + dst.h          // 左下
        };
        
        GLfloat texCoords[] = {
            tx, ty,           // 左上
            tx + tw, ty,      // 右上
            tx + tw, ty + th, // 右下
            tx, ty + th       // 左下
        };
        
        GLfloat colors[] = {
            r, g, b, a,
            r, g, b, a,
            r, g, b, a,
            r, g, b, a
        };
        
        // 頂点配列を使って描画
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        
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
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color {
#if USE_QUAD
    // バッチ処理版
    mu_Rect src = {0, 0, 1, 1}; // ダミーのテクスチャ座標
    [self pushQuad:rect src:src color:color textured:NO];
    
#else
    // 従来の即座描画版（OpenGLES互換）
    float x = rect.x;
    float y = rect.y;
    float w = rect.w;
    float h = rect.h;
    float r = color.r / 255.0;
    float g = color.g / 255.0;
    float b = color.b / 255.0;
    float a = color.a / 255.0;

    glDisable(GL_TEXTURE_2D);
    
    // OpenGLES用にクアッドを頂点配列で描画
    GLfloat vertices[] = {
        x,     y,      // 左上
        x + w, y,      // 右上
        x,     y + h,  // 左下
        x + w, y + h   // 右下
    };
    
    GLfloat colors[] = {
        r, g, b, a,  // 左上
        r, g, b, a,  // 右上
        r, g, b, a,  // 左下
        r, g, b, a   // 右下
    };
    
    // 頂点配列の設定
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_FLOAT, 0, colors);
    
    // 矩形を描画（triangle stripで2つの三角形を描画）
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    
    // 頂点配列を無効化
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
#endif
}
/**
 * アイコンの描画
 */
- (void)drawIcon:(int)id rect:(mu_Rect)rect color:(mu_Color)color {
    if (id < 0) return;
    
    // アイコンの情報を取得
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
    float tx = (float)src.x / ATLAS_WIDTH;
    float ty = (float)src.y / ATLAS_HEIGHT;
    float tw = (float)src.w / ATLAS_WIDTH;
    float th = (float)src.h / ATLAS_HEIGHT;
    
    // テクスチャをバインド
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    // OpenGLES用に描画
    GLfloat vertices[] = {
        dst.x, dst.y,                 // 左上
        dst.x + dst.w, dst.y,         // 右上
        dst.x + dst.w, dst.y + dst.h, // 右下
        dst.x, dst.y + dst.h          // 左下
    };
    
    GLfloat texCoords[] = {
        tx, ty,           // 左上
        tx + tw, ty,      // 右上
        tx + tw, ty + th, // 右下
        tx, ty + th       // 左下
    };
    
    GLfloat colors[] = {
        r, g, b, a,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    };
    
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glColorPointer(4, GL_FLOAT, 0, colors);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisable(GL_TEXTURE_2D);
#endif
}
//
///**
// * クリップ矩形の設定
// */
//- (void)setClipRect:(mu_Rect)rect viewHeight:(int)viewHeight {
//    // OpenGLESの座標系に合わせてクリップを設定
//    glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h);
//}
//
//
//
///**
// * バッチ処理用バッファの初期化
// */
//- (void)initBatchBuffers {
//    // インデックスバッファを事前計算
//    for (int i = 0; i < MAX_QUADS; i++) {
//        int quad_index = i * 6;
//        int vertex_index = i * 4;
//        
//        // 各クアッドのインデックス（時計回り）
//        batch_state.indices[quad_index + 0] = vertex_index + 0; // 左上
//        batch_state.indices[quad_index + 1] = vertex_index + 1; // 右上
//        batch_state.indices[quad_index + 2] = vertex_index + 2; // 右下
//        batch_state.indices[quad_index + 3] = vertex_index + 2; // 右下
//        batch_state.indices[quad_index + 4] = vertex_index + 3; // 左下
//        batch_state.indices[quad_index + 5] = vertex_index + 0; // 左上
//    }
//    
//    // VBOとIBOの生成
//    glGenBuffers(1, &vertex_buffer);
//    glGenBuffers(1, &index_buffer);
//    
//    // インデックスバッファにデータをアップロード（一度だけ）
//    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, index_buffer);
//    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 
//                 sizeof(GLuint) * MAX_QUADS * 6, 
//                 batch_state.indices, 
//                 GL_STATIC_DRAW);
//    
//    NSLog(@"Batch buffers initialized: max_quads=%d", MAX_QUADS);
//}
//
///**
// * バッチにクアッドを追加
// */
//- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured {
//    // バッファが満杯の場合はフラッシュ
//    if (batch_state.vertex_count >= BUFFER_SIZE - 4) {
//        [self flushBatch];
//    }
//    
//    // テクスチャ状態が変わった場合もフラッシュ
//    if (batch_state.vertex_count > 0 && batch_state.is_textured != textured) {
//        [self flushBatch];
//    }
//    
//    batch_state.is_textured = textured;
//    
//    // 色を正規化
//    float r = color.r / 255.0f;
//    float g = color.g / 255.0f;
//    float b = color.b / 255.0f;
//    float a = color.a / 255.0f;
//    
//    // テクスチャ座標を計算
//    float u1 = 0.0f, v1 = 0.0f, u2 = 1.0f, v2 = 1.0f;
//    if (textured) {
//        u1 = (float)src.x / ATLAS_WIDTH;
//        v1 = (float)src.y / ATLAS_HEIGHT;
//        u2 = (float)(src.x + src.w) / ATLAS_WIDTH;
//        v2 = (float)(src.y + src.h) / ATLAS_HEIGHT;
//    }
//    
//    // 4つの頂点を追加
//    int base_index = batch_state.vertex_count;
//    
//    // 左上
//    batch_state.vertices[base_index + 0] = (Vertex){
//        dst.x, dst.y, u1, v1, r, g, b, a
//    };
//    
//    // 右上
//    batch_state.vertices[base_index + 1] = (Vertex){
//        dst.x + dst.w, dst.y, u2, v1, r, g, b, a
//    };
//    
//    // 右下
//    batch_state.vertices[base_index + 2] = (Vertex){
//        dst.x + dst.w, dst.y + dst.h, u2, v2, r, g, b, a
//    };
//    
//    // 左下
//    batch_state.vertices[base_index + 3] = (Vertex){
//        dst.x, dst.y + dst.h, u1, v2, r, g, b, a
//    };
//    
//    batch_state.vertex_count += 4;
//    batch_state// filepath: /Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/ios_main.m
// MicroUIを使ったiOS用ウィジェット表示のコード

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
 * iOS版への移植 (2025年6月11日)
 * - OpenGLからOpenGLESへ変更
 * - NSOpenGLViewからCAEAGLLayerへ変更
 * - マウスイベントをタッチイベントに変換
 * - DisplayLinkをCADisplayLinkに変更
 */

// バッチ処理モード切り替え（1でバッチ処理、0で従来の即座描画）
#define USE_QUAD 0

#import <UIKit/UIKit.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#import <QuartzCore/QuartzCore.h>
#import "microui.h"
#import "atlas.inl"

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
 * MicroUIビュークラス
 * iOSでOpenGLESを使用して三角形とMicroUIを描画するためのカスタムビュー
 */
@interface MicroUIView : UIView {
    EAGLContext *context;
    GLuint defaultFramebuffer;
    GLuint colorRenderbuffer;
    GLuint depthRenderbuffer;
    
    int viewWidth;
    int viewHeight;
    
    // MicroUI関連
    mu_Context* mu_ctx;
    GLuint font_texture;
    char logbuf[64000];
    int logbuf_updated;
    float bg[3];
    
    // ウィンドウ移動検出用
    int prevWindowX;
    int prevWindowY;
    
    // アニメーション用
    CADisplayLink *displayLink;
    
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
- (void)handleMicroUITouchEvent:(UITouch*)touch withType:(int)type;

#if USE_QUAD
// バッチ処理用メソッド
- (void)initBatchBuffers;
- (void)flushBatch;
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured;
- (void)cleanupBatchBuffers;
#endif

// OpenGLES関連
- (void)setupLayer;
- (void)setupContext;
- (void)setupRenderBuffer;
- (void)setupFrameBuffer;
- (void)render;

@end

@implementation MicroUIView

/**
 * 文字幅を計算する関数
 */
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { 
        len = strlen(text); 
    }
    
    // デバッグ情報出力（最初の数回のみ）
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
 * 文字の高さを返す関数
 */
static int text_height(mu_Font font) {
    return 17; // atlas.inlのフォントは17ピクセル高
}

/**
 * uint8スライダー用のヘルパー関数
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
 * CustomViewの初期化
 */
+ (Class)layerClass {
    return [CAEAGLLayer class];
}

- (instancetype)initWithFrame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        // レイヤーの設定
        [self setupLayer];
        
        // OpenGLESコンテキストの設定
        [self setupContext];
        
        // レンダーバッファとフレームバッファの設定
        [self setupRenderBuffer];
        [self setupFrameBuffer];
        
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
        
        // MicroUIの初期化
        [self initMicroUI];
        
        // タッチイベント有効化
        self.multipleTouchEnabled = NO;
        
        // CADisplayLink設定
        displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
    }
    return self;
}

/**
 * レイヤーの設定
 */
- (void)setupLayer {
    CAEAGLLayer *eaglLayer = (CAEAGLLayer *)self.layer;
    
    // Retinaディスプレイに対応
    self.contentScaleFactor = [[UIScreen mainScreen] scale];
    
    eaglLayer.opaque = YES;
    eaglLayer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
}

/**
 * OpenGLESコンテキストの設定
 */
- (void)setupContext {
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    
    if (!context || ![EAGLContext setCurrentContext:context]) {
        NSLog(@"Failed to setup OpenGL ES context");
    }
}

/**
 * レンダーバッファの設定
 */
- (void)setupRenderBuffer {
    glGenRenderbuffers(1, &colorRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER fromDrawable:(CAEAGLLayer *)self.layer];
    
    // ビューのサイズを取得
    GLint width, height;
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &width);
    glGetRenderbufferParameteriv(GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &height);
    
    viewWidth = width;
    viewHeight = height;
    
    NSLog(@"Render buffer created with size: %d x %d", viewWidth, viewHeight);
    
    // 深度バッファ作成 (必要に応じて)
    glGenRenderbuffers(1, &depthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRenderbuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT16, viewWidth, viewHeight);
}

/**
 * フレームバッファの設定
 */
- (void)setupFrameBuffer {
    glGenFramebuffers(1, &defaultFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorRenderbuffer);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRenderbuffer);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        NSLog(@"Failed to create complete framebuffer object: %x", glCheckFramebufferStatus(GL_FRAMEBUFFER));
    }
}

/**
 * レイアウト変更時の処理
 */
- (void)layoutSubviews {
    [super layoutSubviews];
    
    // フレームバッファとレンダーバッファを解放
    if (defaultFramebuffer) {
        glDeleteFramebuffers(1, &defaultFramebuffer);
        defaultFramebuffer = 0;
    }
    
    if (colorRenderbuffer) {
        glDeleteRenderbuffers(1, &colorRenderbuffer);
        colorRenderbuffer = 0;
    }
    
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
    
    // レンダーバッファとフレームバッファを再作成
    [self setupRenderBuffer];
    [self setupFrameBuffer];
    
    // 描画を要求
    [self render];
}

/**
 * 描画処理
 */
- (void)render {
    if (!context || ![EAGLContext setCurrentContext:context]) {
        return;
    }
    
    // フレームバッファをバインド
    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebuffer);
    
    // MicroUIフレーム処理
    [self processMicroUIFrame];
    
    // 背景クリア
    glViewport(0, 0, viewWidth, viewHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    
    // 三角形の描画
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.5f, 0.0f);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
    glVertex3f(-0.5f, -0.5f, 0.0f);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);
    glVertex3f(0.5f, -0.5f, 0.0f);
    glEnd();
    
    // MicroUIの描画
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrthof(0, viewWidth, viewHeight, 0, -1, 1); // iOSでは上下が逆
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    [self drawMicroUI];
    
    // レンダーバッファを表示
    glBindRenderbuffer(GL_RENDERBUFFER, colorRenderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER];
}

/**
 * 解放処理
 */
- (void)dealloc {
    // CADisplayLinkを停止
    [displayLink invalidate];
    
    // フレームバッファとレンダーバッファを解放
    if (defaultFramebuffer) {
        glDeleteFramebuffers(1, &defaultFramebuffer);
        defaultFramebuffer = 0;
    }
    
    if (colorRenderbuffer) {
        glDeleteRenderbuffers(1, &colorRenderbuffer);
        colorRenderbuffer = 0;
    }
    
    if (depthRenderbuffer) {
        glDeleteRenderbuffers(1, &depthRenderbuffer);
        depthRenderbuffer = 0;
    }
    
#if USE_QUAD
    // バッチバッファのクリーンアップ
    [self cleanupBatchBuffers];
#endif
    
    // MicroUIの解放
    glDeleteTextures(1, &font_texture);
    free(mu_ctx);
    
    // コンテキストをnilに設定
    if ([EAGLContext currentContext] == context) {
        [EAGLContext setCurrentContext:nil];
    }
}

#pragma mark - MicroUI関連メソッド
/**
 * MicroUIの初期化
 */
- (void)initMicroUI {
    [EAGLContext setCurrentContext:context];
    
    // エラーをクリア
    while(glGetError() != GL_NO_ERROR);
    
    // OpenGLESの初期設定
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
    [self writeLog:"MicroUI iOS版初期化完了"];
    logbuf_updated = 1;
    
    NSLog(@"MicroUI initialization completed");
    
#if USE_QUAD
    // バッチ処理用バッファの初期化
    [self initBatchBuffers];
#endif
}

/**
 * システムフォントの設定
 */
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

/**
 * フォールバックフォントの設定
 */
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
                            // アルファベットなど他の文字はシンプルなデフォルト形状
                            if ((x == 3 || x == charWidth-4) && (y >= 3 && y <= charHeight-4) ||
                                (y == 3 || y == charHeight-4) && (x >= 3 && x <= charWidth-4)) {
                                pixelValue = 200;
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
    
    NSLog(@"フォールバックフォントを生成しました");
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

/**
 * MicroUIフレームの処理
 */
- (void)processMicroUIFrame {
    // MicroUIの開始
    mu_begin(mu_ctx);
    
    // テストウィンドウの描画
    [self drawTestWindow];
    
    // ログウィンドウとスタイルウィンドウの描画
    [self drawLogWindow];
    [self drawStyleWindow];
    
    // MicroUIの終了
    mu_end(mu_ctx);
}

/**
 * テストウィンドウの描画
 */
- (void)drawTestWindow {
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
- (void)drawLogWindow {
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
 * スタイルエディタウィンドウの描画
 */
- (void)drawStyleWindow {
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

#pragma mark - 描画関連メソッド

/**
 * MicroUIの描画
 */
- (void)drawMicroUI {
    // MicroUIのコンテキストがあるかチェック
    if (!mu_ctx) {
        NSLog(@"MicroUI context is not initialized");
        return;
    }
    
    // OpenGL状態の設定
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
- (void)drawText:(const char*)text pos:(mu_Vec2)pos color:(mu_Color)color {
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
        
        // テクスチャ座標の計算
        float tx = (float)src.x / ATLAS_WIDTH;
        float ty = (float)src.y / ATLAS_HEIGHT;
        float tw = (float)src.w / ATLAS_WIDTH;
        float th = (float)src.h / ATLAS_HEIGHT;
        
        // 頂点とテクスチャ座標を設定してクアッドを描画（OpenGLES用に修正）
        GLfloat vertices[] = {
            dst.x, dst.y,                 // 左上
            dst.x + dst.w, dst.y,         // 右上
            dst.x + dst.w, dst.y + dst.h, // 右下
            dst.x, dst.y + dst.h          // 左下
        };
        
        GLfloat texCoords[] = {
            tx, ty,           // 左上
            tx + tw, ty,      // 右上
            tx + tw, ty + th, // 右下
            tx, ty + th       // 左下
        };
        
        GLfloat colors[] = {
            r, g, b, a,
            r, g, b, a,
            r, g, b, a,
            r, g, b, a
        };
        
        // 頂点配列を使って描画
        glVertexPointer(2, GL_FLOAT, 0, vertices);
        glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
        glColorPointer(4, GL_FLOAT, 0, colors);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        
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
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color {
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
    
    // OpenGLES用にクアッドを描画
    GLfloat vertices[] = {
        x, y,         // 左上
        x + w, y,     // 右上
        x + w, y + h, // 右下
        x, y + h      // 左下
    };
    
    GLfloat colors[] = {
        r, g, b, a,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    };
    
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glColorPointer(4, GL_FLOAT, 0, colors);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
#endif
}

/**
 * アイコンの描画
 */
- (void)drawIcon:(int)id rect:(mu_Rect)rect color:(mu_Color)color {
    if (id < 0) return;
    
    // アイコンの情報を取得
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
    float tx = (float)src.x / ATLAS_WIDTH;
    float ty = (float)src.y / ATLAS_HEIGHT;
    float tw = (float)src.w / ATLAS_WIDTH;
    float th = (float)src.h / ATLAS_HEIGHT;
    
    // テクスチャをバインド
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    
    // OpenGLES用に描画
    GLfloat vertices[] = {
        dst.x, dst.y,                 // 左上
        dst.x + dst.w, dst.y,         // 右上
        dst.x + dst.w, dst.y + dst.h, // 右下
        dst.x, dst.y + dst.h          // 左下
    };
    
    GLfloat texCoords[] = {
        tx, ty,           // 左上
        tx + tw, ty,      // 右上
        tx + tw, ty + th, // 右下
        tx, ty + th       // 左下
    };
    
    GLfloat colors[] = {
        r, g, b, a,
        r, g, b, a,
        r, g, b, a,
        r, g, b, a
    };
    
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glTexCoordPointer(2, GL_FLOAT, 0, texCoords);
    glColorPointer(4, GL_FLOAT, 0, colors);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    
    glDisable(GL_TEXTURE_2D);
#endif
}

/**
 * クリップ矩形の設定
 */
- (void)setClipRect:(mu_Rect)rect viewHeight:(int)viewHeight {
    // OpenGLESの座標系に合わせてクリップを設定
    glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h);
}

#if USE_QUAD

/**
 * バッチ処理用バッファの初期化
 */
- (void)initBatchBuffers {
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
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured {
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
- (void)flushBatch {
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
    // 頂点配列の設定
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    if (batch_state.is_textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    }
    glVertexPointer(2, GL_FLOAT, sizeof(Vertex), &batch_state.vertices[0].x);
    glColorPointer(4, GL_FLOAT, sizeof(Vertex), &batch_state.vertices[0].r);
    if (batch_state.is_textured) {
        glTexCoordPointer(2, GL_FLOAT, sizeof(Vertex), &batch_state.vertices[0].u);
    }
    // インデックスで描画
    int quad_count = batch_state.vertex_count / 4;
    glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_INT, batch_state.indices);
    // 状態リセット
    batch_state.vertex_count = 0;
    batch_state.index_count = 0;
    glDisableClientState(GL_VERTEX_ARRAY);
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}

/**
 * バッチバッファのクリーンアップ
 */
- (void)cleanupBatchBuffers {
    // iOS OpenGLES1.1ではVBO未使用なので何もしない
}

#endif // USE_QUAD

#pragma mark - タッチイベント

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self];
    mu_input_mousemove(mu_ctx, pt.x, pt.y);
    mu_input_mousedown(mu_ctx, pt.x, pt.y, MU_MOUSE_LEFT);
    [self setNeedsDisplay];
}
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self];
    mu_input_mousemove(mu_ctx, pt.x, pt.y);
    [self setNeedsDisplay];
}
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint pt = [touch locationInView:self];
    mu_input_mouseup(mu_ctx, pt.x, pt.y, MU_MOUSE_LEFT);
    [self setNeedsDisplay];
}
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    [self touchesEnded:touches withEvent:event];
}

@end

#pragma mark - アプリケーションエントリポイント

@interface MicroUIViewController : UIViewController
@end
@implementation MicroUIViewController
- (void)loadView {
    self.view = [[MicroUIView alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
}
- (BOOL)prefersStatusBarHidden { return YES; }
@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([MicroUIViewController class]));
    }
}
