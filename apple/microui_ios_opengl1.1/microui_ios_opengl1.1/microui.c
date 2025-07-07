

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "microui.h"
#if defined(_MSC_VER)
  // Microsoft Visual C++
  typedef __int64 mu_int64;
#else
  // GCC, Clang �Ȃ�
  typedef long long mu_int64;
#endif
// unused�}�N��: �������g�p���Ȃ��ꍇ�Ɍx����}�����邽�߂̃}�N��
#define unused(x) ((void) (x))

// expect�}�N��: �������U�̏ꍇ�ɃG���[���b�Z�[�W���o�͂��A�v���O�������I������A�T�[�V�����}�N��
#define expect(x) do {                                               \
    if (!(x)) {                                                      \
      fprintf(stderr, "Fatal error: %s:%d: assertion '%s' failed\n", \
        __FILE__, __LINE__, #x);                                     \
      abort();                                                       \
    }                                                                \
  } while (0)

// push�}�N��: �X�^�b�N�ɒl���v�b�V������B
// �X�^�b�N�̃T�C�Y�������`�F�b�N���A�l���X�^�b�N�ɒǉ�������A�C���f�b�N�X���C���N�������g���܂��B
// �g�p��:
//   push(ctx->id_stack, id);  // id_stack��id��ǉ�
#define push(stk, val) do {                                                 \
    expect((stk).idx < (int) (sizeof((stk).items) / sizeof(*(stk).items))); \
    (stk).items[(stk).idx] = (val);                                         \
    (stk).idx++; /* `val` �����݂̃C���f�b�N�X���g�p����\�������邽�߁A�C���N�������g�͌�Ŏ��s */ \
  } while (0)

// pop�}�N��: �X�^�b�N����l���|�b�v����B
// �X�^�b�N����łȂ����Ƃ��m�F���A�C���f�b�N�X���f�N�������g���܂��B
// �g�p��:
//   pop(ctx->id_stack);  // id_stack����Ō�̒l���폜
#define pop(stk) do {      \
    expect((stk).idx > 0); \
    (stk).idx--;           \
  } while (0)

// unclipped_rect: �N���b�s���O����Ă��Ȃ���`��\���萔�B
// ���ɑ傫�ȋ�`���g�p���āA�N���b�s���O�𖳌�������p�r�Ŏg�p����܂��B
static mu_Rect unclipped_rect = { 0, 0, 0x1000000, 0x1000000 };

// default_style: �f�t�H���g�̃X�^�C���ݒ��ێ�����\���́B
// �t�H���g�A�T�C�Y�A�p�f�B���O�A�X�y�[�V���O�A�F�Ȃǂ�UI�v�f�̊O�ς��`���܂��B
static mu_Style default_style = {
  /* font | size | padding | spacing | indent */
  NULL, { 68, 10 }, 5, 4, 24,
  /* title_height | scrollbar_size | thumb_size */
  24, 12, 8,
  {
    { 230, 230, 230, 255 }, /* MU_COLOR_TEXT: �e�L�X�g�̐F */
    { 25,  25,  25,  255 }, /* MU_COLOR_BORDER: ���E���̐F */
    { 50,  50,  50,  255 }, /* MU_COLOR_WINDOWBG: �E�B���h�E�w�i�̐F */
    { 25,  25,  25,  255 }, /* MU_COLOR_TITLEBG: �^�C�g���o�[�w�i�̐F */
    { 240, 240, 240, 255 }, /* MU_COLOR_TITLETEXT: �^�C�g���e�L�X�g�̐F */
    { 0,   0,   0,   0   }, /* MU_COLOR_PANELBG: �p�l���w�i�̐F */
    { 75,  75,  75,  255 }, /* MU_COLOR_BUTTON: �{�^���̐F */
    { 95,  95,  95,  255 }, /* MU_COLOR_BUTTONHOVER: �{�^���z�o�[���̐F */
    { 115, 115, 115, 255 }, /* MU_COLOR_BUTTONFOCUS: �{�^���t�H�[�J�X���̐F */
    { 30,  30,  30,  255 }, /* MU_COLOR_BASE: ��{�w�i�̐F */
    { 35,  35,  35,  255 }, /* MU_COLOR_BASEHOVER: ��{�w�i�z�o�[���̐F */
    { 40,  40,  40,  255 }, /* MU_COLOR_BASEFOCUS: ��{�w�i�t�H�[�J�X���̐F */
    { 43,  43,  43,  255 }, /* MU_COLOR_SCROLLBASE: �X�N���[���o�[�̃x�[�X�F */
    { 30,  30,  30,  255 }  /* MU_COLOR_SCROLLTHUMB: �X�N���[���o�[�̃T���F */
  }
};

// mu_vec2: 2�����x�N�g���𐶐�����֐�
// ����: 
//   x - �x�N�g����x���W
//   y - �x�N�g����y���W
// �߂�l: 
//   �w�肳�ꂽx, y���W������mu_Vec2�\����
mu_Vec2 mu_vec2(int x, int y) {
  mu_Vec2 res;
  res.x = x; 
  res.y = y;
  return res;
}

// mu_rect: ��`�𐶐�����֐�
// ����: 
//   x - ��`�̍����x���W
//   y - ��`�̍����y���W
//   w - ��`�̕�
//   h - ��`�̍���
// �߂�l: 
//   �w�肳�ꂽ�ʒu�ƃT�C�Y������mu_Rect�\����
mu_Rect mu_rect(int x, int y, int w, int h) {
  mu_Rect res;
  res.x = x; 
  res.y = y; 
  res.w = w; 
  res.h = h;
  return res;
}

// mu_color: �F�𐶐�����֐�
// ����: 
//   r - �Ԃ̒l (0-255)
//   g - �΂̒l (0-255)
//   b - �̒l (0-255)
//   a - �A���t�@�l (0-255)
// �߂�l: 
//   �w�肳�ꂽRGBA�l������mu_Color�\����
mu_Color mu_color(int r, int g, int b, int a) {
  mu_Color res;
  res.r = r; 
  res.g = g; 
  res.b = b; 
  res.a = a;
  return res;
}

// expand_rect: ��`���g������֐�
// ����: 
//   rect - �g���Ώۂ̋�`
//   n - �g�����镝 (�㉺���E��n�s�N�Z�����g��)
// �߂�l: 
//   �g�����ꂽ��`
static mu_Rect expand_rect(mu_Rect rect, int n) {
  return mu_rect(rect.x - n, rect.y - n, rect.w + n * 2, rect.h + n * 2);
}

// intersect_rects: 2�̋�`�̌����������v�Z����֐�
// ����: 
//   r1 - �ŏ��̋�`
//   r2 - 2�Ԗڂ̋�`
// �߂�l: 
//   ���������̋�` (�������Ȃ��ꍇ�͕��ƍ�����0�̋�`)
static mu_Rect intersect_rects(mu_Rect r1, mu_Rect r2) {
  int x1 = mu_max(r1.x, r2.x);
  int y1 = mu_max(r1.y, r2.y);
  int x2 = mu_min(r1.x + r1.w, r2.x + r2.w);
  int y2 = mu_min(r1.y + r1.h, r2.y + r2.h);
  if (x2 < x1) { x2 = x1; }
  if (y2 < y1) { y2 = y1; }
  return mu_rect(x1, y1, x2 - x1, y2 - y1);
}

// rect_overlaps_vec2: ��`���w�肳�ꂽ�_���܂ނ��𔻒肷��֐�
// ����: 
//   r - ����Ώۂ̋�`
//   p - ����Ώۂ̓_ (mu_Vec2�\����)
// �߂�l: 
//   �_����`���ɂ���ꍇ��1�A�����łȂ��ꍇ��0
static int rect_overlaps_vec2(mu_Rect r, mu_Vec2 p) {
  return p.x >= r.x && p.x < r.x + r.w && p.y >= r.y && p.y < r.y + r.h;
}

// draw_frame: �t���[����`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - �`�悷���`
//   colorid - �g�p����F��ID
// �������e:
//   �w�肳�ꂽ��`���w�肳�ꂽ�F�ŕ`�悵�܂��B
//   ����̐FID�̏ꍇ�͋��E����`�悵�܂���B
static void draw_frame(mu_Context *ctx, mu_Rect rect, int colorid) {
  mu_draw_rect(ctx, rect, ctx->style->colors[colorid]);
  if (colorid == MU_COLOR_SCROLLBASE  ||
      colorid == MU_COLOR_SCROLLTHUMB ||
      colorid == MU_COLOR_TITLEBG) { return; }
  /* ���E����`�� */
  if (ctx->style->colors[MU_COLOR_BORDER].a) {
    mu_draw_box(ctx, expand_rect(rect, 1), ctx->style->colors[MU_COLOR_BORDER]);
  }
}

// mu_init: MicroUI�̃R���e�L�X�g������������֐�
// ����:
//   ctx - ����������MicroUI�̃R���e�L�X�g
// �������e:
//   �R���e�L�X�g�̃��������N���A���A�f�t�H���g�X�^�C����`��֐���ݒ肵�܂��B
void mu_init(mu_Context *ctx) {
  memset(ctx, 0, sizeof(*ctx));
  ctx->draw_frame = draw_frame;
  ctx->_style = default_style;
  ctx->style = &ctx->_style;
}

