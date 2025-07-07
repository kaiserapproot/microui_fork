

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"
#if defined(_MSC_VER)
  // Microsoft Visual C++
  typedef __int64 mu_int64;
#else
  // GCC, Clang など
  typedef long long mu_int64;
#endif
// unusedマクロ: 引数を使用しない場合に警告を抑制するためのマクロ
#define unused(x) ((void) (x))

// expectマクロ: 条件が偽の場合にエラーメッセージを出力し、プログラムを終了するアサーションマクロ
#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)

// pushマクロ: スタックに値をプッシュする。
// スタックのサイズ制限をチェックし、値をスタックに追加した後、インデックスをインクリメントします。
// 使用例:
//   push(ctx->id_stack, id);  // id_stackにidを追加
#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* `val` が現在のインデックスを使用する可能性があるため、インクリメントは後で実行 */ \
  } while (0)

// popマクロ: スタックから値をポップする。
// スタックが空でないことを確認し、インデックスをデクリメントします。
// 使用例:
//   pop(ctx->id_stack);  // id_stackから最後の値を削除
#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)

// unclipped_rect: クリッピングされていない矩形を表す定数。
// 非常に大きな矩形を使用して、クリッピングを無効化する用途で使用されます。
static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

// default_style: デフォルトのスタイル設定を保持する構造体。
// フォント、サイズ、パディング、スペーシング、色などのUI要素の外観を定義します。
static mu_Style default_style = {
  /* font | size | padding | spacing | indent */
  NULL, { 68, 10 }, 5, 4, 24,
  /* title_height | scrollbar_size | thumb_size */
  24, 12, 8,
  {
    { 230, 230, 230, 255 }, /* MU_COLOR_TEXT: テキストの色 */
    { 25,  25,  25,  255 }, /* MU_COLOR_BORDER: 境界線の色 */
    { 50,  50,  50,  255 }, /* MU_COLOR_WINDOWBG: ウィンドウ背景の色 */
    { 25,  25,  25,  255 }, /* MU_COLOR_TITLEBG: タイトルバー背景の色 */
    { 240, 240, 240, 255 }, /* MU_COLOR_TITLETEXT: タイトルテキストの色 */
    { 0,   0,   0,   0   }, /* MU_COLOR_PANELBG: パネル背景の色 */
    { 75,  75,  75,  255 }, /* MU_COLOR_BUTTON: ボタンの色 */
    { 95,  95,  95,  255 }, /* MU_COLOR_BUTTONHOVER: ボタンホバー時の色 */
    { 115, 115, 115, 255 }, /* MU_COLOR_BUTTONFOCUS: ボタンフォーカス時の色 */
    { 30,  30,  30,  255 }, /* MU_COLOR_BASE: 基本背景の色 */
    { 35,  35,  35,  255 }, /* MU_COLOR_BASEHOVER: 基本背景ホバー時の色 */
    { 40,  40,  40,  255 }, /* MU_COLOR_BASEFOCUS: 基本背景フォーカス時の色 */
    { 43,  43,  43,  255 }, /* MU_COLOR_SCROLLBASE: スクロールバーのベース色 */
    { 30,  30,  30,  255 }  /* MU_COLOR_SCROLLTHUMB: スクロールバーのサム色 */
  }
};

// mu_vec2: 2次元ベクトルを生成する関数
// 引数: 
//   x - ベクトルのx座標
//   y - ベクトルのy座標
// 戻り値: 
//   指定されたx, y座標を持つmu_Vec2構造体
mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; 
  res.y = y;
  return res;
}

// mu_rect: 矩形を生成する関数
// 引数: 
//   x - 矩形の左上のx座標
//   y - 矩形の左上のy座標
//   w - 矩形の幅
//   h - 矩形の高さ
// 戻り値: 
//   指定された位置とサイズを持つmu_Rect構造体
mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; 
  res.y = y; 
  res.w = w; 
  res.h = h;
  return res;
}

// mu_color: 色を生成する関数
// 引数: 
//   r - 赤の値 (0-255)
//   g - 緑の値 (0-255)
//   b - 青の値 (0-255)
//   a - アルファ値 (0-255)
// 戻り値: 
//   指定されたRGBA値を持つmu_Color構造体
mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; 
  res.g = g; 
  res.b = b; 
  res.a = a;
  return res;
}

// expand_rect: 矩形を拡張する関数
// 引数: 
//   rect - 拡張対象の矩形
//   n - 拡張する幅 (上下左右にnピクセルずつ拡張)
// 戻り値: 
//   拡張された矩形
static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

// intersect_rects: 2つの矩形の交差部分を計算する関数
// 引数: 
//   r1 - 最初の矩形
//   r2 - 2番目の矩形
// 戻り値: 
//   交差部分の矩形 (交差しない場合は幅と高さが0の矩形)
static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}

// rect_overlaps_vec2: 矩形が指定された点を含むかを判定する関数
// 引数: 
//   r - 判定対象の矩形
//   p - 判定対象の点 (mu_Vec2構造体)
// 戻り値: 
//   点が矩形内にある場合は1、そうでない場合は0
static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

// draw_frame: フレームを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - 描画する矩形
//   colorid - 使用する色のID
// 処理内容:
//   指定された矩形を指定された色で描画します。
//   特定の色IDの場合は境界線を描画しません。
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_TITLEBG) { return; }
  /* 境界線を描画 */
  if (ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}

// mu_init: MicroUIのコンテキストを初期化する関数
// 引数:
//   ctx - 初期化するMicroUIのコンテキスト
// 処理内容:
//   コンテキストのメモリをクリアし、デフォルトスタイルや描画関数を設定します。
void mu_init(mu_Context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->draw_frame = draw_frame;
  ctx->_style = default_style;
  ctx->style = &ctx->_style;
}

// mu_begin: フレームの開始処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   各種スタックやリストを初期化し、フレームの準備を行います。
//   必須のテキスト幅・高さ関数が設定されていることを確認します。
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

// compare_zindex: コンテナのZインデックスを比較する関数
// 引数:
//   a - 最初のコンテナへのポインタ
//   b - 2番目のコンテナへのポインタ
// 戻り値:
//   Zインデックスの差分
// 処理内容:
//   2つのコンテナのZインデックスを比較し、その差を返します。
static int compare_zindex(const void *a, const void *b) {
  return (*(mu_Container**) a)->zindex - (*(mu_Container**) b)->zindex;
}

