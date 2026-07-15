#include <layer1/math/BVHTree.h>
#include <layer1/math/Plane.h>

#include <iostream>
#include <array>
//#define PRINT_ALL_BVH_LOG

BVHTree::BVHTree()
{}

BVHTree::~BVHTree()
{
    clearBVHTree();
}

void BVHTree::clearBVHTree()
{
    //释放所有资源m_totalObjEntityMap，递归释放所有node
    if (m_rootNode != nullptr)
    {
        destructNode(m_rootNode);
    }
    m_totalObjEntityMap.clear();
}

void BVHTree::destructNode(BVHNode*& node)
{
    if (node != nullptr)
    {
        //如果非叶子节点，则需先释放左右子节点，再释放当前节点。
        if (node->entities.size() != 0)
        {
            delete node;
            node = nullptr;
            return;
        }
        else
        {
            destructNode(node->left);
            destructNode(node->right);
            delete node;
            node = nullptr;
            return;
        }
    }
}

void BVHTree::buildBVHTree()
{
    if (m_totalObjEntityMap.empty())
    {
#ifdef PRINT_ALL_BVH_LOG
        std::cout << "BVH Tree waning: nothing add to BVH Tree when build!" << std::endl;
#endif
        return;
    }

    if (m_rootNode == nullptr)
    {
        if (m_totalObjEntityMap.size() >= PARALLEL_BUILD_THRESHOLD) {
            buildBVHTreeParallel();
        }
        else {
            std::vector<BVHEntityWithCentroid> tempTotalDataVec;
            tempTotalDataVec.reserve(m_totalObjEntityMap.size());
            for (auto& pair : m_totalObjEntityMap)
            {
                auto& entity = pair.second;
                tempTotalDataVec.emplace_back(entity); //这里计算了 centroid, 并存储在 BVHEntityWithCentroid 里.
            }
            m_rootNode = buildBVHTreeSAH(m_rootNode, tempTotalDataVec.begin(), tempTotalDataVec.end());
        }
    }
    else
    {
#ifdef PRINT_ALL_BVH_LOG
        std::cout << "BVH Tree waning : BVH tree has already been built !" << std::endl;
#endif
    }
}

//==================== 并行构建BVH树 ====================
//三阶段流程：
//  阶段1: Morton编码 + 排序 + 分桶 —— 将entity按空间位置分成N个桶
//  阶段2: 并行建子树 —— 每个桶独立调用buildBVHTree建子树
//  阶段3: 顶层合并 —— 将N棵子树的根用二叉树串起来
void BVHTree::buildBVHTreeParallel()
{
    if (m_totalObjEntityMap.empty()) return;

    //准备数据
    std::vector<BVHEntityWithCentroid> tempTotalDataVec;
    tempTotalDataVec.reserve(m_totalObjEntityMap.size());
    for (auto& pair : m_totalObjEntityMap) {
        tempTotalDataVec.emplace_back(pair.second);
    }

    size_t totalCount = tempTotalDataVec.size();

    //线程数或数据量太少，退化为串行
    unsigned int hwThreads = std::thread::hardware_concurrency();
    if (hwThreads <= 1 || totalCount < PARALLEL_BUILD_THRESHOLD) {
        m_rootNode = buildBVHTreeSAH(nullptr, tempTotalDataVec.begin(), tempTotalDataVec.end());
        return;
    }

    //==================== 阶段1: 计算 Morton 码 + 排序 + 分桶 ====================

    //1a. 计算所有 entity 质心的总包围盒
    AABB centroidBounds;
    for (auto& e : tempTotalDataVec) {
        centroidBounds.merge(e.GetCentroid());
    }

    //1b. 为每个 entity 计算 Morton 码
    std::vector<MortonEntity> mortonEntities(totalCount);
    for (size_t i = 0; i < totalCount; i++) {
        mortonEntities[i].mortonCode = AABB::mortonEncode(
            tempTotalDataVec[i].GetCentroid(),
            centroidBounds.minPt, centroidBounds.maxPt);
        mortonEntities[i].entityPtr = &tempTotalDataVec[i];
    }

    //按Morton码排序，基本保证了空间连续的 entity 会相邻
    std::sort(mortonEntities.begin(), mortonEntities.end());

    //分桶：每个桶包含大致相同数量的 entity
    //保证每个桶至少256个entity
    size_t numBuckets = (std::min)((size_t)hwThreads, totalCount / 256);
    numBuckets = (std::max)(numBuckets, (size_t)1);

    std::vector<size_t> bucketOffsets(numBuckets + 1);
    for (size_t i = 0; i <= numBuckets; i++) {
        bucketOffsets[i] = i * totalCount / numBuckets;
    }

    //==================== 阶段2: 并行建子树 ====================

    std::vector<SubtreeResult> subtrees(numBuckets);
    std::vector<std::future<void>> futures;
    futures.reserve(numBuckets);

    for (size_t b = 0; b < numBuckets; b++) {
        size_t beginIdx = bucketOffsets[b];
        size_t endIdx = bucketOffsets[b + 1];

        if (beginIdx >= endIdx) {
            subtrees[b].rootNode = nullptr;
            continue;
        }

        futures.push_back(std::async(std::launch::async, [&, beginIdx, endIdx, b]() {
            //从 MortonEntity 中提取原始 entity，构造子列表
            //注意：这里必须拷贝，因为每个线程需要独立的 vector（buildBVHTree 会修改 iterator）
            std::vector<BVHEntityWithCentroid> bucketEntities;
            bucketEntities.reserve(endIdx - beginIdx);
            for (size_t i = beginIdx; i < endIdx; i++) {
                bucketEntities.push_back(*mortonEntities[i].entityPtr);
            }

            //复用现有的 buildBVHTree，独立建子树
            subtrees[b].rootNode = buildBVHTreeSAH(nullptr, bucketEntities.begin(), bucketEntities.end());

            //记录子树包围盒
            if (subtrees[b].rootNode) {
                subtrees[b].bounds = subtrees[b].rootNode->bounds;
            }
            }));
    }

    //等待所有子树建完
    for (auto& f : futures) {
        f.get();
    }

    //过滤掉空桶
    std::vector<SubtreeResult> validSubtrees;
    validSubtrees.reserve(numBuckets);
    for (auto& st : subtrees) {
        if (st.rootNode != nullptr) {
            validSubtrees.push_back(std::move(st));
        }
    }

    //==================== 阶段3: 顶层合并 ====================

    if (validSubtrees.empty()) {
        m_rootNode = nullptr;
    }
    else if (validSubtrees.size() == 1) {
        m_rootNode = validSubtrees[0].rootNode;
        m_rootNode->father = nullptr;
    }
    else {
        m_rootNode = mergeSubtrees(validSubtrees);
    }
}

