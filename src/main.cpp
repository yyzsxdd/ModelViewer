#include <iostream>
#include "modelviewer.h"
#include <QApplication>
int main(int argc, char* argv[])
{
    QApplication a(argc,argv);
//    return 0;
    ModelViewer w;
    w.show();
    return a.exec();
}