
#include "Texture.h"
#include "ogl_defs.h"

NAMESPACE_OPENGL_CORE_BEGIN

#pragma region texture

Texture::Texture() = default;

Texture::~Texture() {
    Cleanup();
}

void Texture::Cleanup() {
    if (m_textureID != 0) {
        GL(glDeleteTextures,1, &m_textureID);
        m_textureID = 0;
    }
}

//获取纹理类别（1d/2d/3d/cube）
GLenum Texture::GetGLTarget() const {
    return static_cast<GLenum>(m_type);
}

int Texture::CalculateMipLevels() const {
    int maxDim = m_width;
    if (m_type != TextureType::Texture1D) {
        maxDim = std::max(maxDim, m_height);
    }
    if (m_type == TextureType::Texture3D) {
        maxDim = std::max(maxDim, m_depth);
    }
    return static_cast<int>(std::log2(maxDim)) + 1;
}

bool Texture::Create(const TextureDescriptor& desc) {
    // 验证Cube纹理的depth必须为6
    if (desc.type == TextureType::TextureCubeMap && desc.depth != 6) {
        auto str = "Texture::create: Cube map depth must be 6, got " + std::to_string(desc.depth);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, str.data());
    }

    Cleanup();
    GL(glGenTextures,1, &m_textureID);

    m_type = desc.type;
    m_format = desc.format;
    m_width = desc.width;
    m_height = desc.height;
    m_depth = desc.depth;
    m_minFilter = desc.minFilter;
    m_magFilter = desc.magFilter;
    m_wrapS = desc.wrapS;
    m_wrapT = desc.wrapT;
    m_wrapR = desc.wrapR;
    m_anisotropyLevel = desc.anisotropyLevel;

    
    if (desc.generateMipmaps) {
        m_mipLevels = CalculateMipLevels();
    }
    else {
        m_mipLevels = 1;
    }

    GL(glBindTexture,GetGLTarget(), m_textureID);

    GLenum internalFormat = ToGLInternalFormat(m_format);

    // 根据纹理类型分配存储
    //glTexStorage2D 是 OpenGL 中用于为二维纹理分配不可变存储空间的函数。它是现代 OpenGL（4.2+）推荐的纹理内存管理方式。
    //一次性分配纹理所需的全部显存空间（包括所有 Mipmap 级别），之后纹理的尺寸、格式、Mipmap 级别数都不能再改变。
    //区别于glTexImage2D 可以动态分配，此tuxture 需要删除重建
    switch (m_type) {
    case TextureType::Texture1D:
        GL(glTexStorage1D,GetGLTarget(), m_mipLevels, internalFormat, m_width);
        break;

    case TextureType::Texture2D:
        GL(glTexStorage2D,GetGLTarget(), m_mipLevels, internalFormat, m_width, m_height);
        break;

    case TextureType::Texture3D:
        GL(glTexStorage3D,GetGLTarget(), m_mipLevels, internalFormat, m_width, m_height, m_depth);
        break;

    case TextureType::TextureCubeMap:
        GL(glTexStorage2D,GetGLTarget(), m_mipLevels, internalFormat, m_width, m_height);
        break;
    }

    ApplyParameters();
    glBindTexture(GetGLTarget(), 0);

    return glGetError() == GL_NO_ERROR;
}

bool Texture::Create(uint32_t width, uint32_t height, const void* data, TextureType type , TextureFormat format) {
    TextureDescriptor newDesc;
    newDesc.type = type;
    newDesc.format = format;
    newDesc.width = width;
    newDesc.height = height;
    newDesc.minFilter = TextureFilter::LinearMipmapLinear;
    newDesc.magFilter = TextureFilter::Linear;
    newDesc.wrapS = TextureWrap::ClampToEdge;
    newDesc.wrapT = TextureWrap::ClampToEdge;
    newDesc.generateMipmaps = true;
    return CreateFromData(newDesc,data);
}

bool Texture::Resize(int newWidth, int newHeight, bool preserveContent) {
    if (newWidth == m_width && newHeight == m_height) {
        return true;  // 尺寸没变
    }

    // 保存旧纹理内容和参数
    GLuint oldTexture = m_textureID;
    int oldWidth = m_width;
    int oldHeight = m_height;

    if (preserveContent && oldTexture != 0) {
        // 创建临时纹理保存旧内容
        Texture tempTexture;
        TextureDescriptor tempDesc;
        tempDesc.type = m_type;
        tempDesc.format = m_format;
        tempDesc.width = oldWidth;
        tempDesc.height = oldHeight;
        tempDesc.generateMipmaps = false;
        tempTexture.Create(tempDesc);

        // 复制旧纹理内容到临时纹理
        GL(glCopyImageSubData,oldTexture, GetGLTarget(), 0, 0, 0, 0, tempTexture.GetHandle(), tempTexture.GetGLTarget(), 0, 0, 0, 0, oldWidth, oldHeight, 1);

        // 重建纹理（新尺寸）
        TextureDescriptor newDesc;
        newDesc.type = m_type;
        newDesc.format = m_format;
        newDesc.width = newWidth;
        newDesc.height = newHeight;
        newDesc.depth = m_depth;
        newDesc.minFilter = m_minFilter;
        newDesc.magFilter = m_magFilter;
        newDesc.wrapS = m_wrapS;
        newDesc.wrapT = m_wrapT;
        newDesc.wrapR = m_wrapR;
        newDesc.anisotropyLevel = m_anisotropyLevel;
        newDesc.generateMipmaps = (m_mipLevels == 0);

        if (!Create(newDesc)) {
            return false;
        }

        // 将旧内容复制回新纹理（尽可能多的区域）
        int copyWidth = std::min(oldWidth, newWidth);
        int copyHeight = std::min(oldHeight, newHeight);

        GL(glCopyImageSubData,tempTexture.GetHandle(), tempTexture.GetGLTarget(), 0, 0, 0, 0, m_textureID, GetGLTarget(), 0, 0, 0, 0, copyWidth, copyHeight, 1);
    }
    else {
        // 不保留内容，直接重建
        TextureDescriptor newDesc;
        newDesc.type = m_type;
        newDesc.format = m_format;
        newDesc.width = newWidth;
        newDesc.height = newHeight;
        newDesc.depth = m_depth;
        newDesc.minFilter = m_minFilter;
        newDesc.magFilter = m_magFilter;
        newDesc.wrapS = m_wrapS;
        newDesc.wrapT = m_wrapT;
        newDesc.wrapR = m_wrapR;
        newDesc.anisotropyLevel = m_anisotropyLevel;
        newDesc.generateMipmaps = (m_mipLevels == 0);

        if (!Create(newDesc)) {
            return false;
        }
    }

    // 删除旧纹理
    if (oldTexture != 0 && oldTexture != m_textureID) {
        GL(glDeleteTextures,1, &oldTexture);
    }

    return true;
}

