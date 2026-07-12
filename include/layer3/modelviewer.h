#pragma once

#include <QMainWindow>
#include <QTreeWidget>
#include <QPlainTextEdit>
#include <QDockWidget>

class OglWidget;

class ModelViewer : public QMainWindow
{
    Q_OBJECT

public:
    explicit ModelViewer(QWidget *parent = nullptr);
    ~ModelViewer() override;

private:
    void setupUI();
    void setupMenuBar();

    OglWidget*      m_viewport   = nullptr;
    QTreeWidget*    m_modelTree  = nullptr;
    QPlainTextEdit* m_log        = nullptr;
    QDockWidget*    m_treeDock   = nullptr;
    QDockWidget*    m_logDock    = nullptr;
};

