#include "fileexplorerwidget.h"
#include <QAction>
#include <QDir>
#include <QMessageBox>
#include <QLabel>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QInputDialog>
#include <QFileInfo>
#include <QStandardItem>
#include <QPushButton>
#include <QDrag>
#include <QMenu>
#include <QDateTime>
#include <QProgressBar>
#include <QToolButton>

FileExplorerWidget::FileExplorerWidget(QWidget *parent) : QWidget(parent), connected(false), isLocalDragSource(false), nextTaskId(1), currentTaskId(-1)
{
    ftpClient = new FTPClient(this);
    
    // 连接FTP客户端信号
    connect(ftpClient, &FTPClient::connected, this, &FileExplorerWidget::onSftpConnected);
    connect(ftpClient, &FTPClient::disconnected, this, &FileExplorerWidget::onSftpDisconnected);
    connect(ftpClient, &FTPClient::error, this, &FileExplorerWidget::onSftpError);
    connect(ftpClient, &FTPClient::directoryListed, this, &FileExplorerWidget::onDirectoryListed);
    connect(ftpClient, &FTPClient::transferProgress, this, &FileExplorerWidget::onTransferProgress);
    connect(ftpClient, &FTPClient::transferCompleted, this, &FileExplorerWidget::onTransferCompleted);
    
    setupUI();
    
    // 初始时隐藏file explorer
    this->hide();
    
    // 设置初始远程路径
    currentRemotePath = "/";
    
    // 启用widget的拖放
    setAcceptDrops(true);
}

void FileExplorerWidget::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    toolBar = new QToolBar(this);
    toolBar->setIconSize(QSize(16, 16));
    setupToolbar();
    mainLayout->addWidget(toolBar);
    
    // 创建主分割器 - 上半部分是文件浏览器，下半部分是传输面板
    mainSplitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(mainSplitter);
    
    // 创建文件浏览器分割器
    splitter = new QSplitter(Qt::Vertical, mainSplitter);
    
    // 设置本地文件系统模型
    localFileModel = new QFileSystemModel(this);
    localFileModel->setRootPath(QDir::homePath());
    
    // 本地文件视图
    QWidget *localWidget = new QWidget(splitter);
    QVBoxLayout *localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(0, 0, 0, 0);
    
    // 添加带有路径输入框的标题区域
    QWidget *localHeaderWidget = new QWidget(localWidget);
    QHBoxLayout *localHeaderLayout = new QHBoxLayout(localHeaderWidget);
    localHeaderLayout->setContentsMargins(4, 4, 4, 4);
    
    QLabel *localLabel = new QLabel(tr("Local Files"), localHeaderWidget);
    localLabel->setStyleSheet("QLabel { color: white; }");
    
    localPathEdit = new QLineEdit(localHeaderWidget);
    localPathEdit->setPlaceholderText(tr("Enter local path..."));
    localPathEdit->setText(QDir::homePath());
    
    QPushButton *localGoButton = new QPushButton(tr("Go"), localHeaderWidget);
    localGoButton->setMaximumWidth(40);
    
    localHeaderLayout->addWidget(localLabel);
    localHeaderLayout->addWidget(localPathEdit, 1);  // 1是伸展因子
    localHeaderLayout->addWidget(localGoButton);
    
    localHeaderWidget->setStyleSheet("background-color: #2D2D30;");
    
    // 连接本地路径输入框的信号
    connect(localPathEdit, &QLineEdit::returnPressed, this, &FileExplorerWidget::onLocalPathEntered);
    connect(localGoButton, &QPushButton::clicked, this, &FileExplorerWidget::onLocalPathEntered);
    
    localFileView = new QTreeView(localWidget);
    localFileView->setModel(localFileModel);
    localFileView->setRootIndex(localFileModel->index(QDir::homePath()));
    localFileView->setSortingEnabled(true);
    localFileView->setColumnWidth(0, 250);
    localFileView->setStyleSheet("QTreeView { background-color: #1E1E1E; color: #DCDCDC; }");
    
    // 启用本地文件视图的拖放
    localFileView->setDragEnabled(true);
    localFileView->setAcceptDrops(true);
    localFileView->setDropIndicatorShown(true);
    localFileView->setDragDropMode(QAbstractItemView::DragDrop);
    
    // 安装事件过滤器
    localFileView->viewport()->installEventFilter(this);
    
    localLayout->addWidget(localHeaderWidget);
    localLayout->addWidget(localFileView);
    
    // 远程文件视图，使用标准项模型
    QWidget *remoteWidget = new QWidget(splitter);
    QVBoxLayout *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    
    // 添加带有路径输入框的标题区域
    QWidget *remoteHeaderWidget = new QWidget(remoteWidget);
    QHBoxLayout *remoteHeaderLayout = new QHBoxLayout(remoteHeaderWidget);
    remoteHeaderLayout->setContentsMargins(4, 4, 4, 4);
    
    QLabel *remoteLabel = new QLabel(tr("Remote Files"), remoteHeaderWidget);
    remoteLabel->setStyleSheet("QLabel { color: white; }");
    
    remotePathEdit = new QLineEdit(remoteHeaderWidget);
    remotePathEdit->setPlaceholderText(tr("Enter remote path..."));
    remotePathEdit->setText("/");
    
    QPushButton *remoteGoButton = new QPushButton(tr("Go"), remoteHeaderWidget);
    remoteGoButton->setMaximumWidth(40);
    
    remoteHeaderLayout->addWidget(remoteLabel);
    remoteHeaderLayout->addWidget(remotePathEdit, 1);  // 1是伸展因子
    remoteHeaderLayout->addWidget(remoteGoButton);
    
    remoteHeaderWidget->setStyleSheet("background-color: #2D2D30;");
    
    // 连接远程路径输入框的信号
    connect(remotePathEdit, &QLineEdit::returnPressed, this, &FileExplorerWidget::onRemotePathEntered);
    connect(remoteGoButton, &QPushButton::clicked, this, &FileExplorerWidget::onRemotePathEntered);
    
    // 创建远程文件模型
    remoteFileModel = new QStandardItemModel(this);
    QStringList headers;
    headers << tr("Name") << tr("Size") << tr("Type") << tr("Date Modified");
    remoteFileModel->setHorizontalHeaderLabels(headers);
    
    remoteFileView = new QTreeView(remoteWidget);
    remoteFileView->setModel(remoteFileModel);
    remoteFileView->setSortingEnabled(true);
    remoteFileView->setColumnWidth(0, 250);
    remoteFileView->setStyleSheet("QTreeView { background-color: #1E1E1E; color: #DCDCDC; }");
    
    // 启用远程文件视图的拖放
    remoteFileView->setDragEnabled(true);
    remoteFileView->setAcceptDrops(true);
    remoteFileView->setDropIndicatorShown(true);
    remoteFileView->setDragDropMode(QAbstractItemView::DragDrop);
    
    // 安装事件过滤器
    remoteFileView->viewport()->installEventFilter(this);
    
    connect(remoteFileView, &QTreeView::doubleClicked, this, &FileExplorerWidget::onRemoteDoubleClicked);
    
    remoteLayout->addWidget(remoteHeaderWidget);
    remoteLayout->addWidget(remoteFileView);
    
    // 添加到分割器
    splitter->addWidget(localWidget);
    splitter->addWidget(remoteWidget);
    splitter->setSizes(QList<int>() << height()/2 << height()/2);

    // 连接本地文件视图的双击事件
    connect(localFileView, &QTreeView::doubleClicked, this, [this](const QModelIndex &index) {
        if (localFileModel->isDir(index)) {
            QString path = localFileModel->filePath(index);
            changeLocalDirectory(path);
        }
    });
    
    // 设置远程文件视图的自定义上下文菜单
    remoteFileView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(remoteFileView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex index = remoteFileView->indexAt(pos);
        if (index.isValid()) {
            QMenu menu(this);
            QAction *downloadAction = menu.addAction(tr("Download"));
            connect(downloadAction, &QAction::triggered, this, &FileExplorerWidget::downloadFile);
            menu.exec(remoteFileView->viewport()->mapToGlobal(pos));
        }
    });
    
    // 设置本地文件视图的自定义上下文菜单
    localFileView->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(localFileView, &QTreeView::customContextMenuRequested, this, [this](const QPoint &pos) {
        QModelIndex index = localFileView->indexAt(pos);
        if (index.isValid()) {
            QMenu menu(this);
            QAction *uploadAction = menu.addAction(tr("Upload"));
            connect(uploadAction, &QAction::triggered, this, &FileExplorerWidget::uploadFile);
            menu.exec(localFileView->viewport()->mapToGlobal(pos));
        }
    });
    
    // 设置传输面板
    setupTransferPanel();
    
    // 添加文件浏览器和传输面板到主分割器
    mainSplitter->addWidget(splitter);
    mainSplitter->addWidget(transferWidget);
    
    // 设置主分割器的初始大小比例
    mainSplitter->setSizes(QList<int>() << (height() * 2 / 3) << (height() / 3));
}

