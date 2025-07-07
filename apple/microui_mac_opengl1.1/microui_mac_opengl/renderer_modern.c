// renderer_modern.c - OpenGL Core Profile (3.2+) renderer for MicroUI (macOS)
// シェーダーベースの実装で、iOS版との互換性を確保

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#ifdef __APPLE__
#include <TargetConditionals.h>
#define GLES_SILENCE_DEPRECATION 1
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

#if TARGET_OS_IPHONE || TARGET_OS_IOS || TARGET_IPHONE_SIMULATOR
// iOS向け OpenGL ES 2.0 インクルード
#include <OpenGLES/ES2/gl.h>
#include <OpenGLES/ES2/glext.h>
#elif TARGET_OS_OSX || TARGET_OS_MAC
// macOS向け モダン OpenGL インクルード（Core Profile）
#define GL_DO_NOT_WARN_IF_MULTI_GL_VERSION_HEADERS_INCLUDED
#include <OpenGL/gl3.h>
#endif

#else
// その他のプラットフォーム向け
#include <GL/gl3.h>
#endif

#include "renderer.h"
#define ATLAS_IMPL
#include "atlas.inl"

// シェーダソース
static const char *vertex_shader_src =
    "#version 330 core\n"
    "layout(location = 0) in vec2 a_position;\n"
    "layout(location = 1) in vec2 a_texcoord;\n"
    "layout(location = 2) in vec4 a_color;\n"
    "out vec2 v_texcoord;\n"
    "out vec4 v_color;\n"
    "uniform mat4 u_proj;\n"
    "void main() {\n"
    "  gl_Position = u_proj * vec4(a_position, 0.0, 1.0);\n"
    "  v_texcoord = a_texcoord;\n"
    "  v_color = a_color;\n"
    "}";

static const char *fragment_shader_src =
    "#version 330 core\n"
    "in vec2 v_texcoord;\n"
    "in vec4 v_color;\n"
    "out vec4 FragColor;\n"
    "uniform sampler2D u_tex;\n"
    "void main() {\n"
    "  float a = texture(u_tex, v_texcoord).a;\n"
    "  FragColor = vec4(v_color.rgb, v_color.a * a);\n"
    "}";

// 頂点構造体
struct Vertex {
    float x, y;          // 位置
    float u, v;          // テクスチャ座標
    unsigned char r, g, b, a; // 色
};

#define MAX_QUADS 4096
static struct Vertex vbuf[MAX_QUADS * 4];
static GLuint ibuf[MAX_QUADS * 6];
static int quad_count = 0;

static GLuint prog, vbo, ibo, tex, vao;
static GLint u_proj, u_tex;
static int fb_width = 800, fb_height = 600; // デフォルトサイズ

static void check_shader(GLuint shader, const char *msg) {
    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char buf[512];
        glGetShaderInfoLog(shader, sizeof(buf), NULL, buf);
        printf("Shader compile error (%s): %s\n", msg, buf);
    }
}

static void check_program(GLuint program) {
    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked) {
        char buf[512];
        glGetProgramInfoLog(program, sizeof(buf), NULL, buf);
        printf("Program link error: %s\n", buf);
    }
}

static GLuint compile_shader(GLenum type, const char *src, const char *msg) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, NULL);
    glCompileShader(s);
    check_shader(s, msg);
    return s;
}

// 初期化処理
void r_init(void) {
    // OpenGLの基本設定
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);  // 必須: シザーテスト有効化

    // シェーダプログラムのコンパイルとリンク
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader_src, "vertex");
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader_src, "fragment");
    prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    check_program(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    // ユニフォーム位置の取得
    u_proj = glGetUniformLocation(prog, "u_proj");
    u_tex = glGetUniformLocation(prog, "u_tex");

    // VAOの生成とバインド（macOS/Core Profileでは必須）
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBOとIBOの生成
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ibo);

    // インデックスバッファの初期化
    for (int i = 0, offset = 0; i < MAX_QUADS; i++, offset += 4) {
        ibuf[i * 6 + 0] = offset + 0;
        ibuf[i * 6 + 1] = offset + 1;
        ibuf[i * 6 + 2] = offset + 2;
        ibuf[i * 6 + 3] = offset + 2;
        ibuf[i * 6 + 4] = offset + 3;
        ibuf[i * 6 + 5] = offset + 0;
    }

    // インデックスバッファデータの送信
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(ibuf), ibuf, GL_STATIC_DRAW);

    // テクスチャの生成と設定
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
                 GL_RED, GL_UNSIGNED_BYTE, atlas_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 頂点属性の設定
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    
    // 位置属性 (a_position)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, x));
    
    // テクスチャ座標属性 (a_texcoord)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, u));
    
    // 色属性 (a_color)
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(struct Vertex), (void*)offsetof(struct Vertex, r));

    glBindVertexArray(0);
}

// クリーンアップ
void r_shutdown(void) {
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ibo);
    glDeleteTextures(1, &tex);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(prog);
}

// フレームバッファサイズの設定
void r_resize(int width, int height) {
    fb_width = width;
    fb_height = height;
    glViewport(0, 0, width, height);
}

