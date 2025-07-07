#!/bin/bash

# macOS版のsetupSystemFontメソッドを修正するスクリプト
# 再帰的な呼び出しを削除し、正しくテクスチャをアップロードするように修正

MAIN_FILE="/Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/main.m"
BACKUP_FILE="/Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/main_backup.m"

# バックアップファイルを作成
cp "$MAIN_FILE" "$BACKUP_FILE"
echo "バックアップファイルを作成しました: $BACKUP_FILE"

# スクリプトの内容
sed -i '' 's/\[self setupSystemFont\];/\
    \/\/ atlas\.inlの正しいテクスチャデータをアップロード\
    \/\/ OpenGL 3\.2 Core Profileでは、GL_ALPHAの代わりにGL_REDを使用する必要がある\
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,\
                GL_RED, GL_UNSIGNED_BYTE, atlas_texture);\
    \
    \/\/ テクスチャパラメータの設定\
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); \/\/ シャープなピクセルフォント用\
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);\
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);\
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);/g' "$MAIN_FILE"

echo "setupSystemFontメソッドの再帰呼び出しを修正しました"

# 初期化順序の修正
sed -i '' 's/\/\/ テクスチャパラメータの設定.*$/\
    \/\/ テクスチャパラメータの設定は既にsetupSystemFont内で行われています\
\
    \/\/ カスタムテキスト計測関数を統合レンダラーに登録\
    \/\/ 重要: テクスチャとパラメータ設定の後、レンダラー初期化の前に行う\
    register_text_measurement_functions(text_width, text_height);/g' "$MAIN_FILE"

echo "初期化順序を修正しました"
echo "修正が完了しました"
