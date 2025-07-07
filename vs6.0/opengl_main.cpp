#define GLEW_STATIC
#include "GL/glew.h"
#include "GLFW/glfw3.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"				// for .png loading
#include "../../imgui.h"
#ifdef _MSC_VER
#pragma warning (disable: 4996)		// 'This function or variable may be unsafe': strcpy, strdup, sprintf, vsnprintf, sscanf, fopen
#endif

static GLFWwindow* window;
static GLuint vbo;
static GLuint vao;
static GLuint vertexShader;
static GLuint fragmentShader;
static GLuint shaderProgram;
static GLuint fontTex;
static GLint uniMVP;
static GLint uniClipRect;

static void ImImpl_RenderDrawLists(ImDrawList** const cmd_lists, int cmd_lists_count)
{
	int n;
	size_t total_vtx_count = 0;
	for ( n = 0; n < cmd_lists_count; n++)
		total_vtx_count += cmd_lists[n]->vtx_buffer.size();
	if (total_vtx_count == 0)
		return;

	int read_pos_clip_rect_buf = 0;		// offset in 'clip_rect_buffer'. each PushClipRect command consume 1 of those.

	ImVector<ImVec4> clip_rect_stack;
	clip_rect_stack.push_back(ImVec4(-9999,-9999,+9999,+9999));

	// Setup orthographic projection
	const float L = 0.0f;
	const float R = ImGui::GetIO().DisplaySize.x;
	const float B = ImGui::GetIO().DisplaySize.y;
	const float T = 0.0f;
	const float mvp[4][4] = 
	{
		{ 2.0f/(R-L),	0.0f,			0.0f,		0.0f },
		{ 0.0f,			2.0f/(T-B),		0.0f,		0.0f },
		{ 0.0f,			0.0f,			-1.0f,		0.0f },
		{ -(R+L)/(R-L),	-(T+B)/(T-B),	0.0f,		1.0f },
	};

	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBindVertexArray(vao);
	glBufferData(GL_ARRAY_BUFFER, total_vtx_count * sizeof(ImDrawVert), NULL, GL_STREAM_DRAW);
	unsigned char* buffer_data = (unsigned char*)glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);
	if (!buffer_data)
		return;
	int vtx_consumed = 0;
	for (n = 0; n < cmd_lists_count; n++)
	{
		const ImDrawList* cmd_list = cmd_lists[n];
		if (!cmd_list->vtx_buffer.empty())
		{
			memcpy(buffer_data, &cmd_list->vtx_buffer[0], cmd_list->vtx_buffer.size() * sizeof(ImDrawVert));
			buffer_data += cmd_list->vtx_buffer.size() * sizeof(ImDrawVert);
			vtx_consumed += cmd_list->vtx_buffer.size();
		}
	}
	glUnmapBuffer(GL_ARRAY_BUFFER);
	
	glUseProgram(shaderProgram);
	glUniformMatrix4fv(uniMVP, 1, GL_FALSE, &mvp[0][0]);

	// Setup render state: alpha-blending enabled, no face culling, no depth testing
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);

	glBindTexture(GL_TEXTURE_2D, fontTex);

	vtx_consumed = 0;						// offset in vertex buffer. each command consume ImDrawCmd::vtx_count of those
	bool clip_rect_dirty = true;

	for ( n = 0; n < cmd_lists_count; n++)
	{
		const ImDrawList* cmd_list = cmd_lists[n];
		if (cmd_list->commands.empty() || cmd_list->vtx_buffer.empty())
			continue;
		const ImDrawCmd* pcmd = &cmd_list->commands.front();
		const ImDrawCmd* pcmd_end = &cmd_list->commands.back();
		int clip_rect_buf_consumed = 0;		// offset in cmd_list->clip_rect_buffer. each PushClipRect command consume 1 of those.
		while (pcmd <= pcmd_end)
		{
			const ImDrawCmd& cmd = *pcmd++;
			switch (cmd.cmd_type)
			{
			case ImDrawCmdType_DrawTriangleList:
				if (clip_rect_dirty)
				{
					glUniform4fv(uniClipRect, 1, (float*)&clip_rect_stack.back());
					clip_rect_dirty = false;
				}
				glDrawArrays(GL_TRIANGLES, vtx_consumed, cmd.vtx_count);
				vtx_consumed += cmd.vtx_count;
				break;

			case ImDrawCmdType_PushClipRect:
				clip_rect_stack.push_back(cmd_list->clip_rect_buffer[clip_rect_buf_consumed++]);
				clip_rect_dirty = true;
				break;

			case ImDrawCmdType_PopClipRect:
				clip_rect_stack.pop_back();
				clip_rect_dirty = true;
				break;
			}
		}
	}
}

