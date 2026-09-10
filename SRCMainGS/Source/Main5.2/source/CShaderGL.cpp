#include "stdafx.h"
#include "CShaderGL.h"
#include "ModernGraphicsBootstrap.h"

#ifdef SHADER_VERSION_TEST
#include "Utilities/Log/muConsoleDebug.h"

CShaderGL::CShaderGL()
{
	shader_id = 0;
}

CShaderGL::~CShaderGL()
{
	glDeleteProgram(shader_id);
}

void CShaderGL::Init()
{
	std::string vertex_shader;

	if (!readshader("Shaders\\shader.vs", vertex_shader))
	{
		return;
	}

	std::string frgmen_shader;

	if (!readshader("Shaders\\shader.fs", frgmen_shader))
	{
		return;
	}

	GLuint shader_vertex = run_shader(vertex_shader.data(), GL_VERTEX_SHADER);

	GLuint shader_frgmen = run_shader(frgmen_shader.data(), GL_FRAGMENT_SHADER);

	shader_id = glCreateProgram();
	glAttachShader(shader_id, shader_vertex);
	glAttachShader(shader_id, shader_frgmen);
	glLinkProgram(shader_id);

	int success;
	glGetProgramiv(shader_id, GL_LINK_STATUS, &success);

	if (!success)
	{
		char infoLog[512];
		glGetProgramInfoLog(shader_id, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al enlazar el Shader Program:");
		g_ConsoleDebug->Write(5, infoLog);
	}

	// Eliminar los shaders compilados
	glDeleteShader(shader_vertex);
	glDeleteShader(shader_frgmen);
}

void CShaderGL::RenderShader()
{
	if (this->CheckedShader())
	{
		glUseProgram(shader_id);
	}
}

bool CShaderGL::CheckedShader()
{
	return (shader_id != 0);
}

GLuint CShaderGL::GetShaderId()
{
	return shader_id;
}

bool CShaderGL::readshader(const char* filename, std::string& shader_text)
{
	FILE* compressedFile = fopen(filename, "rb");

	if (compressedFile)
	{
		fseek(compressedFile, 0, SEEK_END);
		long fileSize = ftell(compressedFile);
		fseek(compressedFile, 0, SEEK_SET);

		shader_text.resize(fileSize, 0);
		fread(shader_text.data(), 1, fileSize, compressedFile);
		fclose(compressedFile);

		return true;
	}

	return false;
}

GLuint CShaderGL::run_shader(const char* shader_text, GLenum type)
{
	GLuint shader = glCreateShader(type);
	glShaderSource(shader, 1, &shader_text, NULL);
	glCompileShader(shader);

	// Verificar errores de compilación
	int success;
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

	if (!success)
	{
		char infoLog[512];
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		g_ConsoleDebug->Write(5, "Error al compilar shader:");
		g_ConsoleDebug->Write(5, infoLog);
	}

	return shader;
}

void CShaderGL::run_projection()
{
	if (shader_id != 0)
	{
		glUseProgram(shader_id);

		glm::mat4 view = glm::mat4(1.0f);
		glm::mat4 model = glm::mat4(1.0f);

		view = glm::rotate(view, glm::radians(CameraAngle[1]), glm::vec3(0.0f, 1.0f, 0.0f));
		if (CameraTopViewEnable == false)
			view = glm::rotate(view, glm::radians(CameraAngle[0]), glm::vec3(1.0f, 0.0f, 0.0f));
		view = glm::rotate(view, glm::radians(CameraAngle[2]), glm::vec3(0.0f, 0.0f, 1.0f));

		view = glm::translate(view, glm::vec3(-CameraPosition[0], -CameraPosition[1], -CameraPosition[2]));

		this->setMat4("view", view);
		this->setMat4("model", model);

		glUseProgram(0);

		glUniform1i(glGetUniformLocation(shader_id, "texture1"), 0);

	}
}

void CShaderGL::SetPerspective(float Fov, float Aspect, float ZNear, float ZFar)
{
	if (shader_id != 0)
	{
		glUseProgram(shader_id);
		glm::mat4 projection = glm::perspective(glm::radians(Fov), Aspect, ZNear, ZFar);
		this->setMat4("projection", projection);
		glUseProgram(0);
	}
}

// Funciones para establecer uniforms
void CShaderGL::setBool(const char* name, bool value) const
{
	glUniform1i(glGetUniformLocation(shader_id, name), (int)value);
}

void CShaderGL::setInt(const char* name, int value) const
{
	glUniform1i(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setFloat(const char* name, float value) const
{
	glUniform1f(glGetUniformLocation(shader_id, name), value);
}

void CShaderGL::setVec2(const char* name, float x, float y) const
{
	glUniform2f(glGetUniformLocation(shader_id, name), x, y);
}

void CShaderGL::setVec3(const char* name, float x, float y, float z) const
{
	glUniform3f(glGetUniformLocation(shader_id, name), x, y, z);
}

void CShaderGL::setVec4(const char* name, float x, float y, float z, float w) const
{
	glUniform4f(glGetUniformLocation(shader_id, name), x, y, z, w);
}

void CShaderGL::setMat4(const char* name, glm::mat4& matrix) const
{
	glUniformMatrix4fv(glGetUniformLocation(shader_id, name), 1, GL_FALSE, glm::value_ptr(matrix));
}

CShaderGL* CShaderGL::Instance()
{
	static CShaderGL sInstance;
	return &sInstance;
}
#endif // SHADER_VERSION_TEST

// Phase 2 bootstrap implementation is compiled through this translation unit
// because CShaderGL.cpp is already part of Main.vcxproj. Once Diligent is
// vendored in dependencies and the project file is updated, this implementation
// can move to its own .cpp without changing the public contract.
#include "ModernGraphicsBootstrap.inl"

#ifdef MU_ENABLE_DILIGENT
// Temporary Phase 2 coexistence bridge.
//
// The legacy lifecycle lives in large Winmain.cpp/WINHANDLE.cpp units. During
// the bootstrap phase we observe messages on the main UI thread instead of
// duplicating or replacing the legacy window/context/presentation code. This
// keeps the original SwapBuffers path authoritative and releases Diligent before
// every currently known message path that can tear down the WGL context.
namespace
{
HHOOK g_ModernGraphicsWindowHook = NULL;

void TryAttachModernGraphics(HWND hWnd)
{
	HGLRC currentContext = wglGetCurrentContext();
	HDC currentDC = wglGetCurrentDC();

	if (currentContext == NULL || currentDC == NULL)
		return;

	if (WindowFromDC(currentDC) != hWnd)
		return;

	CModernGraphicsBootstrap& graphics = GetModernGraphics();
	if (graphics.HasAttemptedInitializationFor(hWnd, currentContext))
		return;

	RECT clientRect = { 0, 0, 0, 0 };
	if (!GetClientRect(hWnd, &clientRect))
		return;

	const unsigned int width = clientRect.right > clientRect.left
		? static_cast<unsigned int>(clientRect.right - clientRect.left)
		: 0u;
	const unsigned int height = clientRect.bottom > clientRect.top
		? static_cast<unsigned int>(clientRect.bottom - clientRect.top)
		: 0u;

	graphics.InitializeOpenGL46(hWnd, currentDC, currentContext, width, height);
}

LRESULT CALLBACK ModernGraphicsCallWndProc(int code, WPARAM wParam, LPARAM lParam)
{
	if (code >= 0 && lParam != 0)
	{
		const CWPSTRUCT* message = reinterpret_cast<const CWPSTRUCT*>(lParam);

		switch (message->message)
		{
		case WM_CLOSE:
		case WM_DESTROY:
		case WM_NCDESTROY:
		case WM_USER_MEMORYHACK:
			if (GetModernGraphics().IsAttachedToWindow(message->hwnd))
				GetModernGraphics().Shutdown();
			break;

		default:
			TryAttachModernGraphics(message->hwnd);

			if (message->message == WM_SIZE && message->wParam != SIZE_MINIMIZED)
			{
				CModernGraphicsBootstrap& graphics = GetModernGraphics();
				if (graphics.IsAttachedToWindow(message->hwnd))
				{
					const unsigned int width = static_cast<unsigned int>(LOWORD(message->lParam));
					const unsigned int height = static_cast<unsigned int>(HIWORD(message->lParam));
					if (width != 0u && height != 0u)
						graphics.OnResize(width, height);
				}
			}
			break;
		}
	}

	return CallNextHookEx(g_ModernGraphicsWindowHook, code, wParam, lParam);
}

class CModernGraphicsLifecycleBridge
{
public:
	CModernGraphicsLifecycleBridge()
	{
		g_ModernGraphicsWindowHook = SetWindowsHookEx(
			WH_CALLWNDPROC,
			ModernGraphicsCallWndProc,
			NULL,
			GetCurrentThreadId());

		if (g_ModernGraphicsWindowHook == NULL)
			ModernGraphicsLog("[ModernGraphics] Failed to install the Phase 2 Win32 lifecycle bridge.\n");
	}

	~CModernGraphicsLifecycleBridge()
	{
		if (g_ModernGraphicsWindowHook != NULL)
		{
			UnhookWindowsHookEx(g_ModernGraphicsWindowHook);
			g_ModernGraphicsWindowHook = NULL;
		}
	}
};

CModernGraphicsLifecycleBridge g_ModernGraphicsLifecycleBridge;
}
#endif // MU_ENABLE_DILIGENT
