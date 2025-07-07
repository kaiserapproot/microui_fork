// filepath: /Users/kaiser/work/framework/proj/xcode/framework/microui_mac_opengl/microui_mac_opengl/renderer.h
#ifndef RENDERER_H
#define RENDERER_H

#include "microui.h"
void r_shutdown(void);
void r_init(void);
void r_draw_rect(mu_Rect rect, mu_Color color);
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color);
void r_draw_icon(int id, mu_Rect rect, mu_Color color);
int r_get_text_width(const char *text, int len);
int r_get_text_height(void);
void r_set_clip_rect(mu_Rect rect);
void r_clear(mu_Color color);
void r_present(void);
static void init_indices(void);
void r_set_viewport(int w, int h);
void r_resize(int width, int height);
// テクスチャIDを取得する関数
unsigned int r_get_texture_id(void);

#endif
