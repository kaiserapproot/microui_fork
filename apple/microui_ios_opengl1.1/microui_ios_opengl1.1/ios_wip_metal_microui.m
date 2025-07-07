/*
 * MicroUI iOS Metal実装 - 修正履歴
 * -------------------------------------------------------------
 * 2025-06-22: ウィンドウドラッグ問題の修正
 * 
 * [問題]
 * - Log Windowのタイトルバーをドラッグすると、Demo Windowも一緒に移動する問題
 * 
 * [原因]
 * 1. ウィンドウ移動処理が重複して実装されていた:
 *    - touchesMoved: メソッドで各ウィンドウ名を使って正しく移動
 *    - updateDisplay: メソッドでは「Demo Window」名をハードコードして移動
 * 2. macOSのマウスイベントとiOSのタッチイベントの違いによる状態管理の問題
 *
 * [修正]
 * 1. updateDisplay: メソッドからウィンドウ移動処理を削除し、touchesMoved: のみで行うように変更
 * 2. setupWindowDrag: メソッドでドラッグ開始時の状態を明確に保存
 * 3. フォーカスとホバーの状態管理を改善
 *
 * [iOS/macOS対応の主な違いと実装の詳細]
 * 
 * 1. ホバー状態の処理
 *    - macOS: マウスカーソルがUI要素上に位置するだけでホバー状態が発生
 *    - iOS: カーソルの概念がないため、タッチ中のみホバー状態を擬似的に作成
 *    - 実装: ios_fix_hover_state()関数(107-146行目)でホバー状態を管理
 *           タッチ中はフォーカス要素とホバー要素を同期し、タッチ終了時はホバーをクリア
 *           例: ctx->hover = ctx->focus; // フォーカスとホバーの同期
 * 
 * 2. イベント処理の差異
 *    - macOS: mouseDown/mouseMove/mouseUpなどの細かいイベント体系
 *    - iOS: touchesBegan/Moved/Endedの3状態のみ
 *    - 実装: touchesBegan(1663-1789行目)でマウスイベントをエミュレート
 *           例: mu_input_mousedown(ctx, x, y, MU_MOUSE_LEFT); // タッチをマウスクリックに変換
 * 
 * 3. UI要素位置の管理
 *    - macOS: マウスの高精度な位置情報で直接UI要素を特定可能
 *    - iOS: 指の太さによる不正確さを補うためのヒットテスト拡張が必要
 *    - 実装: HeaderCache構造体(216-221行目)とios_cache_header()関数(254-264行目)
 *           UI描画時に位置情報をキャッシュし、タッチ時に素早く要素を特定
 * 
 * 4. ウィンドウドラッグ処理
 *    - macOS: マウス位置の連続的な変化で自然にドラッグ
 *    - iOS: タッチ開始位置と現在位置の差分計算が必要
 *    - 実装: setupWindowDrag()メソッド(1642-1676行目)とtouchesMoved()メソッド(1805-1842行目)
 *           例: CGFloat dx_f = location.x - self.dragStartTouchPosition.x; // 差分計算
 * 
 * 5. 状態の永続管理
 *    - macOS: ホバー状態などが自然に維持される
 *    - iOS: 明示的な状態保持と復元が必要
 *    - 実装: PersistentHeaderState構造体(144-148行目)と関連関数
 *           ios_set_header_state()、ios_get_header_state()などで永続的状態を管理
 *
 * [iOSの独自処理]
 * 
 * 1. タッチイベントのキュー管理
 *    - 実装: queueEvent()メソッド(1488-1496行目)
 *    - 内容: タッチイベントをNSMutableArrayにキューイングし非同期処理を可能に
 *           背景処理中にタッチイベントが失われないよう保持
 *
 * 2. DisplayLink管理
 *    - 実装: setupDisplayLink()メソッド(1256-1266行目)とupdateDisplay()メソッド(1335-1483行目)
 *    - 内容: 60FPSの安定したレンダリングを実現するためのCADisplayLink制御
 *           メインスレッドでの描画同期とパフォーマンス最適化
 *
 * 3. タッチ座標補正
 *    - 実装: getTouchLocation()メソッド(1558-1565行目)
 *    - 内容: デバイスの解像度とUI座標系の違いを補正し、MicroUIの座標系に正確にマッピング
 *
 * [Metal処理の内容]
 * 
 * 1. レンダラー初期化
 *    - 実装: MetalRenderer initWithMetalKitView()メソッド(663-689行目)
 *    - 内容: Metal用のデバイス、コマンドキュー、レンダリングパイプラインの初期化
 *           固有のシェーダーとバッファ準備
 *
 * 2. アトラステクスチャ管理
 *    - 実装: createAtlasTexture()メソッド(757-790行目)
 *    - 内容: MicroUIのUI要素をレンダリングするためのアトラステクスチャをMetalで処理
 *           8bitグレースケールテクスチャを生成・管理
 *
 * 3. バッファ管理
 *    - 実装: renderFrame()メソッド(891-923行目)
 *    - 内容: 頂点、インデックス、ユニフォームバッファの効率的管理
 *           MicroUIのレンダリングコマンドをMetalのドローコールに変換
 *
 * [MicroUIの処理とiOSへの適応]
 * 
 * 1. コマンド処理パイプライン
 *    - 原理: MicroUIはレンダリングコマンドを生成、実行環境がそれを解釈して描画
 *    - 適応: mu_begin/mu_end()の間で生成されたコマンドをiosDrawCommandList()で処理(1097-1149行目)
 *           例: mu_Command* cmd = NULL; while (mu_next_command(ctx, &cmd)) { ... }
 *
 * 2. ウィジェットレイアウト適応
 *    - 原理: MicroUIは座標と幅/高さで要素を配置
 *    - 適応: iOS画面サイズに合わせた自動スケーリング(1550-1565行目)
 *           タッチ領域の拡大でファットフィンガー問題に対応
 *
 * 3. イベント伝播モデルの変換
 *    - 原理: MicroUIのイベント処理はマウス中心設計
 *    - 適応: タッチイベントをmu_input_mousedown/move/up()に変換(1663-1852行目)
 *           マルチタッチからシングルタッチへの簡略化
 *
 * 4. 描画処理の最適化
 *    - 原理: MicroUIは描画コマンドを抽象化している
 *    - 適応: Metalへ効率的に変換するための中間レイヤー(891-989行目)
 *           バッチ処理によるドローコール最適化
 *           ios_set_header_state()、ios_get_header_state()などで永続的状態を管理
 *
 * [描画処理（Rendering Process）の詳細]
 * 
 * 1. レンダリングパイプラインの全体フロー
 *    - CADisplayLinkによる60FPSタイミング制御（setupDisplayLink: 1256-1266行目）
 *    - updateDisplay:メソッドがフレーム描画のメイン入口点（1451-1483行目）
 *    - mu_begin/mu_end間でMicroUIの論理フレームを構築
 *    - 描画コマンドをMetalコマンドバッファに変換して実行
 *
 * 2. 描画更新とジオメトリ構築（updateDisplay:メソッド内）
 *    - ステップ1: mu_begin(ctx)でMicroUIコンテキスト更新開始（1462行目）
 *    - ステップ2: ヘッダーキャッシュをクリア（1465行目）
 *    - ステップ3: キューに溜まったイベントの処理（1468-1486行目）
 *    - ステップ4: ウィンドウとウィジェットの配置・レイアウト（processFrameメソッド内、1097-1167行目）
 *    - ステップ5: mu_end(ctx)でMicroUIコマンド生成完了（1600行目）
 *    - ステップ6: iosDrawCommandList関数でコマンド解釈・変換（1097-1149行目）
 *    - ステップ7: drawablePrepareToRenderFrameで描画準備（1604行目）
 *    - ステップ8: MEtalView.draw実行（1609行目）
 *
 * 3. Metal描画パイプライン（renderFrameメソッド: 1072-1096行目）
 *    - ステップ1: コマンドバッファとレンダーコマンドエンコーダー生成（891-923行目）
 *    - ステップ2: 頂点バッファ、インデックスバッファ更新（924-950行目） 
 *    - ステップ3: テクスチャとサンプラー設定（952-970行目）
 *    - ステップ4: ドローコール実行：drawIndexedPrimitives（972-980行目）
 *    - ステップ5: コマンドバッファをコミットして描画実行（982-988行目）
 *
 * 4. 描画最適化技術
 *    - インスタンシング: 同一テクスチャの要素をバッチ処理（renderFrameメソッド内）
 *    - アトラステクスチャ: 複数UIパーツを1枚のテクスチャに集約（757-790行目）
 *    - ジオメトリバッチング: 同一マテリアルの要素をグループ化（950-970行目）
 *    - ダブルバッファリング: MetalViewのdrawableTexture切替えで描画とディスプレイ転送を分離
 * 
 * [タップイベント処理の詳細]
 *
 * 1. タップイベント処理のフロー全体
 *    - iOS UIKitイベント → MicroUI内部イベントへの変換
 *    - タッチ座標の正規化処理とMicroUI座標系へのマッピング
 *    - キュー管理による非同期イベント処理
 *    - 階層化されたヒットテスト処理によるUI要素特定
 * 
 * 2. タップ開始処理（touchesBegan:メソッド: 1790-1852行目）
 *    - ステップ1: タッチ座標取得とバリデーション（1791-1809行目）
 *      - getTouchLocation:メソッドで座標を取得（1558-1565行目）
 *      - 座標が有効範囲外なら処理を終了
 *    - ステップ2: タッチ対象の特定（1811-1818行目）
 *      - windowNameAtTitleBarPoint:メソッドでヒットしたウィンドウ判定（以前実装を追加）
 *      - ios_detect_header_at_pointメソッドで特殊ヘッダー要素の判定
 *    - ステップ3: ウィンドウドラッグ開始処理（1820-1835行目）
 *      - ウィンドウタイトルバータッチならドラッグモード開始
 *      - setupWindowDrag:メソッドでドラッグ状態を初期化（1642-1676行目）
 *      - ドラッグ開始位置と対象ウィンドウ情報を保存
 *    - ステップ4: ヘッダー操作処理（1836-1850行目）
 *      - ヘッダータッチならヘッダー固有の状態を設定
 *      - ios_get_header_at_position関数でヘッダーIDと状態を取得
 *      - mu_set_focusとios_set_hoverで状態更新
 *    - ステップ5: イベントキューイング（1866-1886行目）
 *      - queueEvent:メソッドでタッチ状態をイベントキューに追加（1488-1496行目）
 *      - 非同期のUpdateDisplay処理で後ほど反映
 *
 * 3. タップ移動処理（touchesMoved:メソッド: 1891-1945行目）
 *    - ステップ1: ドラッグ判定と処理（1892-1925行目）
 *      - self.isDraggingWindowフラグでドラッグモード確認
 *      - ドラッグ中なら位置差分を計算してウィンドウ移動
 *      - 明示的に対象ウィンドウコンテナのrect.x/yを更新
 *    - ステップ2: ヘッダー操作継続処理（1926-1934行目）
 *      - ヘッダー操作中なら状態維持
 *    - ステップ3: イベントキューイング（1936-1944行目）
 *      - 移動イベントをキューに追加（タイプ="move"）
 * 
 * 4. タップ終了処理（touchesEnded:メソッド: 1946-1980行目）
 *    - ステップ1: 状態のクリーンアップ（1947-1956行目）
 *      - ドラッグ状態や特殊操作フラグをリセット
 *      - ctx->mouse_down = 0など内部状態クリア
 *    - ステップ2: mu_input_mouseupの呼び出し（1958-1962行目）
 *      - MicroUIにマウスアップイベントを送信
 *      - ios_clear_hover()でホバー状態をクリア（iOS特有処理）
 *    - ステップ3: 最終イベント処理（1964-1979行目）
 *      - イベントをキューに追加（タイプ="up"）
 *      - 次のフレーム描画を強制的に要求（self.needsRedraw = YES）
 * 
 * 5. ヒットテスト処理の最適化
 *    - ヘッダーキャッシュによる高速ヒットテスト（HeaderCache, ios_cache_header関数）
 *    - ウィンドウ名を使った効率的なヒットテスト（windowNameAtTitleBarPointメソッド）
 *    - マルチタッチからシングルタッチへの簡略化（最初のタッチのみを処理）
 *    - イベントキューイングによる入力集約と処理最適化
 */

