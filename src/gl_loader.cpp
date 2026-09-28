#include "gl_loader.h"

#include <GLFW/glfw3.h>

#define FS_GL_DEFINE(ret, name, ...) PFN_fs_gl##name fs_gl##name = nullptr;
FS_GL_FUNCTIONS(FS_GL_DEFINE)
#undef FS_GL_DEFINE

bool LoadGLFunctions(const char** missing)
{
#define FS_GL_LOAD(ret, name, ...)                                                   \
    fs_gl##name = reinterpret_cast<PFN_fs_gl##name>(glfwGetProcAddress("gl" #name)); \
    if (!fs_gl##name) {                                                              \
        if (missing) *missing = "gl" #name;                                          \
        return false;                                                                \
    }
    FS_GL_FUNCTIONS(FS_GL_LOAD)
#undef FS_GL_LOAD
    return true;
}
