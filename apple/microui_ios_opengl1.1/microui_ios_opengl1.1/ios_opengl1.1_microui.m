#import <UIKit/UIKit.h>
#import <QuartzCore/QuartzCore.h>
#import <OpenGLES/ES1/gl.h>
#import <OpenGLES/ES1/glext.h>
#include "microui.h"
#include "atlas.inl"
#define USE_OPENGL
#include "microui_share.h" 
/*
 * MicroUI iOS OpenGL ES 1.1実装 - 開発履歴とMetal版との違い
 * -------------------------------------------------------------
 * 
 * 【実装履歴】
 * 2025-06-24: ドキュメント改善 - 実装の違いをより詳細に説明
 *   - OpenGLとMetalの違いを具体的なコード例とともに解説
 *   - 座標変換とイベント処理の仕組みを明確化
 *
 * 2025-06-23: ヘッダー状態管理の完全実装
 *   - Metal版と同様のヘッダーキャッシュとID管理システムを実装
 *   - トグル状態の永続化と同期を完全対応
 *   - MicroUI内部状態(treenode_pool)と永続ヘッダー状態の同期を改善
 *
 * 2025-06-22: ウィンドウドラッグとタップ処理の修正
 *   - Metal版のウィンドウドラッグ処理をポート
 *   - タッチイベント処理とヘッダーの検出・操作を改善
 *   - 座標変換系の問題を修正
 * 
 * 2025-06-21: OpenGL ES 1.1レンダリングの実装
 *   - OpenGL ES 1.1 APIを使用したレンダリング実装
 *   - 投影行列設定でY軸反転(UIKit座標系に合わせる)
 *   - パフォーマンス最適化とバッファ管理
 *
 * 【OpenGL ES 1.1版とMetal版の主な違い - 詳細解説】
 * 
 * 1. グラフィックAPIの基本的な違い
 *   - OpenGL ES 1.1: 
 *     - 固定機能パイプラインを使用（シェーダー不要）
 *     - 一つ一つの描画命令を直接実行（glVertexPointer → glDrawArrays）
 *     - 例: glDrawArrays(GL_TRIANGLE_FAN, 0, 4) で四角形を描画
 *     - シンプルだが、バッチ処理などの最適化が限定的
 *
 *   - Metal: 
 *     - プログラマブルシェーダーを使用（柔軟なGPUプログラミング）
 *     - コマンドをバッファにまとめてGPUに一括送信（効率的）
 *     - 複数の描画をバッチ処理して高速化
 *     - 複雑だが、パフォーマンスが高い
 * 
 * 2. 座標系の違いとその対応方法
 *   - 座標系の基本:
 *     - OpenGL標準: 左下原点(0,0)、Y軸上向き
 *     - UIKit/Metal/MicroUI: 左上原点(0,0)、Y軸下向き
 *
 *   - OpenGL ES 1.1での対応:
 *     - 投影行列変換で座標系を反転: glOrthof(0, width, height, 0, -1, 1)
 *     - シザーテストだけは特殊変換が必要: 
 *       glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h)
 *     - タッチ座標を明示的にスケーリング（532-595行目のconvertPointToOpenGL関数）
 *
 *   - Metal版での対応:
 *     - MTKViewとシェーダーが座標変換を自動的に処理
 *     - タッチ座標はほぼそのまま使用可能（変換不要）
 * 
 * 3. イベント処理とマウスエミュレーションの違い
 *   - 共通の課題:
 *     - MicroUI: マウス操作を前提としたAPI (mouseDown/mouseMove/mouseUp)
 *     - iOS: タッチベースのイベント (touchesBegan/Moved/Ended)
 *
 *   - OpenGL ES 1.1実装:
 *     - タッチ座標をcontentScaleFactorでスケーリング（高解像度対応）
 *     - convertPointToOpenGL()でUIKit→OpenGL座標変換
 *     - 具体例: touchesBegan (636-735行目) でタッチ→マウス変換
 *     - mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y) で座標設定
 *
 *   - Metal実装:
 *     - MTKViewが座標変換を内部処理するためシンプル
 *     - getTouchLocation()関数でタッチ座標をそのまま取得
 * 
 * 4. ヘッダー状態管理の実装方法
 *   - 問題の本質:
 *     - タップ時と描画時でヘッダー要素のIDを一致させる必要がある
 *     - UIKitはホバー状態がないため、擬似的に実装する必要がある
 *
 *   - 解決アプローチ（両実装共通）:
 *     - HeaderCache構造体: ヘッダーの位置情報をキャッシュ（レンダリング時に収集）
 *     - PersistentHeaderState: ヘッダーの開閉状態を永続保存
 *     - ios_detect_header_at_point: タップ位置のヘッダーを特定
 *     - ios_toggle_header_state: MicroUI内部と永続状態を同期
 *
 *   - OpenGL ES 1.1特有の処理:
 *     - mu_PoolItemのlast_updateフィールドへの直接アクセスによる状態管理
 *     - 詳細なデバッグログ出力による状態追跡
 * 
 * 5. ウィンドウドラッグ処理の違い
 *   - 共通実装:
 *     - タイトルバーヒットテスト→ドラッグ状態追跡→位置更新
 *
 *   - OpenGL ES 1.1実装:
 *     - windowNameAtTitleBarPoint関数でタイトルバー検出
 *     - touchesMovedでドラッグ処理（座標変換が必要）
 *     - CGFloat dx = uiPt.x - dragStartTouchPos.x 等で移動量計算
 *
 *   - Metal実装:
 *     - より洗練されたイベントキューイングシステム
 *     - 自動的なスケーリング処理
 *
 * 6. シザーテスト（描画領域制限）の違い
 *   - OpenGL ES 1.1:
 *     - OpenGLの座標系（左下原点）に合わせた特殊な変換が必要
 *     - glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h)
 *
 *   - Metal:
 *     - MTLScissorRect構造体と一貫した座標系
 *     - 自然な座標マッピングが可能
 *
 * 7. パフォーマンス特性
 *   - OpenGL ES 1.1:
 *     - シンプルな実装だが、状態切り替えが多くオーバーヘッドが大きい
 *     - 小さな描画が多いためGPU効率が低い場合がある
 *
 *   - Metal:
 *     - 効率的なバッチ処理と状態管理
 *     - GPUリソースの明示的管理による最適化
 *     - より高速だが、コードは複雑
 *
 * どちらの実装でも、見た目は同じMicroUIインターフェースを提供しますが、
 * 内部の実装は大きく異なります。コードのコメントやログを参照することで、
 * 各部分の詳細な実装の違いを確認できます。
 */

// --- MicroUI用コールバック ---
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) len = (int)strlen(text);
    int res = 0;
    for (int i = 0; i < len; i++) {
        unsigned char chr = (unsigned char)text[i];
        if (chr < 32 || chr >= 127) continue;
        int idx = ATLAS_FONT + chr;
        res += atlas[idx].w;
        if (i < len - 1) res += 1;
    }
    return res;
}
static int text_height(mu_Font font) { return 17; }
//static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
//   static float tmp;
//   int res;
//   mu_push_id(ctx, &value, sizeof(value));
//   tmp = *value;
//   res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
//   *value = tmp;
//   mu_pop_id(ctx);
//   return res;
//}

