#pragma once

#include <layer1/math/Plane.h>
#include <layer1/math/MathDefs.h>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <string>
#include <string_view>
#include <glm/glm.hpp>
#include <chrono>
#include <memory>
#include <vector>
#include <functional>
#include <algorithm>
#include <inttypes.h>
#include <iomanip>
#include <sstream>
#include <iostream>
#include <inttypes.h>

class Frustum;
class IntRect;
class RenderView;

struct MemoryUtils {
	static void BitBlt2D(const uint8_t* srcData, const glm::uvec2& srcSize, const glm::uvec2& dstStart, const glm::uvec2& dstSize, uint8_t* dstData);

	// srcData: 源数据地址
	// srcStart: 源数据起始位置
	// srcSize: 源拷贝尺寸
	// srcRowSize: 源行字节数
	// dstData: 目标数据地址
	// dstStart: 目标数据起始位置
	// dstSize: 目标数据(整个)尺寸
	static void BitBlt2DEx(const uint8_t* srcData, const glm::uvec2& srcStart, const glm::uvec2& srcSize, uint32_t srcRowSize, uint8_t* dstData, const glm::uvec2& dstStart, const glm::uvec2& dstSize);
};

struct  FileUtils {
    // 目录/文件 是否存在
	static bool IsPathExist(const std::string_view& path);

	// 合并路径
    static std::string ComboPath(const std::string_view& parent, const std::string_view& child);

	// 获取父路径
	static std::string_view GetParentPath(const char* filename);

	// 加载图片文件 为 RGBA
	struct Image {
		uint32_t width;
		uint32_t height;
		void* data;
		~Image();
	};

	static std::shared_ptr<Image> LoadImage(const std::string_view& filename);

    // for raw binary write
    static bool WriteToFile(const char* filename, const void* buffer, size_t size, bool append = true);

    // for string write, file size equals string length
    static bool WriteToFile(const char* filename, const char* content, bool append = true);

    // for raw binary read, if buffer is nullptr, @size returns the file-size, and return true
    static bool ReadFromFile(const char* filename, void* buffer, size_t& size);
    static bool ReadFromFile(const char* filename, std::vector<uint8_t>& buffer);

    // 获得 完整路径文件名 中的 文件名(包含后缀)
    static std::string_view GetFileNameWithoutPath(const char* filename);

    static std::string GetEMAppPath();
    
    // 获取系统字体目录 TODO::暂时只获取了Windows的字体目录，linux系统下可能失败（未验证）
    static std::string GetSystemFontsPath();
};

struct  StringUtils {
    const static char* EMPTY_CHARS;
    const static std::string Empty;

    static std::wstring UTF8_2_W(const std::string_view& str);
    static std::wstring GBK_2_W(const std::string_view& str);
    static std::string W_2_UTF8(const std::wstring_view& str);
    static std::string W_2_GBK(const std::wstring_view& str);
    static std::string GBK_2_WCode(const std::string_view& str);

    static std::string_view Trim(const std::string_view& str, const char* checkChars = EMPTY_CHARS);
    static std::vector<std::string_view> Split(const std::string_view& text, const char* splitChars = EMPTY_CHARS, bool ignoreEmptyItem = true, int maxCount = -1);

    static std::string ToLower(const char* text);
    static std::string ToUpper(const char* text);

    static std::string Replace(const std::string_view& strOri, const std::string_view& strSearch, const std::string_view& strReplacement);
    static std::string Replace(const std::string_view& strOri, const std::initializer_list<std::pair<std::string_view, std::string_view>>& values);

#   define CONSTR(...) ::StringUtils::Concat({__VA_ARGS__})
    static std::string Concat(const std::initializer_list<std::string_view>& ss);

    static std::string Join(const char* split, const std::vector<std::string>& values);

    static std::string GetCurrentTimeStringForLog();

    static std::string GetThreadIDForLog();

