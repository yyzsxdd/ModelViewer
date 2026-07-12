
#include "Shader.h"
#include "ogl_defs.h"
#include <iostream>

NAMESPACE_OPENGL_CORE_BEGIN

bool ShaderDefines::ParseDefines(const std::set<std::string>& defines, std::string& dest)
{
    dest.clear();
    for (const auto& def : defines) {
        if (def.size() > 0) {
            dest += (std::string("#define ") + def + "\n");
        }
    }

    return !dest.empty();
}

bool ShaderDefines::ParseVersion(ShaderVersion version, std::string& dest)
{
    if (version == ShaderVersion::GLSL_4_3_Compatibility) {
        dest = "#version 430 compatibility \n";
        return true;
    }
    else if (version == ShaderVersion::GLSL_4_3_Core) {
        dest = "#version 430 core \n";
        return true;
    }

    VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "Not supported shader version!");
}

Shader::Shader(GLenum shaderType, const char* source) : m_shaderType(shaderType), m_source(source)
{}

Shader::~Shader() {
    Clear();
}

void Shader::Clear() {
    m_buildOutput.clear();
    if (m_handle) {
        GL(glDeleteShader, m_handle);
        m_handle = 0;
    }
}

bool Shader::Build() {
    Clear();
    if (m_source.empty()) { return false; }

    auto shader = GL(glCreateShader, m_shaderType);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(shader, false, "Shader Create Shader failure! Error: \n %s", m_buildOutput.c_str());

    if (m_variant.has_value()) {
        std::string definesSnippet;
        m_variant->ParseDefines(m_variant->GetDefines(), definesSnippet);
        std::string versionSnippet;
        m_variant->ParseVersion(m_variant->GetVersion(), versionSnippet);
        m_source = versionSnippet + definesSnippet + m_source;
    }
    const char* sourceCode = m_source.c_str();

    GL(glShaderSource, shader, 1, &sourceCode, nullptr);
    GL(glCompileShader, shader);
    GLint status;
    GL(glGetShaderiv, shader, GL_COMPILE_STATUS, &status);
    // compile failed
    if (!status) {
        GLint infoLen;
        GL(glGetShaderiv, shader, GL_INFO_LOG_LENGTH, &infoLen);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(infoLen, false, "Shader compiling failure! Error: \n %s", m_buildOutput.c_str());
        m_buildOutput.resize(infoLen);
        GL(glGetShaderInfoLog, shader, infoLen, nullptr, (GLchar*)m_buildOutput.c_str());
        GL(glDeleteShader, shader);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "Shader compiling failure! Error: \n %s", m_buildOutput.c_str());
    }
    m_handle = shader;
    return true;
}

bool Shader::Substitute(std::string& source, const std::string_view& search, const std::string_view& replace, bool all)
{
    std::string::size_type pos = 0;
    bool replaced = false;
    while ((pos = source.find(search, pos)) != std::string::npos) {
        source.replace(pos, search.length(), replace);
        if (!all) {
            return true;
        }
        pos += replace.length();
        replaced = true;
    }

    return replaced;
}

NAMESPACE_OPENGL_CORE_END
