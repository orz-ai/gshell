#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QAction>
#include <QMenuBar>
#include <QToolBar>
#include <QMessageBox>
#include <QSplitter>
#include <QVBoxLayout>
#include "sessiondialog.h"
#include "sessionmanagerdialog.h"
#include <QApplication>
#include <QStyle>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    

    sessionManager = new SessionManager(this);
    

    mainSplitter = new QSplitter(Qt::Horizontal, this);
    setCentralWidget(mainSplitter);
    

    sessionTreeWidget = new QTreeWidget(this);
    sessionTreeWidget->setHeaderLabel(tr("Sessions"));
    sessionTreeWidget->setMinimumWidth(200);
    sessionTreeWidget->setMaximumWidth(300);
    

    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);

    mainSplitter->addWidget(sessionTreeWidget);
    mainSplitter->addWidget(tabWidget);
    

    mainSplitter->setSizes(QList<int>() << 200 << width() - 200);

    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MainWindow::closeSession);
    connect(sessionTreeWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::onSessionItemDoubleClicked);
    

    loadSavedSessions();

    setupMenus();
    setupToolbar();
    

    resize(1200, 800);
    setWindowTitle("gshell - SSH Client");
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupMenus()
{

    QMenu *fileMenu = menuBar()->addMenu(tr("File"));
    
    QAction *newSessionAction = fileMenu->addAction(tr("New Session"));
    connect(newSessionAction, &QAction::triggered, this, &MainWindow::newSession);
    
    QAction *manageSessionsAction = fileMenu->addAction(tr("Manage Sessions"));
    connect(manageSessionsAction, &QAction::triggered, this, &MainWindow::showSessionManager);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction(tr("Exit"));
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    

    QMenu *editMenu = menuBar()->addMenu(tr("Edit"));
    QAction *settingsAction = editMenu->addAction(tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
    

    QMenu *helpMenu = menuBar()->addMenu(tr("Help"));
    QAction *aboutAction = helpMenu->addAction(tr("About"));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::about);
}

