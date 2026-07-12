
#include "FBO.h"
#include "OglStateRecovery.h"
#include "ogl_defs.h"

#ifdef WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <GL/glx.h>
#endif

NAMESPACE_OPENGL_CORE_BEGIN

#pragma region FBOHandle

// ==================== FBOHandle 实现===============

static void* GetGLContext() {
#ifdef WIN32
    return wglGetCurrentContext();
#else
    return glXGetCurrentContext();
#endif
}

FrameBufferHandle::FrameBufferHandle(FrameBufferHandle&& other) noexcept {
#ifdef WIN32
    this->m_prevDC = other.m_prevDC;
    this->m_prevContext = other.m_prevContext;
    this->m_framebuffer = other.m_framebuffer;
    other.m_prevDC = nullptr;
    other.m_prevContext = nullptr;
    other.m_framebuffer = 0;
#else
    m_contextToFramebuffer = std::move(other.m_contextToFramebuffer);
#endif
}

GLuint FrameBufferHandle::Get() const {
#ifdef WIN32
    void* context = GetGLContext();
    if (context == m_prevContext) {
        return m_framebuffer;
    }
    else {
        // 这里不执行 glDeleteFramebuffers, 等外部调用 GetRef 时再执行 glDeleteFramebuffers.
        return 0;
    }
#else
    void* context = GetGLContext();
    auto iter = m_contextToFramebuffer.find(context);
    if (iter != m_contextToFramebuffer.end()) {
        return iter->second;
    }
    else {
        return 0;
    }
#endif
}

GLuint& FrameBufferHandle::GetRef() {
#ifdef WIN32
    void* hDC = wglGetCurrentDC();
    void* context = GetGLContext();
    if ((m_framebuffer != 0) && (context != m_prevContext)) { // m_framebuffer 等于 0 时不需要处理.
        // context 不同, 需要销毁之前创建的 framebuffer.
        // 首先尝试 MakeCurrent 创建 framebuffer 时记录的 context (m_framebuffer 不为 0 时, context 一定是创建 framebuffer 时记录的),
        // 成功则销毁 framebuffer, 失败不执行操作.
        if (m_prevDC && m_prevContext) {
            if (wglMakeCurrent((HDC)m_prevDC, (HGLRC)m_prevContext)) {
                GL(glDeleteFramebuffers, 1, &m_framebuffer);
            }
            //上下文已经失效了
            /*else {
                auto err = ::GetLastError();
                assert(false && "FrameBufferHandle::GetRef, Call wglMakeCurrent failed");
            }*/
            if (hDC) {
                // 恢复 context.
                // XXX: 上面 wglMakeCurrent 失败时是否需要恢复 context?
                bool bRet = wglMakeCurrent((HDC)hDC, (HGLRC)context);
                assert(bRet);
            }
        }
        m_framebuffer = 0;
    }
    // 更新 hDC 和 context.
    m_prevContext = context;
    m_prevDC = hDC;
    return m_framebuffer;
#else
    void* context = GetGLContext();
    auto iter = m_contextToFramebuffer.find(context);
    if (iter != m_contextToFramebuffer.end()) {
        return iter->second;
    }
    else {
        auto ret = m_contextToFramebuffer.insert(std::make_pair(context, 0)); // 插入默认值 0.
        assert(ret.second);
        return ret.first->second;
    }
#endif
}

void FrameBufferHandle::Destroy() {
#ifdef WIN32
    if (m_framebuffer != 0) {
        void* hDC = wglGetCurrentDC();
        void* context = GetGLContext();

        if (context != m_prevContext && m_prevDC && m_prevContext) {
            if (wglMakeCurrent((HDC)m_prevDC, (HGLRC)m_prevContext)) {
                GL(glDeleteFramebuffers, 1, &m_framebuffer);
            }
            else {
                auto err = ::GetLastError();
                VI_ASSERT_LOG_ERROR_RETURN(false, "FrameBufferHandle::Destroy, Call wglMakeCurrent failed, err = %ul, m_prevDC = %p, m_prevContext = %p", err, m_prevDC, m_prevContext);
            }
            if (hDC) {
                // 恢复 context.
                bool bRet = wglMakeCurrent((HDC)hDC, (HGLRC)context);
                assert(bRet);
            }
        }

    }
    m_framebuffer = 0;
    m_prevContext = nullptr;
    m_prevDC = nullptr;
#else
    void* context = GetGLContext();
    auto iter = m_contextToFramebuffer.find(context);
    if (iter != m_contextToFramebuffer.end()) {
        GL(glDeleteFramebuffers, 1, &(iter->second));
        iter->second = 0;
    }
#endif
}


