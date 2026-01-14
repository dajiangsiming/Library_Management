// main.cpp
#include "librarymanager.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 设置应用程序信息
    app.setApplicationName("Library Management System");
    app.setOrganizationName("LibrarySoft");
    app.setApplicationVersion("1.0.0");

    LibraryManager window;
    window.setWindowTitle("图书馆管理系统");
    window.show();

    return app.exec();
}
