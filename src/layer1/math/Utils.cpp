#include <layer1/math/Utils.h>
#include <algorithm>
#include <cctype>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <inttypes.h>
#include <sstream>
#include <string.h>
#include <string_view>
#include <thread>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image/stb_image.h>

#ifdef WIN32
    #define NOMINMAX
    #include <windows.h>
    #include <shlobj.h>
    #undef LoadImage
    #undef GetCurrentTime
#elif defined(__linux__)
    #include <limits.h>
    #include <unistd.h>
    #include <locale>
    #include <codecvt>
    #include <libgen.h>
    #include <unistd.h>
    #include <climits>
#elif defined(__APPLE__)
    #include <limits.h>
    #include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;


#ifdef WIN32
    static char PATH_SEPERATOR = '\\';
#else
    static char PATH_SEPERATOR = '/';
#endif

#pragma region FileUtils

std::string FileUtils::GetEMAppPath()
{
#ifdef _WIN32
    char exeFilePath[MAX_PATH] = { 0 };
    GetModuleFileNameA(NULL, exeFilePath, MAX_PATH);
    std::string strExeFilePath = exeFilePath;
    int idx = strExeFilePath.rfind("\\");
#elif defined(__linux__)
    char exe_path[PATH_MAX];
    ssize_t count = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (count != -1) {
        exe_path[count] = '\0';
    }
    else {
        perror("readlink error");
        return "";
    }
    std::string strExeFilePath = exe_path;
    int idx = strExeFilePath.rfind("/");
#endif
    strExeFilePath = strExeFilePath.substr(0, idx);
    return strExeFilePath;
}

std::string FileUtils::GetSystemFontsPath() {
#ifdef WIN32
    TCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_FONTS, NULL, 0, path))) {
        return path;
    } else {
        return "";
    }
#elif defined(__linux__)
    std::string fonts_dir = "/usr/share/fonts";
    return fonts_dir;
#elif defined(__APPLE__)
    std::string fonts_dir = "/System/Library/Fonts";
    return fonts_dir;
#endif
}

bool FileUtils::IsPathExist(const std::string_view& path) {
    fs::path aPath(path);
    return fs::exists(aPath);
}

std::string FileUtils::ComboPath(const std::string_view& parent, const std::string_view& child) {
    if (parent.empty()) { std::string(child); }
    if (child.empty()) { std::string(parent); }
    std::string result(parent);
    auto sA = parent.back();
    auto sb = child.front();
    if (sA != '/' && sA != '\\' && sb != '/' && sb != '\\') {
        result.push_back(PATH_SEPERATOR);
    }
    result.append(child);
    return result;
}

std::string_view FileUtils::GetParentPath(const char* filename) {
    auto pos0 = strrchr(filename, '\\');
    auto pos1 = strrchr(filename, '/');

    // 找到最后一个路径分隔符
    const char* last_separator = nullptr;
    if (pos0 && pos1) {
        last_separator = (pos0 > pos1) ? pos0 : pos1;
    }
    else if (pos0) {
        last_separator = pos0;
    }
    else if (pos1) {
        last_separator = pos1;
    }

    // 如果没有找到路径分隔符，则返回空的 string_view
    if (!last_separator || last_separator == filename) {
        // 可能需要返回一个代表当前目录的视图，或者空视图
        return std::string_view(); // 或者返回 "./" 或 "/" 等
    }

    // 返回从字符串开始到最后一个分隔符之前的部分
    return std::string_view(filename, last_separator - filename);
}

FileUtils::Image::~Image() { stbi_image_free(this->data); }

std::shared_ptr<FileUtils::Image> FileUtils::LoadImage(const std::string_view& filename) {
    int w, h, comp;
    auto data = stbi_load(std::string(filename).c_str(), &w, &h, &comp, 4);
    if (data == nullptr || w <= 0 || h <= 0) { return nullptr; }
    auto result = std::make_shared<Image>();
    result->width = (uint32_t)w;
    result->height = (uint32_t)h;
    result->data = data;
    return result;
}

