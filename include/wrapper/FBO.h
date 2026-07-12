#pragma once

#include "ogl_defs.h"
#include "Texture.h"
#include "RBO.h"

#include <string>
#include <iostream>
#include <cstring>
#include <assert.h>
#include <vector>
#include <memory>
#include <unordered_map>
#include <map>
#include <optional>


NAMESPACE_OPENGL_CORE_BEGIN

/// <summary>
/// 临时方案，用于在上下文重建后重建fbo
/// </summary>
class FrameBufferHandle
{
public:
    FrameBufferHandle() = default;
    ~FrameBufferHandle() = default;

    FrameBufferHandle(FrameBufferHandle&& other) noexcept;

    // 禁用拷贝构造, 拷贝赋值, 移动赋值.
    FrameBufferHandle(const FrameBufferHandle&) = delete;
    FrameBufferHandle& operator=(const FrameBufferHandle&) = delete;
    FrameBufferHandle& operator=(FrameBufferHandle&&) = delete;

    GLuint Get() const; // 用于 glBindFramebuffer.
    GLuint& GetRef(); // 用于 glGenFramebuffers, glDeleteFramebuffers.

    void Destroy();

private:
#ifdef WIN32
    void* m_prevDC = nullptr;
    void* m_prevContext = nullptr;
    GLuint m_framebuffer = 0;
#else
    // linmingchun TODO: context 失效后, m_contextToFramebuffer 没办法移除对应的数据, 这里会有内存泄漏.
    std::map<void*, GLuint> m_contextToFramebuffer;
#endif
};


/// <summary>
/// fbo附件，附件可能来源于texture,multiSampleTexture,RBO
/// </summary>
class Attachment {
public:
    enum class Type { TEXTURE, MULTISAMPLE_TEXTURE, RENDERBUFFER, NONE };

    Attachment() : m_type(Type::NONE), m_attachmentPoint(GL_NONE) {}

    // 从普通纹理创建
    static Attachment FromTexture(std::shared_ptr<Texture> texture, GLenum attachmentPoint);

    // 从多重采样纹理创建
    static Attachment FromMultisampleTexture(std::shared_ptr<MultisampleTexture> texture, GLenum attachmentPoint);

    // 从RenderBuffer创建
    static Attachment FromRenderBuffer(std::shared_ptr<RBO> rbo, GLenum attachmentPoint);

    // 新增：调整大小
    void Resize(int newWidth, int newHeight);

    std::shared_ptr<Texture> GetTexture() const;
    std::shared_ptr<MultisampleTexture> GetMultisampleTexture() const;
    std::shared_ptr<RBO> GetRenderBuffer() const;

    int GetWidth() const;
    int GetHeight() const;
    int GetSamples() const;
    bool IsMultisampled() const;

    Type GetAttachmentType() const { return m_type; }
    bool IsValid() const { return m_type != Type::NONE; }

    //查询绑定点
    GLenum GetAttachmentPoint() const { return m_attachmentPoint; }

private:
    Attachment(Type type, GLenum attachmentPoint);

    Type m_type;
    GLenum m_attachmentPoint;
    std::shared_ptr<Texture> m_texture = nullptr;
    std::shared_ptr<MultisampleTexture> m_multisampleTexture = nullptr;
    std::shared_ptr<RBO> m_rbo = nullptr;
};


/// <summary>
/// FBO对象，管理fbo的生命周期
/// </summary>
class FBO {
public:
    explicit FBO(int width = 0, int height = 0);
    ~FBO();

    // 禁用拷贝
    FBO(const FBO&) = delete;
    FBO& operator=(const FBO&) = delete;

    void SetIsDefaultFBO(bool isDefaultFBO) { m_isDefaultFBO = isDefaultFBO; }

    // 设置附件
    void SetColorAttachment(const Attachment& attachment);
    void SetDepthAttachment(const Attachment& attachment);
    void SetStencilAttachment(const Attachment& attachment);
    void SetDepthStencilAttachment(const Attachment& attachment);

    // 移除附件
    void RemoveColorAttachment(GLenum attachmentPoint);
    void RemoveDepthAttachment();
    void RemoveStencilAttachment();
    void RemoveDepthStencilAttachment();

    // 获取附件，直接返回裸指针，有则返回指针，无则 nullptr
    const Attachment* GetColorAttachment(uint32_t index) const;
    const Attachment* GetDepthAttachment() const;
    const Attachment* GetStencilAttachment() const;
    const Attachment* GetDepthStencilAttachment() const;

    // 调整大小
    void Resize(int newWidth, int newHeight);