    template<typename T>
    static std::string ToString(T value) {
        return std::to_string(value);
    }

    template<typename T>
    static std::string Join(const char* split, T begin, T end) {
        std::stringstream result;
        auto itor = begin;
        if (itor != end) {
            result << *itor; ++itor;
            for (; itor != end; ++itor) {
                result << split << *itor;
            }
        }
        return result.str();
    }


    class ArgBase
    {
    public:
        ArgBase() {}
        virtual ~ArgBase() {}
        virtual void Format(std::ostringstream& ss, const std::string& fmt) = 0;
    };

    template <class T>
    class Arg : public ArgBase
    {
    public:
        Arg(T arg) : m_arg(arg) {}
        virtual ~Arg() {}
        virtual void Format(std::ostringstream& ss, const std::string& fmt)
        {
            if (fmt == "h") {
                ss << std::hex;
            } else {
                ss << std::dec;
            }
            ss << m_arg;
        }
    private:
        T m_arg;
    };

    class ArgArray : public std::vector < ArgBase* >
    {
    public:
        ArgArray() {}
        ~ArgArray()
        {
            std::for_each(begin(), end(), [](ArgBase* p) { delete p; });
        }
    };

    static void FormatItem(std::ostringstream& ss, const std::string& item, const ArgArray& args)
    {
        int index = 0;
        int alignment = 0;
        std::string fmt;

        char* endptr = nullptr;
        index = strtol(&item[0], &endptr, 10);
        if (index < 0 || index >= args.size())
        {
            return;
        }

        if (*endptr == ',')
        {
            alignment = strtol(endptr + 1, &endptr, 10);
            if (alignment > 0)
            {
                ss << std::right << std::setw(alignment);
            } else if (alignment < 0)
            {
                ss << std::left << std::setw(-alignment);
            }
        }

        if (*endptr == ':')
        {
            fmt = endptr + 1;
        }

        args[index]->Format(ss, fmt);

        return;
    }

    template <class T>
    static void Transfer(ArgArray& argArray, T t)
    {
        argArray.push_back(new Arg<T>(t));
    }

    template <class T, typename... Args>
    static void Transfer(ArgArray& argArray, T t, Args&&... args)
    {
        Transfer(argArray, t);
        Transfer(argArray, args...);
    }

    template<typename... Args>
    static std::string Format(const std::string& format, Args&&... args) {
        if (sizeof...(args) == 0)
        {
            return format;
        }

        ArgArray argArray;
        Transfer(argArray, args...);
        size_t start = 0;
        size_t pos = 0;
        std::ostringstream ss;
        while (true)
        {
            pos = format.find('{', start);
            if (pos == std::string::npos)
            {
                ss << format.substr(start);
                break;
            }

            ss << format.substr(start, pos - start);
            const auto& charAfter = format[pos + 1];
            //if (charAfter == '{')
            //{
            //    ss << '{';
            //    start = pos + 2;
            //    continue;
            //}

            if (charAfter < '0' || charAfter > '9') {
                ss << '{';
                start = pos + 1;
                continue;
            }

            start = pos + 1;
            pos = format.find('}', start);
            if (pos == std::string::npos)
            {
                ss << format.substr(start - 1);
                break;
            }

            FormatItem(ss, format.substr(start, pos - start), argArray);
            start = pos + 1;
        }

        return ss.str();
    }

    static uint32_t MakeSignU32(const char* strSign);

    // 非线程安全
    static std::string GenName(const char* prefix) {
        static uint64_t seed = 0;
        return Format("{0}{1}", prefix, ++seed);
    }
};

struct  SysUtils {
#ifdef WIN32
	typedef uint64_t Time;
#else
	typedef std::chrono::system_clock::time_point Time;
#endif // WIN32

	static Time GetCurrentTime();
	static int64_t GetIntervalMS(const Time& start, const Time& end);

};

struct  AutoLogCostMS {
	SysUtils::Time timeStart;
	const char* name;
	AutoLogCostMS(const char* n);
	~AutoLogCostMS();
};