bool FileUtils::WriteToFile(const char* filename, const void* buffer, size_t size, bool append) {
    auto mode = (std::ios_base::openmode)(std::ios_base::out | std::ios_base::binary | (append ? std::ios_base::app : 0));
    std::ofstream stream(filename, mode);
    if (!stream.good()) { assert(false); return false; }
    stream.write((const char*)buffer, size);
    stream.close();
    return true;
}

// for string write, file size equals string length
bool FileUtils::WriteToFile(const char* filename, const char* content, bool append) {
    auto size = strlen(content);
    return WriteToFile(filename, content, size, append);
}

// for raw binary read, if buffer is nullptr, @size returns the file-size, and return true
bool FileUtils::ReadFromFile(const char* filename, void* buffer, size_t& size) {
    std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
    assert(stream.good());
    stream.read((char*)buffer, size);
    stream.close();
    return true;
}

static size_t getFileSize(std::ifstream& stream) {
    auto oldTell = stream.tellg();
    stream.seekg(0, std::ios_base::end);
    auto fileSize = stream.tellg();
    stream.seekg(oldTell, std::ios_base::beg);
    return fileSize;
}

bool FileUtils::ReadFromFile(const char* filename, std::vector<uint8_t>& buffer) {
    struct NoInit { 
        uint8_t a;
        NoInit() {} 
    };
    static_assert(sizeof(NoInit) == sizeof(uint8_t));

    std::ifstream stream(filename, std::ios_base::in | std::ios_base::binary);
    assert(stream.good());

    auto fileSize = getFileSize(stream);
    auto buf = (std::vector<NoInit>*) & buffer;
    const auto oldSize = buf->size();
    buf->resize(oldSize + fileSize);
    auto ptr = (char*)(buf->data() + oldSize);
    stream.read(ptr, fileSize);

    stream.close();

    return true;
}

std::string_view FileUtils::GetFileNameWithoutPath(const char* filename) {
    auto ptr00 = strrchr(filename, '\\');
    auto ptr01 = strrchr(filename, '/');
    auto ptr0 = std::max(ptr00, ptr01);
    return std::string_view(ptr0 + 1);
}
#pragma endregion

#pragma region MemoryUtils

void MemoryUtils::BitBlt2D(const uint8_t* srcData, const glm::uvec2& srcSize, const glm::uvec2& dstStart, const glm::uvec2& dstSize, uint8_t* dstData) {
    const auto copySizePreRow = std::min(srcSize.x, dstSize.x - dstStart.x);
    const auto copyHeight = std::min(srcSize.y, dstSize.y - dstStart.y);
    for (auto y = 0u; y < copyHeight; ++y) {
        const auto srcPtr = srcData + y * srcSize.x;
        const auto dstPtr = dstData + (dstStart.y + y) * dstSize.x + dstStart.x;
        memcpy(dstPtr, srcPtr, copySizePreRow);
    }
}

void MemoryUtils::BitBlt2DEx(const uint8_t* srcData, const glm::uvec2& srcStart, const glm::uvec2& srcSize, uint32_t srcRowSize, uint8_t* dstData, const glm::uvec2& dstStart, const glm::uvec2& dstSize) {
    const auto copySizePreRow = std::min(srcSize.x, dstSize.x - dstStart.x);
    const auto copyHeight = std::min(srcSize.y, dstSize.y - dstStart.y);
    for (auto y = 0u; y < copyHeight; ++y) {
        const auto srcPtr = srcData + (y + srcStart.y) * srcRowSize + srcStart.x;
        const auto dstPtr = dstData + (dstStart.y + y) * dstSize.x + dstStart.x;
        memcpy(dstPtr, srcPtr, copySizePreRow);
    }
}
#pragma endregion

#pragma region StringUtils

const char* StringUtils::EMPTY_CHARS = " \f\n\r\t\v";   // ref: std::isspace
const std::string StringUtils::Empty = std::string();