// MetalMicroUI.h
#ifndef MetalMicroUI_h
#define MetalMicroUI_h

#import <MetalKit/MetalKit.h>
#import <UIKit/UIKit.h>
#include "microui.h"




#include "atlas.inl"  // atlas.inlをインクルード

// 頂点構造体
typedef struct {
    vector_float2 position;
    vector_float2 texCoord;
    vector_float4 color;
} MetalVertex;

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

// micro_ui_view_controllerクラス (スネークケース命名規則に合わせる)
@interface micro_ui_view_controller : UIViewController
@property (nonatomic, strong) MTKView *metalView;
@property (nonatomic, strong) metal_renderer *renderer;
@property (nonatomic, strong) CADisplayLink *displayLink;

// イベントキュー関連のプロパティ
@property (nonatomic, strong) NSMutableArray *pendingEvents;
@property (nonatomic, strong) NSLock *eventLock;
@property (nonatomic, assign) BOOL needsRedraw;

// プルダウン処理関連のプロパティ
@property (nonatomic, assign) CGPoint lastHeaderTapPosition;
@property (nonatomic, assign) BOOL didTapHeader;

// ウィンドウドラッグ関連のプロパティ
@property (nonatomic, assign) CGPoint dragStartTouchPosition;
@property (nonatomic, assign) int dragStartWindowX;
@property (nonatomic, assign) int dragStartWindowY;
@property (nonatomic, assign) BOOL isDraggingWindow;
@property (nonatomic, assign) CGFloat dragStartWindowXF;
@property (nonatomic, assign) CGFloat dragStartWindowYF;
@property (nonatomic, assign) CGFloat currentWindowXF;
@property (nonatomic, assign) CGFloat currentWindowYF;

