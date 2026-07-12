
#include "RBO.h"
#include "ogl_defs.h"

NAMESPACE_OPENGL_CORE_BEGIN

RBO::RBO() = default;

RBO::~RBO() {
    Destroy();
}

bool RBO::Create(RBOType type, RBOFormat format, GLsizei width, GLsizei height, const RBOMultisampleConfig& multisample) {
    // 如果已经存在，先释放
    if (m_handle != 0) {
        Destroy();
    }

    m_type = type;
    m_format = format;
    m_width = width;
    m_height = height;
    m_multisample = multisample;

    // 生成RBO
    GL(glGenRenderbuffers, 1, &m_handle);

    VI_ASSERT_LOG_ERROR_RETURN_VALUE(m_handle != 0, false, "Failed to generate renderbuffer");

    Bind();
    m_isValid = AllocateStorage();
    Unbind();
    return m_isValid;
}

void RBO::Destroy() {
    if (m_handle != 0) {
        GL(glDeleteRenderbuffers, 1, &m_handle);
        m_handle = 0;
    }
    m_width = 0;
    m_height = 0;
    m_isValid = false;
}

void RBO::Bind() const {
    if (m_handle != 0) {
        GL(glBindRenderbuffer, GL_RENDERBUFFER, m_handle);
    }
}

void RBO::Unbind() const {
    GL(glBindRenderbuffer, GL_RENDERBUFFER, 0);
}

bool RBO::Resize(GLsizei width, GLsizei height) {
    if (width == m_width && height == m_height) {
        return true;
    }

    m_width = width;
    m_height = height;

    if (m_handle != 0 && m_isValid) {
        Bind();
        bool success = AllocateStorage();
        Unbind();
        return success;
    }

    return false;
}

bool RBO::EnableMultisampling(GLsizei samples) {

    VI_ASSERT_LOG_ERROR_RETURN_VALUE(samples > 0, false, "Invalid sample count");

    // 检查最大采样数
    GLint maxSamples;
    GL(glGetIntegerv, GL_MAX_SAMPLES, &maxSamples);

    if (samples > maxSamples) {
        std::string errorStr = "Requested samples (" + std::to_string(samples) + ") exceeds maximum (" + std::to_string(maxSamples) + ")";
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, errorStr.data());
    }

    m_multisample.enabled = true;
    m_multisample.samples = samples;

    if (m_handle != 0 && m_isValid && m_width > 0 && m_height > 0) {
        Bind();
        bool success = AllocateStorage();
        Unbind();
        return success;
    }

    return true;
}

void RBO::AttachToFramebuffer(GLenum attachment) const {
    if (m_handle != 0 && m_isValid) {
        GL(glFramebufferRenderbuffer, GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, m_handle);
    }
}

void RBO::DetachFromFramebuffer(GLenum attachment) const {
    GL(glFramebufferRenderbuffer, GL_FRAMEBUFFER, attachment, GL_RENDERBUFFER, 0);
}

void RBO::Clear(const float* clearValue) const {
    if (!m_isValid) {
        return;
    }

    Bind();

    GLbitfield mask = 0;

    switch (m_type) {
    case RBOType::DEPTH:
        mask = GL_DEPTH_BUFFER_BIT;
        GL(glClearDepth, 1.0f);
        break;
    case RBOType::STENCIL:
        mask = GL_STENCIL_BUFFER_BIT;
        GL(glClearStencil, 0);
        break;
    case RBOType::DEPTH_STENCIL:
        mask = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
        GL(glClearDepth, 1.0f);
        GL(glClearStencil, 0);
        break;
    case RBOType::COLOR:
        mask = GL_COLOR_BUFFER_BIT;
        if (clearValue) {
            GL(glClearColor, clearValue[0], clearValue[1], clearValue[2], clearValue[3]);
        }
        else {
            GL(glClearColor, 0, 0, 0, 0);
        }
        break;
    }

    if (mask != 0) {
        GL(glClear, mask);
    }

    Unbind();
}