// mu_begin: �t���[���̊J�n�������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   �e��X�^�b�N�⃊�X�g�����������A�t���[���̏������s���܂��B
//   �K�{�̃e�L�X�g���E�����֐����ݒ肳��Ă��邱�Ƃ��m�F���܂��B
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

// compare_zindex: �R���e�i��Z�C���f�b�N�X���r����֐�
// ����:
//   a - �ŏ��̃R���e�i�ւ̃|�C���^
//   b - 2�Ԗڂ̃R���e�i�ւ̃|�C���^
// �߂�l:
//   Z�C���f�b�N�X�̍���
// �������e:
//   2�̃R���e�i��Z�C���f�b�N�X���r���A���̍���Ԃ��܂��B
static int compare_zindex(const void *a, const void *b) {
  return (*(mu_Container**) a)->zindex - (*(mu_Container**) b)->zindex;
}

// mu_end: �t���[���̏I���������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   �X�^�b�N�̐��������m�F���A�X�N���[����t�H�[�J�X�̏�Ԃ��X�V���܂��B
//   �܂��A���[�g�R���e�i��Z�C���f�b�N�X���Ƀ\�[�g���A�`��R�}���h���œK�����܂��B
void mu_end(mu_Context *ctx) {
  int i, n;

  // �e��X�^�b�N����ł��邱�Ƃ��m�F
  expect(ctx->container_stack.idx == 0);
  expect(ctx->clip_stack.idx == 0);
  expect(ctx->id_stack.idx == 0);
  expect(ctx->layout_stack.idx == 0);

  // �X�N���[�����͂�����
  if (ctx->scroll_target) {
    ctx->scroll_target->scroll.x += ctx->scroll_delta.x;
    ctx->scroll_target->scroll.y += ctx->scroll_delta.y;
  }

  // �t�H�[�J�X���X�V����Ă��Ȃ��ꍇ�A�t�H�[�J�X������
  if (!ctx->updated_focus) {
    ctx->focus = 0;
  }
  ctx->updated_focus = 0;

  // �}�E�X�������ꂽ�ꍇ�A�z�o�[���̃��[�g�R���e�i���őO�ʂɈړ�
  if (ctx->mouse_pressed && ctx->next_hover_root &&
      ctx->next_hover_root->zindex < ctx->last_zindex &&
      ctx->next_hover_root->zindex >= 0) {
    mu_bring_to_front(ctx, ctx->next_hover_root);
  }

  // ���͏�Ԃ����Z�b�g
  ctx->key_pressed = 0;
  ctx->input_text[0] = '\0';
  ctx->mouse_pressed = 0;
  ctx->scroll_delta = mu_vec2(0, 0);
  ctx->last_mouse_pos = ctx->mouse_pos;

  // ���[�g�R���e�i��Z�C���f�b�N�X���Ƀ\�[�g
  n = ctx->root_list.idx;
  qsort(ctx->root_list.items, n, sizeof(mu_Container*), compare_zindex);

  // �e���[�g�R���e�i�̕`��R�}���h��ݒ�
  for (i = 0; i < n; i++) {
    mu_Container *cnt = ctx->root_list.items[i];

    // �ŏ��̃R���e�i�̏ꍇ�A�ŏ��̃R�}���h��ݒ�
    if (i == 0) {
      mu_Command *cmd = (mu_Command*) ctx->command_list.items;
      cmd->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    } else {
      // �O�̃R���e�i�̖��������݂̃R���e�i�̐擪�ɃW�����v������
      mu_Container *prev = ctx->root_list.items[i - 1];
      prev->tail->jump.dst = (char*) cnt->head + sizeof(mu_JumpCommand);
    }

    // �Ō�̃R���e�i�̏ꍇ�A�������R�}���h���X�g�̏I�[�ɃW�����v������
    if (i == n - 1) {
      cnt->tail->jump.dst = ctx->command_list.items + ctx->command_list.idx;
    }
  }
}

// HASH_INITIAL: FNV-1a�n�b�V���̏����l
// 32bit FNV-1a�n�b�V���A���S���Y���Ŏg�p����鏉���l�ł��B
#define HASH_INITIAL 2166136261

// hash: 32bit FNV-1a�n�b�V�����v�Z����֐�
// ����:
//   hash - �n�b�V���l���i�[����|�C���^
//   data - �n�b�V��������f�[�^
//   size - �f�[�^�̃T�C�Y (�o�C�g�P��)
// �������e:
//   FNV-1a�n�b�V���A���S���Y�����g�p���āA�w�肳�ꂽ�f�[�^�̃n�b�V���l���v�Z���܂��B
//   �n�b�V���l�́A�f�[�^�̈�Ӑ������ʂ��邽�߂Ɏg�p����܂��B
static void hash(mu_Id *hash, const void *data, int size) {
  const unsigned char *p = data;
  while (size--) {
    *hash = (*hash ^ *p++) * 16777619; // FNV-1a�A���S���Y���̌v�Z
  }
}

// mu_get_id: �f�[�^�����ӂ�ID�𐶐�����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   data - �n�b�V��������f�[�^
//   size - �f�[�^�̃T�C�Y (�o�C�g�P��)
// �߂�l:
//   �f�[�^�Ɋ�Â��Čv�Z���ꂽ��ӂ�mu_Id
// �������e:
//   �X�^�b�N�̍Ō��ID����ɁA�w�肳�ꂽ�f�[�^�̃n�b�V���l���v�Z���܂��B
//   ����ɂ��A�f�[�^�Ɋ�Â���ӂ�ID����������܂��B
mu_Id mu_get_id(mu_Context *ctx, const void *data, int size) {
  int idx = ctx->id_stack.idx;
  mu_Id res = (idx > 0) ? ctx->id_stack.items[idx - 1] : HASH_INITIAL;
  hash(&res, data, size); // �n�b�V���l���v�Z
  ctx->last_id = res; // �v�Z���ʂ�ۑ�
  return res;
}

// mu_set_focus: �w�肳�ꂽID�Ƀt�H�[�J�X��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   id - �t�H�[�J�X��ݒ肷��Ώۂ�ID
// �������e:
//   �w�肳�ꂽID�����݂̃t�H�[�J�X�Ƃ��Đݒ肵�A�X�V�t���O�𗧂Ă܂��B
void mu_set_focus(mu_Context *ctx, mu_Id id) {
  ctx->focus = id; // �t�H�[�J�X��ݒ�
  ctx->updated_focus = 1; // �X�V�t���O�𗧂Ă�
}

/*
** 32bit FNV-1a�n�b�V���̎g�p���@�ƖړI:
** 
** FNV-1a�n�b�V���́A�y�ʂō����ȃn�b�V���A���S���Y���ł��B
** MicroUI�ł́A�f�[�^�i��: �E�B�W�F�b�g���₻�̑��̎��ʏ��j����ӂɎ��ʂ��邽�߂Ɏg�p����܂��B
** 
** ��̓I�ɂ́A`mu_get_id`�֐��Ńf�[�^�̃n�b�V���l���v�Z���A
** ���̃n�b�V���l��ID�Ƃ��Ďg�p���܂��B����ID�́A�E�B�W�F�b�g��R���e�i�Ȃǂ�
** UI�v�f����ӂɎ��ʂ��邽�߂ɗ��p����܂��B
** 
** �n�b�V�����g�p���邱�ƂŁA�ȉ��̗��_������܂�:
** 1. �f�[�^�̈�Ӑ����ȒP�ɕۏ؂ł���B
** 2. �f�[�^�̔�r���������ł���i�������r�������I�j�B
** 3. �������������ǂ��i�n�b�V���l�͌Œ�T�C�Y�̐����j�B
** 
** ��: 
** - �E�B�W�F�b�g�� "button1" ���n�b�V��������ID�𐶐��B
** - ����ID���g�p���āA�E�B�W�F�b�g�̏�Ԃ�C�x���g���Ǘ��B
*/


// mu_push_id: ID�X�^�b�N�ɐV����ID���v�b�V������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   data - �n�b�V��������f�[�^
//   size - �f�[�^�̃T�C�Y (�o�C�g�P��)
// �������e:
//   �w�肳�ꂽ�f�[�^���n�b�V��������ID�𐶐����AID�X�^�b�N�Ƀv�b�V�����܂��B
void mu_push_id(mu_Context *ctx, const void *data, int size) {
  push(ctx->id_stack, mu_get_id(ctx, data, size));
}

// mu_pop_id: ID�X�^�b�N����ID���|�b�v����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   ID�X�^�b�N�̍Ō��ID���폜���܂��B
void mu_pop_id(mu_Context *ctx) {
  pop(ctx->id_stack);
}