// mu_end: フレームの終了処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   スタックの整合性を確認し、スクロールやフォーカスの状態を更新します。
//   また、ルートコンテナをZインデックス順にソートし、描画コマンドを最適化します。
void mu_end(mu_Context *ctx) {
  int i, n;

  // 各種スタックが空であることを確認
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx == 0);
  expect(ctx->id_stack.idx == 0);
  expect(ctx->layout_stack.idx == 0);

  // スクロール入力を処理
  if (ctx->scroll_target) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  // フォーカスが更新されていない場合、フォーカスを解除
  if (!ctx->updated_focus) {
    ctx->focus = 0;
  }
  ctx->updated_focus = 0;

  // マウスが押された場合、ホバー中のルートコンテナを最前面に移動
  if (ctx->mouse_pressed && ctx->next_hover_root &&
      ctx->next_hover_root->zindex < ctx->last_zindex &&
      ctx->next_hover_root->zindex >= 0) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  // 入力状態をリセット
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;

  // ルートコンテナをZインデックス順にソート
  n = ctx->root_list.idx;
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  // 各ルートコンテナの描画コマンドを設定
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];

    // 最初のコンテナの場合、最初のコマンドを設定
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      // 前のコンテナの末尾を現在のコンテナの先頭にジャンプさせる
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }

    // 最後のコンテナの場合、末尾をコマンドリストの終端にジャンプさせる
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}

// HASH_INITIAL: FNV-1aハッシュの初期値
// 32bit FNV-1aハッシュアルゴリズムで使用される初期値です。
#define HASH_INITIAL 2166136261

// hash: 32bit FNV-1aハッシュを計算する関数
// 引数:
//   hash - ハッシュ値を格納するポインタ
//   data - ハッシュ化するデータ
//   size - データのサイズ (バイト単位)
// 処理内容:
//   FNV-1aハッシュアルゴリズムを使用して、指定されたデータのハッシュ値を計算します。
//   ハッシュ値は、データの一意性を識別するために使用されます。
static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619; // FNV-1aアルゴリズムの計算
  }
}

// mu_get_id: データから一意のIDを生成する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   data - ハッシュ化するデータ
//   size - データのサイズ (バイト単位)
// 戻り値:
//   データに基づいて計算された一意のmu_Id
// 処理内容:
//   スタックの最後のIDを基に、指定されたデータのハッシュ値を計算します。
//   これにより、データに基づく一意のIDが生成されます。
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx;
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size); // ハッシュ値を計算
  ctx->last_id = res; // 計算結果を保存
  return res;
}

// mu_set_focus: 指定されたIDにフォーカスを設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   id - フォーカスを設定する対象のID
// 処理内容:
//   指定されたIDを現在のフォーカスとして設定し、更新フラグを立てます。
void mu_set_focus(mu_Context *ctx, mu_Id id) {
  ctx->focus = id; // フォーカスを設定
  ctx->updated_focus = 1; // 更新フラグを立てる
}

/*
** 32bit FNV-1aハッシュの使用方法と目的:
** 
** FNV-1aハッシュは、軽量で高速なハッシュアルゴリズムです。
** MicroUIでは、データ（例: ウィジェット名やその他の識別情報）を一意に識別するために使用されます。
** 
** 具体的には、`mu_get_id`関数でデータのハッシュ値を計算し、
** そのハッシュ値をIDとして使用します。このIDは、ウィジェットやコンテナなどの
** UI要素を一意に識別するために利用されます。
** 
** ハッシュを使用することで、以下の利点があります:
** 1. データの一意性を簡単に保証できる。
** 2. データの比較を高速化できる（文字列比較より効率的）。
** 3. メモリ効率が良い（ハッシュ値は固定サイズの整数）。
** 
** 例: 
** - ウィジェット名 "button1" をハッシュ化してIDを生成。
** - このIDを使用して、ウィジェットの状態やイベントを管理。
*/


// mu_push_id: IDスタックに新しいIDをプッシュする関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   data - ハッシュ化するデータ
//   size - データのサイズ (バイト単位)
// 処理内容:
//   指定されたデータをハッシュ化してIDを生成し、IDスタックにプッシュします。
void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}

// mu_pop_id: IDスタックからIDをポップする関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   IDスタックの最後のIDを削除します。
void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}

// mu_push_clip_rect: クリッピング矩形スタックに新しい矩形をプッシュする関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - プッシュするクリッピング矩形
// 処理内容:
//   現在のクリッピング矩形と指定された矩形の交差部分を計算し、スタックにプッシュします。
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}

// mu_pop_clip_rect: クリッピング矩形スタックから矩形をポップする関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   クリッピング矩形スタックの最後の矩形を削除します。
void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}

// mu_get_clip_rect: 現在のクリッピング矩形を取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 戻り値:
//   現在のクリッピング矩形
// 処理内容:
//   クリッピング矩形スタックの最後の矩形を返します。
mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}

// mu_check_clip: 矩形が現在のクリッピング矩形に含まれるかを判定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   r - 判定対象の矩形
// 戻り値:
//   MU_CLIP_ALL (完全にクリッピングされる場合)
//   0 (クリッピングされない場合)
//   MU_CLIP_PART (部分的にクリッピングされる場合)
// 処理内容:
//   指定された矩形が現在のクリッピング矩形に含まれるかを判定します。
int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y) {
    return MU_CLIP_ALL; // 完全にクリッピングされる
  }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h) {
    return 0; // クリッピングされない
  }
  return MU_CLIP_PART; // 部分的にクリッピングされる
}
// push_layout: レイアウトスタックに新しいレイアウトをプッシュする関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   body - レイアウトの矩形領域
//   scroll - スクロール位置
// 処理内容:
//   レイアウトを初期化し、スタックにプッシュします。
//   レイアウトの初期位置やサイズを設定します。
static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}

// get_layout: レイアウトスタックの現在のレイアウトを取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 戻り値:
//   現在のレイアウトへのポインタ
static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}

// pop_container: コンテナスタックから現在のコンテナをポップする関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   現在のコンテナの内容サイズを計算し、スタックから削除します。
//   レイアウトスタックとIDスタックも同時にポップします。
static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}

// mu_get_current_container: コンテナスタックの現在のコンテナを取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 戻り値:
//   現在のコンテナへのポインタ
mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ctx->container_stack.idx - 1];
}