// --- MicroUI状態 ---
//static char logbuf[64000];
//static int logbuf_updated = 0;
static float bg[3] = {90, 95, 100};
static int prevWindowX = 40, prevWindowY = 40;

// --- UIView(OpenGL ES 1.1)実装 ---
@interface micro_ui_view : UIView {
    EAGLContext *context;
    GLuint framebuffer, renderbuffer;
    int viewWidth, viewHeight;
    mu_Context* mu_ctx;
    GLuint font_texture;
    CADisplayLink *displayLink;
    BOOL isDraggingWindow;
    CGPoint dragStartTouchPos;
    int dragStartWindowX, dragStartWindowY;
    NSString *draggingWindowName;
    
    // Metal版と同じヘッダーとタッチ関連の変数
    BOOL didTapHeader;
    CGPoint lastHeaderTapPosition;
}
@end

@implementation micro_ui_view

+ (Class)layerClass { return [CAEAGLLayer class]; }

- (instancetype)init_with_frame:(CGRect)frame {
    self = [super initWithFrame:frame];
    if (self) {
        [self setup_layer];
        [self setup_context];
        [self setup_buffers];
        [self setup_micro_ui];
        
        // シンプルなタッチ処理のためにマルチタッチを無効に
        self.multipleTouchEnabled = NO;
        
        // ディスプレイリンクの設定
        displayLink = [CADisplayLink displayLinkWithTarget:self selector:@selector(render)];
        [displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
        
        // displayLinkが正しく設定されたか確認
        if (displayLink) {
            NSLog(@"CADisplayLink 正常に設定されました - セレクタ: render");
        } else {
            NSLog(@"警告: CADisplayLink を設定できませんでした");
        }
        
        NSLog(@"MicroUIView初期化完了: フレームサイズ=%.1f x %.1f", frame.size.width, frame.size.height);
    }
    return self;
}

- (void)setup_layer {
    CAEAGLLayer *layer = (CAEAGLLayer*)self.layer;
    layer.opaque = YES;
    
    // contentScaleFactorの設定
    float scale = [[UIScreen mainScreen] scale];
    self.contentScaleFactor = scale;
    
    NSLog(@"レイヤー設定: スクリーンスケール=%.1f", scale);
    
    layer.drawableProperties = @{
        kEAGLDrawablePropertyRetainedBacking: @NO,
        kEAGLDrawablePropertyColorFormat: kEAGLColorFormatRGBA8
    };
}
- (void)setup_context {
    context = [[EAGLContext alloc] initWithAPI:kEAGLRenderingAPIOpenGLES1];
    [EAGLContext setCurrentContext:context];
}
- (void)setup_buffers {
    glGenFramebuffersOES(1, &framebuffer);
    glGenRenderbuffersOES(1, &renderbuffer);
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, renderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    glFramebufferRenderbufferOES(GL_FRAMEBUFFER_OES, GL_COLOR_ATTACHMENT0_OES, GL_RENDERBUFFER_OES, renderbuffer);
    GLint w, h;
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &w);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &h);
    viewWidth = w; viewHeight = h;
    NSLog(@"Framebuffer/Renderbuffer created: %dx%d", viewWidth, viewHeight);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) NSLog(@"OpenGL error after buffer setup: 0x%04x", err);
}
// UIViewのライフサイクルメソッドはキャメルケースのままにする必要があります
- (void)layoutSubviews {
    [EAGLContext setCurrentContext:context];
    // バッファの再生成は行わず、バインドとサイズ取得のみ
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, renderbuffer);
    [context renderbufferStorage:GL_RENDERBUFFER_OES fromDrawable:(CAEAGLLayer*)self.layer];
    GLint w, h;
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_WIDTH_OES, &w);
    glGetRenderbufferParameterivOES(GL_RENDERBUFFER_OES, GL_RENDERBUFFER_HEIGHT_OES, &h);
    viewWidth = w; viewHeight = h;
    NSLog(@"layoutSubviews: view size = %dx%d", viewWidth, viewHeight);
    [self render];
}
- (void)dealloc {
    [displayLink invalidate];
    if ([EAGLContext currentContext] == context) [EAGLContext setCurrentContext:nil];
    if (font_texture) glDeleteTextures(1, &font_texture);
    if (mu_ctx) free(mu_ctx);
    glDeleteFramebuffersOES(1, &framebuffer);
    glDeleteRenderbuffersOES(1, &renderbuffer);
}

- (void)setup_micro_ui {
    [EAGLContext setCurrentContext:context];
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        NSLog(@"OpenGL error after font texture setup: 0x%04x", err);
    } else {
        NSLog(@"Font texture created (id=%u)", font_texture);
    }
    mu_ctx = malloc(sizeof(mu_Context));
    mu_init(mu_ctx);
    mu_ctx->text_width = text_width;
    mu_ctx->text_height = text_height;
    
    // Metalバージョンと同様に永続的なヘッダー状態管理を初期化
    ios_init_header_states();
    
    // デフォルトでいくつかのヘッダーを展開状態に設定（Metalバージョンと同様）
    mu_Id testButtonsId = mu_get_id(mu_ctx, "Test Buttons", strlen("Test Buttons"));
    ios_set_header_state(testButtonsId, "Test Buttons", 1); // 明示的に1を使用
    
    mu_Id colorHeaderId = mu_get_id(mu_ctx, "Background Color", strlen("Background Color"));
    ios_set_header_state(colorHeaderId, "Background Color", 1); // 明示的に1を使用
    
    // 設定したヘッダーの状態を確認（デバッグ用）
    BOOL expanded = NO;
    ios_get_header_state(testButtonsId, &expanded);
    NSLog(@"初期ヘッダー状態確認: 'Test Buttons' (ID=%u) 状態=%@", 
          testButtonsId, expanded ? @"展開" : @"折りたたみ");
    
    expanded = NO;
    ios_get_header_state(colorHeaderId, &expanded);
    NSLog(@"初期ヘッダー状態確認: 'Background Color' (ID=%u) 状態=%@", 
          colorHeaderId, expanded ? @"展開" : @"折りたたみ");
    
    // MicroUIの座標系とOpenGLの座標系について明示的に表示
    NSLog(@"座標系設定: MicroUI/UIKit（左上原点、Y軸下）, OpenGL描画（左上原点、Y軸下になるよう投影行列を設定）");
    NSLog(@"シザーテストのみOpenGLネイティブ座標系（左下原点）を考慮して変換");
}