#pragma endregion


#pragma region Attachment

// ==================== Attachment 实现 ====================

Attachment::Attachment(Type type, GLenum attachmentPoint) : m_type(type), m_attachmentPoint(attachmentPoint) {}

Attachment Attachment::FromTexture(std::shared_ptr<Texture> texture, GLenum attachmentPoint) {
    Attachment att(Type::TEXTURE, attachmentPoint);
    att.m_texture = texture;
    return att;
}

Attachment Attachment::FromMultisampleTexture(std::shared_ptr<MultisampleTexture> texture, GLenum attachmentPoint) {
    Attachment att(Type::MULTISAMPLE_TEXTURE, attachmentPoint);
    att.m_multisampleTexture = texture;
    return att;
}

Attachment Attachment::FromRenderBuffer(std::shared_ptr<RBO> rbo, GLenum attachmentPoint) {
    Attachment att(Type::RENDERBUFFER, attachmentPoint);
    att.m_rbo = rbo;
    return att;
}

void Attachment::Resize(int newWidth, int newHeight) {
    if (GetWidth() == newWidth && GetHeight() == newHeight) {
        return;  // 尺寸没变，不需要resize
    }

    switch (m_type) {
    case Type::TEXTURE:
        if (m_texture) {
            m_texture->Resize(newWidth, newHeight);
        }
        break;
    case Type::MULTISAMPLE_TEXTURE:
        if (m_multisampleTexture) {
            m_multisampleTexture->Resize(newWidth, newHeight);
        }
        break;

    case Type::RENDERBUFFER:
        if (m_rbo) {
            m_rbo->Resize(newWidth, newHeight);
        }
        break;
    default:
        break;
    }
}

std::shared_ptr<Texture> Attachment::GetTexture() const {
    return (m_type == Type::TEXTURE) ? m_texture : nullptr;
}

std::shared_ptr<MultisampleTexture> Attachment::GetMultisampleTexture() const {
    return (m_type == Type::MULTISAMPLE_TEXTURE) ? m_multisampleTexture : nullptr;
}

std::shared_ptr<RBO> Attachment::GetRenderBuffer() const {
    return (m_type == Type::RENDERBUFFER) ? m_rbo : nullptr;
}

int Attachment::GetWidth() const {
    if (m_type == Type::TEXTURE && m_texture) return m_texture->GetWidth();
    if (m_type == Type::MULTISAMPLE_TEXTURE && m_multisampleTexture) return m_multisampleTexture->GetWidth();
    if (m_type == Type::RENDERBUFFER && m_rbo) return m_rbo->GetWidth();
    return 0;
}

int Attachment::GetHeight() const {
    if (m_type == Type::TEXTURE && m_texture) return m_texture->GetHeight();
    if (m_type == Type::MULTISAMPLE_TEXTURE && m_multisampleTexture) return m_multisampleTexture->GetHeight();
    if (m_type == Type::RENDERBUFFER && m_rbo) return m_rbo->GetHeight();
    return 0;
}

int Attachment::GetSamples() const {
    if (m_type == Type::TEXTURE && m_texture) return 1;  // 普通纹理没有多重采样
    if (m_type == Type::MULTISAMPLE_TEXTURE && m_multisampleTexture) return m_multisampleTexture->GetSamples();
    if (m_type == Type::RENDERBUFFER && m_rbo) return m_rbo->GetSamples();
    return 0;
}

bool Attachment::IsMultisampled() const {
    return GetSamples() > 1;
}

#pragma endregion


#pragma region FBO

// ==================== FBO 实现 ====================