// get_container: コンテナを取得または初期化する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   id - コンテナを識別するための一意のID
//   opt - コンテナのオプションフラグ
// 戻り値:
//   指定されたIDに対応するコンテナへのポインタ
// 処理内容:
//   - 指定されたIDに対応するコンテナをプールから取得します。
//   - コンテナが存在しない場合、新しいコンテナを初期化します。
//   - コンテナが閉じられている場合、NULLを返します。
static mu_Container* get_container(mu_Context *ctx, mu_Id id, int opt) {
  mu_Container *cnt;
  int idx = mu_pool_get(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  if (idx >= 0) {
    if (ctx->containers[idx].open || ~opt & MU_OPT_CLOSED) {
      mu_pool_update(ctx, ctx->container_pool, idx);
    }
    return &ctx->containers[idx];
  }
  if (opt & MU_OPT_CLOSED) { return NULL; }
  idx = mu_pool_init(ctx, ctx->container_pool, MU_CONTAINERPOOL_SIZE, id);
  cnt = &ctx->containers[idx];
  memset(cnt, 0, sizeof(*cnt));
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
  return cnt;
}

// mu_get_container: 名前からコンテナを取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   name - コンテナの名前
// 戻り値:
//   指定された名前に対応するコンテナへのポインタ
// 処理内容:
//   名前をハッシュ化してIDを生成し、get_container関数を呼び出します。
mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}

// mu_bring_to_front: コンテナを最前面に移動する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   cnt - 最前面に移動するコンテナ
// 処理内容:
//   コンテナのZインデックスを更新し、最前面に移動します。
void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) {
  cnt->zindex = ++ctx->last_zindex;
}


/*============================================================================
** pool
**============================================================================*/

// mu_pool_init: プール内のアイテムを初期化する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   items - プール内のアイテム配列
//   len - プールの長さ（アイテム数）
//   id - 初期化するアイテムのID
// 戻り値:
//   初期化されたアイテムのインデックス
// 処理内容:
//   - プール内で最も古いアイテムを見つけ、そのアイテムを初期化します。
//   - アイテムのIDを設定し、更新タイムスタンプを現在のフレームに設定します。
int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for (i = 0; i < len; i++) {
    if (items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1); // プールに空きがあることを確認
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}

// mu_pool_get: プール内のアイテムを取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   items - プール内のアイテム配列
//   len - プールの長さ（アイテム数）
//   id - 検索するアイテムのID
// 戻り値:
//   指定されたIDに対応するアイテムのインデックス（見つからない場合は-1）
// 処理内容:
//   - プール内を検索し、指定されたIDに対応するアイテムを見つけます。
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx); // ctxはこの関数内では使用されない
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1; // 見つからない場合
}

// mu_pool_update: プール内のアイテムの更新タイムスタンプを設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   items - プール内のアイテム配列
//   idx - 更新するアイテムのインデックス
// 処理内容:
//   - 指定されたアイテムの更新タイムスタンプを現在のフレームに設定します。
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}

/*============================================================================
** input handlers
**============================================================================*/
// mu_input_mousemove: マウスの移動イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   x - マウスの現在のX座標
//   y - マウスの現在のY座標
// 処理内容:
//   マウスの現在位置を更新します。
void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  ctx->mouse_pos = mu_vec2(x, y);
}

// mu_input_mousedown: マウスのボタン押下イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   x - マウスの現在のX座標
//   y - マウスの現在のY座標
//   btn - 押下されたボタンの種類 (例: 左ボタン、右ボタン)
// 処理内容:
//   マウスの現在位置を更新し、押下されたボタンの状態を記録します。
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;
}

// mu_input_mouseup: マウスのボタン解放イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   x - マウスの現在のX座標
//   y - マウスの現在のY座標
//   btn - 解放されたボタンの種類
// 処理内容:
//   マウスの現在位置を更新し、解放されたボタンの状態を記録します。
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
}

// mu_input_scroll: マウスホイールのスクロールイベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   x - 水平方向のスクロール量
//   y - 垂直方向のスクロール量
// 処理内容:
//   スクロール量を記録します。
void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}

// mu_input_keydown: キー押下イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   key - 押下されたキーのコード
// 処理内容:
//   押下されたキーの状態を記録します。
void mu_input_keydown(mu_Context *ctx, int key) {
  ctx->key_pressed |= key;
  ctx->key_down |= key;
}

// mu_input_keyup: キー解放イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   key - 解放されたキーのコード
// 処理内容:
//   解放されたキーの状態を記録します。
void mu_input_keyup(mu_Context *ctx, int key) {
  ctx->key_down &= ~key;
}

// mu_input_text: テキスト入力イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   text - 入力されたテキスト
// 処理内容:
//   入力されたテキストをバッファに追加します。
void mu_input_text(mu_Context *ctx, const char *text) {
  int len = strlen(ctx->input_text);
  int size = strlen(text) + 1;
  expect(len + size <= (int) sizeof(ctx->input_text));
  memcpy(ctx->input_text + len, text, size);
}

/*============================================================================
** commandlist
**============================================================================*/

/*
** コマンドリストの概要:
** MicroUIでは、描画やクリッピングなどの操作をコマンドとして記録し、
** 後で一括して処理する仕組みを採用しています。
** コマンドリストは、描画コマンドやクリッピングコマンドなどを格納するためのリストです。
**
** 主な動作:
** - コマンドをリストに追加する際には、`mu_push_command`を使用します。
** - コマンドを順次処理する際には、`mu_next_command`を使用します。
** - コマンドには、矩形描画、テキスト描画、アイコン描画、クリッピング設定などがあります。
**
** 利点:
** - 描画処理を一元管理できる。
** - 描画順序を制御しやすい。
** - 描画の最適化が可能。
*/

// mu_push_command: 新しいコマンドをコマンドリストに追加する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   type - コマンドの種類 (例: 矩形描画、テキスト描画など)
//   size - コマンドのサイズ (バイト単位)
// 戻り値:
//   追加されたコマンドへのポインタ
// 処理内容:
//   - 指定された種類とサイズのコマンドをコマンドリストに追加します。
//   - コマンドリストのインデックスを更新します。
mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}

// mu_next_command: コマンドリスト内の次のコマンドを取得する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   cmd - 現在のコマンドへのポインタ (NULLの場合は最初のコマンドを取得)
// 戻り値:
//   次のコマンドが存在する場合は1、存在しない場合は0
// 処理内容:
//   - 現在のコマンドの次のコマンドを取得します。
//   - ジャンプコマンドをスキップします。
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

// push_jump: ジャンプコマンドをコマンドリストに追加する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   dst - ジャンプ先のコマンドへのポインタ
// 戻り値:
//   追加されたジャンプコマンドへのポインタ
// 処理内容:
//   - ジャンプコマンドをコマンドリストに追加します。
//   - ジャンプ先のコマンドを設定します。
static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}

