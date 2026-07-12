#include "model/StepModel.h"

#include <STEPControl_Reader.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Face.hxx>
#include <iostream>

StepModel::StepModel()
{
}

StepModel::~StepModel()
{
    Unload();
}

bool StepModel::Load(const std::string& filePath)
{
    Unload();

    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(filePath.c_str());

    if (status != IFSelect_RetDone)
    {
        std::cerr << "[StepModel] Failed to read STEP file: " << filePath << std::endl;
        return false;
    }

    // 转移所有根实体
    reader.TransferRoots();
    m_rootCount = reader.NbRootsForTransfer();

    if (m_rootCount == 0)
    {
        std::cerr << "[StepModel] No transferable roots in: " << filePath << std::endl;
        return false;
    }

    // 获取单个 shape
    m_shape = reader.OneShape();

    // 统计面片数量
    TopExp_Explorer exp;
    for (exp.Init(m_shape, TopAbs_FACE); exp.More(); exp.Next())
    {
        m_faceCount++;
    }

    m_loaded = true;
    std::cout << "[StepModel] Loaded " << filePath
              << " | roots: " << m_rootCount
              << " | faces: " << m_faceCount << std::endl;

    return true;
}

void StepModel::Unload()
{
    m_shape.Nullify();
    m_loaded    = false;
    m_faceCount = 0;
    m_rootCount = 0;
}