#pragma mark - MicroUIウィンドウ描画
- (void)draw_test_window {
    if (mu_begin_window(mu_ctx, "Demo Window", mu_rect(40, 40, 300, 450))) {
        mu_Container* win = mu_get_current_container(mu_ctx);
        win->rect.w = mu_max(win->rect.w, 240);
        win->rect.h = mu_max(win->rect.h, 300);
        if (win->rect.x != prevWindowX || win->rect.y != prevWindowY) {
            char msg[128];
            snprintf(msg, sizeof(msg), "Window moved to x=%d, y=%d", win->rect.x, win->rect.y);
            [self write_log:msg];
            prevWindowX = win->rect.x; prevWindowY = win->rect.y;
        }
        
        // Window Info ヘッダー（永続的な状態管理によるヘッダー）
        const char* windowInfoLabel = "Window Info";
        int windowInfoResult = ios_header(mu_ctx, windowInfoLabel, 0);
        if (windowInfoResult) {
            int layout1[2] = {54, -1};
            mu_layout_row(mu_ctx, 2, layout1, 0);
            mu_label(mu_ctx, "Position:");
            char buf[32]; snprintf(buf, sizeof(buf), "%d, %d", win->rect.x, win->rect.y); mu_label(mu_ctx, buf);
            mu_label(mu_ctx, "Size:");
            snprintf(buf, sizeof(buf), "%d, %d", win->rect.w, win->rect.h); mu_label(mu_ctx, buf);
        }
        
        // Test Buttons ヘッダー（永続的な状態管理によるヘッダー）
        const char* testButtonsLabel = "Test Buttons";
        int testButtonsResult = ios_header(mu_ctx, testButtonsLabel, 0);
        if (testButtonsResult) {
            int layout2[3] = {86, -110, -1};
            mu_layout_row(mu_ctx, 3, layout2, 0);
            mu_label(mu_ctx, "Test buttons 1:");
            if (mu_button(mu_ctx, "Button 1")) [self write_log:"Pressed button 1"];
            if (mu_button(mu_ctx, "Button 2")) [self write_log:"Pressed button 2"];
        }
        
        // Background Color ヘッダー（永続的な状態管理によるヘッダー）
        const char* bgColorLabel = "Background Color";
        int bgColorResult = ios_header(mu_ctx, bgColorLabel, 0);
        if (bgColorResult) {
            int layout6[2] = {-78, -1}, layout7[2] = {46, -1};
            mu_layout_row(mu_ctx, 2, layout6, 74);
            mu_layout_begin_column(mu_ctx);
            mu_layout_row(mu_ctx, 2, layout7, 0);
            mu_label(mu_ctx, "Red:");   mu_slider(mu_ctx, &bg[0], 0, 255);
            mu_label(mu_ctx, "Green:"); mu_slider(mu_ctx, &bg[1], 0, 255);
            mu_label(mu_ctx, "Blue:");  mu_slider(mu_ctx, &bg[2], 0, 255);
            mu_layout_end_column(mu_ctx);
            mu_Rect r = mu_layout_next(mu_ctx);
            mu_draw_rect(mu_ctx, r, mu_color(bg[0], bg[1], bg[2], 255));
            char buf[16]; snprintf(buf, sizeof(buf), "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
            mu_draw_control_text(mu_ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
        }
        mu_end_window(mu_ctx);
    }
}
- (void)draw_log_window {
    if (mu_begin_window(mu_ctx, "Log Window", mu_rect(350, 40, 300, 200))) {
        int layout1[1] = {-1}, layout2[2] = {-70, -1};
        mu_layout_row(mu_ctx, 1, layout1, -25);
        mu_begin_panel(mu_ctx, "Log Output");
        mu_Container* panel = mu_get_current_container(mu_ctx);
        mu_layout_row(mu_ctx, 1, layout1, -1);
        mu_text(mu_ctx, logbuf);
        mu_end_panel(mu_ctx);
        if (logbuf_updated) { panel->scroll.y = panel->content_size.y; logbuf_updated = 0; }
        static char buf[128];
        int submitted = 0;
        mu_layout_row(mu_ctx, 2, layout2, 0);
        if (mu_textbox(mu_ctx, buf, sizeof(buf)) & MU_RES_SUBMIT) { mu_set_focus(mu_ctx, mu_ctx->last_id); submitted = 1; }
        if (mu_button(mu_ctx, "Submit")) submitted = 1;
        if (submitted) { [self write_log:buf]; buf[0] = '\0'; }
        mu_end_window(mu_ctx);
    }
}
- (void)draw_style_window {
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

#pragma mark - MicroUIコマンド描画
- (void)draw_micro_ui {
    mu_Command* cmd = NULL;
    while (mu_next_command(mu_ctx, &cmd)) {
        switch (cmd->type) {
            case MU_COMMAND_TEXT: {
                const char* text = cmd->text.str;
                mu_Vec2 pos = cmd->text.pos;
                mu_Color color = cmd->text.color;
                if (!text || !text[0]) break;
                glBindTexture(GL_TEXTURE_2D, font_texture);
                glEnable(GL_TEXTURE_2D);
                glColor4f(color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0);
                float x = pos.x, y = pos.y;
                for (const char* p = text; *p; p++) {
                    unsigned char chr = (unsigned char)*p;
                    if (chr < 32 || chr >= 127) continue;
                    mu_Rect src = atlas[ATLAS_FONT + chr];
                    float tx = (float)src.x / ATLAS_WIDTH, ty = (float)src.y / ATLAS_HEIGHT;
                    float tw = (float)src.w / ATLAS_WIDTH, th = (float)src.h / ATLAS_HEIGHT;
                    float w = src.w, h = src.h;
                    GLfloat vtx[] = { x, y, x+w, y, x+w, y+h, x, y+h };
                    GLfloat uv[]  = { tx, ty, tx+tw, ty, tx+tw, ty+th, tx, ty+th };
                    glEnableClientState(GL_VERTEX_ARRAY);
                    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                    glVertexPointer(2, GL_FLOAT, 0, vtx);
                    glTexCoordPointer(2, GL_FLOAT, 0, uv);
                    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                    glDisableClientState(GL_VERTEX_ARRAY);
                    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                    x += w + 1;
                }
                glDisable(GL_TEXTURE_2D);
                break;
            }
            case MU_COMMAND_RECT: {
                mu_Rect rect = cmd->rect.rect;
                mu_Color color = cmd->rect.color;
                glDisable(GL_TEXTURE_2D);
                glColor4f(color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0);
                GLfloat vtx[] = {
                    rect.x, rect.y,
                    rect.x+rect.w, rect.y,
                    rect.x+rect.w, rect.y+rect.h,
                    rect.x, rect.y+rect.h
                };
                glEnableClientState(GL_VERTEX_ARRAY);
                glVertexPointer(2, GL_FLOAT, 0, vtx);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glDisableClientState(GL_VERTEX_ARRAY);
                break;
            }
            case MU_COMMAND_ICON: {
                int id = cmd->icon.id;
                mu_Rect rect = cmd->icon.rect;
                mu_Color color = cmd->icon.color;
                mu_Rect src = atlas[id];
                int x = rect.x + (rect.w - src.w)/2, y = rect.y + (rect.h - src.h)/2;
                float tx = (float)src.x / ATLAS_WIDTH, ty = (float)src.y / ATLAS_HEIGHT;
                float tw = (float)src.w / ATLAS_WIDTH, th = (float)src.h / ATLAS_HEIGHT;
                glBindTexture(GL_TEXTURE_2D, font_texture);
                glEnable(GL_TEXTURE_2D);
                glColor4f(color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0);
                GLfloat vtx[] = { x, y, x+src.w, y, x+src.w, y+src.h, x, y+src.h };
                GLfloat uv[]  = { tx, ty, tx+tw, ty, tx+tw, ty+th, tx, ty+th };
                glEnableClientState(GL_VERTEX_ARRAY);
                glEnableClientState(GL_TEXTURE_COORD_ARRAY);
                glVertexPointer(2, GL_FLOAT, 0, vtx);
                glTexCoordPointer(2, GL_FLOAT, 0, uv);
                glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
                glDisableClientState(GL_VERTEX_ARRAY);
                glDisableClientState(GL_TEXTURE_COORD_ARRAY);
                glDisable(GL_TEXTURE_2D);
                break;
            }
            case MU_COMMAND_CLIP: {
                mu_Rect rect = cmd->clip.rect;
                glEnable(GL_SCISSOR_TEST);
                
                // 重要な座標変換の違い：OpenGLのシザーテスト座標系
                // 問題点：glOrthofでY軸を反転させても、シザーテストは常にOpenGLネイティブ座標系（左下原点）を使用する
                // Metal版：MTKViewとシェーダーで自動的に座標変換が処理される
                // OpenGL版：明示的に座標変換が必要 - MicroUI座標（左上原点）→OpenGLネイティブ座標（左下原点）
                // 変換式：OpenGL_Y = ViewHeight - (MicroUI_Y + Height)
                glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h);
                break;
            }
        }
        GLenum err = glGetError();
        if (err != GL_NO_ERROR) NSLog(@"OpenGL error in drawMicroUI: 0x%04x", err);
    }
    glDisable(GL_SCISSOR_TEST);
}
- (void)set_clip_rect:(mu_Rect)rect {
    glEnable(GL_SCISSOR_TEST);
    // OpenGLでのシザーテストは左下原点なので、Y座標を変換する必要がある
    glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h);
}
- (void)write_log:(const char*)text {
    if (logbuf[0]) strcat(logbuf, "\n");
    strcat(logbuf, text);
    logbuf_updated = 1;
}

#pragma mark - OpenGL描画
// UIKitから呼び出されるメソッドはキャメルケースのままにする必要があります
- (void)render {
    // デバッグ: 初回のみrender呼び出しを記録
    static BOOL firstRenderCalled = YES;
    if (firstRenderCalled) {
        NSLog(@"初回render呼び出し確認");
        firstRenderCalled = NO;
    }
    
    if (!context || ![EAGLContext setCurrentContext:context]) {
        NSLog(@"OpenGLコンテキストが無効");
        return;
    }
    if (viewWidth == 0 || viewHeight == 0) {
        NSLog(@"Skip render: view size is zero");
        return;
    }
    
    // デバッグ情報を出力
    static BOOL firstRender = YES;
    if (firstRender) {
        NSLog(@"初回レンダリング: viewWidth=%d, viewHeight=%d, contentScale=%.1f", 
              viewWidth, viewHeight, self.contentScaleFactor);
        firstRender = NO;
    }
    
    glBindFramebufferOES(GL_FRAMEBUFFER_OES, framebuffer);
    glViewport(0, 0, viewWidth, viewHeight);
    glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // 正射影設定 - 重要な座標系の設定
    // OpenGL ES 1.1の座標系（左下原点、Y軸上向き）をMicroUIの座標系（左上原点、Y軸下向き）に合わせる
    // Metal版はMTKViewがカメラ設定を通じて同様の変換を行うが、OpenGL版では明示的に行う必要がある
    // 以下の設定により、OpenGL描画座標系がUIKit/MicroUIの座標系と一致するようになる
    glMatrixMode(GL_PROJECTION); 
    glLoadIdentity();
    // Y軸反転のための特殊な設定（0,0を左上に、Y軸を下向きに）
    glOrthof(0, viewWidth, viewHeight, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW); 
    glLoadIdentity();
    
    // MicroUIの描画
    mu_begin(mu_ctx);
    
    // 描画前にヘッダーキャッシュをクリア（Metal版と同様）
    ios_clear_header_cache();
    
    // タイトルバードラッグ中のウィンドウは最後に描画して前面に表示
    NSMutableArray *windowsToDraw = [@[@"Log Window", @"Style Editor", @"Demo Window"] mutableCopy];
    
    // ドラッグ中のウィンドウがあれば最後に描画するように順序を変更
    if (isDraggingWindow && draggingWindowName) {
        [windowsToDraw removeObject:draggingWindowName];
        [windowsToDraw addObject:draggingWindowName];
    }
    
    // 描画順序に従ってウィンドウを描画
    for (NSString *windowName in windowsToDraw) {
        if ([windowName isEqualToString:@"Demo Window"]) [self draw_test_window];
        else if ([windowName isEqualToString:@"Log Window"]) [self draw_log_window];
        else if ([windowName isEqualToString:@"Style Editor"]) [self draw_style_window];
    }
    
    mu_end(mu_ctx);
    [self draw_micro_ui];
    
    glBindRenderbufferOES(GL_RENDERBUFFER_OES, renderbuffer);
    [context presentRenderbuffer:GL_RENDERBUFFER_OES];
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) NSLog(@"OpenGL error after present: 0x%04x", err);
}

