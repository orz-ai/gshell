#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTabWidget>
#include <QSplitter>
#include <QTreeWidget>
#include <QAction>
#include "terminalwidget.h"
#include "fileexplorerwidget.h"
#include "sessionmanager.h"
#include "sessiondialog.h"
#include "sessionmanagerdialog.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    // 连接到会话的方法
    void connectToSession(const QString &host, int port, const QString &username, const QString &password);
    void connectToSessionWithKey(const QString &host, int port, const QString &username, const QString &keyFile, const QString &passphrase);

private slots:
    void newSession();
    void closeSession(int index);
    void about();
    void showSettings();
    void loadSavedSessions();
    void onSessionItemDoubleClicked(QTreeWidgetItem *item, int column);
    void openSession(const SessionInfo &session);
    void disconnectCurrentSession();
    void showSessionManager();
    void toggleSftpExplorer();

private:
    Ui::MainWindow *ui;
    QTabWidget *tabWidget;
    QTreeWidget *sessionTreeWidget;
    QSplitter *mainSplitter;
    SessionManager *sessionManager;
    QAction *connectAction;
    QAction *disconnectAction;
    
    void setupMenus();
    void setupToolbar();
    void createNewTab(const SessionInfo &session);
    void populateSessionTree();
};
#endif // MAINWINDOW_H 