// mu_set_clip: クリッピング矩形を設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - 設定するクリッピング矩形
// 処理内容:
//   - クリッピングコマンドをコマンドリストに追加します。
//   - 指定された矩形をクリッピング領域として設定します。
void mu_set_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}

// mu_draw_rect: 矩形を描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - 描画する矩形
//   color - 矩形の色
// 処理内容:
//   - 矩形をクリッピング領域と交差させます。
//   - 交差部分が存在する場合、矩形描画コマンドをコマンドリストに追加します。
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}

// mu_draw_box: 境界線付きの矩形を描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - 描画する矩形
//   color - 境界線の色
// 処理内容:
//   - 矩形の上下左右に境界線を描画します。
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y + rect.h - 1, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x, rect.y, 1, rect.h), color);
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w - 1, rect.y, 1, rect.h), color);
}

// mu_draw_text: テキストを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   font - 使用するフォント
//   str - 描画する文字列
//   len - 文字列の長さ (-1の場合は自動計算)
//   pos - テキストの描画位置
//   color - テキストの色
// 処理内容:
//   - テキストの描画領域をクリッピング領域と交差させます。
//   - 交差部分が存在する場合、テキスト描画コマンドをコマンドリストに追加します。
void mu_draw_text(mu_Context *ctx, mu_Font font, const char *str, int len,
  mu_Vec2 pos, mu_Color color)
{
  mu_Command *cmd;
  mu_Rect rect = mu_rect(
    pos.x, pos.y, ctx->text_width(font, str, len), ctx->text_height(font));
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  /* add command */
  if (len < 0) { len = strlen(str); }
  cmd = mu_push_command(ctx, MU_COMMAND_TEXT, sizeof(mu_TextCommand) + len);
  memcpy(cmd->text.str, str, len);
  cmd->text.str[len] = '\0';
  cmd->text.pos = pos;
  cmd->text.color = color;
  cmd->text.font = font;
  /* reset clipping if it was set */
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}

// mu_draw_icon: アイコンを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   id - 描画するアイコンのID
//   rect - アイコンの描画領域
//   color - アイコンの色
// 処理内容:
//   - アイコンの描画領域をクリッピング領域と交差させます。
//   - 交差部分が存在する場合、アイコン描画コマンドをコマンドリストに追加します。
void mu_draw_icon(mu_Context *ctx, int id, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  int clipped = mu_check_clip(ctx, rect);
  if (clipped == MU_CLIP_ALL ) { return; }
  if (clipped == MU_CLIP_PART) { mu_set_clip(ctx, mu_get_clip_rect(ctx)); }
  cmd = mu_push_command(ctx, MU_COMMAND_ICON, sizeof(mu_IconCommand));
  cmd->icon.id = id;
  cmd->icon.rect = rect;
  cmd->icon.color = color;
  if (clipped) { mu_set_clip(ctx, unclipped_rect); }
}


enum { RELATIVE = 1, ABSOLUTE = 2 };

/*============================================================================
** layout
**============================================================================*/

/*
** レイアウトの概要:
** MicroUIでは、UI要素を効率的に配置するためにレイアウトシステムを使用しています。
** レイアウトは、行や列を基にUI要素を配置し、サイズや位置を計算します。
**
** 主な動作:
** - `mu_layout_row`を使用して行の幅や高さを設定します。
** - `mu_layout_next`を使用して次の要素の位置とサイズを計算します。
** - `mu_layout_begin_column`と`mu_layout_end_column`を使用して列を管理します。
** - `mu_layout_width`や`mu_layout_height`で要素のサイズを指定できます。
**
** 利点:
** - UI要素の配置を簡単に管理できる。
** - 自動的にサイズや位置を計算するため、手動での調整が不要。
** - レスポンシブなUI設計が可能。
*/

// mu_layout_row: レイアウトの行を設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   items - 行内のアイテム数
//   widths - 各アイテムの幅 (NULLの場合は自動計算)
//   height - 行の高さ (0の場合は自動計算)
// 処理内容:
//   - 行の幅と高さを設定します。
//   - 次の要素の配置位置を初期化します。
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

// mu_layout_begin_column: 新しい列を開始する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - 現在のレイアウトを保存し、新しいレイアウトをスタックにプッシュします。
void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}

// mu_layout_end_column: 列を終了する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - 現在のレイアウトをポップし、親レイアウトに統合します。
void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* 子レイアウトの位置やサイズを親レイアウトに統合 */
  a = get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}

// mu_layout_width: 次の要素の幅を設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   width - 設定する幅
// 処理内容:
//   - 次の要素の幅を指定します。
void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}

// mu_layout_height: 次の要素の高さを設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   height - 設定する高さ
// 処理内容:
//   - 次の要素の高さを指定します。
void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}

// mu_layout_set_next: 次の要素の位置とサイズを設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   r - 次の要素の矩形
//   relative - 相対位置かどうか (1: 相対, 0: 絶対)
// 処理内容:
//   - 次の要素の位置とサイズを設定します。
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}

// mu_layout_next: 次の要素の矩形を計算する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 戻り値:
//   次の要素の矩形
// 処理内容:
//   - 次の要素の位置とサイズを計算します。
//   - 行の終わりに達した場合、新しい行を開始します。
mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if (layout->next_type) {
    /* `mu_layout_set_next`で設定された矩形を処理 */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }

  } else {
    /* 次の行を処理 */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    /* 位置を計算 */
    res.x = layout->position.x;
    res.y = layout->position.y;

    /* サイズを計算 */
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* 位置を更新 */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* ボディのオフセットを適用 */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* 最大位置を更新 */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}

/*============================================================================
** controls
**============================================================================*/

// in_hover_root: 現在のマウス位置がホバールート内にあるかを判定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 戻り値:
//   マウスがホバールート内にある場合は1、そうでない場合は0
// 処理内容:
//   - コンテナスタックを逆順に検索し、ホバールート内にマウスがあるかを確認します。
//   - ルートコンテナに到達した場合、検索を終了します。
static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.idx;
  while (i--) {
    if (ctx->container_stack.items[i] == ctx->hover_root) { return 1; }
    // ルートコンテナに到達したら検索を終了
    if (ctx->container_stack.items[i]->head) { break; }
  }
  return 0;
}