//将多棵子树递归合并为完整BVH
//策略：按子树根包围盒质心的最大轴排序后二分，递归合并
BVHNode* BVHTree::mergeSubtrees(std::vector<SubtreeResult>& subtrees)
{
    if (subtrees.size() == 1) {
        return subtrees[0].rootNode;
    }

    if (subtrees.size() == 2) {
        BVHNode* parent = new BVHNode();
        parent->left = subtrees[0].rootNode;
        parent->right = subtrees[1].rootNode;
        parent->left->father = parent;
        parent->right->father = parent;
        parent->bounds.merge(parent->left->bounds);
        parent->bounds.merge(parent->right->bounds);
        return parent;
    }

    //找到子树根包围盒质心范围最大的轴，按该轴排序后二分
    AABB centroidsBox;
    for (auto& st : subtrees) {
        centroidsBox.merge(st.bounds.centroid());
    }
    int dim = centroidsBox.maxExtent();

    std::sort(subtrees.begin(), subtrees.end(), [dim](const SubtreeResult& a, const SubtreeResult& b) {
        return a.bounds.centroid()[dim] < b.bounds.centroid()[dim];
        });

    auto mid = subtrees.size() / 2;
    std::vector<SubtreeResult> leftSubs(subtrees.begin(), subtrees.begin() + mid);
    std::vector<SubtreeResult> rightSubs(subtrees.begin() + mid, subtrees.end());

    BVHNode* leftTree = mergeSubtrees(leftSubs);
    BVHNode* rightTree = mergeSubtrees(rightSubs);

    BVHNode* parent = new BVHNode();
    parent->left = leftTree;
    parent->right = rightTree;
    leftTree->father = parent;
    rightTree->father = parent;
    parent->bounds.merge(leftTree->bounds);
    parent->bounds.merge(rightTree->bounds);

    return parent;
}

// 已经尝试过把递归转换成循环(需要增加一个栈来保存数据), 并使用广度优先搜索, 但是没有提升性能.
BVHNode* BVHTree::buildBVHTree(BVHNode* father, std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd)
{
    BVHNode* node = new BVHNode();

    auto numEntities = entitiesEnd - entitiesBegin;

    if (numEntities <= MAX_ENTITY_SIZE)
    {
        node->entities.clear();
        for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter)
        {
            auto entity = iter->GetEntity();
            node->bounds.merge(entity->bounds);
            entity->nodePtr = node;
            node->entities.push_back(entity);
        }
        node->father = father;
        return node;
    }
    else
    {
        AABB centroidBounds;
        for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter)
        {
            centroidBounds.merge(iter->GetCentroid());
        }
        int dim = centroidBounds.maxExtent();
        switch (dim)
        {
        case 0:
            std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd, [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return (f1.GetCentroid().x) < (f2.GetCentroid().x);
                });
            break;
        case 1:
            std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd, [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return (f1.GetCentroid().y) < (f2.GetCentroid().y);
                });
            break;
        case 2:
            std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd, [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return (f1.GetCentroid().z) < (f2.GetCentroid().z);
                });
            break;
        }

        //std::nth_element 不会为 std::vector 重新分配内存空间, 所以 entitiesBegin 和 entitiesEnd 可以继续使用.
        //不过 cppreference 上没有找到明确的说明, 这里可能有风险.
        auto middling = entitiesBegin + (numEntities / 2);
        assert(numEntities == ((middling - entitiesBegin) + (entitiesEnd - middling)));

        node->left = buildBVHTree(node, entitiesBegin, middling);
        node->right = buildBVHTree(node, middling, entitiesEnd);
        node->father = father;
        node->bounds.merge(node->left->bounds);
        node->bounds.merge(node->right->bounds);
    }

    return node;

}

//填充桶：将 entity 按质心位置分配到各轴的桶中
void BVHTree::fillBins(
    std::vector<BVHEntityWithCentroid>::iterator entitiesBegin,
    std::vector<BVHEntityWithCentroid>::iterator entitiesEnd,
    const AABB& centroidBounds,
    SAHBin bins[3][SAH_BIN_COUNT])
{
    //初始化所有桶
    for (int dim = 0; dim < 3; dim++) {
        for (int i = 0; i < SAH_BIN_COUNT; i++) {
            bins[dim][i].bounds = AABB();
            bins[dim][i].count = 0;
        }
    }

    //将每个 entity 按质心位置分配到对应桶
    for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter) {
        const glm::vec3& centroid = iter->GetCentroid();
        const AABB& entityBounds = iter->GetEntity()->bounds;

        for (int dim = 0; dim < 3; dim++) {
            //将质心归一化到 [0, SAH_BIN_COUNT-1]
            float extent = centroidBounds.maxPt[dim] - centroidBounds.minPt[dim];
            int binIdx = 0;
            if (extent > 0) {//这里是一定满足的！
                binIdx = (int)(((centroid[dim] - centroidBounds.minPt[dim]) / extent) * SAH_BIN_COUNT);
                //限制在 [0, SAH_BIN_COUNT-1]
                if (binIdx >= SAH_BIN_COUNT) binIdx = SAH_BIN_COUNT - 1;
                if (binIdx < 0) binIdx = 0;
            }
            bins[dim][binIdx].count++;
            bins[dim][binIdx].bounds.merge(entityBounds);
        }
    }
}

//寻找最优划分：枚举所有轴的所有桶边界，选 SAH 代价最小的
BVHTree::SAHSplitResult BVHTree::findBestSplit(
    const AABB& nodeBounds,
    const AABB& centroidBounds,
    const SAHBin bins[3][SAH_BIN_COUNT],
    int totalCount)
{
    SAHSplitResult bestSplit;
    float parentArea = nodeBounds.halfArea();
    if (parentArea <= 0) parentArea = 1e-10f; //防止除零
    float leafCost = /*SAH_INTERSECTION_COST * */totalCount;

    for (int dim = 0; dim < 3; dim++) {
        //从左到右累加，构建前缀信息
        AABB leftAccum[SAH_BIN_COUNT];
        int leftCountAccum[SAH_BIN_COUNT] = { 0 };

        leftAccum[0] = bins[dim][0].bounds;
        leftCountAccum[0] = bins[dim][0].count;
        for (int i = 1; i < SAH_BIN_COUNT; i++) {
            leftAccum[i] = leftAccum[i - 1]; //先拷贝上一轮的累加结果
            leftAccum[i].merge(bins[dim][i].bounds);//再把当前桶 merge 进去
            leftCountAccum[i] = leftCountAccum[i - 1] + bins[dim][i].count;
        }

        //从右到左累加，构建后缀信息
        AABB rightAccum[SAH_BIN_COUNT];
        int rightCountAccum[SAH_BIN_COUNT] = { 0 };

        rightAccum[SAH_BIN_COUNT - 1] = bins[dim][SAH_BIN_COUNT - 1].bounds;
        rightCountAccum[SAH_BIN_COUNT - 1] = bins[dim][SAH_BIN_COUNT - 1].count;
        for (int i = SAH_BIN_COUNT - 2; i >= 0; i--) {
            rightAccum[i] = rightAccum[i + 1];
            rightAccum[i].merge(bins[dim][i].bounds);
            rightCountAccum[i] = rightCountAccum[i + 1] + bins[dim][i].count;
        }

        //枚举所有桶边界：在第 i 个桶之后切分
        //左侧 = bins[0..i]，右侧 = bins[i+1..SAH_BIN_COUNT-1]
        for (int i = 0; i < SAH_BIN_COUNT - 1; i++) {
            int lCount = leftCountAccum[i];
            int rCount = rightCountAccum[i + 1];

            //两侧都不能为空
            if (lCount == 0 || rCount == 0) continue;

            float lArea = leftAccum[i].halfArea();
            float rArea = rightAccum[i + 1].halfArea();

            //抄SAH代价公式，这里与光线追踪的sah区分开，Entity不用进行三角面片与射线求交，所以SAH_INTERSECTION_COST为1
            float cost = SAH_TRAVERSAL_COST
                + (lArea / parentArea) * lCount /** SAH_INTERSECTION_COST*/
                + (rArea / parentArea) * rCount /** SAH_INTERSECTION_COST*/;

            if (cost < bestSplit.cost) {
                bestSplit.cost = cost;
                bestSplit.dim = dim;
                bestSplit.binIndex = i;
                bestSplit.leftBounds = leftAccum[i];
                bestSplit.rightBounds = rightAccum[i + 1];
                bestSplit.leftCount = lCount;
                bestSplit.rightCount = rCount;
            }
        }
    }

    //如果划分代价比不划分更差，标记为无效
    if (bestSplit.cost >= leafCost) {
        bestSplit.dim = -1;
    }

    return bestSplit;
}