// mu_push_clip_rect: �N���b�s���O��`�X�^�b�N�ɐV������`���v�b�V������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - �v�b�V������N���b�s���O��`
// �������e:
//   ���݂̃N���b�s���O��`�Ǝw�肳�ꂽ��`�̌����������v�Z���A�X�^�b�N�Ƀv�b�V�����܂��B
void mu_push_clip_rect(mu_Context *ctx, mu_Rect rect) {
  mu_Rect last = mu_get_clip_rect(ctx);
  push(ctx->clip_stack, intersect_rects(rect, last));
}

// mu_pop_clip_rect: �N���b�s���O��`�X�^�b�N�����`���|�b�v����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   �N���b�s���O��`�X�^�b�N�̍Ō�̋�`���폜���܂��B
void mu_pop_clip_rect(mu_Context *ctx) {
  pop(ctx->clip_stack);
}

// mu_get_clip_rect: ���݂̃N���b�s���O��`���擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �߂�l:
//   ���݂̃N���b�s���O��`
// �������e:
//   �N���b�s���O��`�X�^�b�N�̍Ō�̋�`��Ԃ��܂��B
mu_Rect mu_get_clip_rect(mu_Context *ctx) {
  expect(ctx->clip_stack.idx > 0);
  return ctx->clip_stack.items[ctx->clip_stack.idx - 1];
}

// mu_check_clip: ��`�����݂̃N���b�s���O��`�Ɋ܂܂�邩�𔻒肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   r - ����Ώۂ̋�`
// �߂�l:
//   MU_CLIP_ALL (���S�ɃN���b�s���O�����ꍇ)
//   0 (�N���b�s���O����Ȃ��ꍇ)
//   MU_CLIP_PART (�����I�ɃN���b�s���O�����ꍇ)
// �������e:
//   �w�肳�ꂽ��`�����݂̃N���b�s���O��`�Ɋ܂܂�邩�𔻒肵�܂��B
int mu_check_clip(mu_Context *ctx, mu_Rect r) {
  mu_Rect cr = mu_get_clip_rect(ctx);
  if (r.x > cr.x + cr.w || r.x + r.w < cr.x ||
      r.y > cr.y + cr.h || r.y + r.h < cr.y) {
    return MU_CLIP_ALL; // ���S�ɃN���b�s���O�����
  }
  if (r.x >= cr.x && r.x + r.w <= cr.x + cr.w &&
      r.y >= cr.y && r.y + r.h <= cr.y + cr.h) {
    return 0; // �N���b�s���O����Ȃ�
  }
  return MU_CLIP_PART; // �����I�ɃN���b�s���O�����
}
// push_layout: ���C�A�E�g�X�^�b�N�ɐV�������C�A�E�g���v�b�V������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   body - ���C�A�E�g�̋�`�̈�
//   scroll - �X�N���[���ʒu
// �������e:
//   ���C�A�E�g�����������A�X�^�b�N�Ƀv�b�V�����܂��B
//   ���C�A�E�g�̏����ʒu��T�C�Y��ݒ肵�܂��B
static void push_layout(mu_Context *ctx, mu_Rect body, mu_Vec2 scroll) {
  mu_Layout layout;
  int width = 0;
  memset(&layout, 0, sizeof(layout));
  layout.body = mu_rect(body.x - scroll.x, body.y - scroll.y, body.w, body.h);
  layout.max = mu_vec2(-0x1000000, -0x1000000);
  push(ctx->layout_stack, layout);
  mu_layout_row(ctx, 1, &width, 0);
}

// get_layout: ���C�A�E�g�X�^�b�N�̌��݂̃��C�A�E�g���擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �߂�l:
//   ���݂̃��C�A�E�g�ւ̃|�C���^
static mu_Layout* get_layout(mu_Context *ctx) {
  return &ctx->layout_stack.items[ctx->layout_stack.idx - 1];
}

// pop_container: �R���e�i�X�^�b�N���猻�݂̃R���e�i���|�b�v����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   ���݂̃R���e�i�̓��e�T�C�Y���v�Z���A�X�^�b�N����폜���܂��B
//   ���C�A�E�g�X�^�b�N��ID�X�^�b�N�������Ƀ|�b�v���܂��B
static void pop_container(mu_Context *ctx) {
  mu_Container *cnt = mu_get_current_container(ctx);
  mu_Layout *layout = get_layout(ctx);
  cnt->content_size.x = layout->max.x - layout->body.x;
  cnt->content_size.y = layout->max.y - layout->body.y;
  pop(ctx->container_stack);
  pop(ctx->layout_stack);
  mu_pop_id(ctx);
}

// mu_get_current_container: �R���e�i�X�^�b�N�̌��݂̃R���e�i���擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �߂�l:
//   ���݂̃R���e�i�ւ̃|�C���^
mu_Container* mu_get_current_container(mu_Context *ctx) {
  expect(ctx->container_stack.idx > 0);
  return ctx->container_stack.items[ctx->container_stack.idx - 1];
}

// get_container: �R���e�i���擾�܂��͏���������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   id - �R���e�i�����ʂ��邽�߂̈�ӂ�ID
//   opt - �R���e�i�̃I�v�V�����t���O
// �߂�l:
//   �w�肳�ꂽID�ɑΉ�����R���e�i�ւ̃|�C���^
// �������e:
//   - �w�肳�ꂽID�ɑΉ�����R���e�i���v�[������擾���܂��B
//   - �R���e�i�����݂��Ȃ��ꍇ�A�V�����R���e�i�����������܂��B
//   - �R���e�i�������Ă���ꍇ�ANULL��Ԃ��܂��B
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

// mu_get_container: ���O����R���e�i���擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   name - �R���e�i�̖��O
// �߂�l:
//   �w�肳�ꂽ���O�ɑΉ�����R���e�i�ւ̃|�C���^
// �������e:
//   ���O���n�b�V��������ID�𐶐����Aget_container�֐����Ăяo���܂��B
mu_Container* mu_get_container(mu_Context *ctx, const char *name) {
  mu_Id id = mu_get_id(ctx, name, strlen(name));
  return get_container(ctx, id, 0);
}

// mu_bring_to_front: �R���e�i���őO�ʂɈړ�����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   cnt - �őO�ʂɈړ�����R���e�i
// �������e:
//   �R���e�i��Z�C���f�b�N�X���X�V���A�őO�ʂɈړ����܂��B
void mu_bring_to_front(mu_Context *ctx, mu_Container *cnt) {
  cnt->zindex = ++ctx->last_zindex;
}


/*============================================================================
** pool
**============================================================================*/

// mu_pool_init: �v�[�����̃A�C�e��������������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   items - �v�[�����̃A�C�e���z��
//   len - �v�[���̒����i�A�C�e�����j
//   id - ����������A�C�e����ID
// �߂�l:
//   ���������ꂽ�A�C�e���̃C���f�b�N�X
// �������e:
//   - �v�[�����ōł��Â��A�C�e���������A���̃A�C�e�������������܂��B
//   - �A�C�e����ID��ݒ肵�A�X�V�^�C���X�^���v�����݂̃t���[���ɐݒ肵�܂��B
int mu_pool_init(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i, n = -1, f = ctx->frame;
  for (i = 0; i < len; i++) {
    if (items[i].last_update < f) {
      f = items[i].last_update;
      n = i;
    }
  }
  expect(n > -1); // �v�[���ɋ󂫂����邱�Ƃ��m�F
  items[n].id = id;
  mu_pool_update(ctx, items, n);
  return n;
}

// mu_pool_get: �v�[�����̃A�C�e�����擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   items - �v�[�����̃A�C�e���z��
//   len - �v�[���̒����i�A�C�e�����j
//   id - ��������A�C�e����ID
// �߂�l:
//   �w�肳�ꂽID�ɑΉ�����A�C�e���̃C���f�b�N�X�i������Ȃ��ꍇ��-1�j
// �������e:
//   - �v�[�������������A�w�肳�ꂽID�ɑΉ�����A�C�e���������܂��B
int mu_pool_get(mu_Context *ctx, mu_PoolItem *items, int len, mu_Id id) {
  int i;
  unused(ctx); // ctx�͂��̊֐����ł͎g�p����Ȃ�
  for (i = 0; i < len; i++) {
    if (items[i].id == id) { return i; }
  }
  return -1; // ������Ȃ��ꍇ
}

// mu_pool_update: �v�[�����̃A�C�e���̍X�V�^�C���X�^���v��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   items - �v�[�����̃A�C�e���z��
//   idx - �X�V����A�C�e���̃C���f�b�N�X
// �������e:
//   - �w�肳�ꂽ�A�C�e���̍X�V�^�C���X�^���v�����݂̃t���[���ɐݒ肵�܂��B
void mu_pool_update(mu_Context *ctx, mu_PoolItem *items, int idx) {
  items[idx].last_update = ctx->frame;
}

/*============================================================================
** input handlers
**============================================================================*/
// mu_input_mousemove: �}�E�X�̈ړ��C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   x - �}�E�X�̌��݂�X���W
//   y - �}�E�X�̌��݂�Y���W
// �������e:
//   �}�E�X�̌��݈ʒu���X�V���܂��B
void mu_input_mousemove(mu_Context *ctx, int x, int y) {
  ctx->mouse_pos = mu_vec2(x, y);
}