std::wstring StringUtils::UTF8_2_W(const std::string_view& str) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.from_bytes(str.data(), str.data() + str.size());
}

struct chs_codecvt : public std::codecvt_byname<wchar_t, char, std::mbstate_t> { chs_codecvt() : codecvt_byname("chs") { } };
std::wstring StringUtils::GBK_2_W(const std::string_view& str) {
    static std::wstring_convert<chs_codecvt> converter;
    return converter.from_bytes(str.data(), str.data() + str.size());
}

std::string StringUtils::W_2_UTF8(const std::wstring_view& str) {
    static std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
    return converter.to_bytes(str.data(), str.data() + str.size());
}

std::string StringUtils::W_2_GBK(const std::wstring_view& str) {
    static std::wstring_convert<chs_codecvt> converter;
    return converter.to_bytes(str.data(), str.data() + str.size());
}

std::string StringUtils::GBK_2_WCode(const std::string_view& str) {
    auto w = GBK_2_W(str);
    std::string result;
    for (auto itor = w.begin(); itor != w.end(); ++itor) {
        char buffer[64];
        std::snprintf(buffer, 64, "%" PRIx64, (uint64_t)*itor);
        result += "\\u";
        result += buffer;
    }
    return result;
}

std::string_view StringUtils::Trim(const std::string_view& str, const char* checkChars) {
    auto start = str.data(), end = str.data() + str.size();
    for (; (start < end) && (strchr(checkChars, *start) != nullptr); ++start) {}

    for (auto p = end - 1; p > start; --p) {
        if (strchr(checkChars, *p) == nullptr) {
            end = p + 1;
            break;
        }
    }
    return std::string_view(start, end - start);
}

std::vector<std::string_view> StringUtils::Split(const std::string_view& text, const char* splitChars, bool ignoreEmptyItem, int maxCount) {
    std::vector<std::string_view> result;

    size_t previousStart = 0; // 前一项的起始位置

    for (auto itor = text.begin(); itor != text.end(); ++itor) {
        const auto c = *itor;
        // 如果不是分割字符, 就跳过
        if (strchr(splitChars, c) == nullptr) { continue; }

        // 取当前字符的索引
        const auto idx = itor - text.begin();

        // 当前项的起始
        const auto currentStart = previousStart;
        // 当前项的长度
        const auto currentCount = idx - currentStart;
        // 重新调整 前一项的起始位置
        previousStart = idx + 1;

        // 如果 长度为0, 并且 指定了忽略空项, 就跳过
        if (currentCount == 0 && ignoreEmptyItem) { continue; }

        // 收集当前项
        result.emplace_back(std::string_view(text.data() + currentStart, currentCount));

        // 如果指定了最大收集项数, 并达到, 则提前结束
        if (maxCount != -1 && (int)result.size() >= maxCount) {
            return result;
        }
    }

    // 收集最后一项
    {
        const auto currentStart = previousStart;
        // 当前项的长度
        const auto currentCount = text.size() - currentStart;

        // 如果 不长度为0, 或者 没有指定了忽略空项
        if (currentCount != 0 || !ignoreEmptyItem) {

            // 收集当前项
            result.emplace_back(std::string_view(text.data() + currentStart, currentCount));
        }
    }

    return result;
}

std::string StringUtils::ToLower(const char* text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}
std::string StringUtils::ToUpper(const char* text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(), ::toupper);
    return result;
}

std::string StringUtils::Replace(const std::string_view& strOri, const std::string_view& strSearch, const std::string_view& strReplacement) {
    // 注: 实现效率不高, 不能用于性能敏感位置

    std::string result(strOri);
    for (;;) {
        auto pos = result.find(strSearch);
        if (pos == std::string::npos) { break; }
        result.replace(pos, strSearch.length(), strReplacement);
    }
    return result;
}

std::string StringUtils::Replace(const std::string_view& strOri, const std::initializer_list<std::pair<std::string_view, std::string_view>>& values) {
    // 注: 实现效率不高, 不能用于性能敏感位置
    std::string result(strOri);
    for (auto itor = values.begin(); itor != values.end(); ++itor) {
        result = Replace(result, itor->first, itor->second);
    }
    return result;
}