//Fallback: 当 SAH 找不到有效划分时，用中位数在最大轴上切分
//返回划分点的 iterator 偏移
int BVHTree::fallbackSplit(
    std::vector<BVHEntityWithCentroid>::iterator entitiesBegin,
    std::vector<BVHEntityWithCentroid>::iterator entitiesEnd,
    const AABB& centroidBounds)
{
    int dim = centroidBounds.maxExtent();
    auto numEntities = entitiesEnd - entitiesBegin;

    switch (dim) {
    case 0:
        std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd,
            [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return f1.GetCentroid().x < f2.GetCentroid().x;
            });
        break;
    case 1:
        std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd,
            [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return f1.GetCentroid().y < f2.GetCentroid().y;
            });
        break;
    case 2:
        std::nth_element(entitiesBegin, entitiesBegin + (numEntities / 2), entitiesEnd,
            [](const BVHEntityWithCentroid& f1, const BVHEntityWithCentroid& f2) {
                return f1.GetCentroid().z < f2.GetCentroid().z;
            });
        break;
    }

    return (int)(numEntities / 2);
}

//Binned SAH 构建BVH树（核心递归函数）
BVHNode* BVHTree::buildBVHTreeSAH(BVHNode* father, std::vector<BVHEntityWithCentroid>::iterator entitiesBegin, std::vector<BVHEntityWithCentroid>::iterator entitiesEnd)
{
    BVHNode* node = new BVHNode();
    auto numEntities = entitiesEnd - entitiesBegin;

    //总包围盒
    AABB nodeBounds;
    for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter) {
        nodeBounds.merge(iter->GetEntity()->bounds);
    }
    node->bounds = nodeBounds;

    //叶子节点条件：entity 数量 <= MAX_ENTITY_SIZE
    if (numEntities <= MAX_ENTITY_SIZE) {
        node->entities.clear();
        for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter) {
            auto entity = iter->GetEntity();
            entity->nodePtr = node;
            node->entities.push_back(entity);
        }
        node->father = father;
        return node;
    }

    //计算质心包围盒
    AABB centroidBounds;
    for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter) {
        centroidBounds.merge(iter->GetCentroid());
    }

    //如果质心包围盒退化（所有质心重合），直接作为叶子
    if (centroidBounds.maxExtent() < 0 || centroidBounds.diagonal() == glm::vec3(0)) {
        node->entities.clear();
        for (auto iter = entitiesBegin; iter != entitiesEnd; ++iter) {
            auto entity = iter->GetEntity();
            entity->nodePtr = node;
            node->entities.push_back(entity);
        }
        node->father = father;
        return node;
    }

    //==================== Binned SAH 核心逻辑 ====================

    //步骤1: 填充桶
    SAHBin bins[3][SAH_BIN_COUNT];
    fillBins(entitiesBegin, entitiesEnd, centroidBounds, bins);

    //步骤2: 寻找最优划分
    SAHSplitResult split = findBestSplit(nodeBounds, centroidBounds, bins, (int)numEntities);

    //步骤3: 根据划分结果分组 entity
    auto middling = entitiesEnd; //默认不划分

    if (split.dim >= 0) {
        //SAH 找到了有效划分，按桶边界分组
        //将 entity 按划分轴和桶索引分成左右两组
        float extent = centroidBounds.maxPt[split.dim] - centroidBounds.minPt[split.dim];
        middling = std::partition(entitiesBegin, entitiesEnd,
            [split, &centroidBounds, extent](const BVHEntityWithCentroid& e) {
                float pos = e.GetCentroid()[split.dim];
                int binIdx = 0;
                if (extent > 0) {
                    binIdx = (int)((pos - centroidBounds.minPt[split.dim]) / extent * SAH_BIN_COUNT);
                    if (binIdx >= SAH_BIN_COUNT) binIdx = SAH_BIN_COUNT - 1;
                    if (binIdx < 0) binIdx = 0;
                }
                return binIdx <= split.binIndex;
            });

        //安全检查：确保两侧都不为空
        if (middling == entitiesBegin || middling == entitiesEnd) {
            //partition 失败（边界情况），用 fallback
            int midOffset = fallbackSplit(entitiesBegin, entitiesEnd, centroidBounds);
            middling = entitiesBegin + midOffset;
        }
    }
    else {
        //SAH 认为不划分更优，但 entity 数量 > MAX_ENTITY_SIZE
        //必须强制划分，用 fallback 中位数切分
        int midOffset = fallbackSplit(entitiesBegin, entitiesEnd, centroidBounds);
        middling = entitiesBegin + midOffset;
    }

    //递归构建左右子树
    node->left = buildBVHTreeSAH(node, entitiesBegin, middling);
    node->right = buildBVHTreeSAH(node, middling, entitiesEnd);
    node->father = father;

    //重新计算 node->bounds（子树 bounds 可能因为 entity 分组更精确）
    node->bounds.merge(node->left->bounds);
    node->bounds.merge(node->right->bounds);

    return node;
}

void BVHTree::modifyObject(NodeObjectHandle handle, const AABB& aabb)
{
    removeObject(handle);
    addObject(handle, aabb);
}