bool Texture::Create2dTextureFromPicture(const std::string& pictureFilePath, bool sRGB) {

    //强制以4通道加载数据
    auto image = FileUtils::LoadImage(pictureFilePath);
    if (image == nullptr) { return false; }

    // ⭐ 关键：设置内存对齐为 1（因为 stb_image 是紧凑排列）
    GL(glPixelStorei,GL_UNPACK_ALIGNMENT, 1);

    TextureDescriptor desc;
    desc.width = image->width;
    desc.height = image->height;
    desc.type = TextureType::Texture2D;
    desc.format = TextureFormat::RGBA8;
    desc.depth = 1;

    desc.format = sRGB ? TextureFormat::SRGB8_A8 : TextureFormat::RGBA8;

    if (!Create(desc)) {
        return false;
    }

    //使用4通道加载的数据 必须以 GL_RGBA 上传
    GLenum pixelFormat = GL_RGBA;

    GL(glBindTexture,GetGLTarget(), m_textureID);
    //glTexSubImage2D (图片纹理一般是GL_UNSIGNED_BYTE)
    GL(glTexSubImage2D,GetGLTarget(), 0, 0, 0, image->width, image->height, pixelFormat, GL_UNSIGNED_BYTE, image->data);

    // ⭐ 恢复默认对齐（避免影响其他操作）
    GL(glPixelStorei,GL_UNPACK_ALIGNMENT, 4);

    if (desc.generateMipmaps) {
        GL(glGenerateMipmap,GetGLTarget());
    }

    GL(glBindTexture,GetGLTarget(), 0);

    return true;
}

bool Texture::CreateFromData(const TextureDescriptor& desc, const void* data) {
    if (!Create(desc)) {
        return false;
    }
    if (data) {
        return SetData(data);
    }
    return false;
}

bool Texture::SetData(const void* data, int level) {
    if (!data) return false;
    if (level >= m_mipLevels) return false;

    int width = std::max(1, m_width >> level);
    int height = std::max(1, m_height >> level);
    int depth = std::max(1, m_depth >> level);

    GLenum format = ToGLFormat(m_format);
    GLenum type = ToGLType(m_format);

    glBindTexture(GetGLTarget(), m_textureID);

    switch (m_type) {
    case TextureType::Texture1D:
        GL(glTexSubImage1D,GetGLTarget(), level, 0, width, format, type, data);
        break;

    case TextureType::Texture2D:
        GL(glTexSubImage2D,GetGLTarget(), level, 0, 0, width, height, format, type, data);
        break;

    case TextureType::Texture3D:
        GL(glTexSubImage3D,GetGLTarget(), level, 0, 0, 0, width, height, depth, format, type, data);
        break;

    case TextureType::TextureCubeMap:
    {
        // 立方体贴图：数据包含6个面，按顺序：+X, -X, +Y, -Y, +Z, -Z
        size_t faceSize = width * height * GetBytesPerPixel(m_format);
        for (int face = 0; face < 6; ++face) {
            const uint8_t* faceData = static_cast<const uint8_t*>(data) + faceSize * face;
            GL(glTexSubImage2D,GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, 0, 0, width, height, format, type, faceData);
        }
    }
    break;
    }

    GL(glBindTexture,GetGLTarget(), 0);
    return true;
}

bool Texture::SetSubData(const void* data, int x, int y, int z, int width, int height, int depth, int level) {
    if (!ValidateSubRegion(x, y, z, width, height, depth, level)) return false;

    GLenum format = ToGLFormat(m_format);
    GLenum type = ToGLType(m_format);

    GL(glBindTexture,GetGLTarget(), m_textureID);

    switch (m_type) {
    case TextureType::Texture1D:
        GL(glTexSubImage1D,GetGLTarget(), level, x, width, format, type, data);
        break;

    case TextureType::Texture2D:
        GL(glTexSubImage2D,GetGLTarget(), level, x, y, width, height, format, type, data);
        break;

    case TextureType::Texture3D:
        GL(glTexSubImage3D,GetGLTarget(), level, x, y, z, width, height, depth, format, type, data);
        break;

    case TextureType::TextureCubeMap:
        glBindTexture(GetGLTarget(), 0);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setSubData: For cube maps, use setCubeFaceSubData instead");
    }

    GL(glBindTexture,GetGLTarget(), 0);
    return true;
}

bool Texture::SetCubeFaceData(int face, const void* data, int level) {
    if (m_type != TextureType::TextureCubeMap) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setCubeFaceData: Texture is not a cube map!");
    }

    if (face < 0 || face >= 6) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setCubeFaceData: Invalid face index ");
    }

    if (level >= m_mipLevels) return false;

    int width = std::max(1, m_width >> level);
    int height = std::max(1, m_height >> level);
    GLenum format = ToGLFormat(m_format);
    GLenum type = ToGLType(m_format);

    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glTexSubImage2D,GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, 0, 0, width, height, format, type, data);
    GL(glBindTexture,GetGLTarget(), 0);

    return true;
}

bool Texture::SetCubeFaceSubData(int face, const void* data, int x, int y, int width, int height, int level) {
    if (m_type != TextureType::TextureCubeMap) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setCubeFaceSubData: Texture is not a cube map!");
    }

    if (face < 0 || face >= 6) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setCubeFaceSubData: Invalid face index ");
    }

    int levelWidth = std::max(1, m_width >> level);
    int levelHeight = std::max(1, m_height >> level);

    if (x + width > levelWidth || y + height > levelHeight) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "setCubeFaceSubData: Region out of bounds");
    }

    GLenum format = ToGLFormat(m_format);
    GLenum type = ToGLType(m_format);

    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glTexSubImage2D,GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, x, y, width, height, format, type, data);
    GL(glBindTexture,GetGLTarget(), 0);

    return true;
}

