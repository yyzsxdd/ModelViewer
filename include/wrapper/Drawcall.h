#pragma once

#include "ogl_defs.h"

#include <string>
#include <vector>
#include <set>
#include <assert.h>

NAMESPACE_OPENGL_CORE_BEGIN



//顶点属性指针
struct VertexLayout {
    uint32_t count = 0;			// 顶点属性由多少个datatype类型的数据组成 例layout(location = 0) in vec4 aPos; count 为4
    GLenum dataType = 0;		// 顶点属性的类型  例layout(location = 0) in vec4 aPos; datatype 为float
    uint32_t stride = 0;		// 顶点数据在所在vbo中的 步长
    uint32_t offset = 0;		// 顶点属性在单个步长中的起始偏移
    bool normalized = false;	// 是否归一化: 用于"定点数(如:整数)", 按其取值范围归一化到 -1.0~1.0(有符号时) 或, 0.0~1.0(无符号时)
};

//顶点属性绑定
struct VertexBind {
    GLuint vbo = 0;					// vbo (可以使用不同的vbo中数据进行绘制)
    int location = -1;		        // shader中获取的 attribute location 或者shader中的attribute layout
    VertexLayout layout;            // 顶点属性指针布局

    // 聚合体构造，支持 {vbo, location, layout}
    VertexBind() = default;
    VertexBind(GLuint vbo_, int loc_, VertexLayout layout_) : vbo(vbo_), location(loc_), layout(layout_) {}

};

//实例化渲染顶点属性绑定
struct InstanceVertexBind : public VertexBind {
    //控制顶点属性多久更新一次。
    //默认情况(除数 = 0)：属性每个顶点更新一次。
    //实例化情况(除数 = 1)：属性每个实例更新一次。
    //高级情况(除数 = n, n > 1)：属性每 n 个实例更新一次。
    //例：实例化数据 (另一个VBO存储每个实例的偏移量及其他个性化数据）可以以此控制更新频率
    uint32_t divisor = 0;     // 顶点属性更新率（0=顶点，1=实例）
    // 允许用父对象构造
    explicit InstanceVertexBind(const VertexBind& base) : VertexBind(base) {}

    //支持 {0,0,layout,divisor} 直接构造
    InstanceVertexBind(GLuint vbo_, int loc_, VertexLayout layout_, uint32_t divisor_ = 0) : VertexBind(vbo_, loc_, layout_),divisor(divisor_) {}
};


/// <summary>
/// 非实例化drawcall
/// </summary>
class Drawcall {
public:
    Drawcall();
    ~Drawcall();

    // 禁止拷贝
    Drawcall(const Drawcall&) = delete;
    Drawcall& operator=(const Drawcall&) = delete;

    // 启用 VAO, 注意: 如果 OpenGL 初始化为了 "核心模式", 那么只能启用 VAO
    void BuildVAO(bool clear = true);
   
    void Draw() const;

    void Draw(GLenum primitiveType, uint32_t drawStart, uint32_t drawCount);
  
    // 使用一个直接指向客户端内存（顶点数组）的指针，作为glVertexAttribPointer 的最后一个参数，在ogl3.1 core以上已被废弃，为了兼容老代码而支持这种方式
    void AllowVertexPtr(void* ptr = nullptr){ vertexPtr = ptr; };
    // 使用一个直接指向客户端内存（顶点数组）的指针，作为glDrawElements 的最后一个参数，在ogl3.1 core以上已被废弃，为了兼容老代码而支持这种方式
    void AllowIndexPtr(void* ptr = nullptr){ indexPtr = ptr; };

    //图元重启功能，可以使用同一个索引缓冲区 (EBO/IBO) 绘制多个不相连的三角形条带 (Triangle Strips)。
    //当检索到特殊index后，会切断当前条带绘制，重新开始新条带的绘制
    //重启索引必须是无效的索引值。和 indexType 匹配
    //通常的做法是：定义重启索引为 0xFFFF (对于 16 位) 或 0xFFFFFFFF (对于 32 位)，并在实际数据中确保没有顶点使用这个索引值，或者通过顶点数组的 baseVertex 参数来绕过。
    void EnablePrimIndexRestart(bool flag, uint32_t index = 0xFFFFFFFF) {  m_enablePrimIndexRestart = flag; m_restartIndex = index; };

    void SetIBOHandle(GLuint iboHandle) { m_ibo = iboHandle; };
    void SetPrimitiveType(GLenum primitiveType) { m_primitiveType = primitiveType; };
    void SetIndexType(GLenum indexType) { m_indexType = indexType; };
    void SetDrawStart(uint32_t drawStart) { m_drawStart = drawStart; };
    void SetDrawCount(uint32_t drawCount) { m_drawCount = drawCount; };
    void PushBackvecVLB(const VertexBind& bind) { m_vecVLB.push_back(bind); };

