# DirectX11�V�����_���[(dx11_new_render.c, dx11_new_main.c) �\������Ȃ��������̗���

## 1. ���̊T�v
DirectX9��(main.c, renderer.c)�⋌DirectX11��(dx11_renderer.c, dx11_main.c)�ł�microui��UI������ɕ\������Ă������A
�V�����쐬����dx11_new_render.c, dx11_new_main.c�ł͉����\������Ȃ������B

---
## 2. ����

### (1) WM_PAINT�ŕ`�揈�����Ă΂�Ă��Ȃ�����
- �V��(dx11_new_main.c)�̃E�B���h�E�v���V�[�W��WM_PAINT�ŕ`��֐�(r_draw)���Ă΂�Ă��Ȃ������B
- ���C�����[�v�ł̂ݕ`�悵�Ă������߁AWindows�̍ĕ`��^�C�~���O�ƍ��킸�A�E�B���h�E�\�������C�x���g���ɉ����`�悳��Ȃ������B

### (2) ���_�\���̂̃p�f�B���O�s��v
- dx11_new_render.h��Vertex�\���̂ɕs�v�ȃp�f�B���O(_pad[12])������A�V�F�[�_�[�̓��̓��C�A�E�g�ƕs��v�B
- ���_�o�b�t�@�̃T�C�Y�����킸�A�`�悪�������s���Ȃ������\���B

### (3) �V�F�[�_�[��COLOR�l�̐��K��
- ���_�V�F�[�_�[��color/255.0f�̂悤�Ȑ��K�������Ă������A���̓��C�A�E�g�ƃo�b�t�@�̌^����v���Ă��Ȃ��ꍇ�A�F�����������f����Ȃ��B

### (4) �t���[�������������s��
- r_draw()��DirectX11�̊e��X�e�[�g(�����_�[�^�[�Q�b�g�A�V�F�[�_�[�A�C���v�b�g���C�A�E�g�A�u�����h�A���X�^���C�U��)�̐ݒ肪�s�����Ă����B
- dx11_begin_frame()�����̏����������Ă����B

### (5) �p���I�ȍĕ`��v���s��
- ���[�U�[����(�}�E�X�E�L�[����)����InvalidateRect���Ă΂�Ă��Ȃ��������߁AUI���X�V����Ȃ����Ƃ��������B

---
## 3. �C������

1. WM_PAINT��r_draw()���Ăяo���悤�C��
2. Vertex�\���̂̃p�f�B���O(_pad)���폜���A20�o�C�g�\���ɓ���
3. �V�F�[�_�[��COLOR���K�����폜���A�o�b�t�@�ƈ�v������
4. r_draw()��dx11_begin_frame()�����̏�����������ǉ�
5. ���C�����[�v����̕`�揈�����폜���AWM_PAINT�ł̂ݕ`��
6. �}�E�X�E�L�[���͎���InvalidateRect�ōĕ`��v����ǉ�

---
## 4. �C���O��̃\�[�X�Δ�

### (1) WM_PAINT
#### �C���O (dx11_new_main.c)
```c
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}
```
#### �C����
```c
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    r_draw();
    EndPaint(hwnd, &ps);
    return 0;
}
```

### (2) ���_�\����
#### �C���O (dx11_new_render.h)
```c
struct Vertex {
    float pos[2];
    float uv[2];
    unsigned char color[4];
    unsigned char _pad[12]; // �s�v�ȃp�f�B���O
};
```
#### �C����
```c
struct Vertex {
    float pos[2];
    float uv[2];
    unsigned char color[4];
};
```

### (3) �V�F�[�_�[��COLOR���K��
#### �C���O
```c
output.color = input.color / 255.0f;
```
#### �C����
```c
output.color = input.color;
```

### (4) �t���[������������
#### �C���O
```c
void r_draw(void) {
    // ...
    process_frame(g_ctx);
    r_clear(...);
    // ...
}
```
#### �C����
```c
void r_draw(void) {
    // �t���[��������
    g_context->lpVtbl->OMSetRenderTargets(...);
    g_context->lpVtbl->IASetInputLayout(...);
    g_context->lpVtbl->VSSetShader(...);
    g_context->lpVtbl->PSSetShader(...);
    g_context->lpVtbl->PSSetSamplers(...);
    g_context->lpVtbl->PSSetShaderResources(...);
    g_context->lpVtbl->OMSetBlendState(...);
    g_context->lpVtbl->RSSetState(...);
    g_context->lpVtbl->IASetPrimitiveTopology(...);
    UpdateProjectionMatrix();
    process_frame(g_ctx);
    r_clear(...);
    // ...
}
```

### (5) ���C�����[�v�̕`��폜
#### �C���O
```c
if (currentTime - lastTime > 16) {
    r_draw();
    lastTime = currentTime;
}
```
#### �C����
```c
// ���C�����[�v�ł͕`�悹���AWM_PAINT�ł̂ݕ`��
Sleep(1);
```

### (6) �p���I�ĕ`��v��
#### �C���O
```c
// �}�E�X�E�L�[���͎���InvalidateRect����
```
#### �C����
```c
InvalidateRect(hwnd, NULL, FALSE); // �}�E�X�E�L�[���͎��ɒǉ�
```

---
## 5. DirectX9��DirectX11�̈Ⴂ

| ���� | DirectX9 | DirectX11 |
|------|----------|-----------|
| API | �Œ�@�\�p�C�v���C�� | ���S�v���O���}�u���p�C�v���C��(HLSL) |
| ���_�o�b�t�@ | Lock/Unlock | Map/Unmap |
| �V�F�[�_�[ | FVF, �e�N�X�`���X�e�[�W | HLSL, ���̓��C�A�E�g, �V�F�[�_�[�R���p�C�� |
| �e�N�X�`�� | SetTexture | PSSetShaderResources(SRV) |
| �N���b�s���O | SetScissorRect, D3DRS_SCISSORTESTENABLE | RSSetScissorRects, RasterizerState |
| ���W�ϊ� | �v���W�F�N�V�����s�� | ���_�V�F�[�_�[�Ő��K�����W�ϊ� |
| �R�}���h���� | mu_Command���ڏ��� | �ꎞ�o�b�t�@(SimpleCommand)�Œ~�ρE�`�� |
| �t���[���Ǘ� | BeginScene/EndScene | OMSetRenderTargets, Present |

---
## 6. �܂Ƃ�
- �\������Ȃ����������WM_PAINT�ŕ`�揈�����Ă΂�Ă��Ȃ��������ƁB
- ���_�\���́E�V�F�[�_�[�E�t���[���������Ȃǂ�DirectX11�̎d�l�ɍ��킹�ďC�����K�v�������B
- DirectX9��DirectX11�ł�API�E�p�C�v���C���E���\�[�X�Ǘ��E���W�ϊ��ȂǍ��{�I�ȈႢ�����邽�߁A�ڐA���͍ו��܂Œ��ӂ��K�v�B
- �{�C���ɂ��dx11_new_render.c, dx11_new_main.c�ł�microui��UI������ɕ\�������悤�ɂȂ����B