// mu_draw_control_frame: コントロールのフレームを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   id - コントロールのID
//   rect - 描画する矩形
//   colorid - 使用する色のID
//   opt - オプションフラグ
// 処理内容:
//   - フレームを描画します。
//   - フォーカスやホバー状態に応じて色を変更します。
void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}
// mu_draw_control_text: コントロールのテキストを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   str - 描画する文字列
//   rect - テキストを描画する矩形
//   colorid - 使用する色のID
//   opt - オプションフラグ
// 処理内容:
//   - テキストの描画位置を計算します。
//   - オプションフラグに応じてテキストの配置を変更します。
//   - テキストをクリッピング矩形内に描画します。
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


// mu_mouse_over: マウスが指定された矩形内にあるかを判定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   rect - 判定対象の矩形
// 戻り値:
//   マウスが矩形内にある場合は1、そうでない場合は0
// 処理内容:
//   - マウス位置が矩形内にあるかを確認します。
//   - クリッピング矩形内にあるか、ホバールート内にあるかも判定します。
int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) &&
    rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
    in_hover_root(ctx);
}

// mu_update_control: コントロールの状態を更新する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   id - コントロールのID
//   rect - コントロールの矩形
//   opt - オプションフラグ
// 処理内容:
//   - コントロールのフォーカス、ホバー、クリック状態を更新します。
//   - マウスの位置やクリック状態に応じて、フォーカスやホバー状態を変更します。
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
// mu_text: テキストを複数行にわたって描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   text - 描画する文字列
// 処理内容:
//   - テキストを行ごとに分割し、各行を描画します。
//   - 行の幅や高さを計算し、適切な位置にテキストを配置します。
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


// mu_label: 単一行のラベルを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   text - 描画する文字列
// 処理内容:
//   - テキストを現在のレイアウト位置に描画します。
//   - テキストの色や位置を設定します。
void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}

// mu_button_ex: ボタンを描画し、クリックイベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   label - ボタンに表示するテキスト
//   icon - ボタンに表示するアイコン（オプション）
//   opt - ボタンのオプションフラグ
// 戻り値:
//   ボタンがクリックされた場合は MU_RES_SUBMIT を返す
// 処理内容:
//   - ボタンの矩形を計算し、クリックイベントを処理します。
//   - ボタンの背景、テキスト、アイコンを描画します。
int mu_button_ex(mu_Context *ctx, const char *label, int icon, int opt) {
  int res = 0;
  mu_Id id = label ? mu_get_id(ctx, label, strlen(label))
                   : mu_get_id(ctx, &icon, sizeof(icon));
  mu_Rect r = mu_layout_next(ctx);
  mu_update_control(ctx, id, r, opt);
  /* handle click */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_SUBMIT;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, r, MU_COLOR_BUTTON, opt);
  if (label) { mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, opt); }
  if (icon) { mu_draw_icon(ctx, icon, r, ctx->style->colors[MU_COLOR_TEXT]); }
  return res;
}


// mu_checkbox: チェックボックスを描画し、クリックイベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   label - チェックボックスに表示するテキスト
//   state - チェックボックスの状態（オン/オフ）
// 戻り値:
//   チェックボックスの状態が変更された場合は MU_RES_CHANGE を返す
// 処理内容:
//   - チェックボックスの矩形を計算し、クリックイベントを処理します。
//   - チェックボックスの背景、チェックマーク、テキストを描画します。
int mu_checkbox(mu_Context *ctx, const char *label, int *state) {
  int res = 0;
  mu_Id id = mu_get_id(ctx, &state, sizeof(state));
  mu_Rect r = mu_layout_next(ctx);
  mu_Rect box = mu_rect(r.x, r.y, r.h, r.h);
  mu_update_control(ctx, id, r, 0);
  /* handle click */
  if (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id) {
    res |= MU_RES_CHANGE;
    *state = !*state;
  }
  /* draw */
  mu_draw_control_frame(ctx, id, box, MU_COLOR_BASE, 0);
  if (*state) {
    mu_draw_icon(ctx, MU_ICON_CHECK, box, ctx->style->colors[MU_COLOR_TEXT]);
  }
  r = mu_rect(r.x + box.w, r.y, r.w - box.w, r.h);
  mu_draw_control_text(ctx, label, r, MU_COLOR_TEXT, 0);
  return res;
}




// mu_textbox_raw: テキストボックスを描画し、入力イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   buf - テキストボックスに表示する文字列バッファ
//   bufsz - バッファのサイズ
//   id - テキストボックスのID
//   rect - テキストボックスの矩形
//   opt - オプションフラグ
// 戻り値:
//   テキストが変更された場合は MU_RES_CHANGE を返す
//   テキストが確定された場合は MU_RES_SUBMIT を返す
// 処理内容:
//   - テキストボックスの矩形を計算し、入力イベントを処理します。
//   - テキストの入力や削除、カーソルの描画を行います。
//   - テキストボックスがアクティブな場合、カーソルを表示します。
int mu_textbox_raw(mu_Context *ctx, char *buf, int bufsz, mu_Id id, mu_Rect r,
  int opt)
{
  int res = 0;
  mu_update_control(ctx, id, r, opt | MU_OPT_HOLDFOCUS);

  if (ctx->focus == id) {
    /* handle text input */
    int len = strlen(buf);
    int n = mu_min(bufsz - len - 1, (int) strlen(ctx->input_text));
    if (n > 0) {
      memcpy(buf + len, ctx->input_text, n);
      len += n;
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle backspace */
    if (ctx->key_pressed & MU_KEY_BACKSPACE && len > 0) {
      /* skip utf-8 continuation bytes */
      while ((buf[--len] & 0xc0) == 0x80 && len > 0);
      buf[len] = '\0';
      res |= MU_RES_CHANGE;
    }
    /* handle return */
    if (ctx->key_pressed & MU_KEY_RETURN) {
      mu_set_focus(ctx, 0);
      res |= MU_RES_SUBMIT;
    }
  }

  /* draw */
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

// number_textbox: 数値入力用のテキストボックスを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   value - 入力する数値
//   r - テキストボックスの矩形
//   id - テキストボックスのID
// 戻り値:
//   テキストボックスがアクティブな場合は1、それ以外は0
// 処理内容:
//   - 数値入力モードを処理します。
//   - テキストボックスが確定された場合、数値を更新します。
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

// mu_textbox_ex: テキストボックスを描画し、入力イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   buf - テキストボックスに表示する文字列バッファ
//   bufsz - バッファのサイズ
//   opt - オプションフラグ
// 戻り値:
//   テキストが変更された場合は MU_RES_CHANGE を返す
//   テキストが確定された場合は MU_RES_SUBMIT を返す
// 処理内容:
//   - テキストボックスの矩形を計算し、入力イベントを処理します。
//   - テキストの入力や削除、カーソルの描画を行います。
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}

// mu_slider_ex: スライダーを描画し、入力イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   value - スライダーの値へのポインタ
//   low - スライダーの最小値
//   high - スライダーの最大値
//   step - スライダーのステップ値
//   fmt - スライダーの値を表示するフォーマット文字列
//   opt - オプションフラグ
// 戻り値:
//   スライダーの値が変更された場合は MU_RES_CHANGE を返す
// 処理内容:
//   - スライダーの値を計算し、入力イベントを処理します。
//   - スライダーの背景、スライダーのつまみ、値を描画します。
int mu_slider_ex(mu_Context *ctx, mu_Real *value, mu_Real low, mu_Real high,
  mu_Real step, const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  mu_Rect thumb;
  int x, w, res = 0;
  mu_Real last = *value, v = last;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);

  /* handle text input mode */
  if (number_textbox(ctx, &v, base, id)) { return res; }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if (ctx->focus == id &&
      (ctx->mouse_down | ctx->mouse_pressed) == MU_MOUSE_LEFT)
  {
    v = low + (ctx->mouse_pos.x - base.x) * (high - low) / base.w;
    if (step) { v = ((mu_int64)((v + step / 2) / step)) * step; }
  }
  /* clamp and store value, update res */
  *value = v = mu_clamp(v, low, high);
  if (last != v) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw thumb */
  w = ctx->style->thumb_size;
  x = (v - low) * (base.w - w) / (high - low);
  thumb = mu_rect(base.x + x, base.y, w, base.h);
  mu_draw_control_frame(ctx, id, thumb, MU_COLOR_BUTTON, opt);
  /* draw text  */
  sprintf(buf, fmt, v);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}

