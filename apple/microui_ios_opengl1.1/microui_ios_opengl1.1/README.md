# MicroUI for iOS - OpenGL ES 1.1 vs Metal 実装比較

## プロジェクト概要

このプロジェクトは、軽量UIライブラリである「MicroUI」をiOS上で実行するための実装を提供します。2つの異なるグラフィックスAPIを使用した実装が含まれています：

1. **OpenGL ES 1.1実装** - 古いがシンプルなグラフィックスAPI
2. **Metal実装** - 最新の高効率なApple独自のグラフィックスAPI

このREADMEでは、両実装の違い、アーキテクチャ、および開発中に直面した課題と解決策について説明します。

## アーキテクチャ概要

### 共通のアーキテクチャ要素

```
[MicroUI Core]  ←→  [iOS実装レイヤー]  ←→  [グラフィックスAPI]  →  [画面表示]
 (microui.c)       (iOS固有のコード)      (OpenGL ES 1.1 または Metal)
```

- **MicroUI Core**: UIの論理構造とコマンドリスト生成を担当
- **iOS実装レイヤー**: タッチイベント処理と座標変換
- **グラフィックスAPI**: 実際の描画処理（OpenGL ES 1.1またはMetal）

### MicroUIの基本コンセプト

MicroUIはシンプルなIMMEDIATEモードGUIライブラリで、以下の特徴があります：

- UI要素はフレームごとに再描画される
- `mu_begin` / `mu_end`で囲まれた領域で論理的なUIを構築
- UI構造はコマンドリストに変換される
- 実装レイヤーがコマンドリストを解釈して実際の描画を行う

## OpenGL ES 1.1実装 vs Metal実装 - 詳細比較

### 1. グラフィックスAPIの基本的な違い

#### OpenGL ES 1.1 (`ios_opengl1.1_microui.m`)

```objectivec
// 四角形を描画する例
glDisable(GL_TEXTURE_2D);
glColor4f(color.r/255.0, color.g/255.0, color.b/255.0, color.a/255.0);
GLfloat vtx[] = { rect.x, rect.y, rect.x+rect.w, rect.y, rect.x+rect.w, rect.y+rect.h, rect.x, rect.y+rect.h };
glEnableClientState(GL_VERTEX_ARRAY);
glVertexPointer(2, GL_FLOAT, 0, vtx);
glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
glDisableClientState(GL_VERTEX_ARRAY);
```

- 固定機能パイプラインを使用（シェーダー不要）
- 一つ一つの描画命令を直接実行
- 状態変更が頻繁（`glEnable`/`glDisable`など）
- シンプルだが、バッチ処理などの最適化が限定的

#### Metal (`ios_wip_metal_microui.m`)

```objectivec
// Metalではシェーダーとバッファを使用
id<MTLRenderCommandEncoder> renderEncoder = [commandBuffer renderCommandEncoderWithDescriptor:renderPassDescriptor];
[renderEncoder setRenderPipelineState:pipelineState];
[renderEncoder setVertexBuffer:vertexBuffer offset:0 atIndex:0];
[renderEncoder setFragmentTexture:texture atIndex:0];
[renderEncoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:vertexCount];
[renderEncoder endEncoding];
```

- プログラマブルシェーダーを使用
- コマンドをバッファにまとめてGPUに一括送信
- 複数の描画をバッチ処理して高速化
- 複雑だが、パフォーマンスが高い

### 2. 座標系と変換ロジック

#### 座標系の基本

- **OpenGL標準**: 左下原点(0,0)、Y軸上向き
- **UIKit/Metal**: 左上原点(0,0)、Y軸下向き
- **MicroUI**: 左上原点(0,0)、Y軸下向き（UIKitと同じ）

#### OpenGL ES 1.1での対応

```objectivec
// OpenGLの座標系をUIKit/MicroUI座標系に合わせる（Y軸反転）
glMatrixMode(GL_PROJECTION); 
glLoadIdentity();
glOrthof(0, viewWidth, viewHeight, 0, -1, 1);
```

- シザーテスト（描画領域の制限）では特別な処理が必要：

```objectivec
// OpenGLのシザーテストは左下原点座標系のため変換が必要
glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h);
```

#### Metal実装での対応

- MTKViewとシェーダーパイプラインで座標変換が自動処理
- 特別な座標変換コードは不要

### 3. タッチイベント処理とマウスエミュレーション

#### 共通の課題

MicroUIはマウス中心のイベントモデルを採用していますが、iOSはタッチベースのイベントシステムを使用します。この橋渡しが必要です。

#### OpenGL ES 1.1実装

```objectivec
// タッチ座標をOpenGL座標に変換
- (CGPoint)convertPointToOpenGL:(CGPoint)pt {
    float scale = self.contentScaleFactor;
    return CGPointMake(pt.x * scale, pt.y * scale);
}

// タッチイベント処理
- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint uiPt = [touch locationInView:self];
    CGPoint oglPt = [self convertPointToOpenGL:uiPt];
    
    // MicroUIに座標を渡す
    mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y);
    mu_ctx->mouse_down = MU_MOUSE_LEFT;
    mu_ctx->mouse_pressed = MU_MOUSE_LEFT;
}
```

- タッチ座標を明示的に変換
- contentScaleFactorを考慮したスケーリング