// クリア処理
void r_clear(mu_Color color) {
    glClearColor(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    quad_count = 0;
}

// バッファに四角形を追加
static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
    if (quad_count >= MAX_QUADS) {
        printf("Warning: Quad buffer full\n");
        return;
    }

    // テクスチャ座標の計算
    float sx = src.x / (float) ATLAS_WIDTH;
    float sy = src.y / (float) ATLAS_HEIGHT;
    float sw = src.w / (float) ATLAS_WIDTH;
    float sh = src.h / (float) ATLAS_HEIGHT;

    // 頂点データの設定
    struct Vertex* v = &vbuf[quad_count * 4];
    
    // 左上の頂点
    v[0].x = (float) dst.x;
    v[0].y = (float) dst.y;
    v[0].u = sx;
    v[0].v = sy;
    v[0].r = color.r;
    v[0].g = color.g;
    v[0].b = color.b;
    v[0].a = color.a;
    
    // 右上の頂点
    v[1].x = (float) (dst.x + dst.w);
    v[1].y = (float) dst.y;
    v[1].u = sx + sw;
    v[1].v = sy;
    v[1].r = color.r;
    v[1].g = color.g;
    v[1].b = color.b;
    v[1].a = color.a;
    
    // 右下の頂点
    v[2].x = (float) (dst.x + dst.w);
    v[2].y = (float) (dst.y + dst.h);
    v[2].u = sx + sw;
    v[2].v = sy + sh;
    v[2].r = color.r;
    v[2].g = color.g;
    v[2].b = color.b;
    v[2].a = color.a;
    
    // 左下の頂点
    v[3].x = (float) dst.x;
    v[3].y = (float) (dst.y + dst.h);
    v[3].u = sx;
    v[3].v = sy + sh;
    v[3].r = color.r;
    v[3].g = color.g;
    v[3].b = color.b;
    v[3].a = color.a;

    quad_count++;
}

// バッチ描画の実行
static void flush(void) {
    if (quad_count == 0) {
        return;
    }

    // シェーダープログラムの使用
    glUseProgram(prog);
    
    // 投影行列の設定
    float proj[16] = {
        2.0f / fb_width, 0.0f, 0.0f, 0.0f,
        0.0f, -2.0f / fb_height, 0.0f, 0.0f,
        0.0f, 0.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f, 1.0f
    };
    glUniformMatrix4fv(u_proj, 1, GL_FALSE, proj);
    
    // テクスチャの設定
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glUniform1i(u_tex, 0);
    
    // VAOの設定
    glBindVertexArray(vao);
    
    // VBOデータの更新
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, quad_count * 4 * sizeof(struct Vertex), vbuf, GL_STREAM_DRAW);
    
    // 描画
    glDrawElements(GL_TRIANGLES, quad_count * 6, GL_UNSIGNED_INT, 0);
    
    // リセット
    glBindVertexArray(0);
    quad_count = 0;
}

// 矩形の描画
void r_draw_rect(mu_Rect rect, mu_Color color) {
    push_quad(rect, atlas[ATLAS_WHITE], color);
}

// テキストの描画
void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
    mu_Rect dst = { pos.x, pos.y, 0, 0 };
    for (const char *p = text; *p; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        int chr = mu_min((unsigned char)*p, 127);
        mu_Rect src = atlas[ATLAS_FONT + chr];
        dst.w = src.w;
        dst.h = src.h;
        push_quad(dst, src, color);
        dst.x += dst.w;
    }
}

// アイコンの描画
void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
    mu_Rect src = atlas[id];
    int x = rect.x + (rect.w - src.w) / 2;
    int y = rect.y + (rect.h - src.h) / 2;
    push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

// テキスト幅の取得
int r_get_text_width(const char* text, int len) {
    int chr, res = 0;
    if (len == -1) len = strlen(text);
    for (const char* p = text; *p && len--; p++) {
        if ((*p & 0xc0) == 0x80) continue;
        chr = mu_min((unsigned char)*p, 127);
        res += atlas[ATLAS_FONT + chr].w;
    }
    return res;
}

// テキスト高さの取得
int r_get_text_height(void) {
    return 18; // フォントの高さ（固定値）
}

// クリップ矩形の設定
void r_set_clip_rect(mu_Rect rect) {
    flush();
    glScissor(rect.x, fb_height - (rect.y + rect.h), rect.w, rect.h);
}

// 描画の完了と表示
void r_present(void) {
    flush();
    
#if defined(__APPLE__) && defined(__MACH__)
    #if TARGET_OS_IPHONE || TARGET_OS_IOS || TARGET_IPHONE_SIMULATOR
    // iOS向け: 明示的なスワップバッファは不要、EAGLContextのpresentRenderbufferが使われる
    glFinish();
    #elif TARGET_OS_OSX || TARGET_OS_MAC
    // macOS向け: 描画完了待ち
    // バッファスワップはCGLFlushDrawableまたはNSOpenGLContext等が実行
    glFinish();
    #endif
#else
    // その他のプラットフォーム向け
    glFinish();
#endif
}
