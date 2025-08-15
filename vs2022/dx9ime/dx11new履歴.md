# DirectX11新レンダラー(dx11_new_render.c, dx11_new_main.c) 表示されなかった問題の履歴

## 1. 問題の概要
DirectX9版(main.c, renderer.c)や旧DirectX11版(dx11_renderer.c, dx11_main.c)ではmicrouiのUIが正常に表示されていたが、
新しく作成したdx11_new_render.c, dx11_new_main.cでは何も表示されなかった。

---
## 2. 原因

### (1) WM_PAINTで描画処理が呼ばれていなかった
- 新版(dx11_new_main.c)のウィンドウプロシージャWM_PAINTで描画関数(r_draw)が呼ばれていなかった。
- メインループでのみ描画していたため、Windowsの再描画タイミングと合わず、ウィンドウ表示直後やイベント時に何も描画されなかった。

### (2) 頂点構造体のパディング不一致
- dx11_new_render.hのVertex構造体に不要なパディング(_pad[12])があり、シェーダーの入力レイアウトと不一致。
- 頂点バッファのサイズが合わず、描画が正しく行われなかった可能性。

### (3) シェーダーでCOLOR値の正規化
- 頂点シェーダーでcolor/255.0fのような正規化をしていたが、入力レイアウトとバッファの型が一致していない場合、色が正しく反映されない。

### (4) フレーム初期化処理不足
- r_draw()でDirectX11の各種ステート(レンダーターゲット、シェーダー、インプットレイアウト、ブレンド、ラスタライザ等)の設定が不足していた。
- dx11_begin_frame()相当の処理が抜けていた。

### (5) 継続的な再描画要求不足
- ユーザー操作(マウス・キー入力)時にInvalidateRectが呼ばれていなかったため、UIが更新されないことがあった。

---
## 3. 修正項目

1. WM_PAINTでr_draw()を呼び出すよう修正
2. Vertex構造体のパディング(_pad)を削除し、20バイト構造に統一
3. シェーダーのCOLOR正規化を削除し、バッファと一致させる
4. r_draw()にdx11_begin_frame()相当の初期化処理を追加
5. メインループからの描画処理を削除し、WM_PAINTでのみ描画
6. マウス・キー入力時にInvalidateRectで再描画要求を追加

---
## 4. 修正前後のソース対比

### (1) WM_PAINT
#### 修正前 (dx11_new_main.c)
```c
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    EndPaint(hwnd, &ps);
    return 0;
}
```
#### 修正後
```c
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    r_draw();
    EndPaint(hwnd, &ps);
    return 0;
}
```

### (2) 頂点構造体
#### 修正前 (dx11_new_render.h)
```c
struct Vertex {
    float pos[2];
    float uv[2];
    unsigned char color[4];
    unsigned char _pad[12]; // 不要なパディング
};
```
#### 修正後
```c
struct Vertex {
    float pos[2];
    float uv[2];
    unsigned char color[4];
};
```

### (3) シェーダーのCOLOR正規化
#### 修正前
```c
output.color = input.color / 255.0f;
```
#### 修正後
```c
output.color = input.color;
```

### (4) フレーム初期化処理
#### 修正前
```c
void r_draw(void) {
    // ...
    process_frame(g_ctx);
    r_clear(...);
    // ...
}
```
#### 修正後
```c
void r_draw(void) {
    // フレーム初期化
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

### (5) メインループの描画削除
#### 修正前
```c
if (currentTime - lastTime > 16) {
    r_draw();
    lastTime = currentTime;
}
```
#### 修正後
```c
// メインループでは描画せず、WM_PAINTでのみ描画
Sleep(1);
```

### (6) 継続的再描画要求
#### 修正前
```c
// マウス・キー入力時にInvalidateRect無し
```
#### 修正後
```c
InvalidateRect(hwnd, NULL, FALSE); // マウス・キー入力時に追加
```

---
## 5. DirectX9とDirectX11の違い

| 項目 | DirectX9 | DirectX11 |
|------|----------|-----------|
| API | 固定機能パイプライン | 完全プログラマブルパイプライン(HLSL) |
| 頂点バッファ | Lock/Unlock | Map/Unmap |
| シェーダー | FVF, テクスチャステージ | HLSL, 入力レイアウト, シェーダーコンパイル |
| テクスチャ | SetTexture | PSSetShaderResources(SRV) |
| クリッピング | SetScissorRect, D3DRS_SCISSORTESTENABLE | RSSetScissorRects, RasterizerState |
| 座標変換 | プロジェクション行列 | 頂点シェーダーで正規化座標変換 |
| コマンド処理 | mu_Command直接処理 | 一時バッファ(SimpleCommand)で蓄積・描画 |
| フレーム管理 | BeginScene/EndScene | OMSetRenderTargets, Present |

---
## 6. まとめ
- 表示されなかった主因はWM_PAINTで描画処理が呼ばれていなかったこと。
- 頂点構造体・シェーダー・フレーム初期化などもDirectX11の仕様に合わせて修正が必要だった。
- DirectX9とDirectX11ではAPI・パイプライン・リソース管理・座標変換など根本的な違いがあるため、移植時は細部まで注意が必要。
- 本修正によりdx11_new_render.c, dx11_new_main.cでもmicrouiのUIが正常に表示されるようになった。