void MainWindow::setupToolbar()
{
    QToolBar *mainToolBar = addToolBar(tr("Main Toolbar"));
    mainToolBar->setIconSize(QSize(24, 24));
    
    connectAction = mainToolBar->addAction(QIcon(":/icons/connect.svg"), tr("Connect"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::newSession);
    
    disconnectAction = mainToolBar->addAction(QIcon(":/icons/disconnect.svg"), tr("Disconnect"));
    connect(disconnectAction, &QAction::triggered, this, &MainWindow::disconnectCurrentSession);
    disconnectAction->setEnabled(false);
    
    mainToolBar->addSeparator();
    
    // 添加SFTP按钮
    QAction *sftpAction = mainToolBar->addAction(QIcon(":/icons/folder.svg"), tr("SFTP"));
    connect(sftpAction, &QAction::triggered, this, &MainWindow::toggleSftpExplorer);
    
    mainToolBar->addSeparator();
    
    QAction *settingsAction = mainToolBar->addAction(QIcon(":/icons/settings.svg"), tr("Settings"));
    connect(settingsAction, &QAction::triggered, this, &MainWindow::showSettings);
}

void MainWindow::newSession()
{
    SessionDialog dialog(this);
    

    SessionInfo defaultSession;
    defaultSession.name = "New Session";
    defaultSession.host = "";
    defaultSession.port = 22;
    defaultSession.username = "";
    defaultSession.savePassword = false;
    
    dialog.setSessionInfo(defaultSession);
    
    if (dialog.exec() == QDialog::Accepted) {
        SessionInfo session = dialog.getSessionInfo();
        

        sessionManager->saveSession(session);
        

        createNewTab(session);
    }
}

void MainWindow::createNewTab(const SessionInfo &session)
{
    // Create a splitter for the tab content
    QSplitter *tabSplitter = new QSplitter(Qt::Vertical);
    
    // Create terminal widget
    TerminalWidget *terminal = new TerminalWidget(tabSplitter);
    
    // Create file explorer widget (默认隐藏)
    FileExplorerWidget *fileExplorer = new FileExplorerWidget(tabSplitter);
    
    // Add widgets to splitter
    tabSplitter->addWidget(terminal);
    tabSplitter->addWidget(fileExplorer);
    tabSplitter->setSizes(QList<int>() << 300 << 0);  // 初始时设置文件浏览器高度为0（隐藏）
    
    // Add the tab
    int index = tabWidget->addTab(tabSplitter, session.name);
    tabWidget->setCurrentIndex(index);
    
    // Enable disconnect button
    disconnectAction->setEnabled(true);
    
    // Connect to the server (in a real app, we would use the session info to connect)
    qDebug() << "Connecting to session:" << session.name;
    
    // Connect to SSH server
    terminal->connectToSession(session);
    
    // 连接文件浏览器的状态改变信号
    connect(fileExplorer, &FileExplorerWidget::sftpStatusChanged, this, [=](bool /*connected*/, const QString &message) {
        statusBar()->showMessage(message, 3000);
    });
}

void MainWindow::closeSession(int index)
{
    // In a real implementation, we would disconnect from the server first
    
    // Remove the tab
    tabWidget->removeTab(index);
    
    // Disable disconnect button if no tabs are open
    if (tabWidget->count() == 0) {
        disconnectAction->setEnabled(false);
    }
}

void MainWindow::about()
{
    QMessageBox::about(this, tr("About gshell"),
                       tr("gshell\n\n"
                          "A simple SSH client\n\n"
                          "Version: 1.0.0\n"
                          "Author: Your Name"));
}

void MainWindow::showSettings()
{

    QMessageBox::information(this, tr("Settings"), tr("Settings feature not yet implemented"));
}

void MainWindow::loadSavedSessions()
{
    populateSessionTree();
}

void MainWindow::populateSessionTree()
{
    sessionTreeWidget->clear();
    
    QList<SessionInfo> sessions = sessionManager->getSessions();
    
    QTreeWidgetItem *rootItem = new QTreeWidgetItem(sessionTreeWidget, QStringList() << tr("Sessions"));
    rootItem->setExpanded(true);
    
    for (const SessionInfo &session : sessions) {
        QString displayName = session.name.isEmpty() ? 
            QString("%1@%2:%3").arg(session.username, session.host).arg(session.port) : 
            session.name;
        
        QTreeWidgetItem *sessionItem = new QTreeWidgetItem(rootItem, QStringList() << displayName);
        sessionItem->setData(0, Qt::UserRole, session.name);
        sessionItem->setIcon(0, QIcon(":/icons/server.png"));
    }
    
    sessionTreeWidget->addTopLevelItem(rootItem);
}

void MainWindow::onSessionItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (item->parent()) {
        QString sessionName = item->data(0, Qt::UserRole).toString();
        SessionInfo session = sessionManager->getSession(sessionName);
        
        openSession(session);
    }
}

void MainWindow::openSession(const SessionInfo &session)
{

    for (int i = 0; i < tabWidget->count(); ++i) {
        if (tabWidget->tabText(i) == session.name) {
            tabWidget->setCurrentIndex(i);
            return;
        }
    }

    createNewTab(session);
}

void MainWindow::disconnectCurrentSession()
{
    int currentIndex = tabWidget->currentIndex();
    if (currentIndex >= 0) {
        // Get the tab widget
        QWidget *tabWidget = this->tabWidget->widget(currentIndex);
        
        // Disconnect from SSH server
        QSplitter *splitter = qobject_cast<QSplitter*>(tabWidget);
        if (splitter) {
            TerminalWidget *terminal = qobject_cast<TerminalWidget*>(splitter->widget(0));
            if (terminal) {
                terminal->disconnectFromSession();
            }
        }
        
        // In a real implementation, we would disconnect from the server
        qDebug() << "Disconnecting from session:" << this->tabWidget->tabText(currentIndex);
        
        // Close the tab
        closeSession(currentIndex);
    }
}

void MainWindow::showSessionManager()
{
    SessionManagerDialog dialog(sessionManager, this);
    connect(&dialog, &SessionManagerDialog::sessionSelected, this, &MainWindow::openSession);
    dialog.exec();
}

