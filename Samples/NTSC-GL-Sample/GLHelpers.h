#pragma once

// Header to include all of the GL extension stuff.
// Yeah I know, this is all a nightmare, but Windows is stuck in GL 1.1 land, that's what I'm building on, and I didn't want to pull in
//  any non-WinSDK external dependencies. If you have a real GL header you can safely ignore all of this.

#ifndef NOMINMAX
  #define NOMINMAX
#endif

#include <Windows.h>
#include <GL/gl.h>

#include <assert.h>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

#define WGL_CONTEXT_MAJOR_VERSION_ARB     0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB     0x2092
#define WGL_CONTEXT_LAYER_PLANE_ARB       0x2093
#define WGL_CONTEXT_FLAGS_ARB             0x2094
#define WGL_CONTEXT_PROFILE_MASK_ARB      0x9126


#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
#define GL_TEXTURE_BASE_LEVEL             0x813C
#define GL_TEXTURE_MAX_LEVEL              0x813D
#define GL_RG                             0x8227
#define GL_R32F                           0x822E
#define GL_RG32F                          0x8230
#define GL_TEXTURE0                       0x84C0
#define GL_TEXTURE1                       0x84C1
#define GL_TEXTURE2                       0x84C2
#define GL_TEXTURE3                       0x84C3
#define GL_TEXTURE4                       0x84C4
#define GL_TEXTURE5                       0x84C5
#define GL_TEXTURE6                       0x84C6
#define GL_TEXTURE7                       0x84C7
#define GL_TEXTURE8                       0x84C8
#define GL_TEXTURE9                       0x84C9
#define GL_TEXTURE10                      0x84CA
#define GL_TEXTURE11                      0x84CB
#define GL_TEXTURE12                      0x84CC
#define GL_TEXTURE13                      0x84CD
#define GL_TEXTURE14                      0x84CE
#define GL_TEXTURE15                      0x84CF
#define GL_TEXTURE16                      0x84D0
#define GL_TEXTURE17                      0x84D1
#define GL_TEXTURE18                      0x84D2
#define GL_TEXTURE19                      0x84D3
#define GL_TEXTURE20                      0x84D4
#define GL_TEXTURE21                      0x84D5
#define GL_TEXTURE22                      0x84D6
#define GL_TEXTURE23                      0x84D7
#define GL_TEXTURE24                      0x84D8
#define GL_TEXTURE25                      0x84D9
#define GL_TEXTURE26                      0x84DA
#define GL_TEXTURE27                      0x84DB
#define GL_TEXTURE28                      0x84DC
#define GL_TEXTURE29                      0x84DD
#define GL_TEXTURE30                      0x84DE
#define GL_TEXTURE31                      0x84DF
#define GL_RGBA32F                        0x8814
#define GL_ARRAY_BUFFER                   0x8892
#define GL_STATIC_DRAW                    0x88E4
#define GL_DYNAMIC_DRAW                   0x88E8
#define GL_UNIFORM_BUFFER                 0x8A11
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
#define GL_ACTIVE_UNIFORMS                0x8B86
#define GL_ACTIVE_ATTRIBUTES              0x8B89
#define GL_CLAMP_VERTEX_COLOR             0x891A
#define GL_CLAMP_FRAGMENT_COLOR           0x891B
#define GL_CLAMP_READ_COLOR               0x891C
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_FRAMEBUFFER                    0x8D40

using GLsizeiptr = std::make_signed_t<size_t>;
using GLchar = char;