// mu_number_ex: 数値入力用のコントロールを描画し、入力イベントを処理する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   value - 入力する数値へのポインタ
//   step - 数値の増減ステップ
//   fmt - 数値を表示するフォーマット文字列
//   opt - オプションフラグ
// 戻り値:
//   数値が変更された場合は MU_RES_CHANGE を返す
// 処理内容:
//   - 数値入力モードを処理します。
//   - 数値の増減や入力イベントを処理します。
//   - 数値を表示するテキストを描画します。
int mu_number_ex(mu_Context *ctx, mu_Real *value, mu_Real step,
  const char *fmt, int opt)
{
  char buf[MU_MAX_FMT + 1];
  int res = 0;
  mu_Id id = mu_get_id(ctx, &value, sizeof(value));
  mu_Rect base = mu_layout_next(ctx);
  mu_Real last = *value;

  /* handle text input mode */
  if (number_textbox(ctx, value, base, id)) { return res; }

  /* handle normal mode */
  mu_update_control(ctx, id, base, opt);

  /* handle input */
  if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {
    *value += ctx->mouse_delta.x * step;
  }
  /* set flag if value changed */
  if (*value != last) { res |= MU_RES_CHANGE; }

  /* draw base */
  mu_draw_control_frame(ctx, id, base, MU_COLOR_BASE, opt);
  /* draw text  */
  sprintf(buf, fmt, *value);
  mu_draw_control_text(ctx, buf, base, MU_COLOR_TEXT, opt);

  return res;
}

// header: ヘッダー（折りたたみ可能な要素）を描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   label - ヘッダーのラベル
//   istreenode - ツリーノードかどうかを示すフラグ
//   opt - オプションフラグ
// 戻り値:
//   ヘッダーがアクティブな場合は MU_RES_ACTIVE を返す
// 処理内容:
//   - ヘッダーの矩形を計算し、クリックイベントを処理します。
//   - ヘッダーの背景、アイコン、テキストを描画します。
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

  /* handle click */
  active ^= (ctx->mouse_pressed == MU_MOUSE_LEFT && ctx->focus == id);

  /* update pool ref */
  if (idx >= 0) {
    if (active) { mu_pool_update(ctx, ctx->treenode_pool, idx); }
           else { memset(&ctx->treenode_pool[idx], 0, sizeof(mu_PoolItem)); }
  } else if (active) {
    mu_pool_init(ctx, ctx->treenode_pool, MU_TREENODEPOOL_SIZE, id);
  }

  /* draw */
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

// mu_header_ex: 折りたたみ可能なヘッダーを描画する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   label - ヘッダーのラベル
//   opt - オプションフラグ
// 戻り値:
//   ヘッダーがアクティブな場合は MU_RES_ACTIVE を返す
// 処理内容:
//   - ヘッダーの矩形を計算し、クリックイベントを処理します。
//   - ヘッダーの背景、アイコン、テキストを描画します。
int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, 0, opt);
}


// mu_begin_treenode_ex: 折りたたみ可能なツリーノードを開始する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   label - ツリーノードのラベル
//   opt - オプションフラグ
// 戻り値:
//   ツリーノードが展開されている場合は MU_RES_ACTIVE を返す
// 処理内容:
//   - ツリーノードの矩形を計算し、クリックイベントを処理します。
//   - ツリーノードの背景、アイコン、テキストを描画します。
//   - ツリーノードが展開されている場合、インデントを追加します。
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if (res & MU_RES_ACTIVE) {
    get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->last_id);
  }
  return res;
}

// mu_end_treenode: ツリーノードを終了する関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - ツリーノードのインデントを減らします。
//   - IDスタックからツリーノードのIDをポップします。
void mu_end_treenode(mu_Context *ctx) {
  get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}


