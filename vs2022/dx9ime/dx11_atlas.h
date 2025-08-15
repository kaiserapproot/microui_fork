#ifndef DX11_ATLAS_H
#define DX11_ATLAS_H

#include "microui.h"



// 独自のアトラスインデックス (microui.hとの競合を避ける)
#define DX11_ATLAS_WHITE (MU_ICON_MAX)
#define DX11_ATLAS_FONT (MU_ICON_MAX + 1)

// 独自のコマンドタイプ (microui.hと競合しないよう名前を変更)
enum DX11CommandType {
    DX11_COMMAND_NONE,
    DX11_COMMAND_RECT,
    DX11_COMMAND_TEXT,
    DX11_COMMAND_CLIP,
    DX11_COMMAND_ICON
};

#endif // DX11_ATLAS_H