void BVHTree::removeObject(NodeObjectHandle handle)
{
    //如果树还没构建，则不需要处理节点变化，直接从m_addDataMap 或 m_totalObjEntityMap中进行删除。
    if (m_rootNode == nullptr)
    {
        //在m_totalObjEntityMap里面找
        if (!m_totalObjEntityMap.empty()) {
            auto it = m_totalObjEntityMap.find(handle);
            if (it != m_totalObjEntityMap.end()) {
                m_totalObjEntityMap.erase(it);
                return;
            }
        }
        //两个都没找到，return
        return;
    }

    //BVHEntity* entity;
    BVHNode* node = nullptr;
    BVHEntity* entityPtr = nullptr;

    auto it = m_totalObjEntityMap.find(handle);

    //在m_totalObjEntityMap中查找
    if (it != m_totalObjEntityMap.end())
    {
        entityPtr = &(it->second);
        node = it->second.nodePtr;
    }
    else
    {
#ifdef PRINT_ALL_BVH_LOG
        std::cout << "BVH Tree warning : please check the object which you want to remove from BVH Tree!" << std::endl;
#endif
        return;
    }

    if (node == nullptr || entityPtr == nullptr)
    {
#ifdef PRINT_ALL_BVH_LOG
        std::cout << "BVH Tree warning:  can not find the node that you want to remove !" << std::endl;
#endif
        return;
    }

    if (node == m_rootNode)
    {
        // 优化写法 O(1)：把要删的换到末尾，然后 pop
        auto it2 = std::find(node->entities.begin(), node->entities.end(), entityPtr);
        if (it2 != node->entities.end()) {
            *it2 = std::move(node->entities.back());
            node->entities.pop_back();
        }
        if (entityPtr->bounds.constructBorder(node->bounds))
        {
            AABB aabb;
            for (size_t i = 0; i < node->entities.size(); i++)
            {
                aabb.merge(node->entities[i]->bounds);
            }
            //如果该aabb发生变化，则更新aabb
            if (!aabb.equal(node->bounds)) {
                updateAABB(node, aabb);
            }
        }
        m_totalObjEntityMap.erase(it);
        if (m_totalObjEntityMap.empty()) {
            destructNode(m_rootNode);
        }
        return;
    }
    //删除该entity
    auto it3 = std::find(node->entities.begin(), node->entities.end(), entityPtr);
    if (it3 != node->entities.end()) {
        *it3 = std::move(node->entities.back());
        node->entities.pop_back();
    }
    //如果该entity的aabb构成了当前节点aabb的边界，去掉该节点后，则当前节点aabb会发生变化
    if (entityPtr->bounds.constructBorder(node->bounds))
    {
        AABB aabb;
        for (size_t i = 0; i < node->entities.size(); i++)
        {
            aabb.merge(node->entities[i]->bounds);
        }
        if (!aabb.equal(node->bounds))
        {
            updateAABB(node, aabb);
        }
    }
    //从m_totalObjEntityMap中删除
    m_totalObjEntityMap.erase(it);


    //如果删除后节点entities为空，则处理空节点，
    if (node->entities.empty())
    {
        BVHNode* brother = node->getBrotherNode();

        //这里分为两步：
        //1 将brother节点的father指向上上级node
        //2 将上上级node的left或者right 指向brother
        //3 释放掉node和其father节点
        if (brother == nullptr) assert(false);//只有在node为rootNode的情况下才会brother为空，上面已经处理
        if (node->father->father != nullptr)
        {
            brother->father = node->father->father;
            if (node->father->father->left == node->father)
            {
                node->father->father->left = brother;
            }
            else
            {
                node->father->father->right = brother;
            }
            delete node->father;
            delete node;
        }
        else
        {
            delete m_rootNode;
            delete node;
            m_rootNode = brother;
            m_rootNode->father = nullptr;
        }
    }
}

void BVHTree::removeObjects(const std::vector<NodeObjectHandle>& handles) {
    for (const auto& ent : handles) {
        removeObject(ent);
    }
    //destructNode(m_rootNode);
}


void BVHTree::updateAABB(BVHNode* node, const AABB& aabb)
{
    if (node != nullptr)
    {
        if (!aabb.equal(node->bounds))
        {
            node->bounds = aabb;
            //否则node为根节点,根节点不需要更新其父节点了
            if (node->getBrotherNode() != nullptr)
            {
                AABB aabb1;
                aabb1.merge(node->bounds);
                aabb1.merge(node->getBrotherNode()->bounds);
                //除了更新当前节点的aabb外还要递归更新其父节点的aabb
                if (!aabb1.equal(node->father->bounds))
                {
                    updateAABB(node->father, aabb1);
                }
            }
        }
    }
}

void BVHTree::updateBVHTree()
{
    if (m_rootNode == nullptr)
    {
        if (!m_totalObjEntityMap.empty())
        {
            buildBVHTree();
        }
        else {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVH Tree warning : nothing is in BVH tree,can not build!" << std::endl;
#endif
        }
    }
}

const BVHNode* BVHTree::getRootNode() const {
    return ((BVHTree*)this)->getRootNode();
}

BVHNode* BVHTree::getRootNode() {
    //由于外部使用时获取最大场景包围盒，不一定会先update,所以这里强制update
    ((BVHTree*)this)->updateBVHTree();
    return m_rootNode;
}

bool BVHTree::hasAdd(NodeObjectHandle handle) const {
    auto itorFind = m_totalObjEntityMap.find(handle);
    return itorFind != m_totalObjEntityMap.end();
}

const std::unordered_map<NodeObjectHandle, BVHEntity>& BVHTree::getTotalObjEntityMap() const
{
    return m_totalObjEntityMap;
}

//addObject 目前是性能瓶颈, 但是 m_rootNode == nullptr 分支看起来已经没有优化空间了.
void BVHTree::addObject(NodeObjectHandle handle, const AABB& aabb)
{
    BVHEntity entity;
    entity.object = handle;
    entity.bounds = aabb;

    //如果未建树，则只需在m_totalObjEntityMap中添加
    if (m_rootNode == nullptr)
    {
        //如果还没构建BVH树，则直接加到m_totalObjEntityMap中
        //如果已经有了，默认更新aabb
        m_totalObjEntityMap.insert_or_assign(handle, entity);
    }
    //如果已经建树，则需同时检查m_totalObjEntityMap和m_addDataMap中是否能找到handle,找不到时才insert 到 m_addDataMap中
    else
    {
        auto itor = m_totalObjEntityMap.find(handle);
        if (itor != m_totalObjEntityMap.end()) {
            //如果aabb未发生变化，则不做处理，直接返回。否则需要从m_totalObjEntityMap中删除后再插入到m_addDataMap
            if (!itor->second.bounds.equal(aabb)) {
                removeObject(handle);
            }
            else {
                return;
            }
        }
        auto itor2 = m_totalObjEntityMap.insert_or_assign(handle, std::move(entity));

        insertToNodeSAH(m_rootNode, &(itor2.first->second));
        //m_addDataMap[handle] = aabb;
    }
}

void BVHTree::insertToNode(BVHNode* node, BVHEntity* entitiy)
{
    //左子节点包含该包围盒，且左子节点的子节点也包含该包围盒，则交给左子节点处理
    //右子节点包含该包围盒，且右子节点的子节点也包含该包围盒，则交给右子节点处理
    bool leftContain = node->left->bounds.contain(entitiy->bounds);
    bool rightContain = node->right->bounds.contain(entitiy->bounds);
    bool leftChildContain = node->left->childContain(entitiy->bounds);
    bool rightChildContain = node->right->childContain(entitiy->bounds);
    bool canAddToLeft = leftContain && (node->left->hasChild()) && leftChildContain;
    bool canAddToRight = rightContain && (node->right->hasChild()) && rightChildContain;

    //m_lastAddLeft为标志位，避免每次都加到同一边的节点中导致bvh树不平衡。
    //如果既可以加到左边 又可以加到右边，则发挥该标志位作用
    if ((canAddToLeft && !canAddToRight) || ((canAddToLeft && canAddToRight) && !m_lastAddLeft))
    {
        insertToNode(node->left, entitiy);
        m_lastAddLeft = true;
    }
    else if ((!canAddToLeft && canAddToRight) || ((canAddToLeft && canAddToRight) && m_lastAddLeft))
    {
        insertToNode(node->right, entitiy);
        m_lastAddLeft = false;
    }
    else if (leftContain)
    {
        //左边包含，且不能往左边的子节点递归下去，则将左子节点重新建树
        if ((node->left->hasChild() && !leftChildContain) || !node->left->hasChild())
        {
            std::vector<BVHEntityWithCentroid> tempvec;
            getAllChildOfNode(node->left, tempvec);
            tempvec.emplace_back(*entitiy); //这里计算了 centroid, 并存储在 BVHEntityWithCentroid 里.
            destructNode(node->left);
            node->left = buildBVHTreeSAH(node, tempvec.begin(), tempvec.end());
        }
    }
    else if (rightContain)
    {
        //右边包含，且不能往右边的子节点递归下去，则将右子节点重新建树
        if ((node->right->hasChild() && !rightChildContain) || !node->right->hasChild())
        {

            std::vector<BVHEntityWithCentroid> tempvec;
            getAllChildOfNode(node->right, tempvec);
            tempvec.emplace_back(*entitiy); //这里计算了 centroid, 并存储在 BVHEntityWithCentroid 里.
            destructNode(node->right);
            node->right = buildBVHTreeSAH(node, tempvec.begin(), tempvec.end());

        }
    }
    //如果走到这里说明逻辑出错了。
    else { assert(false); }
}

