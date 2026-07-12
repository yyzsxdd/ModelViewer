
#include "Drawcall.h"
#include "ogl_defs.h"

NAMESPACE_OPENGL_CORE_BEGIN

//绘制时 glDrawElements 的类型参数必须与 IBO 中实际存储的索引类型匹配：
static size_t GetIBOIndexType(GLenum indexType) {
    // 根据索引类型计算字节大小
    size_t indexSize = 0;
    switch (indexType) {
    case GL_UNSIGNED_BYTE:  indexSize = 1; break;
    case GL_UNSIGNED_SHORT: indexSize = 2; break;
    case GL_UNSIGNED_INT:   indexSize = 4; break;
    default: assert(false);
    }
    return indexSize;
}


Drawcall::Drawcall() = default;

Drawcall::~Drawcall() {
    ClearVAO();
}

void Drawcall::ClearVAO() {
    if (m_vao != 0) {
        GL(glDeleteVertexArrays, 1, &m_vao);
        m_vao = 0;
    }
}

void Drawcall::Draw() const {
    if (m_drawCount == 0) return;
    if (m_vao != 0) {
        GL(glBindVertexArray, m_vao);
    }
    else {
        DoBindVBO();
    }

    CallFinal call([&]() {
        if (m_vao != 0)
        {
            GL(glBindVertexArray, 0);
        }
        else
        {
            for (auto itor = m_vecVLB.begin(); itor != m_vecVLB.end(); ++itor)
            {
                const auto& item = *itor;
                if (item.location < 0) { continue; }
                GL(glDisableVertexAttribArray, item.location);

            }
        }
        GL(glBindBuffer, GL_ARRAY_BUFFER, 0);
    });

    DoDraw();

}

void Drawcall::Draw(GLenum primitiveType, uint32_t drawStart, uint32_t drawCount) {
    m_drawCount = drawCount;
    m_drawStart = drawStart;
    m_primitiveType = primitiveType;
    Draw();
}

void Drawcall::DoDraw() const {
    // IBO 绘制方式
    if (m_ibo != 0) {
        if (m_enablePrimIndexRestart) {
            GL(glEnable, GL_PRIMITIVE_RESTART);
            GL(glPrimitiveRestartIndex, m_restartIndex);
        }
        else {
            GL(glDisable, GL_PRIMITIVE_RESTART);
        }

        auto indexSize = GetIBOIndexType(m_indexType);
        VI_ASSERT_LOG_ERROR_RETURN(indexSize != 0, "IBO index type error");
        GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ibo);
        GL(glDrawElements, m_primitiveType, m_drawCount, m_indexType, (const void*)(size_t)(m_drawStart * indexSize));
        GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, 0);
    }
    // SysMemory index 方式
    else if (indexPtr != nullptr) {
        if (m_enablePrimIndexRestart) {
            GL(glEnable, GL_PRIMITIVE_RESTART);
            GL(glPrimitiveRestartIndex, m_restartIndex);
        }
        else {
            GL(glDisable, GL_PRIMITIVE_RESTART);
        }
        GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, 0);	// 必须要绑定空, 否则 glDrawElements 会认为 indices 参数为 offset
        GL(glDrawElements, m_primitiveType, m_drawCount, m_indexType, indexPtr);
    }
    // vbo方式
    else {
        GL(glDrawArrays, m_primitiveType, m_drawStart, m_drawCount);
    }
}

void Drawcall::BuildVAO(bool clear) {
    //VAO 和 顶点数组指针是互斥的
    VI_ASSERT_LOG_ERROR_RETURN(vertexPtr == nullptr, "VAO and vertex array pointer cannot be used simultaneously; vertex array pointer has been deprecated.");
    if (clear) {
        ClearVAO();
        GL(glGenVertexArrays, 1, &m_vao);
    }
    GL(glBindVertexArray, m_vao);
    CallFinal call([]() { GL(glBindVertexArray, 0); });
    DoBindVBO();
    DoBindIBO();
}

void Drawcall::DoBindVBO() const {
    for (auto itor = m_vecVLB.begin(); itor != m_vecVLB.end(); ++itor) {
        const auto& item = *itor;
        if (item.location < 0) { continue; }
        const auto dt = item.layout.dataType;
        if (item.vbo != 0) {
            GL(glBindBuffer, GL_ARRAY_BUFFER, item.vbo);
            if (dt == GL_UNSIGNED_INT || dt == GL_INT || dt == GL_BYTE || dt == GL_UNSIGNED_BYTE || dt == GL_SHORT || dt == GL_UNSIGNED_SHORT) {
                GL(glVertexAttribIPointer, item.location, item.layout.count, item.layout.dataType, item.layout.stride, (const void*)(size_t)item.layout.offset);
            }
            else {
                GL(glVertexAttribPointer, item.location, item.layout.count, item.layout.dataType, item.layout.normalized, item.layout.stride, (const void*)(size_t)item.layout.offset);
            }
        }
        else if (vertexPtr != nullptr) {
            GL(glBindBuffer, GL_ARRAY_BUFFER, 0);
            if (dt == GL_UNSIGNED_INT || dt == GL_INT || dt == GL_BYTE || dt == GL_UNSIGNED_BYTE || dt == GL_SHORT || dt == GL_UNSIGNED_SHORT) {
                GL(glVertexAttribIPointer, item.location, item.layout.count, item.layout.dataType, item.layout.stride, ((const uint8_t*)vertexPtr) + item.layout.offset);
            }
            else {
                GL(glVertexAttribPointer, item.location, item.layout.count, item.layout.dataType, item.layout.normalized, item.layout.stride, ((const uint8_t*)vertexPtr) + item.layout.offset);
            }
        }
        else {
            VI_ASSERT_LOG_ERROR_RETURN(false, "VBO bind error");
        }
        GL(glEnableVertexAttribArray, item.location);
    }
}

