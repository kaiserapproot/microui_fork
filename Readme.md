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

---

# MicroUI Multi-Platform Project (English)

This project is an implementation project for running **microui**, a lightweight Immediate Mode GUI library, on multiple platforms. It supports Visual Studio 6.0 (VC++6.0), macOS, and iOS, with various rendering backends including OpenGL 1.1, OpenGL ES, and Metal.

## Project Structure

### 📁 vs6.0/
Implementation and project files for Visual Studio 6.0 (VC++6.0)

- **Project Files:**
  - `igui.dsp` / `igui.dsw` - ImGUI project
  - `opengl_imgui.dsp` / `opengl_imgui.dsw` - OpenGL + ImGUI project
  - `opengl.dsp` / `opengl.dsw` - OpenGL project
  - `imgui_dx9.dsp` - DirectX 9 project

- **Source Files:**
  - `imgui.cpp` / `imgui.h` - Main ImGUI library
  - `imgui_demo.cpp` - Demo application
  - `imgui_draw.cpp` - Drawing functionality
  - `imgui_tables.cpp` / `imgui_widgets.cpp` - UI components
  - `opengl_main.cpp` - OpenGL sample

- **Backend Implementations:**
  - `backends/` - Various rendering backend implementations
    - DirectX 9, 10, 11, 12
    - OpenGL 2, 3
    - Vulkan
    - Metal
    - Platform implementations for Win32, GLFW, SDL, etc.

### 📁 apple/
Implementation for macOS and iOS

#### 🍎 macOS Projects

- **`microui_mac_metal/`**
  - Metal rendering backend implementation for macOS
  - Xcode project (.xcodeproj)
  - Utilizes high-performance GPU rendering

- **`microui_mac_opengl1.1/`**
  - OpenGL 1.1 implementation for macOS
  - Legacy OpenGL support
  - Ensures compatibility with older Mac systems

#### 📱 iOS Projects

- **`microui_ios_metal/`**
  - Metal rendering backend for iOS
  - High-performance rendering for modern iOS devices
  - Touch interface support

- **`microui_ios_opengl1.1/`**
  - OpenGL ES 1.1 implementation for iOS
  - Compatibility with older iOS devices
  - OpenGL ES optimization

#### 🔧 Shared Components

- **`src/`**
  - `microui_share.h` - Platform-common header
    - Platform-specific processing switching between iOS/macOS
    - Custom wrapper function prototype declarations
    - Slider and textbox caching functionality
  - `old.h` - Legacy code

## Features

### 🎯 Multi-Platform Support
- **Windows**: Full Visual Studio 6.0 (VC++6.0) support
- **macOS**: Metal and OpenGL 1.1 support
- **iOS**: Metal and OpenGL ES 1.1 support

### 🎨 Multi-Renderer Support
- **DirectX**: 9, 10, 11, 12
- **OpenGL**: 1.1, 2.0, 3.0+
- **OpenGL ES**: 1.1 (iOS)
- **Metal**: Native macOS/iOS support
- **Vulkan**: Next-generation low-level API

### 💡 Key Features
- Immediate Mode GUI paradigm
- Lightweight and fast rendering
- Platform-specific optimizations
- Touch device support (iOS)
- Legacy system support

## Build Instructions

### Windows (Visual Studio 6.0)
1. Navigate to the `vs6.0/` folder
2. Open the target project's `.dsw` file with Visual Studio 6.0
3. Select build configuration and compile

### macOS/iOS (Xcode)
1. Navigate to the target project in the `apple/` folder
2. Open the `.xcodeproj` file with Xcode
3. Select target platform and build

## Platform-Specific Implementations

### iOS-Specific Features
- UI elements optimized for touch interfaces
- Slider and textbox caching system
- Platform-specific macro switching functionality

### Legacy Support
- Support for older hardware through OpenGL 1.1
- Support for older development environments through Visual Studio 6.0

## 技術仕様

- **言語**: C/C++, Objective-C (iOS/macOS)
- **GUI パラダイム**: Immediate Mode GUI
- **対応OS**: Windows, macOS, iOS
- **レンダリング**: DirectX, OpenGL, OpenGL ES, Metal, Vulkan

---

このプロジェクトは、microui の多様なプラットフォームでの活用を可能にし、レガシーシステムから最新のモバイルデバイスまで幅広い環境での GUI アプリケーション開発をサポートします。

---

## 関連ドキュメント・詳細解説

### [microui_ios_metal_main.m_history.md](apple/microui_ios_metal/microui_ios_metal/microui_ios_metal_main.m_history.md)
- **内容:**
  - iOS Metal実装の修正履歴と、macOS/iOS間のイベント・描画処理の違い、タッチ・ドラッグ・ホバー・Metal描画パイプラインの詳細な実装解説。
  - iOS独自のタッチイベント管理やMetalバッファ管理、MicroUIコマンド処理のiOS適応方法など、実装の工夫や最適化ポイントを詳述。

### [architecture.md](apple/microui_ios_opengl1.1/microui_ios_opengl1.1/architecture.md)
- **内容:**
  - MicroUIのmacOS/iOS向けOpenGL・Metal実装のアーキテクチャ比較ドキュメント。
  - 各プラットフォーム・APIごとのレンダリングパイプライン、イベント処理、テクスチャ・フォント処理、バッチ処理、ディスプレイ同期、メモリ管理の違いを詳細に比較。
  - 実装スタイルや責務分離、パフォーマンス特性の違いも表形式で整理。

### [history.md](apple/microui_ios_opengl1.1/microui_ios_opengl1.1/history.md)
- **内容:**
  - iOS OpenGL ES 1.1実装の開発履歴とMetal版との主な違いを詳細に解説。
  - グラフィックAPIの違い、座標系変換、イベント処理、ヘッダー状態管理、ウィンドウドラッグ、シザーテスト、パフォーマンス特性など、両実装の内部構造の違いを具体的なコード例とともに説明。

### [microui_ios_opengl1.1/Readme.md](apple/microui_ios_opengl1.1/microui_ios_opengl1.1/Readme.md)
- **内容:**
  - iOS OpenGL ES 1.1版の個別Readme。ビルド方法、依存ライブラリ、iOS固有の実装ポイント、Metal版との使い分けや注意点など、OpenGL ES 1.1実装の利用者向けガイド。

---

これらのドキュメントを参照することで、各プラットフォーム・バックエンドごとの実装方針や技術的な違い、最適化ポイント、開発履歴をより深く理解できます。

---

## 今後の対応・開発方針

- **iOS・macOS間の共通化とリファクタリング**
  - プラットフォームごとに分かれているUI処理・イベント処理・描画処理の共通化を推進
  - 共通ヘッダーや共通ユーティリティの拡充
  - Metal/OpenGL両対応のための抽象化レイヤーの導入検討

- **ソースコードの共通化**
  - iOS/macOSで重複しているロジック（ウィンドウ管理、ウィジェット描画、コマンド処理等）の統合
  - プラットフォーム依存部分を明確に分離し、共通部分の再利用性を向上

- **タッチイベント処理のリファクタリング**
  - iOS/macOSで異なるイベントモデル（タッチ/マウス）を抽象化し、同一インターフェースで扱えるように設計
  - タッチ・マウスイベントの統一的なハンドリング、ヒットテスト・ホバー・ドラッグ処理の共通化
  - テスト容易性・保守性を高めるためのイベント処理構造の見直し

これらの対応により、今後の機能追加やバグ修正、プラットフォーム拡張がより効率的かつ堅牢に行えるようになります。