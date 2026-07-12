#pragma once

#include "ogl_defs.h"
#include <string>
#include <set>


NAMESPACE_OPENGL_CORE_BEGIN

/*
Target 枚举	                    用途	                                典型大小
GL_ARRAY_BUFFER	                顶点属性数据（位置、法线、UV等）	    几 MB ~几百 MB
GL_ELEMENT_ARRAY_BUFFER	        索引数据（glDrawElements 用）   	    几 MB ~几十 MB
GL_UNIFORM_BUFFER	            Uniform 块数据（UBO）	            ≤ 64KB（建议）
GL_SHADER_STORAGE_BUFFER	    着色器读写缓冲区（SSBO）	            几百 MB ~GB 级别
GL_TEXTURE_BUFFER	            将 buffer 作为纹理采样(TBO)	        取决于纹理大小
GL_DRAW_INDIRECT_BUFFER	        间接绘制命令参数	                    几千字节
GL_DISPATCH_INDIRECT_BUFFER	    间接计算调度参数	                    几千字节
GL_PIXEL_PACK_BUFFER	        异步读取帧缓冲(PBO)(glReadPixels）	与分辨率相关
GL_PIXEL_UNPACK_BUFFER	        异步上传纹理数据(PBO)	                与纹理大小相关
GL_TRANSFORM_FEEDBACK_BUFFER	捕获顶点着色器输出	                几 MB
GL_ATOMIC_COUNTER_BUFFER	    原子计数器存储	                    通常 ≤ 64KB
GL_QUERY_BUFFER	                异步查询结果存储	                    几千字节
GL_COPY_READ_BUFFER	            缓冲区间拷贝的源	                    任意
GL_COPY_WRITE_BUFFER	        缓冲区间拷贝的目标	                任意
GL_PARAMETER_BUFFER	            存储着色器参数（GL 4.6）	            几千字节
*/

/*
usage	            频率	       	典型应用
GL_STATIC_DRAW	    一次设置	    读取	模型顶点数据、静态网格
GL_STATIC_READ	    一次设置	    写入 → CPU 读取	离屏渲染结果回读
GL_STATIC_COPY	    一次设置	    读取并拷贝	数据中转缓冲区
GL_DYNAMIC_DRAW	    频繁修改	    读取	每帧更新的 UBO、骨骼动画
GL_DYNAMIC_READ	    频繁修改	    写入 → CPU 读取	物理模拟结果回读
GL_DYNAMIC_COPY	    频繁修改	    读取并拷贝	动态数据的中转缓冲
GL_STREAM_DRAW	    每帧修改	    读取	粒子系统、程序化生成几何体
GL_STREAM_READ	    每帧修改	    写入 → CPU 读取	实时截图、录制
GL_STREAM_COPY	    每帧修改	    读取并拷贝	实时数据转发
*/

/// <summary>
/// 通用创建buffer的方式，
/// 常见buffer类型 
/// VBO	✅  无额外步骤
/// EBO	✅  无额外步骤
/// UBO	✅  无额外步骤
/// SSB ✅  无额外步骤
/// PBO	✅  无额外步骤
/// TBO	✅  需要额外创建纹理对象并关联
/// </summary>
///

/*
通用buffer类，要确定buffer 的target 是否支持扩容，自动扩容的持久映射后需要重新映射
*/

class GeneralBuffer {
public:
    explicit GeneralBuffer(GLenum target);
    ~GeneralBuffer();

    // 禁止拷贝
    GeneralBuffer(const GeneralBuffer&) = delete;
    GeneralBuffer& operator=(const GeneralBuffer&) = delete;

    // 创建并分配内存
    void Create(const void* data, GLsizeiptr size, GLenum usage);

    //使用dsa方式，自动扩容
    void Write(GLintptr offset, GLsizeiptr size, const void* data);

    // 更新数据（完全或部分）
    void Update(const void* data, GLsizeiptr size , GLsizeiptr offset);

    // 便捷方法：更新整个缓冲区
    void Update(const void* data);

    // 销毁缓冲区
    void Destroy();

    // 读取数据到 CPU
    void Dump(GLintptr offset, GLsizeiptr size, void* outData) const;

    // 映射缓冲区以便 CPU 直接访问并写入数据(返回可直接写入的指针)，使用完以后要unmap
    // 相比glBufferSubData效率更高，glBufferSubData多一次额外拷贝，CPU → 驱动缓存 → GPU
    void* Map(GLbitfield access = GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT) const;