#### Metal実装

```objectivec
- (CGPoint)getTouchLocation:(UITouch *)touch {
    CGPoint location = [touch locationInView:self.metalView];
    return location;  // そのまま使用可能
}
```

- MTKViewが自動的に座標スケーリングを処理
- タッチ座標はほぼそのまま使用可能

### 4. ヘッダー（プルダウン）状態管理

#### 問題の本質

1. MicroUIはツリーノードプールで状態を管理
2. 状態が永続的でない（毎フレーム再構築）
3. タップ時と描画時でヘッダーIDを一致させる必要がある

#### 解決策（両実装共通）

1. **ヘッダーキャッシュ**: 描画時にヘッダーの位置とIDを記録

```objectivec
typedef struct {
    mu_Id id;
    mu_Rect rect;
    char label[64];
    BOOL active;
} HeaderCache;
```

2. **永続状態管理**: アプリケーション独自に状態を永続化

```objectivec
typedef struct {
    mu_Id id;
    char label[64];
    int expanded;  // 開閉状態（0=閉、1=開）
} PersistentHeaderState;
```

3. **状態同期の仕組み**:
   - 描画時: 永続状態からMicroUIへ状態を反映
   - タップ時: 永続状態とMicroUI状態の両方を更新

```objectivec
// ヘッダー状態をトグルする関数
static void ios_toggle_header_state(mu_Context* ctx, mu_Id id, const char* label) {
    // 1. 永続状態を取得
    BOOL currentExpanded = NO;
    ios_get_header_state(id, &currentExpanded);
    
    // 2. 状態を反転
    BOOL newState = !currentExpanded;
    
    // 3. 永続状態を更新
    ios_set_header_state(id, label, newState);
    
    // 4. MicroUI内部状態も同期して更新
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    if (idx >= 0) {
        ctx->treenode_pool[idx].last_update = newState ? 1 : 0;
    }
}
```

### 5. ウィンドウのドラッグと操作

#### 共通のアプローチ

1. タイトルバーヒットテスト
2. ドラッグ状態の追跡
3. ウィンドウ位置の更新

#### OpenGL ES 1.1実装の特徴

```objectivec
// ドラッグ処理（touchesMovedメソッド内）
if (isDraggingWindow) {
    // 移動量計算
    CGFloat dx = uiPt.x - dragStartTouchPos.x;
    CGFloat dy = uiPt.y - dragStartTouchPos.y;
    float scale = self.contentScaleFactor;
    
    // 新しい位置を計算（スケール考慮）
    int newX = dragStartWindowX + (dx * scale);
    int newY = dragStartWindowY + (dy * scale);
    
    // コンテナの位置を更新
    mu_Container *win = mu_get_container(mu_ctx, [draggingWindowName UTF8String]);
    if (win) {
        win->rect.x = newX;
        win->rect.y = newY;
    }
}
```

- contentScaleFactorを考慮したスケーリング
- タッチ座標から直接移動量を計算

#### Metal実装の特徴

- イベントキューシステムを導入
- より洗練されたドラッグ状態管理

### 6. パフォーマンス特性

#### OpenGL ES 1.1

- シンプルな描画フロー
- 状態変更が多く、オーバーヘッドが大きい
- 小さな描画コマンドが多数実行される
- 高度なGPU最適化が難しい

#### Metal

- 効率的なバッチ処理
- 明示的なリソース管理によるパフォーマンス向上
- シェーダーによる柔軟な描画処理
- より複雑だが、パフォーマンスが高い

## 実装上の主要な課題と解決策

### 1. 座標変換の一貫性維持

**課題**: OpenGL、UIKit、MicroUIの座標系の違いによる混乱

**解決策**: 
- OpenGL版: 投影行列でY軸反転
- 明示的な座標変換関数の実装と一貫した使用
- シザーテストに特別な変換を適用

### 2. ヘッダー状態の永続化

**課題**: MicroUIのツリーノード状態は毎フレーム再構築される

**解決策**:
- 永続的な状態管理構造体の導入
- ヘッダーの位置情報のキャッシング
- 描画時とタップ時でID一致を保証する仕組み

### 3. タッチ座標の適切な処理

**課題**: 高解像度ディスプレイでの座標精度

**解決策**:
- contentScaleFactorを考慮したスケーリング
- 明示的な座標変換メソッドの実装

## 今後の改善点

1. **パフォーマンス最適化**:
   - OpenGL実装でのバッファ管理改善
   - 不要な状態変更の削減

2. **コード共通化**:
   - イベント処理部分のさらなる共通化
   - ヘッダー状態管理のライブラリ化

3. **アクセシビリティ対応**:
   - タップ領域の拡大
   - VoiceOverなどのアクセシビリティ機能サポート

## 結論

OpenGL ES 1.1とMetal、両方の実装はそれぞれ異なるアプローチを取りつつも、同じMicroUI APIを使用して一貫したユーザーエクスペリエンスを提供します。座標変換、タッチイベント処理、状態管理など様々な点で実装の違いがありますが、最終的なUI表示と操作感は同等です。

より詳細な実装の違いについては、各ソースファイルのヘッダコメントを参照してください。

- `ios_opengl1.1_microui.m` - OpenGL ES 1.1実装
- `ios_wip_metal_microui.m` - Metal実装
