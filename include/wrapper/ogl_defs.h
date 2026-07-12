#pragma once

// ===================================================================
// ogl_defs.h — OpenGL 核心定义，替代原 yunbo/ybvi 框架依赖
// 所有 OpenGL 封装类统一包含此头文件即可
// ===================================================================

#define GLEW_NO_GLU
#include <GL/glew.h>

#include <cassert>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include <functional>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <array>
#include <cstring>

// ===================================================================
// 命名空间宏
// ===================================================================
#define NAMESPACE_OPENGL_CORE_BEGIN   namespace OGLCore {
#define NAMESPACE_OPENGL_CORE_END     }

#define YBVI_API   // 静态库无需导出符号

// ===================================================================
// GL() — OpenGL 调用错误检查宏
// Debug 模式下每次 GL 调用后检查 glGetError
// ===================================================================
#ifndef GL
    #ifdef _DEBUG
        #define GL(func, ...) func(__VA_ARGS__); { auto errorCode = ::glGetError(); assert(errorCode == 0); }
    #else
        #define GL(func, ...) func(__VA_ARGS__);
    #endif
#endif

// ===================================================================
// VI_ASSERT_* — 日志+断言宏（替代原 yunbo Logger）
// ===================================================================
#define VI_ASSERT_LOG_ERROR_RETURN(expr, ...)        \
    do {                                             \
        if (!(expr)) {                               \
            char _vi_buf[1024];                      \
            snprintf(_vi_buf, sizeof(_vi_buf), __VA_ARGS__); \
            std::cerr << "[OGL ERROR] " << _vi_buf << std::endl; \
            assert(expr);                            \
            return;                                  \
        }                                            \
    } while(0)

#define VI_ASSERT_LOG_ERROR_RETURN_VALUE(expr, retVal, ...) \
    do {                                                    \
        if (!(expr)) {                                      \
            char _vi_buf[1024];                             \
            snprintf(_vi_buf, sizeof(_vi_buf), __VA_ARGS__); \
            std::cerr << "[OGL ERROR] " << _vi_buf << std::endl; \
            assert(expr);                                   \
            return retVal;                                  \
        }                                                   \
    } while(0)

// ===================================================================
// CallFinal — RAII 清理辅助类（析构时自动执行回调）
// ===================================================================
struct CallFinal {
    std::function<void()> m_func;
    explicit CallFinal(std::function<void()> f) : m_func(std::move(f)) {}
    ~CallFinal() { if (m_func) m_func(); }
    CallFinal(const CallFinal&) = delete;
    CallFinal& operator=(const CallFinal&) = delete;
};

// ===================================================================
// FOR_VEC — 遍历容器的便捷宏
// ===================================================================
#define FOR_VEC(vec, itor) \
    for (auto itor = (vec).begin(); itor != (vec).end(); ++itor)

// ===================================================================
// MemoryRecord — 内存修改记录（用于 Suspend/Resume 批量操作）
// ===================================================================
struct MemoryRecord {
    using SpaceList = std::vector<std::pair<uint32_t, uint32_t>>;

    MemoryRecord() = default;

    void GrowTo(uint32_t /*newSize*/) {
        // 扩容后记录保持不变（只需确保不会越界）
    }

    void Record(uint32_t offset, uint32_t size) {
        // 添加/合并修改记录区间
        if (size == 0) return;
        m_records.push_back({offset, size});
        MergeRecords();
    }

    bool IsEmpty() const { return m_records.empty(); }

    void Clear() { m_records.clear(); }

    const SpaceList& GetRecords() const { return m_records; }

private:
    void MergeRecords() {
        if (m_records.size() <= 1) return;
        // 简单合并相邻/重叠区间
        std::sort(m_records.begin(), m_records.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
        SpaceList merged;
        merged.push_back(m_records[0]);
        for (size_t i = 1; i < m_records.size(); ++i) {
            auto& last = merged.back();
            const auto& cur = m_records[i];
            uint32_t lastEnd = last.first + last.second;
            if (cur.first <= lastEnd) {
                uint32_t curEnd = cur.first + cur.second;
                last.second = (curEnd > lastEnd) ? (curEnd - last.first) : last.second;
            } else {
                merged.push_back(cur);
            }
        }
        m_records.swap(merged);
    }

    SpaceList m_records;
};

// ===================================================================
// StringUtils — 字符串替换工具
// ===================================================================
namespace StringUtils {
    inline std::string Replace(const char* source,
        const std::initializer_list<std::pair<std::string_view, std::string_view>>& replaces)
    {
        std::string result(source);
        for (const auto& [search, replace] : replaces) {
            size_t pos = 0;
            while ((pos = result.find(search, pos)) != std::string::npos) {
                result.replace(pos, search.length(), replace);
                pos += replace.length();
            }
        }
        return result;
    }

    inline std::string Replace(const std::string& source,
        const std::initializer_list<std::pair<std::string_view, std::string_view>>& replaces)
    {
        return Replace(source.c_str(), replaces);
    }
}

// ===================================================================
// USING_OGL_* — Scope-Use 便捷宏
// 前置声明，具体实现在 Program.h / FBO.h 中定义
// ===================================================================
NAMESPACE_OPENGL_CORE_BEGIN
    struct ScopeUseProgram;
    struct ScopeUseFBO;
NAMESPACE_OPENGL_CORE_END

#define USING_OGL_PROGRAM(p)      for (OGLCore::ScopeUseProgram _SUP_(p); _SUP_; _SUP_.runOut())
#define USING_OGL_FRAMEBUFFER(f)  for (OGLCore::ScopeUseFBO _SUF_(f); _SUF_; _SUF_.runOut())

// ===================================================================
// Image 类声明（如果后续需要图片支持，需自行实现或引入 stb_image）
// ===================================================================
class Image {
public:
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* data = nullptr;

    ~Image() { delete[] data; data = nullptr; }

    bool IsAllocated() const { return data != nullptr; }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }
    int GetNumComponents() const { return channels; }
    unsigned char* GetData() { return data; }
    const unsigned char* GetData() const { return data; }

    void Allocate(int w, int h, int c) {
        delete[] data;
        width = w; height = h; channels = c;
        data = new unsigned char[w * h * c]();
    }
};

// FileUtils::LoadImage — TODO: 需要用户自行集成 stb_image 或其他图片库
// 当前提供空实现，返回 nullptr
namespace FileUtils {
    inline std::shared_ptr<Image> LoadImage(const std::string& filepath) {
        // TODO: 集成 stb_image 或其他图片加载库
        // 使用示例:
        //   int w, h, c;
        //   unsigned char* data = stbi_load(filepath.c_str(), &w, &h, &c, 4);
        //   if (data) { auto img = std::make_shared<Image>();
        //               img->Allocate(w, h, 4);
        //               memcpy(img->data, data, w*h*4);
        //               stbi_image_free(data); return img; }
        std::cerr << "[WARNING] FileUtils::LoadImage not implemented! (file: "
                  << filepath << ")" << std::endl;
        return nullptr;
    }
}