#pragma mark - タッチイベント・ウィンドウドラッグ
// --- iOS向けMicroUI拡張 ---
// ホバー状態を適切に管理するヘルパー関数
 void ios_fix_hover_state(mu_Context* ctx) {
    if (!ctx) return;
    
    // 異常な値のチェックと修正（負の値は無効）
    if (ctx->hover < 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
        return;
    }
    
    // タッチ中（mouse_downがON）の場合はフォーカス要素とホバー要素を同期
    if (ctx->mouse_down && ctx->focus != 0) {
        ctx->hover = ctx->focus;
        ctx->prev_hover = ctx->focus;
    } 
    // タッチが終了した場合（mouse_downがOFF）はホバー状態をクリア
    else if (!ctx->mouse_down && ctx->mouse_pressed == 0) {
        ctx->hover = 0;
        ctx->prev_hover = 0;
    }
}

// 安全なホバー状態設定関数
static void ios_set_hover(mu_Context* ctx, mu_Id id) {
    if (!ctx) return;
    
    // 現在のホバー状態と同じ場合は何もしない
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

// タイトルバーID計算用ヘルパー
static mu_Id calculate_title_id(mu_Context *ctx, const char *name) {
    if (!ctx || !name) return 0;
    
    char titleBuf[64];
    snprintf(titleBuf, sizeof(titleBuf), "%s!title", name);
    return mu_get_id(ctx, titleBuf, strlen(titleBuf));
}

- (mu_Id)calculate_title_id:(mu_Context *)ctx window_name:(const char *)name {
    // 静的ヘルパー関数を使用
    mu_Id id = calculate_title_id(ctx, name);
    NSLog(@"タイトルIDを計算: ウィンドウ名='%s', タイトルID=%u", name, id);
    return id;
}

// UIKit座標からOpenGL座標へ変換するヘルパー
// UIKitとMicroUIはともにY軸が下向きなので変換は単純にスケーリングのみ
- (CGPoint)convert_point_to_open_gl:(CGPoint)pt {
    // contentScaleFactorを考慮
    float scale = self.contentScaleFactor;
    return CGPointMake(pt.x * scale, pt.y * scale);
}

// タイトルバーのヒットテスト（UIKit座標系で）
- (NSString*)window_name_at_title_bar_point:(CGPoint)pt {
    // OpenGL座標系に変換
    CGPoint oglPt = [self convert_point_to_open_gl:pt];
    
    NSLog(@"タイトルバーヒットテスト: UIKit座標=(%.1f,%.1f), OpenGL座標=(%.1f,%.1f), スケール=%.1f", 
          pt.x, pt.y, oglPt.x, oglPt.y, self.contentScaleFactor);
    
    NSArray *names = @[ @"Demo Window", @"Log Window", @"Style Editor" ];
    
    for (NSString *name in names) {
        mu_Container *c = mu_get_container(mu_ctx, name.UTF8String);
        if (!c) {
            NSLog(@"ウィンドウが見つかりません: %@", name);
            continue;
        }
        
        int titleHeight = mu_ctx && mu_ctx->style ? mu_ctx->style->title_height : 24;
        mu_Rect r = c->rect;
        r.h = titleHeight;
        
        // デバッグ：タイトルバー領域を表示
        NSLog(@"タイトルバー領域 %@: x=%d, y=%d, w=%d, h=%d",
              name, r.x, r.y, r.w, r.h);
        
        // OpenGL座標系でヒットテスト
        if (oglPt.x >= r.x && oglPt.x < r.x + r.w &&
            oglPt.y >= r.y && oglPt.y < r.y + r.h) {
            NSLog(@"タイトルバーヒット成功: %@", name);
            return name;
        }
    }
    
    // ヒットしなかった場合はデバッグ情報を出力
    NSLog(@"タイトルバーヒット失敗: どのウィンドウにもヒットしませんでした");
    return nil;
}

// UIKitのタッチイベントメソッドはキャメルケースのままにする必要があります
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint uiPt = [touch locationInView:self];
    CGPoint oglPt = [self convert_point_to_open_gl:uiPt];
    
    // NSLog記録を追加（デバッグ用）
    NSLog(@"touchesBegan: UIKit座標=(%.1f,%.1f), OpenGL座標=(%.1f,%.1f), スケール=%.1f", 
          uiPt.x, uiPt.y, oglPt.x, oglPt.y, self.contentScaleFactor);
    
    // マイクロUIの現在のマウス位置を更新
    mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y);
    
    // 前回の状態をクリーンアップ（Metalバージョンと同様）
    ios_set_hover(mu_ctx, 0);
    
    // Metal版と同様に、タップしたものが何かを判定する変数
    BOOL didTapHeader = NO;
    CGPoint lastHeaderTapPosition = CGPointZero;
    
    // ウィンドウのタイトルバーをタップしたか確認
    NSLog(@"タイトルバーヒットテスト開始...");
    NSString *hitWindow = [self window_name_at_title_bar_point:uiPt];
    NSLog(@"タイトルバーヒットテスト結果: %@", hitWindow ? hitWindow : @"ヒットなし");
    
    // ヘッダー要素の検出（Metal版と同様）
    HeaderInfo headerInfo = {0};
    BOOL isHeaderTouch = ios_detect_header_at_point(mu_ctx, oglPt, &headerInfo);
    
    if (hitWindow) {
        // タイトルバーがタップされた場合の処理
        isDraggingWindow = YES;
        draggingWindowName = hitWindow;
        dragStartTouchPos = uiPt;
        
        mu_Container *win = mu_get_container(mu_ctx, hitWindow.UTF8String);
        if (win) {
            dragStartWindowX = win->rect.x;
            dragStartWindowY = win->rect.y;
            
            // ウィンドウを最前面に（Metal版と同様）
            mu_bring_to_front(mu_ctx, win);
            
            // タイトルバーIDを設定
            mu_Id titleId = [self calculate_title_id:mu_ctx window_name:[hitWindow UTF8String]];
            
            // フォーカスとホバー状態を一貫して設定（Metal版と同様）
            mu_set_focus(mu_ctx, titleId);
            ios_set_hover(mu_ctx, titleId);
            
            // マウス状態を更新（Metal版と同様）
            mu_ctx->mouse_down = MU_MOUSE_LEFT;
            mu_ctx->mouse_pressed = MU_MOUSE_LEFT;
            
            NSLog(@"タイトルバードラッグ開始: %@, ウィンドウ位置=(%d,%d), ID=%u", 
                  hitWindow, dragStartWindowX, dragStartWindowY, titleId);
            
            // Metal版と同様、タイトルバーでの処理後は他の処理はスキップ
            return;
        }
    } else if (isHeaderTouch) {
        // ヘッダーのタップ処理（Metal版と同様に完全再実装）
        didTapHeader = YES;
        lastHeaderTapPosition = oglPt;
        
        NSLog(@"ヘッダーをタップ: '%s' (ID=%u, poolIndex=%d)", 
              headerInfo.label, headerInfo.id, headerInfo.poolIndex);
        
        // ヘッダーID設定とフォーカスの設定
        mu_set_focus(mu_ctx, headerInfo.id);
        ios_set_hover(mu_ctx, headerInfo.id);
        
        // マウス押下を設定してヘッダーロジックをトリガー
        mu_ctx->mouse_down = MU_MOUSE_LEFT;
        mu_ctx->mouse_pressed = MU_MOUSE_LEFT;
        
        // ヘッダーの状態を切り替え（Metal版と同様に実装）
        ios_toggle_header_state(mu_ctx, headerInfo.id, headerInfo.label);
        
        // MicroUI内部状態も正しく更新されているか確認
        int poolIndex = mu_pool_get(mu_ctx, mu_ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerInfo.id);
        
        // 現在の状態をログ出力
        BOOL currentExpanded = NO;
        ios_get_header_state(headerInfo.id, &currentExpanded);
        
        NSLog(@"ヘッダー状態切り替え後: '%s' (ID=%u) treenode_pool[%d]=%d, 永続状態=%d",
             headerInfo.label, headerInfo.id, poolIndex, 
             poolIndex >= 0 ? mu_ctx->treenode_pool[poolIndex].last_update : -1, 
             currentExpanded ? 1 : 0);
        
        // ヘッダータップ後は他のイベント処理をスキップ
        return;
    }
    
    // タイトルバーでもヘッダーでもない場合は通常のマウスイベント処理
    // Metalバージョンと同様、イベントを先に適用
    mu_ctx->mouse_down = MU_MOUSE_LEFT;
    mu_ctx->mouse_pressed = MU_MOUSE_LEFT;
    
    // 位置情報を更新（座標は既に設定済み）
    mu_input_mousemove(mu_ctx, oglPt.x, oglPt.y);
}

