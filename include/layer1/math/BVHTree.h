#pragma once

class Plane;
class PlaneD;

#include "MathDefs.h"
#include "Ray.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <limits>
#include <algorithm>
#include <unordered_map>
#include <thread>
#include <mutex>
#include <future>
#include <atomic>

struct  CylinderParams {
    glm::vec3 start;      // 圆柱体起点（近平面上圆心）
    glm::vec3 direction;  // 圆柱体方向（归一化视线方向）
    double radius;         // 圆柱体半径（世界单位）
};


struct AABB
{
    glm::vec3 minPt;
    glm::vec3 maxPt;

    AABB() :minPt{ (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() },
        maxPt{ (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)(), (std::numeric_limits<float>::lowest)() } {
    }
    AABB(const glm::vec3& pt) :minPt(pt), maxPt(pt) {}
    AABB(const glm::vec3& p1, const glm::vec3& p2)
    {
        minPt = glm::vec3((glm::min)(p1.x, p2.x), (glm::min)(p1.y, p2.y), (glm::min)(p1.z, p2.z));
        maxPt = glm::vec3((glm::max)(p1.x, p2.x), (glm::max)(p1.y, p2.y), (glm::max)(p1.z, p2.z));
    }

    bool isValid() const { return minPt.x != (std::numeric_limits<float>::max)(); }

    void merge(const AABB& other) {
        minPt = (glm::min)(minPt, other.minPt);
        maxPt = (glm::max)(maxPt, other.maxPt);
    }

    void merge(const glm::vec3& pos)
    {
        minPt = (glm::min)(minPt, pos);
        maxPt = (glm::max)(maxPt, pos);
    }

    void merge(const float* posList, const uint32_t& vetextCount)
    {
        for (size_t i = 0; i < vetextCount; i++)
        {
            glm::vec3 v3(*(posList + i * 3), *(posList + (i * 3) + 1), *(posList + (i * 3) + 2));
            merge(v3);
        }
    }

    void merge(const glm::vec4* poslist, int count)
    {
        for (size_t i = 0; i < count; i++)
        {
            glm::vec4 temp = *(poslist + i);
            glm::vec3 v3(temp.x, temp.y, temp.z);
            merge(v3);
        }
    }

    bool contain(const AABB& other)
    {
        if (minPt.x <= other.minPt.x && minPt.y <= other.minPt.y && minPt.z <= other.minPt.z &&
            maxPt.x >= other.maxPt.x && maxPt.y >= other.maxPt.y && maxPt.z >= other.maxPt.z)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    void operator=(const AABB& other)
    {
        minPt.x = other.minPt.x;
        minPt.y = other.minPt.y;
        minPt.z = other.minPt.z;
        maxPt.x = other.maxPt.x;
        maxPt.y = other.maxPt.y;
        maxPt.z = other.maxPt.z;
    }

    bool operator==(const AABB& other)const {
        return
            minPt.x == other.minPt.x &&
            minPt.y == other.minPt.y &&
            minPt.z == other.minPt.z &&
            maxPt.x == other.maxPt.x &&
            maxPt.y == other.maxPt.y &&
            maxPt.z == other.maxPt.z;
    }

    glm::vec3 diagonal()const { return maxPt - minPt; }

    glm::vec3 centroid() const { return glm::vec3((minPt.x + maxPt.x) / 2.0, (minPt.y + maxPt.y) / 2.0, (minPt.z + maxPt.z) / 2.0); }

    bool constructBorder(const AABB& other)
    {
        if (minPt.x == other.minPt.x || minPt.y == other.minPt.y || minPt.z == other.minPt.z ||
            maxPt.x == other.maxPt.x || maxPt.y == other.maxPt.y || maxPt.z == other.maxPt.z)
        {
            return true;
        }
        else
        {
            return false;
        }

    }

    bool equal(const AABB& other)const
    {
        return other.minPt.x == minPt.x && other.minPt.y == minPt.y && other.minPt.z == minPt.z &&
            other.maxPt.x == maxPt.x && other.maxPt.y == maxPt.y && other.maxPt.z == maxPt.z;
    }

    int maxExtent()const
    {
        glm::vec3 d = diagonal();
        if ((std::abs)(d.x) > (std::abs)(d.y) && (std::abs)(d.x) > (std::abs)(d.z))
            return 0;
        else if ((std::abs)(d.y) > (std::abs)(d.z))
            return 1;
        else return 2;
    }

    // 计算半表面积，用于 SAH 代价评估
    float halfArea() const {
        glm::vec3 d = diagonal();
        // diagonal已保证x,y,z >=0;
        return d.x * d.y + d.y * d.z + d.z * d.x;
    }

    bool intersectRay(const Ray& ray)
    {
        double tMin = -std::numeric_limits<double>::infinity();
        double tMax = std::numeric_limits<double>::infinity();

        for (int i = 0; i < 3; i++) {
            if (std::abs(ray.m_direction[i]) < 1e-8) {
                if (ray.m_origin[i] < minPt[i] || ray.m_origin[i] > maxPt[i]) {
                    return false;
                }
            }
            else {
                double ood = 1.0f / ray.m_direction[i];
                double t1 = (minPt[i] - ray.m_origin[i]) * ood;
                double t2 = (maxPt[i] - ray.m_origin[i]) * ood;

                if (t1 > t2) {
                    (std::swap)(t1, t2);
                }

                tMin = (std::max)(tMin, t1);
                tMax = (std::min)(tMax, t2);

                if (tMin > tMax) {
                    return false;
                }
            }
        }
        return true;
    }

    // Morton 编码工具函数：将三维坐标映射为 Z-order curve 的一维编码
    static inline uint32_t splitBy3(uint32_t x) {
        x = (x | (x << 16)) & 0x030000FF;
        x = (x | (x << 8)) & 0x0300F00F;
        x = (x | (x << 4)) & 0x030C30C3;
        x = (x | (x << 2)) & 0x09249249;
        return x;
    }

    static inline uint32_t mortonEncode(const glm::vec3& pos, const glm::vec3& sceneMin, const glm::vec3& sceneMax) {
        // 将坐标归一化到 [0, 1023]（10位精度），再交织为 Morton 码
        glm::vec3 diag = sceneMax - sceneMin;
        glm::vec3 normalized = diag.x > 0 ? (pos - sceneMin) / diag : glm::vec3(0);
        uint32_t ix = (uint32_t)(glm::clamp(normalized.x * 1023.0f, 0.0f, 1023.0f));
        uint32_t iy = (uint32_t)(glm::clamp(normalized.y * 1023.0f, 0.0f, 1023.0f));
        uint32_t iz = (uint32_t)(glm::clamp(normalized.z * 1023.0f, 0.0f, 1023.0f));
        return (splitBy3(ix) << 2) | (splitBy3(iy) << 1) | splitBy3(iz);
    }
};

typedef void* NodeObjectHandle;

struct BVHNode;

struct BVHEntity
{
    NodeObjectHandle object;
    AABB bounds;
    BVHNode* nodePtr{ nullptr };
    BVHEntity() {}
    BVHEntity(const void* _object, const AABB& _bounds)
    {
        object = (NodeObjectHandle)_object;
        bounds = _bounds;
    }
};

struct  BVHNode
{
    AABB bounds;
    BVHNode* father;
    BVHNode* left;
    BVHNode* right;
    std::vector<BVHEntity*> entities;

public:
    BVHNode()
    {
        bounds = AABB();
        father = nullptr;
        left = nullptr;
        right = nullptr;
    }

    bool childContain(const AABB& aabb)
    {
        if (left == nullptr || right == nullptr)
        {
            return false;
        }

        if (left->bounds.contain(aabb) || right->bounds.contain(aabb))
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    BVHNode* getBrotherNode()
    {
        if (father != nullptr)
        {
            return father->left == this ? father->right : father->left;
        }
        return nullptr;
    }

    bool hasChild()
    {
        if (left != nullptr || right != nullptr)
        {
            return true;
        }
        return false;
    }
};

#define MAX_ENTITY_SIZE 64
#define MAX_ADD_SIZE 300
#define PARALLEL_BUILD_THRESHOLD 1024
#define SAH_BIN_COUNT 16               //Binned SAH 的桶数量（通常 16~32）
#define SAH_TRAVERSAL_COST 1.0f        //SAH 遍历代价（内部节点）
#define SAH_INTERSECTION_COST 1.0f     //SAH 求交代价（叶子节点），暂时保留，实际上是不需要的

class  BVHTree
{
public:

    BVHTree();
    ~BVHTree();

    BVHTree(const BVHTree&) = delete;
    BVHTree(const BVHTree&&) = delete;
    BVHTree& operator=(const BVHTree&) = delete;
    BVHTree& operator=(const BVHTree&&) = delete;

private:

struct BVHEntityWithCentroid;

    BVHNode* m_rootNode{ nullptr };

    bool m_lastAddLeft{ false };

    std::unordered_map<NodeObjectHandle, BVHEntity> m_totalObjEntityMap;

    /*std::unordered_map<NodeObjectHandle, AABB> m_addDataMap;*/

    //并行构建的子树结果
    struct SubtreeResult {
        BVHNode* rootNode = nullptr;
        AABB bounds;
    };

    //Morton 码 + entity 的绑定，用于空间排序
    struct MortonEntity {
        uint32_t mortonCode;
        BVHEntityWithCentroid* entityPtr;
        bool operator<(const MortonEntity& other) const {
            return mortonCode < other.mortonCode;
        }
    };

    //Binned SAH 的桶结构
    struct SAHBin {
        AABB bounds;    //桶内所有 entity 的 AABB 并集
        int count = 0;  //桶内 entity 数量
    };

    //Binned SAH 的最优划分结果
    struct SAHSplitResult {
        int dim = -1;           //划分轴 (0=x, 1=y, 2=z)
        int binIndex = -1;      //划分位置（在第 binIndex 个桶之后切分）
        float cost = (std::numeric_limits<float>::max)(); //SAH 代价
        AABB leftBounds;        //左侧合并后的 AABB
        AABB rightBounds;       //右侧合并后的 AABB
        int leftCount = 0;      //左侧 entity 数量
        int rightCount = 0;     //右侧 entity 数量
    };
public:
    //添加/删除/修改
    void addObject(NodeObjectHandle handle, const AABB& aabb);
    void removeObject(NodeObjectHandle handle);
    //批量删除接口，为了提高性能表现。
    void removeObjects(const std::vector<NodeObjectHandle>& handles);
    void modifyObject(NodeObjectHandle handle, const AABB& aabb);

    //判断 对象是否添加
    bool hasAdd(NodeObjectHandle handle) const;

    //获取m_totalObjEntityMap
    const std::unordered_map<NodeObjectHandle, BVHEntity>& getTotalObjEntityMap() const;

    //更新(query 时会自动调用)
    void updateBVHTree();

    //清空
    void clearBVHTree();

    //获取根节点
    const BVHNode* getRootNode() const;
    BVHNode* getRootNode();

    size_t getTotalEntityCount() const { return m_totalObjEntityMap.size(); }

    const AABB& getObjectAABB(NodeObjectHandle handle) const;

    //查询
    void query(const std::vector<Plane>& planes, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult);
    void query(const std::vector<PlaneD>& planes, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult);
    void query(const CylinderParams& cylinderStart, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult);
    void query(const std::vector<glm::vec2>& Points, const glm::mat4& viewProjMatrix, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult);
    void query(const Ray& ray, std::vector<NodeObjectHandle>& intersectResult);
    void query(const PlaneD& plane, std::vector<BVHEntity*>& intersectResult);
    template<typename T>
    void query(const std::vector<Plane>& planes, std::vector<T*>& allInResult, std::vector<T*>& intersectResult) {
        auto allInResult0 = (std::vector<NodeObjectHandle>*) & allInResult;
        auto intersectResult0 = (std::vector<NodeObjectHandle>*) & intersectResult;
        this->query(planes, *allInResult0, *intersectResult0);
    }
    template<typename T>
    void query(const std::vector<PlaneD>& planes, std::vector<T*>& allInResult, std::vector<T*>& intersectResult) {
        auto allInResult0 = (std::vector<NodeObjectHandle>*) & allInResult;
        auto intersectResult0 = (std::vector<NodeObjectHandle>*) & intersectResult;
        this->query(planes, *allInResult0, *intersectResult0);
    }
    template<typename T>
    void query(const Ray& ray, std::vector<T*>& intersectResult) {
        auto intersectResult0 = (std::vector<NodeObjectHandle> *) & intersectResult;
        this->query(ray, *intersectResult0);
    }

    template<typename T>
    void query(const CylinderParams& cylinder, std::vector<T*>& allInResult, std::vector<T*>& intersectResult) {
        auto allInResult0 = (std::vector<NodeObjectHandle>*) & allInResult;
        auto intersectResult0 = (std::vector<NodeObjectHandle>*) & intersectResult;
        this->query(cylinder, *allInResult0, *intersectResult0);
    }

    template<typename T>
    void query(const std::vector<glm::vec2>& Points, const glm::mat4& viewProjMatrix, std::vector<T*>& allInResult, std::vector<T*>& intersectResult) {
        auto allInResult0 = (std::vector<NodeObjectHandle>*) & allInResult;
        auto intersectResult0 = (std::vector<NodeObjectHandle>*) & intersectResult;
        this->query(Points, viewProjMatrix, *allInResult0, *intersectResult0);
    }



private:
    //这个结构体完全是为了缓存 entity.bounds.centroid() 的结果存在的. 只在 buildBVHTree 过程中使用.
    struct BVHEntityWithCentroid {
        explicit BVHEntityWithCentroid(BVHEntity& entity) : m_entity(&entity), m_centroid(entity.bounds.centroid()) {}

        BVHEntity* GetEntity() { return m_entity; }
        const glm::vec3& GetCentroid() const { return m_centroid; }

    private:
        BVHEntity* m_entity;
        glm::vec3 m_centroid;
    };

    //构建BVH树
    void buildBVHTree();
    //并行构建BVH树（三阶段：Morton分桶 → 并行建子树 → 顶层合并）
    void buildBVHTreeParallel();
    //将多棵子树递归合并为完整BVH
    BVHNode* mergeSubtrees(std::vector<SubtreeResult>& subtrees);
    //Binned SAH 构建BVH树（替代旧的 median split）
    BVHNode* buildBVHTreeSAH(BVHNode* father, std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd);
    //Binned SAH: 填充桶
    void fillBins(std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd, const AABB& centroidBounds, SAHBin bins[3][SAH_BIN_COUNT]);
    //Binned SAH: 寻找最优划分
    SAHSplitResult findBestSplit(const AABB& nodeBounds, const AABB& centroidBounds, const SAHBin bins[3][SAH_BIN_COUNT], int totalCount);
    //Binned SAH: 当找不到有效划分时，用中位数在最大轴上 fallback 划分
    int fallbackSplit(std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd, const AABB& centroidBounds);
    Intersection aabbIntersectFrustum(const AABB& aabb, const std::vector<Plane>& planes);
    std::vector<glm::vec2> projectAABBToScreen(const glm::vec3& aabbMin, const glm::vec3& aabbMax, const glm::mat4& viewProjMatrix) const;
    bool isPointInConcavePolygon(const glm::vec2& point, const std::vector<glm::vec2>& polygon) const;
    Intersection checkAABBVsConcavePolygon(const AABB& aabb, const std::vector<glm::vec2>& screenPolygon, const glm::mat4& viewProjMatrix);
    Intersection aabbIntersectFrustum(const AABB& aabb, const std::vector<PlaneD>& planes);
    Intersection aabbIntersectFrustum(const AABB& aabb, const CylinderParams& cylinder) const;
	bool aabbIntersectPlane(const AABB& aabb, const PlaneD& plane) const;
    BVHNode* buildBVHTree(BVHNode* node, std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd);
    void updateAABB(BVHNode* node, const AABB& aabb);
    void insertToNode(BVHNode* node, BVHEntity* entitiy);
    //SAH贪心下降插入：在内部节点处比较三种选择的SAH代价（送入左子树/送入右子树/合并下推），
    //选择最优路径下降到叶子节点，然后添加entity并在必要时分裂叶子。
    void insertToNodeSAH(BVHNode* node, BVHEntity* entity);
    //SAH插入辅助：合并当前节点的左右子树为一个子节点，并为新entity创建独立叶子兄弟节点。
    void addObjectPushdown(BVHNode* curNode, BVHEntity* entity);
    //SAH插入辅助：当叶子中entity数超过MAX_ENTITY_SIZE时，用SAH选择最优分裂轴并分裂。
    void splitLeafSAH(BVHNode* leaf);
    //SAH插入辅助：从指定节点向上逐层重新计算包围盒（仅扩展，不收缩）。
    void refitUpward(BVHNode* node);
    void getAllChildOfNode(BVHNode* node, std::vector<BVHEntity*>& entities);
    void getAllChildOfNode(BVHNode* node, std::vector<BVHEntityWithCentroid>& entities);
    void destructNode(BVHNode*& node);
    void query(const std::vector<Plane>& planes, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult);
    void query(const std::vector<PlaneD>& planes, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult);
    void query(const CylinderParams& cylinderPar, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult);
    void query(const Ray& ray, BVHNode* node, std::vector<BVHEntity*>& intersectResult);
	void query(const PlaneD& plane, BVHNode* node, std::vector<BVHEntity*>& intersectResult);
    void query(const std::vector<glm::vec2>& Points, const glm::mat4& viewProjMatrix, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult);
};