// scrollbar: スクロールバーを描画し、スクロールイベントを処理するマクロ
// 引数:
//   ctx - MicroUIのコンテキスト
//   cnt - スクロール対象のコンテナ
//   b - スクロールバーのボディ（矩形領域）
//   cs - コンテンツサイズ（スクロール可能な領域のサイズ）
//   x, y, w, h - スクロールバーの方向とサイズを指定するためのパラメータ
// 処理内容:
//   - コンテンツサイズがボディサイズを超える場合にスクロールバーを描画します。
//   - スクロールバーの位置とサイズを計算し、スクロール可能な範囲を設定します。
//   - マウス操作によるスクロールイベントを処理します。
//   - スクロールバーの背景とサム（つまみ）を描画します。
//   - マウスホイール操作時にスクロール対象として設定します。
#define scrollbar(ctx, cnt, b, cs, x, y, w, h)                              \
  do {                                                                      \
    /* コンテンツサイズがボディサイズを超える場合のみスクロールバーを追加 */ \
    int maxscroll = cs.y - b->h;                                            \
                                                                            \
    if (maxscroll > 0 && b->h > 0) {                                        \
      mu_Rect base, thumb;                                                  \
      mu_Id id = mu_get_id(ctx, "!scrollbar" #y, 11);                       \
                                                                            \
      /* スクロールバーのサイズと位置を計算 */                              \
      base = *b;                                                            \
      base.x = b->x + b->w;                                                 \
      base.w = ctx->style->scrollbar_size;                                  \
                                                                            \
      /* 入力処理 */                                                        \
      mu_update_control(ctx, id, base, 0);                                  \
      if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {           \
        cnt->scroll.y += ctx->mouse_delta.y * cs.y / base.h;                \
      }                                                                     \
      /* スクロール位置を制限 */                                            \
      cnt->scroll.y = mu_clamp(cnt->scroll.y, 0, maxscroll);                \
                                                                            \
      /* スクロールバーの背景とサムを描画 */                                 \
      ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);                      \
      thumb = base;                                                         \
      thumb.h = mu_max(ctx->style->thumb_size, base.h * b->h / cs.y);       \
      thumb.y += cnt->scroll.y * (base.h - thumb.h) / maxscroll;            \
      ctx->draw_frame(ctx, thumb, MU_COLOR_SCROLLTHUMB);                    \
                                                                            \
      /* マウスがスクロールバー上にある場合、スクロール対象として設定 */     \
      if (mu_mouse_over(ctx, *b)) { ctx->scroll_target = cnt; }             \
    } else {                                                                \
      cnt->scroll.y = 0;                                                    \
    }                                                                       \
  } while (0)

// scrollbars: コンテナに水平および垂直スクロールバーを追加する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   cnt - スクロール対象のコンテナ
//   body - コンテナのボディ（矩形領域）
// 処理内容:
//   - コンテンツサイズに基づいてスクロールバーを追加します。
//   - スクロールバーの位置とサイズを計算し、描画します。
static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding * 2;
  cs.y += ctx->style->padding * 2;
  mu_push_clip_rect(ctx, *body);

  /* スクロールバーのためにボディサイズを調整 */
  if (cs.y > cnt->body.h) { body->w -= sz; }
  if (cs.x > cnt->body.w) { body->h -= sz; }

  /* 水平および垂直スクロールバーを作成 */
  scrollbar(ctx, cnt, body, cs, x, y, w, h);
  scrollbar(ctx, cnt, body, cs, y, x, h, w);

  mu_pop_clip_rect(ctx);
}

// push_container_body: コンテナのボディを設定する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   cnt - コンテナ
//   body - コンテナのボディ（矩形領域）
//   opt - オプションフラグ
// 処理内容:
//   - スクロールバーを追加します（必要な場合）。
//   - レイアウトを初期化し、コンテナのボディを設定します。
static void push_container_body(
  mu_Context *ctx, mu_Container *cnt, mu_Rect body, int opt
) {
  if (~opt & MU_OPT_NOSCROLL) { scrollbars(ctx, cnt, &body); }
  push_layout(ctx, expand_rect(body, -ctx->style->padding), cnt->scroll);
  cnt->body = body;
}
static void begin_root_container(mu_Context *ctx, mu_Container *cnt) {
  push(ctx->container_stack, cnt);
  /* push container to roots list and push head command */
  push(ctx->root_list, cnt);
  cnt->head = push_jump(ctx, NULL);
  /* set as hover root if the mouse is overlapping this container and it has a
  ** higher zindex than the current hover root */
  if (rect_overlaps_vec2(cnt->rect, ctx->mouse_pos) &&
      (!ctx->next_hover_root || cnt->zindex > ctx->next_hover_root->zindex)
  ) {
    ctx->next_hover_root = cnt;
  }
  /* clipping is reset here in case a root-container is made within
  ** another root-containers's begin/end block; this prevents the inner
  ** root-container being clipped to the outer */
  push(ctx->clip_stack, unclipped_rect);
}


// end_root_container: ルートコンテナの終了処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - ルートコンテナの終了処理として、コマンドリストにジャンプコマンドを追加します。
//   - コンテナのクリッピング矩形をポップし、コンテナスタックからコンテナを削除します。
static void end_root_container(mu_Context *ctx) {
  // 現在のコンテナを取得
  mu_Container *cnt = mu_get_current_container(ctx);

  // コマンドリストに終了用のジャンプコマンドを追加
  cnt->tail = push_jump(ctx, NULL);

  // コンテナの先頭コマンドのジャンプ先を現在のコマンドリストの位置に設定
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;

  // クリッピング矩形をポップ
  mu_pop_clip_rect(ctx);

  // コンテナスタックからコンテナを削除
  pop_container(ctx);
}
/**
 * @brief ウィンドウを開始する関数
 *
 * この関数は、MicroUI ライブラリのウィンドウを開始するための関数です。
 * ウィンドウの描画と管理を行うための一連の操作を行います。
 *
 * @param ctx MicroUI のコンテキストを指すポインタ
 * @param title ウィンドウのタイトルを指定する文字列
 * @param rect ウィンドウの初期位置とサイズを指定する矩形
 * @param opt ウィンドウの動作や外観を制御するオプションフラグ
 * @return int ウィンドウがアクティブかどうかを示す値。アクティブな場合は MU_RES_ACTIVE を返す
 */
