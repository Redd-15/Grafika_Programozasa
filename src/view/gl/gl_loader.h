// View: minimal OpenGL 3.3 core function loader (only what this app uses).
// Functions are resolved through glfwGetProcAddress after a context exists.
#pragma once
#include <cstddef>

#ifndef APIENTRY
#  ifdef _WIN32
#    define APIENTRY __stdcall
#  else
#    define APIENTRY
#  endif
#endif

typedef unsigned int  GLenum;
typedef unsigned int  GLuint;
typedef int           GLint;
typedef int           GLsizei;
typedef float         GLfloat;
typedef unsigned char GLboolean;
typedef unsigned int  GLbitfield;
typedef char          GLchar;
typedef unsigned char GLubyte;
typedef std::ptrdiff_t GLsizeiptr;

#define GL_FALSE                0
#define GL_TRUE                 1
#define GL_DEPTH_BUFFER_BIT     0x00000100
#define GL_COLOR_BUFFER_BIT     0x00004000
#define GL_TRIANGLES            0x0004
#define GL_LESS                 0x0201
#define GL_LEQUAL               0x0203
#define GL_SRC_ALPHA            0x0302
#define GL_ONE_MINUS_SRC_ALPHA  0x0303
#define GL_CULL_FACE            0x0B44
#define GL_DEPTH_TEST           0x0B71
#define GL_BLEND                0x0BE2
#define GL_SCISSOR_TEST         0x0C11
#define GL_TEXTURE_2D           0x0DE1
#define GL_UNSIGNED_BYTE        0x1401
#define GL_UNSIGNED_INT         0x1405
#define GL_FLOAT                0x1406
#define GL_RGBA                 0x1908
#define GL_RENDERER             0x1F01
#define GL_VERSION              0x1F02
#define GL_LINEAR               0x2601
#define GL_LINEAR_MIPMAP_LINEAR 0x2703
#define GL_TEXTURE_MAG_FILTER   0x2800
#define GL_TEXTURE_MIN_FILTER   0x2801
#define GL_TEXTURE_WRAP_S       0x2802
#define GL_TEXTURE_WRAP_T       0x2803
#define GL_REPEAT               0x2901
#define GL_RGBA8                0x8058
#define GL_MULTISAMPLE          0x809D
#define GL_TEXTURE0             0x84C0
#define GL_ARRAY_BUFFER         0x8892
#define GL_ELEMENT_ARRAY_BUFFER 0x8893
#define GL_STATIC_DRAW          0x88E4
#define GL_FRAGMENT_SHADER      0x8B30
#define GL_VERTEX_SHADER        0x8B31
#define GL_COMPILE_STATUS       0x8B81
#define GL_LINK_STATUS          0x8B82
#define GL_SRGB8_ALPHA8         0x8C43

#define FS_GL_FUNCTIONS(X)                                                                  \
    X(void, Clear, GLbitfield mask)                                                          \
    X(void, ClearColor, GLfloat r, GLfloat g, GLfloat b, GLfloat a)                          \
    X(void, Viewport, GLint x, GLint y, GLsizei w, GLsizei h)                                \
    X(void, Enable, GLenum cap)                                                              \
    X(void, Disable, GLenum cap)                                                             \
    X(void, BlendFunc, GLenum s, GLenum d)                                                   \
    X(void, DepthMask, GLboolean flag)                                                       \
    X(void, DepthFunc, GLenum func)                                                          \
    X(const GLubyte*, GetString, GLenum name)                                                \
    X(void, GenVertexArrays, GLsizei n, GLuint* arrays)                                      \
    X(void, DeleteVertexArrays, GLsizei n, const GLuint* arrays)                             \
    X(void, BindVertexArray, GLuint array)                                                   \
    X(void, GenBuffers, GLsizei n, GLuint* buffers)                                          \
    X(void, DeleteBuffers, GLsizei n, const GLuint* buffers)                                 \
    X(void, BindBuffer, GLenum target, GLuint buffer)                                        \
    X(void, BufferData, GLenum target, GLsizeiptr size, const void* data, GLenum usage)      \
    X(void, EnableVertexAttribArray, GLuint index)                                           \
    X(void, VertexAttribPointer, GLuint i, GLint n, GLenum t, GLboolean norm, GLsizei stride, const void* p) \
    X(GLuint, CreateShader, GLenum type)                                                     \
    X(void, ShaderSource, GLuint s, GLsizei n, const GLchar* const* str, const GLint* len)   \
    X(void, CompileShader, GLuint s)                                                         \
    X(void, GetShaderiv, GLuint s, GLenum p, GLint* v)                                       \
    X(void, GetShaderInfoLog, GLuint s, GLsizei max, GLsizei* len, GLchar* log)              \
    X(void, DeleteShader, GLuint s)                                                          \
    X(GLuint, CreateProgram, void)                                                           \
    X(void, AttachShader, GLuint p, GLuint s)                                                \
    X(void, LinkProgram, GLuint p)                                                           \
    X(void, GetProgramiv, GLuint p, GLenum n, GLint* v)                                      \
    X(void, GetProgramInfoLog, GLuint p, GLsizei max, GLsizei* len, GLchar* log)             \
    X(void, DeleteProgram, GLuint p)                                                         \
    X(void, UseProgram, GLuint p)                                                            \
    X(GLint, GetUniformLocation, GLuint p, const GLchar* name)                               \
    X(void, UniformMatrix4fv, GLint loc, GLsizei n, GLboolean transpose, const GLfloat* v)   \
    X(void, UniformMatrix3fv, GLint loc, GLsizei n, GLboolean transpose, const GLfloat* v)   \
    X(void, Uniform4fv, GLint loc, GLsizei n, const GLfloat* v)                              \
    X(void, Uniform3fv, GLint loc, GLsizei n, const GLfloat* v)                              \
    X(void, Uniform2f, GLint loc, GLfloat x, GLfloat y)                                      \
    X(void, Uniform1f, GLint loc, GLfloat x)                                                 \
    X(void, Uniform1i, GLint loc, GLint x)                                                   \
    X(void, Uniform1iv, GLint loc, GLsizei n, const GLint* v)                                \
    X(void, DrawElements, GLenum mode, GLsizei count, GLenum type, const void* indices)      \
    X(void, DrawArrays, GLenum mode, GLint first, GLsizei count)                             \
    X(void, GenTextures, GLsizei n, GLuint* textures)                                        \
    X(void, DeleteTextures, GLsizei n, const GLuint* textures)                               \
    X(void, ActiveTexture, GLenum unit)                                                      \
    X(void, BindTexture, GLenum target, GLuint texture)                                      \
    X(void, TexParameteri, GLenum target, GLenum name, GLint value)                          \
    X(void, TexImage2D, GLenum target, GLint level, GLint internalFormat, GLsizei w, GLsizei h, GLint border, GLenum format, GLenum type, const void* pixels) \
    X(void, GenerateMipmap, GLenum target)