// UIKitのタッチイベントメソッドはキャメルケースのままにする必要があります
- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint uiPt = [touch locationInView:self];
    CGPoint oglPt = [self convert_point_to_open_gl:uiPt];
    
    // マウス位置の更新
    mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y);
    
    // ドラッグ状態の判定変数（Metal版と同様）
    BOOL isDraggingTitle = (mu_ctx && mu_ctx->mouse_down == MU_MOUSE_LEFT && 
                            mu_ctx->focus != 0 && isDraggingWindow && draggingWindowName);
    
    // ウィンドウドラッグの処理
    if (isDraggingTitle) {
        mu_Container *win = mu_get_container(mu_ctx, [draggingWindowName UTF8String]);
        if (win) {
            // Metal版と同様、浮動小数点での計算を維持してから整数に変換
            CGFloat dx = uiPt.x - dragStartTouchPos.x;
            CGFloat dy = uiPt.y - dragStartTouchPos.y;
            
            // スケールファクタを考慮して適用
            CGFloat scale = self.contentScaleFactor;
            CGFloat scaledDx = dx * scale;
            CGFloat scaledDy = dy * scale;
            
            // 丸めを適用（より滑らかな動き）
            int newX = dragStartWindowX + (int)roundf(scaledDx);
            int newY = dragStartWindowY + (int)roundf(scaledDy);
            
            win->rect.x = newX;
            win->rect.y = newY;
            
            // 重要なドラッグ状態の変化のみログ出力
            static int lastLoggedX = 0, lastLoggedY = 0;
            if (abs(lastLoggedX - newX) > 5 || abs(lastLoggedY - newY) > 5) {
                lastLoggedX = newX;
                lastLoggedY = newY;
                NSLog(@"ウィンドウドラッグ中: %@ 移動量=(%.1f,%.1f) 新位置=(%d,%d)", 
                     draggingWindowName, dx, dy, newX, newY);
            }
            
            // ドラッグ中はタイトルバーIDのフォーカスとホバーを維持
            mu_Id titleId = [self calculate_title_id:mu_ctx window_name:[draggingWindowName UTF8String]];
            mu_ctx->focus = titleId;
            ios_set_hover(mu_ctx, titleId);
            return;
        }
    }
    
    // ドラッグ中でない場合のみ通常のマウス移動イベントを送信
    mu_input_mousemove(mu_ctx, oglPt.x, oglPt.y);
    
    // Metal版と同様にホバー状態を修正
    ios_fix_hover_state(mu_ctx);
}

