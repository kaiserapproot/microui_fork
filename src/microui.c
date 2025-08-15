#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"

#define unused(x) ((void) (x))

#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)

#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* incremented after incase `val` uses this value */       \
  } while (0)

#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)


static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

static mu_Style default_style = {
  /* フォント | サイズ | パディング | 間隔 | インデント */
  //NULL, { 68, 10 }, 5, 4, 24,
  /* タイトル高さ | スクロールバーサイズ | サムサイズ */
  //24, 12, 8,
    NULL, { 16, 16 }, 4, 4, 16,
     24, 12, 8,
  {
    { 230, 230, 230, 255 }, /* テキスト色 */
    { 25,  25,  25,  255 }, /* 枠線色 */
    { 50,  50,  50,  255 }, /* ウィンドウ背景色 */
    { 25,  25,  25,  255 }, /* タイトルバー背景色 */
    { 240, 240, 240, 255 }, /* タイトルテキスト色 */
    { 0,   0,   0,   0   }, /* パネル背景色 */
    { 75,  75,  75,  255 }, /* ボタン色 */
    { 95,  95,  95,  255 }, /* ボタンホバー色 */
    { 115, 115, 115, 255 }, /* ボタンフォーカス色 */
    { 30,  30,  30,  255 }, /* ベース色 */
    { 35,  35,  35,  255 }, /* ベースホバー色 */
    { 40,  40,  40,  255 }, /* ベースフォーカス色 */
    { 43,  43,  43,  255 }, /* スクロールバー色 */
    { 30,  30,  30,  255 }  /* スクロールサム色 */
  }
};


/**
 * @brief 2次元ベクトル生成
 * x, y座標からmu_Vec2型を生成します。
 * 使い方: mu_vec2(x, y);
 * @param x X座標
 * @param y Y座標
 * @return mu_Vec2 生成されたベクトル
 */
mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; res.y = y;
  return res;
}


/**
 * @brief 矩形生成
 * x, y, w, hからmu_Rect型を生成します。
 * 使い方: mu_rect(x, y, w, h);
 * @param x X座標
 * @param y Y座標
 * @param w 幅
 * @param h 高さ
 * @return mu_Rect 生成された矩形
 */
mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; res.y = y; res.w = w; res.h = h;
  return res;
}


/**
 * @brief 色生成
 * RGBA値からmu_Color型を生成します。
 * 使い方: mu_color(r, g, b, a);
 * @param r 赤
 * @param g 緑
 * @param b 青
 * @param a アルファ
 * @return mu_Color 生成された色
 */
mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; res.g = g; res.b = b; res.a = a;
  return res;
}


/**
 * @brief 矩形拡張
 * 指定した矩形をnピクセル拡張します。
 * 使い方: expand_rect(rect, n);
 * @param rect 元の矩形
 * @param n 拡張量
 * @return mu_Rect 拡張後の矩形
 */
static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

/**
 * @brief 矩形の交差領域取得
 * 2つの矩形の交差部分を取得します。
 * 使い方: intersect_rects(r1, r2);
 * @param r1 矩形1
 * @param r2 矩形2
 * @return mu_Rect 交差領域の矩形
 */
static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}

/**
 * @brief 点が矩形内か判定
 * 指定した点が矩形内にあるか判定します。
 * 使い方: rect_overlaps_vec2(r, p);
 * @param r 判定する矩形
 * @param p 判定する点（mu_Vec2）
 * @return int 点が矩形内なら1、そうでなければ0
 */
static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

/**
 * @brief フレーム描画
 * 指定した矩形領域にフレーム（枠線）を描画します。
 * 使い方: draw_frame(ctx, rect, colorid);
 * @param ctx MicroUIのコンテキスト
 * @param rect 描画する矩形
 * @param colorid 色ID
 * @return なし
 */
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_TITLEBG) { return; }
  /* draw border */
  if (ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}


/**
 * @brief MicroUIコンテキスト初期化
 * MicroUIの状態を初期化します。
 * 使い方: mu_init(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_init(mu_Context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->draw_frame = draw_frame;
  ctx->_style = default_style;
  ctx->style = &ctx->_style;
}

/**
 * @brief フレーム開始処理
 * UIフレームの処理を開始します。
 * 使い方: mu_begin(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_begin(mu_Context *ctx) {
  expect(ctx->text_width && ctx->text_height);
  ctx->command_list.idx = 0;
  ctx->root_list.idx = 0;
  ctx->scroll_target = NULL;
  ctx->hover_root = ctx->next_hover_root;
  ctx->next_hover_root = NULL;
  ctx->mouse_delta.x = ctx->mouse_pos.x - ctx->last_mouse_pos.x;
  ctx->mouse_delta.y = ctx->mouse_pos.y - ctx->last_mouse_pos.y;
  ctx->frame++;
}


/**
 * @brief zindex比較関数
 * コンテナのzindex値を比較します（qsort用）。
 * 使い方: compare_zindex(a, b);
 * @param a コンテナへのポインタ
 * @param b コンテナへのポインタ
 * @return int a < bなら負、a > bなら正、等しければ0
 */
static int compare_zindex(const void *a, const void *b) {
  return (*(mu_Container**) a)->zindex - (*(mu_Container**) b)->zindex;
}

/**
 * @brief フレーム終了処理
 * UIフレームの処理を終了し、入力・状態をリセットします。
 * 使い方: mu_end(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_end(mu_Context *ctx) {
  int i, n;
  /* スタックのチェック */
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx      == 0);
  expect(ctx->id_stack.idx        == 0);
  expect(ctx->layout_stack.idx    == 0);

  /* スクロール入力の処理 */
  if (ctx->scroll_target) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  /* 今フレームでフォーカスIDが更新されなかった場合はフォーカス解除 */
  if (!ctx->updated_focus) { ctx->focus = 0; }
  ctx->updated_focus = 0;

  /* マウス押下時、hover rootを最前面に持ってくる */
  if (ctx->mouse_pressed && ctx->next_hover_root &&
      ctx->next_hover_root->zindex < ctx->last_zindex &&
      ctx->next_hover_root->zindex >= 0
  ) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  /* 入力状態のリセット */
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;

  /* ルートコンテナをzindexでソート */
  n = ctx->root_list.idx;
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  /* ルートコンテナのジャンプコマンド設定 */
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];
    /* 最初のコンテナなら最初のコマンドをジャンプ先にする。
    ** それ以外は前のコンテナのtailをジャンプ先にする */
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }
    /* 最後のコンテナのtailはコマンドリストの末尾にジャンプ */
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}