bool RBO::ReadPixels(void* data, size_t dataSize) const {
    if (!m_isValid) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "RBO is not valid");
    }

    size_t expectedSize = GetRBOTotalSize();
    if (dataSize < expectedSize) {
        auto errorStr = "Buffer too small: expected " + std::to_string(expectedSize) + " bytes, got " + std::to_string(dataSize);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, errorStr.data());
    }

    // 根据附件类型选择读取格式
    GLenum format = GL_RGBA;
    GLenum type = GL_UNSIGNED_BYTE;
    GLenum attachment = GL_COLOR_ATTACHMENT0;

    switch (m_type) {
    case RBOType::COLOR:
        format = GL_RGBA;
        type = GL_UNSIGNED_BYTE;
        attachment = GL_COLOR_ATTACHMENT0;
        break;
    case RBOType::DEPTH:
        format = GL_DEPTH_COMPONENT;
        type = m_format == RBOFormat::DEPTH_32F ? GL_FLOAT : GL_UNSIGNED_INT;  // 深度通常用 float
        attachment = GL_DEPTH_ATTACHMENT;
        break;
    case RBOType::STENCIL:
        format = GL_STENCIL_INDEX;
        type = GL_UNSIGNED_BYTE;
        attachment = GL_STENCIL_ATTACHMENT;
        break;
    case RBOType::DEPTH_STENCIL:
        format = GL_DEPTH_STENCIL;
        type = m_format == RBOFormat::DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_UNSIGNED_INT_24_8;
        attachment = GL_DEPTH_STENCIL_ATTACHMENT;
        break;
    }

    // 注意：读取RBO数据需要创建一个临时的FBO
    GLuint fbo;
    GL(glGenFramebuffers, 1, &fbo);
    GL(glBindFramebuffer, GL_FRAMEBUFFER, fbo);
    AttachToFramebuffer(attachment);

    // 检查FBO完整性
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        GL(glDeleteFramebuffers, 1, &fbo);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "Framebuffer is not complete");
    }

    // 读取像素
    GL(glReadPixels, 0, 0, m_width, m_height, format, type, data);

    // 清理
    DetachFromFramebuffer(attachment);
    GL(glDeleteFramebuffers, 1, &fbo);
    GL(glBindFramebuffer, GL_FRAMEBUFFER, 0);

    return true;
}

bool RBO::Validate() const {
    if (!m_isValid || m_handle == 0) {
        return false;
    }

    Bind();

    GLint param;
    GL(glGetRenderbufferParameteriv, GL_RENDERBUFFER, GL_RENDERBUFFER_WIDTH, &param);
    if (param != m_width) {
        Unbind();
        return false;
    }

    GL(glGetRenderbufferParameteriv, GL_RENDERBUFFER, GL_RENDERBUFFER_HEIGHT, &param);
    if (param != m_height) {
        Unbind();
        return false;
    }

    GL(glGetRenderbufferParameteriv, GL_RENDERBUFFER, GL_RENDERBUFFER_INTERNAL_FORMAT, &param);
    if (param != static_cast<GLint>(GetGLFormat())) {
        Unbind();
        return false;
    }

    if (m_multisample.enabled) {
        GL(glGetRenderbufferParameteriv, GL_RENDERBUFFER, GL_RENDERBUFFER_SAMPLES, &param);
        if (param != m_multisample.samples) {
            Unbind();
            return false;
        }
    }

    Unbind();
    return true;
}

bool RBO::AllocateStorage() {
    if (m_handle == 0 || m_width <= 0 || m_height <= 0) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "Invalid parameters for storage allocation");
    }

    GLenum internalFormat = GetGLFormat();

    if (m_multisample.enabled) {
        // 多重采样渲染缓冲区存储
        GL(glRenderbufferStorageMultisample, GL_RENDERBUFFER, m_multisample.samples, internalFormat, m_width, m_height);
    }
    else {
        // 普通渲染缓冲区存储
        GL(glRenderbufferStorage, GL_RENDERBUFFER, internalFormat, m_width, m_height);
    }

    return true;
}



