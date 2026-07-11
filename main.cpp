#include <iostream>
#include "mainwin.h"
#include <QApplication>
int main(int argc, char* argv[])
{
    QApplication a(argc,argv);
//    return 0;
    MainWin w;
    w.show();
    return a.exec();
}