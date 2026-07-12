
#include "Program.h"
#include "ogl_defs.h"

NAMESPACE_OPENGL_CORE_BEGIN

Program::Program() = default;

Program::Program(const std::string& name) : m_name(name) { }

Program::~Program()
{
    Destroy();
}

bool Program::Build(std::initializer_list<Shader*> shaders) {
    Destroy();

    std::string errorString;

    // 这里 glCreateProgram 后的逗号不是多余的，目的是为里防止 clang 编译期报警告
    auto handle = GL(glCreateProgram, );
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(handle != 0, false, "Shader program create error!");
    for (auto itor = shaders.begin(); itor != shaders.end(); ++itor) {
        const auto& shader = *itor;
        GL(glAttachShader, handle, shader->GetHandle());
    }

    GL(glLinkProgram, handle);
    GLint linkStatus;
    GL(glGetProgramiv, handle, GL_LINK_STATUS, &linkStatus);
    if (!linkStatus) {
        GLint infoLen{ 0 };
        GL(glGetProgramiv, handle, GL_INFO_LOG_LENGTH, &infoLen);  assert(infoLen);
        errorString.resize(infoLen);
        GL(glGetProgramInfoLog, handle, infoLen, nullptr, (GLchar*)errorString.c_str());
        GL(glDeleteProgram, handle);
        handle = 0;
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "GL program Link error: %s", errorString.c_str());
    }
    m_handle = handle;

    return true;
}

void Program::Destroy() {
    if (m_handle && IsValidGLProgram()) {

        // 检查是否是当前激活的程序
        GLint currentProgram;
        glGetIntegerv(GL_CURRENT_PROGRAM, &currentProgram);

        //取消程序的激活状态。这样可以避免后续 OpenGL 调用尝试使用已删除的程序对象。
        if (currentProgram > 0 && (GLuint)currentProgram == m_handle) {
            GL(glUseProgram, 0);
        }

        GL(glDeleteProgram, m_handle);
        m_handle = 0;

    }
}

bool Program::IsValidGLProgram() const {
    if (m_handle == 0)  return false;

    // 检查OpenGL上下文是否有效
    // glGetError可能会在没有有效上下文时产生错误
    GLenum errorBefore = glGetError();

    // 检查是否为程序对象
    GLboolean isProgram = glIsProgram(m_handle);

    GLenum errorAfter = glGetError();

    // 如果调用glIsProgram产生了错误，说明上下文可能无效
    if (errorAfter != errorBefore && errorBefore == GL_NO_ERROR) {
        return false;
    }

    return isProgram == GL_TRUE;

}

void Program::Use() {
    GL(glUseProgram, m_handle);
    m_ssboAutoBindSlotNext = 0;
}

void Program::Detach() const {
    GL(glUseProgram, 0);
}

int  Program::GetAttributeLocation(const char* attrVarName) const {
    if (m_handle) {
        return GL(glGetAttribLocation, m_handle, attrVarName);
    }
    return -1;
}

int  Program::GetUniformLocation(const char* uniVarName) const {
    if (m_handle) {
        return GL(glGetUniformLocation, m_handle, uniVarName);
    }
    return -1;
}

int  Program::GetSSBOLocation(const char* ssboName) const {
    if (m_handle) {
        GLuint value = GL(glGetProgramResourceIndex, m_handle, GL_SHADER_STORAGE_BLOCK, ssboName);
        return (int)value;
    }
    return -1;
}

int  Program::GetFragOutLocation(const char* name) const {
    if (m_handle) {
        return GL(glGetFragDataLocation, m_handle, name);
    }
    return -1;
}

int  Program::GetUBOLocation(const char* name) const {
    if (m_handle) {
        return GL(glGetUniformBlockIndex, m_handle, name);
    }
    return -1;
}