FBO::FBO(int width, int height) : m_width(width), m_height(height) {
    GLuint& fboHandle = m_fboHandle.GetRef();
    GL(glGenFramebuffers, 1, &fboHandle);
    m_handle = fboHandle;
    VI_ASSERT_LOG_ERROR_RETURN(fboHandle != 0, "FBO creation failed");
}

FBO::~FBO() {
    GLuint& fboHandle = m_fboHandle.GetRef();
    if (fboHandle != 0) {
        GL(glDeleteFramebuffers, 1, &fboHandle);
    }
}

GLuint FBO::GetHandle() {
    GLuint& handle = m_fboHandle.GetRef();
    //上下文重建了
    if(handle==0 && m_handle!=0){
        GL(glGenFramebuffers, 1, &m_handle);
        handle = m_handle;
        // 重新附加所有附件到FBO（因为纹理/RBO的ID可能改变）
        ReattachAllAttachments();
    }
    return m_handle;
}

bool FBO::HasMultisampleAttachment() const {
    for (const auto& [idx, att] : m_colorAttachments) {
        if (att.IsMultisampled()) return true;
    }
    if (m_depthAttachment.has_value() && m_depthAttachment->IsMultisampled()) return true;
    if (m_stencilAttachment.has_value() && m_stencilAttachment->IsMultisampled()) return true;
    if (m_depthStencilAttachment.has_value() && m_depthStencilAttachment->IsMultisampled()) return true;
    return false;
}

int FBO::GetMaxSamples() const {
    int maxSamples = 0;
    for (const auto& [idx, att] : m_colorAttachments) {
        maxSamples = std::max(maxSamples, att.GetSamples());
    }
    if (m_depthAttachment.has_value()) {
        maxSamples = std::max(maxSamples, m_depthAttachment->GetSamples());
    }
    if (m_stencilAttachment.has_value()) {
        maxSamples = std::max(maxSamples, m_stencilAttachment->GetSamples());
    }
    if (m_depthStencilAttachment.has_value()) {
        maxSamples = std::max(maxSamples, m_depthStencilAttachment->GetSamples());
    }
    return maxSamples;
}

void FBO::SetColorAttachment(const Attachment& attachment) {
    if (!attachment.IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "Invalid Color attachment");
    }

    if (m_width == 0 || m_height == 0) {
        m_width = attachment.GetWidth();
        m_height = attachment.GetHeight();
    }

    GLenum attachPoint = attachment.GetAttachmentPoint();
    auto idx = (uint32_t)(attachPoint);
    m_colorAttachments[idx] = attachment;
    AttachToFBO(attachment);
    UpdateDrawBuffers();
    CheckCompleteness();
}

void FBO::SetDepthAttachment(const Attachment& attachment) {
    if (!attachment.IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "Invalid Depth attachment");
    }

    if (m_width == 0 || m_height == 0) {
        m_width = attachment.GetWidth();
        m_height = attachment.GetHeight();
    }

    m_depthAttachment = attachment;
    AttachToFBO(attachment);
    CheckCompleteness();
}

void FBO::SetStencilAttachment(const Attachment& attachment) {
    if (!attachment.IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "Invalid Stencil attachment");
    }

    if (m_width == 0 || m_height == 0) {
        m_width = attachment.GetWidth();
        m_height = attachment.GetHeight();
    }

    m_stencilAttachment = attachment;
    AttachToFBO(attachment);
    CheckCompleteness();
}

void FBO::SetDepthStencilAttachment(const Attachment& attachment) {
    if (!attachment.IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "Invalid DepthStencil attachment");
    }

    if (m_width == 0 || m_height == 0) {
        m_width = attachment.GetWidth();
        m_height = attachment.GetHeight();
    }

    m_depthStencilAttachment = attachment;
    AttachToFBO(attachment);
    CheckCompleteness();
}