#define FS_GL_DECLARE(ret, name, ...)                    \
    typedef ret(APIENTRY* PFN_fs_gl##name)(__VA_ARGS__); \
    extern PFN_fs_gl##name fs_gl##name;
FS_GL_FUNCTIONS(FS_GL_DECLARE)
#undef FS_GL_DECLARE

// Map the usual names onto our pointers (avoids clashing with opengl32 exports).
#define glClear                   fs_glClear
#define glClearColor              fs_glClearColor
#define glViewport                fs_glViewport
#define glEnable                  fs_glEnable
#define glDisable                 fs_glDisable
#define glBlendFunc               fs_glBlendFunc
#define glDepthMask               fs_glDepthMask
#define glDepthFunc               fs_glDepthFunc
#define glGetString               fs_glGetString
#define glGenVertexArrays         fs_glGenVertexArrays
#define glDeleteVertexArrays      fs_glDeleteVertexArrays
#define glBindVertexArray         fs_glBindVertexArray
#define glGenBuffers              fs_glGenBuffers
#define glDeleteBuffers           fs_glDeleteBuffers
#define glBindBuffer              fs_glBindBuffer
#define glBufferData              fs_glBufferData
#define glEnableVertexAttribArray fs_glEnableVertexAttribArray
#define glVertexAttribPointer     fs_glVertexAttribPointer
#define glCreateShader            fs_glCreateShader
#define glShaderSource            fs_glShaderSource
#define glCompileShader           fs_glCompileShader
#define glGetShaderiv             fs_glGetShaderiv
#define glGetShaderInfoLog        fs_glGetShaderInfoLog
#define glDeleteShader            fs_glDeleteShader
#define glCreateProgram           fs_glCreateProgram
#define glAttachShader            fs_glAttachShader
#define glLinkProgram             fs_glLinkProgram
#define glGetProgramiv            fs_glGetProgramiv
#define glGetProgramInfoLog       fs_glGetProgramInfoLog
#define glDeleteProgram           fs_glDeleteProgram
#define glUseProgram              fs_glUseProgram
#define glGetUniformLocation      fs_glGetUniformLocation
#define glUniformMatrix4fv        fs_glUniformMatrix4fv
#define glUniformMatrix3fv        fs_glUniformMatrix3fv
#define glUniform4fv              fs_glUniform4fv
#define glUniform3fv              fs_glUniform3fv
#define glUniform2f               fs_glUniform2f
#define glUniform1f               fs_glUniform1f
#define glUniform1i               fs_glUniform1i
#define glUniform1iv              fs_glUniform1iv
#define glDrawElements            fs_glDrawElements
#define glDrawArrays              fs_glDrawArrays
#define glGenTextures             fs_glGenTextures
#define glDeleteTextures          fs_glDeleteTextures
#define glActiveTexture           fs_glActiveTexture
#define glBindTexture             fs_glBindTexture
#define glTexParameteri           fs_glTexParameteri
#define glTexImage2D              fs_glTexImage2D
#define glGenerateMipmap          fs_glGenerateMipmap

// Returns false (and names the missing function) if anything failed to load.
bool LoadGLFunctions(const char** missing = nullptr);