int mu_begin_window_ex(mu_Context* ctx, const char* title, mu_Rect rect, int opt)
{
    mu_Rect body;
    mu_Id id = mu_get_id(ctx, title, strlen(title));  // ウィンドウのタイトルから一意の ID を生成
    mu_Container* cnt = get_container(ctx, id, opt);  // コンテナを取得または初期化
    if (!cnt || !cnt->open) { return 0; }  // コンテナが存在しないか閉じられている場合は終了
    push(ctx->id_stack, id);  // コンテナの ID をスタックにプッシュ

    if (cnt->rect.w == 0) { cnt->rect = rect; }  // コンテナの矩形が未設定の場合は初期矩形を設定
    begin_root_container(ctx, cnt);  // ルートコンテナの初期化
    rect = body = cnt->rect;  // コンテナの矩形を設定

    /* フレームの描画 */
    if (~opt & MU_OPT_NOFRAME)
    {
        ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);  // ウィンドウの背景を描画
    }

    /* タイトルバーの処理 */
    if (~opt & MU_OPT_NOTITLE)
    {
        mu_Rect tr = rect;
        tr.h = ctx->style->title_height;  // タイトルバーの高さを設定
        ctx->draw_frame(ctx, tr, MU_COLOR_TITLEBG);  // タイトルバーの背景を描画

        /* タイトルテキストの描画 */
        if (~opt & MU_OPT_NOTITLE)
        {
            mu_Id id = mu_get_id(ctx, "!title", 6);  // タイトルの ID を生成
            mu_update_control(ctx, id, tr, opt);  // タイトルバーのコントロールを更新
            mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt);  // タイトルテキストを描画
            if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
            {
                cnt->rect.x += ctx->mouse_delta.x;  // ウィンドウのドラッグ操作を処理
                cnt->rect.y += ctx->mouse_delta.y;
            }
            body.y += tr.h;  // タイトルバーの高さ分だけボディの位置を下げる
            body.h -= tr.h;  // タイトルバーの高さ分だけボディの高さを減らす
        }

        /* クローズボタンの処理 */
        if (~opt & MU_OPT_NOCLOSE)
        {
            mu_Id id = mu_get_id(ctx, "!close", 6);  // クローズボタンの ID を生成
            mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);  // クローズボタンの矩形を設定
            tr.w -= r.w;  // タイトルバーの幅をクローズボタンの幅分だけ減らす
            mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);  // クローズアイコンを描画
            mu_update_control(ctx, id, r, opt);  // クローズボタンのコントロールを更新
            if (ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus)
            {
                cnt->open = 0;  // クローズボタンがクリックされた場合はコンテナを閉じる
            }
        }
    }

    push_container_body(ctx, cnt, body, opt);  // コンテナのボディを設定

    /* リサイズハンドルの処理 */
    if (~opt & MU_OPT_NORESIZE)
    {
        int sz = ctx->style->title_height;  // リサイズハンドルのサイズを設定
        mu_Id id = mu_get_id(ctx, "!resize", 7);  // リサイズハンドルの ID を生成
        mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);  // リサイズハンドルの矩形を設定
        mu_update_control(ctx, id, r, opt);  // リサイズハンドルのコントロールを更新
        if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
        {
            cnt->rect.w = mu_max(96, cnt->rect.w + ctx->mouse_delta.x);  // ウィンドウの幅をリサイズ
            cnt->rect.h = mu_max(64, cnt->rect.h + ctx->mouse_delta.y);  // ウィンドウの高さをリサイズ
        }
    }

    /* コンテンツサイズに合わせたリサイズ */
    if (opt & MU_OPT_AUTOSIZE)
    {
        mu_Rect r = get_layout(ctx)->body;  // レイアウトのボディを取得
        cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);  // コンテンツサイズに合わせて幅を調整
        cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);  // コンテンツサイズに合わせて高さを調整
    }

    /* ポップアップウィンドウのクローズ処理 */
    if (opt & MU_OPT_POPUP && ctx->mouse_pressed && ctx->hover_root != cnt)
    {
        cnt->open = 0;  // 他の場所がクリックされた場合はポップアップウィンドウを閉じる
    }

    mu_push_clip_rect(ctx, cnt->body);  // コンテナのボディをクリッピング矩形として設定
    return MU_RES_ACTIVE;  // ウィンドウがアクティブであることを示す
}

// mu_end_window: ウィンドウの終了処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - ウィンドウのクリッピング矩形をポップします。
//   - ルートコンテナの終了処理を行います。
void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}

// mu_open_popup: ポップアップウィンドウを開く関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   name - ポップアップウィンドウの名前
// 処理内容:
//   - ポップアップウィンドウを現在のマウス位置に表示します。
//   - ポップアップウィンドウを開き、最前面に移動します。
void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
}

// mu_begin_popup: ポップアップウィンドウを開始する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   name - ポップアップウィンドウの名前
// 戻り値:
//   ポップアップウィンドウがアクティブな場合は MU_RES_ACTIVE を返す
// 処理内容:
//   - ポップアップウィンドウを初期化し、描画を開始します。
int mu_begin_popup(mu_Context *ctx, const char *name) {
  int opt = MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
            MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED;
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}

// mu_end_popup: ポップアップウィンドウの終了処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - ポップアップウィンドウの終了処理を行います。
void mu_end_popup(mu_Context *ctx) {
  mu_end_window(ctx);
}

// mu_begin_panel_ex: パネルを開始する関数
// 引数:
//   ctx - MicroUIのコンテキスト
//   name - パネルの名前
//   opt - オプションフラグ
// 処理内容:
//   - パネルの矩形を計算し、描画を開始します。
//   - パネルの背景を描画し、クリッピング矩形を設定します。
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

// mu_end_panel: パネルの終了処理を行う関数
// 引数:
//   ctx - MicroUIのコンテキスト
// 処理内容:
//   - パネルのクリッピング矩形をポップします。
//   - コンテナスタックからパネルを削除します。
void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
/////////////////////////////////////////////////////////////////////////
// ios用の追加

/**
 * タッチデバイス用の状態記録関数 - 前フレームの状態を保存する
 * 
 * @param ctx MicroUIのコンテキスト
 */
void mu_store_state(mu_Context *ctx) {
    ctx->prev_hover = ctx->hover;
    ctx->prev_focus = ctx->focus;
    // 新しいフレームごとに状態を保存
}

/**
 * タッチドラッグ開始時に対象を明示的に指定
 * 
 * @param ctx MicroUIのコンテキスト
 * @param id ドラッグ対象のID (通常はタイトルバー)
 */
void mu_begin_dragging(mu_Context *ctx, mu_Id id) {
    ctx->active_drag_id = id;
    ctx->drag_active = 1;
    ctx->hover = id;
    mu_set_focus(ctx, id);
    ctx->updated_focus = 1; // フォーカスが更新されたことをマーク
}

/**
 * ドラッグ状態を維持 - 状態が不意に失われた場合に復元する
 * 
 * @param ctx MicroUIのコンテキスト
 */
void mu_maintain_dragging(mu_Context *ctx) {
    // ドラッグ中にホバー/フォーカスが失われたら復元
    if (ctx->drag_active && ctx->active_drag_id) {
        if (ctx->hover != ctx->active_drag_id) {
            ctx->hover = ctx->active_drag_id;
        }
        if (ctx->focus != ctx->active_drag_id) {
            ctx->focus = ctx->active_drag_id;
            ctx->updated_focus = 1; // フォーカスが更新されたことをマーク
        }
    }
}

/**
 * ドラッグ終了時に状態をリセット
 * 
 * @param ctx MicroUIのコンテキスト
 */
void mu_end_dragging(mu_Context *ctx) {
    ctx->active_drag_id = 0;
    ctx->drag_active = 0;
    // ホバーとフォーカスはmu_endで適切に処理される
}