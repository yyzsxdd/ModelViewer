#pragma once

#include "ogl_defs.h"

#include <memory>
#include <string>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <vector>
#include <cstdint>

// Image 类声明已在 ogl_defs.h 中提供

NAMESPACE_OPENGL_CORE_BEGIN

/*1. 基本维度类型
这是最常用、最基础的纹理类型，用于存储图像或体积数据。

纹理类型	目标常量	维度	坐标	主要用途
1D 纹理	GL_TEXTURE_1D	1 维(条状)	s(单坐标)	查色表(LUT)、频谱图、随时间变化的1D数据
2D 纹理	GL_TEXTURE_2D	2 维(平面)	s, t	    最常用：普通图片、精灵、UI、法线贴图等
3D 纹理	GL_TEXTURE_3D	3 维(体积)	s, t, r	    体积云 / 雾、医学影像(CT / MRI)、体数据可视化
注意：2D纹理中有一个特殊变体——矩形纹理(GL_TEXTURE_RECTANGLE)。它不使用归一化坐标(0 - 1)，而是使用像素坐标(0 - width)，且不支持mipmap。适合需要精确像素访问的场景(如帧缓冲区的直接拷贝)。

2. 数组纹理(Array Textures)
将多个相同尺寸和格式的同维度纹理组合成一个数组对象，通过一个额外的层索引(layer)来访问。

纹理类型	目标常量	结构描述	核心优势
1D 数组	GL_TEXTURE_1D_ARRAY	尺寸 : (宽度, 层数)	        轻量级2D效果，如多组1D LUT
2D 数组	GL_TEXTURE_2D_ARRAY	尺寸 : (宽度, 高度, 层数)	    最实用：纹理数组、角色动画每一帧的贴图集、体素世界的切片
与传统纹理集(Texture Atlas)相比，它没有边界接缝问题，mipmap生成独立自然，是最佳实践方法。

3. 缓冲纹理(Buffer Texture)
GL_TEXTURE_BUFFER：将缓冲区(Buffer Object)的内容当作一维纹理来访问。

特点：尺寸很大(接近缓冲区上限)，只支持线性采样，纹理坐标是整数索引。
用途：需要在着色器中随机读取大数据数组，但又想利用纹理函数(如 imageLoad)时使用，但更现代的方案是直接用 SSBO。

4. 多采样纹理(Multisample Textures)
GL_TEXTURE_2D_MULTISAMPLE / GL_TEXTURE_2D_MULTISAMPLE_ARRAY：用于开启多采样抗锯齿(MSAA)的帧缓冲。

特点：每个像素包含多个样本，不能使用mipmap，采样时需要使用特殊的texelFetch指令。
关键区别：普通纹理存储的是"每个像素一个颜色"，而多采样纹理存储的是"每个像素多个颜色样本"，用于在渲染到纹理时避免锯齿边缘锐化。

5. 立方体贴图(Cube Map)
GL_TEXTURE_CUBE_MAP：由6个正方形纹理面构成的立方体。
坐标：三维方向向量(s, t, r)，从立方体中心向外指向某个面。

主要用途：
环境映射(天空盒 / Skybox)：模拟周围环境反射。
动态反射：设置相机朝6个方向渲染，生成当前场景的反射纹理。
光照探针：存储来自所有方向的光照信息。
变体：GL_TEXTURE_CUBE_MAP_ARRAY(OpenGL 4.0 + )，支持多个立方体贴图组成的数组。

6. 其他特殊类型
类型	目标常量	描述	使用场景
2D 多采样数组	GL_TEXTURE_2D_MULTISAMPLE_ARRAY	多采样 + 数组纹理	高端延迟渲染管线(Deferred Rendering)中的 MSAA 附件
立方体贴图数组	GL_TEXTURE_CUBE_MAP_ARRAY	多个立方体贴图层叠	级联阴影映射(Cascaded Shadow Maps)中为每个光源存储一个完整的立方体贴图

如何选择合适的纹理类型？
渲染普通3D模型的漫反射 / 法线贴图：GL_TEXTURE_2D。
加载字体图集或粒子动画序列：GL_TEXTURE_2D_ARRAY。
实现游戏的天空盒 / 反射效果：GL_TEXTURE_CUBE_MAP。
存储医学影像数据(如3D块状数据)：GL_TEXTURE_3D。
实现需要高精度的UI或屏幕后处理(复制帧缓冲)：GL_TEXTURE_RECTANGLE。
需要在着色器中读取自定义数据结构(如粒子ID数组)：GL_TEXTURE_BUFFER 或 直接使用SSBO。
渲染抗锯齿的离屏效果(如软阴影纹理)：GL_TEXTURE_2D_MULTISAMPLE。
*/