struct AutoTickProgress {
    AutoTickProgress(uint32_t count, std::function<void(float)> tickProgress, uint32_t i = 0);
    ~AutoTickProgress();
    void Tick();
    bool NotFinishd() const { return m_i < m_count; }
    operator uint32_t() const { return m_i; }

private:
    float progress() const;


    std::function<void(float)> m_tickProgress = nullptr;

    uint32_t m_i;			// 循环 i 引用
    uint32_t m_count;		// 循环次数	
    float m_totalTickScale;	// 级联了 所有 Parent 的 0.0~1.0 的步进值
    float m_progressBase;
};

struct CallFinal {
    std::function<void()> m_func;
    CallFinal(std::function<void()> func) :m_func(func) {}
    ~CallFinal() { if (m_func) { m_func(); } }
};

/** 作用域行为，调用方提供开始和结束的行为 */
class ActionScope
{
public:
    ActionScope(std::function<void()> enterFunc, std::function<void()> leaveFunc) : m_leaveFunc(leaveFunc) { enterFunc(); }
    ~ActionScope() { m_leaveFunc(); }

    ActionScope(const ActionScope&) = delete;
    ActionScope& operator=(const ActionScope&) = delete;

    // 允许移动构造（用于返回临时对象）
    ActionScope(ActionScope&& other) noexcept : m_leaveFunc(std::move(other.m_leaveFunc)) {
        other.m_leaveFunc = [](){};
    }

private:
    std::function<void()> m_leaveFunc;
};

// 工具类 增量数据, 非线程安全
// strictMode: 指定为严格模式, 严格模式比较吃性能
// 推荐消费顺序: 删除 -> 新增 -> 修改
template<typename T, bool strictMode = false>
struct Delta {
    explicit Delta(std::function<bool(const T&)> isObjectExists = nullptr)
        : added(isObjectExists)
    {}

    bool rebuildingFlag = false;
    std::vector<T> vecAdding;       // 需要 新增 的对象
    std::vector<T> vecDeleting;     // 需要 删除 的对象
    std::vector<T> vecChanging;     // 需要 修改 的对象

    // 为 Delta 提供 给定对象 是否已经 创建 的 callback
    std::function<bool(const T&)> added;

    // 新增/删除/修改 的 返回值
    // - 一切和谐 返回 OK, 值为 0
    // - 调用成功(即完成了 标记 动作) 但有些违和, 返回 Warning, 值大于 0
    // - 调用失败(即未完成任何 标记 动作), 说明前后矛盾, 无法处理, 返回 Failed, 值小于 0
    enum class Result {
        OK = 0,

        // 新建时
        FAIL_DUPLICATE_ON_ADD = -1,         // 失败: 重复新建
        FAIL_ALREADY_EXISTS_ON_ADD = -2,    // 失败: 对象已经存在
        WARN_HAS_DELETE_ON_ADD = 1,         // 警告: 已经删除, 会 转标记为 Change

        // 删除时
        FAIL_DUPLICATE_ON_DELETE = -3,      // 失败: 重复删除
        FAIL_NOT_EXISTS_ON_DELETE = -4,     // 失败: 删除不存在的对象

        // 修改时
        WARN_DUPLICATE_ON_CHANGE = 2,       // 警告: 重复修改
        WARN_JUST_ADD_ON_CHANGE = 3,        // 警告: 刚刚新建
        FAIL_ALREADY_DELETE_ON_CHANGE = -5, // 失败: 已经删除
        FAIL_NOT_EXISTS_ON_CHANGE = -6,     // 失败: 对象不存在
    };

