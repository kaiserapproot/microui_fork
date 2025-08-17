#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <math.h>  // for sinf
#include "renderer.h"
#include "microui.h"
#include "ttf_font.h"
//#include "japanese_atlas.inl"
//#include <stdint.h>  // uint8_t用
#define STB_IMAGE_WRITE_IMPLEMENTATION
// グローバル変数
int width = 1200;
int height = 800;

mu_Context* g_ctx = NULL;
static  char logbuf[64000];
static   int logbuf_updated = 0;
static float bg[4] = { 90, 95, 100,105 };
static int uint8_slider(mu_Context* ctx, unsigned char* value, int low, int high)
{
	int res ;
	static float tmp;
	mu_push_id(ctx, &value, sizeof(value));
	tmp = *value;
	res = mu_slider_ex(ctx, &tmp, low, high, 0, "%.0f", MU_OPT_ALIGNCENTER);
	*value = tmp;
	mu_pop_id(ctx);
	return res;
}

static void write_log(const char* text)
{
	if (logbuf[0]) { strcat(logbuf, "\n"); }
	strcat(logbuf, text);
	logbuf_updated = 1;
}

static void test_window(mu_Context* ctx)
{
	/* do window */
	if (mu_begin_window(ctx, "Demo Window", mu_rect(40, 40, 300, 250)))
	{
		mu_Container* win = mu_get_current_container(ctx);
		win->rect.w = mu_max(win->rect.w, 240);
		win->rect.h = mu_max(win->rect.h, 300);

		/* window info */
		if (mu_header(ctx, "Window Info"))
		{
			char buf[64];
			int layout[] = { 54, -1 };
			mu_Container* win = mu_get_current_container(ctx);

			mu_layout_row(ctx, 2, layout, 0);
			mu_label(ctx, "Position:");
			sprintf(buf, "%d, %d", win->rect.x, win->rect.y); mu_label(ctx, buf);
			mu_label(ctx, "Size:");
			sprintf(buf, "%d, %d", win->rect.w, win->rect.h); mu_label(ctx, buf);
		}

		/* labels + buttons */
		if (mu_header_ex(ctx, "Test Buttons", MU_OPT_EXPANDED))
		{
			int layout[] = { 86, -110, -1 };
			mu_layout_row(ctx, 3, layout, 0);
			mu_label(ctx, "Test buttons 1:");
			if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
			if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
			mu_label(ctx, "Test buttons 2:");
			if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
			if (mu_button(ctx, "Popup")) { mu_open_popup(ctx, "Test Popup"); }
			if (mu_begin_popup(ctx, "Test Popup"))
			{
				mu_button(ctx, "Hello");
				mu_button(ctx, "World");
				mu_end_popup(ctx);
			}
		}

		/* tree */
		if (mu_header_ex(ctx, "Tree and Text", MU_OPT_EXPANDED))
		{
			int layout[] = { 140, -1 };
			int layout_single[] = { -1 };
			mu_layout_row(ctx, 2, layout, 0);
			mu_layout_begin_column(ctx);
			if (mu_begin_treenode(ctx, "Test 1"))
			{
				if (mu_begin_treenode(ctx, "Test 1a"))
				{
					mu_label(ctx, "Hello");
					mu_label(ctx, "world");
					mu_end_treenode(ctx);
				}
				if (mu_begin_treenode(ctx, "Test 1b"))
				{
					if (mu_button(ctx, "Button 1")) { write_log("Pressed button 1"); }
					if (mu_button(ctx, "Button 2")) { write_log("Pressed button 2"); }
					mu_end_treenode(ctx);
				}
				if (mu_begin_treenode(ctx, "カ日本語テスト"))
				{
					mu_label(ctx, "こんにちは世界");
					mu_label(ctx, "寿限無寿限無");
					if (mu_button(ctx, "テストボタン")) { write_log("日本語ボタンが押されました"); }
					mu_end_treenode(ctx);
				}
				mu_end_treenode(ctx);
			}
			if (mu_begin_treenode(ctx, "Test 2"))
			{
				int layout_buttons[] = { 54, 54 };
				mu_layout_row(ctx, 2, layout_buttons, 0);
				if (mu_button(ctx, "Button 3")) { write_log("Pressed button 3"); }
				if (mu_button(ctx, "Button 4")) { write_log("Pressed button 4"); }
				if (mu_button(ctx, "Button 5")) { write_log("Pressed button 5"); }
				if (mu_button(ctx, "Button 6")) { write_log("Pressed button 6"); }
				mu_end_treenode(ctx);
			}
			if (mu_begin_treenode(ctx, "Test 3"))
			{
				static int checks[3] = { 1, 0, 1 };
				mu_checkbox(ctx, "Checkbox 1", &checks[0]);
				mu_checkbox(ctx, "Checkbox 2", &checks[1]);
				mu_checkbox(ctx, "Checkbox 3", &checks[2]);
				mu_end_treenode(ctx);
			}
			mu_layout_end_column(ctx);

			mu_layout_begin_column(ctx);
			mu_layout_row(ctx, 1, layout_single, 0);
			mu_text(ctx, "Lorem ipsum dolor sit amet, consectetur adipiscing "
				"elit. Maecenas lacinia, sem eu lacinia molestie, mi risus faucibus "
				"ipsum, eu varius magna felis a nulla.");
			mu_layout_end_column(ctx);
		}

		/* background color sliders */
		if (mu_header_ex(ctx, "Background Color", MU_OPT_EXPANDED))
		{
			char buf[32];
			int sz;
			int layout[] = { -78, -1 };
			int layout_sliders[] = { 46, -1 };
			mu_Rect r;
			mu_Rect resize_rect;
			mu_Container* win ;
			mu_layout_row(ctx, 2, layout, 74);
			/* sliders */
			mu_layout_begin_column(ctx);
			mu_layout_row(ctx, 2, layout_sliders, 0);
			mu_label(ctx, "Red:");   mu_slider(ctx, &bg[0], 0, 255);
			mu_label(ctx, "Green:"); mu_slider(ctx, &bg[1], 0, 255);
			mu_label(ctx, "Blue:");  mu_slider(ctx, &bg[2], 0, 255);
			mu_layout_end_column(ctx);
			/* color preview */
			r = mu_layout_next(ctx);
			mu_draw_rect(ctx, r, mu_color(bg[0], bg[1], bg[2], 255));

			sprintf(buf, "#%02X%02X%02X", (int)bg[0], (int)bg[1], (int)bg[2]);
			mu_draw_control_text(ctx, buf, r, MU_COLOR_TEXT, MU_OPT_ALIGNCENTER);
			// サイズ変更エリアの検出
			win = mu_get_current_container(ctx);
			sz = ctx->style->title_height;
			resize_rect = mu_rect(win->rect.x + win->rect.w - sz, win->rect.y + win->rect.h - sz, sz, sz);
			if (ctx->mouse_pos.x >= resize_rect.x && ctx->mouse_pos.x <= resize_rect.x + resize_rect.w &&
				ctx->mouse_pos.y >= resize_rect.y && ctx->mouse_pos.y <= resize_rect.y + resize_rect.h)
			{
				SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
			}
		}

		mu_end_window(ctx);
	}
}

