#include <QMainWindow>
#include <QWidget>
#include "ui_qtcmake.h"
class MainWin : public QMainWindow
{
    Q_OBJECT

public:
    MainWin(QWidget *parent = nullptr);
    ~MainWin();
private:
    Ui::Form ui_;

};

