#pragma once

#include <TopoDS_Shape.hxx>
#include <string>
#include <vector>

/**
 * @brief STEP 模型解析类
 *
 * 使用 OpenCASCADE 加载并解析 STEP 文件，提取几何体信息。
 * 属于 layer2 组件层，供 layer3 界面层调用。
 */
class StepModel
{
public:
    StepModel();
    ~StepModel();

    /// 从 STEP 文件路径加载模型，成功返回 true
    bool Load(const std::string& filePath);

    /// 是否已成功加载
    bool IsLoaded() const { return m_loaded; }

    /// 获取底层 TopoDS_Shape（供渲染/网格化使用）
    const TopoDS_Shape& GetShape() const { return m_shape; }

    /// 获取模型面片数量
    int GetFaceCount() const { return m_faceCount; }

    /// 获取 STEP 文件中的根实体数量
    int GetRootCount() const { return m_rootCount; }

    /// 卸载模型，释放资源
    void Unload();

private:
    TopoDS_Shape m_shape;
    bool         m_loaded    = false;
    int          m_faceCount = 0;
    int          m_rootCount = 0;
};