/**
 * @brief 纹理类型枚举
 * 定义了支持的纹理维度类型，每种类型对应 OpenGL 的目标枚举值
 */
enum class TextureType {
    Texture1D = GL_TEXTURE_1D,              ///< 一维纹理
    Texture2D = GL_TEXTURE_2D,              ///< 二维纹理
    Texture3D = GL_TEXTURE_3D,              ///< 三维纹理（体纹理）
    TextureCubeMap = GL_TEXTURE_CUBE_MAP    ///< 立方体贴图（6个面）
};

/**
 * @brief 纹理内部格式枚举
 * 定义了纹理在 GPU 中的存储格式，包括颜色、浮点、整数和深度格式
 */
enum class TextureFormat {
    // ---- 无符号归一化 8位整型格式 ---- 6
    R8 = GL_R8,           
    RG8 = GL_RG8,           
    RGB8 = GL_RGB8,          
    RGBA8 = GL_RGBA8,         
    SRGB8 = GL_SRGB8,        
    SRGB8_A8 = GL_SRGB8_ALPHA8,  

    // ---- 无符号归一化 16位格式 ---- 4
    R16 = GL_R16,           
    RG16 = GL_RG16,         
    RGB16 = GL_RGB16,         
    RGBA16 = GL_RGBA16,       

    // ---- 无符号整数格式 ---- 12
    R8UI = GL_R8UI,        
    RG8UI = GL_RG8UI,       
    RGB8UI = GL_RGB8UI,      
    RGBA8UI = GL_RGBA8UI,     
    R16UI = GL_R16UI,       
    RG16UI = GL_RG16UI,     
    RGB16UI = GL_RGB16UI,     
    RGBA16UI = GL_RGBA16UI,    
    R32UI = GL_R32UI,      
    RG32UI = GL_RG32UI,     
    RGB32UI = GL_RGB32UI,    
    RGBA32UI = GL_RGBA32UI,    

    // ---- 有符号整数格式 ---- 12
    R8I = GL_R8I,         
    RG8I = GL_RG8I,       
    RGB8I = GL_RGB8I,       
    RGBA8I = GL_RGBA8I,      
    R16I = GL_R16I,        
    RG16I = GL_RG16I,       
    RGB16I = GL_RGB16I,      
    RGBA16I = GL_RGBA16I,     
    R32I = GL_R32I,       
    RG32I = GL_RG32I,      
    RGB32I = GL_RGB32I,     
    RGBA32I = GL_RGBA32I,    

    // ---- 浮点格式 ---- 8
    R16F = GL_R16F,       
    RG16F = GL_RG16F,       
    RGB16F = GL_RGB16F,      
    RGBA16F = GL_RGBA16F,     
    R32F = GL_R32F,        
    RG32F = GL_RG32F,      
    RGB32F = GL_RGB32F,      
    RGBA32F = GL_RGBA32F,    

    // ---- 深度格式 ---- 2
    DEPTH16 = GL_DEPTH_COMPONENT16,   // GL_DEPTH_COMPONENT16
    DEPTH24 = GL_DEPTH_COMPONENT24,   // GL_DEPTH_COMPONENT24
    DEPTH32F = GL_DEPTH_COMPONENT32F,  // GL_DEPTH_COMPONENT32F