// UIKitのタッチイベントメソッドはキャメルケースのままにする必要があります
- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint uiPt = [touch locationInView:self];
    CGPoint oglPt = [self convert_point_to_open_gl:uiPt];
    
    // NSLog記録を追加（デバッグ用）
    NSLog(@"touchesEnded: UIKit座標=(%.1f,%.1f), OpenGL座標=(%.1f,%.1f)", 
          uiPt.x, uiPt.y, oglPt.x, oglPt.y);
    
    // マウス位置の更新
    mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y);
    
    // 状態をログ（Metal版と同様）
    char beforeMsg[128];
    snprintf(beforeMsg, sizeof(beforeMsg), 
             "タッチ終了前: focus=%u, hover=%u, mouse_down=%d, dragging=%s",
             mu_ctx->focus, mu_ctx->hover, mu_ctx->mouse_down, 
             isDraggingWindow ? "YES" : "NO");
    NSLog(@"%s", beforeMsg);
    
    // ドラッグ状態を保存してからリセット
    BOOL wasWindowDrag = isDraggingWindow;
    NSString *draggedWindow = draggingWindowName;
    
    // ドラッグ状態をリセット
    isDraggingWindow = NO;
    draggingWindowName = nil;
    
    // ドラッグ終了の場合は特別処理
    if (wasWindowDrag && draggedWindow) {
        mu_Container *win = mu_get_container(mu_ctx, [draggedWindow UTF8String]);
        if (win) {
            NSLog(@"ウィンドウドラッグ完了: %@, 最終位置=(%d,%d)", 
                  draggedWindow, win->rect.x, win->rect.y);
        }
    }
    
    // Metal版と同様に状態をクリーンアップ
    mu_ctx->mouse_down = 0;
    mu_ctx->mouse_pressed = 0;
    ios_set_hover(mu_ctx, 0);  // ホバー状態を明示的にクリア
    
    // 状態変更をログ（Metal版と同様）
    char afterMsg[128];
    snprintf(afterMsg, sizeof(afterMsg),
             "タッチ終了後: focus=%u, hover=%u, mouse_down=%d",
             mu_ctx->focus, mu_ctx->hover, mu_ctx->mouse_down);
    NSLog(@"%s", afterMsg);
}
// UIKitのタッチイベントメソッドはキャメルケースのままにする必要があります
- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint uiPt = [touch locationInView:self];
    CGPoint oglPt = [self convert_point_to_open_gl:uiPt];
    
    NSLog(@"touchesCancelled: UIKit座標=(%.1f,%.1f), OpenGL座標=(%.1f,%.1f)", 
          uiPt.x, uiPt.y, oglPt.x, oglPt.y);
    
    // Metal版と同様に、すべてのケースで状態をリセット
    mu_ctx->mouse_down = 0;
    mu_ctx->mouse_pressed = 0;
    ios_set_hover(mu_ctx, 0);  // ホバー状態を明示的にクリア
    
    // ドラッグ状態のリセット
    isDraggingWindow = NO;
    draggingWindowName = nil;
    
    // マウス位置の更新
    mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y);
    
    // Metal版と同様、イベントキャンセル時は状態のリセットのみを行い
    // 追加のイベント送信は行わない
}

#pragma mark - ヘッダー要素キャッシュ（Metal版と同様）

// ヘッダー矩形キャッシュ（Metal版と同じ構造）
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
        
        NSLog(@"ヘッダーをキャッシュに追加: '%s' (ID=%u), 矩形=(%d,%d,%d,%d), 状態=%s", 
              label, id, rect.x, rect.y, rect.w, rect.h, active ? "展開" : "折りたたみ");
    }
}

// キャッシュ情報をクリア（フレーム開始時に呼び出す）
static void ios_clear_header_cache() {
    headerCacheCount = 0;
}

// キャッシュから指定座標にあるヘッダーを検索
static BOOL ios_find_header_at_position(CGFloat x, CGFloat y, HeaderCache** outCache) {
    for (int i = 0; i < headerCacheCount; i++) {
        HeaderCache* cache = &headerCache[i];
        
        if (x >= cache->rect.x && x < cache->rect.x + cache->rect.w &&
            y >= cache->rect.y && y < cache->rect.y + cache->rect.h) {
            
            if (outCache) {
                *outCache = cache;
            }
            
            NSLog(@"キャッシュでヘッダー検出: '%s' (ID=%u) 座標=(%.1f,%.1f)", 
                 cache->label, cache->id, x, y);
            
            return YES;
        }
    }
    return NO;
}

// 指定ラベルのヘッダーがクリックされたかをチェック
static BOOL ios_is_header_clicked(mu_Context* ctx, const char* label, CGFloat x, CGFloat y) {
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    
    // キャッシュを検索
    for (int i = 0; i < headerCacheCount; i++) {
        if (headerCache[i].id == id) {
            mu_Rect rect = headerCache[i].rect;
            return (x >= rect.x && x < rect.x + rect.w && 
                   y >= rect.y && y < rect.y + rect.h);
        }
    }
    return NO;
}

