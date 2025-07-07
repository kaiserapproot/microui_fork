# MicroUI 実装アーキテクチャ比較ドキュメント

このドキュメントでは、MicroUIのmacOSおよびiOSプラットフォーム向け実装（OpenGLとMetal）のアーキテクチャと技術詳細を比較・分析します。

## 目次

1. [基本アーキテクチャ](#1-基本アーキテクチャ)
2. [レンダリングパイプライン](#2-レンダリングパイプライン)
3. [イベント処理](#3-イベント処理)
4. [テクスチャ処理とフォントレンダリング](#4-テクスチャ処理とフォントレンダリング)
5. [バッチ処理の実装](#5-バッチ処理の実装)
6. [ディスプレイ同期](#6-ディスプレイ同期)
7. [メモリ管理と最適化](#7-メモリ管理と最適化)
8. [命名規則とコードスタイル](#8-命名規則とコードスタイル)
9. [プラットフォーム固有の実装詳細](#9-プラットフォーム固有の実装詳細)
10. [結論と開発者向けガイダンス](#10-結論と開発者向けガイダンス)

## 1. 基本アーキテクチャ

### 共通アーキテクチャ要素

すべての実装で共通の基本アーキテクチャは以下の通りです：

```
[MicroUI Core]  ←→  [プラットフォーム実装]  ←→  [グラフィックスAPI]  →  [画面表示]
 (microui.c)       (iOS/macOS固有コード)     (OpenGL/Metal)
```

### iOS実装アーキテクチャ

#### iOS OpenGL ES 1.1 実装

```
microui.c → ios_opengl1.1_microui.m（micro_ui_view_controller） → OpenGL ES 1.1 API → 画面表示
```

主要クラス：
- `micro_ui_view_controller` (旧 `ViewController`) - UIViewControllerのサブクラスで、MicroUIの管理とOpenGL ESコンテキストの設定を担当
- `micro_ui_view` (旧 `OpenGLView`) - タッチイベントのハンドリングとEAGLContextの管理を担当

#### iOS Metal 実装

```
microui.c → ios_wip_metal_microui.m（micro_ui_view_controller, metal_renderer） → Metal API → 画面表示
```

主要クラス：
- `micro_ui_view_controller` (旧 `ViewController`) - UIViewControllerのサブクラスでUIイベントを処理
- `metal_renderer` (旧 `MetalRenderer`) - MTKViewDelegateを実装し、レンダリングを担当する専用クラス

### macOS実装アーキテクチャ

#### macOS OpenGL 実装

```
microui.c → main.m（MicroUIMacView） → OpenGL API → 画面表示
```

主要クラス：
- `MicroUIMacView` - NSViewのサブクラスで、OpenGLコンテキスト管理、MicroUI処理、イベント処理をすべて担当する単一のクラス
- `AppDelegate` - アプリケーションのウィンドウとビューの初期化を担当

#### macOS Metal 実装

```
microui.c → microui_mac_opengl_main.m（MetalRenderer, ViewController） → Metal API → 画面表示 
```

主要クラス：
- `MetalRenderer` - MTKViewDelegateを実装し、Metalレンダリングパイプラインを管理
- `ViewController` - NSViewControllerのサブクラスで、MTKViewの設定とイベント処理を担当

### アーキテクチャの違い

| 実装 | アーキテクチャスタイル | 責務分離 | コード構造 |
|------|------------------|---------|----------|
| iOS OpenGL | MVC | 中程度 | 単一コントローラーとビュー |
| iOS Metal | MVP | 高い | 個別のレンダラークラス |
| macOS OpenGL | モノリシック | 低い | 単一の大きなビュークラス |
| macOS Metal | MVP | 高い | 個別のレンダラークラス |

## 2. レンダリングパイプライン

### OpenGL実装のパイプライン (iOS/macOS)

```
[MicroUI core] → [コマンドリスト生成] → [各コマンド個別処理] → [OpenGL API呼び出し]
```

例（矩形描画）:
```objectivec
// iOS OpenGL ES 1.1 (snake_case)
- (void)draw_rect:(mu_Rect)rect color:(mu_Color)color {
    glDisable(GL_TEXTURE_2D);
    glColor4f(color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0);
    GLfloat vtx[] = { rect.x, rect.y, rect.x+rect.w, rect.y, rect.x+rect.w, rect.y+rect.h, rect.x, rect.y+rect.h };
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vtx);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// macOS OpenGL (CamelCase)
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color {
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
}
```

特徴：
- 固定機能パイプラインの使用（レガシーなOpenGL機能）
- 即時モードレンダリング（コマンドの即時実行）
- 頻繁な状態変更（テクスチャ有効/無効の切り替えなど）
- シンプルだが、パフォーマンス最適化の余地が限られる

### Metal実装のパイプライン (iOS/macOS)

```
[MicroUI core] → [コマンドリスト生成] → [頂点バッファに蓄積] → [Metal APIにまとめて送信]
```

例（レンダリングパイプライン）:
```objectivec
// iOS Metal (snake_case)
- (void)render_frame {
    id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
    MTLRenderPassDescriptor *render_pass_descriptor = metal_view.currentRenderPassDescriptor;
    
    id<MTLRenderCommandEncoder> render_encoder = [command_buffer renderCommandEncoderWithDescriptor:render_pass_descriptor];
    [render_encoder setRenderPipelineState:pipeline_state];
    [render_encoder setVertexBuffer:vertex_buffer offset:0 atIndex:0];
    [render_encoder setFragmentTexture:atlas_texture atIndex:0];
    [render_encoder setFragmentSamplerState:sampler_state atIndex:0];
    
    [render_encoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                               indexCount:index_count
                                indexType:MTLIndexTypeUInt16
                              indexBuffer:index_buffer
                        indexBufferOffset:0];
    
    [render_encoder endEncoding];
    [command_buffer presentDrawable:metal_view.currentDrawable];
    [command_buffer commit];
}

// macOS Metal (CamelCase) - 2025年6月24日更新
- (void)drawInMTKView:(MTKView *)view {
    [self renderFrame];
    
    id<MTLCommandBuffer> commandBuffer = [self.commandQueue commandBuffer];
    MTLRenderPassDescriptor *renderPassDescriptor = view.currentRenderPassDescriptor;
    
    if (renderPassDescriptor != nil) {
        id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
        [renderEncoder setRenderPipelineState:self.pipelineState];
        [renderEncoder setVertexBuffer:self.vertexBuffer offset:0 atIndex:0];
        [renderEncoder setVertexBuffer:self.uniformBuffer offset:0 atIndex:1];
        [renderEncoder setFragmentTexture:self.atlasTexture atIndex:0];
        [renderEncoder setFragmentSamplerState:self.samplerState atIndex:0];
        
        if (index_count > 0) {
            [renderEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
                                     indexCount:index_count
                                      indexType:MTLIndexTypeUInt16
                                    indexBuffer:self.indexBuffer
                              indexBufferOffset:0];
        }
        
        [renderEncoder endEncoding];
        [commandBuffer presentDrawable:view.currentDrawable];
    }
    
    [commandBuffer commit];
}
```

特徴：
- プログラマブルシェーダーの使用
- バッファへのコマンド蓄積とバッチ処理
- 描画命令をGPUに効率的に送信
- シェーダーリソース（テクスチャ、サンプラー）の明示的な管理
- より複雑だがパフォーマンスが高い

### レンダリングパイプラインの重要な違い

| 実装 | パイプライン | バッチ処理 | シェーダー | 状態管理 |
|------|------------|----------|----------|---------|
| iOS OpenGL | 固定機能 | オプション | 不要 | 頻繁な変更 |
| iOS Metal | プログラマブル | 標準実装 | 必須 | 効率的 |
| macOS OpenGL | 固定機能 | 条件コンパイル | 不要 | 頻繁な変更 |
| macOS Metal | プログラマブル | 標準実装 | 必須 | 効率的 |

## 3. イベント処理

### iOS イベント処理

#### iOS OpenGL/Metal 共通

```objectivec
// iOS (snake_case)
- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint loc = [touch locationInView:self.view];
    
    mu_input_mousedown(mu_ctx, loc.x, loc.y, MU_MOUSE_LEFT);
}

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint loc = [touch locationInView:self.view];
    
    mu_input_mousemove(mu_ctx, loc.x, loc.y);
}
```

### macOS イベント処理

#### macOS OpenGL

```objectivec
// macOS OpenGL (CamelCase)
- (void)mouseDown:(NSEvent *)event {
    NSPoint point = [self normalizeMousePoint:event];
    
    mu_input_mousemove(mu_ctx, (int)point.x, (int)point.y);
    mu_input_mousedown(mu_ctx, (int)point.x, (int)point.y, MU_MOUSE_LEFT);
    
    // 即座に次のフレームを要求
    [self setNeedsDisplay:YES];
}

- (void)keyDown:(NSEvent *)event {
    NSString *characters = [event charactersIgnoringModifiers];
    unsigned short keyCode = [event keyCode];
    
    // 特殊キーのチェック
    if (keyCode == 36) { // Return
        mu_input_keydown(mu_ctx, MU_KEY_RETURN);
    }
    
    // テキスト入力
    mu_input_text(mu_ctx, [characters UTF8String]);
}
```

#### macOS Metal

```objectivec
// macOS Metal (CamelCase)
- (void)mouseDown:(NSEvent *)event {
    NSPoint windowPoint = [event locationInWindow];
    NSPoint viewPoint = [self.view convertPoint:windowPoint fromView:nil];
    
    mu_input_mousedown(self.renderer.muiContext, viewPoint.x, viewPoint.y, MU_MOUSE_LEFT);
}
```

### イベント処理の主な違い

| 実装 | イベントタイプ | 座標変換 | 入力検知 |
|------|-------------|---------|---------|
| iOS | タッチ（マルチタッチ対応） | UIViewベース | UIResponderチェーン |
| macOS | マウス＋キーボード | NSViewベース | イベントモニタリング |

主な相違点：
- iOSはタッチイベント(`touchesBegan:`, `touchesMoved:`, etc)を使用
- macOSはマウスイベント(`mouseDown:`, `mouseMoved:`, etc)とキーボードイベントを処理
- 座標変換ロジックが異なる（UIKitとAppKit）
- iOSは複数タッチに対応する必要があるが、現在の実装は単一タッチのみ

## 4. テクスチャ処理とフォントレンダリング

### OpenGL テクスチャ処理

```objectivec
// iOS OpenGL ES 1.1 (snake_case)
- (void)setup_font_texture {
    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    
    // アトラステクスチャをロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 
                 0, GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
                 
    // テクスチャフィルタリングの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// macOS OpenGL (CamelCase)
- (void)setupSystemFont {
    glGenTextures(1, &font_texture);
    glBindTexture(GL_TEXTURE_2D, font_texture);
    
    // アトラスデータをロード
    glTexImage2D(GL_TEXTURE_2D, 0, GL_ALPHA, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_ALPHA, GL_UNSIGNED_BYTE, atlas_texture);
                
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}
```

### Metal テクスチャ処理

```objectivec
// iOS Metal (snake_case)
- (void)setup_atlas_texture {
    MTLTextureDescriptor *texture_descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA8Unorm
                                                                                                 width:ATLAS_WIDTH
                                                                                                height:ATLAS_HEIGHT
                                                                                             mipmapped:NO];
    texture_descriptor.usage = MTLTextureUsageShaderRead;
    
    atlas_texture = [device newTextureWithDescriptor:texture_descriptor];
    
    // A8フォーマットからRGBA8へ変換
    uint32_t *rgba_data = malloc(ATLAS_WIDTH * ATLAS_HEIGHT * 4);
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        uint8_t alpha = atlas_texture[i];
        rgba_data[i] = (alpha << 24) | 0x00FFFFFF; // RGB=白、Aはアトラスデータ
    }
    
    [atlas_texture replaceRegion:MTLRegionMake2D(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT)
                     mipmapLevel:0
                       withBytes:rgba_data
                     bytesPerRow:ATLAS_WIDTH * 4];
    
    free(rgba_data);
}

// macOS Metal (CamelCase)
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
        rgbaData[i] = (alpha << 24) | 0x00FFFFFF;
    }
    
    [self.atlasTexture replaceRegion:MTLRegionMake2D(0, 0, ATLAS_WIDTH, ATLAS_HEIGHT)
                         mipmapLevel:0
                           withBytes:rgbaData
                         bytesPerRow:ATLAS_WIDTH * 4];
    
    free(rgbaData);
}
```

### テクスチャ処理の主な違い

| 実装 | テクスチャフォーマット | フィルタリング | 変換処理 |
|------|-------------------|-----------|---------|
| OpenGL | GL_ALPHA（1チャネル） | GL_NEAREST | 不要 |
| Metal | RGBA8Unorm（4チャネル） | MTLSamplerMinMagFilterNearest | A8→RGBA8への変換が必要 |

主な相違点：
- OpenGLはアルファチャンネルのみのテクスチャ（GL_ALPHA）を直接使用可能
- MetalはRGBAフォーマットが基本のため、1チャンネルから4チャンネルへの変換が必要
- どちらもピクセル精度を保つためにNEAREST（最近傍）フィルタリングを使用

### フォントレンダリングロジック

すべての実装で共通のテキスト幅計算関数：

```c
// すべてのプラットフォームで同様のロジック
static int text_width(mu_Font font, const char* text, int len) {
    if (len == -1) { 
        len = strlen(text); 
    }
    
    int res = 0;
    for (int i = 0; i < len; i++) {
        unsigned char chr = (unsigned char) text[i];
        
        // UTF-8の連続バイトをスキップ
        if ((chr & 0xc0) == 0x80) continue;
        
        // アスキー範囲に制限
        chr = mu_min(chr, 127);
        
        // アトラスからグリフ幅を取得
        res += atlas[ATLAS_FONT + chr].w;
    }
    
    return res;
}
```

## 5. バッチ処理の実装

### macOS OpenGL実装におけるUSE_QUADバッチ処理の詳細

macOS OpenGL実装では、`USE_QUAD`フラグによって2つの異なる描画モードを切り替えることができます：

#### バッチ処理モード（USE_QUAD = 1）

**基本構造:**
```objectivec
#define USE_QUAD 1
#define BUFFER_SIZE 16384
#define MAX_QUADS (BUFFER_SIZE / 4)

// 頂点構造体
typedef struct {
    float x, y;       // 位置座標
    float u, v;       // テクスチャ座標
    float r, g, b, a; // 色成分
} Vertex;

// バッチ状態管理構造体
typedef struct {
    Vertex vertices[BUFFER_SIZE];           // 頂点バッファ（~786KB）
    GLuint indices[BUFFER_SIZE * 6 / 4];    // インデックスバッファ（~393KB）
    int vertex_count;                       // 現在の頂点数
    int index_count;                        // 現在のインデックス数
    GLuint current_texture;                 // 現在のテクスチャ
    BOOL is_textured;                       // テクスチャ使用フラグ
} BatchState;
```

**描画処理の流れ:**
```
描画コマンド → pushQuad() → バッファに蓄積 → flushBatch() → 一括描画
```

1. **蓄積フェーズ**: `pushQuad`メソッドで描画データをメモリバッファに蓄積
2. **フラッシュ条件**: 
   - バッファが満杯（16,384頂点）
   - テクスチャ状態が変更される時
3. **一括描画**: `flushBatch`でVBO/IBOを使って効率的に描画

**バッチ処理版のテキスト描画:**
```objectivec
#if USE_QUAD
// 各文字をバッファに蓄積（描画は後で一括実行）
for (p = text; *p; p++, char_count++) {
    chr = (unsigned char)*p;
    if (chr < 32 || chr >= 127) continue;
    
    int idx = ATLAS_FONT + chr;
    src = atlas[idx];
    dst.w = src.w;
    dst.h = src.h;
    
    if (chr == ' ') {
        dst.x += dst.w + 1;
        continue;
    }
    
    // バッチにクアッドを追加（蓄積のみ）
    [self pushQuad:dst src:src color:color textured:YES];
    dst.x += dst.w + 1;
}
#endif
```

**バッチ処理版の矩形描画:**
```objectivec
#if USE_QUAD
// 矩形をバッファに蓄積
mu_Rect src = {0, 0, 1, 1}; // ダミーのテクスチャ座標
[self pushQuad:rect src:src color:color textured:NO];
#endif
```

#### 即座描画モード（USE_QUAD = 0）

**基本構造:**
- 特別なバッファ構造体なし
- 各描画コマンドを個別に即座実行

**描画処理の流れ:**
```
描画コマンド → 即座にOpenGL API呼び出し → 個別描画
```

1. **即座実行**: 各描画コマンドが個別にOpenGL APIを呼び出し
2. **状態変更**: 描画のたびにテクスチャやOpenGL状態を切り替え

**即座描画版のテキスト描画:**
```objectivec
#else
// 各文字を即座に描画
for (p = text; *p; p++, char_count++) {
    // フォントテクスチャをバインド
    glBindTexture(GL_TEXTURE_2D, font_texture);
    glEnable(GL_TEXTURE_2D);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // テクスチャ座標を計算して即座に描画
    glBegin(GL_QUADS);
    glColor4f(r, g, b, a);
    glTexCoord2f(tx, ty);         glVertex2f(dst.x, dst.y);
    glTexCoord2f(tx + tw, ty);    glVertex2f(dst.x + dst.w, dst.y);
    glTexCoord2f(tx + tw, ty + th); glVertex2f(dst.x + dst.w, dst.y + dst.h);
    glTexCoord2f(tx, ty + th);    glVertex2f(dst.x, dst.y + dst.h);
    glEnd();
    
    glDisable(GL_TEXTURE_2D);
}
#endif
```

**即座描画版の矩形描画:**
```objectivec
#else
// 矩形を即座に描画
glDisable(GL_TEXTURE_2D);
glBegin(GL_QUADS);
glColor4f(r, g, b, a);
glVertex2f(x, y);
glVertex2f(x + w, y);
glVertex2f(x + w, y + h);
glVertex2f(x, y + h);
glEnd();
#endif
```

#### パフォーマンス比較

| 比較項目 | バッチ処理モード (USE_QUAD=1) | 即座描画モード (USE_QUAD=0) |
|---------|------------------------------|---------------------------|
| **パフォーマンス** | 高い（一括GPU送信） | 低い（個別GPU呼び出し） |
| **メモリ使用量** | 大（~1.2MB追加バッファ） | 小（追加バッファなし） |
| **実装複雑さ** | 高い（バッファ管理必要） | 低い（ストレートフォワード） |
| **デバッグ容易さ** | 難しい（バッファ状態追跡） | 易しい（即座実行） |
| **OpenGL呼び出し数** | 少ない（バッチ単位） | 多い（要素単位） |
| **状態変更頻度** | 低い（テクスチャ変更時のみ） | 高い（描画要素毎） |

#### 使用場面の推奨

**バッチ処理モード（USE_QUAD = 1）が適している場面:**
- 大量のUI要素を描画する場合
- パフォーマンスが重要なアプリケーション
- 複雑なUIレイアウト
- ゲームやリアルタイムアプリケーション
- 60FPS以上の高フレームレート要求

**即座描画モード（USE_QUAD = 0）が適している場面:**
- 学習目的やプロトタイプ開発
- シンプルなUIの場合
- デバッグ時の動作確認
- メモリ使用量を抑制したい場合
- 小規模なアプリケーション

### OpenGL バッチ処理（その他の実装）

#### macOS OpenGL（条件コンパイルによるバッチ処理）

```objectivec
#if USE_QUAD
- (void)pushQuad:(mu_Rect)dst src:(mu_Rect)src color:(mu_Color)color textured:(BOOL)textured {
    // バッファがいっぱいならフラッシュ
    if (batch_state.vertex_count >= BUFFER_SIZE - 4) {
        [self flushBatch];
    }
    
    // テクスチャの使用状態が変わる場合もフラッシュ
    if (batch_state.vertex_count > 0 && batch_state.is_textured != textured) {
        [self flushBatch];
    }
    
    batch_state.is_textured = textured;
    
    // 色の正規化
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // テクスチャ座標を計算
    float u1 = 0.0f, v1 = 0.0f, u2 = 1.0f, v2 = 1.0f;
    if (textured) {
        u1 = (float)src.x / ATLAS_WIDTH;
        v1 = (float)src.y / ATLAS_HEIGHT;
        u2 = u1 + (float)src.w / ATLAS_WIDTH;
        v2 = v1 + (float)src.h / ATLAS_HEIGHT;
    }
    
    // 4つの頂点を追加（左上、右上、右下、左下）
    int base_index = batch_state.vertex_count;
    batch_state.vertices[base_index + 0] = (Vertex){ dst.x, dst.y, u1, v1, r, g, b, a };
    batch_state.vertices[base_index + 1] = (Vertex){ dst.x + dst.w, dst.y, u2, v1, r, g, b, a };
    batch_state.vertices[base_index + 2] = (Vertex){ dst.x + dst.w, dst.y + dst.h, u2, v2, r, g, b, a };
    batch_state.vertices[base_index + 3] = (Vertex){ dst.x, dst.y + dst.h, u1, v2, r, g, b, a };
    
    batch_state.vertex_count += 4;
    batch_state.index_count += 6;
}

- (void)flushBatch {
    if (batch_state.vertex_count == 0) {
        return;
    }
    
    // テクスチャの設定
    if (batch_state.is_textured) {
        glBindTexture(GL_TEXTURE_2D, font_texture);
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
    
    // 頂点バッファにデータをアップロード
    glBindBuffer(GL_ARRAY_BUFFER, vertex_buffer);
    glBufferData(GL_ARRAY_BUFFER, batch_state.vertex_count * sizeof(Vertex), 
                 batch_state.vertices, GL_DYNAMIC_DRAW);
    
    // 描画
    glDrawElements(GL_TRIANGLES, batch_state.index_count, GL_UNSIGNED_INT, 0);
    
    // 状態をリセット
    batch_state.vertex_count = 0;
    batch_state.index_count = 0;
}
#endif
```

## 6. ディスプレイ同期

### iOS ディスプレイ同期

```objectivec
// iOS (snake_case)
- (void)setup_display_link {
    display_link = [CADisplayLink displayLinkWithTarget:self selector:@selector(render_frame)];
    [display_link addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
}

- (void)render_frame {
    // ここでMicroUIの更新と描画を行う
    [self process_microui_frame];
    [self.view setNeedsDisplay];
}
```

### macOS ディスプレイ同期

```objectivec
// macOS OpenGL (CamelCase)
static CVReturn DisplayLinkCallback(CVDisplayLinkRef displayLink, ... ) {
    MicroUIMacView* view = (__bridge MicroUIMacView*)displayLinkContext;
    
    // メインスレッドで描画を実行
    dispatch_async(dispatch_get_main_queue(), ^{
        [view drawView];
    });
    
    return kCVReturnSuccess;
}

- (void)setupDisplayLink {
    CVReturn result = CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
    CVDisplayLinkSetOutputCallback(displayLink, &DisplayLinkCallback, (__bridge void*)self);
    CVDisplayLinkStart(displayLink);
}

// macOS Metal (CamelCase) - MTKViewが自動的にハンドリング
- (void)viewDidLoad {
    [super viewDidLoad];
    
    self.metalView = [[MTKView alloc] initWithFrame:self.view.bounds];
    self.metalView.device = MTLCreateSystemDefaultDevice();
    self.metalView.delegate = self.renderer;
    
    [self.view addSubview:self.metalView];
}
```

### ディスプレイ同期の主な違い

| 実装 | 同期メカニズム | スレッディング | 更新頻度制御 |
|------|-------------|--------------|------------|
| iOS | CADisplayLink | メインスレッド | preferredFramesPerSecond |
| macOS OpenGL | CVDisplayLink | コールバックからメインスレッドへディスパッチ | CVDisplayLinkSetCurrentCGDisplayFromOpenGLContext |
| macOS Metal | MTKViewデリゲート | メインスレッド | MTKView.preferredFramesPerSecond |

主な相違点：
- iOSはCADisplayLinkを使用（UIKit統合）
- macOS OpenGLはCVDisplayLinkを使用（低レベルCoreVideo）
- macOS MetalはMTKViewのデリゲートパターンで自動化
- スレッド処理がわずかに異なる（macOS OpenGLは明示的なディスパッチが必要）

## 7. メモリ管理と最適化

### 共通のメモリ最適化戦略

すべての実装で共通のメモリ管理戦略：

1. **静的バッファの使用**
   - 固定サイズの頂点バッファとインデックスバッファを使用
   - 動的メモリ割り当てを最小限に抑制

2. **アトラステクスチャの効率的な使用**
   - すべての文字とアイコンを単一テクスチャに格納
   - テクスチャサイズを最適化（ATLAS_WIDTH x ATLAS_HEIGHT）

3. **MicroUIの軽量設計の活用**
   - 最小限のメモリフットプリント
   - コマンドバッファの効率的な利用

### iOS vs macOS のメモリ管理の違い

| 実装 | メモリ割り当て | リソース解放 | オートリリースプール |
|------|-------------|------------|------------------|
| iOS | 制限されたメモリ | 明示的な解放が重要 | @autoreleasepool使用 |
| macOS | 比較的余裕 | 同様に解放が重要 | 標準NSAutoreleasePool |

メモリ管理とリソース解放：

```objectivec
// iOS Metal (snake_case)
- (void)dealloc {
    // リソース解放
    if (mu_ctx) {
        free(mu_ctx);
        mu_ctx = NULL;
    }
    
    // Metal関連リソースは自動的に解放される
}

// macOS OpenGL (CamelCase)
- (void)dealloc {
    // DisplayLinkの停止と解放
    if (displayLink) {
        CVDisplayLinkStop(displayLink);
        CVDisplayLinkRelease(displayLink);
    }
    
    // OpenGLリソースの解放
    if (openGLContext) {
        [openGLContext makeCurrentContext];
        
        if (font_texture) {
            glDeleteTextures(1, &font_texture);
        }
        
        // バッチバッファのクリーンアップ
        [self cleanupBatchBuffers];
        
        if (mu_ctx) {
            free(mu_ctx);
        }
        
        [NSOpenGLContext clearCurrentContext];
    }
}
```

主な相違点：
- iOSではメモリ使用量の最適化がより重要
- macOSでは追加のリソース（DisplayLinkなど）の解放が必要
- Metal実装はARC（自動参照カウント）と組み合わせてより安全なメモリ管理
- OpenGL実装ではより明示的なリソース管理が必要

## 8. 命名規則とコードスタイル

### iOS (リファクタリング後)

```objectivec
// iOS - snake_case命名規則
@interface micro_ui_view_controller : UIViewController
{
    // snake_caseのインスタンス変数
    mu_Context* mu_ctx;
    EAGLContext* eagl_context;
    GLuint frame_buffer;
    GLuint color_buffer;
    GLuint font_texture;
}

// メソッドもsnake_case
- (void)init_micro_ui;
- (void)process_microui_frame;
- (void)draw_rect:(mu_Rect)rect color:(mu_Color)color;
@end
```

### macOS

```objectivec
// macOS - CamelCase命名規則
@interface MicroUIMacView : NSView
{
    // CamelCaseのインスタンス変数
    NSOpenGLContext* openGLContext;
    mu_Context* mu_ctx;
    GLuint fontTexture;
}

// メソッドもCamelCase
- (void)initMicroUI;
- (void)processMicroUIFrame;
- (void)drawRect:(mu_Rect)rect color:(mu_Color)color;
@end
```

### 命名規則とコードスタイルの比較

| 実装 | 命名規則 | クラス名規則 | メソッド命名 | API規則への順守 | 最新更新 |
|------|---------|------------|------------|---------------|----------|
| iOS (リファクタリング後) | snake_case | snake_case | snake_case | UIKit APIはCamelCaseを維持 | - |
| iOS (リファクタリング前) | CamelCase | CamelCase | CamelCase | UIKit APIと統一 | - |
| macOS OpenGL | CamelCase | CamelCase | CamelCase | AppKit APIと統一 | 2025年6月24日 |
| macOS Metal | CamelCase | CamelCase | CamelCase | AppKit APIと統一 | 2025年6月24日 |

特記事項（2025年6月24日更新）：
- iOS実装は近年snake_caseへリファクタリングされた
- **macOS実装（OpenGL/Metal両方）は標準的なObjective-CのCamelCase規則を維持**
- どちらもプラットフォームのネイティブAPI（UIKit/AppKit）呼び出しでは元の命名規則を尊重
- snake_caseへの移行はiOS版のコード一貫性のために行われたが、macOS版は従来のApple開発慣習に準拠
- **macOS Metal実装では重要なUI問題修正（テキストボックスフォーカス問題）が完了**

### macOS Metal実装の重要な改善（2025年6月24日）

macOS Metal実装では以下の重要な問題修正と改善が行われました：

#### テキストボックスフォーカス問題の修正
```objectivec
// 修正前: フォーカスが即座に失われる問題
- (void)mouseDown:(NSEvent *)event {
    mu_input_mousedown(self.renderer.muiContext, viewPoint.x, viewPoint.y, MU_MOUSE_LEFT);
}

// 修正後: OpenGL版と同じイベント処理順序に統一
- (void)mouseDown:(NSEvent *)event {
    NSPoint windowPoint = [event locationInWindow];
    NSPoint viewPoint = [self.view convertPoint:windowPoint fromView:nil];
    
    // OpenGL版と同じ順序: mousemove → mousedown
    mu_input_mousemove(self.renderer.muiContext, viewPoint.x, viewPoint.y);
    mu_input_mousedown(self.renderer.muiContext, viewPoint.x, viewPoint.y, MU_MOUSE_LEFT);
    
    NSLog(@"[Metal] mouseDown at (%.1f, %.1f), focus before: %d, after: %d", 
          viewPoint.x, viewPoint.y, focus_before, self.renderer.muiContext->focus);
}
```

#### 主な修正内容
1. **マウスイベント処理順序の統一**: OpenGL版と同じ`mu_input_mousemove()` → `mu_input_mousedown()`の順序
2. **フォーカス状態の保護**: 不適切なフォーカスリセットを防止
3. **詳細なデバッグログ**: 問題特定と解決を支援
4. **マウストラッキングエリアの改善**: 重複によるmouseExited頻発の防止

#### 結果
- Metal版でもOpenGL版と同様のテキスト入力が正常動作
- 異なるレンダリングバックエンド間でのUI動作統一
- ユーザーエクスペリエンスの大幅改善

## 9. プラットフォーム固有の実装詳細

### iOS固有の実装詳細

1. **向き対応**
   - 画面回転時の座標系調整が必要
   - UIDeviceOrientationNotificationへの対応

2. **Retinaディスプレイ対応**
   - コンテンツスケーリングファクターの管理
   - 高解像度レンダリングのための調整

3. **メモリ警告処理**
   - `didReceiveMemoryWarning`での適切なリソース解放

```objectivec
// iOS固有コード例
- (void)viewWillTransitionToSize:(CGSize)size withTransitionCoordinator:(id<UIViewControllerTransitionCoordinator>)coordinator {
    [super viewWillTransitionToSize:size withTransitionCoordinator:coordinator];
    
    // 画面回転に対応するための処理
    [coordinator animateAlongsideTransition:^(id<UIViewControllerTransitionCoordinatorContext> context) {
        // フレームバッファサイズの更新
        [self update_framebuffer_size];
    } completion:nil];
}

- (void)didReceiveMemoryWarning {
    [super didReceiveMemoryWarning];
    
    // メモリ警告時の処理
    [self cleanup_unused_resources];
}
```

### macOS固有の実装詳細

1. **ウィンドウ管理**
   - NSWindowとの連携
   - フルスクリーンモードのサポート

2. **キーボード入力処理**
   - NSEventのキーコードマッピング
   - モディファイアキーの追跡

3. **高DPIディスプレイ対応**
   - `wantsBestResolutionOpenGLSurface`プロパティの管理
   - バッキングスケールファクターの処理

```objectivec
// macOS固有コード例
- (void)viewDidMoveToWindow {
    [super viewDidMoveToWindow];
    
    if (self.window) {
        // OpenGLコンテキストをウィンドウのビューに設定
        if (openGLContext) {
            [openGLContext setView:self];
        }
        
        // DisplayLinkの開始
        if (displayLink && !CVDisplayLinkIsRunning(displayLink)) {
            CVReturn result = CVDisplayLinkStart(displayLink);
            // ...
        }
    }
}

- (BOOL)acceptsFirstResponder {
    return YES; // キーボードイベントを受け取るために必要
}
```

### プラットフォーム固有の実装比較

| 実装 | ビュー階層 | イベントシステム | 高解像度対応 | ウィンドウ管理 |
|------|----------|---------------|----------|------------|
| iOS | UIView / UIViewController | UIResponderチェーン | UIScreen.scale | 非該当 |
| macOS | NSView / NSViewController | NSEventモニタリング | backingScaleFactor | NSWindow連携 |

## 10. 結論と開発者向けガイダンス

### プラットフォーム・API選択ガイダンス

**iOS実装:**

1. **OpenGL ES 1.1**
   - **長所**: シンプルな実装、固定機能パイプライン、少ないボイラープレート
   - **短所**: 非推奨技術（iOSでは廃止予定）、パフォーマンス制限
   - **推奨**: レガシーデバイスのサポートが必要な場合、または学習目的

2. **Metal**
   - **長所**: 最高のパフォーマンス、最新のAPI、将来的な安全性
   - **短所**: やや複雑な実装、シェーダープログラミングが必要
   - **推奨**: 新規プロジェクト、高パフォーマンスが必要な場合

**macOS実装:**

1. **OpenGL**
   - **長所**: クロスプラットフォーム互換性、実装の簡潔さ、USE_QUADによるバッチ処理最適化
   - **短所**: 非推奨技術（macOSでも廃止予定）
   - **推奨**: クロスプラットフォーム対応が必要な場合、または学習目的
   - **注意**: USE_QUAD=1でバッチ処理を有効化することで大幅なパフォーマンス向上が可能

2. **Metal**
   - **長所**: 最高のパフォーマンス、Apple機器での最適化、将来的な安全性、標準バッチ処理
   - **短所**: Apple専用、シェーダープログラミングの複雑さ
   - **推奨**: macOSのみのターゲット、または高パフォーマンス要件がある場合
   - **最新**: 2025年6月24日にテキストボックスフォーカス問題が修正済み

### 開発者向けベストプラクティス（2025年6月24日更新）

1. **共通コードの共有**
   - プラットフォーム共通のロジックを抽象化
   - グラフィックス実装を分離可能にする

2. **一貫した命名規則**
   - プロジェクト全体で一貫した命名規則を採用
   - プラットフォームAPIとの統合を考慮
   - macOS実装ではCamelCaseを継続使用（Apple開発慣習に準拠）

3. **イベント処理の統一**
   - 異なるレンダリングバックエンド間でのイベント処理順序を統一
   - MicroUIは正確なマウス位置把握後にクリック処理を行う必要がある
   - `mu_input_mousemove()` → `mu_input_mousedown()` の順序を遵守

4. **バッチ処理の実装**
   - すべてのグラフィックスAPI実装でバッチ処理を活用
   - パフォーマンス向上のため描画コマンドをバッファリング
   - macOS OpenGLでは`USE_QUAD=1`で有効化

5. **リソース管理の注意点**
   - 明示的なリソース解放を徹底
   - メモリリーク防止のためのデアロケーション処理

6. **フォーカス管理の重要性**
   - UIフレームワーク移植時はフォーカス状態の適切な管理が重要
   - 不適切なフォーカスリセットによるユーザビリティ問題を防止
   - 十分なデバッグログは問題解決を大幅に加速

7. **プラットフォーム間のUI動作統一**
   - 標準化されたイベントインターフェイスの実装
   - プラットフォーム固有のイベントを抽象化

### 将来の展望（2025年6月24日更新）

1. **Metal APIへの完全移行**
   - OpenGLベースの実装からMetalへの完全移行を検討
   - パフォーマンス向上と将来的な互換性の確保
   - **macOS Metal実装で主要なUI問題が解決済み**

2. **命名規則の最適化**
   - プラットフォーム固有の開発慣習を尊重した命名規則
   - iOS: snake_case（リファクタリング済み）
   - macOS: CamelCase（Apple開発慣習に準拠）

3. **さらなる最適化**
   - バッチ処理のパフォーマンス向上（macOS OpenGLのUSE_QUAD実装を参考）
   - メモリ使用量とGPU効率の改善

4. **拡張機能の追加と問題解決**
   - マルチタッチサポートの強化
   - アニメーション機能の拡張
   - 高度なレイアウトシステムの実装
   - **UI問題の継続的な修正とユーザビリティ向上**

## 11. 技術的な教訓と知見（2025年6月24日）

### UIフレームワーク移植時の重要な知見

1. **イベント処理順序の重要性**
   - 異なるレンダリングバックエンド間でも一貫したイベント処理順序が必要
   - MicroUIのようなImmediate Mode GUIでは、正確なマウス位置の事前把握が重要
   - `mu_input_mousemove()` → `mu_input_mousedown()`の順序違反はフォーカス問題を引き起こす

2. **フォーカス管理のベストプラクティス**
   - UIフォーカス状態は慎重に管理し、不必要なリセットは避ける
   - テキストボックスのフォーカスは他のUI要素クリックまで維持されるべき
   - mouseUpやmouseExitedでの自動フォーカスリセットは問題の原因となりやすい

3. **デバッグの重要性**
   - 十分なデバッグログは問題の特定と解決を大幅に加速する
   - フォーカス状態の変化をトラッキングするログは特に有効
   - 異なる実装間での動作比較により根本原因を特定しやすくなる

4. **プラットフォーム統一性**
   - 異なるレンダリングバックエンドでも同一のUI体験を提供すべき
   - 技術的実装の違いはユーザーには透明であるべき
   - OpenGL版で正常動作する機能はMetal版でも同様に動作すべき

MicroUIの実装は、プラットフォーム（iOS/macOS）とグラフィックスAPI（OpenGL/Metal）の組み合わせによって異なる特性を持ちますが、基本的な設計哲学とアーキテクチャは共通しています。2025年6月24日の更新により、macOS Metal実装の重要な問題が解決され、より安定したクロスプラットフォーム開発が可能になりました。これらの実装を理解することで、効率的なUIレンダリングシステムの構築と問題解決に必要な知識と選択肢を得ることができます。

## 2025-06-24 調査結果: ios_header_ex関数の削除について

### ios_header_ex関数の重複と削除

調査の結果、`ios_header_ex`関数は`ios_header_wrapper`関数と機能が重複していることが判明しました。

#### 重複の内容

1. **同じ処理の実装**
   - 両関数は永続状態とMicroUI内部状態の同期を行う
   - ヘッダー情報のキャッシュ機能も同じ
   - `ios_header_ex`は`ios_header_wrapper`の機能のサブセット

2. **使用状況**
   - `ios_header_ex`: 定義されているが実際には使用されていない
   - `ios_header_wrapper`: 実際に使用されており、より汎用的（ツリーノードとヘッダー両方に対応）

3. **設計上の問題**
   - 機能重複によるコードの保守性低下
   - 混乱を招く可能性のある類似関数

#### 削除の理由

- **機能的重複**: `ios_header_wrapper`が同じ機能をより汎用的に提供
- **未使用コード**: 実際のUIで使用されていない
- **保守性向上**: 重複コードの削除によりコードベースを整理

#### 実際の使用パターン（削除後）

```objectivec
// UIでの使用
ios_header() → ios_header_wrapper() → mu_header_ex()

// 内部構造
- ios_header(): UIで使用するシンプルなAPI
- ios_header_wrapper(): 汎用的なラッパー（ヘッダー/ツリーノード対応）
- mu_header_ex(): MicroUIの標準関数
```

この削除により、コードがより整理され、保守しやすくなりました。

---