    // ---- 深度+模板格式 ---- 3
    DEPTH24_STENCIL8 = GL_DEPTH24_STENCIL8,   // GL_DEPTH24_STENCIL8
    DEPTH32F_STENCIL8 = GL_DEPTH32F_STENCIL8,  // GL_DEPTH32F_STENCIL8
};

/**
 * @brief 多重采样纹理格式枚举
 *
 * 多重采样纹理支持的内部格式，相比普通纹理有所限制
 */
enum class MultisampleTextureFormat {
    // 常规颜色格式
    RGBA8 = GL_RGBA8,               ///< 8位RGBA
    RGBA16F = GL_RGBA16F,           ///< 16位浮点RGBA
    RGBA32F = GL_RGBA32F,           ///< 32位浮点RGBA
    SRGB8_ALPHA8 = GL_SRGB8_ALPHA8, ///< sRGB alpha

    // 单/双通道格式
    R8 = GL_R8,                     ///< 单通道8位
    RG8 = GL_RG8,                   ///< 双通道8位
    R16F = GL_R16F,                 ///< 单通道16位浮点
    RG16F = GL_RG16F,               ///< 双通道16位浮点
    R32F = GL_R32F,                 ///< 单通道32位浮点
    RG32F = GL_RG32F,               ///< 双通道32位浮点

    // 深度/模板格式
    Depth24Stencil8 = GL_DEPTH24_STENCIL8,     ///< 24位深度 + 8位模板
    Depth32FStencil8 = GL_DEPTH32F_STENCIL8,   ///< 32位浮点深度 + 8位模板
    Depth32F = GL_DEPTH_COMPONENT32F,          ///< 32位浮点深度
    Depth24 = GL_DEPTH_COMPONENT24,            ///< 24位深度
    Depth16 = GL_DEPTH_COMPONENT16             ///< 16位深度
};

/**
 * @brief 纹理过滤模式枚举
 * 定义纹理放大/缩小时的采样方式
 * 例:GL_LINEAR_MIPMAP_LINEAR 前者是采样方式，后者是邻近级别纹理/或两个级别纹理之间线性插值
 */
enum class TextureFilter {
    Nearest = GL_NEAREST,                               ///< 最近邻采样（像素化效果）
    Linear = GL_LINEAR,                                 ///< 线性插值（平滑效果）
    NearestMipmapNearest = GL_NEAREST_MIPMAP_NEAREST,   ///< 最近邻 mipmap，最近邻采样
    LinearMipmapNearest = GL_LINEAR_MIPMAP_NEAREST,     ///< 最近邻 mipmap，线性采样
    NearestMipmapLinear = GL_NEAREST_MIPMAP_LINEAR,     ///< 线性 mipmap，最近邻采样
    LinearMipmapLinear = GL_LINEAR_MIPMAP_LINEAR        ///< 三线性过滤（最佳质量）
};

/**
 * @brief 纹理环绕模式枚举
 * 定义纹理坐标超出 [0,1] 范围时的处理方式
 */
enum class TextureWrap {
    Repeat = GL_REPEAT,                 ///< 重复纹理（平铺效果）
    MirroredRepeat = GL_MIRRORED_REPEAT,///< 镜像重复
    ClampToEdge = GL_CLAMP_TO_EDGE,     ///< 钳位到边缘（边缘像素延伸）
    ClampToBorder = GL_CLAMP_TO_BORDER  ///< 钳位到边框（使用固定边框颜色）
};

/**
 * @brief 纹理创建描述符
 * 用于配置纹理的所有创建参数
 */
struct TextureDescriptor {
    TextureType type = TextureType::Texture2D;          ///< 纹理类型（1D/2D/3D/Cube）
    TextureFormat format = TextureFormat::RGBA8;        ///< 纹理内部格式
    int width = 0;                                      ///< 纹理宽度（必须 >0）
    int height = 1;                                     ///< 纹理高度（1D纹理时忽略）
    int depth = 1;                                      ///< 3D纹理深度 或 Cube纹理必须为6

