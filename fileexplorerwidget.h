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
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QListWidget>
#include <QProgressBar>
#include <QMap>
#include "ftpclient.h"

struct TransferTask {
    enum Type { Upload, Download };
    
    QString localPath;
    QString remotePath;
    Type type;
    QString fileName;
    qint64 fileSize;
    qint64 transferred;
    int progress;
    bool completed;
    bool error;
    QString errorMessage;
    int taskId;
};

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

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

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
    
    void onLocalItemDragged(const QModelIndex &index);
    void onRemoteItemDragged(const QModelIndex &index);
    void onLocalViewDragEnter(QDragEnterEvent *event);
    void onLocalViewDragMove(QDragMoveEvent *event);
    void onLocalViewDrop(QDropEvent *event);
    void onRemoteViewDragEnter(QDragEnterEvent *event);
    void onRemoteViewDragMove(QDragMoveEvent *event);
    void onRemoteViewDrop(QDropEvent *event);

    void startLocalItemDrag();
    void startRemoteItemDrag();
    
    void onTransferProgress(qint64 bytesSent, qint64 bytesTotal);
    void onTransferCompleted();
    void clearCompletedTransfers();
    void cancelTransfer();

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
    
    QString dragSourcePath;
    bool isLocalDragSource;
    
    QPoint dragStartPosition;

    QSplitter *mainSplitter;
    QWidget *transferWidget;
    QListWidget *transferList;
    QMap<int, TransferTask> transferTasks;
    int nextTaskId;
    int currentTaskId;
    
    void setupUI();
    void setupToolbar();
    void setupTransferPanel();
    void populateRemoteView(const QStringList &entries);
    void changeSftpDirectory(const QString &path);
    void changeLocalDirectory(const QString &path);
    
    QString getRemoteFilePath(const QModelIndex &index);
    void uploadLocalFile(const QString &localPath, const QString &remotePath);
    void downloadRemoteFile(const QString &remotePath, const QString &localPath);
    
    int addTransferTask(const QString &localPath, const QString &remotePath, TransferTask::Type type);
    void updateTransferProgress(int taskId, qint64 transferred, qint64 total);
    void completeTransferTask(int taskId, bool success = true, const QString &errorMessage = QString());
    void updateTransferListItem(int taskId);
    void processNextTransfer();
};

#endif // FILEEXPLORERWIDGET_H 