static const char* ImImpl_GetClipboardTextFn()
{
	return glfwGetClipboardString(window);
}

static void ImImpl_SetClipboardTextFn(const char* text, const char* text_end)
{
	if (!text_end)
		text_end = text + strlen(text);

	char* buf = (char*)malloc(text_end - text + 1);
	memcpy(buf, text, text_end-text);
	buf[text_end-text] = '\0';
	glfwSetClipboardString(window, buf);
	free(buf);
}

// Shader sources
// FIXME-OPT: clip at vertex level
const GLchar* vertexSource =
    "#version 150 core\n"
	"uniform mat4 MVP;"
    "in vec2 i_pos;"
	"in vec2 i_uv;"
	"in vec4 i_col;"
	"out vec4 col;"
	"out vec2 pixel_pos;"
	"out vec2 uv;"
    "void main() {"
	"   col = i_col;"
	"   pixel_pos = i_pos;"
	"   uv = i_uv;"
	"   gl_Position = MVP * vec4(i_pos.x, i_pos.y, 0.0f, 1.0f);"
    "}";

const GLchar* fragmentSource =
    "#version 150 core\n"
	"uniform sampler2D Tex;"
	"uniform vec4 ClipRect;"
	"in vec4 col;"
	"in vec2 pixel_pos;"
	"in vec2 uv;"
    "out vec4 o_col;"
    "void main() {"
    "   o_col = texture(Tex, uv) * col;"
	//"   if (pixel_pos.x < ClipRect.x || pixel_pos.y < ClipRect.y || pixel_pos.x > ClipRect.z || pixel_pos.y > ClipRect.w) discard;"						// Clipping: using discard	
	//"   if (step(ClipRect.x,pixel_pos.x) * step(ClipRect.y,pixel_pos.y) * step(pixel_pos.x,ClipRect.z) * step(pixel_pos.y,ClipRect.w) < 1.0f) discard;"	// Clipping: using discard and step
	"   o_col.w *= (step(ClipRect.x,pixel_pos.x) * step(ClipRect.y,pixel_pos.y) * step(pixel_pos.x,ClipRect.z) * step(pixel_pos.y,ClipRect.w));"			// Clipping: branch-less, set alpha 0.0f
    "}";

static void glfw_error_callback(int error, const char* description)
{
    fputs(description, stderr);
}

static float mouse_wheel = 0.0f;
static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	mouse_wheel = (float)yoffset;
}

static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	ImGuiIO& io = ImGui::GetIO();
	if (action == GLFW_PRESS)
		io.KeysDown[key] = true;
	if (action == GLFW_RELEASE)
		io.KeysDown[key] = false;
	io.KeyCtrl = (mods & GLFW_MOD_CONTROL) != 0;
	io.KeyShift = (mods & GLFW_MOD_SHIFT) != 0;
}

static void glfw_char_callback(GLFWwindow* window, unsigned int c)
{
	if (c > 0 && c <= 255)
		ImGui::GetIO().AddInputCharacter((char)c);
}

// OpenGL code based on http://open.gl tutorials
void InitGL()
{
	glfwSetErrorCallback(glfw_error_callback);

    if (!glfwInit())
        exit(1);

	//glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	//glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
	glfwWindowHint(GLFW_REFRESH_RATE, 60);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	window = glfwCreateWindow(1280, 720, "ImGui OpenGL example", NULL, NULL);
	glfwMakeContextCurrent(window);

	glfwSetKeyCallback(window, glfw_key_callback);
	glfwSetScrollCallback(window, glfw_scroll_callback);
	glfwSetCharCallback(window, glfw_char_callback);

	glewExperimental = GL_TRUE;
	glewInit();

	GLenum err = GL_NO_ERROR;
	GLint status = GL_TRUE;
	err = glGetError(); IM_ASSERT(err == GL_NO_ERROR);

    // Create and compile the vertex shader
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexSource, NULL);
    glCompileShader(vertexShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
	if (status != GL_TRUE)
	{
		char buffer[512];
		glGetShaderInfoLog(vertexShader, 1024, NULL, buffer);
		printf("%s", buffer);
		IM_ASSERT(status == GL_TRUE);
	}

    // Create and compile the fragment shader
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fragmentSource, NULL);
    glCompileShader(fragmentShader);
	glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &status);
	IM_ASSERT(status == GL_TRUE);

    // Link the vertex and fragment shader into a shader program
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glBindFragDataLocation(shaderProgram, 0, "o_col");
    glLinkProgram(shaderProgram);
	glGetProgramiv(shaderProgram, GL_LINK_STATUS, &status);
	IM_ASSERT(status == GL_TRUE);

	uniMVP = glGetUniformLocation(shaderProgram, "MVP");
	uniClipRect = glGetUniformLocation(shaderProgram, "ClipRect");

	// Create Vertex Buffer Objects & Vertex Array Objects
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	
	GLint posAttrib = glGetAttribLocation(shaderProgram, "i_pos");
	glVertexAttribPointer(posAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), 0);
	glEnableVertexAttribArray(posAttrib);

	GLint uvAttrib = glGetAttribLocation(shaderProgram, "i_uv");
	glEnableVertexAttribArray(uvAttrib);
	glVertexAttribPointer(uvAttrib, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (void*)(2*sizeof(float)));

	GLint colAttrib = glGetAttribLocation(shaderProgram, "i_col");
	glVertexAttribPointer(colAttrib, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (void*)(4*sizeof(float)));
	glEnableVertexAttribArray(colAttrib);
	err = glGetError(); IM_ASSERT(err == GL_NO_ERROR);
}

