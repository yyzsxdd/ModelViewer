#pragma once

#include <QOpenGLWidget>

/**
 * @brief OpenGL 视口控件
 *
 * 继承自 QOpenGLWidget，封装 OpenGL 渲染上下文及基础渲染循环。
 * 属于 layer2 组件层，供 layer3 界面层嵌入使用。
 */
class OglWidget : public QOpenGLWidget
{
    Q_OBJECT

public:
    explicit OglWidget(QWidget *parent = nullptr);
    ~OglWidget() override;

protected:
    /// 初始化 OpenGL 资源和状态
    void initializeGL() override;

    /// 窗口尺寸变化时调整视口和投影
    void resizeGL(int w, int h) override;

    /// 每帧渲染回调
    void paintGL() override;

    /// 清理 OpenGL 资源
    void cleanupGL();
};