// mu_input_mousedown: �}�E�X�̃{�^�������C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   x - �}�E�X�̌��݂�X���W
//   y - �}�E�X�̌��݂�Y���W
//   btn - �������ꂽ�{�^���̎�� (��: ���{�^���A�E�{�^��)
// �������e:
//   �}�E�X�̌��݈ʒu���X�V���A�������ꂽ�{�^���̏�Ԃ��L�^���܂��B
void mu_input_mousedown(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down |= btn;
  ctx->mouse_pressed |= btn;
}

// mu_input_mouseup: �}�E�X�̃{�^������C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   x - �}�E�X�̌��݂�X���W
//   y - �}�E�X�̌��݂�Y���W
//   btn - ������ꂽ�{�^���̎��
// �������e:
//   �}�E�X�̌��݈ʒu���X�V���A������ꂽ�{�^���̏�Ԃ��L�^���܂��B
void mu_input_mouseup(mu_Context *ctx, int x, int y, int btn) {
  mu_input_mousemove(ctx, x, y);
  ctx->mouse_down &= ~btn;
}

// mu_input_scroll: �}�E�X�z�C�[���̃X�N���[���C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   x - ���������̃X�N���[����
//   y - ���������̃X�N���[����
// �������e:
//   �X�N���[���ʂ��L�^���܂��B
void mu_input_scroll(mu_Context *ctx, int x, int y) {
  ctx->scroll_delta.x += x;
  ctx->scroll_delta.y += y;
}

// mu_input_keydown: �L�[�����C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   key - �������ꂽ�L�[�̃R�[�h
// �������e:
//   �������ꂽ�L�[�̏�Ԃ��L�^���܂��B
void mu_input_keydown(mu_Context *ctx, int key) {
  ctx->key_pressed |= key;
  ctx->key_down |= key;
}

// mu_input_keyup: �L�[����C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   key - ������ꂽ�L�[�̃R�[�h
// �������e:
//   ������ꂽ�L�[�̏�Ԃ��L�^���܂��B
void mu_input_keyup(mu_Context *ctx, int key) {
  ctx->key_down &= ~key;
}

// mu_input_text: �e�L�X�g���̓C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   text - ���͂��ꂽ�e�L�X�g
// �������e:
//   ���͂��ꂽ�e�L�X�g���o�b�t�@�ɒǉ����܂��B
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
** �R�}���h���X�g�̊T�v:
** MicroUI�ł́A�`���N���b�s���O�Ȃǂ̑�����R�}���h�Ƃ��ċL�^���A
** ��ňꊇ���ď�������d�g�݂��̗p���Ă��܂��B
** �R�}���h���X�g�́A�`��R�}���h��N���b�s���O�R�}���h�Ȃǂ��i�[���邽�߂̃��X�g�ł��B
**
** ��ȓ���:
** - �R�}���h�����X�g�ɒǉ�����ۂɂ́A`mu_push_command`���g�p���܂��B
** - �R�}���h��������������ۂɂ́A`mu_next_command`���g�p���܂��B
** - �R�}���h�ɂ́A��`�`��A�e�L�X�g�`��A�A�C�R���`��A�N���b�s���O�ݒ�Ȃǂ�����܂��B
**
** ���_:
** - �`�揈�����ꌳ�Ǘ��ł���B
** - �`�揇���𐧌䂵�₷���B
** - �`��̍œK�����\�B
*/

// mu_push_command: �V�����R�}���h���R�}���h���X�g�ɒǉ�����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   type - �R�}���h�̎�� (��: ��`�`��A�e�L�X�g�`��Ȃ�)
//   size - �R�}���h�̃T�C�Y (�o�C�g�P��)
// �߂�l:
//   �ǉ����ꂽ�R�}���h�ւ̃|�C���^
// �������e:
//   - �w�肳�ꂽ��ނƃT�C�Y�̃R�}���h���R�}���h���X�g�ɒǉ����܂��B
//   - �R�}���h���X�g�̃C���f�b�N�X���X�V���܂��B
mu_Command* mu_push_command(mu_Context *ctx, int type, int size) {
  mu_Command *cmd = (mu_Command*) (ctx->command_list.items + ctx->command_list.idx);
  expect(ctx->command_list.idx + size < MU_COMMANDLIST_SIZE);
  cmd->base.type = type;
  cmd->base.size = size;
  ctx->command_list.idx += size;
  return cmd;
}

// mu_next_command: �R�}���h���X�g���̎��̃R�}���h���擾����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   cmd - ���݂̃R�}���h�ւ̃|�C���^ (NULL�̏ꍇ�͍ŏ��̃R�}���h���擾)
// �߂�l:
//   ���̃R�}���h�����݂���ꍇ��1�A���݂��Ȃ��ꍇ��0
// �������e:
//   - ���݂̃R�}���h�̎��̃R�}���h���擾���܂��B
//   - �W�����v�R�}���h���X�L�b�v���܂��B
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

// push_jump: �W�����v�R�}���h���R�}���h���X�g�ɒǉ�����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   dst - �W�����v��̃R�}���h�ւ̃|�C���^
// �߂�l:
//   �ǉ����ꂽ�W�����v�R�}���h�ւ̃|�C���^
// �������e:
//   - �W�����v�R�}���h���R�}���h���X�g�ɒǉ����܂��B
//   - �W�����v��̃R�}���h��ݒ肵�܂��B
static mu_Command* push_jump(mu_Context *ctx, mu_Command *dst) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_JUMP, sizeof(mu_JumpCommand));
  cmd->jump.dst = dst;
  return cmd;
}

// mu_set_clip: �N���b�s���O��`��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - �ݒ肷��N���b�s���O��`
// �������e:
//   - �N���b�s���O�R�}���h���R�}���h���X�g�ɒǉ����܂��B
//   - �w�肳�ꂽ��`���N���b�s���O�̈�Ƃ��Đݒ肵�܂��B
void mu_set_clip(mu_Context *ctx, mu_Rect rect) {
  mu_Command *cmd;
  cmd = mu_push_command(ctx, MU_COMMAND_CLIP, sizeof(mu_ClipCommand));
  cmd->clip.rect = rect;
}

// mu_draw_rect: ��`��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - �`�悷���`
//   color - ��`�̐F
// �������e:
//   - ��`���N���b�s���O�̈�ƌ��������܂��B
//   - �������������݂���ꍇ�A��`�`��R�}���h���R�}���h���X�g�ɒǉ����܂��B
void mu_draw_rect(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_Command *cmd;
  rect = intersect_rects(rect, mu_get_clip_rect(ctx));
  if (rect.w > 0 && rect.h > 0) {
    cmd = mu_push_command(ctx, MU_COMMAND_RECT, sizeof(mu_RectCommand));
    cmd->rect.rect = rect;
    cmd->rect.color = color;
  }
}

// mu_draw_box: ���E���t���̋�`��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - �`�悷���`
//   color - ���E���̐F
// �������e:
//   - ��`�̏㉺���E�ɋ��E����`�悵�܂��B
void mu_draw_box(mu_Context *ctx, mu_Rect rect, mu_Color color) {
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x + 1, rect.y + rect.h - 1, rect.w - 2, 1), color);
  mu_draw_rect(ctx, mu_rect(rect.x, rect.y, 1, rect.h), color);
  mu_draw_rect(ctx, mu_rect(rect.x + rect.w - 1, rect.y, 1, rect.h), color);
}

// mu_draw_text: �e�L�X�g��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   font - �g�p����t�H���g
//   str - �`�悷�镶����
//   len - ������̒��� (-1�̏ꍇ�͎����v�Z)
//   pos - �e�L�X�g�̕`��ʒu
//   color - �e�L�X�g�̐F
// �������e:
//   - �e�L�X�g�̕`��̈���N���b�s���O�̈�ƌ��������܂��B
//   - �������������݂���ꍇ�A�e�L�X�g�`��R�}���h���R�}���h���X�g�ɒǉ����܂��B
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

// mu_draw_icon: �A�C�R����`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   id - �`�悷��A�C�R����ID
//   rect - �A�C�R���̕`��̈�
//   color - �A�C�R���̐F
// �������e:
//   - �A�C�R���̕`��̈���N���b�s���O�̈�ƌ��������܂��B
//   - �������������݂���ꍇ�A�A�C�R���`��R�}���h���R�}���h���X�g�ɒǉ����܂��B
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
** ���C�A�E�g�̊T�v:
** MicroUI�ł́AUI�v�f�������I�ɔz�u���邽�߂Ƀ��C�A�E�g�V�X�e�����g�p���Ă��܂��B
** ���C�A�E�g�́A�s�������UI�v�f��z�u���A�T�C�Y��ʒu���v�Z���܂��B
**
** ��ȓ���:
** - `mu_layout_row`���g�p���čs�̕��⍂����ݒ肵�܂��B
** - `mu_layout_next`���g�p���Ď��̗v�f�̈ʒu�ƃT�C�Y���v�Z���܂��B
** - `mu_layout_begin_column`��`mu_layout_end_column`���g�p���ė���Ǘ����܂��B
** - `mu_layout_width`��`mu_layout_height`�ŗv�f�̃T�C�Y���w��ł��܂��B
**
** ���_:
** - UI�v�f�̔z�u���ȒP�ɊǗ��ł���B
** - �����I�ɃT�C�Y��ʒu���v�Z���邽�߁A�蓮�ł̒������s�v�B
** - ���X�|���V�u��UI�݌v���\�B
*/