void (*glGenBuffers) (GLsizei n, GLuint *arraysOut) = nullptr;
void (*glBindBuffer) (GLenum target, GLuint buffer) = nullptr;
void (*glBufferData) (GLenum target, GLsizeiptr size, const void *data, GLenum usage) = nullptr;
void (*glDeleteBuffers) (GLsizei n, const GLuint * buffers);
GLuint (*glCreateShader) (GLenum shaderType) = nullptr;
HGLRC (WINAPI *wglCreateContextAttribsARB) (HDC hDC, HGLRC hShareContext, const int *attribList) = nullptr;
void (*glShaderSource) (GLuint shader, GLsizei count, const GLchar **string, const GLint *length) = nullptr;
void (*glCompileShader) (GLuint shader) = nullptr;
void (*glGetShaderiv) (GLuint shader, GLenum pname, GLint *params) = nullptr;
void (*glGetShaderInfoLog) (GLuint shader, GLsizei maxLength, GLsizei *length, GLchar *infoLog) = nullptr;
GLuint (*glCreateProgram)() = nullptr;
void (*glAttachShader)(GLuint program, GLuint shader) = nullptr;
void (*glLinkProgram)(GLuint program) = nullptr;
void (*glGetProgramiv)(GLuint program, GLenum pname, GLint *params) = nullptr;
void (*glGetProgramInfoLog)(GLuint program, GLsizei maxLength, GLsizei *length,  	GLchar *infoLog) = nullptr;
void (*glDeleteShader)(GLuint shader) = nullptr;
void (*glVertexAttribPointer) (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) = nullptr;
void (*glVertexAttribIPointer) (GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) = nullptr;
void (*glVertexAttribLPointer) (GLuint index, GLint size, GLenum type, GLsizei stride, const void * pointer) = nullptr;
void (*glEnableVertexAttribArray) (GLuint index) = nullptr;
void (*glDisableVertexAttribArray) (GLuint index) = nullptr;
void (*glEnableVertexArrayAttrib) (GLuint vaobj, GLuint index) = nullptr;
void (*glDisableVertexArrayAttrib) (GLuint vaobj, GLuint index) = nullptr;
void (*glUseProgram) (GLuint program) = nullptr;
void (*glBindVertexArray) (GLuint array) = nullptr;
void (*glGenVertexArrays) (GLsizei n, GLuint *arrays) = nullptr;
void (*glDeleteVertexArrays) (GLsizei n, const GLuint *arrays);
void (*glUniformBlockBinding) (GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding);
GLuint (*glGetUniformBlockIndex) (GLuint program, const GLchar *uniformBlockName);
void (*glBindBufferBase) (GLenum target, GLuint index, GLuint buffer);
void (*glGenerateTextureMipmap) (GLuint texture);
void (*glGenFramebuffers) (GLsizei n, GLuint *ids);
void (*glBindFramebuffer) (GLenum target, GLuint framebuffer);
GLenum (*glCheckFramebufferStatus) (GLenum target);
void (*glDeleteFramebuffers) (GLsizei n, GLuint *framebuffers);
void (*glFramebufferTexture2D) (GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
void (*glActiveTexture) (GLenum texture);
void (*glUniform1i) (GLint location, GLint v0);
GLint (*glGetUniformLocation) (GLuint program, const GLchar *name);
void (*glGetActiveAttrib) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void (*glGetActiveUniform) (GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name);
void (*glClampColor) (GLenum target, GLenum clamp);
void (*glDeleteProgram) (GLuint program);


// Using an out parameter here so I don't have to specify the function output type as a template parameter.
template <typename FuncType>
void LoadGLFunction(const char *name, FuncType *funcOut)
{
  *funcOut = reinterpret_cast<FuncType>(wglGetProcAddress(name));
  if (*funcOut == nullptr)
  {
    char buffer[2048];
    sprintf_s(buffer, "Failed to get address of proc '%s'", name);
    throw std::exception(buffer);
  }
}


#define LOAD_GL_FUNCTION(name) LoadGLFunction(#name, &name);


inline void InitializeGLHelpers()
{
  static bool s_initialized = []
  {
    LOAD_GL_FUNCTION(glGenBuffers);
    LOAD_GL_FUNCTION(glBindBuffer);
    LOAD_GL_FUNCTION(glBufferData);
    LOAD_GL_FUNCTION(glDeleteBuffers);
    LOAD_GL_FUNCTION(wglCreateContextAttribsARB);
    LOAD_GL_FUNCTION(glCreateShader);
    LOAD_GL_FUNCTION(glShaderSource);
    LOAD_GL_FUNCTION(glCompileShader);
    LOAD_GL_FUNCTION(glGetShaderiv);
    LOAD_GL_FUNCTION(glGetShaderInfoLog);

    LOAD_GL_FUNCTION(glCreateProgram);
    LOAD_GL_FUNCTION(glAttachShader);
    LOAD_GL_FUNCTION(glLinkProgram);
    LOAD_GL_FUNCTION(glGetProgramiv);
    LOAD_GL_FUNCTION(glGetProgramInfoLog);
    LOAD_GL_FUNCTION(glDeleteShader);
    LOAD_GL_FUNCTION(glVertexAttribPointer);
    LOAD_GL_FUNCTION(glVertexAttribIPointer);
    LOAD_GL_FUNCTION(glVertexAttribLPointer);
    LOAD_GL_FUNCTION(glEnableVertexAttribArray);
    LOAD_GL_FUNCTION(glDisableVertexAttribArray);
    LOAD_GL_FUNCTION(glEnableVertexArrayAttrib);
    LOAD_GL_FUNCTION(glDisableVertexArrayAttrib);
    LOAD_GL_FUNCTION(glUseProgram);
    LOAD_GL_FUNCTION(glBindVertexArray);
    LOAD_GL_FUNCTION(glGenVertexArrays);
    LOAD_GL_FUNCTION(glDeleteVertexArrays);
    LOAD_GL_FUNCTION(glUniformBlockBinding);
    LOAD_GL_FUNCTION(glGetUniformBlockIndex);
    LOAD_GL_FUNCTION(glBindBufferBase);
    LOAD_GL_FUNCTION(glGenerateTextureMipmap);
    LOAD_GL_FUNCTION(glGenFramebuffers);
    LOAD_GL_FUNCTION(glBindFramebuffer);
    LOAD_GL_FUNCTION(glCheckFramebufferStatus);
    LOAD_GL_FUNCTION(glDeleteFramebuffers);
    LOAD_GL_FUNCTION(glFramebufferTexture2D);
    LOAD_GL_FUNCTION(glActiveTexture);
    LOAD_GL_FUNCTION(glUniform1i);
    LOAD_GL_FUNCTION(glGetUniformLocation);
    LOAD_GL_FUNCTION(glGetActiveAttrib);
    LOAD_GL_FUNCTION(glGetActiveUniform);
    LOAD_GL_FUNCTION(glClampColor);
    LOAD_GL_FUNCTION(glDeleteProgram);
    return true;
  }();
}


inline void CheckGLError()
{
  auto err = glGetError();
  switch (err)
  {
  case GL_NO_ERROR: return;
  case GL_INVALID_ENUM: assert(false); throw std::exception("Invalid enum");
  case GL_INVALID_VALUE: assert(false); throw std::exception("Invalid value");
  case GL_INVALID_OPERATION: assert(false); throw std::exception("Invalid operation");
  case GL_INVALID_FRAMEBUFFER_OPERATION: assert(false); throw std::exception("Invalid framebuffer operation");
  case GL_OUT_OF_MEMORY: assert(false); throw std::exception("Out of memory");
  case GL_STACK_UNDERFLOW: assert(false); throw std::exception("Stack underflow");
  case GL_STACK_OVERFLOW: assert(false); throw std::exception("Stack overflow");
  default:
    char buf[1024];
    sprintf_s(buf, "Unknown error %d", err);
    throw std::exception(buf);
  }
}


// A sadly-necessary-because-I-couldn't-find-a-better-way helper to convert a path (which might be a wide string) to a std::string.
template <typename PathValueType>
std::string PathCharsToStdString(const PathValueType *pathName)
{
  if constexpr (std::is_same_v<PathValueType, wchar_t>)
  {
    char buffer[2048];
    sprintf_s(buffer, "%S", pathName);
    return std::string {buffer};
  }
  else
  {
    return std::string {pathName};
  }
};


// Load the text from a shader file, appending a #version header (and the #define GLSL it needs) and handling any #includes that we find.
std::string GetShaderText(std::filesystem::path path, std::vector<std::filesystem::path> &knownPaths)
{
  size_t fileID;
  if (auto iter = std::ranges::find_if(knownPaths, [path](const auto &testPath) { return path == testPath; });
    iter != knownPaths.end())
  {
    fileID = size_t(std::distance(knownPaths.begin(), iter));
  }
  else
  {
    knownPaths.push_back(path);
    fileID = knownPaths.size() - 1;
  }

  // If you hate the iostream things, me too, but this code was built quick with all the standard-compliant stuff I could use, so, sorry.
  std::ifstream stream { path };
  if (!stream)
  {
    #ifdef _MSC_VER
      #pragma warning (push)
      #pragma warning (disable: 4996) // 'strerror': This function or variable may be unsafe.
    #endif
    char buffer[2048];
    sprintf_s(buffer, "Failed to open file '%s\n', error: %s", PathCharsToStdString(path.c_str()).c_str(), std::strerror(errno));
    throw std::exception(buffer);
    #ifdef _MSC_VER
      #pragma warning (pop)
    #endif
  }

  std::string contents;
  if (knownPaths.size() == 1)
  {
    // This is the root-level file, so set our shader version and define GLSL so our cross-platform stuff works.
    contents += "#version 330 core\n#define GLSL\n";
  }

  // Helper function to build a #line directive (with a comment in it containing the filename for good measure)
  auto BuildLineDirective = [](size_t line, size_t fileIndex, std::filesystem::path pathName)
  {
    char buffer[2048];
    sprintf_s(buffer, "// File: \"%s\"\n#line %zu %zu\n", PathCharsToStdString(pathName.c_str()).c_str(), line, fileIndex);
    return std::string {buffer};
  };

  // Start with a directive that says "THIS is line 1 of this file"
  contents += BuildLineDirective(1, fileID, path);

  size_t lineIndex = 1;
  while (!stream.eof())
  {
    std::string line;
    std::getline(stream, line);

    // Do a super hacky job of handling #includes
    std::string trimmed = line;
    trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](auto ch) { return !std::isspace(ch); }));

    if (!trimmed.empty() && trimmed.starts_with("#include"))
    {
      // We found an #include, remove everything before the first quote, then everything after the last quote. I'm sure there's a more
      //  clever way to do this but again, written fast.
      trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), [](auto ch) { return ch == '\"'; }));
      trimmed.erase(0, 1);
      trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), [](auto ch) { return ch == '\"'; }).base(), trimmed.end());
      trimmed.erase(trimmed.size() - 1, 1);

      // Assemble our new path and get its contents.
      auto includePath = std::filesystem::absolute(path.parent_path() / trimmed);
      auto includeContents = GetShaderText(includePath, knownPaths);

      // Append the content of that file into our own, then send us back to the correct line of this file (The line index *after* the
      //  line we are currently handling)
      contents += includeContents;
      contents += "\n\n";
      contents += BuildLineDirective(lineIndex + 1, fileID, path);
    }
    else
    {
      // Not an #include directive so just append this line directly.
      contents += line;
      contents += "\n";
    }

    lineIndex++;
  }

  return contents;
}