void BVHTree::refitUpward(BVHNode* node)
{
    BVHNode* cur = node;
    while (cur != nullptr && cur->father != nullptr) {
        AABB newBounds;
        newBounds.merge(cur->bounds);
        //兄弟节点的包围盒也需要纳入
        BVHNode* sibling = (cur->father->left == cur) ? cur->father->right : cur->father->left;
        if (sibling != nullptr) {
            newBounds.merge(sibling->bounds);
        }
        if (newBounds == cur->father->bounds) {
            break; //父节点包围盒未变化，停止向上传播
        }
        cur->father->bounds = newBounds;
        cur = cur->father;
    }
}

//==================== SAH 贪心下降插入 ====================
//在每个内部节点处评估三种选择的SAH代价：
//1. 送入左子树
//2. 送入右子树
//3. 合并下推仅当远优于 1/2 时采用）
//选择代价最小的路径持续下降，直到抵达叶子节点。

void BVHTree::insertToNodeSAH(BVHNode* node, BVHEntity* entity)
{
    BVHNode* curNode = node;

    //====== 阶段1：从当前节点贪心下降，找到最佳叶子 ======
    //curNode->entities.empty() 说明是内部节点（有左右子节点）
    while (curNode->entities.empty() && curNode->left != nullptr && curNode->right != nullptr) {
        BVHNode* left = curNode->left;
        BVHNode* right = curNode->right;

        float leftSAH = left->bounds.halfArea();
        float rightSAH = right->bounds.halfArea();

        //左子树扩展后的包围盒
        AABB leftExpanded = left->bounds;
        leftExpanded.merge(entity->bounds);

        //右子树扩展后的包围盒
        AABB rightExpanded = right->bounds;
        rightExpanded.merge(entity->bounds);

        //三种选择的SAH代价：
        float sendLeftSAH = leftExpanded.halfArea() + rightSAH;
        float sendRightSAH = leftSAH + rightExpanded.halfArea();

        AABB mergedBounds = left->bounds;
        mergedBounds.merge(right->bounds);
        float mergedSAH = mergedBounds.halfArea() + entity->bounds.halfArea();

        //合并下推的折扣因子：只有当合并代价远小于送入子树时才采用，
        //因为合并下推会改变树结构，代价较高。
        const float MERGE_DISCOUNT = 0.3f;

        if (mergedSAH < (std::min)(sendLeftSAH, sendRightSAH) * MERGE_DISCOUNT) {
            //合并左右子树为一个子节点，新entity作为兄弟叶子
            addObjectPushdown(curNode, entity);
            return;
        }
        else {
            //向 SAH 代价更小的子树方向继续下降
            if (sendLeftSAH <= sendRightSAH) {
                curNode = left;
            }
            else {
                curNode = right;
            }
        }
    }

    //====== 阶段2：到达叶子节点，添加 entity ======
    curNode->entities.push_back(entity);
    entity->nodePtr = curNode;
    curNode->bounds.merge(entity->bounds);
    const auto& curBounds = curNode->bounds;
    //向上逐层更新包围盒
    refitUpward(curNode);

    //如果叶子中 entity 数超过上限，则用 SAH 分裂
    if ((int)curNode->entities.size() > MAX_ENTITY_SIZE) {
        splitLeafSAH(curNode);
        assert(curNode->bounds.equal(curBounds));
        //分裂后子树结构变化，再次向上传播以确保包围盒精确
        refitUpward(curNode);
    }
}

void BVHTree::addObjectPushdown(BVHNode* curNode, BVHEntity* entity)
{
    //将当前节点的左右子树合并为一个中间子节点
    BVHNode* mergedSubnode = new BVHNode();
    mergedSubnode->left = curNode->left;
    mergedSubnode->right = curNode->right;
    mergedSubnode->father = curNode;
    mergedSubnode->bounds = curNode->left->bounds;
    mergedSubnode->bounds.merge(curNode->right->bounds);
    curNode->left->father = mergedSubnode;
    curNode->right->father = mergedSubnode;

    //为新 entity 创建独立叶子节点
    BVHNode* newLeaf = new BVHNode();
    newLeaf->entities.push_back(entity);
    entity->nodePtr = newLeaf;
    newLeaf->bounds = entity->bounds;
    newLeaf->father = curNode;

    //将合并子树和新叶子挂到当前节点下
    curNode->left = mergedSubnode;
    curNode->right = newLeaf;

    //重新计算 curNode 自身的包围盒（从两个新子节点的并集）
    curNode->bounds = AABB();
    curNode->bounds.merge(mergedSubnode->bounds);
    curNode->bounds.merge(newLeaf->bounds);

    //从当前节点向上传播包围盒更新
    refitUpward(curNode);
}

void BVHTree::splitLeafSAH(BVHNode* leaf)
{
    auto& entities = leaf->entities;
    int count = (int)entities.size();
    if (count <= MAX_ENTITY_SIZE) return;

    int center = count / 2;

    //对 三个轴分别尝试排序+分裂，选择 SAH 代价最小的轴
    float bestCost = (std::numeric_limits<float>::max)();
    int   bestAxis = 0;

    auto sortAndEval = [&](int axis) {
        std::vector<BVHEntity*> sorted = entities;
        switch (axis) {
        case 0:
            std::sort(sorted.begin(), sorted.end(),
                [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().x < b->bounds.centroid().x; });
            break;
        case 1:
            std::sort(sorted.begin(), sorted.end(),
                [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().y < b->bounds.centroid().y; });
            break;
        case 2:
            std::sort(sorted.begin(), sorted.end(),
                [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().z < b->bounds.centroid().z; });
            break;
        }

        AABB leftBox, rightBox;
        for (int i = 0; i < center; i++)
            leftBox.merge(sorted[i]->bounds);

        for (int i = center; i < count; i++)
            rightBox.merge(sorted[i]->bounds);

        //SAH 代价 = 左半包围盒半表面积 × 左半数量 + 右半包围盒半表面积 × 右半数量
        float cost = leftBox.halfArea() * center + rightBox.halfArea() * (count - center);
        if (cost < bestCost) {
            bestCost = cost;
            bestAxis = axis;
        }
    };

    sortAndEval(0);
    sortAndEval(1);
    sortAndEval(2);

    //按最优轴排序
    switch (bestAxis) {
    case 0:
        std::sort(entities.begin(), entities.end(),
            [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().x < b->bounds.centroid().x; });
        break;
    case 1:
        std::sort(entities.begin(), entities.end(),
            [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().y < b->bounds.centroid().y; });
        break;
    case 2:
        std::sort(entities.begin(), entities.end(),
            [](BVHEntity* a, BVHEntity* b) { return a->bounds.centroid().z < b->bounds.centroid().z; });
        break;
    }

    //创建左右子节点
    BVHNode* leftChild = new BVHNode();
    BVHNode* rightChild = new BVHNode();

    leftChild->father = leaf;
    rightChild->father = leaf;

    for (int i = 0; i < center; i++) {
        leftChild->entities.push_back(entities[i]);
        entities[i]->nodePtr = leftChild;
        leftChild->bounds.merge(entities[i]->bounds);
    }
    for (int i = center; i < count; i++) {
        rightChild->entities.push_back(entities[i]);
        entities[i]->nodePtr = rightChild;
        rightChild->bounds.merge(entities[i]->bounds);
    }

    //叶子 → 内部节点：清空 entities，挂上左右子节点
    leaf->entities.clear();
    leaf->left = leftChild;
    leaf->right = rightChild;

    //重新计算叶子的包围盒（现在等于左右子节点的并集）
    leaf->bounds = AABB();
    leaf->bounds.merge(leftChild->bounds);
    leaf->bounds.merge(rightChild->bounds);

    //如果子节点 entity 数仍然超限，递归分裂
    if ((int)leftChild->entities.size() > MAX_ENTITY_SIZE) {
        splitLeafSAH(leftChild);
    }
    if ((int)rightChild->entities.size() > MAX_ENTITY_SIZE) {
        splitLeafSAH(rightChild);
    }
}