std::vector<uint8_t> Texture::Dump(int level) {
    std::vector<uint8_t> data;
    size_t totalSize = CalculateLevelSize(level);

    if (totalSize == 0) return data;

    data.resize(totalSize);
    if (DumpToBuffer(data.data(), totalSize, level)) {
        return data;
    }

    return {};
}

bool Texture::Dump(void* buffer, int level) {
    size_t totalSize = CalculateLevelSize(level);
    if (totalSize == 0) return false;
    if (DumpToBuffer(buffer, totalSize, level)) {
        return true;
    }
    return false;
}

bool  Texture::DumpToImage(Image& image) {
    if (!image.IsAllocated() || image.GetWidth() != GetWidth() || image.GetHeight() != GetHeight() ||
        image.GetNumComponents() != Texture::GetBytesPerPixel(GetFormat())) {
        image.Allocate(GetWidth(), GetHeight(), Texture::GetBytesPerPixel(GetFormat()));
    }
     return Dump(image.GetData());
}

bool Texture::DumpToBuffer(void* buffer, size_t bufferSize, int level) {
    size_t requiredSize = CalculateLevelSize(level);
    if (bufferSize < requiredSize) {
        auto str = "getDataToBuffer: Buffer too small. Need " + std::to_string(requiredSize) + " bytes";
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, str.data());
    }

    GLenum format = ToGLFormat(m_format);
    GLenum type = ToGLType(m_format);

    GL(glBindTexture,GetGLTarget(), m_textureID);
    // 使用像素缓冲区对象（PBO）可以异步读取，这里简化
    GL(glGetTexImage,GetGLTarget(), level, format, type, buffer);
    GL(glBindTexture,GetGLTarget(), 0);

    return true;
}

size_t Texture::CalculateLevelSize(int level) const {
    int width = std::max(1, m_width >> level);
    int height = std::max(1, m_height >> level);
    int depth = std::max(1, m_depth >> level);

    size_t pixelCount = width * height * depth;

    // 立方体贴图有6个面
    if (m_type == TextureType::TextureCubeMap) {
        pixelCount *= 6;
    }

    return pixelCount * GetBytesPerPixel(m_format);
}

void Texture::Bind(GLuint unit) const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::bind: Texture is not valid!");
    }
    GL(glActiveTexture,GL_TEXTURE0 + unit);
    GL(glBindTexture,GetGLTarget(), m_textureID);
}

void Texture::Unbind() const {
    GL(glBindTexture,GetGLTarget(), 0);
}

void Texture::ApplyParameters() {
    // 过滤模式
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_MIN_FILTER, static_cast<GLint>(m_minFilter));
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_MAG_FILTER, static_cast<GLint>(m_magFilter));

    // 环绕模式
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_S, static_cast<GLint>(m_wrapS));
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_T, static_cast<GLint>(m_wrapT));

    if (m_type == TextureType::Texture3D) {
        GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_R, static_cast<GLint>(m_wrapR));
    }

    // 各向异性过滤
    if (m_anisotropyLevel > 1.0f) {
        GLfloat maxAniso;
        GL(glGetFloatv,GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        GL(glTexParameterf,GetGLTarget(), GL_TEXTURE_MAX_ANISOTROPY, std::min(m_anisotropyLevel, maxAniso));
    }
}

void Texture::GenerateMipmaps() {
    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glGenerateMipmap,GetGLTarget());
    GL(glBindTexture,GetGLTarget(), 0);
}

void Texture::SetFilter(TextureFilter minFilter, TextureFilter magFilter) {
    m_minFilter = minFilter;
    m_magFilter = magFilter;
    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_MIN_FILTER, static_cast<GLint>(minFilter));
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_MAG_FILTER, static_cast<GLint>(magFilter));
    GL(glBindTexture,GetGLTarget(), 0);
}

void Texture::SetWrap(TextureWrap s, TextureWrap t, TextureWrap r) {
    m_wrapS = s;
    m_wrapT = t;
    m_wrapR = r;
    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_S, static_cast<GLint>(s));
    GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_T, static_cast<GLint>(t));
    if (m_type == TextureType::Texture3D) {
        GL(glTexParameteri,GetGLTarget(), GL_TEXTURE_WRAP_R, static_cast<GLint>(r));
    }
    GL(glBindTexture,GetGLTarget(), 0);
}

void Texture::SetAnisotropy(float level) {
    m_anisotropyLevel = level;
    GLfloat maxAniso;
    GL(glGetFloatv,GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
    GL(glBindTexture,GetGLTarget(), m_textureID);
    GL(glTexParameterf,GetGLTarget(), GL_TEXTURE_MAX_ANISOTROPY, std::min(level, maxAniso));
    GL(glBindTexture,GetGLTarget(), 0);
}

bool Texture::ValidateSubRegion(int x, int y, int z, int width, int height, int depth, int level) const {
    int levelWidth = std::max(1, m_width >> level);
    int levelHeight = std::max(1, m_height >> level);
    int levelDepth = std::max(1, m_depth >> level);

    bool valid = (x >= 0 && y >= 0 && z >= 0 &&
        x + width <= levelWidth &&
        y + height <= levelHeight &&
        z + depth <= levelDepth);

    if (!valid) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "validateSubRegion: Region out of bounds!");
    }

    return valid;
}

// ============ 通用 Attach/Detach ============

bool Texture::checkFBOStatus(GLenum target) const {
    bool flag = (glCheckFramebufferStatus(target) == GL_FRAMEBUFFER_COMPLETE);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(flag, false, "validate FBO status");
    return true;
}

bool Texture::AttachToFBO(GLenum attachment, GLenum target, int level, int layer) const {
    if (m_textureID == 0) return false;

    switch (m_type) {
    case TextureType::Texture1D:
        return Attach1DToFBO(attachment, target, level);
    case TextureType::Texture2D:
        return Attach2DToFBO(attachment, target, level);
    case TextureType::Texture3D:
        return Attach3DToFBO(attachment, target, level, layer);
    case TextureType::TextureCubeMap:
        return AttachCubeToFBO(attachment, target, level, layer);
    }
    return false;
}