void InitImGui()
{
	int w, h;
	glfwGetWindowSize(window, &w, &h);

	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2((float)w, (float)h);
	io.DeltaTime = 1.0f/60.0f;
	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;
	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	io.RenderDrawListsFn = ImImpl_RenderDrawLists;
	io.SetClipboardTextFn = ImImpl_SetClipboardTextFn;
	io.GetClipboardTextFn = ImImpl_GetClipboardTextFn;

	// Load font texture
	glGenTextures(1, &fontTex);
	glBindTexture(GL_TEXTURE_2D, fontTex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	const void* png_data;
	unsigned int png_size;
	ImGui::GetDefaultFontData(NULL, NULL, &png_data, &png_size);
	int tex_x, tex_y, tex_comp;
	void* tex_data = stbi_load_from_memory((const unsigned char*)png_data, (int)png_size, &tex_x, &tex_y, &tex_comp, 0);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex_x, tex_y, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex_data);
	stbi_image_free(tex_data);
}

void Shutdown()
{
	ImGui::Shutdown();

    glDeleteProgram(shaderProgram);
    glDeleteShader(fragmentShader);
    glDeleteShader(vertexShader);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);

	glfwTerminate();
}

int main(int argc, char** argv)
{
	InitGL();
	InitImGui();

	double time = glfwGetTime();
	while (!glfwWindowShouldClose(window))
	{
		ImGuiIO& io = ImGui::GetIO();
		glfwPollEvents();

		// 1) ImGui start frame, setup time delta & inputs
		const double current_time =  glfwGetTime();
		io.DeltaTime = (float)(current_time - time);
		time = current_time;
		double mouse_x, mouse_y;
		glfwGetCursorPos(window, &mouse_x, &mouse_y);
		io.MousePos = ImVec2((float)mouse_x, (float)mouse_y);
		io.MouseDown[0] = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) != 0;
		io.MouseDown[1] = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) != 0;
		io.MouseWheel = (mouse_wheel != 0) ? mouse_wheel > 0.0f ? 1 : - 1 : 0;
		mouse_wheel = 0.0f;
		ImGui::NewFrame();

		// 2) ImGui usage
		static bool show_test_window = true;
		static bool show_another_window = false;
		static float f;
		ImGui::Text("Hello, world!");
		ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
		show_test_window ^= ImGui::Button("Test Window");
		show_another_window ^= ImGui::Button("Another Window");

		// Calculate and show framerate
		static float ms_per_frame[120] = { 0 };
		static int ms_per_frame_idx = 0;
		static float ms_per_frame_accum = 0.0f;
		ms_per_frame_accum -= ms_per_frame[ms_per_frame_idx];
		ms_per_frame[ms_per_frame_idx] = io.DeltaTime * 1000.0f;
		ms_per_frame_accum += ms_per_frame[ms_per_frame_idx];
		ms_per_frame_idx = (ms_per_frame_idx + 1) % 120;
		const float ms_per_frame_avg = ms_per_frame_accum / 120;
		ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", ms_per_frame_avg, 1000.0f / ms_per_frame_avg);

		if (show_test_window)
		{
			// More example code in ShowTestWindow()
			ImGui::SetNewWindowDefaultPos(ImVec2(650, 20));		// Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
			ImGui::ShowTestWindow(&show_test_window);
		}

		if (show_another_window)
		{
			ImGui::Begin("Another Window", &show_another_window, ImVec2(200,100));
			ImGui::Text("Hello");
			ImGui::End();
		}

        // 3) Render
        glClearColor(0.8f, 0.6f, 0.6f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
		ImGui::Render();

		glfwSwapBuffers(window);
	}

	Shutdown();

	return 0;
}