void BVHTree::getAllChildOfNode(BVHNode* node, std::vector<BVHEntity*>& entities)
{
    if (nullptr == node)
        return;

    if (node->entities.size() != 0)
    {
        entities.insert(entities.end(), node->entities.begin(), node->entities.end());
    }
    else
    {
        getAllChildOfNode(node->left, entities);
        getAllChildOfNode(node->right, entities);
    }
}

void BVHTree::getAllChildOfNode(BVHNode* node, std::vector<BVHEntityWithCentroid>& entities)
{
    if (nullptr == node)
        return;

    if (node->entities.size() != 0)
    {
        for (auto entity : node->entities)
        {
            entities.emplace_back(*entity);
        }
    }
    else
    {
        getAllChildOfNode(node->left, entities);
        getAllChildOfNode(node->right, entities);
    }
}

void BVHTree::query(const Ray& ray, std::vector<NodeObjectHandle>& intersectResult)
{
    //查询前如果 没建树 或者 需要更新 则先刷新树。
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVH Tree Error:BVH tree is Empty! can not query!" << std::endl;
#endif
            return;
        }
    }

    std::vector<BVHEntity*> interRes;

    query(ray, m_rootNode, interRes);

    for (size_t i = 0; i < interRes.size(); i++)
    {
        intersectResult.emplace_back(interRes[i]->object);
    }
}

void BVHTree::query(const Ray& ray, BVHNode* node, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;

    if (node->bounds.intersectRay(ray))
    {
        if (!node->entities.empty())
        {
            for (const auto& entity : node->entities)
            {
                if (entity->bounds.intersectRay(ray))
                {
                    intersectResult.emplace_back(entity);
                }
            }
            return;
        }

        if (node->left != nullptr)
        {
            query(ray, node->left, intersectResult);
        }

        if (node->right != nullptr)
        {
            query(ray, node->right, intersectResult);
        }
    }
}

void BVHTree::query(const std::vector<Plane>& planes, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult)
{
    //查询前需要保证树是最新的
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVHTree Error:BVH tree is Empty!" << std::endl;
#endif
            return;
        }
    }

    std::vector<BVHEntity*> allInRes;
    std::vector<BVHEntity*> interRes;
    //这里真正执行查询求交过程。
    query(planes, m_rootNode, allInRes, interRes);

    for (size_t i = 0; i < allInRes.size(); i++)
    {
        allInResult.emplace_back(allInRes[i]->object);
    }

    for (size_t i = 0; i < interRes.size(); i++)
    {
        intersectResult.emplace_back(interRes[i]->object);
    }
}

void BVHTree::query(const std::vector<Plane>& planes, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;
    //若不相交 返回
    Intersection res = aabbIntersectFrustum(node->bounds, planes);
    if (Intersection::OUTSIDE == res)
        return;

    //若相交，则递归判断其子节点与planes的关系，并最终判断每一个entity的aabb是否与planes相交
    if (res == Intersection::INTERSECTS)
    {
        //若没有子节点（本身为叶子节点）则判断每一个entity的aabb是否与planes相交
        if (!node->entities.empty())
        {
            for (const auto& entity : node->entities)
            {
                Intersection resEntiy = aabbIntersectFrustum(entity->bounds, planes);
                if (resEntiy == Intersection::OUTSIDE) {
                    continue;
                }
                else if (resEntiy == Intersection::INSIDE) {
                    allInResult.emplace_back(entity);
                }
                else {
                    intersectResult.emplace_back(entity);
                }
            }
            return;
        }
        else
        {
            query(planes, node->left, allInResult, intersectResult);
            query(planes, node->right, allInResult, intersectResult);
        }
    }
    //若节点的aabb完全在planes范围内，则将node下所有entity作为结果返回
    else
    {
        if (node->left == nullptr || node->right == nullptr)
        {
            allInResult.insert(allInResult.end(), node->entities.begin(), node->entities.end());
        }
        else
        {
            getAllChildOfNode(node->left, allInResult);
            getAllChildOfNode(node->right, allInResult);
        }
    }
}

void BVHTree::query(const std::vector<PlaneD>& planes, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult)
{
    //查询前需要保证树是最新的，没有建树先建树，添加了新entity则更新树
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVHTree Error:BVH tree is Empty!" << std::endl;
#endif
            return;
        }
    }

    std::vector<BVHEntity*> allInRes;
    std::vector<BVHEntity*> interRes;
    //这里真正执行查询求交过程。
    query(planes, m_rootNode, allInRes, interRes);

    for (size_t i = 0; i < allInRes.size(); i++)
    {
        allInResult.emplace_back(allInRes[i]->object);
    }

    for (size_t i = 0; i < interRes.size(); i++)
    {
        intersectResult.emplace_back(interRes[i]->object);
    }
}

void BVHTree::query(const std::vector<PlaneD>& planes, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;
    //若不相交 返回
    Intersection res = aabbIntersectFrustum(node->bounds, planes);
    if (Intersection::OUTSIDE == res)
        return;
    //若相交，则递归判断其子节点与planes的关系，并最终判断每一个entity的aabb是否与planes相交
    if (res == Intersection::INTERSECTS)
    {
        //若没有子节点（本身为叶子节点）则判断每一个entity的aabb是否与planes相交
        if (!node->entities.empty())
        {
            for (const auto& entity : node->entities)
            {
                Intersection resEntiy = aabbIntersectFrustum(entity->bounds, planes);
                if (resEntiy == Intersection::OUTSIDE) {
                    continue;
                }
                else if (resEntiy == Intersection::INSIDE) {
                    allInResult.emplace_back(entity);
                }
                else {
                    intersectResult.emplace_back(entity);
                }
            }
            return;
        }
        else
        {
            query(planes, node->left, allInResult, intersectResult);
            query(planes, node->right, allInResult, intersectResult);
        }
    }
    //若节点的aabb完全在planes范围内，则将node下所有entity作为结果返回
    else
    {
        if (node->left == nullptr || node->right == nullptr)
        {
            allInResult.insert(allInResult.end(), node->entities.begin(), node->entities.end());
        }
        else
        {
            getAllChildOfNode(node->left, allInResult);
            getAllChildOfNode(node->right, allInResult);
        }
    }
}