bool Texture::DetachFromFBO(GLenum attachment, GLenum target) const {
    if (m_textureID == 0) return false;
    GL(glFramebufferTexture,target, attachment, 0, 0);
    return true;
}

// ============ 1D Texture ============

bool Texture::Attach1DToFBO(GLenum attachment, GLenum target, int level) const {
    if (m_textureID == 0 || m_type != TextureType::Texture1D) return false;

    GL(glFramebufferTexture1D,target, attachment, GL_TEXTURE_1D, m_textureID, level);
    return checkFBOStatus(target);
}

bool Texture::Detach1DFromFBO(GLenum attachment, GLenum target) const {
    GL(glFramebufferTexture1D,target, attachment, GL_TEXTURE_1D, 0, 0);
    return true;
}

// ============ 2D Texture ============

bool Texture::Attach2DToFBO(GLenum attachment, GLenum target, int level) const {
    if (m_textureID == 0 || m_type != TextureType::Texture2D) return false;

    GL(glFramebufferTexture2D,target, attachment, GL_TEXTURE_2D, m_textureID, level);
    return checkFBOStatus(target);
}

bool Texture::Detach2DFromFBO(GLenum attachment, GLenum target) const {
    GL(glFramebufferTexture2D,target, attachment, GL_TEXTURE_2D, 0, 0);
    return true;
}

// ============ 3D Texture ============

bool Texture::Attach3DToFBO(GLenum attachment, GLenum target, int level, int layer) const {
    if (m_textureID == 0 || m_type != TextureType::Texture3D) return false;

    // 使用现代API
    GL(glFramebufferTextureLayer,target, attachment, m_textureID, level, layer);
    return checkFBOStatus(target);
}

bool Texture::Detach3DFromFBO(GLenum attachment, GLenum target) const {
    GL(glFramebufferTextureLayer,target, attachment, 0, 0, 0);
    return true;
}

// ============ Cube Map ============

bool Texture::AttachCubeToFBO(GLenum attachment, GLenum target, int level, int cubeFace) const {
    if (m_textureID == 0 || m_type != TextureType::TextureCubeMap) return false;
    if (cubeFace < 0 || cubeFace >= 6) return false;

    static const GLenum cubeTargets[] = {
        GL_TEXTURE_CUBE_MAP_POSITIVE_X,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
        GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
        GL_TEXTURE_CUBE_MAP_NEGATIVE_Z
    };
    GLenum cubeTarget = cubeTargets[cubeFace];
    GL(glFramebufferTexture2D,target, attachment, cubeTarget, m_textureID, level);
    return checkFBOStatus(target);
}

bool Texture::DetachCubeFromFBO(GLenum attachment, GLenum target) const {
    GL(glFramebufferTexture2D,target, attachment, GL_TEXTURE_CUBE_MAP_POSITIVE_X, 0, 0);
    return true;
}

// ============ 便捷接口 ============

bool Texture::AttachAsColor(int colorIndex, int level, int layer) const {
    return AttachToFBO(GL_COLOR_ATTACHMENT0 + colorIndex, GL_FRAMEBUFFER, level, layer);
}

bool Texture::AttachAsDepth(int level, int layer) const {
    return AttachToFBO(GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER, level, layer);
}

bool Texture::AttachAsStencil(int level, int layer) const {
    return AttachToFBO(GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER, level, layer);
}

bool Texture::AttachAsDepthStencil(int level, int layer) const {
    return AttachToFBO(GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER, level, layer);
}

bool Texture::DetachAsColor(int colorIndex) const {
    return DetachFromFBO(GL_COLOR_ATTACHMENT0 + colorIndex, GL_FRAMEBUFFER);
}

bool Texture::DetachAsDepth() const {
    return DetachFromFBO(GL_DEPTH_ATTACHMENT, GL_FRAMEBUFFER);
}

bool Texture::DetachAsStencil() const {
    return DetachFromFBO(GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER);
}

bool Texture::DetachAsDepthStencil() const {
    return DetachFromFBO(GL_DEPTH_STENCIL_ATTACHMENT, GL_FRAMEBUFFER);
}

// ============ 静态工具函数实现 ============

GLenum Texture::ToGLInternalFormat(TextureFormat format) {
    return static_cast<GLenum>(format);
}

