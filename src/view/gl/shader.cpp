#include "view/gl/shader.h"

#include "view/gl/gl_loader.h"

#include <glm/gtc/type_ptr.hpp>

namespace {

GLuint CompileShader(GLenum type, const char* src, std::string* error)
{
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        if (error) *error = std::string("Shader compile error:\n") + log;
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

unsigned LinkProgram(const char* vs, const char* fs, std::string* error)
{
    GLuint v = CompileShader(GL_VERTEX_SHADER, vs, error);
    if (!v) return 0;
    GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs, error);
    if (!f) { glDeleteShader(v); return 0; }
    GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof(log), nullptr, log);
        if (error) *error = std::string("Shader link error:\n") + log;
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

void SetMat4(unsigned p, const char* name, const glm::mat4& m)
{
    glUniformMatrix4fv(glGetUniformLocation(p, name), 1, GL_FALSE, glm::value_ptr(m));
}
