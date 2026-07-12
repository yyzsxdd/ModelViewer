#include "modelviewer.h"
#include "widget/OglWidget.h"

#include <QMenuBar>
#include <QToolBar>
#include <QAction>

ModelViewer::ModelViewer(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString::fromUtf8("ModelViewer"));
    resize(1200, 800);

    setupUI();
    setupMenuBar();
}

ModelViewer::~ModelViewer() = default;

void ModelViewer::setupUI()
{
    // ---------- 中央 3D 视口 ----------
    m_viewport = new OglWidget(this);
    m_viewport->setMinimumSize(400, 300);
    setCentralWidget(m_viewport);

    // ---------- 左侧模型树 ----------
    m_treeDock = new QDockWidget(QString::fromUtf8(u8"\u6A21\u578B\u6811"), this);
    m_treeDock->setObjectName("ModelTreeDock");
    m_treeDock->setMinimumWidth(200);

    m_modelTree = new QTreeWidget(m_treeDock);
    m_modelTree->setHeaderLabel(QString::fromUtf8(u8"\u6A21\u578B\u7ED3\u6784"));
    m_treeDock->setWidget(m_modelTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_treeDock);

    // ---------- 底部日志输出 ----------
    m_logDock = new QDockWidget(QString::fromUtf8(u8"\u8F93\u51FA\u65E5\u5FD7"), this);
    m_logDock->setObjectName("LogDock");

    m_log = new QPlainTextEdit(m_logDock);
    m_log->setReadOnly(true);
    m_log->setMaximumBlockCount(1000);
    m_logDock->setWidget(m_log);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);
}

void ModelViewer::setupMenuBar()
{
    // ---------- 文件菜单 ----------
    QMenu* fileMenu = menuBar()->addMenu(QString::fromUtf8(u8"\u6587\u4EF6(&F)"));
    QAction* actOpen = fileMenu->addAction(QString::fromUtf8(u8"\u6253\u5F00 STEP..."));
    actOpen->setShortcut(QKeySequence::Open);
    fileMenu->addSeparator();
    fileMenu->addAction(QString::fromUtf8(u8"\u9000\u51FA(&X)"), this, &QWidget::close, QKeySequence::Quit);

    // ---------- 编辑菜单 ----------
    QMenu* editMenu = menuBar()->addMenu(QString::fromUtf8(u8"\u7F16\u8F91(&E)"));
    editMenu->addAction(QString::fromUtf8(u8"\u64A4\u9500"), nullptr, nullptr, QKeySequence::Undo);
    editMenu->addAction(QString::fromUtf8(u8"\u91CD\u505A"), nullptr, nullptr, QKeySequence::Redo);

    // ---------- 视图菜单 ----------
    QMenu* viewMenu = menuBar()->addMenu(QString::fromUtf8(u8"\u89C6\u56FE(&V)"));
    viewMenu->addAction(m_treeDock->toggleViewAction());
    viewMenu->addAction(m_logDock->toggleViewAction());

    // ---------- 帮助菜单 ----------
    menuBar()->addMenu(QString::fromUtf8(u8"\u5E2E\u52A9(&H)"));

    // ---------- 工具栏 ----------
    QToolBar* toolbar = addToolBar(QString::fromUtf8(u8"\u4E3B\u5DE5\u5177\u680F"));
    toolbar->addAction(actOpen);
}