/**
 * @brief フォーカス設定
 * 指定IDにフォーカスを設定します。
 * 使い方: mu_set_focus(ctx, id);
 * @param ctx MicroUIのコンテキスト
 * @param id フォーカスを設定するID
 * @return なし
 */
void mu_set_focus(mu_Context *ctx, mu_Id id) {
  ctx->focus = id;
  ctx->updated_focus = 1;
}


/* 32bit fnv-1a hash */
#define HASH_INITIAL 2166136261

static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619;
  }
}


/**
 * @brief ID生成
 * 任意データから一意なIDを生成します。
 * 使い方: mu_get_id(ctx, data, size);
 * @param ctx MicroUIのコンテキスト
 * @param data ID生成用データ
 * @param size データサイズ
 * @return mu_Id 生成されたID
 */
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx;
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size);
  ctx->last_id = res;
  return res;
}


/**
 * @brief IDスタックにIDをプッシュ
 * 現在のIDスタックにIDを追加します。
 * 使い方: mu_push_id(ctx, data, size);
 * @param ctx MicroUIのコンテキスト
 * @param data ID生成用データ
 * @param size データサイズ
 * @return なし
 */
void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}


/**
 * @brief IDスタックからIDをポップ
 * IDスタックのトップを削除します。
 * 使い方: mu_pop_id(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}


/**
 * @brief クリップ矩形をプッシュ
 * クリップ矩形スタックに新しい矩形を追加します。
 * 使い方: mu_push_clip_rect(ctx, rect);
 * @param ctx MicroUIのコンテキスト
 * @param rect クリップ矩形
 * @return なし
 */
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}


/**
 * @brief クリップ矩形をポップ
 * クリップ矩形スタックのトップを削除します。
 * 使い方: mu_pop_clip_rect(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}


/**
 * @brief 現在のクリップ矩形取得
 * クリップ矩形スタックのトップを取得します。
 * 使い方: mu_get_clip_rect(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return mu_Rect 現在のクリップ矩形
 */
mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}


/**
 * @brief クリップ判定
 * 指定矩形がクリップ範囲内か判定します。
 * 使い方: mu_check_clip(ctx, rect);
 * @param ctx MicroUIのコンテキスト
 * @param rect 判定する矩形
 * @return int クリップ状態（MU_CLIP_ALL, MU_CLIP_PART, 0）
 */
int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y   ) { return MU_CLIP_ALL; }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h ) { return 0; }
  return MU_CLIP_PART;
}


static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}


static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}


static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  /* pop container, layout and id */
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}


mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ ctx->container_stack.idx - 1 ];
}


static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  /* try to get existing container from pool */
  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if (idx >= 0) {
    if (ctx->containers[idx].open || ~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }
    return &ctx->containers[idx];
  }
  if (opt & MU_OPT_CLOSED) { return NULL; }
  /* container not found in pool: init new container */
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
  return cnt;
}


/**
 * @brief コンテナ取得
 * 名前を指定してコンテナを取得します。
 * 使い方: mu_get_container(ctx, "コンテナ名");
 * @param ctx MicroUIのコンテキスト
 * @param name コンテナ名
 * @return mu_Container* コンテナへのポインタ
 */
mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}


void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) {
  cnt->zindex = ++ctx->last_zindex;
}


/*============================================================================
** pool
**============================================================================*/

int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for (i = 0; i < len; i++) {
    if (items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1);
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}


int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx);
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1;
}


void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}


/*============================================================================
** input handlers
**============================================================================*/

/**
 * @brief マウス移動入力処理
 * マウス座標を更新します。
 * 使い方: mu_input_mousemove(ctx, x, y);
 * @param ctx MicroUIのコンテキスト
 * @param x マウスX座標
 * @param y マウスY座標
 * @return なし
 */
void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  ctx->mouse_pos = mu_vec2(x, y);
}


/**
 * @brief マウスボタン押下入力処理
 * マウスボタン押下状態を更新します。
 * 使い方: mu_input_mousedown(ctx, x, y, btn);
 * @param ctx MicroUIのコンテキスト
 * @param x マウスX座標
 * @param y マウスY座標
 * @param btn ボタン種別
 * @return なし
 */
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;
}


/**
 * @brief マウスボタン離し入力処理
 * マウスボタン離し状態を更新します。
 * 使い方: mu_input_mouseup(ctx, x, y, btn);
 * @param ctx MicroUIのコンテキスト
 * @param x マウスX座標
 * @param y マウスY座標
 * @param btn ボタン種別
 * @return なし
 */
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
}


/**
 * @brief マウススクロール入力処理
 * スクロール量を更新します。
 * 使い方: mu_input_scroll(ctx, x, y);
 * @param ctx MicroUIのコンテキスト
 * @param x スクロールX量
 * @param y スクロールY量
 * @return なし
 */
void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}


/**
 * @brief キー押下入力処理
 * キー押下状態を更新します。
 * 使い方: mu_input_keydown(ctx, key);
 * @param ctx MicroUIのコンテキスト
 * @param key キーコード
 * @return なし
 */
void mu_input_keydown(mu_Context *ctx, int key) {
  ctx->key_pressed |= key;
  ctx->key_down |= key;
}


/**
 * @brief キー離し入力処理
 * キー離し状態を更新します。
 * 使い方: mu_input_keyup(ctx, key);
 * @param ctx MicroUIのコンテキスト
 * @param key キーコード
 * @return なし
 */
void mu_input_keyup(mu_Context *ctx, int key) {
  ctx->key_down &= ~key;
}


/**
 * @brief テキスト入力処理
 * 入力テキストをバッファに追加します。
 * 使い方: mu_input_text(ctx, "入力文字");
 * @param ctx MicroUIのコンテキスト
 * @param text 入力文字列
 * @return なし
 */
