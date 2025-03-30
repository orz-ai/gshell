#ifndef FILEEXPLORERWIDGET_H
#define FILEEXPLORERWIDGET_H

#include <QWidget>
#include <QSplitter>
#include <QTreeView>
#include <QFileSystemModel>
#include <QVBoxLayout>
#include <QToolBar>
#include <QInputDialog>
#include <QFileInfo>
#include <QStandardItemModel>
#include <QLineEdit>
#include <QHBoxLayout>
#include "ftpclient.h"

class FileExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);

    void connectToSftp(const QString &host, int port, const QString &username, const QString &password);
    void showExplorer();
    void hideExplorer();
    bool isVisible() const;

signals:
    void sftpStatusChanged(bool connected, const QString &message);

private slots:
    void uploadFile();
    void downloadFile();
    void createDirectory();
    void deleteItem();
    void refreshView();
    void onRemoteDoubleClicked(const QModelIndex &index);
    void onDirectoryListed(const QStringList &entries);
    void onSftpError(const QString &errorMessage);
    void onSftpConnected();
    void onSftpDisconnected();
    void onLocalPathEntered();
    void onRemotePathEntered();

private:
    QSplitter *splitter;
    QTreeView *localFileView;
    QTreeView *remoteFileView;
    QFileSystemModel *localFileModel;
    QStandardItemModel *remoteFileModel;
    QLineEdit *localPathEdit;
    QLineEdit *remotePathEdit;
    
    QToolBar *toolBar;
    FTPClient *ftpClient;
    QString currentRemotePath;
    bool connected;
    
    void setupUI();
    void setupToolbar();
    void populateRemoteView(const QStringList &entries);
    void changeSftpDirectory(const QString &path);
    void changeLocalDirectory(const QString &path);
};

#endif // FILEEXPLORERWIDGET_H 