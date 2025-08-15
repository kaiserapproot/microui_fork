#ifndef DX11_ATLAS_H
#define DX11_ATLAS_H

#include "microui.h"



// �Ǝ��̃A�g���X�C���f�b�N�X (microui.h�Ƃ̋����������)
#define DX11_ATLAS_WHITE (MU_ICON_MAX)
#define DX11_ATLAS_FONT (MU_ICON_MAX + 1)

// �Ǝ��̃R�}���h�^�C�v (microui.h�Ƌ������Ȃ��悤���O��ύX)
enum DX11CommandType {
    DX11_COMMAND_NONE,
    DX11_COMMAND_RECT,
    DX11_COMMAND_TEXT,
    DX11_COMMAND_CLIP,
    DX11_COMMAND_ICON
};

#endif // DX11_ATLAS_H