void mu_input_text(mu_Context *ctx, const char *text) {
  int len = strlen(ctx->input_text);
  int size = strlen(text) + 1;
  expect(len + size <= (int) sizeof(ctx->input_text));
  memcpy(ctx->input_text + len, text, size);
}


/*============================================================================
** commandlist
* 
MicroUIのコマンドリストは、UI描画や状態変更のための命令（コマンド）を一時的に蓄積するバッファです。
各UI要素（矩形、テキスト、アイコン、クリップなど）の描画や状態変更は、直接描画せずにコマンドとしてリストに追加されます。
最終的にこのコマンドリストを走査することで、UIの描画や処理が一括して行われます。
コマンドリストは、mu_Command型の配列として管理され、各コマンドは型（type）とサイズ（size）を持ちます。
コマンドの種類には、矩形描画（RECT）、テキスト描画（TEXT）、アイコン描画（ICON）、クリップ（CLIP）、ジャンプ（JUMP）などがあります。
コマンドリストの走査は、mu_next_command関数で行い、JUMPコマンドによる分岐もサポートしています。
**============================================================================*/

/**
 * @brief コマンドをコマンドリストに追加する
 * UI描画や状態変更のためのコマンド（矩形、テキスト、アイコン、クリップなど）を
 * コマンドリストに追加します。コマンドリストはフレームごとに蓄積され、
 * 最終的に走査されて描画処理が行われます。
 * @param ctx MicroUIのコンテキスト
 * @param type コマンド種別（MU_COMMAND_RECTなど）
 * @param size コマンドサイズ（バイト数）
 * @return mu_Command* 追加されたコマンドへのポインタ
 */
mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}

/**
 * @brief コマンドリストの次のコマンドを取得する
 * コマンドリストを走査し、次の有効なコマンド（JUMP以外）を取得します。
 * JUMPコマンドの場合はジャンプ先に移動します。
 * @param ctx MicroUIのコンテキスト
 * @param cmd コマンドへのポインタ（NULLの場合は先頭から）
 * @return int 有効なコマンドがあれば1、末尾なら0
 */
int mu_next_command(mu_Context *ctx, mu_Command **cmd) {
  if (*cmd) {
    *cmd = (mu_Command*) (((char*) *cmd) + (*cmd)->base.size);
  } else {
    *cmd = (mu_Command*) ctx->command_list.items;
  }
  while ((char*) *cmd != ctx->command_list.items + ctx->command_list.idx) {
    if ((*cmd)->type != MU_COMMAND_JUMP) { return 1; }
    *cmd = (*cmd)->jump.dst;
  }
  return 0;
}

/**
 * @brief JUMPコマンドをコマンドリストに追加する（内部関数）
 * JUMPコマンドはコマンドリスト内の分岐やループを実現するために使われます。
 * dstでジャンプ先を指定します。
 * @param ctx MicroUIのコンテキスト
 * @param dst ジャンプ先コマンドへのポインタ
 * @return mu_Command* 追加されたJUMPコマンドへのポインタ
 */
static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}

/**
 * @brief クリップコマンドをコマンドリストに追加する
 * 指定した矩形領域で描画をクリップ（制限）するコマンドを追加します。
 * @param ctx MicroUIのコンテキスト
 * @param rect クリップする矩形領域
 * @return なし
 */
void mu_set_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}

/**
 * @brief 矩形描画コマンドをコマンドリストに追加する
 * 指定した矩形領域と色で矩形を描画するコマンドを追加します。
 * クリップ領域と交差している場合のみ追加されます。
 * @param ctx MicroUIのコンテキスト
 * @param rect 描画する矩形
 * @param color 描画色
 * @return なし
 */
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}

/**
 * @brief 枠線描画コマンドをコマンドリストに追加する
 * 指定した矩形領域の四辺に枠線を描画するコマンドを追加します。
 * @param ctx MicroUIのコンテキスト
 * @param rect 枠線を描画する矩形
 * @param color 枠線色
 * @return なし
 */
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y + rect.h - 1, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x, rect.y, 1, rect.h), color);
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w - 1, rect.y, 1, rect.h), color);
}

/**
 * @brief テキスト描画コマンドをコマンドリストに追加する
 * 指定した位置・フォント・色でテキストを描画するコマンドを追加します。
 * クリップ領域に応じてクリップコマンドも追加されます。
 * @param ctx MicroUIのコンテキスト
 * @param font フォント
 * @param str 描画する文字列
 * @param len 文字列長（-1なら自動判定）
 * @param pos 描画位置
 * @param color 描画色
 * @return なし
 */
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len,
  mu_Vec2 pos, mu_Color color)
{
  mu_Command *cmd;
  mu_Rect rect = mu_rect(
    pos.x, pos.y, ctx->text_width(font, str, len), ctx->text_height(font));
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* コマンド追加 */
  if (len < 0) { len = strlen(str); }
  cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
  memcpy(cmd->text.str, str, len);
  cmd->text.str[len] = '\0';
  cmd->text.pos = pos;
  cmd->text.color = color;
  cmd->text.font = font;
  /* クリップをリセット */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

/**
 * @brief アイコン描画コマンドをコマンドリストに追加する
 * 指定した位置・サイズ・色でアイコンを描画するコマンドを追加します。
 * クリップ領域に応じてクリップコマンドも追加されます。
 * @param ctx MicroUIのコンテキスト
 * @param id アイコンID
 * @param rect 描画する矩形
 * @param color 描画色
 * @return なし
 */
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  /* 矩形がクリップ矩形内に完全に含まれていない場合はクリップコマンドを発行 */
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* アイコンコマンド発行 */
  cmd = mu_push_command(ctx, MU_COMMAND_ICON, sizeof(mu_IconCommand));
  cmd->icon.id = id;
  cmd->icon.rect = rect;
  cmd->icon.color = color;
  /* クリッピングをリセット（必要な場合） */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}


/*============================================================================
** layout
**============================================================================*/