GLenum Texture::ToGLFormat(TextureFormat format)
{
    switch (format)
    {
        // ================================================================
        // 单通道 - 浮点/归一化
        // ================================================================
    case TextureFormat::R8:
    case TextureFormat::R16:
    case TextureFormat::R16F:
    case TextureFormat::R32F:
        return GL_RED;

        // ================================================================
        // 单通道 - 无符号整数
        // ================================================================
    case TextureFormat::R8UI:
    case TextureFormat::R16UI:
    case TextureFormat::R32UI:
        return GL_RED_INTEGER;

        // ================================================================
        // 单通道 - 有符号整数
        // ================================================================
    case TextureFormat::R8I:
    case TextureFormat::R16I:
    case TextureFormat::R32I:
        return GL_RED_INTEGER;

        // ================================================================
        // 双通道 - 浮点/归一化
        // ================================================================
    case TextureFormat::RG8:
    case TextureFormat::RG16:
    case TextureFormat::RG16F:
    case TextureFormat::RG32F:
        return GL_RG;

        // ================================================================
        // 双通道 - 无符号整数
        // ================================================================
    case TextureFormat::RG8UI:
    case TextureFormat::RG16UI:
    case TextureFormat::RG32UI:
        return GL_RG_INTEGER;

        // ================================================================
        // 双通道 - 有符号整数
        // ================================================================
    case TextureFormat::RG8I:
    case TextureFormat::RG16I:
    case TextureFormat::RG32I:
        return GL_RG_INTEGER;

        // ================================================================
        // 三通道 - 浮点/归一化
        // ================================================================
    case TextureFormat::RGB8:
    case TextureFormat::RGB16:
    case TextureFormat::RGB16F:
    case TextureFormat::RGB32F:
    case TextureFormat::SRGB8:
        return GL_RGB;

        // ================================================================
        // 三通道 - 无符号整数
        // ================================================================
    case TextureFormat::RGB8UI:
    case TextureFormat::RGB16UI:
    case TextureFormat::RGB32UI:
        return GL_RGB_INTEGER;

        // ================================================================
        // 三通道 - 有符号整数
        // ================================================================
    case TextureFormat::RGB8I:
    case TextureFormat::RGB16I:
    case TextureFormat::RGB32I:
        return GL_RGB_INTEGER;

        // ================================================================
        // 四通道 - 浮点/归一化
        // ================================================================
    case TextureFormat::RGBA8:
    case TextureFormat::RGBA16:
    case TextureFormat::RGBA16F:
    case TextureFormat::RGBA32F:
    case TextureFormat::SRGB8_A8:
        return GL_RGBA;

        // ================================================================
        // 四通道 - 无符号整数
        // ================================================================
    case TextureFormat::RGBA8UI:
    case TextureFormat::RGBA16UI:
    case TextureFormat::RGBA32UI:
        return GL_RGBA_INTEGER;

        // ================================================================
        // 四通道 - 有符号整数
        // ================================================================
    case TextureFormat::RGBA8I:
    case TextureFormat::RGBA16I:
    case TextureFormat::RGBA32I:
        return GL_RGBA_INTEGER;

        // ================================================================
        // 深度
        // ================================================================
    case TextureFormat::DEPTH16:
    case TextureFormat::DEPTH24:
    case TextureFormat::DEPTH32F:
        return GL_DEPTH_COMPONENT;

        // ================================================================
        // 深度+模板
        // ================================================================
    case TextureFormat::DEPTH24_STENCIL8:
    case TextureFormat::DEPTH32F_STENCIL8:
        return GL_DEPTH_STENCIL;

    default:
        return 0;
    }
}

GLenum Texture::ToGLType(TextureFormat format)
{
    switch (format)
    {
        // ================================================================
        // 无符号字节 - 浮点/归一化 8位
        // ================================================================
    case TextureFormat::R8:
    case TextureFormat::RG8:
    case TextureFormat::RGB8:
    case TextureFormat::RGBA8:
    case TextureFormat::SRGB8:
    case TextureFormat::SRGB8_A8:
        return GL_UNSIGNED_BYTE;

        // ================================================================
        // 无符号字节 - 无符号整数 8位
        // ================================================================
    case TextureFormat::R8UI:
    case TextureFormat::RG8UI:
    case TextureFormat::RGB8UI:
    case TextureFormat::RGBA8UI:
        return GL_UNSIGNED_BYTE;

        // ================================================================
        // 有符号字节 - 有符号整数 8位
        // ================================================================
    case TextureFormat::R8I:
    case TextureFormat::RG8I:
    case TextureFormat::RGB8I:
    case TextureFormat::RGBA8I:
        return GL_BYTE;

        // ================================================================
        // 无符号短整型 - 浮点/归一化 16位
        // ================================================================
    case TextureFormat::R16:
    case TextureFormat::RG16:
    case TextureFormat::RGB16:
    case TextureFormat::RGBA16:
        return GL_UNSIGNED_SHORT;

        // ================================================================
        // 无符号短整型 - 无符号整数 16位
        // ================================================================
    case TextureFormat::R16UI:
    case TextureFormat::RG16UI:
    case TextureFormat::RGB16UI:
    case TextureFormat::RGBA16UI:
        return GL_UNSIGNED_SHORT;

        // ================================================================
        // 有符号短整型 - 有符号整数 16位
        // ================================================================
    case TextureFormat::R16I:
    case TextureFormat::RG16I:
    case TextureFormat::RGB16I:
    case TextureFormat::RGBA16I:
        return GL_SHORT;

        // ================================================================
        // 无符号整型 - 无符号整数 32位
        // ================================================================
    case TextureFormat::R32UI:
    case TextureFormat::RG32UI:
    case TextureFormat::RGB32UI:
    case TextureFormat::RGBA32UI:
        return GL_UNSIGNED_INT;

        // ================================================================
        // 有符号整型 - 有符号整数 32位
        // ================================================================
    case TextureFormat::R32I:
    case TextureFormat::RG32I:
    case TextureFormat::RGB32I:
    case TextureFormat::RGBA32I:
        return GL_INT;

        // ================================================================
        // 半精度浮点 - 浮点 16位
        // ================================================================
    case TextureFormat::R16F:
    case TextureFormat::RG16F:
    case TextureFormat::RGB16F:
    case TextureFormat::RGBA16F:
        return GL_HALF_FLOAT;

        // ================================================================
        // 单精度浮点 - 浮点 32位
        // ================================================================
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return GL_FLOAT;

        // ================================================================
        // 深度 16位
        // ================================================================
    case TextureFormat::DEPTH16:
        return GL_UNSIGNED_SHORT;

        // ================================================================
        // 深度 24位
        // ================================================================
    case TextureFormat::DEPTH24:
        return GL_UNSIGNED_INT;

        // ================================================================
        // 深度 32位浮点
        // ================================================================
    case TextureFormat::DEPTH32F:
        return GL_FLOAT;

        // ================================================================
        // 深度24+模板8
        // ================================================================
    case TextureFormat::DEPTH24_STENCIL8:
        return GL_UNSIGNED_INT_24_8;

        // ================================================================
        // 深度32F+模板8
        // ================================================================
    case TextureFormat::DEPTH32F_STENCIL8:
        return GL_FLOAT_32_UNSIGNED_INT_24_8_REV;

    default:
        return 0;
    }
}