void FileExplorerWidget::setupTransferPanel()
{
    // 创建传输面板
    transferWidget = new QWidget(mainSplitter);
    QVBoxLayout *transferLayout = new QVBoxLayout(transferWidget);
    transferLayout->setContentsMargins(0, 0, 0, 0);
    
    // 传输面板标题栏
    QWidget *transferHeader = new QWidget(transferWidget);
    QHBoxLayout *headerLayout = new QHBoxLayout(transferHeader);
    headerLayout->setContentsMargins(4, 4, 4, 4);
    
    QLabel *transferLabel = new QLabel(tr("Transfers"), transferHeader);
    transferLabel->setStyleSheet("QLabel { color: white; font-weight: bold; }");
    
    QToolButton *clearButton = new QToolButton(transferHeader);
    clearButton->setIcon(QIcon(":/icons/delete.svg"));
    clearButton->setToolTip(tr("Clear Completed Transfers"));
    connect(clearButton, &QToolButton::clicked, this, &FileExplorerWidget::clearCompletedTransfers);
    
    headerLayout->addWidget(transferLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(clearButton);
    
    transferHeader->setStyleSheet("background-color: #2D2D30;");
    
    // 传输列表
    transferList = new QListWidget(transferWidget);
    transferList->setStyleSheet("QListWidget { background-color: #1E1E1E; color: #DCDCDC; }");
    
    // 设置上下文菜单
    transferList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(transferList, &QListWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
        QListWidgetItem *item = transferList->itemAt(pos);
        if (item) {
            QMenu menu(this);
            QAction *cancelAction = menu.addAction(tr("Cancel"));
            connect(cancelAction, &QAction::triggered, this, &FileExplorerWidget::cancelTransfer);
            menu.exec(transferList->viewport()->mapToGlobal(pos));
        }
    });
    
    transferLayout->addWidget(transferHeader);
    transferLayout->addWidget(transferList);
}

