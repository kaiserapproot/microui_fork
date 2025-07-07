// 修正されたinitMicroUIメソッド
- (void)initMicroUI
{
    // エラーをクリア
    while(glGetError() != GL_NO_ERROR);
    
    // OpenGLの初期設定
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_CULL_FACE);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_TEXTURE_2D);
    
    // クライアント状態を有効にする
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    
    // atlas.inlデータをロード前にATLAS定数を表示
    NSLog(@"ATLAS constants: ATLAS_FONT=%d, ATLAS_WIDTH=%d, ATLAS_HEIGHT=%d", 
          ATLAS_FONT, ATLAS_WIDTH, ATLAS_HEIGHT);
    
    // アトラス内の文字情報をデバッグ出力
    for (int i = 0; i < 10; i++) {
        mu_Rect r = atlas[ATLAS_FONT + i];
        NSLog(@"Atlas[%d]: x=%d, y=%d, w=%d, h=%d", 
              ATLAS_FONT + i, r.x, r.y, r.w, r.h);
    }
    
    // フォントテクスチャの初期化
    glGenTextures(1, &font_texture);
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"OpenGL Error (glGenTextures): %04X", error);
        return;
    }
    
    glBindTexture(GL_TEXTURE_2D, font_texture);
    error = glGetError();
    if (error != GL_NO_ERROR) {
        NSLog(@"OpenGL Error (glBindTexture): %04X", error);
        return;
    }
    
    // atlas.inlのデータを直接使用
    // atlas配列とatlas_textureは既にatlas.inlで定義されている
    // フォントテクスチャを設定
    [self setupSystemFont];
    
    // カスタムテキスト計測関数を統合レンダラーに登録
    // 重要: テクスチャとパラメータ設定の後、レンダラー初期化の前に行う
    register_text_measurement_functions(text_width, text_height);
    
    // 統合レンダラーの初期化
    r_init();
    NSRect bounds = [self bounds];
    GLsizei viewWidth = bounds.size.width;
    GLsizei viewHeight = bounds.size.height;
    if ([self wantsBestResolutionOpenGLSurface]) {
        NSSize backingSize = [self convertSizeToBacking:bounds.size];
        viewWidth = backingSize.width;
        viewHeight = backingSize.height;
    }
    r_resize(viewWidth, viewHeight);
    NSLog(@"統合レンダラー初期化: r_init()を呼び出しました (ビューポートサイズ: %dx%d)", viewWidth, viewHeight);
    
    // MicroUIコンテキストの初期化
    mu_ctx = (mu_Context*)malloc(sizeof(mu_Context));
    if (!mu_ctx) {
        NSLog(@"Failed to allocate MicroUI context");
        return;
    }
    
    mu_init(mu_ctx);
    
    // テキスト幅と高さの計算関数を設定
    mu_ctx->text_width = text_width;
    mu_ctx->text_height = text_height;
    
    // 初期のログメッセージ
    [self writeLog:"MicroUI initialized successfully"];
    logbuf_updated = 1;
    
    NSLog(@"MicroUI initialization completed");
}
