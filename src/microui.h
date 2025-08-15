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
		void (*draw_text)(mu_Context* ctx, mu_Font font, const char* str, int len, mu_Vec2 pos, mu_Color color); // �ǉ�

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
	 * @brief 2�����x�N�g������
	 * x, y���W����mu_Vec2�^�𐶐����܂��B
	 * �g����: mu_vec2(x, y);
	 * @param x X���W
	 * @param y Y���W
	 * @return mu_Vec2 �������ꂽ�x�N�g��
	 */
	mu_Vec2 mu_vec2(int x, int y);

	/**
	 * @brief ��`����
	 * x, y, w, h����mu_Rect�^�𐶐����܂��B
	 * �g����: mu_rect(x, y, w, h);
	 * @param x X���W
	 * @param y Y���W
	 * @param w ��
	 * @param h ����
	 * @return mu_Rect �������ꂽ��`
	 */
	mu_Rect mu_rect(int x, int y, int w, int h);

	/**
	 * @brief �F����
	 * RGBA�l����mu_Color�^�𐶐����܂��B
	 * �g����: mu_color(r, g, b, a);
	 * @param r ��
	 * @param g ��
	 * @param b ��
	 * @param a �A���t�@
	 * @return mu_Color �������ꂽ�F
	 */
	mu_Color mu_color(int r, int g, int b, int a);

	/**
	 * @brief MicroUI�R���e�L�X�g������
	 * MicroUI�̏�Ԃ����������܂��B
	 * �g����: mu_init(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_init(mu_Context* ctx);

	/**
	 * @brief �t���[���J�n����
	 * UI�t���[���̏������J�n���܂��B
	 * �g����: mu_begin(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_begin(mu_Context* ctx);

	/**
	 * @brief �t���[���I������
	 * UI�t���[���̏������I�����A���́E��Ԃ����Z�b�g���܂��B
	 * �g����: mu_end(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_end(mu_Context* ctx);

	/**
	 * @brief �t�H�[�J�X�ݒ�
	 * �w��ID�Ƀt�H�[�J�X��ݒ肵�܂��B
	 * �g����: mu_set_focus(ctx, id);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param id �t�H�[�J�X��ݒ肷��ID
	 * @return �Ȃ�
	 */
	void mu_set_focus(mu_Context* ctx, mu_Id id);

	/**
	 * @brief ID����
	 * �C�Ӄf�[�^�����ӂ�ID�𐶐����܂��B
	 * �g����: mu_get_id(ctx, data, size);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param data ID�����p�f�[�^
	 * @param size �f�[�^�T�C�Y
	 * @return mu_Id �������ꂽID
	 */
	mu_Id mu_get_id(mu_Context* ctx, const void* data, int size);

	/**
	 * @brief ID�X�^�b�N��ID���v�b�V��
	 * ���݂�ID�X�^�b�N��ID��ǉ����܂��B
	 * �g����: mu_push_id(ctx, data, size);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param data ID�����p�f�[�^
	 * @param size �f�[�^�T�C�Y
	 * @return �Ȃ�
	 */
	void mu_push_id(mu_Context* ctx, const void* data, int size);

	/**
	 * @brief ID�X�^�b�N����ID���|�b�v
	 * ID�X�^�b�N�̃g�b�v���폜���܂��B
	 * �g����: mu_pop_id(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_pop_id(mu_Context* ctx);

	/**
	 * @brief �N���b�v��`���v�b�V��
	 * �N���b�v��`�X�^�b�N�ɐV������`��ǉ����܂��B
	 * �g����: mu_push_clip_rect(ctx, rect);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect �N���b�v��`
	 * @return �Ȃ�
	 */
	void mu_push_clip_rect(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief �N���b�v��`���|�b�v
	 * �N���b�v��`�X�^�b�N�̃g�b�v���폜���܂��B
	 * �g����: mu_pop_clip_rect(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_pop_clip_rect(mu_Context* ctx);

	/**
	 * @brief ���݂̃N���b�v��`�擾
	 * �N���b�v��`�X�^�b�N�̃g�b�v���擾���܂��B
	 * �g����: mu_get_clip_rect(ctx);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return mu_Rect ���݂̃N���b�v��`
	 */
	mu_Rect mu_get_clip_rect(mu_Context* ctx);

	/**
	 * @brief �N���b�v����
	 * �w���`���N���b�v�͈͓������肵�܂��B
	 * �g����: mu_check_clip(ctx, rect);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect ���肷���`
	 * @return int �N���b�v��ԁiMU_CLIP_ALL, MU_CLIP_PART, 0�j
	 */
	int mu_check_clip(mu_Context* ctx, mu_Rect r);
	mu_Container* mu_get_current_container(mu_Context* ctx);
	/**
 * @brief �R���e�i�擾
 * ���O���w�肵�ăR���e�i���擾���܂��B
 * �g����: mu_get_container(ctx, "�R���e�i��");
 * @param ctx MicroUI�̃R���e�L�X�g
 * @param name �R���e�i��
 * @return mu_Container* �R���e�i�ւ̃|�C���^
 */
	mu_Container* mu_get_container(mu_Context* ctx, const char* name);

	/**
	 * @brief �R���e�i�̍őO�ʉ�
	 * �w�肵���R���e�i���őO�ʂɎ����Ă��܂��B
	 * �g����: mu_bring_to_front(ctx, cnt);
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param cnt �R���e�i�ւ̃|�C���^
	 * @return �Ȃ�
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
	 * @brief �R�}���h���X�g�̎��̃R�}���h���擾����
	 * �R�}���h���X�g�𑖍����A���̗L���ȃR�}���h�iJUMP�ȊO�j���擾���܂��B
	 * JUMP�R�}���h�̏ꍇ�̓W�����v��Ɉړ����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param cmd �R�}���h�ւ̃|�C���^�iNULL�̏ꍇ�͐擪����j
	 * @return int �L���ȃR�}���h�������1�A�����Ȃ�0
	 */
	int mu_next_command(mu_Context* ctx, mu_Command** cmd);

	/**
	 * @brief �N���b�v�R�}���h���R�}���h���X�g�ɒǉ�����
	 * �w�肵����`�̈�ŕ`����N���b�v�i�����j����R�}���h��ǉ����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect �N���b�v�����`�̈�
	 * @return �Ȃ�
	 */
	void mu_set_clip(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief ��`�`��R�}���h���R�}���h���X�g�ɒǉ�����
	 * �w�肵����`�̈�ƐF�ŋ�`��`�悷��R�}���h��ǉ����܂��B
	 * �N���b�v�̈�ƌ������Ă���ꍇ�̂ݒǉ�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect �`�悷���`
	 * @param color �`��F
	 * @return �Ȃ�
	 */
	void mu_draw_rect(mu_Context* ctx, mu_Rect rect, mu_Color color);

	/**
	 * @brief �g���`��R�}���h���R�}���h���X�g�ɒǉ�����
	 * �w�肵����`�̈�̎l�ӂɘg����`�悷��R�}���h��ǉ����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect �g����`�悷���`
	 * @param color �g���F
	 * @return �Ȃ�
	 */
	void mu_draw_box(mu_Context* ctx, mu_Rect rect, mu_Color color);

	/**
	 * @brief �e�L�X�g�`��R�}���h���R�}���h���X�g�ɒǉ�����
	 * �w�肵���ʒu�E�t�H���g�E�F�Ńe�L�X�g��`�悷��R�}���h��ǉ����܂��B
	 * �N���b�v�̈�ɉ����ăN���b�v�R�}���h���ǉ�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param font �t�H���g
	 * @param str �`�悷�镶����
	 * @param len �����񒷁i-1�Ȃ玩������j
	 * @param pos �`��ʒu
	 * @param color �`��F
	 * @return �Ȃ�
	 */
	void mu_draw_text(mu_Context* ctx, mu_Font font, const char* str, int len, mu_Vec2 pos, mu_Color color);

	/**
	 * @brief �A�C�R���`��R�}���h���R�}���h���X�g�ɒǉ�����
	 * �w�肵���ʒu�E�T�C�Y�E�F�ŃA�C�R����`�悷��R�}���h��ǉ����܂��B
	 * �N���b�v�̈�ɉ����ăN���b�v�R�}���h���ǉ�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param id �A�C�R��ID
	 * @param rect �`�悷���`
	 * @param color �`��F
	 * @return �Ȃ�
	 */
	void mu_draw_icon(mu_Context* ctx, int id, mu_Rect rect, mu_Color color);

	/**
	 * @brief ���C�A�E�g�s�̐ݒ�
	 * ���݂̃��C�A�E�g�ɐV�����s��ݒ肵�܂��B
	 * �s���̃A�C�e������e�A�C�e���̕��A�������w��ł��܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param items �s���̃A�C�e����
	 * @param widths �e�A�C�e���̕��iNULL�̏ꍇ�͎����j
	 * @param height �s�̍���
	 * @return �Ȃ�
	 */
	void mu_layout_row(mu_Context* ctx, int items, const int* widths, int height);

	/**
	 * @brief ���C�A�E�g�̕���ݒ�
	 * ���݂̃��C�A�E�g�̕��i���̃A�C�e���̕��j���w�肵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param width ��
	 * @return �Ȃ�
	 */
	void mu_layout_width(mu_Context* ctx, int width);

	/**
	 * @brief ���C�A�E�g�̍�����ݒ�
	 * ���݂̃��C�A�E�g�̍����i���̃A�C�e���̍����j���w�肵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param height ����
	 * @return �Ȃ�
	 */
	void mu_layout_height(mu_Context* ctx, int height);

	/**
	 * @brief �J�������C�A�E�g�̊J�n
	 * ���݂̃��C�A�E�g�ʒu�ɐV�����J�����i�c���у��C�A�E�g�j���J�n���܂��B
	 * �J�������̗v�f�͏c�����ɐςݏd�˂��܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_layout_begin_column(mu_Context* ctx);

	/**
	 * @brief �J�������C�A�E�g�̏I��
	 * ���݂̃J�������C�A�E�g���I�����A�e���C�A�E�g�ɍő�ʒu��s���𔽉f���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_layout_end_column(mu_Context* ctx);

	/**
	 * @brief ���̃A�C�e���̃��C�A�E�g�ʒu��ݒ�
	 * ���̃A�C�e���̈ʒu�E�T�C�Y���΂܂��͑��΂Ŏw�肵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param r �w�肷���`
	 * @param relative ���Ύw��Ȃ�1�A��Ύw��Ȃ�0
	 * @return �Ȃ�
	 */
	void mu_layout_set_next(mu_Context* ctx, mu_Rect r, int relative);

	/**
	 * @brief ���̃A�C�e���̃��C�A�E�g��`���擾
	 * ���݂̃��C�A�E�g��񂩂玟�̃A�C�e���̈ʒu�E�T�C�Y�i��`�j���v�Z���ĕԂ��܂��B
	 * �s�̃A�C�e�����╝�E�����Abody�I�t�Z�b�g�A�C���f���g�Ȃǂ��l�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return mu_Rect ���̃A�C�e���̋�`
	 */
	mu_Rect mu_layout_next(mu_Context* ctx);

	/**
	 * @brief �R���g���[���t���[���`��
	 * �R���g���[���̘g����w�i��`�悵�܂��B
	 * �t�H�[�J�X�E�z�o�[��Ԃɉ����ĐF��ύX���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param id �R���g���[��ID
	 * @param rect �`�悷���`
	 * @param colorid �FID
	 * @param opt �I�v�V����
	 */
	void mu_draw_control_frame(mu_Context* ctx, mu_Id id, mu_Rect rect, int colorid, int opt);

	/**
	 * @brief �R���g���[���e�L�X�g�`��
	 * �R���g���[�����Ƀe�L�X�g��`�悵�܂��B
	 * �z�u�I�v�V�����ɉ����ăe�L�X�g�ʒu�𒲐����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param str �`�悷��e�L�X�g
	 * @param rect �`�悷���`
	 * @param colorid �e�L�X�g�FID
	 * @param opt �z�u�I�v�V����
	 */
	void mu_draw_control_text(mu_Context* ctx, const char* str, mu_Rect rect, int colorid, int opt);

	/**
	 * @brief �}�E�X�I�[�o�[����
	 * �}�E�X���w���`��ɂ��邩���肵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param rect ���肷���`
	 * @return int �}�E�X����`��ɂ����1�A�����łȂ����0
	 */
	int mu_mouse_over(mu_Context* ctx, mu_Rect rect);

	/**
	 * @brief �R���g���[����ԍX�V
	 * �R���g���[���̃z�o�[�ƃt�H�[�J�X��Ԃ��X�V���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param id �R���g���[��ID
	 * @param rect �R���g���[����`
	 * @param opt �R���g���[���I�v�V����
	 * @return �Ȃ�
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
	 * @brief �e�L�X�g�`��
	 * �����s�̃e�L�X�g��`�悵�܂��B�����I�ɐ܂�Ԃ��܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param text �`�悷��e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_text(mu_Context* ctx, const char* text);

	/**
	 * @brief ���x���`��
	 * �P��s�̃��x���e�L�X�g��`�悵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param text �`�悷��e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_label(mu_Context* ctx, const char* text);

	/**
	 * @brief �g���{�^���`��E��������
	 * �e�L�X�g�܂��̓A�C�R���t���̃{�^����`�悵�A����������s���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param label �{�^�����x���iNULL�̏ꍇ�̓A�C�R���̂݁j
	 * @param icon �A�C�R��ID�i0�̏ꍇ�̓e�L�X�g�̂݁j
	 * @param opt �{�^���I�v�V����
	 * @return int �����ꂽ��MU_RES_SUBMIT�A�����łȂ����0
	 */
	int mu_button_ex(mu_Context* ctx, const char* label, int icon, int opt);

	/**
	 * @brief �`�F�b�N�{�b�N�X�`��E��ԍX�V
	 * �`�F�b�N�{�b�N�X��`�悵�A��Ԃ��X�V���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param label �`�F�b�N�{�b�N�X���x��
	 * @param state �`�F�b�N��Ԃ��i�[����ϐ��ւ̃|�C���^
	 * @return int ��Ԃ��ω�������MU_RES_CHANGE�A�����łȂ����0
	 */
	int mu_checkbox(mu_Context* ctx, const char* label, int* state);

	/**
	 * @brief �e�L�X�g�{�b�N�X�`��E���͏����i��{�֐��j
	 * �e�L�X�g�{�b�N�X��`�悵�A�e�L�X�g���͏������s���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param buf ���̓e�L�X�g�i�[�o�b�t�@
	 * @param bufsz �o�b�t�@�T�C�Y
	 * @param id �e�L�X�g�{�b�N�XID
	 * @param r �e�L�X�g�{�b�N�X��`
	 * @param opt �I�v�V����
	 * @return int ���͕ω���MU_RES_CHANGE�A�m�莞MU_RES_SUBMIT
	 */
	int mu_textbox_raw(mu_Context* ctx, char* buf, int bufsz, mu_Id id, mu_Rect r, int opt);

	/**
	 * @brief �e�L�X�g�{�b�N�X�`��E���͏����i�g���֐��j
	 * �e�L�X�g�{�b�N�X��`�悵�A�e�L�X�g���͏������s���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param buf ���̓e�L�X�g�i�[�o�b�t�@
	 * @param bufsz �o�b�t�@�T�C�Y
	 * @param opt �I�v�V����
	 * @return int ���͕ω���MU_RES_CHANGE�A�m�莞MU_RES_SUBMIT
	 */
	int mu_textbox_ex(mu_Context* ctx, char* buf, int bufsz, int opt);

	/**
	 * @brief �X���C�_�[�`��E�l�擾
	 * �X���C�_�[��`�悵�A�l���擾���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param value �X���C�_�[�l�imu_Real*�j
	 * @param low �ŏ��l
	 * @param high �ő�l
	 * @param step �X�e�b�v�l
	 * @param fmt �\���t�H�[�}�b�g
	 * @param opt �I�v�V����
	 * @return int �l�ω���MU_RES_CHANGE
	 */
	int mu_slider_ex(mu_Context* ctx, mu_Real* value, mu_Real low, mu_Real high, mu_Real step, const char* fmt, int opt);

	/**
	 * @brief ���l���͕`��E�l�擾
	 * ���l���̓R���g���[����`�悵�A�l���擾���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param value ���l�l�imu_Real*�j
	 * @param step �X�e�b�v�l
	 * @param fmt �\���t�H�[�}�b�g
	 * @param opt �I�v�V����
	 * @return int �l�ω���MU_RES_CHANGE
	 */
	int mu_number_ex(mu_Context* ctx, mu_Real* value, mu_Real step, const char* fmt, int opt);

	/**
	 * @brief �w�b�_�[�`��E��ԊǗ�
	 * �܂肽���݉\�ȃw�b�_�[��`�悵�܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param label �w�b�_�[�e�L�X�g
	 * @param opt �I�v�V����
	 * @return int �W�J����Ă����MU_RES_ACTIVE�A�����łȂ����0
	 */
	int mu_header_ex(mu_Context* ctx, const char* label, int opt);

	/**
	 * @brief �c���[�m�[�h�J�n
	 * �܂肽���݉\�ȃc���[�m�[�h���J�n���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param label �m�[�h���x��
	 * @param opt �I�v�V����
	 * @return int �W�J����Ă����MU_RES_ACTIVE�A�����łȂ����0
	 */
	int mu_begin_treenode_ex(mu_Context* ctx, const char* label, int opt);

	/**
	 * @brief �c���[�m�[�h�I��
	 * mu_begin_treenode�ŊJ�n�����c���[�m�[�h���I�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_end_treenode(mu_Context* ctx);

	/**
	 * @brief �E�B���h�E�J�n�֐�
	 * MicroUI�̃E�B���h�E���J�n���A�`��E�Ǘ����s���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param title �E�B���h�E�^�C�g��������
	 * @param rect �E�B���h�E�̈ʒu�ƃT�C�Y
	 * @param opt �E�B���h�E�̓���I�v�V����
	 * @return int �E�B���h�E���A�N�e�B�u�Ȃ�MU_RES_ACTIVE�A��A�N�e�B�u�Ȃ�0
	 */
	int mu_begin_window_ex(mu_Context* ctx, const char* title, mu_Rect rect, int opt);

	/**
	 * @brief �E�B���h�E�I���֐�
	 * mu_begin_window_ex�ŊJ�n�����E�B���h�E�̏������I�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_end_window(mu_Context* ctx);

	/**
	 * @brief �|�b�v�A�b�v�E�B���h�E���J��
	 * �w�肵�����O�̃|�b�v�A�b�v�E�B���h�E���}�E�X�ʒu�ɕ\�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param name �|�b�v�A�b�v�E�B���h�E��
	 * @return �Ȃ�
	 */
	void mu_open_popup(mu_Context* ctx, const char* name);

	/**
	 * @brief �|�b�v�A�b�v�E�B���h�E�J�n�֐�
	 * �|�b�v�A�b�v�E�B���h�E�̕`��E�Ǘ����J�n���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param name �|�b�v�A�b�v�E�B���h�E��
	 * @return int �|�b�v�A�b�v���A�N�e�B�u�Ȃ�MU_RES_ACTIVE�A��A�N�e�B�u�Ȃ�0
	 */
	int mu_begin_popup(mu_Context* ctx, const char* name);

	/**
	 * @brief �|�b�v�A�b�v�E�B���h�E�I���֐�
	 * mu_begin_popup�ŊJ�n�����|�b�v�A�b�v�E�B���h�E�̏������I�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_end_popup(mu_Context* ctx);

	/**
	 * @brief �p�l���J�n�֐�
	 * �p�l���̕`��E�Ǘ����J�n���܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @param name �p�l����
	 * @param opt �p�l���̓���I�v�V����
	 * @return �Ȃ�
	 */
	void mu_begin_panel_ex(mu_Context* ctx, const char* name, int opt);

	/**
	 * @brief �p�l���I���֐�
	 * mu_begin_panel_ex�ŊJ�n�����p�l���̏������I�����܂��B
	 * @param ctx MicroUI�̃R���e�L�X�g
	 * @return �Ȃ�
	 */
	void mu_end_panel(mu_Context* ctx);

#ifdef __cplusplus
}
#endif

#endif