void FileExplorerWidget::setupToolbar()
{
    QAction *uploadAction = toolBar->addAction(QIcon(":/icons/upload.svg"), tr("Upload"));
    connect(uploadAction, &QAction::triggered, this, &FileExplorerWidget::uploadFile);
    
    QAction *downloadAction = toolBar->addAction(QIcon(":/icons/download.svg"), tr("Download"));
    connect(downloadAction, &QAction::triggered, this, &FileExplorerWidget::downloadFile);
    
    toolBar->addSeparator();
    
    QAction *newDirAction = toolBar->addAction(QIcon(":/icons/folder.svg"), tr("New Folder"));
    connect(newDirAction, &QAction::triggered, this, &FileExplorerWidget::createDirectory);
    
    QAction *deleteAction = toolBar->addAction(QIcon(":/icons/delete.svg"), tr("Delete"));
    connect(deleteAction, &QAction::triggered, this, &FileExplorerWidget::deleteItem);
    
    toolBar->addSeparator();
    
    QAction *refreshAction = toolBar->addAction(QIcon(":/icons/refresh.svg"), tr("Refresh"));
    connect(refreshAction, &QAction::triggered, this, &FileExplorerWidget::refreshView);
}

void FileExplorerWidget::connectToSftp(const QString &host, int port, const QString &username, const QString &password)
{
    // 连接到SFTP服务器（异步操作，不会阻塞UI）
    QMetaObject::invokeMethod(this, [=]() {
        emit sftpStatusChanged(false, tr("Connecting to %1...").arg(host));
        
        if (ftpClient->connect(host, port, username, password)) {
            // 连接成功后，列出根目录的内容
            ftpClient->listDirectory(currentRemotePath);
        } else {
            emit sftpStatusChanged(false, tr("Failed to connect to %1").arg(host));
        }
    }, Qt::QueuedConnection);
}

void FileExplorerWidget::showExplorer()
{
    this->show();
}

void FileExplorerWidget::hideExplorer()
{
    this->hide();
}

bool FileExplorerWidget::isVisible() const
{
    return QWidget::isVisible();
}

void FileExplorerWidget::onSftpConnected()
{
    connected = true;
    remotePathEdit->setText(currentRemotePath);
    emit sftpStatusChanged(true, tr("Connected to SFTP server"));
}

void FileExplorerWidget::onSftpDisconnected()
{
    connected = false;
    emit sftpStatusChanged(false, tr("Disconnected from SFTP server"));
    
    // 清空远程文件列表
    remoteFileModel->clear();
    QStringList headers;
    headers << tr("Name") << tr("Size") << tr("Type") << tr("Date Modified");
    remoteFileModel->setHorizontalHeaderLabels(headers);
}

void FileExplorerWidget::onSftpError(const QString &errorMessage)
{
    emit sftpStatusChanged(false, errorMessage);
    QMessageBox::warning(this, tr("SFTP Error"), errorMessage);
}

void FileExplorerWidget::onDirectoryListed(const QStringList &entries)
{
    populateRemoteView(entries);
    remotePathEdit->setText(currentRemotePath);
}

void FileExplorerWidget::populateRemoteView(const QStringList &entries)
{
    // 清空模型
    remoteFileModel->clear();
    QStringList headers;
    headers << tr("Name") << tr("Size") << tr("Type") << tr("Date Modified");
    remoteFileModel->setHorizontalHeaderLabels(headers);
    
    // 添加父目录项（如果不是根目录）
    if (currentRemotePath != "/") {
        QStandardItem *parentItem = new QStandardItem("..");
        parentItem->setData("..", Qt::UserRole);
        parentItem->setData("parent", Qt::UserRole + 1);
        parentItem->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileDialogToParent));
        remoteFileModel->appendRow(parentItem);
    }
    
    // 填充目录和文件
    for (const QString &entry : entries) {
        QStringList parts = entry.split("|");
        QString name = parts.count() > 0 ? parts[0] : entry;
        QString size = parts.count() > 1 ? parts[1] : "";
        QString type = parts.count() > 2 ? parts[2] : "";
        QString date = parts.count() > 3 ? parts[3] : "";
        
        bool isDirectory = name.endsWith("/") || type == "directory";
        if (isDirectory && name.endsWith("/")) {
            name = name.left(name.length() - 1);
        }
        
        QList<QStandardItem*> items;
        QStandardItem *nameItem = new QStandardItem(name);
        nameItem->setData(name, Qt::UserRole);
        nameItem->setData(isDirectory ? "directory" : "file", Qt::UserRole + 1);
        nameItem->setIcon(isDirectory ? 
                         QApplication::style()->standardIcon(QStyle::SP_DirIcon) : 
                         QApplication::style()->standardIcon(QStyle::SP_FileIcon));
        
        items.append(nameItem);
        items.append(new QStandardItem(size));
        items.append(new QStandardItem(isDirectory ? tr("Folder") : tr("File")));
        items.append(new QStandardItem(date));
        
        remoteFileModel->appendRow(items);
    }
}