    TextureFilter minFilter = TextureFilter::Linear;  ///< 缩小过滤模式
    TextureFilter magFilter = TextureFilter::Linear;              ///< 放大过滤模式
    TextureWrap wrapS = TextureWrap::Repeat;                      ///< S坐标环绕模式
    TextureWrap wrapT = TextureWrap::Repeat;                      ///< T坐标环绕模式
    TextureWrap wrapR = TextureWrap::Repeat;                      ///< R坐标环绕模式（仅3D纹理）

    bool generateMipmaps = false;                    ///< 是否自动生成mipmap链
    float anisotropyLevel = 1.0f;                   ///< 各向异性过滤级别（1.0=关闭）
};


/**
 * @brief 通用纹理管理类
 * 封装 OpenGL 纹理对象，提供统一的纹理创建、更新、下载和管理接口。
 * 支持 1D、2D、3D 和 Cube 纹理，以及多种格式和过滤模式。
 */
class Texture {
public:
   
    //创建空纹理对象，生成 OpenGL 纹理句柄
    Texture();
    ~Texture();

    explicit Texture(const std::string& name) : m_name(name) { };

    // 禁止拷贝
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // ==================== 创建空纹理 ====================
   /**
    * @brief 根据描述符创建纹理,并分配存储空间（无数据），如果已经创建过了会销毁原有纹理数据，重新分配存储
    * @param desc 纹理描述符，包含所有创建参数
    * @return 成功返回 true，失败返回 false
    *
    * 注意：Cube纹理的 depth 必须为 6，否则创建失败
    */
    bool Create(const TextureDescriptor& desc);

    bool Create(uint32_t width, uint32_t height, const void* data, TextureType type = TextureType::Texture2D, TextureFormat format = TextureFormat::RGBA8);

    // ==================== 更新纹理数据 ====================
    /**
     * @brief 上传完整纹理数据
     * @param data 像素数据指针
     * @param level mipmap级别（0为基础级别）
     * @return 成功返回 true，失败返回 false
     *
     * 对于Cube纹理，data 必须包含全部6个面的数据，按顺序排列
     */
    bool SetData(const void* data, int level = 0);

    //====================== 构造纹理 ======================
    /**
     * @brief 从内存数据创建纹理
     * @param desc 纹理描述符
     * @param data 纹理像素数据（可为 nullptr，此时只分配存储）
     * @return 成功返回 true，失败返回 false
     * 如果已经创建过，会销毁原有纹理数据，重新分配存储
     */
    bool CreateFromData(const TextureDescriptor& desc, const void* data);

    //================= 依据图片文件创建纹理 ===================
    /**
     * @brief 从图片文件创建纹理
     * @param pictureFilePath 图片的路径
     * @param sRGB srgb图片（非线性色彩，本身带有伽马矫正的图片）
     * @return 成功返回 true，失败返回 false
     * 强制以4通道rgba 加载图片数据
     */
    bool Create2dTextureFromPicture(const std::string& pictureFilePath, bool sRGB = false);

    //================= 修改texture的size =====================
    /**
     * @brief 修改纹理的尺寸
     * @param newWidth 纹理宽度
     * @param newHeight 纹理高度
     * @param preserveContent 保留原来的数据，默认不保留，极少数情况下会需要保留
     * @return 成功返回 true，失败返回 false
     */
    bool Resize(int newWidth, int newHeight, bool preserveContent = false);

    //====================== 更新纹理数据 ======================
    /**
    * @brief 更新纹理子区域
    * @param data 像素数据指针
    * @param x X轴起始坐标
    * @param y Y轴起始坐标
    * @param z Z轴起始坐标（3D纹理专用）
    * @param width 更新区域宽度
    * @param height 更新区域高度
    * @param depth 更新区域深度
    * @param level mipmap级别
    * @return 成功返回 true，失败返回 false
    *
    * 注意：不支持Cube纹理，请使用 setCubeFaceSubData
    */
    bool SetSubData(const void* data, int x, int y, int z, int width, int height, int depth, int level = 0);

