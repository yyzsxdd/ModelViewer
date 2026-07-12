
#include "Buffer.h"
#include "Texture.h"
#include "ogl_defs.h"
#include <cstring>

NAMESPACE_OPENGL_CORE_BEGIN

//创建buffer时需要知道buffer类型
GeneralBuffer::GeneralBuffer(GLenum target) :m_target(target) {}

GeneralBuffer::~GeneralBuffer() {
    Destroy();
}

void GeneralBuffer::Create(const void* data, GLsizeiptr size, GLenum usage) {
    // 销毁旧的
    Destroy();

    m_usage = usage;
    m_size = size;

    //DSA用法(opengl4.5+)  glNamedBufferData(m_handle, size, data, usage); 可以免去频繁bind/unbind
    GL(glGenBuffers, 1, &m_handle);
    VI_ASSERT_LOG_ERROR_RETURN(m_handle != 0, "Failed to generate buffer handle");

    Bind();
    GL(glBufferData, m_target, size, data, usage);
    Unbind();
}

// 更新数据（完全或部分）
void GeneralBuffer::Update(const void* data, GLsizeiptr size, GLsizeiptr offset) {
  
    VI_ASSERT_LOG_ERROR_RETURN(m_handle != 0, "could not updata buffer before create");
    VI_ASSERT_LOG_ERROR_RETURN(offset + size <= m_size, "Buffer update out of bounds");
   
    //DSA用法(opengl4.5+) GL(glNamedBufferSubData, m_target, offset, size, data);
    Bind();
    GL(glBufferSubData, m_target, offset, size, data);
    Unbind();
}

// 便捷方法：更新整个缓冲区
void GeneralBuffer::Update(const void* data) {
    Update(data, m_size, 0);
}

// 销毁缓冲区
void GeneralBuffer::Destroy() {
    if (m_handle != 0) {
        GL(glDeleteBuffers, 1, &m_handle);
        m_handle = 0;
    }
    m_size = 0;
}

void GeneralBuffer::Resize(GLsizeiptr newSize, bool preserveData)
{
    VI_ASSERT_LOG_ERROR_RETURN(m_handle != 0, "could not resize buffer before create");
    if (newSize == m_size) return;

    // 如果需要保留数据，先拷贝出来
    std::vector<char> oldData;
    if (preserveData && m_size > 0) {
        oldData.resize(m_size);
        Dump(0, m_size, oldData.data());
    }

    Bind();
    if (preserveData && !oldData.empty()) {
        // 保留数据：用新大小重新分配，然后拷回数据
        GL(glNamedBufferData,m_target, newSize, nullptr, m_usage);
        if (newSize > m_size) {
            // 新缓冲区更大，只拷旧数据部分
            GL(glBufferSubData,m_target, 0, m_size, oldData.data());
        }
        else {
            // 新缓冲区更小，拷部分数据
            GL(glBufferSubData,m_target, 0, newSize, oldData.data());
        }
    }
    else {
        // 不保留数据，直接重新分配
        GL(glBufferData,m_target, newSize, nullptr, m_usage);
    }
    Unbind();

    m_size = newSize;
}

// 读取数据到 CPU
void GeneralBuffer::Dump(GLintptr offset, GLsizeiptr size, void* outData) const {

    VI_ASSERT_LOG_ERROR_RETURN(offset + size <= m_size, "Buffer read out of bounds");
    // 需要绑定后才能读取
    Bind();

    // 方法1：使用 glGetBufferSubData（需要 OpenGL 4.5+ 或 ARB_buffer_storage）
    // 方法2：使用 glMapBufferRange（兼容性更好）
    void* ptr = glMapBufferRange(m_target, offset, size, GL_MAP_READ_BIT);
    if (ptr) {
        memcpy(outData, ptr, size);
        glUnmapBuffer(m_target);
    }
    else {
        Unbind();
        VI_ASSERT_LOG_ERROR_RETURN(false, "Failed to map buffer for reading");
    }
    Unbind();
}