    // 发生 新增对象操作
    Result Adding(const T& obj) {

        Result result = Result::OK;

        // 严格模式
        if constexpr (strictMode) {
            // 1. 判断重复 adding
            if (CONTAINS(vecAdding, obj)) { assert(false); return Result::FAIL_DUPLICATE_ON_ADD; }

            // 2. 断言 不会 标记为 Changing, 注: 这里只是简单断言, 完整消费时 不应出现
            assert(!CONTAINS(vecChanging, obj));

            // 3. 如果 obj 已经创建, 认为是非法操作
            if (added && added(obj)) { assert(false); return Result::FAIL_ALREADY_EXISTS_ON_ADD; }
        }

        // 4. 判断是否被标记为删除了, 当导入模型就会发生这种情况
        //    说明: km 逻辑会删除 之前的 Part0, 然后再新增 Part0, todo: 为何不会触发 rebuild
        //          如果 发生, 就 改标记为 changing
        auto itorFind = VEC_FIND(vecDeleting, obj);
        if (itorFind != vecDeleting.end()) {
            vecDeleting.erase(itorFind);
            Changing(obj);
            result = Result::WARN_HAS_DELETE_ON_ADD;
        }

        // 标记到 vecAdding 中
        vecAdding.push_back(obj);

        return result;
    }

    // 发生 删除对象操作
    Result Deleting(const T& obj) {

        Result result = Result::OK;

        // 1. 如果 要删除的 对象 就是刚刚 创建的, 直接 删 vecAdding 即可
        {
            auto itorFind = VEC_FIND(vecAdding, obj);
            if (itorFind != vecAdding.end()) {
                vecAdding.erase(itorFind);
            }
        }

        // 2. 如果 要删除的 对象 刚刚 有编辑操作, 则 不需要编辑了
        {
            auto itorFind = VEC_FIND(vecChanging, obj);
            if (itorFind != vecChanging.end()) {
                vecChanging.erase(itorFind);
            }
        }

        // 严格模式
        if constexpr (strictMode) {

            // 3. 重复删除, 认为是非法操作
            if (CONTAINS(vecDeleting, obj)) {
                assert(false);
                return Result::FAIL_DUPLICATE_ON_DELETE;
            }

            // 4. 如果 要删除的 对象 不存在, 认为是非法操作
            if (added && !added(obj)) {
                assert(false);
                return Result::FAIL_NOT_EXISTS_ON_DELETE;
            }

        }

        // 标记到 vecDeleting 中
        vecDeleting.push_back(obj);

        return result;
    }

    // 发生 修改对象操作
    Result Changing(const T& obj) {

        if constexpr (strictMode) {

            // 如果 要编辑的 对象 刚刚 有编辑操作, 则 不需要重复标记了
            if (CONTAINS(vecChanging, obj)) { return Result::WARN_DUPLICATE_ON_CHANGE; }

            // 如果 要编辑的 对象 刚刚 新建, 也不需要标记 为 编辑
            if (CONTAINS(vecAdding, obj)) { return Result::WARN_JUST_ADD_ON_CHANGE; }

            // 如果 要编辑的 对象 被标记为了 删除, 认为是非法操作
            if (CONTAINS(vecDeleting, obj)) { assert(false); return Result::FAIL_ALREADY_DELETE_ON_CHANGE; }

            // 如果 要编辑的 对象 不存在, 认为是非法操作, 注: 上文已经判断了 不在 vecAdding 中, 那么 肯定在 exists 中
            if (added && !added(obj)) { assert(false); return Result::FAIL_NOT_EXISTS_ON_CHANGE; }

        } else {
            // 对于 "修改" 和 "新增" 两个容器, 为了性能, 只简单判断 尾部重复
            if (END_REP(vecChanging, obj)) { return Result::WARN_DUPLICATE_ON_CHANGE; }
            if (END_REP(vecAdding, obj)) { return Result::WARN_JUST_ADD_ON_CHANGE; }
        }

        // 标记到 vecChanging 中
        vecChanging.push_back(obj);

        return Result::OK;
    }

    // 标记 全部重置
    void Rebuilding() {
        Clear();
        rebuildingFlag = true;
    }