    // ==================Cube专用：更新单个面======================
     /**
     * @brief 更新Cube纹理的单个面
     * @param face 面索引（0-5，顺序：+X, -X, +Y, -Y, +Z, -Z）
     * @param data 该面的像素数据
     * @param level mipmap级别
     * @return 成功返回 true，失败返回 false
     */
    bool SetCubeFaceData(int face, const void* data, int level = 0);

    /**
     * @brief 更新Cube纹理单个面的子区域
     * @param face 面索引（0-5）
     * @param data 像素数据
     * @param x X轴起始坐标
     * @param y Y轴起始坐标
     * @param width 更新区域宽度
     * @param height 更新区域高度
     * @param level mipmap级别
     * @return 成功返回 true，失败返回 false
     */
    bool SetCubeFaceSubData(int face, const void* data, int x, int y, int width, int height, int level = 0);

    // ==================== 下载纹理数据 ====================
    /**
     * @brief 从GPU下载纹理数据到内存
     * @param level mipmap级别
     * @return 包含纹理数据的字节数组
     *
     * 注意：对于Cube纹理，返回的数据包含全部6个面
     */
    std::vector<uint8_t> Dump(int level = 0);

    bool Dump(void* buffer, int level = 0);

    //=====================把纹理内容下载到图片中=============
    bool DumpToImage(Image& image);

    /**
     * @brief 下载纹理数据到指定的缓冲区
     * @param buffer 目标缓冲区指针
     * @param bufferSize 缓冲区大小（字节）
     * @param level mipmap级别
     * @return 成功返回 true，失败返回 false
     */
    bool DumpToBuffer(void* buffer, size_t bufferSize, int level = 0);

    // ==================== 纹理绑定 ====================
    /**
     * @brief 绑定纹理到指定纹理单元
     * @param unit 纹理单元索引（0 到 GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS-1）
     *
     * 等价于：glActiveTexture(GL_TEXTURE0 + unit); glBindTexture(...)
     */
    void Bind(GLuint unit = 0) const;

    /**
   * @brief 解绑当前纹理
   * 将当前绑定的纹理对象解绑
   */
    void Unbind() const;

    // ==================== Mipmap 操作 ====================
    /**
    * @brief 手动生成 mipmap 链
    * 基于基础级别（level 0）自动生成所有 mipmap 级别
    */
    void GenerateMipmaps();

    // ==================== 纹理参数设置 ====================
    /**
     * @brief 设置纹理过滤模式
     * @param minFilter 缩小过滤模式
     * @param magFilter 放大过滤模式
     */
    void SetFilter(TextureFilter minFilter, TextureFilter magFilter);

    /**
     * @brief 设置纹理环绕模式
     * @param s S坐标环绕模式
     * @param t T坐标环绕模式
     * @param r R坐标环绕模式（仅3D纹理有效）
     */
    void SetWrap(TextureWrap s, TextureWrap t, TextureWrap r = TextureWrap::Repeat);

    /**
     * @brief 设置各向异性过滤级别
     * @param level 各向异性级别（1.0=关闭，最大值取决于硬件，通常16.0）
     *
     * 更高的值可以改善倾斜视角下的纹理清晰度，但会降低性能
     */
    void SetAnisotropy(float level);

    // ==================== 附加到fbo ====================

    // 通用接口（自动根据纹理类型选择）
    bool AttachToFBO(GLenum attachment, GLenum target, int level = 0, int layer = 0) const;
    bool DetachFromFBO(GLenum attachment, GLenum target) const;

    // 1D纹理专用
    bool Attach1DToFBO(GLenum attachment, GLenum target, int level = 0) const;
    bool Detach1DFromFBO(GLenum attachment, GLenum target) const;

    // 2D纹理专用
    bool Attach2DToFBO(GLenum attachment, GLenum target, int level = 0) const;
    bool Detach2DFromFBO(GLenum attachment, GLenum target) const;