int Texture::GetBytesPerPixel(TextureFormat format)
{
    switch (format)
    {
        // ================================================================
        // 1 字节 - 单通道 8位
        // ================================================================
    case TextureFormat::R8:
    case TextureFormat::R8UI:
    case TextureFormat::R8I:
        return 1;

        // ================================================================
        // 2 字节 - 双通道 8位 / 单通道 16位
        // ================================================================
    case TextureFormat::RG8:
    case TextureFormat::RG8UI:
    case TextureFormat::RG8I:
    case TextureFormat::R16:
    case TextureFormat::R16F:
    case TextureFormat::R16UI:
    case TextureFormat::R16I:
        return 2;

        // ================================================================
        // 2 字节 - 深度 16位
        // ================================================================
    case TextureFormat::DEPTH16:
        return 2;

        // ================================================================
        // 3 字节 - 三通道 8位
        // ================================================================
    case TextureFormat::RGB8:
    case TextureFormat::SRGB8:
    case TextureFormat::RGB8UI:
    case TextureFormat::RGB8I:
        return 3;

        // ================================================================
        // 4 字节 - 四通道 8位 / 双通道 16位 / 单通道 32位
        // ================================================================
    case TextureFormat::RGBA8:
    case TextureFormat::SRGB8_A8:
    case TextureFormat::RGBA8UI:
    case TextureFormat::RGBA8I:
    case TextureFormat::RG16:
    case TextureFormat::RG16F:
    case TextureFormat::RG16UI:
    case TextureFormat::RG16I:
    case TextureFormat::R32F:
    case TextureFormat::R32UI:
    case TextureFormat::R32I:
        return 4;

        // ================================================================
        // 4 字节 - 深度 24位 / 深度 32位 / 深度24+模板8
        // ================================================================
    case TextureFormat::DEPTH24:
    case TextureFormat::DEPTH32F:
    case TextureFormat::DEPTH24_STENCIL8:
        return 4;

        // ================================================================
        // 6 字节 - 三通道 16位
        // ================================================================
    case TextureFormat::RGB16:
    case TextureFormat::RGB16F:
    case TextureFormat::RGB16UI:
    case TextureFormat::RGB16I:
        return 6;

        // ================================================================
        // 8 字节 - 四通道 16位 / 双通道 32位
        // ================================================================
    case TextureFormat::RGBA16:
    case TextureFormat::RGBA16F:
    case TextureFormat::RGBA16UI:
    case TextureFormat::RGBA16I:
    case TextureFormat::RG32F:
    case TextureFormat::RG32UI:
    case TextureFormat::RG32I:
        return 8;

        // ================================================================
        // 8 字节 - 深度32F+模板8
        // ================================================================
    case TextureFormat::DEPTH32F_STENCIL8:
        return 8;

        // ================================================================
        // 12 字节 - 三通道 32位
        // ================================================================
    case TextureFormat::RGB32F:
    case TextureFormat::RGB32UI:
    case TextureFormat::RGB32I:
        return 12;

        // ================================================================
        // 16 字节 - 四通道 32位
        // ================================================================
    case TextureFormat::RGBA32F:
    case TextureFormat::RGBA32UI:
    case TextureFormat::RGBA32I:
        return 16;

    default:
        return 0;
    }
}

bool Texture::IsDepthFormat(TextureFormat format) {
    return format == TextureFormat::DEPTH16 ||
        format == TextureFormat::DEPTH24 ||
        format == TextureFormat::DEPTH32F ||
        format == TextureFormat::DEPTH24_STENCIL8 ||
        format == TextureFormat::DEPTH32F_STENCIL8;
}

bool Texture::IstencilFormat(TextureFormat format) {
    return format == TextureFormat::DEPTH24_STENCIL8 ||
        format == TextureFormat::DEPTH32F_STENCIL8;
}

bool Texture::IsIntegerFormat(TextureFormat format)
{
    switch (format)
    {
        // ================================================================
        // 无符号整数 - 8位
        // ================================================================
    case TextureFormat::R8UI:
    case TextureFormat::RG8UI:
    case TextureFormat::RGB8UI:
    case TextureFormat::RGBA8UI:
        return true;

        // ================================================================
        // 无符号整数 - 16位
        // ================================================================
    case TextureFormat::R16UI:
    case TextureFormat::RG16UI:
    case TextureFormat::RGB16UI:
    case TextureFormat::RGBA16UI:
        return true;

        // ================================================================
        // 无符号整数 - 32位
        // ================================================================
    case TextureFormat::R32UI:
    case TextureFormat::RG32UI:
    case TextureFormat::RGB32UI:
    case TextureFormat::RGBA32UI:
        return true;

        // ================================================================
        // 有符号整数 - 8位
        // ================================================================
    case TextureFormat::R8I:
    case TextureFormat::RG8I:
    case TextureFormat::RGB8I:
    case TextureFormat::RGBA8I:
        return true;

        // ================================================================
        // 有符号整数 - 16位
        // ================================================================
    case TextureFormat::R16I:
    case TextureFormat::RG16I:
    case TextureFormat::RGB16I:
    case TextureFormat::RGBA16I:
        return true;

        // ================================================================
        // 有符号整数 - 32位
        // ================================================================
    case TextureFormat::R32I:
    case TextureFormat::RG32I:
    case TextureFormat::RGB32I:
    case TextureFormat::RGBA32I:
        return true;

    default:
        return false;
    }
}

bool Texture::IsFloatFormat(TextureFormat format)
{
    switch (format)
    {
        // ================================================================
        // 半精度浮点 - 16位
        // ================================================================
    case TextureFormat::R16F:
    case TextureFormat::RG16F:
    case TextureFormat::RGB16F:
    case TextureFormat::RGBA16F:
        return true;

        // ================================================================
        // 单精度浮点 - 32位
        // ================================================================
    case TextureFormat::R32F:
    case TextureFormat::RG32F:
    case TextureFormat::RGB32F:
    case TextureFormat::RGBA32F:
        return true;

        // ================================================================
        // 深度浮点
        // ================================================================
    case TextureFormat::DEPTH32F:
    case TextureFormat::DEPTH32F_STENCIL8:
        return true;

    default:
        return false;
    }
}

std::shared_ptr<Texture> Texture::Create2dTextureFromFile(const std::string& filename) {
    auto texture = std::make_shared<Texture>();
    texture->Create2dTextureFromPicture(filename,false);
    return texture;
}

