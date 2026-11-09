// View: compiling GLSL programs and setting uniforms.
#pragma once
#include <string>

#include <glm/glm.hpp>

// Compiles and links a vertex + fragment shader. Returns 0 and fills `error` on failure.
unsigned LinkProgram(const char* vertexSource, const char* fragmentSource, std::string* error);
void SetMat4(unsigned program, const char* name, const glm::mat4& m);
