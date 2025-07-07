# MicroUI Multi-Platform Project

このプロジェクトは、軽量なImmediate Mode GUI ライブラリである **microui** を複数のプラットフォームで動作させるための実装プロジェクトです。Visual Studio 6.0 (VC++6.0)、macOS、iOSに対応し、OpenGL 1.1、OpenGL ES、Metalなどの様々なレンダリングバックエンドをサポートしています。

## プロジェクト構成

### 📁 vs6.0/
Visual Studio 6.0 (VC++6.0) 用の実装とプロジェクトファイル

- **プロジェクトファイル:**
  - `igui.dsp` / `igui.dsw` - ImGUI プロジェクト
  - `opengl_imgui.dsp` / `opengl_imgui.dsw` - OpenGL + ImGUI プロジェクト
  - `opengl.dsp` / `opengl.dsw` - OpenGL プロジェクト
  - `imgui_dx9.dsp` - DirectX 9 プロジェクト

- **ソースファイル:**
  - `imgui.cpp` / `imgui.h` - メインImGUIライブラリ
  - `imgui_demo.cpp` - デモアプリケーション
  - `imgui_draw.cpp` - 描画機能
  - `imgui_tables.cpp` / `imgui_widgets.cpp` - UI コンポーネント
  - `opengl_main.cpp` - OpenGL サンプル

- **バックエンド実装:**
  - `backends/` - 各種レンダリングバックエンドの実装
    - DirectX 9, 10, 11, 12
    - OpenGL 2, 3
    - Vulkan
    - Metal
    - Win32, GLFW, SDL等のプラットフォーム実装

### 📁 apple/
macOS および iOS 用の実装

#### 🍎 macOS プロジェクト

- **`microui_mac_metal/`**
  - macOS用 Metal レンダリングバックエンド実装
  - Xcodeプロジェクト (.xcodeproj)
  - 高性能なGPU描画を活用

- **`microui_mac_opengl1.1/`**
  - macOS用 OpenGL 1.1 実装
  - レガシーOpenGL対応
  - 古いMacシステムとの互換性を確保

#### 📱 iOS プロジェクト

- **`microui_ios_metal/`**
  - iOS用 Metal レンダリングバックエンド
  - モダンなiOSデバイス向け高性能描画
  - タッチインターフェース対応

- **`microui_ios_opengl1.1/`**
  - iOS用 OpenGL ES 1.1 実装
  - 古いiOSデバイスとの互換性
  - OpenGL ES最適化

#### 🔧 共有コンポーネント

- **`src/`**
  - `microui_share.h` - プラットフォーム共通ヘッダー
    - iOS/macOS間でのプラットフォーム別処理の切り替え
    - カスタムラッパー関数のプロトタイプ宣言
    - スライダーとテキストボックスのキャッシュ機能
  - `old.h` - レガシーコード

## 特徴

### 🎯 マルチプラットフォーム対応
- **Windows**: Visual Studio 6.0 (VC++6.0) 完全対応
- **macOS**: Metal および OpenGL 1.1 サポート
- **iOS**: Metal および OpenGL ES 1.1 サポート

### 🎨 マルチレンダラー対応
- **DirectX**: 9, 10, 11, 12
- **OpenGL**: 1.1, 2.0, 3.0+
- **OpenGL ES**: 1.1 (iOS)
- **Metal**: macOS/iOS ネイティブサポート
- **Vulkan**: 次世代低レベルAPI

### 💡 主な機能
- Immediate Mode GUI パラダイム
- 軽量で高速な描画
- プラットフォーム固有の最適化
- タッチデバイス対応（iOS）
- レガシーシステム対応

## ビルド方法

### Windows (Visual Studio 6.0)
1. `vs6.0/` フォルダに移動
2. 対象プロジェクトの `.dsw` ファイルを Visual Studio 6.0 で開く
3. ビルド構成を選択してコンパイル

### macOS/iOS (Xcode)
1. `apple/` フォルダ内の対象プロジェクトに移動
2. `.xcodeproj` ファイルを Xcode で開く
3. ターゲットプラットフォームを選択してビルド

## プラットフォーム固有の実装

### iOS 特有の機能
- タッチインターフェースに最適化されたUI要素
- スライダーとテキストボックスのキャッシュシステム
- プラットフォーム別のマクロ切り替え機能

### レガシー対応
- OpenGL 1.1 サポートによる古いハードウェア対応
- Visual Studio 6.0 対応による古い開発環境サポート

## 技術仕様

- **言語**: C/C++, Objective-C (iOS/macOS)
- **GUI パラダイム**: Immediate Mode GUI
- **対応OS**: Windows, macOS, iOS
- **レンダリング**: DirectX, OpenGL, OpenGL ES, Metal, Vulkan

---

このプロジェクトは、microui の多様なプラットフォームでの活用を可能にし、レガシーシステムから最新のモバイルデバイスまで幅広い環境での GUI アプリケーション開発をサポートします。