void BVHTree::query(const CylinderParams& cylinderPar, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;
    //若不相交 返回
    Intersection res = aabbIntersectFrustum(node->bounds, cylinderPar);
    if (Intersection::OUTSIDE == res)
        return;
    //若相交，则递归判断其子节点与planes的关系，并最终判断每一个entity的aabb是否与planes相交
    if (res == Intersection::INTERSECTS)
    {
        //若没有子节点（本身为叶子节点）则判断每一个entity的aabb是否与planes相交
        if (!node->entities.empty())
        {
            for (const auto& entity : node->entities)
            {
                Intersection resEntiy = aabbIntersectFrustum(entity->bounds, cylinderPar);
                if (resEntiy == Intersection::OUTSIDE) {
                    continue;
                }
                else if (resEntiy == Intersection::INSIDE) {
                    allInResult.emplace_back(entity);
                }
                else {
                    intersectResult.emplace_back(entity);
                }
            }
            return;
        }
        else
        {
            query(cylinderPar, node->left, allInResult, intersectResult);
            query(cylinderPar, node->right, allInResult, intersectResult);
        }
    }
    //若节点的aabb完全在planes范围内，则将node下所有entity作为结果返回
    else
    {
        if (node->left == nullptr || node->right == nullptr)
        {
            allInResult.insert(allInResult.end(), node->entities.begin(), node->entities.end());
        }
        else
        {
            getAllChildOfNode(node->left, allInResult);
            getAllChildOfNode(node->right, allInResult);
        }
    }
}

void BVHTree::query(const PlaneD& plane, BVHNode* node, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;
    //若不相交 返回
    bool ret = aabbIntersectPlane(node->bounds, plane);
    if (!ret)
        return;

    if (!node->entities.empty())
    {
        for (auto& entity : node->entities)
        {
            ret = aabbIntersectPlane(entity->bounds, plane);
            if (!ret) {
                continue;
            }
            intersectResult.push_back(entity);

        }
        return;
    }
    else
    {
        query(plane, node->left, intersectResult);
        query(plane, node->right, intersectResult);
    }
}

void BVHTree::query(const std::vector<glm::vec2>& Points, const glm::mat4& viewProjMatrix, BVHNode* node, std::vector<BVHEntity*>& allInResult, std::vector<BVHEntity*>& intersectResult)
{
    if (nullptr == node)
        return;
    //若不相交 返回

    Intersection res = checkAABBVsConcavePolygon(node->bounds, Points, viewProjMatrix);
    if (Intersection::OUTSIDE == res)
        return;
    //若相交，则递归判断其子节点与planes的关系，并最终判断每一个entity的aabb是否与planes相交
    if (res == Intersection::INTERSECTS)
    {
        //若没有子节点（本身为叶子节点）则判断每一个entity的aabb是否与planes相交
        if (!node->entities.empty())
        {
            for (const auto& entity : node->entities)
            {
                Intersection resEntiy = checkAABBVsConcavePolygon(entity->bounds, Points, viewProjMatrix);
                if (resEntiy == Intersection::OUTSIDE) {
                    continue;
                }
                else if (resEntiy == Intersection::INSIDE) {
                    allInResult.emplace_back(entity);
                }
                else {
                    intersectResult.emplace_back(entity);
                }
            }
            return;
        }
        else
        {
            query(Points, viewProjMatrix, node->left, allInResult, intersectResult);
            query(Points, viewProjMatrix, node->right, allInResult, intersectResult);
        }
    }
    //若节点的aabb完全在planes范围内，则将node下所有entity作为结果返回
    else
    {
        if (node->left == nullptr || node->right == nullptr)
        {
            allInResult.insert(allInResult.end(), node->entities.begin(), node->entities.end());
        }
        else
        {
            getAllChildOfNode(node->left, allInResult);
            getAllChildOfNode(node->right, allInResult);
        }
    }
}

void BVHTree::query(const CylinderParams& cylinderPar, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult)
{
    //查询前需要保证树是最新的，没有建树先建树，添加了新entity则更新树
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVHTree Error:BVH tree is Empty!" << std::endl;
#endif
            return;
        }
    }

    std::vector<BVHEntity*> allInRes;
    std::vector<BVHEntity*> interRes;
    //这里真正执行查询求交过程。
    query(cylinderPar, m_rootNode, allInRes, interRes);

    for (size_t i = 0; i < allInRes.size(); i++)
    {
        allInResult.emplace_back(allInRes[i]->object);
    }

    for (size_t i = 0; i < interRes.size(); i++)
    {
        intersectResult.emplace_back(interRes[i]->object);
    }

}

void BVHTree::query(const std::vector<glm::vec2>& Points, const glm::mat4& viewProjMatrix, std::vector<NodeObjectHandle>& allInResult, std::vector<NodeObjectHandle>& intersectResult)
{
    //查询前需要保证树是最新的，没有建树先建树，添加了新entity则更新树
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
#ifdef PRINT_ALL_BVH_LOG
            std::cout << "BVHTree Error:BVH tree is Empty!" << std::endl;
#endif
            return;
        }
    }

    std::vector<BVHEntity*> allInRes;
    std::vector<BVHEntity*> interRes;
    //这里真正执行查询求交过程。
    query(Points, viewProjMatrix, m_rootNode, allInRes, interRes);

    for (size_t i = 0; i < allInRes.size(); i++)
    {
        allInResult.emplace_back(allInRes[i]->object);
    }

    for (size_t i = 0; i < interRes.size(); i++)
    {
        intersectResult.emplace_back(interRes[i]->object);
    }

}


void BVHTree::query(const PlaneD& plane, std::vector<BVHEntity*>& intersectResult)
{
    //查询前需要保证树是最新的
    if (m_rootNode == nullptr)
    {
        updateBVHTree();
        if (m_rootNode == nullptr)
        {
            std::cout << "BVHTree Error:BVH tree is Empty!" << std::endl;
            return;
        }
    }
    if (!intersectResult.empty())
    {
        intersectResult.clear();
    }
    //这里真正执行查询求交过程
    query(plane, m_rootNode, intersectResult);
}

const AABB& BVHTree::getObjectAABB(NodeObjectHandle handle) const
{
    const static AABB s_invalidBox;

    auto iter = m_totalObjEntityMap.find(handle);
    if (iter != m_totalObjEntityMap.end())
    {
        return iter->second.bounds;
    }

    return s_invalidBox;
}

Intersection BVHTree::aabbIntersectFrustum(const AABB& aabb, const std::vector<Plane>& planes)
{
    glm::vec3 temp = aabb.centroid();
    glm::vec3 center{ temp.x, temp.y, temp.z };

    glm::vec3 edge{ center[0] - aabb.minPt.x, center[1] - aabb.minPt.y, center[2] - aabb.minPt.z };
    bool allInside = true;

    for (const auto& plane : planes) {
        auto dist = glm::dot(plane.m_normal, center) + plane.m_d; //包围盒中心到平面的距离（有符号）
        float absDist = glm::dot(plane.m_absNormal, edge); //从包围盒中心出发的最大投影距离（正值）

        if (dist < -absDist) { //此情况发生时，dist为负值，也就是包围盒中心在平面的另一侧，且大于最大投影距离，判定为包围盒完全在外部
            return Intersection::OUTSIDE;
        }
        else if (dist < absDist) { //包围盒中心到平面的距离小于最大投影距离，判定为相交
            allInside = false;
        }
    }

    return allInside ? Intersection::INSIDE : Intersection::INTERSECTS;
}