    //重设顶点属性指针中的buffer
    bool ResetVertexBuffer(uint32_t index, GLuint vertexBufferHandle);
   
    uint32_t GetDrawCount() const { return m_drawCount; };
    uint32_t GetDrawStart() const { return m_drawStart; };
private:
    void ClearVAO();
    void DoBindVBO() const;
    void DoBindIBO() const;
    void DoDraw() const;

private:

    GLenum m_primitiveType = GL_TRIANGLE_STRIP;	// 图元类型
    GLenum m_indexType = GL_UNSIGNED_BYTE;		// 索引类型, GL_UNSIGNED_BYTE(最多 256 个顶点), GL_UNSIGNED_SHORT(最多 65536 个顶点，最常用), or GL_UNSIGNED_INT(大量顶点时使用)
    uint32_t m_drawStart = 0;		// 起始位置, 顶点缓冲区 或 索引缓冲区
    uint32_t m_drawCount = 0;		// 绘制个数, 顶点缓冲区 或 索引缓冲区
    std::vector<VertexBind> m_vecVLB;	// 顶点绑定信息: 顶点布局,顶点缓冲区,vertex-shader-attribute-location

    bool m_enablePrimIndexRestart = false;
    uint32_t m_restartIndex = 0xFFFFFFFF;

    void* indexPtr = nullptr;		// 兼容 一个直接指向客户端内存（顶点索引数组）的指针
    void* vertexPtr = nullptr;      // 兼容 一个直接指向客户端内存（顶点数组）的指针

    GLuint m_vao = 0;       //VAO
    GLuint m_ibo = 0;		//ibo
};


/// <summary>
/// 实例化drawcall
/// </summary>
class InstanceDrawcall {
public:
    InstanceDrawcall();
    ~InstanceDrawcall();

    // 禁止拷贝
    InstanceDrawcall(const InstanceDrawcall&) = delete;
    InstanceDrawcall& operator=(const InstanceDrawcall&) = delete;

    void BuildVAO(bool clear = true);
    void Draw() const;

    void EnablePrimIndexRestart(bool flag, uint32_t index) { m_enablePrimIndexRestart = flag; m_restartIndex = index; };

    void SetIBOHandle(GLuint iboHandle) { m_ibo = iboHandle; };
    void SetPrimitiveType(GLenum primitiveType) { m_primitiveType = primitiveType; };
    void SetIndexType(GLenum indexType) { m_indexType = indexType; };
    void SetDrawStart(uint32_t drawStart) { m_drawStart = drawStart; };
    void SetDrawCount(uint32_t drawCount) { m_drawCount = drawCount; };
    void SetInstanceCount(uint32_t count) { m_instanceCount = count; }
    void PushBackvecVLB(const InstanceVertexBind& bind) { m_vecVLB.push_back(bind); };
    //instanceBuffer 重建后重新设置绑定指针,只会影响已经push_back进来的InstanceVertexBind
    //返回ture 表示instanceBuffer的handle做了更新，返回false 入参和已有一致，不需要更新
    bool ResetInstanceBuffer(GLuint instanceBufferHandle);   

    uint32_t GetDrawCount() const  { return m_drawCount; };
    uint32_t GetDrawStart() const { return m_drawStart; };
    uint32_t GetInstanceCount() const { return m_instanceCount; };

private:
    void DoBindVBO() const;
    void DoBindIBO() const;
    void DoDraw() const;
    void ClearVAO();
    
private:

    uint32_t m_instanceCount = 1; // 默认为1（单实例）
    uint32_t m_drawStart = 0;
    uint32_t m_drawCount = 0;
    GLenum m_primitiveType = GL_TRIANGLE_STRIP;	// 图元类型
    GLenum m_indexType = GL_UNSIGNED_BYTE;		// 索引类型, GL_UNSIGNED_BYTE, GL_UNSIGNED_SHORT, or GL_UNSIGNED_INT
    std::vector<InstanceVertexBind> m_vecVLB;	// 顶点绑定信息: 顶点布局,顶点缓冲区,vertex-shader-attribute-location

    bool m_enablePrimIndexRestart = false;
    uint32_t m_restartIndex = 0xFFFFFFFF;

    GLuint m_vao = 0;
    GLuint m_ibo = 0;	
};

NAMESPACE_OPENGL_CORE_END