void MainWindow::connectToSession(const QString &host, int port, const QString &username, const QString &password)
{
    // Create a new terminal tab
    TerminalWidget *terminal = new TerminalWidget(this);
    
    // 创建 SessionInfo 对象
    SessionInfo sessionInfo;
    sessionInfo.host = host;
    sessionInfo.port = port;
    sessionInfo.username = username;
    sessionInfo.password = password;
    sessionInfo.authType = 0; // 密码认证
    
    // 设置默认的终端外观
    sessionInfo.fontName = "Consolas";
    sessionInfo.fontSize = 10;
    sessionInfo.backgroundColor = "#1E1E1E";
    sessionInfo.textColor = "#DCDCDC";
    
    // 连接到会话
    terminal->connectToSession(sessionInfo);
    
    // Add to tab widget
    int index = tabWidget->addTab(terminal, host);
    tabWidget->setCurrentIndex(index);
    
    // Enable disconnect action
    disconnectAction->setEnabled(true);
}

void MainWindow::connectToSessionWithKey(const QString &host, int port, const QString &username, const QString &keyFile, const QString &passphrase)
{
    // Create a new terminal tab
    TerminalWidget *terminal = new TerminalWidget(this);
    
    // 创建 SessionInfo 对象
    SessionInfo sessionInfo;
    sessionInfo.host = host;
    sessionInfo.port = port;
    sessionInfo.username = username;
    sessionInfo.keyFile = keyFile;
    sessionInfo.password = passphrase; // 密钥密码
    sessionInfo.authType = 1; // 密钥认证
    
    // 设置默认的终端外观
    sessionInfo.fontName = "Consolas";
    sessionInfo.fontSize = 10;
    sessionInfo.backgroundColor = "#1E1E1E";
    sessionInfo.textColor = "#DCDCDC";
    
    // 连接到会话
    terminal->connectToSession(sessionInfo);
    
    // Add to tab widget
    int index = tabWidget->addTab(terminal, host);
    tabWidget->setCurrentIndex(index);
    
    // Enable disconnect action
    disconnectAction->setEnabled(true);
}

void MainWindow::toggleSftpExplorer()
{
    if (tabWidget->count() == 0) {
        QMessageBox::warning(this, tr("SFTP Explorer"), tr("Please connect to a session first"));
        return;
    }
    
    // 获取当前标签
    QSplitter *tabSplitter = qobject_cast<QSplitter*>(tabWidget->currentWidget());
    if (!tabSplitter || tabSplitter->count() < 2) {
        return;
    }
    
    // 获取文件浏览器
    FileExplorerWidget *fileExplorer = qobject_cast<FileExplorerWidget*>(tabSplitter->widget(1));
    if (!fileExplorer) {
        return;
    }
    
    // 获取当前会话
    int currentIndex = tabWidget->currentIndex();
    QString sessionName = tabWidget->tabText(currentIndex);
    
    // 获取会话信息
    SessionInfo sessionInfo;
    QList<SessionInfo> sessions = sessionManager->getSessions();
    for (const SessionInfo &session : sessions) {
        if (session.name == sessionName) {
            sessionInfo = session;
            break;
        }
    }
    
    if (sessionInfo.name.isEmpty()) {
        QMessageBox::warning(this, tr("SFTP Explorer"), tr("Session information not found"));
        return;
    }
    
    // 切换文件浏览器可见性
    if (fileExplorer->isVisible()) {
        fileExplorer->hideExplorer();
        tabSplitter->setSizes(QList<int>() << tabSplitter->height() << 0);
    } else {
        // 显示文件浏览器，同时连接SFTP（如果尚未连接）
        fileExplorer->showExplorer();
        tabSplitter->setSizes(QList<int>() << tabSplitter->height() / 2 << tabSplitter->height() / 2);
        
        // 连接SFTP
        fileExplorer->connectToSftp(sessionInfo.host, sessionInfo.port, sessionInfo.username, 
                                    sessionInfo.password.isEmpty() ? QString() : sessionInfo.password);
    }
} 
