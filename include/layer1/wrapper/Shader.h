#pragma once

#include "ogl_defs.h"
#include <string>
#include <set>
#include <optional>


NAMESPACE_OPENGL_CORE_BEGIN

class shader;

/// <summary>
/// 版本声明，宏接口定义
/// </summary>
class ShaderDefines
{
public:
    enum class ShaderVersion
    {
        GLSL_4_3_Compatibility = 1,
        GLSL_4_3_Core = 2
    };

    ShaderDefines() = default;
    ~ShaderDefines() = default;

    bool AddDefine(const std::string& define) { return m_defines.insert(define).second; }
    const std::set<std::string>& GetDefines() const { return m_defines; }

    void SetVersion(ShaderVersion version) { m_version = version; }
    ShaderVersion GetVersion() const { return m_version; };

private:
    friend class Shader;
    bool ParseDefines(const std::set<std::string>& defines, std::string& dest);
    bool ParseVersion(ShaderVersion version, std::string& dest);

    std::set<std::string> m_defines;
    ShaderVersion m_version =ShaderVersion::GLSL_4_3_Compatibility;
};



/// <summary>
/// shader 支持版本声明，宏定义等接口，如需定义接口，手动拼接
/// </summary>
class Shader {
public:
    explicit Shader(GLenum shaderType, const char* source = nullptr);
    ~Shader();

    // 禁止拷贝
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    GLenum GetType() const { return m_shaderType; }

    void SetSource(const char* source) { m_source = source; };
    const char* GetSource() const { return m_source.c_str(); }

    void SetDefs(const ShaderDefines variant) { m_variant = variant; }

    const char* GetBuildOutput() const { return m_buildOutput.c_str(); }

    GLuint GetHandle() const { return m_handle; }

    bool Build();

    void Clear();

    /// <summary>
    /// 替换源码中的字符串 
    /// </summary>
    /// <param name="source">源码</param>
    /// <param name="search">源码中需要查找的字符串</param>
    /// <param name="replace">用以替换的字符串</param>
    /// <param name="all">是否全部替换，false表示只替换第一个</param>
    /// <returns></returns>
    static bool Substitute(std::string& source, const std::string_view& search, const std::string_view& replace, bool all = true);

private:
    //shader类型。vertex/geometry/fragment/computer
    GLenum m_shaderType;
    //字符串源码
    std::string m_source;
    //版本声明，宏接口定义
    std::optional<ShaderDefines> m_variant;
    //编译时的输出信息
    std::string m_buildOutput;
    //shader句柄
    GLuint m_handle{0};
};


NAMESPACE_OPENGL_CORE_END
