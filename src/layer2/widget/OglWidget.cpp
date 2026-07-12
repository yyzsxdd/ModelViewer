#include "widget/OglWidget.h"

OglWidget::OglWidget(QWidget *parent)
    : QOpenGLWidget(parent)
{
    // 设置 surface 格式（如需多重采样等）
    QSurfaceFormat fmt;
    fmt.setSamples(4);
    fmt.setDepthBufferSize(24);
    setFormat(fmt);
}

OglWidget::~OglWidget()
{
    makeCurrent();
    cleanupGL();
    doneCurrent();
}

void OglWidget::initializeGL()
{
    QOpenGLWidget::initializeGL();
}

void OglWidget::resizeGL(int w, int h)
{
    QOpenGLWidget::resizeGL(w, h);
}

void OglWidget::paintGL()
{
    QOpenGLWidget::paintGL();
}

void OglWidget::cleanupGL()
{
    // 子类重写以释放纹理、VBO 等 GPU 资源
}