    // 3D纹理专用
    bool Attach3DToFBO(GLenum attachment, GLenum target, int level = 0, int layer = 0) const;
    bool Detach3DFromFBO(GLenum attachment, GLenum target) const;

    // Cube Map专用
    bool AttachCubeToFBO(GLenum attachment, GLenum target, int level = 0, int cubeFace = 0) const;
    bool DetachCubeFromFBO(GLenum attachment, GLenum target) const;

    // ============ 便捷接口 ============
    bool AttachAsColor(int colorIndex = 0, int level = 0, int layer = 0) const;
    bool AttachAsDepth(int level = 0, int layer = 0) const;
    bool AttachAsStencil(int level = 0, int layer = 0) const;
    bool AttachAsDepthStencil(int level = 0, int layer = 0) const;

    bool DetachAsColor(int colorIndex = 0) const;
    bool DetachAsDepth() const;
    bool DetachAsStencil() const;
    bool DetachAsDepthStencil() const;

    // ==================== 状态查询 ====================
    bool IsValid() const { return m_textureID != 0 && m_width > 0 && m_height > 0; }  ///< 纹理是否有效
    bool IsCube() const { return m_type == TextureType::TextureCubeMap; }              ///< 是否为立方体贴图
    bool Is3D() const { return m_type == TextureType::Texture3D; }                     ///< 是否为3D纹理
    bool Is1D() const { return m_type == TextureType::Texture1D; }                     ///< 是否为1D纹理

    // ==================== 纹理信息查询 ====================
    GLuint GetHandle() const { return m_textureID; }                    ///< 获取OpenGL纹理句柄
    GLenum GetGLTarget() const;                                         ///< 获取OpenGL纹理目标枚举 （实际和GetType()是一致的）
    TextureType GetType() const { return m_type; }                      ///< 获取纹理类型
    TextureFormat GetFormat() const { return m_format; }                ///< 获取纹理格式
    int GetWidth() const { return m_width; }                            ///< 获取纹理宽度
    int GetHeight() const { return m_height; }                          ///< 获取纹理高度
    int GetDepth() const { return m_depth; }                            ///< 获取纹理深度
    int GetMipLevels() const { return m_mipLevels; }                    ///< 获取mipmap级别数
    glm::ivec3 GetSize() const { return glm::ivec3(m_width, m_height, m_depth); } ///< 获取纹理尺寸（vec3）

    const std::string& GetName() const { return m_name; }

    // ==================== 静态工具函数 ====================
    static GLenum ToGLInternalFormat(TextureFormat format);    ///< 转换纹理格式到OpenGL内部格式
    static GLenum ToGLFormat(TextureFormat format);            ///< 转换纹理格式到OpenGL像素格式
    static GLenum ToGLType(TextureFormat format);              ///< 转换纹理格式到OpenGL数据类型
    static int GetBytesPerPixel(TextureFormat format);         ///< 获取每种格式的每像素字节数
    static bool IsDepthFormat(TextureFormat format);           ///< 判断是否为深度格式
    static bool IstencilFormat(TextureFormat format);         ///< 判断是否包含模板分量
    static bool IsIntegerFormat(TextureFormat format);         ///< 判断是否为整型格式
    static bool IsFloatFormat(TextureFormat format);           ///< 判断是否为浮点格式

    static std::shared_ptr<Texture> Create2dTextureFromFile(const std::string& filename);
    static std::shared_ptr<Texture> CreateCubeTextureFromFiles(const char* name[6], bool enableMipmap = false);

private:
    // ==================== 内部辅助函数 ====================
    void Cleanup();                                                    ///< 清理OpenGL资源
    void ApplyParameters();                                            ///< 应用纹理参数到OpenGL
    int CalculateMipLevels() const;                                    ///< 计算完整mipmap链级别数
    bool ValidateSubRegion(int x, int y, int z, int width, int height, int depth, int level) const;  ///< 验证子区域有效性
    size_t CalculateLevelSize(int level) const;                        ///< 计算指定级别的数据大小