// mu_layout_row: ���C�A�E�g�̍s��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   items - �s���̃A�C�e����
//   widths - �e�A�C�e���̕� (NULL�̏ꍇ�͎����v�Z)
//   height - �s�̍��� (0�̏ꍇ�͎����v�Z)
// �������e:
//   - �s�̕��ƍ�����ݒ肵�܂��B
//   - ���̗v�f�̔z�u�ʒu�����������܂��B
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

// mu_layout_begin_column: �V��������J�n����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - ���݂̃��C�A�E�g��ۑ����A�V�������C�A�E�g���X�^�b�N�Ƀv�b�V�����܂��B
void mu_layout_begin_column(mu_Context *ctx) {
  push_layout(ctx, mu_layout_next(ctx), mu_vec2(0, 0));
}

// mu_layout_end_column: ����I������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - ���݂̃��C�A�E�g���|�b�v���A�e���C�A�E�g�ɓ������܂��B
void mu_layout_end_column(mu_Context *ctx) {
  mu_Layout *a, *b;
  b = get_layout(ctx);
  pop(ctx->layout_stack);
  /* �q���C�A�E�g�̈ʒu��T�C�Y��e���C�A�E�g�ɓ��� */
  a = get_layout(ctx);
  a->position.x = mu_max(a->position.x, b->position.x + b->body.x - a->body.x);
  a->next_row = mu_max(a->next_row, b->next_row + b->body.y - a->body.y);
  a->max.x = mu_max(a->max.x, b->max.x);
  a->max.y = mu_max(a->max.y, b->max.y);
}

// mu_layout_width: ���̗v�f�̕���ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   width - �ݒ肷�镝
// �������e:
//   - ���̗v�f�̕����w�肵�܂��B
void mu_layout_width(mu_Context *ctx, int width) {
  get_layout(ctx)->size.x = width;
}

// mu_layout_height: ���̗v�f�̍�����ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   height - �ݒ肷�鍂��
// �������e:
//   - ���̗v�f�̍������w�肵�܂��B
void mu_layout_height(mu_Context *ctx, int height) {
  get_layout(ctx)->size.y = height;
}

// mu_layout_set_next: ���̗v�f�̈ʒu�ƃT�C�Y��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   r - ���̗v�f�̋�`
//   relative - ���Έʒu���ǂ��� (1: ����, 0: ���)
// �������e:
//   - ���̗v�f�̈ʒu�ƃT�C�Y��ݒ肵�܂��B
void mu_layout_set_next(mu_Context *ctx, mu_Rect r, int relative) {
  mu_Layout *layout = get_layout(ctx);
  layout->next = r;
  layout->next_type = relative ? RELATIVE : ABSOLUTE;
}

// mu_layout_next: ���̗v�f�̋�`���v�Z����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �߂�l:
//   ���̗v�f�̋�`
// �������e:
//   - ���̗v�f�̈ʒu�ƃT�C�Y���v�Z���܂��B
//   - �s�̏I���ɒB�����ꍇ�A�V�����s���J�n���܂��B
mu_Rect mu_layout_next(mu_Context *ctx) {
  mu_Layout *layout = get_layout(ctx);
  mu_Style *style = ctx->style;
  mu_Rect res;

  if (layout->next_type) {
    /* `mu_layout_set_next`�Őݒ肳�ꂽ��`������ */
    int type = layout->next_type;
    layout->next_type = 0;
    res = layout->next;
    if (type == ABSOLUTE) { return (ctx->last_rect = res); }

  } else {
    /* ���̍s������ */
    if (layout->item_index == layout->items) {
      mu_layout_row(ctx, layout->items, NULL, layout->size.y);
    }

    /* �ʒu���v�Z */
    res.x = layout->position.x;
    res.y = layout->position.y;

    /* �T�C�Y���v�Z */
    res.w = layout->items > 0 ? layout->widths[layout->item_index] : layout->size.x;
    res.h = layout->size.y;
    if (res.w == 0) { res.w = style->size.x + style->padding * 2; }
    if (res.h == 0) { res.h = style->size.y + style->padding * 2; }
    if (res.w <  0) { res.w += layout->body.w - res.x + 1; }
    if (res.h <  0) { res.h += layout->body.h - res.y + 1; }

    layout->item_index++;
  }

  /* �ʒu���X�V */
  layout->position.x += res.w + style->spacing;
  layout->next_row = mu_max(layout->next_row, res.y + res.h + style->spacing);

  /* �{�f�B�̃I�t�Z�b�g��K�p */
  res.x += layout->body.x;
  res.y += layout->body.y;

  /* �ő�ʒu���X�V */
  layout->max.x = mu_max(layout->max.x, res.x + res.w);
  layout->max.y = mu_max(layout->max.y, res.y + res.h);

  return (ctx->last_rect = res);
}

/*============================================================================
** controls
**============================================================================*/

// in_hover_root: ���݂̃}�E�X�ʒu���z�o�[���[�g���ɂ��邩�𔻒肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �߂�l:
//   �}�E�X���z�o�[���[�g���ɂ���ꍇ��1�A�����łȂ��ꍇ��0
// �������e:
//   - �R���e�i�X�^�b�N���t���Ɍ������A�z�o�[���[�g���Ƀ}�E�X�����邩���m�F���܂��B
//   - ���[�g�R���e�i�ɓ��B�����ꍇ�A�������I�����܂��B
static int in_hover_root(mu_Context *ctx) {
  int i = ctx->container_stack.idx;
  while (i--) {
    if (ctx->container_stack.items[i] == ctx->hover_root) { return 1; }
    // ���[�g�R���e�i�ɓ��B�����猟�����I��
    if (ctx->container_stack.items[i]->head) { break; }
  }
  return 0;
}



// mu_draw_control_frame: �R���g���[���̃t���[����`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   id - �R���g���[����ID
//   rect - �`�悷���`
//   colorid - �g�p����F��ID
//   opt - �I�v�V�����t���O
// �������e:
//   - �t���[����`�悵�܂��B
//   - �t�H�[�J�X��z�o�[��Ԃɉ����ĐF��ύX���܂��B
void mu_draw_control_frame(mu_Context *ctx, mu_Id id, mu_Rect rect,
  int colorid, int opt)
{
  if (opt & MU_OPT_NOFRAME) { return; }
  colorid += (ctx->focus == id) ? 2 : (ctx->hover == id) ? 1 : 0;
  ctx->draw_frame(ctx, rect, colorid);
}
// mu_draw_control_text: �R���g���[���̃e�L�X�g��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   str - �`�悷�镶����
//   rect - �e�L�X�g��`�悷���`
//   colorid - �g�p����F��ID
//   opt - �I�v�V�����t���O
// �������e:
//   - �e�L�X�g�̕`��ʒu���v�Z���܂��B
//   - �I�v�V�����t���O�ɉ����ăe�L�X�g�̔z�u��ύX���܂��B
//   - �e�L�X�g���N���b�s���O��`���ɕ`�悵�܂��B
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


// mu_mouse_over: �}�E�X���w�肳�ꂽ��`���ɂ��邩�𔻒肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   rect - ����Ώۂ̋�`
// �߂�l:
//   �}�E�X����`���ɂ���ꍇ��1�A�����łȂ��ꍇ��0
// �������e:
//   - �}�E�X�ʒu����`���ɂ��邩���m�F���܂��B
//   - �N���b�s���O��`���ɂ��邩�A�z�o�[���[�g���ɂ��邩�����肵�܂��B
int mu_mouse_over(mu_Context *ctx, mu_Rect rect) {
  return rect_overlaps_vec2(rect, ctx->mouse_pos) &&
    rect_overlaps_vec2(mu_get_clip_rect(ctx), ctx->mouse_pos) &&
    in_hover_root(ctx);
}

// mu_update_control: �R���g���[���̏�Ԃ��X�V����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   id - �R���g���[����ID
//   rect - �R���g���[���̋�`
//   opt - �I�v�V�����t���O
// �������e:
//   - �R���g���[���̃t�H�[�J�X�A�z�o�[�A�N���b�N��Ԃ��X�V���܂��B
//   - �}�E�X�̈ʒu��N���b�N��Ԃɉ����āA�t�H�[�J�X��z�o�[��Ԃ�ύX���܂��B
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
// mu_text: �e�L�X�g�𕡐��s�ɂ킽���ĕ`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   text - �`�悷�镶����
// �������e:
//   - �e�L�X�g���s���Ƃɕ������A�e�s��`�悵�܂��B
//   - �s�̕��⍂�����v�Z���A�K�؂Ȉʒu�Ƀe�L�X�g��z�u���܂��B
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


