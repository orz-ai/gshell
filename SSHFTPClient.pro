QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

TARGET = gshell
TEMPLATE = app

SOURCES += \
    fileexplorerwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    sessiondialog.cpp \
    sessionmanager.cpp \
    sessionmanagerdialog.cpp \
    sshclient.cpp \
    sshconnectionthread.cpp \
    terminalwidget.cpp

HEADERS += \
    fileexplorerwidget.h \
    mainwindow.h \
    sessioninfo.h \
    sessiondialog.h \
    sessionmanager.h \
    sessionmanagerdialog.h \
    sshclient.h \
    sshconnectionthread.h \
    terminalwidget.h

FORMS += \
    mainwindow.ui \
    sessiondialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resources.qrc

# Compiler flags based on compiler type
unix|macx|mingw {
    QMAKE_CXXFLAGS += -Wno-deprecated-declarations
}

# For Windows
win32 {
    LIBS += -lws2_32
    
    # 在Windows上，您需要指定libssh2的路径
    # 请根据您的实际安装路径修改
    LIBS += -L"D:/project/other/vcpkg-master/packages/libssh2_x64-windows/lib" -llibssh2
    INCLUDEPATH += "D:/project/other/vcpkg-master/packages/libssh2_x64-windows/include"
    
    # 使用 windeployqt 部署 Qt 依赖项
    CONFIG(debug, debug|release) {
        DEPLOY_TARGET = $$shell_path($$OUT_PWD/debug/gshell.exe)
        QMAKE_POST_LINK += $$quote(windeployqt --debug $$DEPLOY_TARGET $$escape_expand(\n\t))
        
        # 手动复制 libssh2 和 OpenSSL DLL
        LIBSSH2_DLL = $$shell_path(D:/project/other/vcpkg-master/packages/libssh2_x64-windows/bin/libssh2.dll)
        OPENSSL_DLL1 = $$shell_path(D:/project/other/vcpkg-master/packages/openssl_x64-windows/bin/libssl-3-x64.dll)
        OPENSSL_DLL2 = $$shell_path(D:/project/other/vcpkg-master/packages/openssl_x64-windows/bin/libcrypto-3-x64.dll)
        ZLIB_DLL = $$shell_path(D:/project/other/vcpkg-master/packages/zlib_x64-windows/bin/zlib1.dll)
        
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$LIBSSH2_DLL $$dirname(DEPLOY_TARGET) $$escape_expand(\n\t))
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$OPENSSL_DLL1 $$dirname(DEPLOY_TARGET) $$escape_expand(\n\t))
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$OPENSSL_DLL2 $$dirname(DEPLOY_TARGET) $$escape_expand(\n\t))
        QMAKE_POST_LINK += $$quote(cmd /c copy /y $$ZLIB_DLL $$dirname(DEPLOY_TARGET) $$escape_expand(\n\t))
    }
}

# 对于libssh2的依赖
# 在Windows上，您需要指定libssh2的路径
# LIBS += -L$$PWD/lib/ -llibssh2
# INCLUDEPATH += $$PWD/include

# 在Linux上，通常可以使用系统库
unix:!macx: LIBS += -lssh2

# 默认规则使应用程序的翻译可以被撤销
#TRANSLATIONS += \
#    gshell_zh_CN.ts