// 追加: ドラッグ中のウィンドウ名を保持
@property (nonatomic, strong) NSString *draggingWindowName;

// タイトルバーのウィンドウ名取得メソッド
- (NSString *)windowNameAtTitleBarPoint:(CGPoint)point;
@end


#endif /* MetalMicroUI_h */

// MetalMicroUI.m
#import <simd/simd.h>

// 関数プロトタイプ宣言
static void write_log(const char* text);
static void ios_fix_hover_state(mu_Context* ctx);
static int ios_header_ex(mu_Context* ctx, const char* label, int opt);
static mu_Id ios_get_header_id(mu_Context* ctx, const char* label);
static void ios_toggle_header_state(mu_Context* ctx, mu_Id id);

// モバイル対応のためのMicroUI拡張関数
static mu_Id lastHoverId = 0;

/**
 * iOS用のホバー状態管理関数 - タッチデバイスでホバー状態を適切に扱うための最適化版
 * - マウス操作では自動的にcursor位置でのホバーが検出されるが、タッチではそれができない
 * - この関数はタッチでも適切なホバー状態を維持するよう補助する
 */
static void ios_fix_hover_state(mu_Context* ctx) {
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
        
        #ifdef DEBUG
        // デバッグ時のみログ出力
        write_log("ホバー状態をクリア");
        #endif
    } else {
        // ホバー状態を設定
        ctx->hover = id;
        ctx->prev_hover = id;
        
        #ifdef DEBUG
        // デバッグ時のみログ出力
        write_log([NSString stringWithFormat:@"ホバー設定: ID=%u", id].UTF8String);
        #endif
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
    
    char msg[128];
    snprintf(msg, sizeof(msg), "ヘッダー状態を切り替え: '%s' -> %s (既存=%s)",
           label ? label : "不明", !currentState ? "展開" : "折りたたみ", found ? "有" : "無");
    write_log(msg);
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

// キャッシュから指定座標にあるヘッダーを検索
static BOOL ios_find_header_at_position(int x, int y, HeaderCache** outCache) {
    for (int i = 0; i < headerCacheCount; i++) {
        HeaderCache* cache = &headerCache[i];
        if (x >= cache->rect.x && x < cache->rect.x + cache->rect.w &&
            y >= cache->rect.y && y < cache->rect.y + cache->rect.h) {
            
            if (outCache) {
                *outCache = cache;
            }
            return YES;
        }
    }
    return NO;
}

// ヘッダーエレメント検出ヘルパー（改良版）
static BOOL ios_is_header_clicked(mu_Context* ctx, const char* label, int x, int y) {
    if (!ctx || !label) return NO;
    
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    
    // キャッシュから検索
    for (int i = 0; i < headerCacheCount; i++) {
        if (headerCache[i].id == id) {
            mu_Rect rect = headerCache[i].rect;
            if (x >= rect.x && x < rect.x + rect.w &&
                y >= rect.y && y < rect.y + rect.h) {
                
                char msg[128];
                snprintf(msg, sizeof(msg), "キャッシュからヘッダー検出: '%s' (ID=%u) 座標=(%d,%d)", 
                       label, id, x, y);
                write_log(msg);
                
                return YES;
            }
        }
    }
    
    // キャッシュにない場合は前回の描画データから検索（従来の方法）
    if (ctx->last_id == id) {
        if (x >= ctx->last_rect.x && x < ctx->last_rect.x + ctx->last_rect.w &&
            y >= ctx->last_rect.y && y < ctx->last_rect.y + ctx->last_rect.h) {
            
            char msg[128];
            snprintf(msg, sizeof(msg), "直前のヘッダー検出: '%s' (ID=%u) 座標=(%d,%d)", 
                   label, id, x, y);
            write_log(msg);
            
            return YES;
        }
    }
    
    return NO;
}

// ヘッダーID取得関数
static mu_Id ios_get_header_id(mu_Context* ctx, const char* label) {
    if (!ctx || !label) return 0;
    
    mu_Id id = mu_get_id(ctx, label, strlen(label));
    
    char msg[128];
    snprintf(msg, sizeof(msg), "ヘッダーID計算: '%s' -> ID=%u", label, id);
    write_log(msg);
    
    return id;
}

// 直接ヘッダーの状態を切り替える（MicroUIの内部処理をエミュレート）
static void ios_toggle_header_state(mu_Context* ctx, mu_Id id) {
    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
    
    if (idx >= 0) {
        // ヘッダーが展開されている場合、折りたたむ
        memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem));
        write_log("ヘッダーを閉じました");
    } else {
        // ヘッダーが折りたたまれている場合、展開する
        mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
        write_log("ヘッダーを展開しました");
    }
}