void FBO::AttachToFBO(const Attachment& attachment) {

    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;

    GL(glBindFramebuffer, GL_FRAMEBUFFER, m_handle);

    GLenum attachPoint = attachment.GetAttachmentPoint();

    if (attachment.GetAttachmentType() == Attachment::Type::TEXTURE) {
        auto texture = attachment.GetTexture();
        texture->AttachToFBO(attachPoint, GL_FRAMEBUFFER);
    }
    else if (attachment.GetAttachmentType() == Attachment::Type::MULTISAMPLE_TEXTURE) {
        auto multisampleTex = attachment.GetMultisampleTexture();
        GL(glFramebufferTexture2D, GL_FRAMEBUFFER, attachPoint, GL_TEXTURE_2D_MULTISAMPLE, multisampleTex->GetHandle(), 0);
    }
    else if (attachment.GetAttachmentType() == Attachment::Type::RENDERBUFFER) {
        auto rbo = attachment.GetRenderBuffer();
        GL(glFramebufferRenderbuffer, GL_FRAMEBUFFER, attachPoint, GL_RENDERBUFFER, rbo->GetHandle());
    }
}

void FBO::DetachFromFBO(GLenum attachmentPoint) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    GL(glBindFramebuffer, GL_FRAMEBUFFER, m_handle);
    GL(glFramebufferTexture2D, GL_FRAMEBUFFER, attachmentPoint, GL_TEXTURE_2D, 0, 0);
    GL(glFramebufferRenderbuffer, GL_FRAMEBUFFER, attachmentPoint, GL_RENDERBUFFER, 0);
}

void FBO::RemoveColorAttachment(GLenum attachmentPoint) {
    auto idx = (uint32_t)(attachmentPoint);
    if (auto it = m_colorAttachments.find(idx); it != m_colorAttachments.end()) {
        DetachFromFBO(attachmentPoint);
        m_colorAttachments.erase(it);
        UpdateDrawBuffers();
        CheckCompleteness();
    }
}

void FBO::RemoveDepthAttachment() {
    if (m_depthAttachment.has_value()) {
        DetachFromFBO(GL_DEPTH_ATTACHMENT);
        m_depthAttachment.reset();
        CheckCompleteness();
    }
}

void FBO::RemoveStencilAttachment() {
    if (m_stencilAttachment.has_value()) {
        DetachFromFBO(GL_STENCIL_ATTACHMENT);
        m_stencilAttachment.reset();
        CheckCompleteness();
    }
}

void FBO::RemoveDepthStencilAttachment() {
    if (m_depthStencilAttachment.has_value()) {
        DetachFromFBO(GL_DEPTH_STENCIL_ATTACHMENT);
        m_depthStencilAttachment.reset();
        CheckCompleteness();
    }
}


// 直接返回裸指针，有则返回指针，无则 nullptr
// index 从0开始计数
const Attachment* FBO::GetColorAttachment(uint32_t index) const {
    auto it = m_colorAttachments.find(index + GL_COLOR_ATTACHMENT0);
    return (it != m_colorAttachments.end()) ? &it->second : nullptr;
}

const Attachment* FBO::GetDepthAttachment() const {
    return m_depthAttachment.has_value() ? &m_depthAttachment.value() : nullptr;
}

const Attachment* FBO::GetStencilAttachment() const {
    return m_stencilAttachment.has_value() ? &m_stencilAttachment.value() : nullptr;
}

const Attachment* FBO::GetDepthStencilAttachment() const {
    return m_depthStencilAttachment.has_value() ? &m_depthStencilAttachment.value() : nullptr;
}

void FBO::ReattachAllAttachments() {
    // 重新附加所有颜色附件
    for (const auto& [index, attachment] : m_colorAttachments) {
        AttachToFBO(attachment);
        CheckCompleteness();
    }

    // 重新附加深度附件
    if (m_depthAttachment.has_value()) {
        AttachToFBO(m_depthAttachment.value());
        CheckCompleteness();
    }

    // 重新附加模板附件
    if (m_stencilAttachment.has_value()) {
        AttachToFBO(m_stencilAttachment.value());
        CheckCompleteness();
    }

    // 重新附加深度模板附件
    if (m_depthStencilAttachment.has_value()) {
        AttachToFBO(m_depthStencilAttachment.value());
        CheckCompleteness();
    }

    // 更新draw buffers
    UpdateDrawBuffers();
    
}