// 指定ラベルのヘッダーIDを取得
 mu_Id ios_get_header_id(mu_Context* ctx, const char* label) {
    return mu_get_id(ctx, label, strlen(label));
}

// プルダウンヘッダーの検出用構造体
typedef struct {
    mu_Id id;
    mu_Rect rect;
    const char* label;
    int poolIndex;
} HeaderInfo;

// 指定位置のヘッダーを検出する（Metal版と同様）
/*
 * ヘッダー検出とステート管理の重要点：
 * 1. タップとレンダリングの一貫性
 *    - iOSではマウスホバーがなく、タップ時とレンダリング時でヘッダーIDを一致させる必要がある
 *    - Metal版とOpenGL版の両方で、キャッシュシステムを使用してこの一致を保証
 * 
 * 2. ステート管理の階層
 *    - mu_Context内部のtreenode_pool：MicroUIのウィジェット状態を保持
 *    - 永続ヘッダー状態：アプリ独自の状態管理（PersistentHeaderState構造体）
 *    - ヘッダーキャッシュ：描画後の位置情報をタップ検出に使用（HeaderCache構造体）
 *
 * 3. 状態同期の仕組み
 *    - 描画時：ios_header_wrapper関数で状態を確認・キャッシュ
 *    - タップ時：ios_detect_header_at_pointでID特定、ios_toggle_header_stateで状態変更
 *    - 状態変更：永続状態とtreenode_pool両方を同期的に更新
 */
static BOOL ios_detect_header_at_point(mu_Context* ctx, CGPoint point, HeaderInfo* outInfo) {
    if (!ctx || !outInfo) return NO;

    // まずキャッシュから検索（Metal版と同じ方式）
    // キャッシュにより、描画時とタップ時で同じIDが使用されることを保証
    HeaderCache* cache = NULL;
    if (ios_find_header_at_position(point.x, point.y, &cache)) {
        outInfo->id = cache->id;
        outInfo->rect = cache->rect;
        outInfo->label = cache->label;
        outInfo->poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, cache->id);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "ヘッダー検出（キャッシュ）: '%s' ID=%u 位置=(%d,%d) poolIndex=%d", 
               cache->label, cache->id, cache->rect.x, cache->rect.y, outInfo->poolIndex);
        NSLog(@"%s", msg);
        
        return YES;
    }

    // キャッシュになければウィンドウ内のヘッダーをスキャン
    mu_Container* win = mu_get_container(ctx, "Demo Window");
    if (!win) {
        NSLog(@"ウィンドウが見つかりません");
        return NO;
    }

    // ウィンドウ内の座標かチェック
    if (point.x < win->rect.x || point.x >= win->rect.x + win->rect.w ||
        point.y < win->rect.y || point.y >= win->rect.y + win->rect.h) {
        NSLog(@"ウィンドウ外のタップを検出");
        return NO;
    }

    // 既知のヘッダーをチェック
    const char* knownHeaders[] = {
        "Test Buttons",
        "Background Color",
        "Window Info",
        NULL
    };

    for (const char** label = knownHeaders; *label != NULL; label++) {
        if (ios_is_header_clicked(ctx, *label, point.x, point.y)) {
            mu_Id id = ios_get_header_id(ctx, *label);
            outInfo->id = id;
            
            // キャッシュから矩形情報を取得
            for (int i = 0; i < headerCacheCount; i++) {
                if (headerCache[i].id == id) {
                    outInfo->rect = headerCache[i].rect;
                    break;
                }
            }
            
            outInfo->label = *label;
            outInfo->poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
            
            char msg[128];
            snprintf(msg, sizeof(msg), "ヘッダー検出（スキャン）: '%s' ID=%u 位置=(%d,%d) poolIndex=%d", 
                   *label, id, outInfo->rect.x, outInfo->rect.y, outInfo->poolIndex);
            NSLog(@"%s", msg);
            
            return YES;
        }
    }
    
    return NO;
}

// 直接ヘッダーの状態を切り替える（Metal版と同様に実装）
/*
 * ヘッダー状態トグル処理の重要点：
 *
 * 1. 二重状態管理の同期
 *    - 問題：MicroUIはtreenode_poolで状態を管理、iOSはそれとは別に永続状態が必要
 *    - Metal版：永続状態とtreenode_poolの両方を明示的に更新
 *    - OpenGL版：同様に両方を更新し、常に同期を保つ
 * 
 * 2. mu_PoolItem構造体の理解
 *    - treenode_pool：ヘッダー開閉状態を保持するMicroUIの内部配列
 *    - last_update：状態を実際に保持するフィールド（0=閉、1=開）
 *    - Metal版とOpenGL版の違い：Metal版はワードサイズのcastを使用、OpenGL版は明示的にフィールドにアクセス
 *
 * 3. タイミングの違い
 *    - Metal版：タップ時に即座に状態が変わり、次の描画で反映
 *    - OpenGL版：同じ流れだが、明示的なプール更新と永続状態の同期が必要
 */
 void ios_toggle_header_state(mu_Context* ctx, mu_Id id, const char* label) {
    if (!ctx || id == 0) return;
    
    // 現在の永続的状態を取得
    BOOL currentExpanded = NO;
    ios_get_header_state(id, &currentExpanded);
    
    // 永続的状態を反転
    BOOL newState = !currentExpanded;
    
    // 永続的状態を更新
    ios_set_header_state(id, label, newState);
    
    // MicroUI内部状態（treenode_pool）も同期して更新（これが重要）
    // Metal版とOpenGL版の重要な違い：
    // - Metal版：(int)をキャストで直接代入
    // - OpenGL版：mu_PoolItemの.last_updateフィールドに値を設定
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    if (idx >= 0) {
        // mu_PoolItemは構造体なので、last_updateフィールドを使って状態を記録
        ctx->treenode_pool[idx].last_update = newState ? 1 : 0;
        NSLog(@"treenodeプール状態を更新: ID=%u, インデックス=%d -> %d", 
             id, idx, ctx->treenode_pool[idx].last_update);
    } else {
        // プールにエントリがない場合は新しく追加
        idx = mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
        if (idx >= 0) {
            ctx->treenode_pool[idx].last_update = newState ? 1 : 0;
            NSLog(@"treenodeプール状態を新規追加: ID=%u, インデックス=%d -> %d", 
                 id, idx, ctx->treenode_pool[idx].last_update);
        }
    }
    
    NSLog(@"ヘッダー状態を切り替え: '%s' (ID=%u) -> %@", 
          label ? label : "不明", id, newState ? @"展開" : @"折りたたみ");
}

#pragma mark - MicroUI拡張（ヘッダーキャッシュ）