// マウス位置にあるヘッダーのIDを取得する
static mu_Id mu_get_hover_id(mu_Context* ctx, int x, int y) {
    if (!ctx) return 0;
    
    // コマンドリストをスキャンしてヘッダーの矩形を探す
    mu_Command* cmd = NULL;
    mu_Command* header_cmd = NULL;
    mu_Id header_id = 0;
    
    // コマンドリストを最初から読み直す
    ctx->command_list.idx = 0;
    
    while (mu_next_command(ctx, &cmd)) {
        if (cmd->type == MU_COMMAND_CLIP) {
            mu_Rect rect = cmd->clip.rect;
            if (x >= rect.x && x < rect.x + rect.w &&
                y >= rect.y && y < rect.y + rect.h) {
                header_cmd = cmd;
                header_id = ctx->last_id;
            }
        }
    }
    
    return header_id;
}

// 指定座標にあるヘッダー情報を取得する統合関数
static BOOL ios_get_header_at_position(mu_Context* ctx, CGPoint position, mu_Id* outId, const char** outLabel, BOOL* outActive) {
    char msg[128];
    snprintf(msg, sizeof(msg), "[LOG] ios_get_header_at_position: pos=(%.1f,%.1f)", position.x, position.y);
    write_log(msg);

    HeaderCache* cache = NULL;
    
    // キャッシュを検索
    if (ios_find_header_at_position(position.x, position.y, &cache)) {
        if (outId) *outId = cache->id;
        if (outLabel) *outLabel = cache->label;
        if (outActive) *outActive = cache->active;
        
        char msg[128];
        snprintf(msg, sizeof(msg), "ヘッダー発見: '%s' ID=%u 状態=%s", 
               cache->label, cache->id, cache->active ? "展開" : "折りたたみ");
        write_log(msg);
        
        return YES;
    }
    
    // キャッシュになければ、既知のヘッダー名リストから試す
    const char* knownHeaders[] = {
        "Window Info",
        "Test Buttons",
        "Background Color"
    };
    int headerCount = sizeof(knownHeaders) / sizeof(knownHeaders[0]);
    
    for (int i = 0; i < headerCount; i++) {
        if (ios_is_header_clicked(ctx, knownHeaders[i], position.x, position.y)) {
            mu_Id id = ios_get_header_id(ctx, knownHeaders[i]);
            int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
            
            if (outId) *outId = id;
            if (outLabel) *outLabel = knownHeaders[i];
            if (outActive) *outActive = (idx >= 0);
            
            return YES;
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
        char msg[128];
        snprintf(msg, sizeof(msg), "ヘッダー '%s' がクリックされました (ID=%u)",
               headerLabel, headerId);
        write_log(msg);
        
        // ヘッダーIDの状態を確認
        int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerId);
        snprintf(msg, sizeof(msg), "ヘッダープール状態: idx=%d (負の場合は非アクティブ)", idx);
        write_log(msg);
        
        return YES;
    }
    
    return NO;
}

// グローバル変数
static char logbuf[64000];
static int logbuf_updated = 0;
static float bg[4] = { 90, 95, 100, 105 };
static char last_log_text[256] = "";
static int drag_log_counter = 0;
static long header_log_counter = 0;

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

// 最適化された重複抑制付きwrite_log関数
static void write_log(const char* text) {
    if (!text || !*text) return;  // NULLまたは空文字列チェック
    
    // 直前と同じ内容のログは抑制（メモリ比較で高速化）
    if (strcmp(text, last_log_text) == 0) return;
    
    // 現在のログを保存
    strncpy(last_log_text, text, sizeof(last_log_text)-1);
    last_log_text[sizeof(last_log_text)-1] = '\0';
    
    // NSLogに出力
    NSLog(@"[MicroUI] %s", text);
    
    // 既存のlogbufにも記録（互換性のため）
    size_t text_len = strlen(text);
    size_t current_len = strlen(logbuf);
    size_t required_len = current_len + text_len + 2; // テキスト + 改行 + NULL終端
    
    // バッファオーバーフローを防ぐ
    if (required_len >= sizeof(logbuf)) {
        // より効率的なバッファ管理: 必要なスペースを確保
        size_t clear_size = required_len - sizeof(logbuf) + 1024; // 余裕を持たせる
        if (clear_size > current_len) {
            // バッファを完全にクリア
            logbuf[0] = '\0';
            current_len = 0;
        } else {
            // 必要な量だけ古いログを削除
            memmove(logbuf, logbuf + clear_size, current_len - clear_size);
            logbuf[current_len - clear_size] = '\0';
            current_len = strlen(logbuf);
        }
    }
    
    // 改行を追加（最初のエントリでない場合）
    if (current_len > 0) {
        strcat(logbuf, "\n");
    }
    
    // テキストを追加
    strncat(logbuf, text, sizeof(logbuf) - current_len - 2);
    
    logbuf_updated = 1;
}