enum { RELATIVE = 1, ABSOLUTE = 2 };

 /**
 * @brief カラムレイアウトの開始
 * 現在のレイアウト位置に新しいカラム（縦並びレイアウト）を開始します。
 * カラム内の要素は縦方向に積み重ねられます。
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}

/**
 * @brief カラムレイアウトの終了
 * 現在のカラムレイアウトを終了し、親レイアウトに最大位置や行情報を反映します。
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* 子レイアウトのposition/next_row/maxが大きい場合は継承する */
  a = get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}


/**
 * @brief レイアウト行の設定
 * 現在のレイアウトに新しい行を設定します。
 * 行内のアイテム数や各アイテムの幅、高さを指定できます。
 * @param ctx MicroUIのコンテキスト
 * @param items 行内のアイテム数
 * @param widths 各アイテムの幅（NULLの場合は自動）
 * @param height 行の高さ
 * @return なし
 */
void mu_layout_row(mu_Context *ctx, int items, const int *widths, int height) {
  mu_Layout *layout = get_layout(ctx);
  if (widths) {
    expect(items <= MU_MAX_WIDTHS);
    memcpy(layout->widths, widths, items * sizeof(widths[0]));
  }
  layout->items = items;
  layout->position = mu_vec2(layout->indent, layout->next_row);
  layout->size.y = height;
  layout->item_index = 0;
}


/**
 * @brief レイアウトの幅を設定
 * 現在のレイアウトの幅（次のアイテムの幅）を指定します。
 * @param ctx MicroUIのコンテキスト
 * @param width 幅
 * @return なし
 */
void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}


/**
 * @brief レイアウトの高さを設定
 * 現在のレイアウトの高さ（次のアイテムの高さ）を指定します。
 * @param ctx MicroUIのコンテキスト
 * @param height 高さ
 * @return なし
 */
void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}


/**
 * @brief 次のアイテムのレイアウト位置を設定
 * 次のアイテムの位置・サイズを絶対または相対で指定します。
 * @param ctx MicroUIのコンテキスト
 * @param r 指定する矩形
 * @param relative 相対指定なら1、絶対指定なら0
 * @return なし
 */
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}


/**
 * @brief 次のアイテムのレイアウト矩形を取得
 * 現在のレイアウト情報から次のアイテムの位置・サイズ（矩形）を計算して返します。
 * 行のアイテム数や幅・高さ、bodyオフセット、インデントなどを考慮します。
 * @param ctx MicroUIのコンテキスト
 * @return mu_Rect 次のアイテムの矩形
 */
mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if (layout->next_type) {
    /* mu_layout_set_nextで設定された矩形を処理 */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }

  } else {
    /* 次の行を処理 */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    /* 位置設定 */
    res.x = layout->position.x;
    res.y = layout->position.y;

    /* サイズ設定 */
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* 位置更新 */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* bodyオフセット適用 */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* 最大位置更新 */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}


/*============================================================================
** controls
**============================================================================*/
/**
 * @brief hover root判定
 * 現在のコンテナスタックにhover rootが含まれているか判定します。
 * 使い方: in_hover_root(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return int 含まれていれば1、そうでなければ0
 */

static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.idx;
  while (i--) {
    if (ctx->container_stack.items[i] == ctx->hover_root) { return 1; }
    /* ルートコンテナのみheadフィールドがセットされる。現在のルートコンテナまで到達したら探索終了 */
    if (ctx->container_stack.items[i]->head) { break; }
  }
  return 0;
}


void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}


/**
 * @brief コントロールテキスト描画
 * コントロール内にテキストを描画します。
 * 使い方: mu_draw_control_text(ctx, str, rect, colorid, opt);
 * @param ctx MicroUIのコンテキスト
 * @param str 描画するテキスト
 * @param rect 描画する矩形
 * @param colorid テキスト色ID
 * @param opt 配置オプション
 * @return なし
 */
void mu_draw_control_text(mu_Context *ctx, const char *str, mu_Rect rect,
  int colorid, int opt)
{
  mu_Vec2 pos;
  mu_Font font = ctx->style->font;
  int tw = ctx->text_width(font, str, -1);
  mu_push_clip_rect(ctx, rect);
  pos.y = rect.y + (rect.h - ctx->text_height(font)) / 2;
  if (opt & MU_OPT_ALIGNCENTER) {
    pos.x = rect.x + (rect.w - tw) / 2;
  } else if (opt & MU_OPT_ALIGNRIGHT) {
    pos.x = rect.x + rect.w - tw - ctx->style->padding;
  } else {
    pos.x = rect.x + ctx->style->padding;
  }
  mu_draw_text(ctx, font, str, -1, pos, ctx->style->colors[colorid]);
  mu_pop_clip_rect(ctx);
}


/**
 * @brief マウスオーバー判定
 * マウスが指定矩形上にあるか判定します。
 * 使い方: mu_mouse_over(ctx, rect);
 * @param ctx MicroUIのコンテキスト
 * @param rect 判定する矩形
 * @return int マウスが矩形上にあれば1、そうでなければ0
 */
int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) &&
    rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
    in_hover_root(ctx);
}


/**
 * @brief コントロール状態更新
 * コントロールのホバーとフォーカス状態を更新します。
 * 使い方: mu_update_control(ctx, id, rect, opt);
 * @param ctx MicroUIのコンテキスト
 * @param id コントロールID
 * @param rect コントロール矩形
 * @param opt コントロールオプション
 * @return なし
 */
void mu_update_control(mu_Context *ctx, mu_Id id, mu_Rect rect, int opt) {
  int mouseover = mu_mouse_over(ctx, rect);

  if (ctx->focus == id) { ctx->updated_focus = 1; }
  if (opt & MU_OPT_NOINTERACT) { return; }
  if (mouseover && !ctx->mouse_down) { ctx->hover = id; }

  if (ctx->focus == id) {
    if (ctx->mouse_pressed && !mouseover) { mu_set_focus(ctx, 0); }
    if (!ctx->mouse_down && ~opt & MU_OPT_HOLDFOCUS) { mu_set_focus(ctx, 0); }
  }

  if (ctx->hover == id) {
    if (ctx->mouse_pressed) {
      mu_set_focus(ctx, id);
    } else if (!mouseover) {
      ctx->hover = 0;
    }
  }
}