void FileExplorerWidget::onRemoteDoubleClicked(const QModelIndex &index)
{
    QStandardItem *item = remoteFileModel->itemFromIndex(index);
    if (!item) return;
    
    QString itemType = item->data(Qt::UserRole + 1).toString();
    QString itemName = item->data(Qt::UserRole).toString();
    
    if (itemType == "directory") {
        // 切换到目录
        QString newPath;
        if (itemName == "..") {
            // 上一级目录
            int lastSlash = currentRemotePath.lastIndexOf('/');
            if (lastSlash > 0) {
                newPath = currentRemotePath.left(lastSlash);
            } else {
                newPath = "/";
            }
        } else {
            // 子目录
            newPath = currentRemotePath;
            if (!newPath.endsWith("/")) newPath += "/";
            newPath += itemName;
        }
        
        changeSftpDirectory(newPath);
    } else if (itemType == "parent") {
        // 上一级目录
        int lastSlash = currentRemotePath.lastIndexOf('/');
        if (lastSlash > 0) {
            changeSftpDirectory(currentRemotePath.left(lastSlash));
        } else {
            changeSftpDirectory("/");
        }
    }
}

void FileExplorerWidget::changeSftpDirectory(const QString &path)
{
    if (!connected) {
        QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
        return;
    }
    
    currentRemotePath = path;
    remotePathEdit->setText(path);
    ftpClient->listDirectory(currentRemotePath);
}

void FileExplorerWidget::uploadFile()
{
    // 检查是否已连接
    if (!connected) {
        QMessageBox::warning(this, tr("Upload File"), tr("Not connected to SFTP server"));
        return;
    }
    
    // 获取本地视图中选择的文件
    QModelIndex selectedIndex = localFileView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, tr("Upload File"), tr("Please select a file to upload"));
        return;
    }
    
    QString filePath = localFileModel->filePath(selectedIndex);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) {
        QMessageBox::warning(this, tr("Upload File"), tr("Please select a file, not a directory"));
        return;
    }
    
    // 构建远程路径
    QString remotePath = currentRemotePath;
    if (!remotePath.endsWith("/")) remotePath += "/";
    remotePath += fileInfo.fileName();
    
    // 执行上传
    uploadLocalFile(filePath, remotePath);
}

void FileExplorerWidget::downloadFile()
{
    // 检查是否已连接
    if (!connected) {
        QMessageBox::warning(this, tr("Download File"), tr("Not connected to SFTP server"));
        return;
    }
    
    // 获取远程视图中选择的文件
    QModelIndex selectedIndex = remoteFileView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, tr("Download File"), tr("Please select a file to download"));
        return;
    }
    
    QStandardItem *item = remoteFileModel->itemFromIndex(selectedIndex);
    if (!item) return;
    
    QString itemType = item->data(Qt::UserRole + 1).toString();
    QString itemName = item->data(Qt::UserRole).toString();
    
    if (itemType != "file") {
        QMessageBox::warning(this, tr("Download File"), tr("Please select a file, not a directory"));
        return;
    }
    
    // 获取本地保存路径
    QString localPath = QDir::homePath() + "/" + itemName;
    
    // 构建远程路径
    QString remotePath = currentRemotePath;
    if (!remotePath.endsWith("/")) remotePath += "/";
    remotePath += itemName;
    
    // 执行下载
    downloadRemoteFile(remotePath, localPath);
}

void FileExplorerWidget::createDirectory()
{
    // 检查是否已连接
    if (!connected) {
        QMessageBox::warning(this, tr("New Folder"), tr("Not connected to SFTP server"));
        return;
    }
    
    bool ok;
    QString folderName = QInputDialog::getText(this, tr("New Folder"), 
                                              tr("Enter folder name:"), 
                                              QLineEdit::Normal, 
                                              tr("New Folder"), &ok);
                                              
    if (ok && !folderName.isEmpty()) {
        // 构建远程路径
        QString remotePath = currentRemotePath;
        if (!remotePath.endsWith("/")) remotePath += "/";
        remotePath += folderName;
        
        // 创建目录
        if (ftpClient->createDirectory(remotePath)) {
            // 刷新当前目录
            ftpClient->listDirectory(currentRemotePath);
        }
    }
}

