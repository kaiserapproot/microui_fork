#include "renderer_text_helper.h"

// グローバル関数ポインタの定義
TextWidthFunc g_text_width_func = 0;
TextHeightFunc g_text_height_func = 0;

// プラットフォーム固有のテキスト計測関数を登録
void register_text_measurement_functions(TextWidthFunc width_func, TextHeightFunc height_func) {
    g_text_width_func = width_func;
    g_text_height_func = height_func;
}