std::shared_ptr<Texture> Texture::CreateCubeTextureFromFiles(const char* name[6], bool enableMipmap) {
    auto image0 = FileUtils::LoadImage(name[0]);
    auto image1 = FileUtils::LoadImage(name[1]);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(image0->width == image1->width && image0->height == image1->height, nullptr, "validate cube texture1 size!");
    auto image2 = FileUtils::LoadImage(name[2]);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(image0->width == image2->width && image0->height == image2->height, nullptr, "validate cube texture2 size!");
    auto image3 = FileUtils::LoadImage(name[3]);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(image0->width == image3->width && image0->height == image3->height, nullptr, "validate cube texture3 size!");
    auto image4 = FileUtils::LoadImage(name[4]);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(image0->width == image4->width && image0->height == image4->height, nullptr, "validate cube texture4 size!");
    auto image5 = FileUtils::LoadImage(name[5]);
    VI_ASSERT_LOG_ERROR_RETURN_VALUE(image0->width == image5->width && image0->height == image5->height, nullptr, "validate cube texture5 size!");

    const void* data[6] = { image0->data, image1->data, image2->data, image3->data, image4->data, image5->data };
    // 创建 Cube 纹理
    auto cubeTex = std::make_shared<Texture>();

    TextureDescriptor desc;
    desc.type = TextureType::TextureCubeMap;
    desc.format = TextureFormat::RGBA8;
    desc.width = image0->width;
    desc.height = image0->height;
    desc.depth = 6;           // 必须为6，create() 函数会验证
    desc.minFilter = TextureFilter::LinearMipmapLinear;
    desc.magFilter = TextureFilter::Linear;
    desc.generateMipmaps = true;  // 或手动调用 generateMipmaps()
    desc.anisotropyLevel = 4.f;

    cubeTex->Create(desc);

    // 上传各个面的数据
    for (int face = 0; face < 6; ++face) {
        cubeTex->SetCubeFaceData(face, data[face]);
    }
    if (enableMipmap)cubeTex->GenerateMipmaps();
    return cubeTex;
}

#pragma endregion



#pragma region multiSampleTexture

MultisampleTexture::MultisampleTexture() = default;

MultisampleTexture::~MultisampleTexture() {
    Cleanup();
}

void MultisampleTexture::Cleanup() {
    if (m_MStextureID != 0) {
        GL(glDeleteTextures,1, &m_MStextureID);
        m_MStextureID = 0;
    }
}

bool MultisampleTexture::Create(int width, int height, int samples, MultisampleTextureFormat format) {
    // 验证采样数（通常应为2、4、8、16，但具体取决于硬件）
    GLint maxSamples;
    GL(glGetIntegerv,GL_MAX_SAMPLES, &maxSamples);

    if (samples > maxSamples) {
        auto str = "MultisampleTexture::create: Requested samples " + std::to_string(samples) + " exceeds maximum " + std::to_string(maxSamples);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, str.data());
        samples = maxSamples;
    }

    if (samples < 1) samples = 1;

    Cleanup();
    glGenTextures(1, &m_MStextureID);

    m_width = width;
    m_height = height;
    m_samples = samples;
    m_format = format;
    m_isDepth = IsDepthFormat(format);

    GL(glBindTexture,GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID);

    // 创建多重采样纹理存储
    GLenum internalFormat = ToGLInternalFormat(m_format);
    GL(glTexImage2DMultisample,GL_TEXTURE_2D_MULTISAMPLE, m_samples, internalFormat, m_width, m_height, GL_TRUE);

    // 设置多重采样纹理的固定参数(和mipmap有关，msaa 不支持mipmap)
    GL(glTexParameteri,GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_BASE_LEVEL, 0);
    GL(glTexParameteri,GL_TEXTURE_2D_MULTISAMPLE, GL_TEXTURE_MAX_LEVEL, 0);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        GL(glBindTexture,GL_TEXTURE_2D_MULTISAMPLE, 0);
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "MultisampleTexture::create: OpenGL error");
    }

    GL(glBindTexture,GL_TEXTURE_2D_MULTISAMPLE, 0);
    return true;
}

bool MultisampleTexture::Resize(int newWidth, int newHeight) {
    if (newWidth <= 0 || newHeight <= 0) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, false, "MultisampleTexture::resize: Invalid dimensions ");
    }

    // 不保留数据，直接重建
    if (!Create(newWidth, newHeight, m_samples, m_format)) {
        return false;
    }
    return true;
}

bool MultisampleTexture::CreateDepth(int width, int height, int samples) {
    return Create(width, height, samples, MultisampleTextureFormat::Depth24Stencil8);
}

void MultisampleTexture::AttachToFBO(GLenum attachment, GLuint fbo) const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::attachToFBO: Texture is not valid!");
    }

    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 检查帧缓冲完整性
    GLenum status = GL(glCheckFramebufferStatus,GL_FRAMEBUFFER);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);

    if (status != GL_FRAMEBUFFER_COMPLETE) {
        switch (status) {
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::attachToFBO: Framebuffer incomplete! Status: GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT ");
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::attachToFBO: Framebuffer incomplete! Status: GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT ");
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::attachToFBO: Framebuffer incomplete! Status: GL_FRAMEBUFFER_UNSUPPORTED ");
            break;
        default:
            VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::attachToFBO: Framebuffer incomplete! Status: Unknown error ");
            break;
        }
    }
}

void MultisampleTexture::DetachFromFBO(GLenum attachment, GLuint fbo) const {
    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_FRAMEBUFFER, attachment, GL_TEXTURE_2D_MULTISAMPLE, 0, 0);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);
}

void MultisampleTexture::ResolveToTexture(GLuint targetTexture, int targetWidth, int targetHeight) const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::resolveToTexture: Source texture is not valid!");
    }
    // 创建两个 FBO
    GLuint readFbo, drawFbo;
    GL(glGenFramebuffers,1, &readFbo);
    GL(glGenFramebuffers,1, &drawFbo);

    // 设置源 FBO（附加多采样纹理）
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, readFbo);
    GL(glFramebufferTexture2D,GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置目标 FBO（附加普通纹理）
    GL(glBindFramebuffer,GL_DRAW_FRAMEBUFFER, drawFbo);
    GL(glFramebufferTexture2D,GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,  GL_TEXTURE_2D, targetTexture, 0);

    // 执行解析(多采样纹理只能用GL_NEAREST)
    GL(glBlitFramebuffer,0, 0, m_width, m_height, 0, 0, targetWidth, targetHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 清理
    GL(glDeleteFramebuffers,1, &readFbo);
    GL(glDeleteFramebuffers,1, &drawFbo);
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, 0);
    GL(glBindFramebuffer,GL_DRAW_FRAMEBUFFER, 0);

}

