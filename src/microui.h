/*
** Copyright (c) 2024 rxi
**
** This library is free software; you can redistribute it and/or modify it
** under the terms of the MIT license. See `microui.c` for details.
*/

#ifndef MICROUI_H
#define MICROUI_H

#ifdef __cplusplus
extern "C" {
#endif

#define MU_VERSION "2.02"

#define MU_COMMANDLIST_SIZE     (256 * 1024)
#define MU_ROOTLIST_SIZE        32
#define MU_CONTAINERSTACK_SIZE  32
#define MU_CLIPSTACK_SIZE       32
#define MU_IDSTACK_SIZE         32
#define MU_LAYOUTSTACK_SIZE     16
#define MU_CONTAINERPOOL_SIZE   48
#define MU_TREENODEPOOL_SIZE    48
#define MU_MAX_WIDTHS           16
#define MU_REAL                 float
#define MU_REAL_FMT             "%.3g"
#define MU_SLIDER_FMT           "%.2f"
#define MU_MAX_FMT              127

#define mu_stack(T, n)          struct { int idx; T items[n]; }
#define mu_min(a, b)            ((a) < (b) ? (a) : (b))
#define mu_max(a, b)            ((a) > (b) ? (a) : (b))
#define mu_clamp(x, a, b)       mu_min(b, mu_max(a, x))

	enum
	{
		MU_CLIP_PART = 1,
		MU_CLIP_ALL
	};

	enum
	{
		MU_COMMAND_JUMP = 1,
		MU_COMMAND_CLIP,
		MU_COMMAND_RECT,
		MU_COMMAND_TEXT,
		MU_COMMAND_ICON,
		MU_COMMAND_MAX
	};

	enum
	{
		MU_COLOR_TEXT,
		MU_COLOR_BORDER,
		MU_COLOR_WINDOWBG,
		MU_COLOR_TITLEBG,
		MU_COLOR_TITLETEXT,
		MU_COLOR_PANELBG,
		MU_COLOR_BUTTON,
		MU_COLOR_BUTTONHOVER,
		MU_COLOR_BUTTONFOCUS,
		MU_COLOR_BASE,
		MU_COLOR_BASEHOVER,
		MU_COLOR_BASEFOCUS,
		MU_COLOR_SCROLLBASE,
		MU_COLOR_SCROLLTHUMB,
		MU_COLOR_MAX
	};

	enum
	{
		MU_ICON_CLOSE = 1,
		MU_ICON_CHECK,
		MU_ICON_COLLAPSED,
		MU_ICON_EXPANDED,
		MU_ICON_MAX
	};

	enum
	{
		MU_RES_ACTIVE = (1 << 0),
		MU_RES_SUBMIT = (1 << 1),
		MU_RES_CHANGE = (1 << 2)
	};

	enum
	{
		MU_OPT_ALIGNCENTER = (1 << 0),
		MU_OPT_ALIGNRIGHT = (1 << 1),
		MU_OPT_NOINTERACT = (1 << 2),
		MU_OPT_NOFRAME = (1 << 3),
		MU_OPT_NORESIZE = (1 << 4),
		MU_OPT_NOSCROLL = (1 << 5),
		MU_OPT_NOCLOSE = (1 << 6),
		MU_OPT_NOTITLE = (1 << 7),
		MU_OPT_HOLDFOCUS = (1 << 8),
		MU_OPT_AUTOSIZE = (1 << 9),
		MU_OPT_POPUP = (1 << 10),
		MU_OPT_CLOSED = (1 << 11),
		MU_OPT_EXPANDED = (1 << 12)
	};

	enum
	{
		MU_MOUSE_LEFT = (1 << 0),
		MU_MOUSE_RIGHT = (1 << 1),
		MU_MOUSE_MIDDLE = (1 << 2)
	};

	enum
	{
		MU_KEY_SHIFT = (1 << 0),
		MU_KEY_CTRL = (1 << 1),
		MU_KEY_ALT = (1 << 2),
		MU_KEY_BACKSPACE = (1 << 3),
		MU_KEY_RETURN = (1 << 4)
	};


	typedef struct mu_Context mu_Context;
	typedef unsigned mu_Id;
	typedef MU_REAL mu_Real;
	typedef void* mu_Font;

	typedef struct { int x, y; } mu_Vec2;
	typedef struct { int x, y, w, h; } mu_Rect;
	typedef struct { unsigned char r, g, b, a; } mu_Color;
	typedef struct { mu_Id id; int last_update; } mu_PoolItem;

	typedef struct { int type, size; } mu_BaseCommand;
	typedef struct { mu_BaseCommand base; void* dst; } mu_JumpCommand;
	typedef struct { mu_BaseCommand base; mu_Rect rect; } mu_ClipCommand;
	typedef struct { mu_BaseCommand base; mu_Rect rect; mu_Color color; } mu_RectCommand;
	typedef struct { mu_BaseCommand base; mu_Font font; mu_Vec2 pos; mu_Color color; char str[1]; } mu_TextCommand;
	typedef struct { mu_BaseCommand base; mu_Rect rect; int id; mu_Color color; } mu_IconCommand;

	typedef union
	{
		int type;
		mu_BaseCommand base;
		mu_JumpCommand jump;
		mu_ClipCommand clip;
		mu_RectCommand rect;
		mu_TextCommand text;
		mu_IconCommand icon;
	} mu_Command;

	typedef struct
	{
		mu_Rect body;
		mu_Rect next;
		mu_Vec2 position;
		mu_Vec2 size;
		mu_Vec2 max;
		int widths[MU_MAX_WIDTHS];
		int items;
		int item_index;
		int next_row;
		int next_type;
		int indent;
	} mu_Layout;

	typedef struct
	{
		mu_Command* head, * tail;
		mu_Rect rect;
		mu_Rect body;
		mu_Vec2 content_size;
		mu_Vec2 scroll;
		int zindex;
		int open;
	} mu_Container;

	typedef struct
	{
		mu_Font font;
		mu_Vec2 size;
		int padding;
		int spacing;
		int indent;
		int title_height;
		int scrollbar_size;
		int thumb_size;
		mu_Color colors[MU_COLOR_MAX];
	} mu_Style;

	struct mu_Context
	{
		/* callbacks */
		int (*text_width)(mu_Font font, const char* str, int len);
		int (*text_height)(mu_Font font);
		void (*draw_frame)(mu_Context* ctx, mu_Rect rect, int colorid);
		void (*draw_text)(mu_Context* ctx, mu_Font font, const char* str, int len, mu_Vec2 pos, mu_Color color); // 追加

		/* core state */
		mu_Style _style;
		mu_Style* style;
		mu_Id hover;
		mu_Id focus;
		mu_Id last_id;
		mu_Rect last_rect;
		int last_zindex;
		int updated_focus;
		int frame;
		mu_Container* hover_root;
		mu_Container* next_hover_root;
		mu_Container* scroll_target;
		char number_edit_buf[MU_MAX_FMT];
		mu_Id number_edit;
		/* stacks */
		mu_stack(char, MU_COMMANDLIST_SIZE) command_list;
		mu_stack(mu_Container*, MU_ROOTLIST_SIZE) root_list;
		mu_stack(mu_Container*, MU_CONTAINERSTACK_SIZE) container_stack;
		mu_stack(mu_Rect, MU_CLIPSTACK_SIZE) clip_stack;
		mu_stack(mu_Id, MU_IDSTACK_SIZE) id_stack;
		mu_stack(mu_Layout, MU_LAYOUTSTACK_SIZE) layout_stack;
		/* retained state pools */
		mu_PoolItem container_pool[MU_CONTAINERPOOL_SIZE];
		mu_Container containers[MU_CONTAINERPOOL_SIZE];
		mu_PoolItem treenode_pool[MU_TREENODEPOOL_SIZE];
		/* input state */
		mu_Vec2 mouse_pos;
		mu_Vec2 last_mouse_pos;
		mu_Vec2 mouse_delta;
		mu_Vec2 scroll_delta;
		int mouse_down;
		int mouse_pressed;
		int key_down;
		int key_pressed;
		char input_text[32];
	};


	/**
	 * @brief 2次元ベクトル生成
	 * x, y座標からmu_Vec2型を生成します。
	 * 使い方: mu_vec2(x, y);
	 * @param x X座標
	 * @param y Y座標
	 * @return mu_Vec2 生成されたベクトル
	 */
	mu_Vec2 mu_vec2(int x, int y);

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
	mu_Rect mu_rect(int x, int y, int w, int h);

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
	mu_Color mu_color(int r, int g, int b, int a);

	/**
	 * @brief MicroUIコンテキスト初期化
	 * MicroUIの状態を初期化します。
	 * 使い方: mu_init(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_init(mu_Context* ctx);

	/**
	 * @brief フレーム開始処理
	 * UIフレームの処理を開始します。
	 * 使い方: mu_begin(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_begin(mu_Context* ctx);

	/**
	 * @brief フレーム終了処理
	 * UIフレームの処理を終了し、入力・状態をリセットします。
	 * 使い方: mu_end(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_end(mu_Context* ctx);

	/**
	 * @brief フォーカス設定
	 * 指定IDにフォーカスを設定します。
	 * 使い方: mu_set_focus(ctx, id);
	 * @param ctx MicroUIのコンテキスト
	 * @param id フォーカスを設定するID
	 * @return なし
	 */
	void mu_set_focus(mu_Context* ctx, mu_Id id);

	/**
	 * @brief ID生成
	 * 任意データから一意なIDを生成します。
	 * 使い方: mu_get_id(ctx, data, size);
	 * @param ctx MicroUIのコンテキスト
	 * @param data ID生成用データ
	 * @param size データサイズ
	 * @return mu_Id 生成されたID
	 */
	mu_Id mu_get_id(mu_Context* ctx, const void* data, int size);

	/**
	 * @brief IDスタックにIDをプッシュ
	 * 現在のIDスタックにIDを追加します。
	 * 使い方: mu_push_id(ctx, data, size);
	 * @param ctx MicroUIのコンテキスト
	 * @param data ID生成用データ
	 * @param size データサイズ
	 * @return なし
	 */
	void mu_push_id(mu_Context* ctx, const void* data, int size);

	/**
	 * @brief IDスタックからIDをポップ
	 * IDスタックのトップを削除します。
	 * 使い方: mu_pop_id(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_pop_id(mu_Context* ctx);

	/**
	 * @brief クリップ矩形をプッシュ
	 * クリップ矩形スタックに新しい矩形を追加します。
	 * 使い方: mu_push_clip_rect(ctx, rect);
	 * @param ctx MicroUIのコンテキスト
	 * @param rect クリップ矩形
	 * @return なし
	 */
	void mu_push_clip_rect(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief クリップ矩形をポップ
	 * クリップ矩形スタックのトップを削除します。
	 * 使い方: mu_pop_clip_rect(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_pop_clip_rect(mu_Context* ctx);

	/**
	 * @brief 現在のクリップ矩形取得
	 * クリップ矩形スタックのトップを取得します。
	 * 使い方: mu_get_clip_rect(ctx);
	 * @param ctx MicroUIのコンテキスト
	 * @return mu_Rect 現在のクリップ矩形
	 */
	mu_Rect mu_get_clip_rect(mu_Context* ctx);

	/**
	 * @brief クリップ判定
	 * 指定矩形がクリップ範囲内か判定します。
	 * 使い方: mu_check_clip(ctx, rect);
	 * @param ctx MicroUIのコンテキスト
	 * @param rect 判定する矩形
	 * @return int クリップ状態（MU_CLIP_ALL, MU_CLIP_PART, 0）
	 */
	int mu_check_clip(mu_Context* ctx, mu_Rect r);
	mu_Container* mu_get_current_container(mu_Context* ctx);
	/**
 * @brief コンテナ取得
 * 名前を指定してコンテナを取得します。
 * 使い方: mu_get_container(ctx, "コンテナ名");
 * @param ctx MicroUIのコンテキスト
 * @param name コンテナ名
 * @return mu_Container* コンテナへのポインタ
 */
	mu_Container* mu_get_container(mu_Context* ctx, const char* name);

	/**
	 * @brief コンテナの最前面化
	 * 指定したコンテナを最前面に持ってきます。
	 * 使い方: mu_bring_to_front(ctx, cnt);
	 * @param ctx MicroUIのコンテキスト
	 * @param cnt コンテナへのポインタ
	 * @return なし
	 */
	void mu_bring_to_front(mu_Context* ctx, mu_Container* cnt);

	int mu_pool_init(mu_Context* ctx, mu_PoolItem* items, int len, mu_Id id);
	int mu_pool_get(mu_Context* ctx, mu_PoolItem* items, int len, mu_Id id);
	void mu_pool_update(mu_Context* ctx, mu_PoolItem* items, int idx);

	void mu_input_mousemove(mu_Context* ctx, int x, int y);
	void mu_input_mousedown(mu_Context* ctx, int x, int y, int btn);
	void mu_input_mouseup(mu_Context* ctx, int x, int y, int btn);
	void mu_input_scroll(mu_Context* ctx, int x, int y);
	void mu_input_keydown(mu_Context* ctx, int key);
	void mu_input_keyup(mu_Context* ctx, int key);
	void mu_input_text(mu_Context* ctx, const char* text);

	mu_Command* mu_push_command(mu_Context* ctx, int type, int size);

	/**
	 * @brief コマンドリストの次のコマンドを取得する
	 * コマンドリストを走査し、次の有効なコマンド（JUMP以外）を取得します。
	 * JUMPコマンドの場合はジャンプ先に移動します。
	 * @param ctx MicroUIのコンテキスト
	 * @param cmd コマンドへのポインタ（NULLの場合は先頭から）
	 * @return int 有効なコマンドがあれば1、末尾なら0
	 */
	int mu_next_command(mu_Context* ctx, mu_Command** cmd);

	/**
	 * @brief クリップコマンドをコマンドリストに追加する
	 * 指定した矩形領域で描画をクリップ（制限）するコマンドを追加します。
	 * @param ctx MicroUIのコンテキスト
	 * @param rect クリップする矩形領域
	 * @return なし
	 */
	void mu_set_clip(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief 矩形描画コマンドをコマンドリストに追加する
	 * 指定した矩形領域と色で矩形を描画するコマンドを追加します。
	 * クリップ領域と交差している場合のみ追加されます。
	 * @param ctx MicroUIのコンテキスト
	 * @param rect 描画する矩形
	 * @param color 描画色
	 * @return なし
	 */
	void mu_draw_rect(mu_Context* ctx, mu_Rect rect, mu_Color color);

	/**
	 * @brief 枠線描画コマンドをコマンドリストに追加する
	 * 指定した矩形領域の四辺に枠線を描画するコマンドを追加します。
	 * @param ctx MicroUIのコンテキスト
	 * @param rect 枠線を描画する矩形
	 * @param color 枠線色
	 * @return なし
	 */
	void mu_draw_box(mu_Context* ctx, mu_Rect rect, mu_Color color);

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
	void mu_draw_text(mu_Context* ctx, mu_Font font, const char* str, int len, mu_Vec2 pos, mu_Color color);

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
	void mu_draw_icon(mu_Context* ctx, int id, mu_Rect rect, mu_Color color);

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
	void mu_layout_row(mu_Context* ctx, int items, const int* widths, int height);

	/**
	 * @brief レイアウトの幅を設定
	 * 現在のレイアウトの幅（次のアイテムの幅）を指定します。
	 * @param ctx MicroUIのコンテキスト
	 * @param width 幅
	 * @return なし
	 */
	void mu_layout_width(mu_Context* ctx, int width);

	/**
	 * @brief レイアウトの高さを設定
	 * 現在のレイアウトの高さ（次のアイテムの高さ）を指定します。
	 * @param ctx MicroUIのコンテキスト
	 * @param height 高さ
	 * @return なし
	 */
	void mu_layout_height(mu_Context* ctx, int height);

	/**
	 * @brief カラムレイアウトの開始
	 * 現在のレイアウト位置に新しいカラム（縦並びレイアウト）を開始します。
	 * カラム内の要素は縦方向に積み重ねられます。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_layout_begin_column(mu_Context* ctx);

	/**
	 * @brief カラムレイアウトの終了
	 * 現在のカラムレイアウトを終了し、親レイアウトに最大位置や行情報を反映します。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_layout_end_column(mu_Context* ctx);

	/**
	 * @brief 次のアイテムのレイアウト位置を設定
	 * 次のアイテムの位置・サイズを絶対または相対で指定します。
	 * @param ctx MicroUIのコンテキスト
	 * @param r 指定する矩形
	 * @param relative 相対指定なら1、絶対指定なら0
	 * @return なし
	 */
	void mu_layout_set_next(mu_Context* ctx, mu_Rect r, int relative);

	/**
	 * @brief 次のアイテムのレイアウト矩形を取得
	 * 現在のレイアウト情報から次のアイテムの位置・サイズ（矩形）を計算して返します。
	 * 行のアイテム数や幅・高さ、bodyオフセット、インデントなどを考慮します。
	 * @param ctx MicroUIのコンテキスト
	 * @return mu_Rect 次のアイテムの矩形
	 */
	mu_Rect mu_layout_next(mu_Context* ctx);

	/**
	 * @brief コントロールフレーム描画
	 * コントロールの枠線や背景を描画します。
	 * フォーカス・ホバー状態に応じて色を変更します。
	 * @param ctx MicroUIのコンテキスト
	 * @param id コントロールID
	 * @param rect 描画する矩形
	 * @param colorid 色ID
	 * @param opt オプション
	 */
	void mu_draw_control_frame(mu_Context* ctx, mu_Id id, mu_Rect rect, int colorid, int opt);

	/**
	 * @brief コントロールテキスト描画
	 * コントロール内にテキストを描画します。
	 * 配置オプションに応じてテキスト位置を調整します。
	 * @param ctx MicroUIのコンテキスト
	 * @param str 描画するテキスト
	 * @param rect 描画する矩形
	 * @param colorid テキスト色ID
	 * @param opt 配置オプション
	 */
	void mu_draw_control_text(mu_Context* ctx, const char* str, mu_Rect rect, int colorid, int opt);

	/**
	 * @brief マウスオーバー判定
	 * マウスが指定矩形上にあるか判定します。
	 * @param ctx MicroUIのコンテキスト
	 * @param rect 判定する矩形
	 * @return int マウスが矩形上にあれば1、そうでなければ0
	 */
	int mu_mouse_over(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief コントロール状態更新
	 * コントロールのホバーとフォーカス状態を更新します。
	 * @param ctx MicroUIのコンテキスト
	 * @param id コントロールID
	 * @param rect コントロール矩形
	 * @param opt コントロールオプション
	 * @return なし
	 */
	void mu_update_control(mu_Context* ctx, mu_Id id, mu_Rect rect, int opt);

#define mu_button(ctx, label)             mu_button_ex(ctx, label, 0, MU_OPT_ALIGNCENTER)
#define mu_textbox(ctx, buf, bufsz)       mu_textbox_ex(ctx, buf, bufsz, 0)
#define mu_slider(ctx, value, lo, hi)     mu_slider_ex(ctx, value, lo, hi, 0, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_number(ctx, value, step)       mu_number_ex(ctx, value, step, MU_SLIDER_FMT, MU_OPT_ALIGNCENTER)
#define mu_header(ctx, label)             mu_header_ex(ctx, label, 0)
#define mu_begin_treenode(ctx, label)     mu_begin_treenode_ex(ctx, label, 0)
#define mu_begin_window(ctx, title, rect) mu_begin_window_ex(ctx, title, rect, 0)
#define mu_begin_panel(ctx, name)         mu_begin_panel_ex(ctx, name, 0)

	/**
	 * @brief テキスト描画
	 * 複数行のテキストを描画します。自動的に折り返します。
	 * @param ctx MicroUIのコンテキスト
	 * @param text 描画するテキスト
	 * @return なし
	 */
	void mu_text(mu_Context* ctx, const char* text);

	/**
	 * @brief ラベル描画
	 * 単一行のラベルテキストを描画します。
	 * @param ctx MicroUIのコンテキスト
	 * @param text 描画するテキスト
	 * @return なし
	 */
	void mu_label(mu_Context* ctx, const char* text);

	/**
	 * @brief 拡張ボタン描画・押下判定
	 * テキストまたはアイコン付きのボタンを描画し、押下判定を行います。
	 * @param ctx MicroUIのコンテキスト
	 * @param label ボタンラベル（NULLの場合はアイコンのみ）
	 * @param icon アイコンID（0の場合はテキストのみ）
	 * @param opt ボタンオプション
	 * @return int 押されたらMU_RES_SUBMIT、そうでなければ0
	 */
	int mu_button_ex(mu_Context* ctx, const char* label, int icon, int opt);

	/**
	 * @brief チェックボックス描画・状態更新
	 * チェックボックスを描画し、状態を更新します。
	 * @param ctx MicroUIのコンテキスト
	 * @param label チェックボックスラベル
	 * @param state チェック状態を格納する変数へのポインタ
	 * @return int 状態が変化したらMU_RES_CHANGE、そうでなければ0
	 */
	int mu_checkbox(mu_Context* ctx, const char* label, int* state);

	/**
	 * @brief テキストボックス描画・入力処理（基本関数）
	 * テキストボックスを描画し、テキスト入力処理を行います。
	 * @param ctx MicroUIのコンテキスト
	 * @param buf 入力テキスト格納バッファ
	 * @param bufsz バッファサイズ
	 * @param id テキストボックスID
	 * @param r テキストボックス矩形
	 * @param opt オプション
	 * @return int 入力変化時MU_RES_CHANGE、確定時MU_RES_SUBMIT
	 */
	int mu_textbox_raw(mu_Context* ctx, char* buf, int bufsz, mu_Id id, mu_Rect r, int opt);

	/**
	 * @brief テキストボックス描画・入力処理（拡張関数）
	 * テキストボックスを描画し、テキスト入力処理を行います。
	 * @param ctx MicroUIのコンテキスト
	 * @param buf 入力テキスト格納バッファ
	 * @param bufsz バッファサイズ
	 * @param opt オプション
	 * @return int 入力変化時MU_RES_CHANGE、確定時MU_RES_SUBMIT
	 */
	int mu_textbox_ex(mu_Context* ctx, char* buf, int bufsz, int opt);

	/**
	 * @brief スライダー描画・値取得
	 * スライダーを描画し、値を取得します。
	 * @param ctx MicroUIのコンテキスト
	 * @param value スライダー値（mu_Real*）
	 * @param low 最小値
	 * @param high 最大値
	 * @param step ステップ値
	 * @param fmt 表示フォーマット
	 * @param opt オプション
	 * @return int 値変化時MU_RES_CHANGE
	 */
	int mu_slider_ex(mu_Context* ctx, mu_Real* value, mu_Real low, mu_Real high, mu_Real step, const char* fmt, int opt);

	/**
	 * @brief 数値入力描画・値取得
	 * 数値入力コントロールを描画し、値を取得します。
	 * @param ctx MicroUIのコンテキスト
	 * @param value 数値値（mu_Real*）
	 * @param step ステップ値
	 * @param fmt 表示フォーマット
	 * @param opt オプション
	 * @return int 値変化時MU_RES_CHANGE
	 */
	int mu_number_ex(mu_Context* ctx, mu_Real* value, mu_Real step, const char* fmt, int opt);

	/**
	 * @brief ヘッダー描画・状態管理
	 * 折りたたみ可能なヘッダーを描画します。
	 * @param ctx MicroUIのコンテキスト
	 * @param label ヘッダーテキスト
	 * @param opt オプション
	 * @return int 展開されていればMU_RES_ACTIVE、そうでなければ0
	 */
	int mu_header_ex(mu_Context* ctx, const char* label, int opt);

	/**
	 * @brief ツリーノード開始
	 * 折りたたみ可能なツリーノードを開始します。
	 * @param ctx MicroUIのコンテキスト
	 * @param label ノードラベル
	 * @param opt オプション
	 * @return int 展開されていればMU_RES_ACTIVE、そうでなければ0
	 */
	int mu_begin_treenode_ex(mu_Context* ctx, const char* label, int opt);

	/**
	 * @brief ツリーノード終了
	 * mu_begin_treenodeで開始したツリーノードを終了します。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_end_treenode(mu_Context* ctx);

	/**
	 * @brief ウィンドウ開始関数
	 * MicroUIのウィンドウを開始し、描画・管理を行います。
	 * @param ctx MicroUIのコンテキスト
	 * @param title ウィンドウタイトル文字列
	 * @param rect ウィンドウの位置とサイズ
	 * @param opt ウィンドウの動作オプション
	 * @return int ウィンドウがアクティブならMU_RES_ACTIVE、非アクティブなら0
	 */
	int mu_begin_window_ex(mu_Context* ctx, const char* title, mu_Rect rect, int opt);

	/**
	 * @brief ウィンドウ終了関数
	 * mu_begin_window_exで開始したウィンドウの処理を終了します。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_end_window(mu_Context* ctx);

	/**
	 * @brief ポップアップウィンドウを開く
	 * 指定した名前のポップアップウィンドウをマウス位置に表示します。
	 * @param ctx MicroUIのコンテキスト
	 * @param name ポップアップウィンドウ名
	 * @return なし
	 */
	void mu_open_popup(mu_Context* ctx, const char* name);

	/**
	 * @brief ポップアップウィンドウ開始関数
	 * ポップアップウィンドウの描画・管理を開始します。
	 * @param ctx MicroUIのコンテキスト
	 * @param name ポップアップウィンドウ名
	 * @return int ポップアップがアクティブならMU_RES_ACTIVE、非アクティブなら0
	 */
	int mu_begin_popup(mu_Context* ctx, const char* name);

	/**
	 * @brief ポップアップウィンドウ終了関数
	 * mu_begin_popupで開始したポップアップウィンドウの処理を終了します。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_end_popup(mu_Context* ctx);

	/**
	 * @brief パネル開始関数
	 * パネルの描画・管理を開始します。
	 * @param ctx MicroUIのコンテキスト
	 * @param name パネル名
	 * @param opt パネルの動作オプション
	 * @return なし
	 */
	void mu_begin_panel_ex(mu_Context* ctx, const char* name, int opt);

	/**
	 * @brief パネル終了関数
	 * mu_begin_panel_exで開始したパネルの処理を終了します。
	 * @param ctx MicroUIのコンテキスト
	 * @return なし
	 */
	void mu_end_panel(mu_Context* ctx);

#ifdef __cplusplus
}
#endif

#endif
