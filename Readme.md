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

## Technical Specifications

- **Languages**: C/C++, Objective-C (iOS/macOS)
- **GUI Paradigm**: Immediate Mode GUI
- **Supported OS**: Windows, macOS, iOS
- **Rendering**: DirectX, OpenGL, OpenGL ES, Metal, Vulkan

---

このプロジェクトは、microui の多様なプラットフォームでの活用を可能にし、レガシーシステムから最新のモバイルデバイスまで幅広い環境での GUI アプリケーション開発をサポートします。