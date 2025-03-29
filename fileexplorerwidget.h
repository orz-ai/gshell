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

class FileExplorerWidget : public QWidget
{
    Q_OBJECT
public:
    explicit FileExplorerWidget(QWidget *parent = nullptr);

private slots:
    void uploadFile();
    void downloadFile();
    void createDirectory();
    void deleteItem();
    void refreshView();

private:
    QSplitter *splitter;
    QTreeView *localFileView;
    QTreeView *remoteFileView;
    QFileSystemModel *localFileModel;
    
    QToolBar *toolBar;
    
    void setupUI();
    void setupToolbar();
};

#endif // FILEEXPLORERWIDGET_H 