bool RBO::GetDepthAttachmentInfoForFBO(GLuint srcFbo, RBOFormat& internalFormat) {
    //对硬件fbo查询会得到error code,硬件fbo的信息取决于上下文创建时候的设定
    if (srcFbo == 0) return false;
    // 保存当前绑定的状态
    GLint oldReadFbo;
    internalFormat = RBOFormat::DEPTH_24;

    GL(glGetIntegerv, GL_READ_FRAMEBUFFER_BINDING, &oldReadFbo);

    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, srcFbo);

    // 1. 查询深度附件是否存在,硬件fbo可能查不出来，其附件状态与ogl上下文创建时有关
    GLint objectType = 0;
    GL(glGetFramebufferAttachmentParameteriv, GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_OBJECT_TYPE, &objectType);
    if (objectType == GL_NONE) {
        GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, oldReadFbo);
        return false;
    }

    // 2. 查询分量类型（GL_FLOAT 或 GL_UNSIGNED_NORMALIZED）
    GLint componentType = 0;
    GL(glGetFramebufferAttachmentParameteriv, GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &componentType);

    // 3. 查询深度位数
    GLint depthSize = 0;
    GL(glGetFramebufferAttachmentParameteriv, GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &depthSize);

    // 4. 查询模板位数（如果存在打包格式，此值 > 0）
    // 注意：打包格式的模板位数需要从同一个深度附件上查询，或者从 GL_STENCIL_ATTACHMENT 查询
    GLint stencilSize = 0;
    GL(glGetFramebufferAttachmentParameteriv, GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilSize);
    // 如果上面查询不到，尝试从单独的模板附件查询
    if (stencilSize == 0) {
       // 此处查询会触发一个error code，原因待确认
       // GL(glGetFramebufferAttachmentParameteriv, GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencilSize);
    }

    // 情况1: 打包格式 Depth + Stencil
    if (stencilSize > 0) {
        if (depthSize == 24 && stencilSize == 8 && componentType == GL_UNSIGNED_NORMALIZED) {
            internalFormat = RBOFormat::DEPTH24_STENCIL8;
        }
        else if (depthSize == 32 && stencilSize == 8 && componentType == GL_FLOAT) {
            internalFormat = RBOFormat::DEPTH32F_STENCIL8;
        }
        else {
            // 其他罕见的组合 (如 GL_DEPTH24_STENCIL8 已经是主流) 如果无法精确匹配，回退到最常用的 GL_DEPTH24_STENCIL8
            internalFormat = RBOFormat::DEPTH24_STENCIL8;
        }
    }
    // 情况2: 纯深度格式
    else {
        if (componentType == GL_FLOAT) {
            internalFormat = RBOFormat::DEPTH_32F;
        }
        else if (componentType == GL_UNSIGNED_NORMALIZED) {
            // 根据位数选择对应的定点深度格式
            if (depthSize <= 16) internalFormat = RBOFormat::DEPTH_16;
            else if (depthSize <= 24) internalFormat = RBOFormat::DEPTH_24;
            else internalFormat = RBOFormat::DEPTH_32;  // 32位定点深度
        }
        else {
            // 默认回退
            internalFormat = RBOFormat::DEPTH_24;
        }
    }
    // 恢复状态
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, oldReadFbo);
    return true;
}

void RBO::ResolveToTexture(GLuint textureId, GLsizei targetWidth, GLsizei targetHeight) const {
    if (!m_multisample.enabled) {
        // 非MSAA可以直接复制
        Bind();
        GL(glBindTexture, GL_TEXTURE_2D, textureId);
        GL(glCopyTexImage2D, GL_TEXTURE_2D, 0, GetGLFormat(), 0, 0, m_width, m_height, 0);
        return;
    }

    // MSAA需要blit
    GLuint resolveFBO, drawFBO;

    // 源FBO
    GL(glGenFramebuffers, 1, &resolveFBO);
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, resolveFBO);
    GL(glFramebufferRenderbuffer, GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_handle);

    // 目标FBO
    GL(glGenFramebuffers, 1, &drawFBO);
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, drawFBO);
    GL(glFramebufferTexture2D, GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, textureId, 0);

    GLsizei targetW = (targetWidth > 0) ? targetWidth : m_width;
    GLsizei targetH = (targetHeight > 0) ? targetHeight : m_height;

    GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, targetW, targetH, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 清理
    GL(glDeleteFramebuffers, 1, &resolveFBO);
    GL(glDeleteFramebuffers, 1, &drawFBO);
    GL(glBindFramebuffer, GL_FRAMEBUFFER, 0);
}

