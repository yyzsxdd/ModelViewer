#include <QMainWindow>
#include <QWidget>
#include "ui_qtcmake.h"
class ModelViewer : public QMainWindow
{
    Q_OBJECT

public:
    ModelViewer(QWidget *parent = nullptr);
    ~ModelViewer();
private:
    Ui::Form ui_;

};

