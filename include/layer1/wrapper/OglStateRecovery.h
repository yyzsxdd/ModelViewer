#pragma once

#include "ogl_defs.h"

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <tuple>

// 获取gl上下文
#ifdef WIN32
#define NOMINMAX
#include <windows.h>
#undef GetCurrentTime
#else
#include "GL/glx.h"
#endif

NAMESPACE_OPENGL_CORE_BEGIN

// StateRecovery 为 模板 "基类"
// 每一种 OpenGL 状态的保存与恢复 将实现为 该类的 一个特化
template<int stateType>
struct StateRecovery {
private:	// 隐藏构造函数, 用于在编译时抛出 未被支持的 状态
    StateRecovery() = default;
    ~StateRecovery() = default;
};

// 解决 StateRecovery 对象间 赋值 导致 提前调用 析构函数的问题
// 注意点:
// 1. m_uniqueOwner 用于 标记 是否 真正有内容所属, 只有 是拥有者 才能真正的调用 析构恢复 状态
// 2. m_uniqueOwner 会在 move 构造 中改变 双方
// 3. 删除了 赋值构造, 外部代码只允许 move
// 4. 派生类 必须也 重载 move 构造, 避免 并且要 调用 基类的 move 构造
// 5. 在 派生类 的 move 构造中 只需 直接对 内容赋值(转移), 不需要重复调用 glGet
// 6. 最后需要 在 派生类 的 析构 中 使用 m_uniqueOwner 来决定 是否真正调用 recovery 代码
struct StateRecoveryBase {
    StateRecoveryBase() = default;
    StateRecoveryBase(StateRecoveryBase&& o) noexcept
    {
        o.m_uniqueOwner = false;
    }
    // 禁用赋值构造, 外部代码只允许 move
    StateRecoveryBase(const StateRecoveryBase&) = delete;
    StateRecoveryBase& operator=(const StateRecoveryBase&) = delete;
    ~StateRecoveryBase() = default;

    bool m_uniqueOwner = true;
};

// StateRecovery 只能静态执行, 当需要动态 Recovery 某些状态时, 就需要用 DynamicStateRecovery
// 说明: 
// 用法:
typedef std::function< std::shared_ptr<struct DynamicStateRecoveryBase>(void) > DynamicStateRecoveryCreator;
std::shared_ptr<struct DynamicStateRecoveryBase> RegDynamicStateRecoveryCreator(int stateType, DynamicStateRecoveryCreator creator);
typedef std::vector<std::shared_ptr<struct DynamicStateRecoveryBase>> DynamicStatesRecovery;
struct DynamicStateRecoveryBase {
    virtual ~DynamicStateRecoveryBase() = default;
    static DynamicStatesRecovery Creates(const std::vector<int>& states);
};
template<int StateRecoveryType> struct DynamicStateRecovery : DynamicStateRecoveryBase {
    StateRecovery<StateRecoveryType> m_stateRec;
};

#define DEF_STATE_RECOVERY_PART_1(STATE_TYPE, SAVE_CODE, REC_CODE, FILED_TYPE, FILED_NAME) \
	template<> struct StateRecovery<STATE_TYPE> : public StateRecoveryBase { \
		StateRecovery() { \
			SAVE_CODE; \
		} \
		StateRecovery(StateRecovery&& o) noexcept \
			: StateRecoveryBase(std::move(o)) \
			, FILED_NAME(o.FILED_NAME) \
		{} \
		~StateRecovery() { \
			if (m_uniqueOwner) { \
				REC_CODE; \
			} \
		} \
		FILED_TYPE FILED_NAME; \
	}
#ifdef STATE_RECOVERY_IMPL
#	define DEF_STATE_RECOVERY_PART_2(STATE_TYPE) \
	std::shared_ptr<struct DynamicStateRecoveryBase> _USELESS_##STATE_TYPE = RegDynamicStateRecoveryCreator(STATE_TYPE, [](){ \
		return std::make_shared<DynamicStateRecovery<STATE_TYPE>>(); \
	})
#else
#	define DEF_STATE_RECOVERY_PART_2(STATE_TYPE) \
	extern std::shared_ptr<struct DynamicStateRecoveryBase> _USELESS_##STATE_TYPE
#endif // STATE_RECOVERY_IMPL


#define DEF_STATE_RECOVERY(STATE_TYPE, SAVE_CODE, REC_CODE, FILED_TYPE, FILED_NAME) \
	DEF_STATE_RECOVERY_PART_1(STATE_TYPE, SAVE_CODE, REC_CODE, FILED_TYPE, FILED_NAME); \
	DEF_STATE_RECOVERY_PART_2(STATE_TYPE)


