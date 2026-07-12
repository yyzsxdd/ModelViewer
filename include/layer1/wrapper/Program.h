#pragma once

#include "ogl_defs.h"
#include "Shader.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <string>
#include <memory>
#include <vector>

NAMESPACE_OPENGL_CORE_BEGIN

class Program
{
public:
    Program();
    ~Program();

    explicit Program(const std::string& name);

    // 禁止拷贝
    Program(const Program&) = delete;
    Program& operator=(const Program&) = delete;

    void Destroy();

    //Use和Detach 配合使用,建议使用ScopeUse,program 使用完以后自动恢复状态
    void Use();

    void Detach() const;

    GLuint GetHandle() const { return m_handle; }

    bool IsValidGLProgram() const;

    //获取变量 位置
    int GetAttributeLocation(const char* attrVarName) const;
    //获取常量 位置
    int GetUniformLocation(const char* uniVarName) const;
    //获取SSBO 位置
    int GetSSBOLocation(const char* ssboName) const;
    //获取片段着色器中的out变量的位置，可用于多渲染目标(Multiple Render Targets)
    //如果没有显式指定 layout(location = N)，OpenGL 会自动分配位置，但可以用 glGetFragDataLocation 查询。
    int GetFragOutLocation(const char* name) const;
    //获取ubo的位置
    int GetUBOLocation(const char* name) const;

    bool SetUniform(int location, int value);
    bool SetUniform(const char* name, int value);

    bool SetUniform(int location, uint32_t value);
    bool SetUniform(const char* name, uint32_t value);

    //给uniform uint u_data[count]设置数据
    bool SetUniforms(int location, const uint32_t* value, uint32_t count);
    bool SetUniforms(const char* name, const uint32_t* values, uint32_t count);

    bool SetUniform(int location, float value);
    bool SetUniform(const char* name, float value);

    bool SetUniforms(int location, const float* values, uint32_t count);
    bool SetUniforms(const char* name, const float* values, uint32_t count);

    bool SetUniform(int location, double value);
    bool SetUniform(const char* name, double value);

    bool SetUniform(int location, const glm::vec2& value);
    bool SetUniform(const char* name, const glm::vec2& value);

    bool SetUniform(int location, const glm::vec3& value);
    bool SetUniform(const char* name, const glm::vec3& value);

    //给uniform vec3 u_data[count]设置数据
    bool SetUniforms(int location, const glm::vec3* values, uint32_t count);
    bool SetUniforms(const char* name, const glm::vec3* values, uint32_t count);

    bool SetUniform(int location, const glm::vec4& values);
    bool SetUniform(const char* name, const glm::vec4& values);

    //给uniform vec4 u_data[count]设置数据
    bool SetUniforms(int location, const glm::vec4* value, uint32_t count);
    bool SetUniforms(const char* name, const glm::vec4* values, uint32_t count);

    bool SetUniform(int location, const glm::mat4& value);
    bool SetUniform(const char* name, const glm::mat4& value);

    bool SetSSBO(int location, GLuint ssboHandle, uint32_t bindSlot);
    bool SetSSBO(const char* name, GLuint ssboHandle, uint32_t bindSlot);

    bool SetSSBOAutoSlot(int location, GLuint ssboHandle);
    bool SetSSBOAutoSlot(const char* name, uint32_t ssboHandle);

    bool SetUBO(int location, GLuint handle, uint32_t size, uint32_t bindSlot);
    bool SetUBO(const char* name, GLuint handle, uint32_t size, uint32_t bindSlot);

    struct UniformTexture {
        int location;
        GLuint textureID;
        GLenum target;
    };

    //设置纹理, todo: 参数类型需要收敛
    void SetUniformTextures(const std::initializer_list<UniformTexture>& textures);
    void SetUniformTextures(const std::vector<UniformTexture>& textures);

    bool Create(Shader& vsShader, Shader& fsShader, Shader& gsShader);
    bool Create(Shader& vsShader, Shader& fsShader);
    static std::shared_ptr<Program> CreateGLProgram(const char* vsScript, const char* fsScript, const char* gsScript = nullptr);
    static std::shared_ptr<Program> CreateGLProgram(const char* vsScript, const char* fsScript, const char* gsScript, const std::initializer_list<std::pair<std::string_view, std::string_view>>& commonReplaces);
    static std::shared_ptr<Program> CreateComputerProgram(const char* csScript);

    const std::string& GetName() const { return m_name; }

private:
    bool Build(std::initializer_list<Shader*> shaders);

    static std::string ReplaceCpp2GLSL(const char* script);

    std::string m_name;
    GLuint m_handle{ 0 };
    mutable GLuint m_ssboAutoBindSlotNext{ 0 };
};

struct ScopeUseProgram {
    explicit ScopeUseProgram(Program& p) {
        GL(glGetIntegerv, GL_CURRENT_PROGRAM, &m_oldProgram);
        p.Use();
    }
    ~ScopeUseProgram() {
        GL(glUseProgram, m_oldProgram);
    }
    void runOut() {
        m_isRunOut = true;
    }
    //重载bool运算，作为for的判断条件，让循环只进入一次
    explicit operator bool() const {
        return !m_isRunOut;
    }

    // 禁止拷贝
    ScopeUseProgram(const ScopeUseProgram&) = delete;
    ScopeUseProgram& operator=(const ScopeUseProgram&) = delete;
private:
    bool m_isRunOut = false;
    GLint m_oldProgram;
};

NAMESPACE_OPENGL_CORE_END