void style_window(mu_Context* ctx)
{
	static struct { const char* label; int idx; } colors[] = {
	  { "text:",         MU_COLOR_TEXT        },
	  { "border:",       MU_COLOR_BORDER      },
	  { "windowbg:",     MU_COLOR_WINDOWBG    },
	  { "titlebg:",      MU_COLOR_TITLEBG     },
	  { "titletext:",    MU_COLOR_TITLETEXT   },
	  { "panelbg:",      MU_COLOR_PANELBG     },
	  { "button:",       MU_COLOR_BUTTON      },
	  { "buttonhover:",  MU_COLOR_BUTTONHOVER },
	  { "buttonfocus:",  MU_COLOR_BUTTONFOCUS },
	  { "base:",         MU_COLOR_BASE        },
	  { "basehover:",    MU_COLOR_BASEHOVER   },
	  { "basefocus:",    MU_COLOR_BASEFOCUS   },
	  { "scrollbase:",   MU_COLOR_SCROLLBASE  },
	  { "scrollthumb:",  MU_COLOR_SCROLLTHUMB },
	  { NULL,NULL }
	};

	if (mu_begin_window(ctx, "Style Editor", mu_rect(350, 250, 300, 290)))
	{
		//OutputDebugStringA("Style Editor window started\n");
		int i, sz;
		mu_Container* win;
		mu_Rect resize_rect;
		int sw = mu_get_current_container(ctx)->body.w * 0.14;
		int layout[] = { 80, sw, sw, sw, sw, -1 };
		mu_layout_row(ctx, 6, layout, 0);
		for (i = 0; colors[i].label; i++)
		{
			mu_Rect r;
			mu_label(ctx, colors[i].label);
			uint8_slider(ctx, &ctx->style->colors[i].r, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].g, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].b, 0, 255);
			uint8_slider(ctx, &ctx->style->colors[i].a, 0, 255);
			r = mu_layout_next(ctx);
			mu_draw_rect(ctx, r, ctx->style->colors[i]);
		}
		// サイズ変更エリアの検出
		win = mu_get_current_container(ctx);
		sz = ctx->style->title_height;
		resize_rect = mu_rect(win->rect.x + win->rect.w - sz, win->rect.y + win->rect.h - sz, sz, sz);
		if (ctx->mouse_pos.x >= resize_rect.x && ctx->mouse_pos.x <= resize_rect.x + resize_rect.w &&
			ctx->mouse_pos.y >= resize_rect.y && ctx->mouse_pos.y <= resize_rect.y + resize_rect.h)
		{
			SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
		}

		mu_end_window(ctx);
		//OutputDebugStringA("Style Editor window ended\n");
	}
}