    bool checkFBOStatus(GLenum target) const;

    // ==================== 成员变量 ====================
    std::string m_name;
    GLuint m_textureID = 0;                     ///< OpenGL纹理句柄
    TextureType m_type = TextureType::Texture2D;///< 纹理类型
    TextureFormat m_format = TextureFormat::RGBA8;  ///< 纹理格式
    int m_width = 0;                            ///< 纹理宽度
    int m_height = 0;                           ///< 纹理高度
    int m_depth = 0;                            ///< 纹理深度（3D/Cube专用）
    int m_mipLevels = 0;                        ///< mipmap级别数

    TextureFilter m_minFilter = TextureFilter::LinearMipmapLinear;  ///< 缩小过滤模式
    TextureFilter m_magFilter = TextureFilter::Linear;              ///< 放大过滤模式
    TextureWrap m_wrapS = TextureWrap::Repeat;                      ///< S坐标环绕模式
    TextureWrap m_wrapT = TextureWrap::Repeat;                      ///< T坐标环绕模式
    TextureWrap m_wrapR = TextureWrap::Repeat;                      ///< R坐标环绕模式
    float m_anisotropyLevel = 1.0f;             ///< 各向异性过滤级别
};


/**
 * @brief 多重采样纹理类
 *
 * 专门用于多重采样抗锯齿（MSAA）的纹理，作为FBO的渲染目标。
 * 与普通纹理的区别：
 * - 不能直接从CPU上传数据
 * - 不支持mipmap
 * - 采样方式不同（需要gl_SampleID等）
 * - 必须通过解析（resolve）转换为普通纹理才能用于显示或后处理
 *
 * 使用示例：
 * @code
 * // 创建MSAA颜色纹理
 * MultisampleTexture msaaColor;
 * msaaColor.create(1920, 1080, 4, MultisampleFormat::RGBA8);
 *
 * // 创建MSAA深度纹理
 * MultisampleTexture msaaDepth;
 * msaaDepth.createDepth(1920, 1080, 4);
 *
 * // 附加到FBO
 * GLuint fbo;
 * glGenFramebuffers(1, &fbo);
 * msaaColor.attachToFBO(GL_COLOR_ATTACHMENT0, fbo);
 * msaaDepth.attachToFBO(GL_DEPTH_STENCIL_ATTACHMENT, fbo);
 *
 * // 渲染完成后解析到普通纹理
 * Texture resolveTexture;
 * msaaColor.resolveToTexture(resolveTexture);
 * @endcode
 */
class MultisampleTexture {
public:
    MultisampleTexture();
    ~MultisampleTexture();

    // 禁止拷贝
    MultisampleTexture(const MultisampleTexture&) = delete;
    MultisampleTexture& operator=(const MultisampleTexture&) = delete;

    // ==================== 创建纹理 ====================
    /**
     * @brief 创建多重采样颜色纹理
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param samples 采样点数（通常为2、4、8、16，受硬件限制）
     * @param format 纹理格式
     * @return 成功返回 true，失败返回 false
     * 注意：创建后纹理内容未定义，需要渲染填充
     */
    bool Create(int width, int height, int samples, MultisampleTextureFormat format);

    /*-采样数保持不变
      */
    bool Resize(int newWidth, int newHeight);

    /**
     * @brief 创建多重采样深度/模板纹理（便捷方法）
     * @param width 纹理宽度
     * @param height 纹理高度
     * @param samples 采样点数
     * @return 成功返回 true，失败返回 false
     * 自动使用 Depth24Stencil8 格式
     */
    bool CreateDepth(int width, int height, int samples);

    // ==================== FBO 附件操作 ====================
    /**
     * @brief 将纹理附加到FBO作为指定附件
     * @param attachment 附件类型（GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT, GL_DEPTH_STENCIL_ATTACHMENT）
     * @param fbo 目标FBO句柄
     * @param mipLevel mipmap级别（多重采样纹理通常为0）
     * 使用前需先绑定FBO：glBindFramebuffer(GL_FRAMEBUFFER, fbo)
     */
    void AttachToFBO(GLenum attachment, GLuint fbo) const;