// MicroUIのヘッダー描画をフックするラッパー関数（Metal版と同様に実装）
static int ios_header_wrapper(mu_Context* ctx, const char* label, int istreenode, int opt) {
    // ヘッダーIDを取得
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    
    // 永続的な状態を取得
    BOOL expanded = NO;
    BOOL found = ios_get_header_state(id, &expanded);
    
    // パフォーマンスのためにプールインデックスも取得
    int poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    
    // 状態ログ
    NSLog(@"ios_header_wrapper: '%s' (ID=%u) 永続状態=%@, 見つかりました=%@, プールインデックス=%d", 
          label, id, expanded ? @"展開" : @"折りたたみ", 
          found ? @"YES" : @"NO", poolIndex);
    
    // プールの状態と永続的状態の同期を確認
    if (found && poolIndex >= 0) {
        // プールの状態と永続状態に不一致がある場合は修正
        if ((ctx->treenode_pool[poolIndex].last_update != 0) != expanded) {
            NSLog(@"状態の不一致を修正: '%s' (ID=%u), treenode_pool[%d]=%d, 永続状態=%d", 
                 label, id, poolIndex, ctx->treenode_pool[poolIndex].last_update, expanded ? 1 : 0);
            
            // プールの状態を永続状態に合わせる
            ctx->treenode_pool[poolIndex].last_update = expanded ? 1 : 0;
        }
    }
    
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
    
    // 結果をログ
    NSLog(@"ヘッダー描画結果: '%s' 結果=%d, オプション=%d", label, result, opt);
    
    // プールの状態を確認（実際のプールインデックスはmuライブラリ内で更新される）
    int newPoolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    
    // プールインデックスが更新された場合
    if (newPoolIndex >= 0 && (!found || result != expanded)) {
        // 永続的な状態を更新
        ios_set_header_state(id, label, result != 0);
        NSLog(@"ヘッダー状態を更新: '%s' (ID=%u) -> %@", 
              label, id, result ? @"展開" : @"折りたたみ");
    }
    
    // 最後に描画されたヘッダーの情報をキャッシュ
    ios_cache_header(ctx, label, ctx->last_rect, result);
    
    // 元の結果を返す
    return result;
}

// プルダウンのラッパーヘルパー（読みやすさのため）
 int ios_header(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 0, opt);
}

// ツリーノードのラッパーヘルパー（読みやすさのため）
static int ios_begin_treenode(mu_Context* ctx, const char* label, int opt) {
    return ios_header_wrapper(ctx, label, 1, opt);
}

#pragma mark - 永続的なヘッダー状態管理 (Metalバージョンと同様)

// プルダウン（ヘッダー）の状態を保持するための構造体
typedef struct {
    mu_Id id;
    char label[64];
    int expanded;  // BOOLではなくintを使用（0=折りたたみ、1=展開）
} PersistentHeaderState;

// 永続的なヘッダー状態を保持する配列
static PersistentHeaderState persistentHeaders[50];
static int persistentHeaderCount = 0;

// 永続的なヘッダー状態を初期化
static void ios_init_header_states(void) {
    persistentHeaderCount = 0;
    memset(persistentHeaders, 0, sizeof(persistentHeaders));
    
    for (int i = 0; i < 50; i++) {
        persistentHeaders[i].id = 0;
        persistentHeaders[i].expanded = 0;  // 明示的に0を設定
        persistentHeaders[i].label[0] = '\0';
    }
    
    NSLog(@"永続的ヘッダー状態管理を初期化しました");
}

// 永続的なヘッダー状態を設定する
static void ios_set_header_state(mu_Id id, const char* label, BOOL expanded) {
    if (id == 0) return;
    
    // expanded値を明示的に0か1に強制
    int expandedInt = expanded ? 1 : 0;
    
    // デバッグ用：入力値の確認
    NSLog(@"ios_set_header_state: ID=%u, ラベル='%s', 展開状態=%d (BOOL=%@)", 
          id, label ? label : "不明", expandedInt, expanded ? @"YES" : @"NO");
    
    // 既存のエントリを検索
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            persistentHeaders[i].expanded = expandedInt;
            NSLog(@"ヘッダー状態を更新: '%s' (ID=%u) -> %d", 
                 label ? label : "不明", id, expandedInt);
            return;
        }
    }
    
    // 新しいエントリを追加
    if (persistentHeaderCount < 50) {
        persistentHeaders[persistentHeaderCount].id = id;
        if (label) {
            strncpy(persistentHeaders[persistentHeaderCount].label, label, 63);
            persistentHeaders[persistentHeaderCount].label[63] = '\0';
        } else {
            persistentHeaders[persistentHeaderCount].label[0] = '\0';
        }
        persistentHeaders[persistentHeaderCount].expanded = expandedInt;
        NSLog(@"ヘッダー状態を新規追加: '%s' (ID=%u) -> %d", 
             label ? label : "不明", id, expandedInt);
        persistentHeaderCount++;
    }
}

// 永続的なヘッダー状態を取得する
static BOOL ios_get_header_state(mu_Id id, BOOL* outExpanded) {
    if (id == 0 || !outExpanded) return NO;
    
    for (int i = 0; i < persistentHeaderCount; i++) {
        if (persistentHeaders[i].id == id) {
            // 整数値を明示的にBOOLにキャスト
            int expandedInt = persistentHeaders[i].expanded;
            *outExpanded = expandedInt != 0 ? YES : NO;
            
            NSLog(@"ヘッダー状態を取得: ID=%u, 整数値=%d, BOOL値=%@", 
                 id, expandedInt, *outExpanded ? @"YES" : @"NO");
            return YES;
        }
    }
    
    // 見つからない場合はデフォルトでNO
    // *outExpanded = NO;
    // NSLog(@"ヘッダー状態が見つかりません: ID=%u, デフォルト値=NO", id);
    return NO;
}

// 永続的なヘッダー状態を切り替える
static void ios_toggle_persistent_header_state(mu_Id id, const char* label) {
    if (id == 0) return;
    
    BOOL currentState = NO;
    BOOL found = ios_get_header_state(id, &currentState);
    
    ios_set_header_state(id, label, !currentState);
    
    NSLog(@"ヘッダー状態を切り替え: '%s' -> %s (既存=%s)",
         label ? label : "不明", !currentState ? "展開" : "折りたたみ", found ? "有" : "無");
}
@end
#pragma mark - エントリポイント
@interface micro_ui_view_controller : UIViewController
@end
@implementation micro_ui_view_controller
// UIKitのライフサイクルメソッドはキャメルケースのままにする必要があります
- (void)loadView { self.view = [[micro_ui_view alloc] init_with_frame:[[UIScreen mainScreen] bounds]]; }
// UIKitのステータスバー関連メソッドもキャメルケースのままにする
- (BOOL)prefersStatusBarHidden { return YES; }
@end
@interface app_delegate : UIResponder <UIApplicationDelegate>
@property (strong, nonatomic) UIWindow *window;
@end

@implementation app_delegate

// UIKit委譲メソッドはキャメルケースのままにする必要があります
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
    self.window = [[UIWindow alloc] initWithFrame:[[UIScreen mainScreen] bounds]];
    self.window.rootViewController = [[micro_ui_view_controller alloc] init];
    [self.window makeKeyAndVisible];
    return YES;
}

@end

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([app_delegate class]));
    }
}
