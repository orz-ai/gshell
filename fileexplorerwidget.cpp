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

FileExplorerWidget::FileExplorerWidget(QWidget *parent) : QWidget(parent), connected(false)
{
    ftpClient = new FTPClient(this);
    
    // 连接FTP客户端信号
    connect(ftpClient, &FTPClient::connected, this, &FileExplorerWidget::onSftpConnected);
    connect(ftpClient, &FTPClient::disconnected, this, &FileExplorerWidget::onSftpDisconnected);
    connect(ftpClient, &FTPClient::error, this, &FileExplorerWidget::onSftpError);
    connect(ftpClient, &FTPClient::directoryListed, this, &FileExplorerWidget::onDirectoryListed);
    
    setupUI();
    
    // 初始时隐藏file explorer
    this->hide();
    
    // 设置初始远程路径
    currentRemotePath = "/";
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
    
    splitter = new QSplitter(Qt::Vertical, this);
    mainLayout->addWidget(splitter);

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
    if (ftpClient->uploadFile(filePath, remotePath)) {
        QMessageBox::information(this, tr("Upload File"), 
                                tr("Started uploading file: %1").arg(fileInfo.fileName()));
    }
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
    if (ftpClient->downloadFile(remotePath, localPath)) {
        QMessageBox::information(this, tr("Download File"), 
                                tr("Started downloading file: %1").arg(itemName));
    }
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