void FBO::Resize(int newWidth, int newHeight) {
    if (newWidth == m_width && newHeight == m_height) {
        return;
    }

    m_width = newWidth;
    m_height = newHeight;

    // 调整所有附件的大小
    for (auto& [index, attachment] : m_colorAttachments) {
        attachment.Resize(newWidth, newHeight);
    }

    if (m_depthAttachment.has_value()) {
        m_depthAttachment->Resize(newWidth, newHeight);
    }

    if (m_stencilAttachment.has_value()) {
        m_stencilAttachment->Resize(newWidth, newHeight);
    }

    if (m_depthStencilAttachment.has_value()) {
        m_depthStencilAttachment->Resize(newWidth, newHeight);
    }

    // 重新附加所有附件到FBO（因为纹理/RBO的ID可能改变）
    ReattachAllAttachments();
}

void FBO::Use(GLenum target) {

    const GLuint fboHandle = GetHandle();
    GL(glBindFramebuffer, target, fboHandle);

    if (target == GL_FRAMEBUFFER || target == GL_DRAW_FRAMEBUFFER) {
        if (!m_drawBuffers.empty()) {
            GL(glDrawBuffers, static_cast<GLsizei>(m_drawBuffers.size()), m_drawBuffers.data());
        }
    }

    if (target == GL_FRAMEBUFFER || target == GL_READ_FRAMEBUFFER) {
        GL(glReadBuffer, m_readBuffer);
    }

    // 重新附加所有附件到FBO（fbo可能因为上下文问题重建了）
    ReattachAllAttachments();
}

void FBO::Unbind(GLuint perTarget) {
    GL(glBindFramebuffer, GL_FRAMEBUFFER, perTarget);
}

void FBO::UpdateDrawBuffers() {
    m_drawBuffers.clear();
    for (const auto& [index, attachment] : m_colorAttachments) {
        m_drawBuffers.push_back(attachment.GetAttachmentPoint());
    }
}

void FBO::EnableAllColorAttachments() {
    UpdateDrawBuffers();
    const GLuint fboHandle = GetHandle();
    if (fboHandle != 0) {
        OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
        Use();
        if (!m_drawBuffers.empty()) {
            GL(glDrawBuffers, static_cast<GLsizei>(m_drawBuffers.size()), m_drawBuffers.data());
        }
    }
}

bool FBO::CheckCompleteness() {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    GL(glBindFramebuffer, GL_FRAMEBUFFER, m_handle);
    GLenum status = GL(glCheckFramebufferStatus, GL_FRAMEBUFFER);
    m_isComplete = status == GL_FRAMEBUFFER_COMPLETE;
    if (!m_isComplete) {
        switch (status) {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, " FBO Incomplete attachment");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Missing attachment");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Incomplete draw buffer");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Incomplete read buffer");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Unsupported");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Incomplete multisample");
            break;
        default:
            VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "FBO Unknown error");
            break;
        }
    }
    return m_isComplete;
}

void FBO::SetDrawBuffers(const std::vector<GLenum>& buffers) {
    m_drawBuffers = buffers;
    const GLuint fboHandle = GetHandle();
    if (fboHandle != 0) {
        OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
        Use();
        GL(glDrawBuffers, static_cast<GLsizei>(buffers.size()), buffers.data());
    }
}

void FBO::SetReadBuffer(GLenum buffer) {
    m_readBuffer = buffer;
    const GLuint fboHandle = GetHandle();
    if (fboHandle != 0) {
        OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
        Use();
        GL(glReadBuffer, buffer);
    }
}

void FBO::BlitTo(FBO* dst, GLbitfield mask, GLenum filter) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    const GLuint fboHandle = GetHandle();
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, dst->GetHandle());
    GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, dst->GetWidth(), dst->GetHeight(), mask, filter);
}

void FBO::BlitFrom(FBO* src, GLbitfield mask, GLenum filter) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    const GLuint fboHandle = GetHandle();
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, src->GetHandle());
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, fboHandle);
    GL(glBlitFramebuffer, 0, 0, src->GetWidth(), src->GetHeight(), 0, 0, m_width, m_height, mask, filter);
}