void RBO::ResolveToRBO(const RBO& targetRBO) const {
    if (!m_multisample.enabled || !targetRBO.IsValid()) {
        return;
    }

    GLuint resolveFBO, drawFBO;

    GL(glGenFramebuffers, 1, &resolveFBO);
    GL(glGenFramebuffers, 1, &drawFBO);

    // 源
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, resolveFBO);
    GL(glFramebufferRenderbuffer, GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, m_handle);

    // 目标
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, drawFBO);
    GL(glFramebufferRenderbuffer, GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, targetRBO.GetHandle());

    GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, targetRBO.GetWidth(), targetRBO.GetHeight(), GL_COLOR_BUFFER_BIT, GL_NEAREST);

    GL(glDeleteFramebuffers, 1, &resolveFBO);
    GL(glDeleteFramebuffers, 1, &drawFBO);
    GL(glBindFramebuffer, GL_FRAMEBUFFER, 0);
}


size_t RBO::GetRBOTotalSize() const {
    return GetPixelSize() * m_width * m_height;
}

bool RBO::IsDepthStencilFormat() const {
    return (m_type == RBOType::DEPTH ||  m_type == RBOType::STENCIL || m_type == RBOType::DEPTH_STENCIL);
}

bool RBO::IsColorFormat() const {
    return m_type == RBOType::COLOR;
}

bool  RBO::IsMultisampled() const {
    return m_multisample.enabled;
}

size_t RBO::GetPixelSize() const {
    switch (m_format) {
    case RBOFormat::DEPTH_16: return 2;
    case RBOFormat::DEPTH_24: return 3;
    case RBOFormat::DEPTH_32: return 4;
    case RBOFormat::DEPTH_32F: return 4;
    case RBOFormat::DEPTH24_STENCIL8: return 4;
    case RBOFormat::DEPTH32F_STENCIL8: return 8;
    case RBOFormat::STENCIL_INDEX8: return 1;
    case RBOFormat::RGBA8: return 4;
    case RBOFormat::RGB8: return 3;
    default: return 0;
    }
}

GLenum RBO::GetGLFormat() const {
    switch (m_format) {
    case RBOFormat::DEPTH_16: return GL_DEPTH_COMPONENT16;
    case RBOFormat::DEPTH_24: return GL_DEPTH_COMPONENT24;
    case RBOFormat::DEPTH_32: return GL_DEPTH_COMPONENT32;
    case RBOFormat::DEPTH_32F: return GL_DEPTH_COMPONENT32F;
    case RBOFormat::DEPTH24_STENCIL8: return GL_DEPTH24_STENCIL8;
    case RBOFormat::DEPTH32F_STENCIL8: return GL_DEPTH32F_STENCIL8;
    case RBOFormat::STENCIL_INDEX8: return GL_STENCIL_INDEX8;
    case RBOFormat::RGBA8: return GL_RGBA8;
    case RBOFormat::RGB8: return GL_RGB8;
    default: return GL_RGBA8;
    }
}

GLenum RBO::getGLAttachment() const {
    switch (m_type) {
    case RBOType::DEPTH: return GL_DEPTH_ATTACHMENT;
    case RBOType::STENCIL: return GL_STENCIL_ATTACHMENT;
    case RBOType::DEPTH_STENCIL: return GL_DEPTH_STENCIL_ATTACHMENT;
    case RBOType::COLOR: return GL_COLOR_ATTACHMENT0;
    default: return GL_NONE;
    }
}

NAMESPACE_OPENGL_CORE_END