// mu_label: �P��s�̃��x����`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   text - �`�悷�镶����
// �������e:
//   - �e�L�X�g�����݂̃��C�A�E�g�ʒu�ɕ`�悵�܂��B
//   - �e�L�X�g�̐F��ʒu��ݒ肵�܂��B
void mu_label(mu_Context *ctx, const char *text) {
  mu_draw_control_text(ctx, text, mu_layout_next(ctx), MU_COLOR_TEXT, 0);
}

// mu_button_ex: �{�^����`�悵�A�N���b�N�C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   label - �{�^���ɕ\������e�L�X�g
//   icon - �{�^���ɕ\������A�C�R���i�I�v�V�����j
//   opt - �{�^���̃I�v�V�����t���O
// �߂�l:
//   �{�^�����N���b�N���ꂽ�ꍇ�� MU_RES_SUBMIT ��Ԃ�
// �������e:
//   - �{�^���̋�`���v�Z���A�N���b�N�C�x���g���������܂��B
//   - �{�^���̔w�i�A�e�L�X�g�A�A�C�R����`�悵�܂��B
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


// mu_checkbox: �`�F�b�N�{�b�N�X��`�悵�A�N���b�N�C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   label - �`�F�b�N�{�b�N�X�ɕ\������e�L�X�g
//   state - �`�F�b�N�{�b�N�X�̏�ԁi�I��/�I�t�j
// �߂�l:
//   �`�F�b�N�{�b�N�X�̏�Ԃ��ύX���ꂽ�ꍇ�� MU_RES_CHANGE ��Ԃ�
// �������e:
//   - �`�F�b�N�{�b�N�X�̋�`���v�Z���A�N���b�N�C�x���g���������܂��B
//   - �`�F�b�N�{�b�N�X�̔w�i�A�`�F�b�N�}�[�N�A�e�L�X�g��`�悵�܂��B
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




// mu_textbox_raw: �e�L�X�g�{�b�N�X��`�悵�A���̓C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   buf - �e�L�X�g�{�b�N�X�ɕ\�����镶����o�b�t�@
//   bufsz - �o�b�t�@�̃T�C�Y
//   id - �e�L�X�g�{�b�N�X��ID
//   rect - �e�L�X�g�{�b�N�X�̋�`
//   opt - �I�v�V�����t���O
// �߂�l:
//   �e�L�X�g���ύX���ꂽ�ꍇ�� MU_RES_CHANGE ��Ԃ�
//   �e�L�X�g���m�肳�ꂽ�ꍇ�� MU_RES_SUBMIT ��Ԃ�
// �������e:
//   - �e�L�X�g�{�b�N�X�̋�`���v�Z���A���̓C�x���g���������܂��B
//   - �e�L�X�g�̓��͂�폜�A�J�[�\���̕`����s���܂��B
//   - �e�L�X�g�{�b�N�X���A�N�e�B�u�ȏꍇ�A�J�[�\����\�����܂��B
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

// number_textbox: ���l���͗p�̃e�L�X�g�{�b�N�X��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   value - ���͂��鐔�l
//   r - �e�L�X�g�{�b�N�X�̋�`
//   id - �e�L�X�g�{�b�N�X��ID
// �߂�l:
//   �e�L�X�g�{�b�N�X���A�N�e�B�u�ȏꍇ��1�A����ȊO��0
// �������e:
//   - ���l���̓��[�h���������܂��B
//   - �e�L�X�g�{�b�N�X���m�肳�ꂽ�ꍇ�A���l���X�V���܂��B
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

// mu_textbox_ex: �e�L�X�g�{�b�N�X��`�悵�A���̓C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   buf - �e�L�X�g�{�b�N�X�ɕ\�����镶����o�b�t�@
//   bufsz - �o�b�t�@�̃T�C�Y
//   opt - �I�v�V�����t���O
// �߂�l:
//   �e�L�X�g���ύX���ꂽ�ꍇ�� MU_RES_CHANGE ��Ԃ�
//   �e�L�X�g���m�肳�ꂽ�ꍇ�� MU_RES_SUBMIT ��Ԃ�
// �������e:
//   - �e�L�X�g�{�b�N�X�̋�`���v�Z���A���̓C�x���g���������܂��B
//   - �e�L�X�g�̓��͂�폜�A�J�[�\���̕`����s���܂��B
int mu_textbox_ex(mu_Context *ctx, char *buf, int bufsz, int opt) {
  mu_Id id = mu_get_id(ctx, &buf, sizeof(buf));
  mu_Rect r = mu_layout_next(ctx);
  return mu_textbox_raw(ctx, buf, bufsz, id, r, opt);
}

// mu_slider_ex: �X���C�_�[��`�悵�A���̓C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   value - �X���C�_�[�̒l�ւ̃|�C���^
//   low - �X���C�_�[�̍ŏ��l
//   high - �X���C�_�[�̍ő�l
//   step - �X���C�_�[�̃X�e�b�v�l
//   fmt - �X���C�_�[�̒l��\������t�H�[�}�b�g������
//   opt - �I�v�V�����t���O
// �߂�l:
//   �X���C�_�[�̒l���ύX���ꂽ�ꍇ�� MU_RES_CHANGE ��Ԃ�
// �������e:
//   - �X���C�_�[�̒l���v�Z���A���̓C�x���g���������܂��B
//   - �X���C�_�[�̔w�i�A�X���C�_�[�̂܂݁A�l��`�悵�܂��B
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

// mu_number_ex: ���l���͗p�̃R���g���[����`�悵�A���̓C�x���g����������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   value - ���͂��鐔�l�ւ̃|�C���^
//   step - ���l�̑����X�e�b�v
//   fmt - ���l��\������t�H�[�}�b�g������
//   opt - �I�v�V�����t���O
// �߂�l:
//   ���l���ύX���ꂽ�ꍇ�� MU_RES_CHANGE ��Ԃ�
// �������e:
//   - ���l���̓��[�h���������܂��B
//   - ���l�̑�������̓C�x���g���������܂��B
//   - ���l��\������e�L�X�g��`�悵�܂��B
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

// header: �w�b�_�[�i�܂肽���݉\�ȗv�f�j��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   label - �w�b�_�[�̃��x��
//   istreenode - �c���[�m�[�h���ǂ����������t���O
//   opt - �I�v�V�����t���O
// �߂�l:
//   �w�b�_�[���A�N�e�B�u�ȏꍇ�� MU_RES_ACTIVE ��Ԃ�
// �������e:
//   - �w�b�_�[�̋�`���v�Z���A�N���b�N�C�x���g���������܂��B
//   - �w�b�_�[�̔w�i�A�A�C�R���A�e�L�X�g��`�悵�܂��B
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

// mu_header_ex: �܂肽���݉\�ȃw�b�_�[��`�悷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   label - �w�b�_�[�̃��x��
//   opt - �I�v�V�����t���O
// �߂�l:
//   �w�b�_�[���A�N�e�B�u�ȏꍇ�� MU_RES_ACTIVE ��Ԃ�
// �������e:
//   - �w�b�_�[�̋�`���v�Z���A�N���b�N�C�x���g���������܂��B
//   - �w�b�_�[�̔w�i�A�A�C�R���A�e�L�X�g��`�悵�܂��B
int mu_header_ex(mu_Context *ctx, const char *label, int opt) {
  return header(ctx, label, 0, opt);
}


// mu_begin_treenode_ex: �܂肽���݉\�ȃc���[�m�[�h���J�n����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   label - �c���[�m�[�h�̃��x��
//   opt - �I�v�V�����t���O
// �߂�l:
//   �c���[�m�[�h���W�J����Ă���ꍇ�� MU_RES_ACTIVE ��Ԃ�
// �������e:
//   - �c���[�m�[�h�̋�`���v�Z���A�N���b�N�C�x���g���������܂��B
//   - �c���[�m�[�h�̔w�i�A�A�C�R���A�e�L�X�g��`�悵�܂��B
//   - �c���[�m�[�h���W�J����Ă���ꍇ�A�C���f���g��ǉ����܂��B
int mu_begin_treenode_ex(mu_Context *ctx, const char *label, int opt) {
  int res = header(ctx, label, 1, opt);
  if (res & MU_RES_ACTIVE) {
    get_layout(ctx)->indent += ctx->style->indent;
    push(ctx->id_stack, ctx->last_id);
  }
  return res;
}

// mu_end_treenode: �c���[�m�[�h���I������֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - �c���[�m�[�h�̃C���f���g�����炵�܂��B
//   - ID�X�^�b�N����c���[�m�[�h��ID���|�b�v���܂��B
void mu_end_treenode(mu_Context *ctx) {
  get_layout(ctx)->indent -= ctx->style->indent;
  mu_pop_id(ctx);
}