// 映射缓冲区以便 CPU 直接访问并写入数据，使用完以后要unmap
void* GeneralBuffer::Map(GLbitfield access) const {
    Bind();
    //glMapBufferRange 使用前需要bind
    void* ptr = glMapBufferRange(m_target, 0, m_size, access);
    if (!ptr) {
        Unbind();
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false, nullptr, "Failed to map buffer");
    }
    return ptr;
}

void GeneralBuffer::UnMap() const {
    if (glUnmapBuffer(m_target) != GL_TRUE) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "Failed to unmap buffer");
    }
    Unbind();
}

// 绑定到通用目标
void GeneralBuffer::Bind() const {
    GL(glBindBuffer, m_target, m_handle);
}

// 解绑
void GeneralBuffer::Unbind() const {
    // todo: need use safe recovery
    GL(glBindBuffer,m_target, 0);
}

// 绑定到绑定点（用于 UBO/SSBO）
void GeneralBuffer::BindBase(GLuint bindingPoint) const {
    GL(glBindBufferBase,m_target, bindingPoint, m_handle);
}

// 绑定范围（用于 UBO/SSBO 的部分绑定）
void GeneralBuffer::BindRange(GLuint bindingPoint, GLintptr offset, GLsizeiptr size) const {
    
    VI_ASSERT_LOG_ERROR_RETURN(offset + size <= m_size,"Buffer range out of bounds");
    
    GL(glBindBufferRange,m_target, bindingPoint, m_handle, offset, size);
}

void GeneralBuffer::GrowTo(uint32_t newSize)
{
    //内存映射期间不允许扩容操作
    assert(m_isMapped == false);
    if (newSize <= m_size) { return; }
    uint32_t oldSize = m_size;
    if (m_isSuspend) {
        doCacheBufferGrowTo(oldSize, newSize);		// 调整 m_recCacheMemory 和 m_cacheMemory
    }
    else {
        doGLBufferGrowTo(oldSize, newSize);		// 调整 Opengl Buffer
    }
    m_size = newSize;
}

void GeneralBuffer::Reserve(uint32_t newSize)
{
    if (newSize <= GetSize()) { return; }
    uint32_t oldSize = GetSize();
    uint32_t growToSize = std::max((oldSize < MAX_INCREMENT_SIZE ? oldSize << 1 : oldSize + MAX_INCREMENT_SIZE), newSize);
    this->GrowTo(growToSize);
}

void GeneralBuffer::Write(GLintptr offset, GLsizeiptr size, const void* data)
{
   
    Reserve((uint32_t)(offset + size));
    
    if (m_isSuspend) {
        assert(m_cacheMemory != nullptr);
        m_recCacheMemory.Record((uint32_t)offset, (uint32_t)size);
        memcpy(m_cacheMemory + offset, data, size);
    }
    else {
        assert(m_cacheMemory == nullptr);
        GL(glNamedBufferSubData, m_handle, offset, size, data);
    }
}

void GeneralBuffer::Suspend(bool writeOnly)
{
    if (m_isSuspend) { return; }
    //内存映射期间不允许使用挂起
    assert(m_isMapped == false );
    //挂起时，不允许存有缓存数据
    assert(m_cacheMemory == nullptr && m_recCacheMemory.IsEmpty());
    m_cacheMemory = new uint8_t[m_size];
    m_recCacheMemory.GrowTo(m_size);
    m_isSuspendForWriteOnly = writeOnly;
    //挂起过程中可能有读有写，如果不是只写，需要把gpu数据取出来
    if (writeOnly == false) {
        GetData(m_cacheMemory, 0, m_size);
    }
    m_isSuspend = true;
}

void GeneralBuffer::GetData(GLvoid* data, GLintptr offset, GLsizeiptr size) const
{
    //挂起时，只写模式不需要也不允许获取数据
    if (m_isSuspend) {
        //内存映射期间不允许使用挂起
        assert(m_isMapped == false);

        assert(m_isSuspendForWriteOnly == false);
        memcpy(data, m_cacheMemory + offset, size);
    }
    else {
        GL(glGetNamedBufferSubData, m_handle, offset, size, data);
    }
}