bool Program::SetUniform(int location, int value) {
    if (m_handle && location != -1) {
        GL(glUniform1i, location, value);
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, int value) {

    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniform(int location, uint32_t value) {
    if (m_handle && location != -1) {
        GL(glUniform1ui, location, value);
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, uint32_t value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniforms(int location, const uint32_t* value, uint32_t count) {
    if (m_handle && location != -1 && count != 0) {
        GL(glUniform1uiv, location, count, value);
        return true;
    }
    return false;
}

bool Program::SetUniforms(const char* name, const uint32_t* values, uint32_t count) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniforms(location, values, count);
    }
    return false;
}

bool Program::SetUniform(int location, float value) {
    if (m_handle && location != -1) {
        GL(glUniform1f, location, value);
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, float value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniforms(int location, const float* values, uint32_t count) {
    if (m_handle && location != -1 && count != 0) {
        GL(glUniform1fv, location, count, values);
        return true;
    }
    return false;
}
bool Program::SetUniforms(const char* name, const float* values, uint32_t count) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniforms(location, values, count);
    }
    return false;
}

bool Program::SetUniform(int location, double value) {
    if (m_handle && location != -1) {
        GL(glUniform1d, location, value);
        return true;
    }
    return false;
}
bool Program::SetUniform(const char* name, double value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniform(int location, const glm::vec2& value) {
    if (m_handle && location != -1) {
        GL(glUniform2f, location, value.x, value.y);
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, const glm::vec2& value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniform(int location, const glm::vec3& value) {
    if (m_handle && location != -1) {
        GL(glUniform3f, location, value.x, value.y, value.z);
        return true;
    }
    return false;
}
bool Program::SetUniform(const char* name, const glm::vec3& value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniforms(int location, const glm::vec3* values, uint32_t count) {
    if (m_handle && location != -1 && count != 0) {
        GL(glUniform3fv, location, count, glm::value_ptr(values[0]));
        return true;
    }
    return false;
}

bool Program::SetUniforms(const char* name, const glm::vec3* values, uint32_t count) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniforms(location, values, count);
    }
    return false;
}

bool Program::SetUniform(int location, const glm::vec4& value) {
    if (m_handle && location != -1) {
        GL(glUniform4f, location, value.x, value.y, value.z, value.w);
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, const glm::vec4& value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetUniforms(int location, const glm::vec4* values, uint32_t count) {
    if (m_handle && location != -1 && count != 0) {
        GL(glUniform4fv, location, count, glm::value_ptr(values[0]));
        return true;
    }
    return false;
}

bool Program::SetUniforms(const char* name, const glm::vec4* values, uint32_t count) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniforms(location, values, count);
    }
    return false;
}

bool Program::SetUniform(int location, const glm::mat4& value) {
    if (m_handle && location != -1) {
        GL(glUniformMatrix4fv, location, 1, GL_FALSE, glm::value_ptr(value));
        return true;
    }
    return false;
}

bool Program::SetUniform(const char* name, const glm::mat4& value) {
    int location = GetUniformLocation(name);
    if (location != -1) {
        return SetUniform(location, value);
    }
    return false;
}

bool Program::SetSSBO(int location, GLuint ssboHandle, uint32_t bindSlot) {
    if (m_handle && ssboHandle != -1) {
        GL(glShaderStorageBlockBinding, m_handle, location, bindSlot);
        GL(glBindBufferBase, GL_SHADER_STORAGE_BUFFER, bindSlot, ssboHandle);
        return true;
    }
    return false;
}

bool Program::SetSSBO(const char* name, GLuint ssboHandle, uint32_t bindSlot) {
    int location = GetSSBOLocation(name);
    if (location != -1) {
        return SetSSBO(location, ssboHandle, bindSlot);
    }
    return false;
}

bool Program::SetSSBOAutoSlot(int location, GLuint ssboHandle) {
    if (location == -1 || m_handle == 0) { return false; }
    GL(glShaderStorageBlockBinding, m_handle, location, m_ssboAutoBindSlotNext);
    GL(glBindBufferBase, GL_SHADER_STORAGE_BUFFER, m_ssboAutoBindSlotNext, ssboHandle);
    ++m_ssboAutoBindSlotNext;
    return true;
}

bool Program::SetSSBOAutoSlot(const char* name, uint32_t ssboHandle) {
    int location = GetSSBOLocation(name);
    if (location != -1) {
        return SetSSBOAutoSlot(location, ssboHandle);
    }
    return false;
}

bool Program::SetUBO(int location, GLuint handle, uint32_t size, uint32_t bindSlot) {
    if (location == -1 || m_handle == 0) { return false; }
    GL(glUniformBlockBinding, m_handle, location, bindSlot);
    GL(glBindBufferRange, GL_UNIFORM_BUFFER, bindSlot, handle, 0, size);
    return true;
}

bool Program::SetUBO(const char* name, GLuint handle, uint32_t size, uint32_t bindSlot) {
    int location = GetUBOLocation(name);
    if (location != -1) {
        return SetUBO(location, handle, size, bindSlot);
    }
    return false;
}

void Program::SetUniformTextures(const std::initializer_list<UniformTexture>& textures) {
    FOR_VEC(textures, itor) {
        const auto idx = itor - textures.begin();
        const auto& tex = *itor;
        GL(glActiveTexture, GLenum(GL_TEXTURE0 + idx));
        GL(glBindTexture, tex.target, tex.textureID);
        GL(glUniform1i, tex.location, (GLint)idx);
    }
}

void Program::SetUniformTextures(const std::vector<UniformTexture>& textures) {
    FOR_VEC(textures, itor) {
        const auto idx = itor - textures.begin();
        const auto& tex = *itor;
        GL(glActiveTexture, GLenum(GL_TEXTURE0 + idx));
        GL(glBindTexture, tex.target, tex.textureID);
        GL(glUniform1i, tex.location, (GLint)idx);
    }
}

std::shared_ptr<Program> Program::CreateGLProgram(const char* vsScript, const char* fsScript, const char* gsScript) {
    Shader vs(GL_VERTEX_SHADER, ReplaceCpp2GLSL(vsScript).c_str());
    auto bResult = vs.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_VERTEX_SHADER compilation error");

    Shader fs(GL_FRAGMENT_SHADER, ReplaceCpp2GLSL(fsScript).c_str());
    bResult = fs.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_FRAGMENT_SHADER compilation error");

    auto result = std::make_shared<Program>();
    if (gsScript == nullptr) {
        bResult = result->Build({ &vs, &fs });
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_Program link error");
    }
    else {
        Shader gs(GL_GEOMETRY_SHADER, ReplaceCpp2GLSL(gsScript).c_str());
        bResult = gs.Build();
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_GEOMETRY_SHADER compilation error");

        bResult = result->Build({ &vs, &fs, &gs });
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_Program link error");
    }
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(result->GetHandle() != 0, {}, "GL_Program creation error");
    return result;
}

bool Program::Create(Shader& vsShader, Shader& fsShader, Shader& gsShader) {
    auto bResult = vsShader.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_VERTEX_SHADER compilation error");
    bResult = fsShader.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_FRAGMENT_SHADER compilation error");
    bResult = gsShader.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_GEOMETRY_SHADER compilation error");
    bResult = Build({ &vsShader, &fsShader, &gsShader });
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_Program link error");
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(GetHandle() != 0, {}, "GL_Program creation error");
    return bResult;
}

bool Program::Create(Shader& vsShader, Shader& fsShader) {
    auto bResult = vsShader.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_VERTEX_SHADER compilation error");
    bResult = fsShader.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_FRAGMENT_SHADER compilation error");
    auto result = std::make_shared<Program>();
    bResult = Build({ &vsShader, &fsShader });
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_Program link error");
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(GetHandle() != 0, {}, "GL_Program creation error");
    return bResult;
}

std::shared_ptr<Program> Program::CreateGLProgram(const char* vsScript, const char* fsScript, const char* gsScript, const std::initializer_list<std::pair<std::string_view, std::string_view>>& commonReplaces)
{
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(vsScript != nullptr && fsScript != nullptr, {}, "opengl program creation failure!");

        std::array<std::string, 3> scripts = { vsScript, fsScript, gsScript == nullptr ? "" : gsScript };
    for (auto& script : scripts) {
        script = StringUtils::Replace(script, commonReplaces);
    }
    return CreateGLProgram(scripts[0].c_str(), scripts[1].c_str(), scripts[2].empty() ? nullptr : scripts[2].c_str());
}

std::shared_ptr<Program> Program::CreateComputerProgram(const char* csScript) {
    Shader cs(GL_COMPUTE_SHADER, ReplaceCpp2GLSL(csScript).c_str());
    auto bResult = cs.Build();
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "GL_COMPUTER_SHADER compilation error");
        auto result = std::make_shared<Program>();
    bResult = result->Build({ &cs });
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(bResult, {}, "COMPUTER_Program link error");
    return result;
}


std::string Program::ReplaceCpp2GLSL(const char* script) {
    return StringUtils::Replace(script, {
        { "uint32_t", "uint" },
        { "glm::", "" }
        });
}


NAMESPACE_OPENGL_CORE_END