void Drawcall::DoBindIBO() const {
    if (m_ibo != 0) {
        GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    }
}

bool Drawcall::ResetVertexBuffer(uint32_t index, GLuint vertexBufferHandle) {
    if (index >= m_vecVLB.size()) {
        VI_ASSERT_LOG_ERROR_RETURN_VALUE(false,false, "index out range");
    }
    m_vecVLB[index].vbo = vertexBufferHandle;
    return true;
}

///=======================instanceDrawcall================================//

InstanceDrawcall::InstanceDrawcall() = default;

InstanceDrawcall::~InstanceDrawcall() {
    ClearVAO();
}

void InstanceDrawcall::ClearVAO() {
    if (m_vao != 0) {
        GL(glDeleteVertexArrays, 1, &m_vao);
        m_vao = 0;
    }
}

void InstanceDrawcall::Draw() const {
    if (m_drawCount == 0) return;
    if (m_instanceCount == 0)  return;

    if (m_vao != 0) {
        GL(glBindVertexArray, m_vao);
    }
    else {
        DoBindVBO();
    }

    CallFinal call([&]() {
        if (m_vao != 0)
        {
            GL(glBindVertexArray, 0);
        }
        else
        {
            for (auto itor = m_vecVLB.begin(); itor != m_vecVLB.end(); ++itor)
            {
                const auto& item = *itor;
                if (item.location < 0) { continue; }
                GL(glDisableVertexAttribArray, item.location);

                if (item.divisor > 0)
                {
                    glVertexAttribDivisor(item.location, 0);
                }

            }
        }
        GL(glBindBuffer, GL_ARRAY_BUFFER, 0);
        });

    DoDraw();

}

void InstanceDrawcall::DoDraw() const {
    if (m_ibo != 0) {
        auto indexSize = GetIBOIndexType(m_indexType);
        VI_ASSERT_LOG_ERROR_RETURN(indexSize != 0, "IBO index type error");

        if (m_enablePrimIndexRestart) {
            GL(glEnable, GL_PRIMITIVE_RESTART);
            GL(glPrimitiveRestartIndex, m_restartIndex);
        }
        else {
            GL(glDisable, GL_PRIMITIVE_RESTART);
        }

        GL(glDrawElementsInstanced, m_primitiveType, m_drawCount, m_indexType, (const void*)(size_t)(m_drawStart * indexSize), (GLsizei)m_instanceCount);
    }
    else {
        GL(glDrawArraysInstanced, m_primitiveType, m_drawStart, m_drawCount, (GLsizei)m_instanceCount);
    }
}

void InstanceDrawcall::BuildVAO(bool clear) {
    if (clear) {
        ClearVAO();
        GL(glGenVertexArrays, 1, &m_vao);
    }
    GL(glBindVertexArray, m_vao);
    CallFinal call([]() { GL(glBindVertexArray, 0); });
    DoBindVBO();
    DoBindIBO();
}

void InstanceDrawcall::DoBindIBO() const {
    VI_ASSERT_LOG_ERROR_RETURN(m_ibo != 0, "ibo bind error");
    GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ibo);
}

void InstanceDrawcall::DoBindVBO() const {
    for (auto itor = m_vecVLB.begin(); itor != m_vecVLB.end(); ++itor) {
        const auto& item = *itor;
        if (item.location < 0 || item.vbo == 0) { continue; }

        const auto dt = item.layout.dataType;

        GL(glBindBuffer, GL_ARRAY_BUFFER, item.vbo);
        if (dt == GL_UNSIGNED_INT || dt == GL_INT || dt == GL_BYTE || dt == GL_UNSIGNED_BYTE || dt == GL_SHORT || dt == GL_UNSIGNED_SHORT) {
            GL(glVertexAttribIPointer, item.location, item.layout.count, item.layout.dataType, item.layout.stride, (const void*)(size_t)item.layout.offset);
        }
        else {
            GL(glVertexAttribPointer, item.location, item.layout.count, item.layout.dataType, item.layout.normalized, item.layout.stride, (const void*)(size_t)item.layout.offset);
        }
        // 处理实例属性 //实例化数据的核心函数 //此参数默认行为是0
        if (item.divisor > 0)
        {
            GL(glVertexAttribDivisor, item.location, item.divisor);
        }

        GL(glEnableVertexAttribArray, item.location);
    }
    if (m_ibo != 0) {
        GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    }
}

bool InstanceDrawcall::ResetInstanceBuffer(GLuint instanceBufferHandle) {
    bool flag = false;
    for (auto itor = m_vecVLB.begin(); itor != m_vecVLB.end(); ++itor)
    {
        auto& item = *itor;
        if (item.location < 0) { continue; }
        if (item.divisor > 0 && item.vbo != instanceBufferHandle)
        {
            item.vbo = instanceBufferHandle;
            flag = true;
        }
    }
    return flag;
}


NAMESPACE_OPENGL_CORE_END