void GeneralBuffer::Resume()
{
    //如果未被挂起，复位也无效
    if (m_isSuspend == false) { return; }
    //内存映射期间不允许使用挂起/复位
    assert(m_isMapped == false);
    assert(m_cacheMemory != nullptr);

    CallFinal callFinal([&]() {
        delete[] m_cacheMemory;
        m_cacheMemory = nullptr;
        m_recCacheMemory.Clear();
        m_isSuspendForWriteOnly = false;
        m_isSuspend = false; });

    // 判断挂起期间是否有过操作
    if (m_recCacheMemory.IsEmpty()) { return; }

    // 判断是否在挂起期间有过扩容
    uint32_t bufferSize;
    GL(glGetNamedBufferParameteriv, m_handle, GL_BUFFER_SIZE, (GLint*)&bufferSize);

    //数据量过大
    assert(bufferSize < INT_MAX);

    if (bufferSize < m_size) {
        doGLBufferGrowTo(bufferSize, m_size);
    }
    else {
        assert(bufferSize == m_size);
    }

    flushDataToGLBuffer(true);
}

void GeneralBuffer::flushDataToGLBuffer(bool clearCache)
{
    if (m_cacheMemory == nullptr) { return; }
    const MemoryRecord::SpaceList& recList = m_recCacheMemory.GetRecords();
    if (!recList.empty()) {
        for (auto itor = recList.begin(); itor != recList.end(); ++itor) {
            const auto start = itor->first;
            const auto size = itor->second;
            GL(glNamedBufferSubData, m_handle, start, size, m_cacheMemory + start);
        }
    }
    if (clearCache) {
        delete[] m_cacheMemory; m_cacheMemory = nullptr;
        m_recCacheMemory.Clear();
    }
}

void GeneralBuffer::doGLBufferGrowTo(uint32_t oldSize, uint32_t newSize)
{
    GLuint newHandle;
    GL(glGenBuffers, 1, &newHandle);
    assert(newHandle != 0);

    GL(glNamedBufferDataEXT, newHandle, newSize, nullptr, m_usage);
 
    GL(glCopyNamedBufferSubData, m_handle, newHandle, 0, 0, oldSize);
    
    GL(glDeleteBuffers, 1, &m_handle);
    m_handle = newHandle;
   
}

void GeneralBuffer::doCacheBufferGrowTo(uint32_t oldSize, uint32_t newSize)
{
    assert(m_cacheMemory != nullptr);
    auto newCacheMemory = new uint8_t[newSize];
    assert(newCacheMemory != nullptr);

    memcpy(newCacheMemory, m_cacheMemory, oldSize);

    delete[] m_cacheMemory;
    m_cacheMemory = newCacheMemory;
    // 注: 同时修改 rec
    m_recCacheMemory.GrowTo(newSize);
}


#pragma region PBO

void ReadPBO::ReadPixels(int x, int y, int width, int height, TextureFormat oglFormat)
{
    Bind();
    GL(glReadPixels,x, y, width, height, Texture::ToGLFormat(oglFormat), Texture::ToGLType(oglFormat), 0);
    Unbind();
}

void ReadPBO::GetBuffer(int offset, int size, uint8_t* dstBuff)
{
    if (offset < 0 || offset + size > static_cast<size_t>(GetSize())) {
        VI_ASSERT_LOG_ERROR_RETURN(false, "PBO read out of range!");
    }
    Bind();
    GL(glGetBufferSubData,GL_PIXEL_PACK_BUFFER, offset, size, dstBuff);
    Unbind();
}

void ReadPBO::GetBufferByMap(uint8_t* dstBuff)
{
    Bind();
    void* ptr = Map(GL_MAP_READ_BIT);  // 等待 GPU 完成并映射
    if (ptr) {
        memcpy(dstBuff, ptr, GetSize());
        UnMap();
    }
    Unbind();
}


#pragma endregion



NAMESPACE_OPENGL_CORE_END