/**
 * @brief テキスト描画
 * 複数行のテキストを描画します。自動的に折り返します。
 * 使い方: mu_text(ctx, "テキスト内容");
 * @param ctx MicroUIのコンテキスト
 * @param text 描画するテキスト
 * @return なし
 */
void mu_text(mu_Context *ctx, const char *text) {
  const char *start, *end, *p = text;
  int width = -1;
  mu_Font font = ctx->style->font;
  mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
  mu_layout_begin_column(ctx);
  mu_layout_row(ctx, 1, &width, ctx->text_height(font));
  do {
    mu_Rect r = mu_layout_next(ctx);
    int w = 0;
    start = end = p;
    do {
      const char* word = p;
      while (*p && *p != ' ' && *p != '\n') { p++; }
      w += ctx->text_width(font, word, p - word);
      if (w > r.w && end != start) { break; }
      w += ctx->text_width(font, p, 1);
      end = p++;
    } while (*end && *end != '\n');
    mu_draw_text(ctx, font, start, end - start, mu_vec2(r.x, r.y), color);
    p = end + 1;
  } while (*end);
  mu_layout_end_column(ctx);
}


/**
 * @brief ラベル描画
 * 単一行のラベルテキストを描画します。
 * 使い方: mu_label(ctx, "ラベルテキスト");
 * @param ctx MicroUIのコンテキスト
 * @param text 描画するテキスト
 * @return なし
 */
void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}


/**
 * @brief 拡張ボタン描画・押下判定
 * テキストまたはアイコン付きのボタンを描画し、押下判定を行います。
 * 使い方: mu_button_ex(ctx, "ボタンラベル", アイコンID, オプション);
 * @param ctx MicroUIのコンテキスト
 * @param label ボタンラベル（NULLの場合はアイコンのみ）
 * @param icon アイコンID（0の場合はテキストのみ）
 * @param opt ボタンオプション
 * @return int 押されたらMU_RES_SUBMIT、そうでなければ0
 */
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label))
                   : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  /* クリック処理 */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_SUBMIT;
  }
  /* 描画 */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
  if (label) { mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt); }
  if (icon) { mu_draw_icon(ctx, icon, r, ctx->style->colors[MU_COLOR_TEXT]); }
  return res;
}


/**
 * @brief チェックボックス描画・状態更新
 * チェックボックスを描画し、状態を更新します。
 * 使い方: mu_checkbox(ctx, "ラベル", &状態変数);
 * @param ctx MicroUIのコンテキスト
 * @param label チェックボックスラベル
 * @param state チェック状態を格納する変数へのポインタ
 * @return int 状態が変化したらMU_RES_CHANGE、そうでなければ0
 */
int mu_checkbox(mu_Context *ctx, const char *label, int *state) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, &state, sizeof(state));
  mu_Rect r = mu_layout_next(ctx);
  mu_Rect box = mu_rect(r.x, r.y, r.h, r.h);
  mu_update_control(ctx, id, r, 0);
  /* クリック処理 */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_CHANGE;
    *state = !*state;
  }
  /* 描画 */
  mu_draw_control_frame(ctx, id, box, MU_COLOR_BASE, 0);
  if (*state) {
    mu_draw_icon(ctx, MU_ICON_CHECK, box, ctx->style->colors[MU_COLOR_TEXT]);
  }
  r = mu_rect(r.x + box.w, r.y, r.w - box.w, r.h);
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);
  return res;
}


/**
 * @brief テキストボックス描画・入力処理（基本関数）
 * テキストボックスを描画し、テキスト入力処理を行います。
 * 使い方: mu_textbox_raw(ctx, buffer, バッファサイズ, id, 矩形, オプション);
 * @param ctx MicroUIのコンテキスト
 * @param buf 入力テキスト格納バッファ
 * @param bufsz バッファサイズ
 * @param id テキストボックスID
 * @param r テキストボックス矩形
 * @param opt オプション
 * @return int 入力変化時MU_RES_CHANGE、確定時MU_RES_SUBMIT
 */
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r,
  int opt)
{
  int res = 0;
  mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);

  if (ctx->focus == id) {
    /* テキスト入力処理 */
    int len = strlen(buf);
    int n = mu_min(bufsz - len - 1, (int) strlen(ctx->input_text));
    if (n > 0) {
      memcpy(buf + len, ctx->input_text, n);
      len += n;
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* バックスペース処理 */
    if (ctx->key_pressed & MU_KEY_BACKSPACE && len > 0) {
      /* utf-8 続きバイトをスキップ */
      while ((buf[--len] & 0xc0) == 0x80 && len > 0);
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* リターン処理 */
    if (ctx->key_pressed & MU_KEY_RETURN) {
      mu_set_focus(ctx, 0);
      res |= MU_RES_SUBMIT;
    }
  }

  /* 描画 */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BASE, opt);
  if (ctx->focus == id) {
    mu_Color color = ctx->style->colors[MU_COLOR_TEXT];
    mu_Font font = ctx->style->font;
    int textw = ctx->text_width(font, buf, -1);
    int texth = ctx->text_height(font);
    int ofx = r.w - ctx->style->padding - textw - 1;
    int textx = r.x + mu_min(ofx, ctx->style->padding);
    int texty = r.y + (r.h - texth) / 2;
    mu_push_clip_rect(ctx, r);
    mu_draw_text(ctx, font, buf, -1, mu_vec2(textx, texty), color);
    mu_draw_rect(ctx, mu_rect(textx + textw, texty, 1, texth), color);
    mu_pop_clip_rect(ctx);
  } else {
    mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, opt);
  }

  return res;
}


/**
 * @brief 数値テキスト入力処理（内部関数）
 * 数値入力用テキストボックスを表示・処理します。
 * 使い方: number_textbox(ctx, &value, rect, id);
 * @param ctx MicroUIのコンテキスト
 * @param value 編集する数値へのポインタ
 * @param r テキストボックス矩形
 * @param id コントロールID
 * @return int テキスト編集モードなら1、そうでなければ0
 */