// 方便宏, 可处理 所有的 glEnable
#define DEF_STATE_RECOVERY_ENABLE(STATE_TYPE) \
	DEF_STATE_RECOVERY(STATE_TYPE,\
		GL(glGetBooleanv, STATE_TYPE, &m_value), \
		if (m_value) { GL(glEnable, STATE_TYPE); } else { GL(glDisable, STATE_TYPE); }, \
		GLboolean, m_value)

// 方便宏, 处理所有 bool类型 状态, 恢复代码需要明确给出
#define DEF_STATE_RECOVERY_BOOLEAN(STATE_TYPE, REC_CODE) \
	DEF_STATE_RECOVERY(STATE_TYPE,\
		GL(glGetBooleanv, STATE_TYPE, &m_value), \
		REC_CODE, \
		GLboolean, m_value)

// 方便宏, 处理所有 int类型 状态, 恢复代码需要明确给出
#define DEF_STATE_RECOVERY_INT(STATE_TYPE, REC_CODE) \
	DEF_STATE_RECOVERY(STATE_TYPE,\
		GL(glGetIntegerv, STATE_TYPE, &m_value), \
		REC_CODE, \
		GLint, m_value)

// 方便宏, 处理所有 float类型 状态, 恢复代码需要明确给出
#define DEF_STATE_RECOVERY_FLOAT(STATE_TYPE, REC_CODE) \
	DEF_STATE_RECOVERY(STATE_TYPE,\
		GL(glGetFloatv, STATE_TYPE, &m_value), \
		REC_CODE, \
		GLfloat, m_value)

// ======================================================================================
DEF_STATE_RECOVERY_ENABLE(GL_CULL_FACE);
DEF_STATE_RECOVERY_ENABLE(GL_DEPTH_TEST);
DEF_STATE_RECOVERY_ENABLE(GL_LINE_STIPPLE);
DEF_STATE_RECOVERY_ENABLE(GL_SAMPLE_SHADING);
DEF_STATE_RECOVERY_ENABLE(GL_PRIMITIVE_RESTART);
DEF_STATE_RECOVERY_ENABLE(GL_POLYGON_OFFSET_FILL);
DEF_STATE_RECOVERY_ENABLE(GL_POLYGON_OFFSET_LINE);
DEF_STATE_RECOVERY_ENABLE(GL_POLYGON_OFFSET_POINT);
DEF_STATE_RECOVERY_ENABLE(GL_POINT_SPRITE);
DEF_STATE_RECOVERY_ENABLE(GL_PROGRAM_POINT_SIZE);
DEF_STATE_RECOVERY_ENABLE(GL_STENCIL_TEST);
DEF_STATE_RECOVERY_ENABLE(GL_MULTISAMPLE);
DEF_STATE_RECOVERY_ENABLE(GL_SCISSOR_TEST);

DEF_STATE_RECOVERY_BOOLEAN(GL_DEPTH_WRITEMASK, GL(glDepthMask, m_value));

DEF_STATE_RECOVERY_INT(GL_CURRENT_PROGRAM, GL(glUseProgram, m_value));

DEF_STATE_RECOVERY_INT(GL_TEXTURE_BINDING_2D, GL(glBindTexture, GL_TEXTURE_2D, m_value));

DEF_STATE_RECOVERY_INT(GL_TEXTURE_BINDING_2D_MULTISAMPLE, GL(glBindTexture, GL_TEXTURE_2D_MULTISAMPLE, m_value));

DEF_STATE_RECOVERY_INT(GL_TEXTURE_BINDING_2D_ARRAY, GL(glBindTexture, GL_TEXTURE_2D_ARRAY, m_value));

DEF_STATE_RECOVERY_INT(GL_TEXTURE_BINDING_CUBE_MAP, GL(glGetIntegerv, GL_TEXTURE_BINDING_CUBE_MAP, &m_value));

DEF_STATE_RECOVERY_INT(GL_FRONT_FACE, GL(glFrontFace, m_value));

DEF_STATE_RECOVERY_INT(GL_CULL_FACE_MODE, GL(glCullFace, m_value));

DEF_STATE_RECOVERY_INT(GL_ARRAY_BUFFER_BINDING, GL(glBindBuffer, GL_ARRAY_BUFFER, m_value));

DEF_STATE_RECOVERY_INT(GL_ELEMENT_ARRAY_BUFFER_BINDING, GL(glBindBuffer, GL_ELEMENT_ARRAY_BUFFER, m_value));

DEF_STATE_RECOVERY_INT(GL_VERTEX_ARRAY_BINDING, GL(glBindVertexArray, m_value));

DEF_STATE_RECOVERY_INT(GL_UNIFORM_BUFFER_BINDING, GL(glBindBuffer, GL_UNIFORM_BUFFER, m_value));

// 指示 glTexImage2D 函数 执行 内存到现存传递数据时 的 alignment, OpenGL 状态机默认为 4
DEF_STATE_RECOVERY_INT(GL_UNPACK_ALIGNMENT, GL(glPixelStorei, GL_UNPACK_ALIGNMENT, m_value));