    // 清空记录
    void Clear() {
        rebuildingFlag = false;
        vecAdding.clear();
        vecDeleting.clear();
        vecChanging.clear();
    }

    bool Empty() const {
        return rebuildingFlag == false && vecAdding.empty() && vecDeleting.empty() && vecChanging.empty();
    }
};

// 工具类 增量数据, 非线程安全
// strictMode: 指定为严格模式, 严格模式比较吃性能
// 推荐消费顺序: 删除 -> 新增 -> 修改
template<typename T>
struct DeltaEx {
    explicit DeltaEx(std::function<bool(const T&)> isObjectExists = nullptr)
        : added(isObjectExists)
    {}

    bool rebuildingFlag = false;
    std::unordered_set<T> vecAdding;       // 需要 新增 的对象
    std::unordered_set<T> vecDeleting;     // 需要 删除 的对象
    std::unordered_set<T> vecChanging;     // 需要 修改 的对象

    // 为 Delta 提供 给定对象 是否已经 创建 的 callback
    std::function<bool(const T&)> added;

    // 新增/删除/修改 的 返回值
    // - 一切和谐 返回 OK, 值为 0
    // - 调用成功(即完成了 标记 动作) 但有些违和, 返回 Warning, 值大于 0
    // - 调用失败(即未完成任何 标记 动作), 说明前后矛盾, 无法处理, 返回 Failed, 值小于 0
    enum class Result {
        OK = 0,

        // 新建时
        FAIL_DUPLICATE_ON_ADD = -1,         // 失败: 重复新建
        FAIL_ALREADY_EXISTS_ON_ADD = -2,    // 失败: 对象已经存在
        WARN_HAS_DELETE_ON_ADD = 1,         // 警告: 已经删除, 会 转标记为 Change

        // 删除时
        FAIL_DUPLICATE_ON_DELETE = -3,      // 失败: 重复删除
        FAIL_NOT_EXISTS_ON_DELETE = -4,     // 失败: 删除不存在的对象
        SUCC_ANNIHILATION = 4,              // 成功: 删除了一个刚刚创建的对象

        // 修改时
        WARN_DUPLICATE_ON_CHANGE = 2,       // 警告: 重复修改
        WARN_JUST_ADD_ON_CHANGE = 3,        // 警告: 刚刚新建
        FAIL_ALREADY_DELETE_ON_CHANGE = -5, // 失败: 已经删除
        FAIL_NOT_EXISTS_ON_CHANGE = -6,     // 失败: 对象不存在
    };

    // 发生 新增对象操作
    Result Adding(const T& obj) {
        const auto result = TryAdding(obj);
        assert(result != Result::FAIL_ALREADY_EXISTS_ON_ADD); // 如果对象已经创建, 认为是非法操作
        return result;
    }

    // 发生 新增对象操作
    Result TryAdding(const T& obj) {

        // 1. 检查 对象 是否已经创建
        if (added && added(obj)) { return Result::FAIL_ALREADY_EXISTS_ON_ADD; }

        // 2. 判断是否被标记为删除了, 当导入模型就会发生这种情况
        //    说明: km 逻辑会删除 之前的 Part0, 然后再新增 Part0, todo: 为何不会触发 rebuild
        //          如果 发生, 就 改标记为 changing
        if (auto itorFind = vecDeleting.find(obj); itorFind != vecDeleting.end()) {
            vecDeleting.erase(itorFind);
            Changing(obj);
            return Result::WARN_HAS_DELETE_ON_ADD;
        }

        // 3. 断言 不会 标记为 Changing, 注: 这里只是简单断言, 完整消费时 不应出现
        assert(vecChanging.find(obj) == vecChanging.end());

        // 1. 判断重复 adding
        auto [itor, bInsertRet] = vecAdding.insert(obj); if (!bInsertRet) { return Result::FAIL_DUPLICATE_ON_ADD; }

        return Result::OK;
    }