static int number_textbox(mu_Context *ctx, mu_Real *value, mu_Rect r, mu_Id id) {
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->key_down & MU_KEY_SHIFT &&
      ctx->hover == id
  ) {
    ctx->number_edit = id;
    sprintf(ctx->number_edit_buf, MU_REAL_FMT, *value);
  }
  if (ctx->number_edit == id) {
    int res = mu_textbox_raw(
      ctx, ctx->number_edit_buf, sizeof(ctx->number_edit_buf), id, r, 0);
    if (res & MU_RES_SUBMIT || ctx->focus != id) {
      *value = strtod(ctx->number_edit_buf, NULL);
      ctx->number_edit = 0;
    } else {
      return 1;
    }
  }
  return 0;
}


/**
 * @brief テキストボックス描画・入力処理（拡張関数）
 * テキストボックスを描画し、テキスト入力処理を行います。
 * 使い方: mu_textbox_ex(ctx, buffer, バッファサイズ, オプション);
 * @param ctx MicroUIのコンテキスト
 * @param buf 入力テキスト格納バッファ
 * @param bufsz バッファサイズ
 * @param opt オプション
 * @return int 入力変化時MU_RES_CHANGE、確定時MU_RES_SUBMIT
 */
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}


/**
 * @brief スライダー描画・値取得
 * スライダーを描画し、値を取得します。
 * 使い方: mu_slider_ex(ctx, &value, low, high, step, fmt, opt);
 * @param ctx MicroUIのコンテキスト
 * @param value スライダー値（mu_Real*）
 * @param low 最小値
 * @param high 最大値
 * @param step ステップ値
 * @param fmt 表示フォーマット
 * @param opt オプション
 * @return int 値変化時MU_RES_CHANGE
 */
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high, mu_Real step, const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  mu_Rect thumb;
  int x, w, res = 0;
  mu_Real last = *value, v = last;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);

  /* テキスト入力モードの処理 */
  if (number_textbox(ctx, &v, base, id)) { return res; }

  /* 通常モードの処理 */
  mu_update_control(ctx, id, base, opt);

  /* 入力処理 */
  if (ctx->focus == id &&
      (ctx->mouse_down | ctx->mouse_pressed) == MU_MOUSE_LEFT)
  {
    v = low + (ctx->mouse_pos.x - base.x) * (high - low) / base.w;
    if (step) { v = ((__int64)((v + step / 2) / step)) * step; }
  }
  /* 値を制約し保存、resを更新 */
  *value = v = mu_clamp(v, low, high);
  if (last != v) { res |= MU_RES_CHANGE; }

  /* ベースを描画 */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* サムを描画 */
  w = ctx->style->thumb_size;
  x = (v - low) * (base.w - w) / (high - low);
  thumb = mu_rect(base.x + x, base.y, w, base.h);
  mu_draw_control_frame(ctx, id, thumb, MU_COLOR_BUTTON, opt);
  /* テキストを描画 */
  sprintf(buf, fmt, v);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}


/**
 * @brief 数値入力描画・値取得
 * 数値入力コントロールを描画し、値を取得します。
 * 使い方: mu_number_ex(ctx, &value, step, fmt, opt);
 * @param ctx MicroUIのコンテキスト
 * @param value 数値値（mu_Real*）
 * @param step ステップ値
 * @param fmt 表示フォーマット
 * @param opt オプション
 * @return int 値変化時MU_RES_CHANGE
 */
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step, const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  int res = 0;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);
  mu_Real last = *value;

  /* テキスト入力モードの処理 */
  if (number_textbox(ctx, value, base, id)) { return res; }

  /* 通常モードの処理 */
  mu_update_control(ctx, id, base, opt);

  /* 入力処理 */
  if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    *value += ctx->mouse_delta.x * step;
  }
  /* 値が変わったらフラグをセット */
  if (*value != last) { res |= MU_RES_CHANGE; }

  /* ベースを描画 */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* テキストを描画 */
  sprintf(buf, fmt, *value);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}


/**
 * @brief ヘッダー・ツリーノード共通処理（内部関数）
 * ヘッダーとツリーノードの共通部分を処理します。
 * 使い方: header(ctx, "ラベル", is_treenode, オプション);
 * @param ctx MicroUIのコンテキスト
 * @param label 表示テキスト
 * @param istreenode ツリーノードとして描画するかのフラグ
 * @param opt オプション
 * @return int 展開されていればMU_RES_ACTIVE、そうでなければ0
 */
static int header(mu_Context *ctx, const char *label, int istreenode, int opt) {
  mu_Rect r;
  int active, expanded;
  mu_Id id = mu_get_id(ctx, label, strlen(label));
  int idx = mu_pool_get(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  int width = -1;
  mu_layout_row(ctx, 1, &width, 0);

  active = (idx >= 0);
  expanded = (opt & MU_OPT_EXPANDED) ? !active : active;
  r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, 0);

  /* クリック処理 */
  active ^= (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);

  /* プール参照の更新 */
  if (idx >= 0) {
    if (active) { mu_pool_update(ctx, ctx->treenode_pool, idx); }
           else { memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem)); }
  } else if (active) {
    mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  }

  /* 描画 */
  if (istreenode) {
    if (ctx->hover == id) { ctx->draw_frame(ctx, r, MU_COLOR_BUTTONHOVER); }
  } else {
    mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, 0);
  }
  mu_draw_icon(
    ctx, expanded ? MU_ICON_EXPANDED : MU_ICON_COLLAPSED,
    mu_rect(r.x, r.y, r.h, r.h), ctx->style->colors[MU_COLOR_TEXT]);
  r.x += r.h - ctx->style->padding;
  r.w -= r.h - ctx->style->padding;
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);

  return expanded ? MU_RES_ACTIVE : 0;
}


/**
 * @brief ヘッダー描画・状態管理
 * 折りたたみ可能なヘッダーを描画します。
 * 使い方: mu_header_ex(ctx, "ヘッダータイトル", オプション);
 * @param ctx MicroUIのコンテキスト
 * @param label ヘッダーテキスト
 * @param opt オプション
 * @return int 展開されていればMU_RES_ACTIVE、そうでなければ0
 */
int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, 0, opt);
}


/**
 * @brief ツリーノード開始
 * 折りたたみ可能なツリーノードを開始します。
 * 使い方: mu_begin_treenode_ex(ctx, "ノード名", オプション);
 * @param ctx MicroUIのコンテキスト
 * @param label ノードラベル
 * @param opt オプション
 * @return int 展開されていればMU_RES_ACTIVE、そうでなければ0
 */
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if (res & MU_RES_ACTIVE) {
    get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->last_id);
  }
  return res;
}


