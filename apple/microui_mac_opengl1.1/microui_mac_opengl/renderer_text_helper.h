#ifndef RENDERER_TEXT_HELPER_H
#define RENDERER_TEXT_HELPER_H

#include "microui.h"

// テキスト計測関数のタイプ定義
typedef int (*TextWidthFunc)(mu_Font font, const char* text, int len);
typedef int (*TextHeightFunc)(mu_Font font);

// グローバル関数ポインタ - レンダラーからアクセス可能
extern TextWidthFunc g_text_width_func;
extern TextHeightFunc g_text_height_func;

// プラットフォーム固有のテキスト計測関数を登録
void register_text_measurement_functions(TextWidthFunc width_func, TextHeightFunc height_func);

#endif // RENDERER_TEXT_HELPER_H