void FileExplorerWidget::deleteItem()
{
    // 检查是否已连接
    if (!connected) {
        QMessageBox::warning(this, tr("Delete"), tr("Not connected to SFTP server"));
        return;
    }
    
    // 获取远程视图中选择的项
    QModelIndex selectedIndex = remoteFileView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, tr("Delete"), tr("Please select an item to delete"));
        return;
    }
    
    QStandardItem *item = remoteFileModel->itemFromIndex(selectedIndex);
    if (!item) return;
    
    QString itemType = item->data(Qt::UserRole + 1).toString();
    QString itemName = item->data(Qt::UserRole).toString();
    
    if (itemType == "parent") {
        QMessageBox::warning(this, tr("Delete"), tr("Cannot delete parent directory"));
        return;
    }
    
    // 确认删除
    if (QMessageBox::question(this, tr("Confirm Delete"), 
                             tr("Are you sure you want to delete %1?").arg(itemName),
                             QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    
    // 构建远程路径
    QString remotePath = currentRemotePath;
    if (!remotePath.endsWith("/")) remotePath += "/";
    remotePath += itemName;
    
    bool success = false;
    if (itemType == "directory") {
        success = ftpClient->removeDirectory(remotePath);
    } else {
        success = ftpClient->removeFile(remotePath);
    }
    
    if (success) {
        // 刷新当前目录
        ftpClient->listDirectory(currentRemotePath);
    }
}

void FileExplorerWidget::refreshView()
{
    // 检查是否已连接
    if (!connected) {
        QMessageBox::warning(this, tr("Refresh"), tr("Not connected to SFTP server"));
        return;
    }
    
    // 刷新远程目录
    ftpClient->listDirectory(currentRemotePath);
}

// 新增: 处理本地路径输入
void FileExplorerWidget::onLocalPathEntered()
{
    QString path = localPathEdit->text();
    if (QDir(path).exists()) {
        changeLocalDirectory(path);
    } else {
        QMessageBox::warning(this, tr("Invalid Path"), tr("The specified local path does not exist."));
        localPathEdit->setText(localFileModel->filePath(localFileView->rootIndex()));
    }
}

// 新增: 处理远程路径输入
void FileExplorerWidget::onRemotePathEntered()
{
    if (!connected) {
        QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
        remotePathEdit->setText(currentRemotePath);
        return;
    }
    
    QString path = remotePathEdit->text();
    changeSftpDirectory(path);
}

// 新增: 改变本地目录
void FileExplorerWidget::changeLocalDirectory(const QString &path)
{
    QModelIndex index = localFileModel->index(path);
    if (index.isValid()) {
        localFileView->setRootIndex(index);
        localPathEdit->setText(path);
    }
}

// 拖放相关事件处理函数
void FileExplorerWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        // 确定放置区域是本地还是远程视图
        QWidget *childWidget = childAt(event->pos());
        bool isLocalViewDrop = false;
        
        // 向上搜索父部件，确定是哪个视图接收了拖放
        while (childWidget) {
            if (childWidget == localFileView->viewport()) {
                isLocalViewDrop = true;
                break;
            } else if (childWidget == remoteFileView->viewport()) {
                isLocalViewDrop = false;
                break;
            }
            childWidget = childWidget->parentWidget();
        }
        
        // 处理拖放操作
        if (isLocalViewDrop) {
            // 拖到本地视图，执行下载操作
            if (connected) {
                for (const QUrl &url : urlList) {
                    // 确保URL是有效的文件路径
                    if (url.isLocalFile()) {
                        QString localPath = url.toLocalFile();
                        QFileInfo fileInfo(localPath);
                        
                        if (!fileInfo.isDir()) {
                            // 获取目标路径 (当前本地目录)
                            QString targetPath = localFileModel->filePath(localFileView->rootIndex());
                            if (!targetPath.endsWith('/') && !targetPath.endsWith('\\')) {
                                targetPath += QDir::separator();
                            }
                            targetPath += fileInfo.fileName();
                            
                            // 上传文件
                            uploadLocalFile(localPath, currentRemotePath + "/" + fileInfo.fileName());
                        }
                    }
                }
            } else {
                QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
            }
        } else {
            // 拖到远程视图，执行上传操作
            if (connected) {
                for (const QUrl &url : urlList) {
                    if (url.isLocalFile()) {
                        QString filePath = url.toLocalFile();
                        QFileInfo fileInfo(filePath);
                        
                        if (!fileInfo.isDir()) {
                            // 构建远程路径
                            QString remotePath = currentRemotePath;
                            if (!remotePath.endsWith("/")) remotePath += "/";
                            remotePath += fileInfo.fileName();
                            
                            // 上传文件
                            uploadLocalFile(filePath, remotePath);
                        }
                    }
                }
            } else {
                QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
            }
        }
    }
    
    event->acceptProposedAction();
}

bool FileExplorerWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == localFileView->viewport()) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent*>(event);
            onLocalViewDragEnter(dragEvent);
            return true;
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dragEvent = static_cast<QDragMoveEvent*>(event);
            onLocalViewDragMove(dragEvent);
            return true;
        } else if (event->type() == QEvent::Drop) {
            QDropEvent *dropEvent = static_cast<QDropEvent*>(event);
            onLocalViewDrop(dropEvent);
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // 检查是否已经开始拖动
                if ((mouseEvent->pos() - dragStartPosition).manhattanLength() 
                    >= QApplication::startDragDistance()) {
                    startLocalItemDrag();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // 记录拖动开始位置
                dragStartPosition = mouseEvent->pos();
            }
        }
    } else if (watched == remoteFileView->viewport()) {
        if (event->type() == QEvent::DragEnter) {
            QDragEnterEvent *dragEvent = static_cast<QDragEnterEvent*>(event);
            onRemoteViewDragEnter(dragEvent);
            return true;
        } else if (event->type() == QEvent::DragMove) {
            QDragMoveEvent *dragEvent = static_cast<QDragMoveEvent*>(event);
            onRemoteViewDragMove(dragEvent);
            return true;
        } else if (event->type() == QEvent::Drop) {
            QDropEvent *dropEvent = static_cast<QDropEvent*>(event);
            onRemoteViewDrop(dropEvent);
            return true;
        } else if (event->type() == QEvent::MouseMove) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons() & Qt::LeftButton) {
                // 检查是否已经开始拖动
                if ((mouseEvent->pos() - dragStartPosition).manhattanLength() 
                    >= QApplication::startDragDistance()) {
                    startRemoteItemDrag();
                    return true;
                }
            }
        } else if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // 记录拖动开始位置
                dragStartPosition = mouseEvent->pos();
            }
        }
    }
    
    return QWidget::eventFilter(watched, event);
}

