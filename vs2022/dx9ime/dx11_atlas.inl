#ifndef DX11_ATLAS_INL
#define DX11_ATLAS_INL

// dx11_atlas.h �Œ�`�������̂��g�p
#include "dx11_atlas.h"

// atlas_texture �� atlas.inl ����C���N���[�h
static unsigned char atlas_texture[ATLAS_WIDTH * ATLAS_HEIGHT] = {
  /* ... atlas texture data from atlas.inl ... */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  // �ȗ����̂��߁A�ȉ����ۂ̃f�[�^�͏ȗ�����Ă��܂�
  // ���ۂ̎����ł� atlas.inl ����f�[�^���R�s�[���Ă�������
};

// atlas �̋�`�f�[�^
static mu_Rect atlas[] = {
  {0,0,0,0},
  { 88, 68, 16, 16 },  // MU_ICON_CLOSE
  { 0, 0, 18, 18 },    // MU_ICON_CHECK
  { 113, 68, 5, 7 },   // MU_ICON_COLLAPSED
  { 118, 68, 7, 5 },   // MU_ICON_EXPANDED
  { 125, 68, 3, 3 },   // DX11_ATLAS_WHITE (formerly ATLAS_WHITE)
  // ... �c��̃A�g���X�f�[�^
};

#endif // DX11_ATLAS_INL