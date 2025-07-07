# ios_opengl1.1_microui.m 先頭コメント履歴・内容

---

MicroUI iOS OpenGL ES 1.1実装 - 開発履歴とMetal版との違い
-------------------------------------------------------------

【実装履歴】
2025-06-24: ドキュメント改善 - 実装の違いをより詳細に説明
  - OpenGLとMetalの違いを具体的なコード例とともに解説
  - 座標変換とイベント処理の仕組みを明確化

2025-06-23: ヘッダー状態管理の完全実装
  - Metal版と同様のヘッダーキャッシュとID管理システムを実装
  - トグル状態の永続化と同期を完全対応
  - MicroUI内部状態(treenode_pool)と永続ヘッダー状態の同期を改善

2025-06-22: ウィンドウドラッグとタップ処理の修正
  - Metal版のウィンドウドラッグ処理をポート
  - タッチイベント処理とヘッダーの検出・操作を改善
  - 座標変換系の問題を修正

2025-06-21: OpenGL ES 1.1レンダリングの実装
  - OpenGL ES 1.1 APIを使用したレンダリング実装
  - 投影行列設定でY軸反転(UIKit座標系に合わせる)
  - パフォーマンス最適化とバッファ管理


【OpenGL ES 1.1版とMetal版の主な違い - 詳細解説】

1. グラフィックAPIの基本的な違い
  - OpenGL ES 1.1: 
    - 固定機能パイプラインを使用（シェーダー不要）
    - 一つ一つの描画命令を直接実行（glVertexPointer → glDrawArrays）
    - 例: glDrawArrays(GL_TRIANGLE_FAN, 0, 4) で四角形を描画
    - シンプルだが、バッチ処理などの最適化が限定的

  - Metal: 
    - プログラマブルシェーダーを使用（柔軟なGPUプログラミング）
    - コマンドをバッファにまとめてGPUに一括送信（効率的）
    - 複数の描画をバッチ処理して高速化
    - 複雑だが、パフォーマンスが高い

2. 座標系の違いとその対応方法
  - 座標系の基本:
    - OpenGL標準: 左下原点(0,0)、Y軸上向き
    - UIKit/Metal/MicroUI: 左上原点(0,0)、Y軸下向き

  - OpenGL ES 1.1での対応:
    - 投影行列変換で座標系を反転: glOrthof(0, width, height, 0, -1, 1)
    - シザーテストだけは特殊変換が必要: 
      glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h)
    - タッチ座標を明示的にスケーリング（532-595行目のconvertPointToOpenGL関数）

  - Metal版での対応:
    - MTKViewとシェーダーが座標変換を自動的に処理
    - タッチ座標はほぼそのまま使用可能（変換不要）

3. イベント処理とマウスエミュレーションの違い
  - 共通の課題:
    - MicroUI: マウス操作を前提としたAPI (mouseDown/mouseMove/mouseUp)
    - iOS: タッチベースのイベント (touchesBegan/Moved/Ended)

  - OpenGL ES 1.1実装:
    - タッチ座標をcontentScaleFactorでスケーリング（高解像度対応）
    - convertPointToOpenGL()でUIKit→OpenGL座標変換
    - 具体例: touchesBegan (636-735行目) でタッチ→マウス変換
    - mu_ctx->mouse_pos = mu_vec2(oglPt.x, oglPt.y) で座標設定

  - Metal実装:
    - MTKViewが座標変換を内部処理するためシンプル
    - getTouchLocation()関数でタッチ座標をそのまま取得

4. ヘッダー状態管理の実装方法
  - 問題の本質:
    - タップ時と描画時でヘッダー要素のIDを一致させる必要がある
    - UIKitはホバー状態がないため、擬似的に実装する必要がある

  - 解決アプローチ（両実装共通）:
    - HeaderCache構造体: ヘッダーの位置情報をキャッシュ（レンダリング時に収集）
    - PersistentHeaderState: ヘッダーの開閉状態を永続保存
    - ios_detect_header_at_point: タップ位置のヘッダーを特定
    - ios_toggle_header_state: MicroUI内部と永続状態を同期

  - OpenGL ES 1.1特有の処理:
    - mu_PoolItemのlast_updateフィールドへの直接アクセスによる状態管理
    - 詳細なデバッグログ出力による状態追跡

5. ウィンドウドラッグ処理の違い
  - 共通実装:
    - タイトルバーヒットテスト→ドラッグ状態追跡→位置更新

  - OpenGL ES 1.1実装:
    - windowNameAtTitleBarPoint関数でタイトルバー検出
    - touchesMovedでドラッグ処理（座標変換が必要）
    - CGFloat dx = uiPt.x - dragStartTouchPos.x 等で移動量計算

  - Metal実装:
    - より洗練されたイベントキューイングシステム
    - 自動的なスケーリング処理

6. シザーテスト（描画領域制限）の違い
  - OpenGL ES 1.1:
    - OpenGLの座標系（左下原点）に合わせた特殊な変換が必要
    - glScissor(rect.x, viewHeight - (rect.y + rect.h), rect.w, rect.h)

  - Metal:
    - MTLScissorRect構造体と一貫した座標系
    - 自然な座標マッピングが可能

7. パフォーマンス特性
  - OpenGL ES 1.1:
    - シンプルな実装だが、状態切り替えが多くオーバーヘッドが大きい
    - 小さな描画が多いためGPU効率が低い場合がある

  - Metal:
    - 効率的なバッチ処理と状態管理
    - GPUリソースの明示的管理による最適化
    - より高速だが、コードは複雑

どちらの実装でも、見た目は同じMicroUIインターフェースを提供しますが、
内部の実装は大きく異なります。コードのコメントやログを参照することで、
各部分の詳細な実装の違いを確認できます。