void FileExplorerWidget::onLocalViewDragEnter(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::onLocalViewDragMove(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::onLocalViewDrop(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        if (!connected) {
            QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
            event->acceptProposedAction();
            return;
        }
        
        // 获取目标路径
        QString localTargetDir = localFileModel->filePath(localFileView->rootIndex());
        
        // 下载文件到本地
        for (const QUrl &url : urlList) {
            if (url.scheme() == "sftp") {
                // 解析SFTP URL以获取远程路径
                QString remotePath = url.path();
                QFileInfo remoteFileInfo(remotePath);
                
                // 构建本地目标路径
                QString localTargetPath = localTargetDir;
                if (!localTargetPath.endsWith('/') && !localTargetPath.endsWith('\\')) {
                    localTargetPath += QDir::separator();
                }
                localTargetPath += remoteFileInfo.fileName();
                
                // 下载文件
                downloadRemoteFile(remotePath, localTargetPath);
            }
        }
    }
    
    event->acceptProposedAction();
}

void FileExplorerWidget::onRemoteViewDragEnter(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::onRemoteViewDragMove(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void FileExplorerWidget::onRemoteViewDrop(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        
        if (!connected) {
            QMessageBox::warning(this, tr("SFTP Error"), tr("Not connected to SFTP server"));
            event->acceptProposedAction();
            return;
        }
        
        // 上传文件到远程
        for (const QUrl &url : urlList) {
            if (url.isLocalFile()) {
                QString localPath = url.toLocalFile();
                QFileInfo fileInfo(localPath);
                
                if (!fileInfo.isDir()) {
                    // 构建远程路径
                    QString remotePath = currentRemotePath;
                    if (!remotePath.endsWith("/")) remotePath += "/";
                    remotePath += fileInfo.fileName();
                    
                    // 上传文件
                    uploadLocalFile(localPath, remotePath);
                }
            }
        }
    }
    
    event->acceptProposedAction();
}

void FileExplorerWidget::uploadLocalFile(const QString &localPath, const QString &remotePath)
{
    if (!connected) {
        QMessageBox::warning(this, tr("Upload File"), tr("Not connected to SFTP server"));
        return;
    }
    
    QFileInfo fileInfo(localPath);
    if (fileInfo.isDir()) {
        QMessageBox::warning(this, tr("Upload File"), tr("Directories cannot be uploaded"));
        return;
    }
    
    // 创建传输任务
    int taskId = addTransferTask(localPath, remotePath, TransferTask::Upload);
    
    // 如果当前没有活动传输，启动这个传输
    if (currentTaskId == -1) {
        currentTaskId = taskId;
        
        // 执行上传
        if (ftpClient->uploadFile(localPath, remotePath)) {
            QMessageBox::information(this, tr("Upload File"), 
                                    tr("Started uploading file: %1").arg(fileInfo.fileName()));
        } else {
            // 上传失败
            completeTransferTask(taskId, false, tr("Failed to start upload"));
            currentTaskId = -1;
            processNextTransfer();
        }
    }
}

void FileExplorerWidget::downloadRemoteFile(const QString &remotePath, const QString &localPath)
{
    if (!connected) {
        QMessageBox::warning(this, tr("Download File"), tr("Not connected to SFTP server"));
        return;
    }
    
    // 创建传输任务
    int taskId = addTransferTask(localPath, remotePath, TransferTask::Download);
    
    // 如果当前没有活动传输，启动这个传输
    if (currentTaskId == -1) {
        currentTaskId = taskId;
        
        // 执行下载
        if (ftpClient->downloadFile(remotePath, localPath)) {
            QFileInfo fileInfo(localPath);
            QMessageBox::information(this, tr("Download File"), 
                                    tr("Started downloading file: %1").arg(fileInfo.fileName()));
        } else {
            // 下载失败
            completeTransferTask(taskId, false, tr("Failed to start download"));
            currentTaskId = -1;
            processNextTransfer();
        }
    }
}

// 新增：添加传输任务
int FileExplorerWidget::addTransferTask(const QString &localPath, const QString &remotePath, TransferTask::Type type)
{
    TransferTask task;
    task.localPath = localPath;
    task.remotePath = remotePath;
    task.type = type;
    task.transferred = 0;
    task.progress = 0;
    task.completed = false;
    task.error = false;
    task.taskId = nextTaskId++;
    
    // 设置文件名和大小
    QFileInfo fileInfo(type == TransferTask::Upload ? localPath : QFileInfo(remotePath).fileName());
    task.fileName = fileInfo.fileName();
    task.fileSize = type == TransferTask::Upload ? fileInfo.size() : 0;
    
    // 添加到任务映射
    transferTasks[task.taskId] = task;
    
    // 创建并添加列表项
    QListWidgetItem *item = new QListWidgetItem(transferList);
    item->setData(Qt::UserRole, task.taskId);
    
    // 设置自定义小部件
    QWidget *taskWidget = new QWidget(transferList);
    QVBoxLayout *taskLayout = new QVBoxLayout(taskWidget);
    taskLayout->setContentsMargins(4, 4, 4, 4);
    
    // 任务信息行
    QWidget *infoWidget = new QWidget(taskWidget);
    QHBoxLayout *infoLayout = new QHBoxLayout(infoWidget);
    infoLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *typeIcon = new QLabel(infoWidget);
    typeIcon->setPixmap(QIcon(type == TransferTask::Upload ? ":/icons/upload.svg" : ":/icons/download.svg").pixmap(16, 16));
    
    QLabel *nameLabel = new QLabel(task.fileName, infoWidget);
    nameLabel->setStyleSheet("QLabel { color: white; }");
    
    QLabel *statusLabel = new QLabel(tr("Queued"), infoWidget);
    statusLabel->setStyleSheet("QLabel { color: #8E8E8E; }");
    statusLabel->setObjectName("statusLabel");
    
    infoLayout->addWidget(typeIcon);
    infoLayout->addWidget(nameLabel);
    infoLayout->addStretch();
    infoLayout->addWidget(statusLabel);
    
    // 进度条
    QProgressBar *progressBar = new QProgressBar(taskWidget);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setTextVisible(true);
    progressBar->setObjectName("progressBar");
    progressBar->setStyleSheet("QProgressBar { background-color: #2D2D30; color: white; border: 1px solid #3D3D3D; height: 16px; text-align: center; }"
                             "QProgressBar::chunk { background-color: #4A86E8; }");
    
    taskLayout->addWidget(infoWidget);
    taskLayout->addWidget(progressBar);
    
    // 设置列表项和小部件
    item->setSizeHint(taskWidget->sizeHint());
    transferList->setItemWidget(item, taskWidget);
    
    // 滚动到最新项
    transferList->scrollToItem(item);
    
    return task.taskId;
}

// 新增：更新传输进度
void FileExplorerWidget::updateTransferProgress(int taskId, qint64 transferred, qint64 total)
{
    if (!transferTasks.contains(taskId))
        return;
    
    TransferTask &task = transferTasks[taskId];
    task.transferred = transferred;
    task.fileSize = total;
    task.progress = total > 0 ? (int)(transferred * 100 / total) : 0;
    
    updateTransferListItem(taskId);
}

// 新增：更新传输列表项
void FileExplorerWidget::updateTransferListItem(int taskId)
{
    if (!transferTasks.contains(taskId))
        return;
    
    const TransferTask &task = transferTasks[taskId];
    
    // 查找对应的列表项
    for (int i = 0; i < transferList->count(); ++i) {
        QListWidgetItem *item = transferList->item(i);
        if (item->data(Qt::UserRole).toInt() == taskId) {
            // 获取进度条和状态标签
            QWidget *taskWidget = transferList->itemWidget(item);
            QProgressBar *progressBar = taskWidget->findChild<QProgressBar*>("progressBar");
            QLabel *statusLabel = taskWidget->findChild<QLabel*>("statusLabel");
            
            if (progressBar && statusLabel) {
                // 更新进度
                progressBar->setValue(task.progress);
                
                // 更新状态文本
                QString statusText;
                if (task.completed) {
                    if (task.error) {
                        statusText = tr("Error: %1").arg(task.errorMessage);
                        statusLabel->setStyleSheet("QLabel { color: #FF4040; }");
                    } else {
                        statusText = tr("Completed");
                        statusLabel->setStyleSheet("QLabel { color: #40C040; }");
                    }
                } else if (task.taskId == currentTaskId) {
                    // 计算传输速率和剩余时间
                    double mbTransferred = task.transferred / (1024.0 * 1024.0);
                    statusText = tr("Transferring: %1 MB").arg(mbTransferred, 0, 'f', 2);
                    statusLabel->setStyleSheet("QLabel { color: #4A86E8; }");
                } else {
                    statusText = tr("Queued");
                    statusLabel->setStyleSheet("QLabel { color: #8E8E8E; }");
                }
                
                statusLabel->setText(statusText);
            }
            
            break;
        }
    }
}

// 新增：完成传输任务
void FileExplorerWidget::completeTransferTask(int taskId, bool success, const QString &errorMessage)
{
    if (!transferTasks.contains(taskId))
        return;
    
    TransferTask &task = transferTasks[taskId];
    task.completed = true;
    task.error = !success;
    task.errorMessage = errorMessage;
    
    // 如果是上传文件完成，刷新远程目录
    if (success && task.type == TransferTask::Upload) {
        ftpClient->listDirectory(currentRemotePath);
    }
    
    updateTransferListItem(taskId);
}

// 新增：处理下一个传输任务
void FileExplorerWidget::processNextTransfer()
{
    // 找到队列中下一个未完成的任务
    int nextTaskId = -1;
    
    for (auto it = transferTasks.begin(); it != transferTasks.end(); ++it) {
        if (!it.value().completed) {
            nextTaskId = it.key();
            break;
        }
    }
    
    if (nextTaskId != -1) {
        currentTaskId = nextTaskId;
        const TransferTask &task = transferTasks[nextTaskId];
        
        if (task.type == TransferTask::Upload) {
            if (!ftpClient->uploadFile(task.localPath, task.remotePath)) {
                completeTransferTask(nextTaskId, false, tr("Failed to start upload"));
                currentTaskId = -1;
                processNextTransfer();
            }
        } else {
            if (!ftpClient->downloadFile(task.remotePath, task.localPath)) {
                completeTransferTask(nextTaskId, false, tr("Failed to start download"));
                currentTaskId = -1;
                processNextTransfer();
            }
        }
    } else {
        currentTaskId = -1;
    }
}

// 新增：传输进度更新处理
void FileExplorerWidget::onTransferProgress(qint64 bytesSent, qint64 bytesTotal)
{
    if (currentTaskId != -1) {
        updateTransferProgress(currentTaskId, bytesSent, bytesTotal);
    }
}

// 新增：传输完成处理
void FileExplorerWidget::onTransferCompleted()
{
    if (currentTaskId != -1) {
        completeTransferTask(currentTaskId, true);
        
        // 记录完成的任务ID
        int completedTaskId = currentTaskId;
        currentTaskId = -1;
        
        // 处理下一个任务
        processNextTransfer();
    }
}

// 新增：清除已完成的传输记录
void FileExplorerWidget::clearCompletedTransfers()
{
    QList<int> tasksToRemove;
    
    // 找出所有已完成的任务
    for (auto it = transferTasks.begin(); it != transferTasks.end(); ++it) {
        if (it.value().completed) {
            tasksToRemove.append(it.key());
        }
    }
    
    // 从列表和映射中删除任务
    for (int taskId : tasksToRemove) {
        // 从列表中删除
        for (int i = 0; i < transferList->count(); ++i) {
            QListWidgetItem *item = transferList->item(i);
            if (item->data(Qt::UserRole).toInt() == taskId) {
                delete transferList->takeItem(i);
                break;
            }
        }
        
        // 从映射中删除
        transferTasks.remove(taskId);
    }
}

// 新增：取消传输
void FileExplorerWidget::cancelTransfer()
{
    // 获取当前选择的项
    QListWidgetItem *item = transferList->currentItem();
    if (!item)
        return;
    
    int taskId = item->data(Qt::UserRole).toInt();
    if (!transferTasks.contains(taskId))
        return;
    
    // 如果是当前正在传输的任务，取消它
    if (taskId == currentTaskId) {
        // 目前libssh2不支持直接取消传输
        // 我们只能标记为出错
        completeTransferTask(taskId, false, tr("Canceled by user"));
        currentTaskId = -1;
        
        // 处理下一个任务
        processNextTransfer();
    } else if (!transferTasks[taskId].completed) {
        // 如果是队列中的任务，直接标记为取消
        completeTransferTask(taskId, false, tr("Canceled by user"));
    }
}

// 启动本地文件拖放
void FileExplorerWidget::startLocalItemDrag()
{
    QModelIndex index = localFileView->currentIndex();
    if (!index.isValid()) return;
    
    QString filePath = localFileModel->filePath(index);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) return; // 不支持拖放目录
    
    // 创建拖放对象
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();
    
    // 设置URL列表
    QList<QUrl> urls;
    urls << QUrl::fromLocalFile(filePath);
    mimeData->setUrls(urls);
    
    // 设置拖放数据
    drag->setMimeData(mimeData);
    
    // 设置拖放图标
    QPixmap pixmap = QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(32, 32);
    drag->setPixmap(pixmap);
    
    // 标记为本地拖放源
    isLocalDragSource = true;
    
    // 执行拖放
    drag->exec(Qt::CopyAction);
    
    // 重置标记
    isLocalDragSource = false;
}

// 启动远程文件拖放
void FileExplorerWidget::startRemoteItemDrag()
{
    QModelIndex index = remoteFileView->currentIndex();
    if (!index.isValid()) return;
    
    QStandardItem *item = remoteFileModel->itemFromIndex(index);
    if (!item) return;
    
    QString itemName = item->data(Qt::UserRole).toString();
    QString itemType = item->data(Qt::UserRole + 1).toString();
    
    if (itemType != "file") return; // 仅支持文件拖放
    
    // 构建远程路径
    QString remotePath = currentRemotePath;
    if (!remotePath.endsWith("/")) remotePath += "/";
    remotePath += itemName;
    
    // 创建拖放对象
    QDrag *drag = new QDrag(this);
    QMimeData *mimeData = new QMimeData();
    
    // 为远程文件创建特殊URL (sftp://...)
    QUrl url;
    url.setScheme("sftp");
    url.setPath(remotePath);
    
    QList<QUrl> urls;
    urls << url;
    mimeData->setUrls(urls);
    
    // 设置拖放数据
    drag->setMimeData(mimeData);
    
    // 设置拖放图标
    QPixmap pixmap = QApplication::style()->standardIcon(QStyle::SP_FileIcon).pixmap(32, 32);
    drag->setPixmap(pixmap);
    
    // 标记为非本地拖放源
    isLocalDragSource = false;
    
    // 执行拖放
    drag->exec(Qt::CopyAction);
}

// 添加未实现的槽函数
void FileExplorerWidget::onLocalItemDragged(const QModelIndex &index)
{
    if (!index.isValid()) return;
    
    QString filePath = localFileModel->filePath(index);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) return; // 不支持拖放目录
    
    startLocalItemDrag();
}

void FileExplorerWidget::onRemoteItemDragged(const QModelIndex &index)
{
    if (!index.isValid()) return;
    
    QStandardItem *item = remoteFileModel->itemFromIndex(index);
    if (!item) return;
    
    QString itemType = item->data(Qt::UserRole + 1).toString();
    
    if (itemType != "file") return; // 仅支持文件拖放
    
    startRemoteItemDrag();
}

// 重新添加丢失的方法
QString FileExplorerWidget::getRemoteFilePath(const QModelIndex &index)
{
    if (!index.isValid()) return QString();
    
    QStandardItem *item = remoteFileModel->itemFromIndex(index);
    if (!item) return QString();
    
    QString itemName = item->data(Qt::UserRole).toString();
    QString itemType = item->data(Qt::UserRole + 1).toString();
    
    // 只处理文件，不处理目录
    if (itemType == "file") {
        QString path = currentRemotePath;
        if (!path.endsWith("/")) path += "/";
        path += itemName;
        return path;
    }
    
    return QString();
} 