// UTF-8文字列を生成するヘルパー関数
static const char* create_utf8_string(int codepoint) {
    static char buffer[8]; // UTF-8は最大4バイト + 終端NUL
    
    if (codepoint <= 0x7F) {
        // 1バイト (ASCII)
        buffer[0] = (char)codepoint;
        buffer[1] = 0;
    } 
    else if (codepoint <= 0x7FF) {
        // 2バイト
        buffer[0] = (char)(0xC0 | (codepoint >> 6));
        buffer[1] = (char)(0x80 | (codepoint & 0x3F));
        buffer[2] = 0;
    } 
    else if (codepoint <= 0xFFFF) {
        // 3バイト (日本語はここ)
        buffer[0] = (char)(0xE0 | (codepoint >> 12));
        buffer[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[2] = (char)(0x80 | (codepoint & 0x3F));
        buffer[3] = 0;
    } 
    else {
        // 4バイト (絵文字など)
        buffer[0] = (char)(0xF0 | (codepoint >> 18));
        buffer[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        buffer[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        buffer[3] = (char)(0x80 | (codepoint & 0x3F));
        buffer[4] = 0;
    }
    
    return buffer;
}

// 空のウィジェット用関数
static void empty_window(mu_Context* ctx) {
    static int init = 0;
    static const char* katakana_a = NULL;
    static const char* katakana_i = NULL;
    if (!katakana_a) {
        katakana_a = create_utf8_string(0x30A2);
        katakana_i = create_utf8_string(0x30A4);
    }
    // カタカナ「ア」を表示する
    if (mu_begin_window(ctx, "こんにちは世界", mu_rect(150, 150, 300, 200))) {
		//wchar_t* wstr = L"こんにちは世界";
		//char utf8str[64];
		//WideCharToMultiByte(CP_UTF8, 0, wstr, -1, utf8str, sizeof(utf8str), NULL, NULL);
		mu_label(ctx, "なんてこった。ホゲホゲ 嗚呼");
		mu_label(ctx, "寿限無じゅべむ");
        mu_end_window(ctx);
    }
}

void process_frame(mu_Context* ctx)
{
     mu_begin(ctx);
     // 既存のウィジェットをコメントアウト
      //style_window(ctx);
      //log_window(ctx);
      //test_window(ctx);
     
     // 代わりに空のウィンドウを表示
    empty_window(ctx);
     
     mu_end(ctx);
}
 static void log_window(mu_Context* ctx)
 {
	 if (mu_begin_window(ctx, "Log Window", mu_rect(350, 40, 300, 200)))
	 {
		 /* output text panel */
		 mu_Container* panel;
		 static char buf[128];
		 int submitted = 0;
		 int layout1[1] = { -1 };
		 int layout2[2] = { -70, -1 };

		 mu_layout_row(ctx, 1, layout1, -25);
		 mu_begin_panel(ctx, "Log Output");
		 panel = mu_get_current_container(ctx);
		 mu_layout_row(ctx, 1, layout1, -1);
		 mu_text(ctx, logbuf);
		 mu_end_panel(ctx);
		 if (logbuf_updated)
		 {
			 panel->scroll.y = panel->content_size.y;
			 logbuf_updated = 0;
		 }

		 /* input textbox + submit button */
		 mu_layout_row(ctx, 2, layout2, 0);
		 if (mu_textbox(ctx, buf, sizeof(buf)) & MU_RES_SUBMIT)
		 {
			 mu_set_focus(ctx, ctx->last_id);
			 submitted = 1;
		 }
		 if (mu_button(ctx, "Submit")) { submitted = 1; }
		 if (submitted)
		 {
			 write_log(buf);
			 buf[0] = '\0';
		 }

		 mu_end_window(ctx);
	 }
 }

// テキスト関連関数
static int text_width(mu_Font font, const char* text, int len)
{
	if (len < 0) len = (int)strlen(text);  // 警告修正
	return r_get_text_width(text, len);
}

static int text_height(mu_Font font)
{
	return r_get_text_height();
}


// ウィンドウプロシージャの修正
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static DWORD lastDebugTime = 0;
	DWORD currentTime = GetTickCount();
	if (g_ctx == NULL)
	{
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}

	switch (uMsg)
	{
	case WM_MOUSEMOVE: {
		float scale_x;
		float scale_y;
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		// 座標変換
		RECT rect;
		GetClientRect(hwnd, &rect);
		scale_x = (float)width / (float)rect.right;
		scale_y = (float)height / (float)rect.bottom;

		x = (int)(x * scale_x);
		y = (int)(y * scale_y);
		mu_input_mousemove(g_ctx, x, y);
		InvalidateRect(hwnd, NULL, TRUE);  // 再描画要求
		return 0;
	}

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);
		r_draw();  // フレーム描画
		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_LBUTTONDOWN: {
		float scale_x;
		float scale_y;
		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		// 座標変換
		RECT rect;
		GetClientRect(hwnd, &rect);
		scale_x = (float)width / (float)rect.right;
		scale_y = (float)height / (float)rect.bottom;

		x = (int)(x * scale_x);
		y = (int)(y * scale_y);
		mu_input_mousedown(g_ctx, x, y, MU_MOUSE_LEFT);
		SetCapture(hwnd);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}

	case WM_LBUTTONUP: {
		RECT rect;
		float scale_x;
		float scale_y;

		int x = LOWORD(lParam);
		int y = HIWORD(lParam);
		// 座標変換
		GetClientRect(hwnd, &rect);
		scale_x = (float)width / (float)rect.right;
		scale_y = (float)height / (float)rect.bottom;

		x = (int)(x * scale_x);
		y = (int)(y * scale_y);
		mu_input_mouseup(g_ctx, x, y, MU_MOUSE_LEFT);
		ReleaseCapture();
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}

	case WM_KEYDOWN: {
		mu_input_keydown(g_ctx, wParam);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}

	case WM_CHAR: {
		char text[2] = { (char)wParam, '\0' };
		mu_input_text(g_ctx, text);
		InvalidateRect(hwnd, NULL, TRUE);
		return 0;
	}
	case WM_SETCURSOR: {
		// マウスカーソルの変更
		if (LOWORD(lParam) == HTBOTTOMRIGHT)
		{
			SetCursor(LoadCursor(NULL, IDC_SIZENWSE));
			return TRUE;
		}
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		return TRUE;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow)
{
	WNDCLASS wc;
	HWND hwnd;
	const char CLASS_NAME[] = "Sample Window Class";
	MSG msg = { 0 };  // メッセージ構造体を初期化
	// メッセージループ
	DWORD lastTime = GetTickCount();

	// microUIコンテキストの初期化
	g_ctx = malloc(sizeof(mu_Context));
	if (!g_ctx)
	{
		MessageBox(NULL, "Failed to allocate microUI context", "Error", MB_OK);
		return 1;
	}

	memset(g_ctx, 0, sizeof(mu_Context));
	mu_init(g_ctx);

	g_ctx->text_width = text_width;
	g_ctx->text_height = text_height;
	// ウィンドウクラスの設定
	ZeroMemory(&wc, sizeof(wc));
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.lpszClassName = CLASS_NAME;

	RegisterClass(&wc);

	// ウィンドウの作成
	hwnd = CreateWindowEx(
		0,                      // オプションのウィンドウスタイル
		CLASS_NAME,             // ウィンドウクラス
		"DirectX Application",  // ウィンドウテキスト
		WS_OVERLAPPEDWINDOW,   // ウィンドウスタイル
		CW_USEDEFAULT, CW_USEDEFAULT, // 位置
		width, height,   // サイズ
		NULL,       // 親ウィンドウ
		NULL,       // メニュー
		hInstance,  // インスタンスハンドル
		NULL        // 追加のアプリケーションデータ
	);

	if (hwnd == NULL)
	{
		MessageBox(NULL, "Window Creation Failed!", "Error", MB_OK);
		return 0;
	}

	// Direct3Dの初期化
	InitD3D(hwnd);

	// ウィンドウの表示
	ShowWindow(hwnd, nCmdShow);


	while (TRUE)
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				break;
			}
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			DWORD currentTime = GetTickCount();
			if (currentTime - lastTime > 16)
			{  // ~60FPS
				r_draw();
				lastTime = currentTime;
			}
		}
	}

	// クリーンアップ処理
	r_cleanup();
	CleanD3D();
	free(g_ctx);

	return 0;
}