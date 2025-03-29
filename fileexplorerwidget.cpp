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

FileExplorerWidget::FileExplorerWidget(QWidget *parent) : QWidget(parent)
{
    setupUI();
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

    localFileModel = new QFileSystemModel(this);
    localFileModel->setRootPath(QDir::homePath());
    

    QWidget *localWidget = new QWidget(splitter);
    QVBoxLayout *localLayout = new QVBoxLayout(localWidget);
    localLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *localLabel = new QLabel(tr("Local Files"), localWidget);
    localLabel->setStyleSheet("QLabel { background-color: #2D2D30; color: white; padding: 4px; }");
    
    localFileView = new QTreeView(localWidget);
    localFileView->setModel(localFileModel);
    localFileView->setRootIndex(localFileModel->index(QDir::homePath()));
    localFileView->setSortingEnabled(true);
    localFileView->setColumnWidth(0, 250);
    localFileView->setStyleSheet("QTreeView { background-color: #1E1E1E; color: #DCDCDC; }");
    
    localLayout->addWidget(localLabel);
    localLayout->addWidget(localFileView);
    

    QWidget *remoteWidget = new QWidget(splitter);
    QVBoxLayout *remoteLayout = new QVBoxLayout(remoteWidget);
    remoteLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel *remoteLabel = new QLabel(tr("Remote Files"), remoteWidget);
    remoteLabel->setStyleSheet("QLabel { background-color: #2D2D30; color: white; padding: 4px; }");
    
    remoteFileView = new QTreeView(remoteWidget);
    remoteFileView->setModel(localFileModel);
    remoteFileView->setRootIndex(localFileModel->index(QDir::homePath()));
    remoteFileView->setSortingEnabled(true);
    remoteFileView->setColumnWidth(0, 250);
    remoteFileView->setStyleSheet("QTreeView { background-color: #1E1E1E; color: #DCDCDC; }");
    
    remoteLayout->addWidget(remoteLabel);
    remoteLayout->addWidget(remoteFileView);
    

    splitter->addWidget(localWidget);
    splitter->addWidget(remoteWidget);
    splitter->setSizes(QList<int>() << height()/2 << height()/2);
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

void FileExplorerWidget::uploadFile()
{
    // Get the selected file from the local view
    QModelIndex selectedIndex = localFileView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, "Upload File", "Please select a file to upload");
        return;
    }
    
    QString filePath = localFileModel->filePath(selectedIndex);
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) {
        QMessageBox::warning(this, "Upload File", "Please select a file, not a directory");
        return;
    }
    
    // In a real implementation, we would upload the file to the remote server
    QMessageBox::information(this, "Upload File", 
                            QString("Simulating upload of file: %1").arg(filePath));
}

void FileExplorerWidget::downloadFile()
{
    // Get the selected file from the remote view
    QModelIndex selectedIndex = remoteFileView->currentIndex();
    if (!selectedIndex.isValid()) {
        QMessageBox::warning(this, "Download File", "Please select a file to download");
        return;
    }
    
    QString filePath = localFileModel->filePath(selectedIndex); // Using localFileModel for now
    QFileInfo fileInfo(filePath);
    
    if (fileInfo.isDir()) {
        QMessageBox::warning(this, "Download File", "Please select a file, not a directory");
        return;
    }
    
    // In a real implementation, we would download the file from the remote server
    QMessageBox::information(this, "Download File", 
                            QString("Simulating download of file: %1").arg(filePath));
}

void FileExplorerWidget::createDirectory()
{
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder", 
                                              "Enter folder name:", 
                                              QLineEdit::Normal, 
                                              "New Folder", &ok);
    if (ok && !folderName.isEmpty()) {
        // In a real implementation, we would create the folder on the remote server
        QMessageBox::information(this, "New Folder", 
                                QString("Simulating creation of folder: %1").arg(folderName));
    }
}

void FileExplorerWidget::deleteItem()
{
    QMessageBox::information(this, tr("Delete"), tr("Delete feature not yet implemented"));
}

void FileExplorerWidget::refreshView()
{
    QMessageBox::information(this, tr("Refresh"), tr("Refresh feature not yet implemented"));
} 