// scrollbar: �X�N���[���o�[��`�悵�A�X�N���[���C�x���g����������}�N��
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   cnt - �X�N���[���Ώۂ̃R���e�i
//   b - �X�N���[���o�[�̃{�f�B�i��`�̈�j
//   cs - �R���e���c�T�C�Y�i�X�N���[���\�ȗ̈�̃T�C�Y�j
//   x, y, w, h - �X�N���[���o�[�̕����ƃT�C�Y���w�肷�邽�߂̃p�����[�^
// �������e:
//   - �R���e���c�T�C�Y���{�f�B�T�C�Y�𒴂���ꍇ�ɃX�N���[���o�[��`�悵�܂��B
//   - �X�N���[���o�[�̈ʒu�ƃT�C�Y���v�Z���A�X�N���[���\�Ȕ͈͂�ݒ肵�܂��B
//   - �}�E�X����ɂ��X�N���[���C�x���g���������܂��B
//   - �X�N���[���o�[�̔w�i�ƃT���i�܂݁j��`�悵�܂��B
//   - �}�E�X�z�C�[�����쎞�ɃX�N���[���ΏۂƂ��Đݒ肵�܂��B
#define scrollbar(ctx, cnt, b, cs, x, y, w, h)                              \
  do {                                                                      \
    /* �R���e���c�T�C�Y���{�f�B�T�C�Y�𒴂���ꍇ�̂݃X�N���[���o�[��ǉ� */ \
    int maxscroll = cs.y - b->h;                                            \
                                                                            \
    if (maxscroll > 0 && b->h > 0) {                                        \
      mu_Rect base, thumb;                                                  \
      mu_Id id = mu_get_id(ctx, "!scrollbar" #y, 11);                       \
                                                                            \
      /* �X�N���[���o�[�̃T�C�Y�ƈʒu���v�Z */                              \
      base = *b;                                                            \
      base.x = b->x + b->w;                                                 \
      base.w = ctx->style->scrollbar_size;                                  \
                                                                            \
      /* ���͏��� */                                                        \
      mu_update_control(ctx, id, base, 0);                                  \
      if (ctx->focus == id && ctx->mouse_down == MU_MOUSE_LEFT) {           \
        cnt->scroll.y += ctx->mouse_delta.y * cs.y / base.h;                \
      }                                                                     \
      /* �X�N���[���ʒu�𐧌� */                                            \
      cnt->scroll.y = mu_clamp(cnt->scroll.y, 0, maxscroll);                \
                                                                            \
      /* �X�N���[���o�[�̔w�i�ƃT����`�� */                                 \
      ctx->draw_frame(ctx, base, MU_COLOR_SCROLLBASE);                      \
      thumb = base;                                                         \
      thumb.h = mu_max(ctx->style->thumb_size, base.h * b->h / cs.y);       \
      thumb.y += cnt->scroll.y * (base.h - thumb.h) / maxscroll;            \
      ctx->draw_frame(ctx, thumb, MU_COLOR_SCROLLTHUMB);                    \
                                                                            \
      /* �}�E�X���X�N���[���o�[��ɂ���ꍇ�A�X�N���[���ΏۂƂ��Đݒ� */     \
      if (mu_mouse_over(ctx, *b)) { ctx->scroll_target = cnt; }             \
    } else {                                                                \
      cnt->scroll.y = 0;                                                    \
    }                                                                       \
  } while (0)

// scrollbars: �R���e�i�ɐ�������ѐ����X�N���[���o�[��ǉ�����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   cnt - �X�N���[���Ώۂ̃R���e�i
//   body - �R���e�i�̃{�f�B�i��`�̈�j
// �������e:
//   - �R���e���c�T�C�Y�Ɋ�Â��ăX�N���[���o�[��ǉ����܂��B
//   - �X�N���[���o�[�̈ʒu�ƃT�C�Y���v�Z���A�`�悵�܂��B
static void scrollbars(mu_Context *ctx, mu_Container *cnt, mu_Rect *body) {
  int sz = ctx->style->scrollbar_size;
  mu_Vec2 cs = cnt->content_size;
  cs.x += ctx->style->padding * 2;
  cs.y += ctx->style->padding * 2;
  mu_push_clip_rect(ctx, *body);

  /* �X�N���[���o�[�̂��߂Ƀ{�f�B�T�C�Y�𒲐� */
  if (cs.y > cnt->body.h) { body->w -= sz; }
  if (cs.x > cnt->body.w) { body->h -= sz; }

  /* ��������ѐ����X�N���[���o�[���쐬 */
  scrollbar(ctx, cnt, body, cs, x, y, w, h);
  scrollbar(ctx, cnt, body, cs, y, x, h, w);

  mu_pop_clip_rect(ctx);
}

// push_container_body: �R���e�i�̃{�f�B��ݒ肷��֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   cnt - �R���e�i
//   body - �R���e�i�̃{�f�B�i��`�̈�j
//   opt - �I�v�V�����t���O
// �������e:
//   - �X�N���[���o�[��ǉ����܂��i�K�v�ȏꍇ�j�B
//   - ���C�A�E�g�����������A�R���e�i�̃{�f�B��ݒ肵�܂��B
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


// end_root_container: ���[�g�R���e�i�̏I���������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - ���[�g�R���e�i�̏I�������Ƃ��āA�R�}���h���X�g�ɃW�����v�R�}���h��ǉ����܂��B
//   - �R���e�i�̃N���b�s���O��`���|�b�v���A�R���e�i�X�^�b�N����R���e�i���폜���܂��B
static void end_root_container(mu_Context *ctx) {
  // ���݂̃R���e�i���擾
  mu_Container *cnt = mu_get_current_container(ctx);

  // �R�}���h���X�g�ɏI���p�̃W�����v�R�}���h��ǉ�
  cnt->tail = push_jump(ctx, NULL);

  // �R���e�i�̐擪�R�}���h�̃W�����v������݂̃R�}���h���X�g�̈ʒu�ɐݒ�
  cnt->head->jump.dst = ctx->command_list.items + ctx->command_list.idx;

  // �N���b�s���O��`���|�b�v
  mu_pop_clip_rect(ctx);

  // �R���e�i�X�^�b�N����R���e�i���폜
  pop_container(ctx);
}
/**
 * @brief �E�B���h�E���J�n����֐�
 *
 * ���̊֐��́AMicroUI ���C�u�����̃E�B���h�E���J�n���邽�߂̊֐��ł��B
 * �E�B���h�E�̕`��ƊǗ����s�����߂̈�A�̑�����s���܂��B
 *
 * @param ctx MicroUI �̃R���e�L�X�g���w���|�C���^
 * @param title �E�B���h�E�̃^�C�g�����w�肷�镶����
 * @param rect �E�B���h�E�̏����ʒu�ƃT�C�Y���w�肷���`
 * @param opt �E�B���h�E�̓����O�ς𐧌䂷��I�v�V�����t���O
 * @return int �E�B���h�E���A�N�e�B�u���ǂ����������l�B�A�N�e�B�u�ȏꍇ�� MU_RES_ACTIVE ��Ԃ�
 */
int mu_begin_window_ex(mu_Context* ctx, const char* title, mu_Rect rect, int opt)
{
    mu_Rect body;
    mu_Id id = mu_get_id(ctx, title, strlen(title));  // �E�B���h�E�̃^�C�g�������ӂ� ID �𐶐�
    mu_Container* cnt = get_container(ctx, id, opt);  // �R���e�i���擾�܂��͏�����
    if (!cnt || !cnt->open) { return 0; }  // �R���e�i�����݂��Ȃ��������Ă���ꍇ�͏I��
    push(ctx->id_stack, id);  // �R���e�i�� ID ���X�^�b�N�Ƀv�b�V��

    if (cnt->rect.w == 0) { cnt->rect = rect; }  // �R���e�i�̋�`�����ݒ�̏ꍇ�͏�����`��ݒ�
    begin_root_container(ctx, cnt);  // ���[�g�R���e�i�̏�����
    rect = body = cnt->rect;  // �R���e�i�̋�`��ݒ�

    /* �t���[���̕`�� */
    if (~opt & MU_OPT_NOFRAME)
    {
        ctx->draw_frame(ctx, rect, MU_COLOR_WINDOWBG);  // �E�B���h�E�̔w�i��`��
    }

    /* �^�C�g���o�[�̏��� */
    if (~opt & MU_OPT_NOTITLE)
    {
        mu_Rect tr = rect;
        tr.h = ctx->style->title_height;  // �^�C�g���o�[�̍�����ݒ�
        ctx->draw_frame(ctx, tr, MU_COLOR_TITLEBG);  // �^�C�g���o�[�̔w�i��`��

        /* �^�C�g���e�L�X�g�̕`�� */
        if (~opt & MU_OPT_NOTITLE)
        {
            mu_Id id = mu_get_id(ctx, "!title", 6);  // �^�C�g���� ID �𐶐�
            mu_update_control(ctx, id, tr, opt);  // �^�C�g���o�[�̃R���g���[�����X�V
            mu_draw_control_text(ctx, title, tr, MU_COLOR_TITLETEXT, opt);  // �^�C�g���e�L�X�g��`��
            if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
            {
                cnt->rect.x += ctx->mouse_delta.x;  // �E�B���h�E�̃h���b�O���������
                cnt->rect.y += ctx->mouse_delta.y;
            }
            body.y += tr.h;  // �^�C�g���o�[�̍����������{�f�B�̈ʒu��������
            body.h -= tr.h;  // �^�C�g���o�[�̍����������{�f�B�̍��������炷
        }

        /* �N���[�Y�{�^���̏��� */
        if (~opt & MU_OPT_NOCLOSE)
        {
            mu_Id id = mu_get_id(ctx, "!close", 6);  // �N���[�Y�{�^���� ID �𐶐�
            mu_Rect r = mu_rect(tr.x + tr.w - tr.h, tr.y, tr.h, tr.h);  // �N���[�Y�{�^���̋�`��ݒ�
            tr.w -= r.w;  // �^�C�g���o�[�̕����N���[�Y�{�^���̕����������炷
            mu_draw_icon(ctx, MU_ICON_CLOSE, r, ctx->style->colors[MU_COLOR_TITLETEXT]);  // �N���[�Y�A�C�R����`��
            mu_update_control(ctx, id, r, opt);  // �N���[�Y�{�^���̃R���g���[�����X�V
            if (ctx->mouse_pressed == MU_MOUSE_LEFT && id == ctx->focus)
            {
                cnt->open = 0;  // �N���[�Y�{�^�����N���b�N���ꂽ�ꍇ�̓R���e�i�����
            }
        }
    }

    push_container_body(ctx, cnt, body, opt);  // �R���e�i�̃{�f�B��ݒ�

    /* ���T�C�Y�n���h���̏��� */
    if (~opt & MU_OPT_NORESIZE)
    {
        int sz = ctx->style->title_height;  // ���T�C�Y�n���h���̃T�C�Y��ݒ�
        mu_Id id = mu_get_id(ctx, "!resize", 7);  // ���T�C�Y�n���h���� ID �𐶐�
        mu_Rect r = mu_rect(rect.x + rect.w - sz, rect.y + rect.h - sz, sz, sz);  // ���T�C�Y�n���h���̋�`��ݒ�
        mu_update_control(ctx, id, r, opt);  // ���T�C�Y�n���h���̃R���g���[�����X�V
        if (id == ctx->focus && ctx->mouse_down == MU_MOUSE_LEFT)
        {
            cnt->rect.w = mu_max(96, cnt->rect.w + ctx->mouse_delta.x);  // �E�B���h�E�̕������T�C�Y
            cnt->rect.h = mu_max(64, cnt->rect.h + ctx->mouse_delta.y);  // �E�B���h�E�̍��������T�C�Y
        }
    }

    /* �R���e���c�T�C�Y�ɍ��킹�����T�C�Y */
    if (opt & MU_OPT_AUTOSIZE)
    {
        mu_Rect r = get_layout(ctx)->body;  // ���C�A�E�g�̃{�f�B���擾
        cnt->rect.w = cnt->content_size.x + (cnt->rect.w - r.w);  // �R���e���c�T�C�Y�ɍ��킹�ĕ��𒲐�
        cnt->rect.h = cnt->content_size.y + (cnt->rect.h - r.h);  // �R���e���c�T�C�Y�ɍ��킹�č����𒲐�
    }

    /* �|�b�v�A�b�v�E�B���h�E�̃N���[�Y���� */
    if (opt & MU_OPT_POPUP && ctx->mouse_pressed && ctx->hover_root != cnt)
    {
        cnt->open = 0;  // ���̏ꏊ���N���b�N���ꂽ�ꍇ�̓|�b�v�A�b�v�E�B���h�E�����
    }

    mu_push_clip_rect(ctx, cnt->body);  // �R���e�i�̃{�f�B���N���b�s���O��`�Ƃ��Đݒ�
    return MU_RES_ACTIVE;  // �E�B���h�E���A�N�e�B�u�ł��邱�Ƃ�����
}

// mu_end_window: �E�B���h�E�̏I���������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - �E�B���h�E�̃N���b�s���O��`���|�b�v���܂��B
//   - ���[�g�R���e�i�̏I���������s���܂��B
void mu_end_window(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  end_root_container(ctx);
}

// mu_open_popup: �|�b�v�A�b�v�E�B���h�E���J���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   name - �|�b�v�A�b�v�E�B���h�E�̖��O
// �������e:
//   - �|�b�v�A�b�v�E�B���h�E�����݂̃}�E�X�ʒu�ɕ\�����܂��B
//   - �|�b�v�A�b�v�E�B���h�E���J���A�őO�ʂɈړ����܂��B
void mu_open_popup(mu_Context *ctx, const char *name) {
  mu_Container *cnt = mu_get_container(ctx, name);
  /* set as hover root so popup isn't closed in begin_window_ex()  */
  ctx->hover_root = ctx->next_hover_root = cnt;
  /* position at mouse cursor, open and bring-to-front */
  cnt->rect = mu_rect(ctx->mouse_pos.x, ctx->mouse_pos.y, 1, 1);
  cnt->open = 1;
  mu_bring_to_front(ctx, cnt);
}

// mu_begin_popup: �|�b�v�A�b�v�E�B���h�E���J�n����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   name - �|�b�v�A�b�v�E�B���h�E�̖��O
// �߂�l:
//   �|�b�v�A�b�v�E�B���h�E���A�N�e�B�u�ȏꍇ�� MU_RES_ACTIVE ��Ԃ�
// �������e:
//   - �|�b�v�A�b�v�E�B���h�E�����������A�`����J�n���܂��B
int mu_begin_popup(mu_Context *ctx, const char *name) {
  int opt = MU_OPT_POPUP | MU_OPT_AUTOSIZE | MU_OPT_NORESIZE |
            MU_OPT_NOSCROLL | MU_OPT_NOTITLE | MU_OPT_CLOSED;
  return mu_begin_window_ex(ctx, name, mu_rect(0, 0, 0, 0), opt);
}

// mu_end_popup: �|�b�v�A�b�v�E�B���h�E�̏I���������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - �|�b�v�A�b�v�E�B���h�E�̏I���������s���܂��B
void mu_end_popup(mu_Context *ctx) {
  mu_end_window(ctx);
}

// mu_begin_panel_ex: �p�l�����J�n����֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
//   name - �p�l���̖��O
//   opt - �I�v�V�����t���O
// �������e:
//   - �p�l���̋�`���v�Z���A�`����J�n���܂��B
//   - �p�l���̔w�i��`�悵�A�N���b�s���O��`��ݒ肵�܂��B
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

// mu_end_panel: �p�l���̏I���������s���֐�
// ����:
//   ctx - MicroUI�̃R���e�L�X�g
// �������e:
//   - �p�l���̃N���b�s���O��`���|�b�v���܂��B
//   - �R���e�i�X�^�b�N����p�l�����폜���܂��B
void mu_end_panel(mu_Context *ctx) {
  mu_pop_clip_rect(ctx);
  pop_container(ctx);
}
/////////////////////////////////////////////////////////////////////////
// ios�p�̒ǉ�

/**
 * �^�b�`�f�o�C�X�p�̏�ԋL�^�֐� - �O�t���[���̏�Ԃ�ۑ�����
 * 
 * @param ctx MicroUI�̃R���e�L�X�g
 */
void mu_store_state(mu_Context *ctx) {
    ctx->prev_hover = ctx->hover;
    ctx->prev_focus = ctx->focus;
    // �V�����t���[�����Ƃɏ�Ԃ�ۑ�
}

/**
 * �^�b�`�h���b�O�J�n���ɑΏۂ𖾎��I�Ɏw��
 * 
 * @param ctx MicroUI�̃R���e�L�X�g
 * @param id �h���b�O�Ώۂ�ID (�ʏ�̓^�C�g���o�[)
 */
void mu_begin_dragging(mu_Context *ctx, mu_Id id) {
    ctx->active_drag_id = id;
    ctx->drag_active = 1;
    ctx->hover = id;
    mu_set_focus(ctx, id);
    ctx->updated_focus = 1; // �t�H�[�J�X���X�V���ꂽ���Ƃ��}�[�N
}

/**
 * �h���b�O��Ԃ��ێ� - ��Ԃ��s�ӂɎ���ꂽ�ꍇ�ɕ�������
 * 
 * @param ctx MicroUI�̃R���e�L�X�g
 */
void mu_maintain_dragging(mu_Context *ctx) {
    // �h���b�O���Ƀz�o�[/�t�H�[�J�X������ꂽ�畜��
    if (ctx->drag_active && ctx->active_drag_id) {
        if (ctx->hover != ctx->active_drag_id) {
            ctx->hover = ctx->active_drag_id;
        }
        if (ctx->focus != ctx->active_drag_id) {
            ctx->focus = ctx->active_drag_id;
            ctx->updated_focus = 1; // �t�H�[�J�X���X�V���ꂽ���Ƃ��}�[�N
        }
    }
}

/**
 * �h���b�O�I�����ɏ�Ԃ����Z�b�g
 * 
 * @param ctx MicroUI�̃R���e�L�X�g
 */
void mu_end_dragging(mu_Context *ctx) {
    ctx->active_drag_id = 0;
    ctx->drag_active = 0;
    // �z�o�[�ƃt�H�[�J�X��mu_end�œK�؂ɏ��������
}