std::string StringUtils::Concat(const std::initializer_list<std::string_view>& ss) {
    std::string result;
    for (auto itor = ss.begin(); itor != ss.end(); ++itor) {
        result += *itor;
    }
    return result;
}

std::string StringUtils::Join(const char* split, const std::vector<std::string>& values) {
    std::string result;
    if (values.size() == 0) { return result; }
    for (auto itor = values.begin(); itor + 1 != values.end(); ++itor) {
        result.append(*itor);
        result.append(split);
}
    result.append(*(values.begin() + values.size() - 1));
    return result;
    }

std::string StringUtils::GetCurrentTimeStringForLog() {
    const auto now = std::chrono::system_clock::now();
    time_t time = std::chrono::system_clock::to_time_t(now);
    tm* timeinfo = localtime(&time);
    char buffer[70];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return buffer;
}

std::string StringUtils::GetThreadIDForLog() {
    // 说明:
    // - windows 平台 该值 即为 GetCurrentThreadId(), 与调试线程窗口中的ID号 一致
    // todo: stringstream 效率不高
    std::stringstream ss;
    ss << std::this_thread::get_id();
    return ss.str();
}

uint32_t StringUtils::MakeSignU32(const char* strSign) {
    uint32_t result = 0;
    for (auto i = 0; i < 4; ++i) {
        const char c = strSign[i];
        if (c == '\0') { break; }
        result = c << (i << 3);
    }
    return result;
}
#pragma endregion

#pragma region SysUtils

SysUtils::Time SysUtils::GetCurrentTime() {
#ifdef WIN32
    return ::GetTickCount64();
#else
    return std::chrono::system_clock::now();
#endif // WIN32
}

int64_t SysUtils::GetIntervalMS(const Time& start, const Time& end) {
#ifdef WIN32
    return (int64_t)end - (int64_t)start;
#else
    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
#endif // WIN32
}


#pragma endregion

#pragma region AutoLogCostMS

AutoLogCostMS::AutoLogCostMS(const char* n)
    : name(n)
    , timeStart(SysUtils::GetCurrentTime())
{}

AutoLogCostMS::~AutoLogCostMS() {
}
#pragma endregion

#pragma region AutoTickProgress

// todo: 考虑放到 TLS
static std::vector<AutoTickProgress*> g_tickStack;
static uint32_t g_iLastTick = 0xFFFFFFFF;
static const uint32_t g_tickCount = 100;

AutoTickProgress::AutoTickProgress(uint32_t count, std::function<void(float)> tickProgress, uint32_t i)
    : m_tickProgress(tickProgress), m_i(i), m_count(count)
{
    assert(m_tickProgress != nullptr);
    if (g_tickStack.empty()) {
        m_totalTickScale = (1.0f / (float)(count == 0 ? 1 : count));
        m_progressBase = 0.0f;
        m_tickProgress(-0.1f);        // <0 用于显示 ProgressBar
    } else {
        const auto parent = g_tickStack.back();
        m_totalTickScale = (1.0f / (float)(count == 0 ? 1 : count)) * parent->m_totalTickScale;
        m_progressBase = parent->progress();
    }
    g_tickStack.push_back(this);
}

AutoTickProgress::~AutoTickProgress() {
    assert(g_tickStack.size());
    assert(g_tickStack.back() == this);
    g_tickStack.pop_back();
    if (g_tickStack.empty()) {
        m_tickProgress(1.1f);        // >1 用于隐藏 ProgressBar
    }
}

void AutoTickProgress::Tick() {
    ++m_i;
    const float progress = this->progress();
    const auto tick = (uint32_t)(progress * 100);
    if (g_iLastTick == tick) { return; }
    g_iLastTick = tick;
    m_tickProgress(progress);
}

float AutoTickProgress::progress() const {
    return m_i* m_totalTickScale + m_progressBase;
}
#pragma endregion