    //map完成以后要unmap
    void UnMap() const;

    // 绑定到通用目标
    void Bind() const;

    //解绑
    void Unbind() const;

    // 绑定到绑定点（用于 UBO/SSBO）
    void BindBase(GLuint bindingPoint) const;

    // 绑定范围（用于 UBO/SSBO 的部分绑定）
    void BindRange(GLuint bindingPoint, GLintptr offset, GLsizeiptr size) const;

    //重新分配大小
    void Resize(GLsizeiptr newSize, bool preserveData = true);

    // 无效化buffer内容（提示驱动可以丢弃旧数据）
    void invalidate() const  {glInvalidateBufferData(m_handle);}

    void invalidateRange(GLintptr offset, GLsizeiptr length) const {
        glInvalidateBufferSubData(m_handle, offset, length);
    }

    void GrowTo(uint32_t newSize);		// 扩展到 newSize
    void Reserve(uint32_t newSize);		// 确保有 newSize, 实际会更大

    void GetData(GLvoid* data, GLintptr offset, GLsizeiptr size) const;

    //操作挂起，用于批量操作时的效率提升（是否只读）
    void Suspend(bool writeOnly = false);
    //操作复位，批量提交缓存
    void Resume();  


    GLuint GetHandle() const { return m_handle; }
    GLsizeiptr GetSize() const { return m_size; }
    GLenum GetTarget() const { return m_target; }
    GLenum GetUsage() const { return m_usage; }
    bool IsValid() const { return m_handle != 0; }

private:
    static constexpr uint32_t MAX_INCREMENT_SIZE = 65536; // 64KB

    void doGLBufferGrowTo(uint32_t oldSize, uint32_t newSize);
    void doCacheBufferGrowTo(uint32_t oldSize, uint32_t newSize);

    void flushDataToGLBuffer(bool clearCache = true);  //上传缓存数据到gpu

    GLenum m_target{  GL_ARRAY_BUFFER };     //buffer 的类型
    GLenum m_usage {  GL_STATIC_DRAW };     //buffer 的用途声明

    GLuint m_handle = 0;
    GLsizeiptr m_size = 0;                  //buffer的大小
    MemoryRecord m_recCacheMemory{};        //挂起过程中写入数据的缓存记录
    bool m_isSuspend = false;                       // 是否挂起
    bool m_isSuspendForWriteOnly =false;           // 挂起状态中是否只是写入数据，而不需要读取数据
    bool m_isMapped = false;                        //是否持续化映射了（映射地址可能会失效，由此标签管理是否重新映射）

    uint8_t* m_cacheMemory =nullptr;                 //cpu缓存数据
    uint8_t* m_mapPtr =nullptr;                      //持续映射的gpu内存地址
};


//VBO
class VBO :public GeneralBuffer {
public:
    VBO(): GeneralBuffer(GL_ARRAY_BUFFER) { }
};


//EBO
class EBO :public GeneralBuffer {
public:
    EBO() : GeneralBuffer(GL_ELEMENT_ARRAY_BUFFER) { }
};


//SSBO
class SSBO :public GeneralBuffer {
public:
    SSBO() : GeneralBuffer(GL_SHADER_STORAGE_BUFFER) { }
};


//UBO
class UBO :public GeneralBuffer {
public:
    UBO() : GeneralBuffer(GL_UNIFORM_BUFFER) {  }
};

//TBO
class TBO :public GeneralBuffer {
public:
    TBO() : GeneralBuffer(GL_TEXTURE_BUFFER) { }
};


enum class TextureFormat;
class Texture;

//readPBO （从 GPU 读回数据,异步读取）
class ReadPBO :public GeneralBuffer {
public:
    ReadPBO() : GeneralBuffer(GL_PIXEL_PACK_BUFFER) { }

    void ReadPixels(int x, int y, int width, int height, TextureFormat oglFormat);

    void GetBuffer(int offset, int size, uint8_t* dstBuff);
    void GetBufferByMap(uint8_t* dstBuff);
};


//UploadPBO （异步上传（从 CPU 上传到 GPU））
class UploadPBO :public GeneralBuffer {
public:
    UploadPBO() : GeneralBuffer(GL_PIXEL_UNPACK_BUFFER) {
    }
};


NAMESPACE_OPENGL_CORE_END