/**
 * @brief ツリーノード終了
 * mu_begin_treenodeで開始したツリーノードを終了します。
 * 使い方: mu_end_treenode(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_end_treenode(mu_Context *ctx) {
  get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}


#define scrollbar(ctx, cnt, b, cs, x, y, w, h)                              \
  do {                                                                      \
    /* コンテンツサイズがボディより大きい場合のみスクロールバーを追加 */            \
    int maxscroll = cs.y - b->h;                                            \
                                                                            \
    if (maxscroll > 0 && b->h > 0) {                                        \
      mu_Rect base, thumb;                                                  \
      mu_Id id = mu_get_id(ctx, "!scrollbar" #y, 11);                       \
                                                                            \
      /* サイズ・位置の取得 */                                               \
      base = *b;                                                            \
      base.x = b->x + b->w;                                                 \
      base.w = ctx->style->scrollbar_size;                                  \
                                                                            \
      /* 入力処理 */                                                        \
      mu_update_control(ctx, id, base, 0);                                  \
      if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {           \
        cnt->scroll.y += ctx->mouse_delta.y * cs.y / base.h;                \
      }                                                                     \
      /* スクロール量を制限 */                                              \
      cnt->scroll.y = mu_clamp(cnt->scroll.y, 0, maxscroll);                \
                                                                            \
      /* ベースとサムの描画 */                                               \
      ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);                      \
      thumb = base;                                                         \
      thumb.h = mu_max(ctx->style->thumb_size, base.h * b->h / cs.y);       \
      thumb.y += cnt->scroll.y * (base.h - thumb.h) / maxscroll;            \
      ctx->draw_frame(ctx, thumb, MU_COLOR_SCROLLTHUMB);                    \
                                                                            \
      /* マウスが上にある場合はスクロールターゲットに設定（ホイールでスクロール可能） */ \
      if (mu_mouse_over(ctx, *b)) { ctx->scroll_target = cnt; }             \
    } else {                                                                \
      cnt->scroll.y = 0;                                                    \
    }                                                                       \
  } while (0)


/**
 * @brief スクロールバー処理（内部関数）
 * コンテナにスクロールバーを追加します。
 * 使い方: scrollbars(ctx, container, &body);
 * @param ctx MicroUIのコンテキスト
 * @param cnt 対象コンテナ
 * @param body ボディ矩形（更新される）
 * @return なし
 */
static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding * 2;
  cs.y += ctx->style->padding * 2;
  mu_push_clip_rect(ctx, *body);
  /* スクロールバーのためにボディをリサイズ */
  if (cs.y > cnt->body.h) { body->w -= sz; }
  if (cs.x > cnt->body.w) { body->h -= sz; }
  /* 縦または横のスクロールバーを作成するためにほぼ同じコードが使用される
  ** `x|y` `w|h`への参照だけが切り替わる */
  scrollbar(ctx, cnt, body, cs, x, y, w, h);
  scrollbar(ctx, cnt, body, cs, y, x, h, w);
  mu_pop_clip_rect(ctx);
}


/**
 * @brief コンテナボディ設定（内部関数）
 * コンテナのボディ領域を設定し、レイアウトをプッシュします。
 * 使い方: push_container_body(ctx, container, body, opt);
 * @param ctx MicroUIのコンテキスト
 * @param cnt 対象コンテナ
 * @param body ボディ矩形
 * @param opt オプション
 * @return なし
 */
static void push_container_body(
  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt
) {
  if (~opt & MU_OPT_NOSCROLL) { scrollbars(ctx, cnt, &body); }
  push_layout(ctx, expand_rect(body, -ctx->style->padding), cnt->scroll);
  cnt->body = body;
}


static void begin_root_container(mu_Context *ctx, mu_Container *cnt) {
  push(ctx->container_stack, cnt);
  /* ルートリストにコンテナを追加し、headコマンドをプッシュ */
  push(ctx->root_list, cnt);
  cnt->head = push_jump(ctx, NULL);
  /* マウスがこのコンテナ上にあり、現在のhover rootよりzindexが高い場合はhover rootに設定 */
  if (rect_overlaps_vec2(cnt->rect, ctx->mouse_pos) &&
      (!ctx->next_hover_root || cnt->zindex > ctx->next_hover_root->zindex)
  ) {
    ctx->next_hover_root = cnt;
  }
  /* ルートコンテナのbegin/endブロック内でさらにルートコンテナが作られた場合、
  ** 内側のルートコンテナが外側にクリップされないようにクリッピングをリセット */
  push(ctx->clip_stack, unclipped_rect);
}

/**
 * @brief ルートコンテナ終了（内部関数）
 * ルートコンテナの処理を終了します。
 * 使い方: end_root_container(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
static void end_root_container(mu_Context *ctx) {
  /* tailジャンプコマンドをプッシュし、headジャンプコマンドを設定。
  ** 最終的な初期化はmu_end()で行う */
  mu_Container *cnt = mu_get_current_container(ctx);
  cnt->tail = push_jump(ctx, NULL);
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;
  /* ベースクリップ矩形とコンテナをポップ */
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
/**
 * @brief ウィンドウ開始関数
 * MicroUIのウィンドウを開始し、描画・管理を行います。
 * 使い方: mu_begin_window_ex(ctx, "タイトル", rect, オプション);
 * @param ctx MicroUIのコンテキスト
 * @param title ウィンドウタイトル文字列
 * @param rect ウィンドウの位置とサイズ
 * @param opt ウィンドウの動作オプション
 * @return int ウィンドウがアクティブならMU_RES_ACTIVE、非アクティブなら0
 */