GLuint CompileShaderFromFile(GLenum shaderType, const char *pathStr)
{
  // Get the text of the shader (and an ordered list of all of the paths involved)
  std::vector<std::filesystem::path> knownPaths;
  auto content = GetShaderText(std::filesystem::absolute(pathStr), knownPaths);

  // Create and compile!
  GLuint shaderHandle = glCreateShader(shaderType);
  const GLchar *contentPtr = content.c_str();
  glShaderSource(shaderHandle, 1, &contentPtr, nullptr);
  glCompileShader(shaderHandle);

  int success;
  glGetShaderiv(shaderHandle, GL_COMPILE_STATUS, &success);
  if (!success)
  {
    // We failed so we want to display the errors. In order to do this we want to replace the file indices with actual filename
    //  information.

    int logLength;
    glGetShaderiv(shaderHandle, GL_INFO_LOG_LENGTH, &logLength);
    std::vector<char> log;
    log.resize(logLength);
    glGetShaderInfoLog(shaderHandle, GLsizei(log.size()), nullptr, &log[0]);

    // Now we get to iterate through the log and manually swap in the filenames in place of the numbers (because god forbid the GLSL
    //  #line directive have taken a string for the file ID instead of a number)
    std::stringstream logFullUnparsedStream {&log[0]};
    std::string parsed;
    while (!logFullUnparsedStream.eof())
    {
      std::string line;
      std::getline(logFullUnparsedStream, line);

      if (line.empty())
      {
        parsed += line;
        continue;
      }

      char *endPtr;
      auto filenameIndex = std::strtol(line.c_str(), &endPtr, 10);
      if (*endPtr != '(' || filenameIndex >= knownPaths.size())
      {
        parsed = &log[0];
        break;
      }

      parsed += PathCharsToStdString(knownPaths[filenameIndex].filename().c_str());
      parsed += endPtr;
      parsed += "\n";
    }

    using namespace std;

    #ifdef _WIN32
      OutputDebugStringA(parsed.c_str());
      OutputDebugStringA("\n");
    #endif

    throw std::exception(("Shader compilation failed:\n\n"s + parsed).c_str());
  }

  CheckGLError();
  return shaderHandle;
}


inline GLuint LinkShaderProgram(GLuint vertexShader, GLuint fragmentShader, const char *optionalName = nullptr)
{
  GLuint shaderProgram = glCreateProgram();
  // Attach the vertex and fragment shaders, then link!
  glAttachShader(shaderProgram, vertexShader);
  glAttachShader(shaderProgram, fragmentShader);
  glLinkProgram(shaderProgram);

  int success;
  glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
  if (!success)
  {
    CheckGLError();

    char log[1024];
    glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);

    char buffer[1536];
    sprintf_s(
      buffer,
      "Failed to link shader program '%s'. Log:\n\n%s",
      optionalName != nullptr ? optionalName : "<unnamed>",
      log);
    throw std::exception(buffer);
  }

  CheckGLError();
  return shaderProgram;
}


#undef LOAD_GL_FUNCTION