DEF_STATE_RECOVERY_INT(GL_DEPTH_FUNC, GL(glDepthFunc, m_value));

DEF_STATE_RECOVERY_INT(GL_RENDERBUFFER_BINDING, GL(glBindRenderbuffer, GL_RENDERBUFFER, m_value));

DEF_STATE_RECOVERY_INT(GL_READ_BUFFER, GL(glReadBuffer, m_value));

DEF_STATE_RECOVERY_FLOAT(GL_LINE_WIDTH, GL(glLineWidth, m_value));

DEF_STATE_RECOVERY_FLOAT(GL_POINT_SIZE, GL(glPointSize, m_value));

DEF_STATE_RECOVERY_FLOAT(GL_MIN_SAMPLE_SHADING_VALUE, GL(glMinSampleShading, m_value));

DEF_STATE_RECOVERY_FLOAT(GL_DEPTH_CLEAR_VALUE, GL(glClearDepthf, m_value));

struct BlendFuncSeparateInfo {
    GLboolean enable;
    int srcRGB, dstRGB, srcAlpha, dstAlpha;
};
DEF_STATE_RECOVERY(
    GL_BLEND,
    glGetBooleanv(GL_BLEND, &m_info.enable);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &m_info.dstAlpha);
        glGetIntegerv(GL_BLEND_DST_RGB, &m_info.dstRGB);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &m_info.srcAlpha);
        glGetIntegerv(GL_BLEND_SRC_RGB, &m_info.srcRGB),
    if (m_info.enable) { glEnable(GL_BLEND); glBlendFuncSeparate(m_info.srcRGB, m_info.dstRGB, m_info.srcAlpha, m_info.dstAlpha); }
        else { glDisable(GL_BLEND); },
    BlendFuncSeparateInfo, m_info
);

struct PolygonModeInfo { GLint front, back; };
DEF_STATE_RECOVERY(
    GL_POLYGON_MODE,
    GL(glGetIntegerv, GL_POLYGON_MODE, (GLint*)&m_values),
    glPolygonMode(GL_FRONT, m_values.front); glPolygonMode(GL_BACK, m_values.back),
    PolygonModeInfo, m_values
);

struct ViewportInfo { GLint x, y, w, h; };
DEF_STATE_RECOVERY(
    GL_VIEWPORT,
    GL(glGetIntegerv, GL_VIEWPORT, (GLint*)&m_vp),
    GL(glViewport, m_vp.x, m_vp.y, m_vp.w, m_vp.h),
    ViewportInfo, m_vp
);

struct FrameBufferInfo { GLint draw, read; };
DEF_STATE_RECOVERY(
    GL_FRAMEBUFFER_BINDING,
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &m_info.draw); glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &m_info.read),
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_info.draw); glBindFramebuffer(GL_READ_FRAMEBUFFER, m_info.read),
    FrameBufferInfo, m_info
);	// 说明：GL_FRAMEBUFFER_BINDING 控制了 两个 状态 *DRAW* 和 *READ*，而且 发现 GL_FRAMEBUFFER_BINDING 的值 与 *DRAW* 是一样的，将来要分开时需要注意

DEF_STATE_RECOVERY(
    GL_COLOR_CLEAR_VALUE,
    glGetFloatv(GL_COLOR_CLEAR_VALUE, glm::value_ptr(m_color)),
    glClearColor(m_color[0], m_color[1], m_color[2], m_color[3]),
    glm::vec4, m_color
);

DEF_STATE_RECOVERY(
    GL_SCISSOR_BOX,
    glGetIntegerv(GL_SCISSOR_BOX, glm::value_ptr(m_scissorBox)),
    glScissor(m_scissorBox.x, m_scissorBox.y, m_scissorBox.z, m_scissorBox.w),
    glm::ivec4, m_scissorBox
);

// ======================================================================================
// 外部代码使用 StatesRecovery 类来实现 一个或多个Opengl 状态的 保存与恢复
template<int ... stateTypes>
struct StatesRecovery {
    StatesRecovery() = default;
    ~StatesRecovery() = default;

private:
    std::tuple<StateRecovery<stateTypes>...> m_status;
};


// 当前上下文
struct AutoRecovery_OpenGLContext {
    AutoRecovery_OpenGLContext()
#ifdef  WIN32
        : m_hdc(wglGetCurrentDC())
        , m_hrc(wglGetCurrentContext())
#endif //  WIN32
    {}
    ~AutoRecovery_OpenGLContext() {
#ifdef  WIN32
        wglMakeCurrent(m_hdc, m_hrc);
#endif //  WIN32
    }
private:
#ifdef  WIN32
    HDC m_hdc;
    HGLRC m_hrc;
#endif //  WIN32
};


NAMESPACE_OPENGL_CORE_END