int mu_begin_window_ex(mu_Context* ctx, const char* title, mu_Rect rect, int opt)
{
    mu_Rect body;
    mu_Id id = mu_get_id(ctx, title, strlen(title));  // ウィンドウタイトルからIDを生成
    mu_Container* cnt = get_container(ctx, id, opt);  // コンテナを取得または初期化
    if (!cnt || !cnt->open) { return 0; }  // コンテナが無効または閉じている場合は終了
    push(ctx->id_stack, id);  // コンテナIDをIDスタックにプッシュ

    if (cnt->rect.w == 0) { cnt->rect = rect; }  // 初回のみ矩形を設定
    begin_root_container(ctx, cnt);  // ルートコンテナの開始
    rect = body = cnt->rect;  // コンテナの矩形を設定

    /* フレーム描画 */
    if (~opt & MU_OPT_NOFRAME)
    {
        ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);  // ウィンドウ背景描画
    }

    /* タイトルバー描画 */
    if (~opt & MU_OPT_NOTITLE)
    {
        mu_Rect tr = rect;
        tr.h = ctx->style->title_height;  // タイトルバー高さ設定
        ctx->draw_frame(ctx, tr, MU_COLOR_TITLEBG);  // タイトルバー背景描画

        /* タイトルテキスト描画 */
        if (~opt & MU_OPT_NOTITLE)
        {
            mu_Id id = mu_get_id(ctx, "!title", 6);  // タイトルID生成
            mu_update_control(ctx, id, tr, opt);  // タイトルバーのコントロール更新
            mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt);  // タイトルテキスト描画
            if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
            {
                cnt->rect.x += ctx->mouse_delta.x;  // ドラッグでウィンドウ移動
                cnt->rect.y += ctx->mouse_delta.y;
            }
            body.y += tr.h;  // タイトルバー分だけボディのY座標をずらす
            body.h -= tr.h;  // タイトルバー分だけボディの高さを減らす
        }

        /* クローズボタン描画 */
        if (~opt & MU_OPT_NOCLOSE)
        {
            mu_Id id = mu_get_id(ctx, "!close", 6);  // クローズボタンID生成
            mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);  // クローズボタン矩形
            tr.w -= r.w;  // タイトルバー幅からクローズボタン分を減らす
            mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);  // クローズアイコン描画
            mu_update_control(ctx, id, r, opt);  // クローズボタンのコントロール更新
            if (ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus)
            {
                cnt->open = 0;  // クローズボタン押下でウィンドウを閉じる
            }
        }
    }

    push_container_body(ctx, cnt, body, opt);  // ボディ部分のレイアウト設定

    /* リサイズハンドル描画 */
    if (~opt & MU_OPT_NORESIZE)
    {
        int sz = ctx->style->title_height;  // リサイズハンドルサイズ
        mu_Id id = mu_get_id(ctx, "!resize", 7);  // リサイズハンドルID生成
        mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);  // リサイズハンドル矩形
        mu_update_control(ctx, id, r, opt);  // リサイズハンドルのコントロール更新
        if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
        {
            cnt->rect.w = mu_max(96, cnt->rect.w + ctx->mouse_delta.x);  // ウィンドウ幅リサイズ
            cnt->rect.h = mu_max(64, cnt->rect.h + ctx->mouse_delta.y);  // ウィンドウ高さリサイズ
        }
    }

    /* オートサイズ処理 */
    if (opt & MU_OPT_AUTOSIZE)
    {
        mu_Rect r = get_layout(ctx)->body;  // レイアウトボディ取得
        cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);  // コンテンツサイズに合わせて幅調整
        cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);  // コンテンツサイズに合わせて高さ調整
    }

    /* ポップアップウィンドウのクローズ処理 */
    if (opt & MU_OPT_POPUP && ctx->mouse_pressed && ctx->hover_root != cnt)
    {
        cnt->open = 0;  // 他の場所クリックでポップアップを閉じる
    }

    mu_push_clip_rect(ctx, cnt->body);  // ボディ部分のクリッピング設定
    return MU_RES_ACTIVE;  // ウィンドウがアクティブならMU_RES_ACTIVEを返す
}


/**
 * @brief ウィンドウ終了関数
 * mu_begin_window_exで開始したウィンドウの処理を終了します。
 * 使い方: mu_end_window(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}


/**
 * @brief ポップアップウィンドウを開く
 * 指定した名前のポップアップウィンドウをマウス位置に表示します。
 * 使い方: mu_open_popup(ctx, "ポップアップ名");
 * @param ctx MicroUIのコンテキスト
 * @param name ポップアップウィンドウ名
 * @return なし
 */
void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
}


/**
 * @brief ポップアップウィンドウ開始関数
 * ポップアップウィンドウの描画・管理を開始します。
 * 使い方: mu_begin_popup(ctx, "ポップアップ名");
 * @param ctx MicroUIのコンテキスト
 * @param name ポップアップウィンドウ名
 * @return int ポップアップがアクティブならMU_RES_ACTIVE、非アクティブなら0
 */
int mu_begin_popup(mu_Context *ctx, const char *name) {
  int opt = MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
            MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED;
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}


/**
 * @brief ポップアップウィンドウ終了関数
 * mu_begin_popupで開始したポップアップウィンドウの処理を終了します。
 * 使い方: mu_end_popup(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_end_popup(mu_Context *ctx) {
  mu_end_window(ctx);
}


/**
 * @brief パネル開始関数
 * パネルの描画・管理を開始します。
 * 使い方: mu_begin_panel_ex(ctx, "パネル名", オプション);
 * @param ctx MicroUIのコンテキスト
 * @param name パネル名
 * @param opt パネルの動作オプション
 * @return なし
 */
void mu_begin_panel_ex(mu_Context *ctx, const char *name, int opt) {
  mu_Container *cnt;
  mu_push_id(ctx, name, strlen(name));
  cnt = get_container(ctx, ctx->last_id, opt);
  cnt->rect = mu_layout_next(ctx);
  if (~opt & MU_OPT_NOFRAME) {
    ctx->draw_frame(ctx, cnt->rect, MU_COLOR_PANELBG);
  }
  push(ctx->container_stack, cnt);
  push_container_body(ctx, cnt, cnt->rect, opt);
  mu_push_clip_rect(ctx, cnt->body);
}


/**
 * @brief パネル終了関数
 * mu_begin_panel_exで開始したパネルの処理を終了します。
 * 使い方: mu_end_panel(ctx);
 * @param ctx MicroUIのコンテキスト
 * @return なし
 */
void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