    /**
     * @brief 从FBO分离纹理附件
     * @param attachment 附件类型
     * @param fbo 目标FBO句柄
     */
    void DetachFromFBO(GLenum attachment, GLuint fbo) const;

    // ==================== 解析（Resolve）操作 ====================
    /**
     * @brief 将MSAA纹理解析到普通纹理
     * @param targetTexture 目标普通纹理的句柄
     * @param targetWidth 目标纹理宽度
     * @param targetHeight 目标纹理高度
     * 将多重采样纹理的每个像素的多重采样值平均（或按规则采样）为单个值
     */
    void ResolveToTexture(GLuint targetTexture, int targetWidth, int targetHeight) const;

    /**
     * @brief 将MSAA纹理解析到目标FBO
     * @param targetFBO 目标FBO句柄
     * @param targetWidth 目标宽度
     * @param targetHeight 目标高度
     */
    void ResolveToFramebuffer(GLuint targetFBO, int targetWidth, int targetHeight) const;

    /**
     * @brief 将MSAA纹理直接解析到默认帧缓冲（屏幕）
     * 用于直接显示MSAA渲染结果
     */
    void ResolveToDefaultFramebuffer() const;

    // ==================== 清除操作 ====================
    /**
     * @brief 清除颜色纹理
     * @param color 清除颜色（RGBA）
     */
    void ClearColor(const glm::vec4& color);

    /**
     * @brief 清除深度纹理
     * @param depth 深度值（0.0 ~ 1.0，1.0为最远）
     */
    void ClearDepth(float depth);

    /**
     * @brief 清除模板纹理
     * @param stencil 模板值（0-255）
     */
    void ClearStencil(int stencil);

    /**
     * @brief 清除深度+模板纹理
     * @param depth 深度值
     * @param stencil 模板值
     */
    void ClearDepthStencil(float depth, int stencil);

    // ==================== 信息查询 ====================
    GLuint GetHandle() const { return m_MStextureID; }           ///< 获取OpenGL纹理句柄
    int GetWidth() const { return m_width; }                   ///< 获取纹理宽度
    int GetHeight() const { return m_height; }                 ///< 获取纹理高度
    int GetSamples() const { return m_samples; }               ///< 获取采样点数
    MultisampleTextureFormat GetFormat() const { return m_format; }   ///< 获取纹理格式
    bool IsDepthFormat() const { return m_isDepth; }           ///< 是否为深度格式
    bool IsValid() const { return m_MStextureID != 0 && m_width > 0 && m_height > 0; }  ///< 纹理是否有效

    // ==================== 绑定 ====================
    /**
     * @brief 绑定纹理到指定纹理单元
     * @param unit 纹理单元索引
     * 注意：在着色器中采样MSAA纹理需要使用 sampler2DMS 类型
     * vec4 color = texelFetch(msaaTexture, texCoord, 2);  // 采样子像素2，可以以此手动实现抗锯齿算法
     * 并通过 texelFetch 函数访问，不能使用 texture 函数
     */
    void Bind(GLuint unit = 0) const;
    void Unbind() const;

    // ==================== 静态工具函数 ====================
    static GLenum ToGLInternalFormat(MultisampleTextureFormat format);  ///< 转换到OpenGL内部格式
    static int GetBytesPerPixel(MultisampleTextureFormat format);       ///< 获取每像素字节数
    static bool IsDepthFormat(MultisampleTextureFormat format);         ///< 判断是否为深度格式
    static bool IsStencilFormat(MultisampleTextureFormat format);       ///< 判断是否包含模板

private:
    void Cleanup();

    GLuint m_MStextureID = 0;
    int m_width = 0;
    int m_height = 0;
    int m_samples = 1;
    MultisampleTextureFormat m_format = MultisampleTextureFormat::RGBA8;
    bool m_isDepth = false;
};


NAMESPACE_OPENGL_CORE_END