    // 绑定/解绑
    // todo:: bind 时候需要把read/draw/frame 三种状态的bind记录下来以便使用完后恢复
    // TODO:: 提供using scope 的使用方式
    // 附: 关于 glBindFramebuffer 的 参数 target GL行为说明 https://registry.khronos.org/OpenGL-Refpages/gl4/html/glBindFramebuffer.xhtml
    //     在 nVidia RTX A4000 得到验证
    // 1. 当 target 为 GL_FRAMEBUFFER 时: 会同时设置 GL_"DRAW"_FRAMEBUFFER 和 GL_"READ"_FRAMEBUFFER
    //    此时 调用 glGet GL_FRAMEBUFFER_BINDING, GL_"DRAW"_FRAMEBUFFER_BINDING 和 GL_"READ"_FRAMEBUFFER_BINDING 其结果都是 刚刚设置的 framebuffer 句柄
    // 2. 当 target 为 GL_"DRAW"_FRAMEBUFFER 时: 仅仅 只设置 "写" 缓冲
    //    此时 调用 glGet GL_FRAMEBUFFER_BINDING 或 GL_"DRAW"_FRAMEBUFFER_BINDING 都能获取 刚刚设置的 framebuffer 句柄
    //    但 GL_"READ"_FRAMEBUFFER_BINDING 不受影响
    // 3. 当 target 为 GL_"READ"_FRAMEBUFFER 时: 仅仅 只设置 "读" 缓冲
    //    此时 调用 glGet 仅仅 GL_"READ"_FRAMEBUFFER_BINDING 能获取 刚刚设置的 framebuffer 句柄
    //    而 GL_FRAMEBUFFER_BINDING 和 GL_"DRAW"_FRAMEBUFFER_BINDING 都不受影响
    // 总结: target 为 GL_FRAMEBUFFER 的行为 更贴近 "DRAW"
    void Use(GLenum target = GL_FRAMEBUFFER);
    void Unbind(GLuint perTarget = 0);

    // 检查FBO是否包含多重采样附件
    bool HasMultisampleAttachment() const;
    int GetMaxSamples() const;  // 获取所有附件的最大采样数

    // MRT支持
    void SetDrawBuffers(const std::vector<GLenum>& buffers);
    void SetReadBuffer(GLenum buffer);
    void EnableAllColorAttachments();

    // Blit操作（自动处理多重采样）
    void BlitTo(FBO* dst, GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST);
    void BlitFrom(FBO* src, GLbitfield mask = GL_DEPTH_BUFFER_BIT, GLenum filter = GL_NEAREST);
    //直接将图像解析传输到默认fbo的color缓冲中
    void BlitToDefaultFramebuffer(GLbitfield mask = GL_COLOR_BUFFER_BIT, GLenum filter = GL_NEAREST);

    // 多重采样解析 - 将多重采样FBO解析到非多重采样FBO
    void ResolveTo(FBO& dst, GLbitfield mask = GL_COLOR_BUFFER_BIT);
    void ResolveToDefaultFramebuffer(GLbitfield mask = GL_COLOR_BUFFER_BIT);

    // dump函数可能存在错误需要后续测试
    // 导出数据（自动处理多重采样）
    // bufferPtr 指定 nullptr, 可获得 buffer 大小
    size_t DumpColorBuffer(uint8_t* bufferPtr, uint32_t index = 0);
    std::vector<float> DumpDepthBuffer();
    std::vector<uint8_t> DumpStencilBuffer();

    // 清除
    // GL_COLOR_BUFFER_BIT/GL_DEPTH_BUFFER_BIT/GL_STENCIL_BUFFER_BIT 按位或（OR）可以清理多个缓存
    void ClearBufferBit(GLbitfield mask, const float* color = nullptr, float depth = 1.0f, int stencil = 0);
    void ClearColor(int attachmentIndex, const float* color);

    //获取FBO句柄，fbo重建时，重新附加附件
    GLuint GetHandle();

    // 获取信息
    int GetWidth() const { return m_width; }
    int GetHeight() const { return m_height; }
    bool IsComplete() const { return m_isComplete; }

private:
    void AttachToFBO(const Attachment& attachment); //附加附件
    void DetachFromFBO(GLenum attachmentPoint);     //移除附件
    void UpdateDrawBuffers();       // 更新MRT绘制目标
    void ReattachAllAttachments();  // 重新附加所有附件
    bool CheckCompleteness();  // 检查完整性

    FrameBufferHandle m_fboHandle;
    GLuint m_handle{0}; //TODO::临时方案,用于自动处理上下文
    int m_width{ 0 };
    int m_height{ 0 };
    bool m_isComplete{ false };

    bool m_isDefaultFBO{};

    // 附件存储
    //所有为attachment point ,并非0，1，2索引
    std::map<uint32_t, Attachment> m_colorAttachments;
    std::optional<Attachment> m_depthAttachment;
    std::optional<Attachment> m_stencilAttachment;
    std::optional<Attachment> m_depthStencilAttachment;

    // MRT配置
    std::vector<GLenum> m_drawBuffers;
    GLenum m_readBuffer = GL_COLOR_ATTACHMENT0;
};


/// <summary>
///  scope Use FBO，自动恢复状态
/// OpenGL 规范定义 DrawBuffers 是 FBO 对象的状态（FBO-bind-point state），在切换fbo时会自动切换drawbuffers,因而不需要在scopeUse中处理
/// </summary>
struct ScopeUseFBO {
    ScopeUseFBO(FBO& frameBuffer) {
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &draw);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &read);
        frameBuffer.Use();
    }
    ~ScopeUseFBO() {
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, draw);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, read);
    }
    void runOut() { m_isRunOut = true; }
    operator bool() const { return !m_isRunOut; }
private:
    bool m_isRunOut = false;
    GLint draw;
    GLint read;
};

NAMESPACE_OPENGL_CORE_END