void FBO::BlitToDefaultFramebuffer(GLbitfield mask, GLenum filter) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    const GLuint fboHandle = GetHandle();
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboHandle);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
    glBlitFramebuffer(0, 0, m_width, m_height, 0, 0, m_width, m_height, mask, filter);
}

void FBO::ResolveTo(FBO& dst, GLbitfield mask) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    const GLuint fboHandle = GetHandle();
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, dst.GetHandle());
    GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, dst.GetWidth(), dst.GetHeight(), mask, GL_NEAREST);
}

void FBO::ResolveToDefaultFramebuffer(GLbitfield mask) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    const GLuint fboHandle = GetHandle();
    GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
    GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, 0);
    GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, m_width, m_height, mask, GL_NEAREST);
}

size_t FBO::DumpColorBuffer(uint8_t* bufferPtr, uint32_t index) {
    auto idx = (uint32_t)(index + GL_COLOR_ATTACHMENT0);
    auto it = m_colorAttachments.find(idx);
    if (it == m_colorAttachments.end()) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, {}, "Color attachment not found");
    }
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    
    const Attachment& att = it->second;

    if (att.IsMultisampled()) {
        const auto size = m_width * m_height * 4;
        if (bufferPtr == nullptr) return  size;
        // 多重采样：需要先解析到临时FBO
        GLuint tempFBO, tempTexture;
        GL(glGenFramebuffers, 1, &tempFBO);
        GL(glGenTextures, 1, &tempTexture);

        GL(glBindTexture, GL_TEXTURE_2D, tempTexture);
        GL(glTexImage2D, GL_TEXTURE_2D, 0, GL_RGBA8, m_width, m_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tempTexture, 0);

        const GLuint fboHandle = GetHandle();
        GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
        GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, tempFBO);
        GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glReadPixels, 0, 0, m_width, m_height, GL_RGBA, GL_UNSIGNED_BYTE, bufferPtr);

        GL(glDeleteFramebuffers, 1, &tempFBO);
        GL(glDeleteTextures, 1, &tempTexture);

        return size;
    }
    else {
        // 非多重采样：直接读取
        Use();
        GLenum formart = Texture::ToGLFormat(att.GetTexture()->GetFormat());
        GLenum type = Texture::ToGLType(att.GetTexture()->GetFormat());
        size_t sizePerPixel = Texture::GetBytesPerPixel(att.GetTexture()->GetFormat());
        const auto size = sizePerPixel * m_width * m_height;
        if (bufferPtr == nullptr) return  size;
        GL(glReadBuffer, idx);
        GL(glReadPixels, 0, 0, m_width, m_height, formart, type, bufferPtr);
        return  size;
    }
   
}

std::vector<float> FBO::DumpDepthBuffer() {
    if (!m_depthAttachment.has_value() && !m_depthStencilAttachment.has_value()) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, {}, "No depth attachment");
    }
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;

    std::vector<float> data(m_width * m_height);

    bool isMultisampled = (m_depthAttachment.has_value() && m_depthAttachment->IsMultisampled()) ||  (m_depthStencilAttachment.has_value() && m_depthStencilAttachment->IsMultisampled());
    if (isMultisampled) {
        GLuint tempFBO, tempTexture;
        GL(glGenFramebuffers, 1, &tempFBO);
        GL(glGenTextures, 1, &tempTexture);

        GL(glBindTexture, GL_TEXTURE_2D, tempTexture);
        GL(glTexImage2D, GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, m_width, m_height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        GL(glTexParameteri, GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, tempTexture, 0);
        const GLuint fboHandle = GetHandle();
        GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
        GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, tempFBO);
        GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glReadPixels, 0, 0, m_width, m_height, GL_DEPTH_COMPONENT, GL_FLOAT, data.data());

        GL(glDeleteFramebuffers, 1, &tempFBO);
        GL(glDeleteTextures, 1, &tempTexture);
    }
    else {
        Use();
        GL(glReadPixels, 0, 0, m_width, m_height, GL_DEPTH_COMPONENT, GL_FLOAT, data.data());
    }

    return data;
}

