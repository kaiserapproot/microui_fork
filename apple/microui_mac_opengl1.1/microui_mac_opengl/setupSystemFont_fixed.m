// 修正されたsetupSystemFontメソッド
- (void)setupSystemFont {
    // atlas.inlの正しいフォントテクスチャを使用
    NSLog(@"atlas.inlのフォントテクスチャを使用します");
    
    // アトラステクスチャが正しく定義されているかチェック
    BOOL hasValidData = NO;
    int nonZeroCount = 0;
    
    // データが存在するかチェック
    for (int i = 0; i < ATLAS_WIDTH * ATLAS_HEIGHT; i++) {
        if (atlas_texture[i] != 0) {
            hasValidData = YES;
            nonZeroCount++;
            if (nonZeroCount <= 10) {
                NSLog(@"atlas_texture[%d] = %d", i, atlas_texture[i]);
            }
        }
    }
    
    NSLog(@"atlas_textureの非ゼロピクセル数: %d/%d", nonZeroCount, ATLAS_WIDTH * ATLAS_HEIGHT);
    
    if (!hasValidData) {
        NSLog(@"警告: atlas_textureにデータがありません");
        [self setupFallbackFont];
        return;
    }
    
    // atlas.inlの正しいテクスチャデータをアップロード
    // OpenGL 3.2 Core Profileでは、GL_ALPHAの代わりにGL_REDを使用する必要がある
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                GL_RED, GL_UNSIGNED_BYTE, atlas_texture);
    
    // テクスチャパラメータの設定
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST); // シャープなピクセルフォント用
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
}