    // 发生 删除对象操作
    Result Deleting(const T& obj) {
        const auto result = TryDeleting(obj);
        assert(result != Result::FAIL_NOT_EXISTS_ON_DELETE); // 要删除的对象不存在, 认为是非法操作
        assert(result != Result::FAIL_DUPLICATE_ON_DELETE); // 重复删除, 认为是非法操作
        return result;
    }

    // 发生 删除对象操作
    Result TryDeleting(const T& obj) {

        // 1. 如果 要删除的 对象 刚刚 有编辑操作, 则 不需要编辑了
        {
            auto itorFind = vecChanging.find(obj);
            if (itorFind != vecChanging.end()) {
                vecChanging.erase(itorFind);
            }
        }

        // 2. 如果 要删除的 对象 就是刚刚 创建的, 直接 删 vecAdding 即可
        {
            auto itorFind = vecAdding.find(obj);
            if (itorFind != vecAdding.end()) {
                vecAdding.erase(itorFind);
                return Result::SUCC_ANNIHILATION;
            }
        }

        // 3. 检查 要删除的 对象 是否存在
        if (added && !added(obj)) {
            return Result::FAIL_NOT_EXISTS_ON_DELETE;
        }

        // 4. 检查 是否重复删除
        if (auto insertResult = vecDeleting.insert(obj); !insertResult.second) {
            return Result::FAIL_DUPLICATE_ON_DELETE;
        }

        return Result::OK;
    }

    // 发生 修改对象操作
    Result Changing(const T& obj) {
        const auto result = TryChanging(obj);
        assert(result != Result::FAIL_NOT_EXISTS_ON_CHANGE); // 如果要编辑的对象不存在, 认为是非法操作
        assert(result != Result::FAIL_ALREADY_DELETE_ON_CHANGE); // 如果要编辑的对象被标记为了删除, 认为是非法操作
        return result;
    }

    // 发生 修改对象操作
    Result TryChanging(const T& obj) {

        // 如果 要编辑的 对象 刚刚 新建, 不需要标记 为 编辑
        {
            if (vecAdding.find(obj) != vecAdding.end()) { return Result::WARN_JUST_ADD_ON_CHANGE; }
        }

        // 检查 要编辑的 对象 是否存在, 注: 上文已经判断了 不在 vecAdding 中, 那么 肯定在 exists 中
        {
            if (added && !added(obj)) { return Result::FAIL_NOT_EXISTS_ON_CHANGE; }
        }

        // 检查 要编辑的 对象 是否被标记为了 删除
        {
            if (vecDeleting.find(obj) != vecDeleting.end()) { return Result::FAIL_ALREADY_DELETE_ON_CHANGE; }
        }

        // 标记到 vecChanging 中, 如果 要编辑的 对象 刚刚 有编辑操作, 则 不需要重复标记了
        {
            auto insertResult = vecChanging.insert(obj);
            if (!!insertResult.second) { return Result::WARN_DUPLICATE_ON_CHANGE; }
        }

        return Result::OK;
    }

    // 标记 全部重置
    void Rebuilding() {
        Clear();
        rebuildingFlag = true;
    }

    // 清空记录
    void Clear() {
        rebuildingFlag = false;
        vecAdding.clear();
        vecDeleting.clear();
        vecChanging.clear();
    }

    bool Empty() const {
        return rebuildingFlag == false && vecAdding.empty() && vecDeleting.empty() && vecChanging.empty();
    }
};

namespace glm {
    template<unsigned int DIM, typename T, qualifier Q>
    void assign(const glm::vec<DIM, T, Q>& v, T(&arr)[DIM]) {
        for (auto i = 0u; i < DIM; ++i) {
            arr[i] = v[i];
        }
    }
}

#define DELETE_ASSIGN_MOVE_CTOR(className) \
    className(const className &); \
    className(className&&); \
    className& operator =(const className&); \
    className& operator =(className&&);