// より安全なuint8_slider関数
static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high) {
    if (!ctx || !value) return 0;  // NULLポインタチェック
    
    int res;
    static float tmp;
    mu_push_id(ctx, &value, sizeof(value));
    tmp = *value;
    res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
    *value = (unsigned char)tmp;  // 明示的なキャスト
    mu_pop_id(ctx);
    return res;
}

// MicroUIのヘッダー描画をフックするラッパー関数
int ios_header_wrapper(mu_Context* ctx, const char* label, int istreenode, int opt) {
//    char msg[128];
//    snprintf(msg, sizeof(msg), "[LOG] ios_header_wrapper: label=%s", label);
//    write_log(msg);

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

// 指定位置のヘッダーを検出する
static BOOL ios_detect_header_at_point(mu_Context* ctx, CGPoint point, HeaderInfo* outInfo) {
    if (!ctx || !outInfo) return NO;

    // まずキャッシュから検索
    HeaderCache* cache = NULL;
    if (ios_find_header_at_position(point.x, point.y, &cache)) {
        outInfo->id = cache->id;
        outInfo->rect = cache->rect;
        outInfo->label = cache->label;
        outInfo->poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, cache->id);
        
        char msg[128];
        snprintf(msg, sizeof(msg), "ヘッダー検出（キャッシュ）: '%s' ID=%u 位置=(%d,%d)", 
               cache->label, cache->id, cache->rect.x, cache->rect.y);
        write_log(msg);
        
        return YES;
    }

    // キャッシュになければウィンドウ内のヘッダーをスキャン
    mu_Container* win = mu_get_container(ctx, "Demo Window");
    if (!win) {
        write_log("ウィンドウが見つかりません");
        return NO;
    }

    // ウィンドウ内の座標かチェック
    if (point.x < win->rect.x || point.x >= win->rect.x + win->rect.w ||
        point.y < win->rect.y || point.y >= win->rect.y + win->rect.h) {
        write_log("ウィンドウ外のタップを検出");
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
            outInfo->rect = ctx->last_rect;  // 最後の描画位置を使用
            outInfo->label = *label;
            outInfo->poolIndex = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
            
            char msg[128];
            snprintf(msg, sizeof(msg), "ヘッダー検出（スキャン）: '%s' ID=%u 位置=(%d,%d)", 
                   *label, id, ctx->last_rect.x, ctx->last_rect.y);
            write_log(msg);
            
            return YES;
        }
    }

    write_log("ヘッダーが見つかりませんでした");
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
    // フレーム開始時の状態
    // mu_Context *ctx = self.muiContext;
    // NSString *startMsg = [NSString stringWithFormat:@"FRAME_START focus=%d down=%d", 
    //                       ctx->focus, ctx->mouse_down];
    // write_log([startMsg UTF8String]);
    
    // mu_begin(self.muiContext);
    // [self processFrame];
    // mu_end(self.muiContext);
    
    // フレーム終了時の状態
    // NSString *endMsg = [NSString stringWithFormat:@"FRAME_END focus=%d down=%d", 
    //                     ctx->focus, ctx->mouse_down];
    // write_log([endMsg UTF8String]);
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
            
            mu_label(self.muiContext, "Red:");   uint8_slider(self.muiContext, (unsigned char*)&bg[0], 0, 255);
            mu_label(self.muiContext, "Green:"); uint8_slider(self.muiContext, (unsigned char*)&bg[1], 0, 255);
            mu_label(self.muiContext, "Blue:");  uint8_slider(self.muiContext, (unsigned char*)&bg[2], 0, 255);
        }
        
        mu_end_window(self.muiContext);
    }
    
    // ログウィンドウ
    if (mu_begin_window(self.muiContext, "Log Window", mu_rect(40, 310, 300, 200))) {
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

// MicroUIのヘッダー処理をiOS向けに拡張
static int ios_header_ex(mu_Context* ctx, const char* label, int opt) {
    if (!ctx || !label) return 0;
    
    // 標準のヘッダー処理を実行
    return mu_header_ex(ctx, label, opt);
}

// MetalRendererクラスのdealloc修正

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

// より詳細なログ出力関数を追加


// micro_ui_view_controller実装 (スネークケース命名規則に合わせる)
@implementation micro_ui_view_controller
// コンテナのドラッグ状態を判定するヘルパーメソッドを修正
- (void)logContainerState:(NSString *)prefix {
    mu_Container *win = mu_get_container(self.renderer.muiContext, "Demo Window");
    if (win) {
        // MicroUIのコンテナの正しい判定方法
        // ウィンドウのドラッグ状態はmuiContextのfocusとmouse_downの状態で判定
        // mu_Containerには直接的なidxは存在しません
        char msg[256];
        
        BOOL isDragging = (self.renderer.muiContext->mouse_down && 
                          self.renderer.muiContext->focus != 0);
        
        snprintf(msg, sizeof(msg), "%s - ウィンドウ位置: x=%d, y=%d, 状態=%s", 
                [prefix UTF8String], win->rect.x, win->rect.y,
                isDragging ? "ドラッグ中" : "通常");
        write_log(msg);
    }
}
// 強化版状態ログ出力関数
- (void)logMicroUIState:(NSString*)eventType location:(CGPoint)location {
    mu_Context *ctx = self.renderer.muiContext;
    
    // コンソールに詳細ログ
    NSLog(@"[MicroUI状態] %@ - 座標: (%.1f, %.1f)", eventType, location.x, location.y);
    NSLog(@"[MicroUI状態] hover=%d, focus=%d, mouse_down=%d", ctx->hover, ctx->focus, ctx->mouse_down);
    NSLog(@"[MicroUI状態] mouse_pressed=%d, updated_focus=%d", ctx->mouse_pressed, ctx->updated_focus);
    
    // アプリ内ログウィンドウにも情報を出力
    char detailedLog[256];
    snprintf(detailedLog, sizeof(detailedLog), 
             "%s - 座標:(%.0f,%.0f) focus:%d hover:%d down:%d", 
             [eventType UTF8String], location.x, location.y, 
             ctx->focus, ctx->hover, ctx->mouse_down);
    write_log(detailedLog);
    
    // コンテナ状態もログ
    [self logContainerState:eventType];
}

// タイトルバーをクリックしたウィンドウ名を取得するメソッド
- (NSString *)windowNameAtTitleBarPoint:(CGPoint)point {
    // MicroUIコンテキストの取得
    mu_Context *ctx = self.renderer.muiContext;
    if (!ctx) return nil;

    // 既知のウィンドウ名リスト
    NSArray<NSString *> *knownWindows = @[
        @"Demo Window", 
        @"Log Window", 
        @"Style Editor"
    ];

    // 各ウィンドウをチェック
    for (NSString *windowName in knownWindows) {
        const char *name = [windowName UTF8String];
        mu_Container *container = mu_get_container(ctx, name);
        if (!container) continue;

        // タイトルバー領域（典型的にはウィンドウの上部18-24ピクセル）
        mu_Rect titleBar = container->rect;
        titleBar.h = 24; // タイトルバーの高さ
        
        // 座標がタイトルバーの矩形内にあるかチェック
        if (point.x >= titleBar.x && point.x < titleBar.x + titleBar.w &&
            point.y >= titleBar.y && point.y < titleBar.y + titleBar.h) {
            return windowName;
        }
    }
    
    return nil;
}

// viewDidLoadメソッド内の修正
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
}
// イベントキューにイベントを追加するメソッド
- (void)queueEvent:(NSDictionary *)event {
    [self.eventLock lock];
    [self.pendingEvents addObject:event];
    self.needsRedraw = YES;
    [self.eventLock unlock];
    
    // 即時描画を要求してUI応答性を向上
    dispatch_async(dispatch_get_main_queue(), ^{
        [self.metalView setNeedsDisplay];
    });
}

