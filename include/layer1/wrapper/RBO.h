#pragma once

#include "ogl_defs.h"

#include <string>
#include <iostream>
#include <cstring>


NAMESPACE_OPENGL_CORE_BEGIN

/**
 * RBO类型枚举
 */
enum class RBOType {
    DEPTH,          // 深度缓冲区
    STENCIL,        // 模板缓冲区
    DEPTH_STENCIL,  // 深度+模板组合
    COLOR           // 颜色缓冲区
};

/**
 * RBO格式枚举
 */
enum class RBOFormat {
    DEPTH_16,      // 16位深度 （归一化）
    DEPTH_24,      // 24位深度 （归一化）
    DEPTH_32,      // 32位深度 （归一化）
    DEPTH_32F,     // 32位浮点深度 （float类型）
    DEPTH24_STENCIL8,       // 24位深度+8位模板 （uintl类型）
    DEPTH32F_STENCIL8,      // 32位浮点深度+8位模板
    STENCIL_INDEX8,         // 8位模板
    RGBA8,                  // 8位RGBA颜色
    RGB8                    // 8位RGB颜色
};

/**
 * 多重采样配置
 */
struct RBOMultisampleConfig {
    bool enabled = false;
    GLsizei samples = 4;        // 采样数：2,4,8,16
};

/**
 * RBO包装类
 * 代表一个OpenGL Renderbuffer Object
 */
class RBO {
public:

    RBO();
    ~RBO();

    // 拷贝构造函数 - 禁用
    RBO(const RBO&) = delete;
    RBO& operator=(const RBO&) = delete;

    /**
     * 创建/重新创建RBO
     * @param type RBO类型
     * @param format 数据格式
     * @param width 宽度
     * @param height 高度
     * @param multisample 多重采样配置
     * @return 是否创建成功
     */
    bool Create(RBOType type, RBOFormat format, GLsizei width, GLsizei height, const RBOMultisampleConfig& multisample = RBOMultisampleConfig());

    // 绑定RBO
    void Bind() const;

    //解绑RBO
    void Unbind() const;

    // 验证RBO完整性
    bool Validate() const;

    //调整RBO大小
    bool Resize(GLsizei width, GLsizei height);

    // 启用多重采样
    bool EnableMultisampling(GLsizei samples);

    /**
     * 附加到当前绑定的帧缓冲区（FBO）
     * @param attachment 附件类型（GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT,  GL_DEPTH_STENCIL_ATTACHMENT, GL_COLOR_ATTACHMENT0等）
     */
    void AttachToFramebuffer(GLenum attachment) const;

    /**
     * 从当前绑定的帧缓冲区分离指定附件
     * @param attachment 附件类型
     */
    void DetachFromFramebuffer(GLenum attachment) const;

    /**
     * 清除RBO内容
     * @param clearValue 清除值（仅对颜色缓冲区有效，RGBA数组）
     */
    void Clear(const float* clearValue = nullptr) const;

    /**
     * 读取RBO数据到内存（只支持颜色的读取，暂不支持深度和模板的读取）
     * @param data 输出数据缓冲区
     * @param dataSize 缓冲区大小
     * @return 是否成功
     */
    bool ReadPixels(void* data, size_t dataSize) const;

    /**
     * 解析到纹理（用于MSAA RBO）
     * @param textureId 目标纹理ID
     * @param targetWidth 目标宽度（0表示使用当前宽度）
     * @param targetHeight 目标高度
     */
    void ResolveToTexture(GLuint textureId, GLsizei targetWidth = 0, GLsizei targetHeight = 0) const;

    /**
     * 解析到另一个RBO（用于MSAA）
     * @param targetRBO 目标RBO
     */
    void ResolveToRBO(const RBO& targetRBO) const;

    
    // ===============获取信息================
    GLuint GetHandle() const { return m_handle; }     // 获取RBO ID
    RBOType GetType() const { return m_type; }        // 获取RBO类型
    RBOFormat GetFormat() const { return m_format; }  // 获取RBO格式
    GLsizei GetWidth() const { return m_width; }      // 获取宽度
    GLsizei GetHeight() const { return m_height; }    // 获取高度
    GLsizei GetSamples() const { return m_multisample.enabled ? m_multisample.samples : 1; }   //获取采样点个数 
    size_t GetRBOTotalSize() const;        // 获取当前RBO的总字节大小

    bool IsValid() const { return m_isValid; } // 是否有效
    bool IsMultisampled() const;        // 是否是多重采样
    bool IsDepthStencilFormat() const;  // 判断是否是深度/模板格式
    bool IsColorFormat() const;         // 判断是否是颜色格式

    // false时候可能是无附件，也可能是硬件fbo，查询不到，其内部格式在结构中是DEPTH_24
    static bool GetDepthAttachmentInfoForFBO(GLuint srcFbo, RBOFormat& internalFormat);

private:
    GLuint m_handle = 0;                    // OpenGL RBO ID
    RBOType m_type = RBOType::DEPTH_STENCIL;                 // RBO类型
    RBOFormat m_format = RBOFormat::DEPTH24_STENCIL8;             // 数据格式
    GLsizei m_width = 0;                // 宽度
    GLsizei m_height = 0;               // 高度
    bool m_isValid = false;                 // 是否有效
    RBOMultisampleConfig m_multisample; // 多重采样配置

    //内部创建函数,分配存储空间
    bool AllocateStorage();

    //释放RBO资源
    void Destroy();

    // OpenGL格式映射
    // ==================== 工具函数 ====================
    GLenum GetGLFormat() const;    //获取内部格式对应的ogl枚举
    size_t GetPixelSize() const;  // 获取内部格式的字节大小（每像素）
    GLenum getGLAttachment() const; //获取RBO类型对应的attarchment枚举
};

NAMESPACE_OPENGL_CORE_END
