
#include "OglStateRecovery.h"

NAMESPACE_OPENGL_CORE_BEGIN

std::shared_ptr<struct DynamicStateRecoveryBase> RegDynamicStateRecoveryCreator(int stateType, DynamicStateRecoveryCreator creator) {
    static std::map<int, DynamicStateRecoveryCreator> _map;
    if (creator == nullptr) {
        auto itorFind = _map.find(stateType); assert(itorFind != _map.end());
        return itorFind->second();
    }
    else {
        auto itorInsert = _map.insert(std::make_pair(stateType, creator));
        assert(itorInsert.second);
        return nullptr;
    }
}

DynamicStatesRecovery DynamicStateRecoveryBase::Creates(const std::vector<int>& states) {
    DynamicStatesRecovery result;
    for (auto itor = states.begin(); itor != states.end(); ++itor) {
        result.push_back(RegDynamicStateRecoveryCreator(*itor, nullptr));
    }
    return result;
}

NAMESPACE_OPENGL_CORE_END