std::vector<glm::vec2> BVHTree::projectAABBToScreen(
    const glm::vec3& aabbMin,
    const glm::vec3& aabbMax,
    const glm::mat4& viewProjMatrix) const {

    std::vector<glm::vec2> screenPoints;
    screenPoints.reserve(8);

    //AABB的8个顶点
    std::array<glm::vec3, 8> vertices = {
        glm::vec3(aabbMin.x, aabbMin.y, aabbMin.z),
        glm::vec3(aabbMax.x, aabbMin.y, aabbMin.z),
        glm::vec3(aabbMax.x, aabbMax.y, aabbMin.z),
        glm::vec3(aabbMin.x, aabbMax.y, aabbMin.z),
        glm::vec3(aabbMin.x, aabbMin.y, aabbMax.z),
        glm::vec3(aabbMax.x, aabbMin.y, aabbMax.z),
        glm::vec3(aabbMax.x, aabbMax.y, aabbMax.z),
        glm::vec3(aabbMin.x, aabbMax.y, aabbMax.z)
    };

    for (const auto& vertex : vertices) {
        glm::vec4 clipPos = viewProjMatrix * glm::vec4(vertex, 1.0f);

        if (fabs(clipPos.w) > 1e-10) {
            glm::vec3 ndc = glm::vec3(clipPos) / clipPos.w;
            //转换到屏幕坐标（0-1范围）
            glm::vec2 screenPos = (glm::vec2(ndc) + 1.0f) * 0.5f;
            screenPoints.push_back(screenPos);
        }
    }

    return screenPoints;
}

//判断点是否在凹多边形内（射线法）
bool BVHTree::isPointInConcavePolygon(const glm::vec2& point,
    const std::vector<glm::vec2>& polygon) const {
    int count = 0;
    int n = polygon.size();

    for (int i = 0; i < n; i++) {
        int j = (i + 1) % n;
        const glm::vec2& p1 = polygon[i];
        const glm::vec2& p2 = polygon[j];

        if (((p1.y > point.y) != (p2.y > point.y)) &&
            (point.x < (p2.x - p1.x) * (point.y - p1.y) / (p2.y - p1.y) + p1.x)) {
            count++;
        }
    }
    return (count % 2) == 1;
}


Intersection BVHTree::checkAABBVsConcavePolygon(const AABB& aabb, const std::vector<glm::vec2>& screenPolygon, const glm::mat4& viewProjMatrix)
{
    //1. 将AABB投影到屏幕空间
    std::vector<glm::vec2> screenPoints = projectAABBToScreen(
        aabb.minPt, aabb.maxPt, viewProjMatrix);

    if (screenPoints.empty()) {
        return Intersection::OUTSIDE;
    }

    //2. 统计投影顶点在凹多边形内的数量
    int verticesInside = 0;
    for (const auto& point : screenPoints) {
        if (isPointInConcavePolygon(point, screenPolygon)) {
            verticesInside++;
        }
    }

    //3. 判断关系
    if (verticesInside == screenPoints.size()) {
        return Intersection::INSIDE;
    }
    else if (verticesInside > 0) {
        return Intersection::INTERSECTS;
    }
    else {
        //没有顶点在多边形内，但可能多边形在AABB投影内
        //计算投影的AABB边界
        glm::vec2 projMin = screenPoints[0];
        glm::vec2 projMax = screenPoints[0];
        for (const auto& p : screenPoints) {
            projMin = glm::min(projMin, p);
            projMax = glm::max(projMax, p);
        }

        //检查多边形顶点是否在投影AABB内
        for (const auto& polyVertex : screenPolygon) {
            if (polyVertex.x >= projMin.x && polyVertex.x <= projMax.x &&
                polyVertex.y >= projMin.y && polyVertex.y <= projMax.y) {
                return Intersection::INTERSECTS;
            }
        }

        return Intersection::OUTSIDE;
    }
}


Intersection BVHTree::aabbIntersectFrustum(const AABB& aabb, const std::vector<PlaneD>& planes)
{
    glm::dvec3 temp = aabb.centroid();
    glm::dvec3 center{ temp.x, temp.y, temp.z };

    glm::dvec3 edge{ center[0] - aabb.minPt.x, center[1] - aabb.minPt.y, center[2] - aabb.minPt.z };
    bool allInside = true;

    for (const auto& plane : planes) {
        auto dist = glm::dot(plane.m_normal, center) + plane.m_d;
        auto absDist = glm::dot(plane.m_absNormal, edge);

        if (dist < -absDist) {
            return Intersection::OUTSIDE;
        }
        else if (dist < absDist) {
            allInside = false;
        }
    }

    return allInside ? Intersection::INSIDE : Intersection::INTERSECTS;
}

static inline float vec3LengthSq(const glm::vec3& v) {
    return glm::dot(v, v);
}

inline float distancePointToLine(const glm::vec3& point,
    const glm::vec3& linePoint,
    const glm::vec3& lineDir) {
    //1. 计算从直线上点到目标点的向量
    glm::vec3 v = point - linePoint;

    //2. 计算 v 在 lineDir 上的投影长度 (标量投影)
    //因为 lineDir 是单位向量，dot(v, lineDir) 即为投影长度 t
    float t = glm::dot(v, lineDir);

    //3. 计算直线上的投影点
    glm::vec3 projection = linePoint + t * lineDir;

    //4. 计算目标点到投影点的距离
    //使用 length2 + sqrt 避免多次 sqrt 调用，虽然这里只调用一次
    float distSq = vec3LengthSq(point - projection);

    return std::sqrt(distSq);
}

Intersection BVHTree::aabbIntersectFrustum(const AABB& aabb, const CylinderParams& cylinder) const {
    //1. 计算AABB的外接球
    glm::vec3 aabbCenter = (aabb.minPt + aabb.maxPt) * 0.5f;
    float aabbRadius = glm::length(aabb.maxPt - aabb.minPt) * 0.5f;

    //2. 计算球心到圆柱体轴线的距离
    float distance = distancePointToLine(aabbCenter, cylinder.start, cylinder.direction);

    //3. 比较
    if (distance > cylinder.radius + aabbRadius) {
        return Intersection::OUTSIDE;  //肯定不相交
    }
    else if (distance + aabbRadius < cylinder.radius) {
        return Intersection::INSIDE;  //可能包含
    }
    else {
        return  Intersection::INTERSECTS;  //可能相交
    }
}


bool BVHTree::aabbIntersectPlane(const AABB& aabb, const PlaneD& plane) const
{
    glm::dvec3 center((aabb.minPt.x + aabb.maxPt.x) / 2.0, (aabb.minPt.y + aabb.maxPt.y) / 2.0, (aabb.minPt.z + aabb.maxPt.z) / 2.0);
    glm::dvec3 edge{ center[0] - aabb.minPt.x, center[1] - aabb.minPt.y, center[2] - aabb.minPt.z };

    double dist = glm::dot(plane.m_normal, center) + plane.m_d + 1.0e-5;//宽松小于
    double absDist = glm::dot(plane.m_absNormal, edge);

    if (dist < -absDist) {
        return false;
    }
    else if (dist < absDist) {
        return true;
    }
    return false;
}