std::vector<uint8_t> FBO::DumpStencilBuffer() {
    if (!m_stencilAttachment.has_value() && !m_depthStencilAttachment.has_value()) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, {}, "No stencil attachment");
    }
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    std::vector<uint8_t> data(m_width * m_height);

    bool isMultisampled = (m_stencilAttachment.has_value() && m_stencilAttachment->IsMultisampled()) || (m_depthStencilAttachment.has_value() && m_depthStencilAttachment->IsMultisampled());

    if (isMultisampled) {
        GLuint tempFBO, tempTexture;
        GL(glGenFramebuffers, 1, &tempFBO);
        GL(glGenTextures, 1, &tempTexture);

        GL(glBindTexture, GL_TEXTURE_2D, tempTexture);
        GL(glTexImage2D, GL_TEXTURE_2D, 0, GL_STENCIL_INDEX8, m_width, m_height, 0, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, nullptr);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glFramebufferTexture2D, GL_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, tempTexture, 0);
        const GLuint fboHandle = GetHandle();
        GL(glBindFramebuffer, GL_READ_FRAMEBUFFER, fboHandle);
        GL(glBindFramebuffer, GL_DRAW_FRAMEBUFFER, tempFBO);
        GL(glBlitFramebuffer, 0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_STENCIL_BUFFER_BIT, GL_NEAREST);

        GL(glBindFramebuffer, GL_FRAMEBUFFER, tempFBO);
        GL(glReadPixels, 0, 0, m_width, m_height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, data.data());

        GL(glDeleteFramebuffers, 1, &tempFBO);
        GL(glDeleteTextures, 1, &tempTexture);
    }
    else {
        Use();
        GL(glReadPixels, 0, 0, m_width, m_height, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, data.data());
    }

    return data;
}

void FBO::ClearBufferBit(GLbitfield mask, const float* color, float depth, int stencil) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    Use();

    if (mask & GL_COLOR_BUFFER_BIT && color != nullptr) {
        GL(glClearColor, color[0], color[1], color[2], color[3]);
    }
    if (mask & GL_DEPTH_BUFFER_BIT) {
        GL(glClearDepth, depth);
    }
    if (mask & GL_STENCIL_BUFFER_BIT) {
        GL(glClearStencil, stencil);
    }
    GL(glClear, mask);
}

void FBO::ClearColor(int attachmentIndex, const float* color) {
    OGLCore::StatesRecovery<GL_FRAMEBUFFER_BINDING> STATUS_REC;
    Use();
    GL(glClearBufferfv, GL_COLOR, attachmentIndex, color);
}

#pragma endregion


#pragma region 使用示例

/*
使用示例：

cpp
// 创建普通纹理
auto colorTex = std::make_shared<Texture>();
colorTex->create2D(1024, 768, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

// 创建多重采样纹理
auto colorTexMS = std::make_shared<MultisampleTexture>();
colorTexMS->create(1024, 768, 4, GL_RGBA8);

// 创建RBO（支持多重采样）
auto depthRBO = std::make_shared<RenderBuffer>();
depthRBO->createMultisample(1024, 768, 4, GL_DEPTH_COMPONENT24);

// 创建FBO
FBO fbo(1024, 768);

// 设置附件
fbo.setColorAttachment(0, Attachment::fromMultisampleTexture(colorTexMS, GL_COLOR_ATTACHMENT0));
fbo.setDepthAttachment(Attachment::fromRenderBuffer(depthRBO, GL_DEPTH_ATTACHMENT));

// MRT混合使用普通纹理和多重采样纹理
auto colorTex2 = std::make_shared<Texture>();
colorTex2->create2D(1024, 768, GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE);

fbo.setColorAttachment(1, Attachment::fromTexture(colorTex2, GL_COLOR_ATTACHMENT1));
fbo.setDrawBuffers({ GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 });

// 检查完整性
if (fbo.checkCompleteness()) {
    std::cout << "FBO is complete" << std::endl;
}
*/

#pragma endregion


NAMESPACE_OPENGL_CORE_END