void MultisampleTexture::ResolveToFramebuffer(GLuint targetFBO, int targetWidth, int targetHeight) const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::resolveToFramebuffer: Source texture is not valid!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);

    // 设置源FBO
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置目标FBO
    GL(glBindFramebuffer,GL_DRAW_FRAMEBUFFER, targetFBO);

    // 执行解析
    GL(glBlitFramebuffer,0, 0, m_width, m_height, 0, 0, targetWidth, targetHeight, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, 0);
}

void MultisampleTexture::ResolveToDefaultFramebuffer() const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::resolveToDefaultFramebuffer: Texture is not valid!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);

    // 设置源FBO
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置目标为默认帧缓冲
    GL(glBindFramebuffer,GL_DRAW_FRAMEBUFFER, 0);

    // 执行解析到屏幕
    GL(glBlitFramebuffer,0, 0, m_width, m_height, 0, 0, m_width, m_height, GL_COLOR_BUFFER_BIT, GL_NEAREST);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_READ_FRAMEBUFFER, 0);
}

void MultisampleTexture::ClearColor(const glm::vec4& color) {
    if (!IsValid() || m_isDepth) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clear: Cannot clear non-color or invalid texture!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置清除颜色
    GL(glClearColor,color.r, color.g, color.b, color.a);

    // 执行清除
    GL(glClear,GL_COLOR_BUFFER_BIT);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);
}

void MultisampleTexture::ClearDepth(float depth) {
    if (!IsValid() || !m_isDepth) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clearDepth: Texture is not a depth texture!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);

    // 根据格式决定附加类型
    if (m_format == MultisampleTextureFormat::Depth24Stencil8 ||  m_format == MultisampleTextureFormat::Depth32FStencil8) {
        GL(glFramebufferTexture2D,GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,  GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);
    }
    else {
        GL(glFramebufferTexture2D,GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);
    }

    // 设置深度清除值
    GL(glClearDepth,depth);

    // 执行清除
    GL(glClear,GL_DEPTH_BUFFER_BIT);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);
}

void MultisampleTexture::ClearStencil(int stencil) {
    if (!IsValid() || !m_isDepth) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clearStencil: Texture is not a depth-stencil texture!");
    }

    if (m_format != MultisampleTextureFormat::Depth24Stencil8 &&  m_format != MultisampleTextureFormat::Depth32FStencil8) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clearStencil: Format does not have stencil component!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT,  GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置模板清除值
    GL(glClearStencil,stencil);

    // 执行清除（只清除模板部分）
    GL(glClear,GL_STENCIL_BUFFER_BIT);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);
}

void MultisampleTexture::ClearDepthStencil(float depth, int stencil) {
    if (!IsValid() || !m_isDepth) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clearDepthStencil: Texture is not a depth-stencil texture!");
    }

    if (m_format != MultisampleTextureFormat::Depth24Stencil8 && m_format != MultisampleTextureFormat::Depth32FStencil8) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::clearDepthStencil: Format does not have stencil component!");
    }

    GLuint fbo;
    GL(glGenFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, fbo);
    GL(glFramebufferTexture2D,GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID, 0);

    // 设置清除值
    GL(glClearDepth,depth);
    GL(glClearStencil,stencil);

    // 执行清除
    GL(glClear,GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    // 清理
    GL(glDeleteFramebuffers,1, &fbo);
    GL(glBindFramebuffer,GL_FRAMEBUFFER, 0);
}

void MultisampleTexture::Bind(GLuint unit) const {
    if (!IsValid()) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "MultisampleTexture::bind: Texture is not valid!");
    }

    GL(glActiveTexture,GL_TEXTURE0 + unit);
    GL(glBindTexture,GL_TEXTURE_2D_MULTISAMPLE, m_MStextureID);
}

void MultisampleTexture::Unbind() const {
    GL(glBindTexture,GL_TEXTURE_2D_MULTISAMPLE, 0);
}

// ==================== 静态工具函数实现 ====================

GLenum MultisampleTexture::ToGLInternalFormat(MultisampleTextureFormat format) {
    return static_cast<GLenum>(format);
}

int MultisampleTexture::GetBytesPerPixel(MultisampleTextureFormat format) {
    switch (format) {
    case MultisampleTextureFormat::R8:
        return 1;
    case MultisampleTextureFormat::RG8:
        return 2;
    case MultisampleTextureFormat::RGBA8:
    case MultisampleTextureFormat::SRGB8_ALPHA8:
    case MultisampleTextureFormat::R16F:
    case MultisampleTextureFormat::RG16F:
    case MultisampleTextureFormat::R32F:
    case MultisampleTextureFormat::Depth32F:
        return 4;
    case MultisampleTextureFormat::RGBA16F:
        return 8;
    case MultisampleTextureFormat::RGBA32F:
    case MultisampleTextureFormat::RG32F:
        return 16;
    case MultisampleTextureFormat::Depth24Stencil8:
    case MultisampleTextureFormat::Depth32FStencil8:
        return 4;
    case MultisampleTextureFormat::Depth24:
        return 3;
    case MultisampleTextureFormat::Depth16:
        return 2;
    default:
        return 4;
    }
}

bool MultisampleTexture::IsDepthFormat(MultisampleTextureFormat format) {
    return format == MultisampleTextureFormat::Depth16 ||
        format == MultisampleTextureFormat::Depth24 ||
        format == MultisampleTextureFormat::Depth32F ||
        format == MultisampleTextureFormat::Depth24Stencil8 ||
        format == MultisampleTextureFormat::Depth32FStencil8;
}

bool MultisampleTexture::IsStencilFormat(MultisampleTextureFormat format) {
    return format == MultisampleTextureFormat::Depth24Stencil8 ||
        format == MultisampleTextureFormat::Depth32FStencil8;
}

#pragma endregion


NAMESPACE_OPENGL_CORE_END