// メインスレッドでのレンダリング処理とイベント処理
- (void)updateDisplay:(CADisplayLink *)displayLink {
    mu_Context *ctx = self.renderer.muiContext;
    
    // ログの最初に現在の状態をダンプ（フレーム開始前）
    if (ctx->mouse_down || ctx->hover != 0 || ctx->focus != 0) {
        char startMsg[128];
        snprintf(startMsg, sizeof(startMsg), 
               "フレーム開始状態: mouse_down=%d, focus=%u, hover=%u",
               ctx->mouse_down, ctx->focus, ctx->hover);
        write_log(startMsg);
    }
    
    // iOS向けのMicroUI状態修正（毎フレーム実行）
    ios_fix_hover_state(ctx);
    
    mu_begin(ctx);
    
    // 新しいフレームの開始時にヘッダーキャッシュをクリア
    ios_clear_header_cache();
    
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
        
        // 簡潔なログ出力
        write_log("ヘッダータップ処理完了");
    }
    
    // 大量イベントの場合のみログ出力
    if (events.count > 10) {
        write_log([NSString stringWithFormat:@"イベントキュー処理: %lu件", (unsigned long)events.count].UTF8String);
    }
    
    // イベントを処理
    BOOL hadEvents = (events.count > 0);
    mu_Id lastFocusedId = ctx->focus;  // 現在のフォーカス状態を保存
    
    for (NSDictionary *event in events) {
        NSString *type = event[@"type"];
        CGPoint position = [event[@"position"] CGPointValue];
        
        // 安全確認: 座標が有効範囲内か
        int x = (int)position.x;
        int y = (int)position.y;
        
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
            BOOL isHeaderTouch = [event[@"isHeaderTouch"] boolValue];
            
            if (isTitleBarTouch) {
                // タイトルバークリック処理
                mu_Container *win = mu_get_container(ctx, "Demo Window");
                if (win) {
                    // タイトルバーのIDを確実に設定
                    mu_Id id = [self calculateTitleId:ctx windowName:"Demo Window"];
                    mu_set_focus(ctx, id);
                    ios_set_hover(ctx, id);
                    
                    // デバッグログ
                    char msg[128];
                    snprintf(msg, sizeof(msg), "タイトルバークリック処理: ID=%u 設定完了", id);
                    write_log(msg);
                }
            } 
            else if (isHeaderTouch) {
                // ヘッダークリック処理の準備
                self.didTapHeader = YES;
                self.lastHeaderTapPosition = position;
                
                write_log("ヘッダークリック処理準備開始");
                
                // 改善: ヘッダー情報の取得
                mu_Id headerId = 0;
                const char* headerLabel = NULL;
                
                BOOL isActive = NO;
                if (ios_get_header_at_position(ctx, position, &headerId, &headerLabel, &isActive)) {
                    // ヘッダーID設定とフォーカスの設定
                    mu_set_focus(ctx, headerId);
                    ios_set_hover(ctx, headerId);
                    
                    // mouse_pressedを設定して、ヘッダーロジックをトリガー
                    ctx->mouse_pressed = MU_MOUSE_LEFT;
                    
                    // 現在のヘッダー状態をログに記録
                    int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, headerId);
                    char msg[128];
                    snprintf(msg, sizeof(msg), 
                           "ヘッダー '%s' のフォーカス設定: ID=%u, アクティブ状態=%s", 
                           headerLabel, headerId, (idx >= 0) ? "アクティブ" : "非アクティブ");
                    write_log(msg);
                    
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
            }
        }
        else if ([type isEqualToString:@"up"]) {
            // マウスアップイベント - タッチ終了
            BOOL wasDragging = [event[@"wasDragging"] boolValue];
            
            mu_input_mousemove(ctx, x, y);
            mu_input_mouseup(ctx, x, y, MU_MOUSE_LEFT);
            
            // タッチ終了後の状態を安定化
            ios_set_hover(ctx, 0);
            
            // ドラッグ操作終了の場合の追加処理
            if (wasDragging) {
                char msg[128];
                snprintf(msg, sizeof(msg), "ドラッグ終了後処理: focus=%u, hover=%u", 
                        ctx->focus, ctx->hover);
                write_log(msg);
            }
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
    
    // ヘッダー操作後の状態クリーンアップを優先
    // ヘッダー操作後はタッチが終了しているのでmouse_pressedをクリアする
    if (ctx->mouse_pressed && !ctx->mouse_down) {
        ctx->mouse_pressed = 0;
        // ログを減らし、10回に1回だけ出力
        static int cleanupCount = 0;
        if (++cleanupCount % 10 == 0) {
            write_log("フレーム終了時: mouse_pressedをクリア（タッチ終了状態）");
        }
    }
    
    // ドラッグ状態だけはホバー状態を維持
    if (ctx->mouse_down && ctx->focus != 0 && ctx->hover != ctx->focus) {
        ios_set_hover(ctx, ctx->focus);
        
        // ログを少なくして必要なときだけ出力
        static int dragFixCount = 0;
        if (++dragFixCount % 10 == 0) {
            char msg[128];
            snprintf(msg, sizeof(msg), 
                   "ドラッグ中のホバー状態を維持: focus=%u, hover=%u",
                   ctx->focus, ctx->hover);
            write_log(msg);
        }
    }
    
    // タッチがない場合は確実にホバーをクリア（安定化）
    if (!ctx->mouse_down && ctx->hover != 0) {
        ios_set_hover(ctx, 0);
    }
    
    // 異常値のチェック（フレーム終了時）
    if (ctx->hover < 0 || ctx->focus < 0) {
        char errorMsg[128];
        snprintf(errorMsg, sizeof(errorMsg), 
               "異常値検出: focus=%d, hover=%d - 強制リセット",
               ctx->focus, ctx->hover);
        write_log(errorMsg);
        
        // 強制リセット
        ctx->hover = 0;
        ctx->prev_hover = 0;
        
        if (ctx->focus < 0) {
            ctx->focus = 0;
            ctx->prev_focus = 0;
        }
    }
    
    // タッチイベント関連のグローバル状態を定期的にログに記録（重要な部分だけ）
    static int frameCounter = 0;
    if (++frameCounter % 60 == 0) { // 約1秒に1回
        if (ctx->focus != 0 || ctx->hover != 0 || ctx->mouse_down || ctx->mouse_pressed) {
            char statusMsg[128];
            snprintf(statusMsg, sizeof(statusMsg), 
                   "状態サマリー: focus=%u, hover=%u, mouse_down=%d, mouse_pressed=%d",
                   ctx->focus, ctx->hover, ctx->mouse_down, ctx->mouse_pressed);
            write_log(statusMsg);
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
    mu_Rect title_rect = { win ? win->rect.x : -1, win ? win->rect.y : -1, win ? win->rect.w : -1, titleHeight };
    char msg[128];
    snprintf(msg, sizeof(msg), "[LOG] タイトルバー判定: point=(%.1f,%.1f) win=(%d,%d,%d,%d) title_height=%d", point.x, point.y, win ? win->rect.x : -1, win ? win->rect.y : -1, win ? win->rect.w : -1, win ? win->rect.h : -1, titleHeight);
    write_log(msg);

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
    mu_Container *win = NULL;
    
    #ifdef DEBUG
    write_log([NSString stringWithFormat:@"touchesBegan: (%.1f,%.1f)", location.x, location.y].UTF8String);
    #endif
    
    // 座標チェックとコンテキスト確認（範囲外または無効な場合は早期リターン）
    if (!ctx || location.x < 0 || location.x >= self.metalView.bounds.size.width ||
        location.y < 0 || location.y >= self.metalView.bounds.size.height) {
        return;
    }
    
    // 安全に前の状態をクリア
    ios_set_hover(ctx, 0);
    self.didTapHeader = NO;
    
    // タイトルバー判定
    NSString *hitWindow = [self windowNameAtTitleBarPoint:location];
    // ヘッダー判定
    HeaderInfo headerInfo = {0};
    BOOL isHeaderTouch = ios_detect_header_at_point(ctx, location, &headerInfo);
    // eventからの辞書アクセスは一切しない
    
    if (hitWindow) {
        self.draggingWindowName = hitWindow;
        [self setupWindowDrag:ctx windowName:[hitWindow UTF8String] position:location];
        write_log("タイトルバータッチ: ドラッグモード開始");
        ctx->mouse_down = 1;
        ctx->mouse_pressed = 1;
        ios_set_hover(ctx, mu_get_id(ctx, [hitWindow UTF8String], strlen([hitWindow UTF8String])));
        self.isDraggingWindow = YES;
        self.dragStartTouchPosition = location;
        win = mu_get_container(ctx, [hitWindow UTF8String]);
        if (win) {
            self.dragStartWindowX = win->rect.x;
            self.dragStartWindowY = win->rect.y;
        }
    } else if (isHeaderTouch) {
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
            char msg[128];
            snprintf(msg, sizeof(msg), 
                   "ヘッダー '%s' のフォーカス設定: ID=%u, アクティブ状態=%s", 
                   headerLabel, headerId, (idx >= 0) ? "アクティブ" : "非アクティブ");
            write_log(msg);
            
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
        
        ctx->mouse_down = 1;
        ctx->mouse_pressed = 1;
    }
    
    // マウス座標を更新
    ctx->mouse_pos = mu_vec2(location.x, location.y);
    
    // 再描画をトリガー
    self.needsRedraw = YES;
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    mu_Context *ctx = self.renderer.muiContext;
    
    // ドラッグ状態の判定変数を再定義
    BOOL isDraggingTitle = (ctx && ctx->mouse_down == MU_MOUSE_LEFT && ctx->focus != 0 && 
                          self.isDraggingWindow && self.draggingWindowName);
    
    // ウィンドウドラッグ処理
    if (isDraggingTitle) {
        mu_Container *win = mu_get_container(ctx, [self.draggingWindowName UTF8String]);
        if (win) {
            // 浮動小数点での計算を維持してから最終的に整数に変換（より滑らかな動き）
            CGFloat dx_f = location.x - self.dragStartTouchPosition.x;
            CGFloat dy_f = location.y - self.dragStartTouchPosition.y;
            int newX = self.dragStartWindowX + (int)roundf(dx_f);
            int newY = self.dragStartWindowY + (int)roundf(dy_f);
            
            // ウィンドウ位置の更新
            win->rect.x = newX;
            win->rect.y = newY;
            
            #ifdef DEBUG
            // シグニフィカントな変更のみログに記録
            static int lastLoggedX = 0, lastLoggedY = 0;
            if (abs(lastLoggedX - newX) > 5 || abs(lastLoggedY - newY) > 5) {
                lastLoggedX = newX;
                lastLoggedY = newY;
                write_log([NSString stringWithFormat:@"ウィンドウ移動: %s -> (%d,%d)", 
                          [self.draggingWindowName UTF8String], newX, newY].UTF8String);
            }
            #endif
        }
    }
    
    // イベントをキューに追加
    [self queueEvent:@{
        @"type": @"move",
        @"position": [NSValue valueWithCGPoint:location],
        @"isDragging": @(isDraggingTitle),
        @"previousPosition": [NSValue valueWithCGPoint:[touch previousLocationInView:self.metalView]]
    }];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event {
    UITouch *touch = [touches anyObject];
    CGPoint location = [self getTouchLocation:touch];
    mu_Context *ctx = self.renderer.muiContext;
    
    #ifdef DEBUG
    // ドラッグ中のウィンドウがある場合のみログ出力
    if (self.isDraggingWindow && self.draggingWindowName) {
        mu_Container *win = mu_get_container(ctx, [self.draggingWindowName UTF8String]);
        write_log([NSString stringWithFormat:@"タッチ終了: (%.1f,%.1f), ウィンドウ位置=(%d,%d)", 
                  location.x, location.y, win ? win->rect.x : -1, win ? win->rect.y : -1].UTF8String);
    }
    #endif

    // ドラッグ・ヘッダータップ状態の判定
    BOOL wasDragging = (ctx->focus != 0 && ctx->mouse_down);
    BOOL wasHeaderTap = self.didTapHeader;

    // 状態をログ
    char beforeMsg[128];
    snprintf(beforeMsg, sizeof(beforeMsg), 
           "タッチ終了前: focus=%u, hover=%u, mouse_down=%d, dragging=%s, header=%s",
           ctx->focus, ctx->hover, ctx->mouse_down, 
           wasDragging ? "YES" : "NO",
           wasHeaderTap ? "YES" : "NO");
    write_log(beforeMsg);
    
    if (wasDragging) {
        // ドラッグ操作の終了処理
        mu_Container *win = mu_get_container(ctx, "Demo Window");
        if (win) {
            char msg[128];
            snprintf(msg, sizeof(msg), 
                   "ドラッグ完了: 最終位置=(%d,%d)", 
                   win->rect.x, win->rect.y);
            write_log(msg);
        }
        
    } else if (wasHeaderTap) {
        // ヘッダークリック処理
        HeaderInfo headerInfo = {0};
        if (ios_detect_header_at_point(ctx, self.lastHeaderTapPosition, &headerInfo)) {
            char msg[128];
            snprintf(msg, sizeof(msg),
                   "ヘッダータップ完了: '%s' (ID=%u)",
                   headerInfo.label, headerInfo.id);
            write_log(msg);
            
            // 必要に応じてヘッダー状態を更新
            if (headerInfo.id != 0) {
                ios_toggle_header_state(ctx, headerInfo.id);
            }
        }
    }
    
    // 状態のクリーンアップ
    ctx->mouse_down = 0;
    ctx->mouse_pressed = 0;
    ios_set_hover(ctx, 0);  // ホバー状態を明示的にクリア
    self.didTapHeader = NO;
    
    // マウス位置の更新
    ctx->mouse_pos = mu_vec2(location.x, location.y);
    
    // イベントをキューに追加
    [self queueEvent:@{
        @"type": @"up",
        @"position": [NSValue valueWithCGPoint:location],
        @"wasDragging": @(wasDragging),
        @"wasHeaderTap": @(wasHeaderTap)
    }];
    
    // 状態遷移をログ出力
    char afterMsg[128];
    snprintf(afterMsg, sizeof(afterMsg),
           "タッチ終了後: focus=%u, hover=%u, mouse_down=%d",
           ctx->focus, ctx->hover, ctx->mouse_down);
    write_log(afterMsg);
    
    // 再描画をトリガー
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
        // 次回のフレーム描画で確実に反映されるようにイベントを直接発行
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
