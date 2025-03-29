#ifndef SESSIONMANAGERDIALOG_H
#define SESSIONMANAGERDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "sessionmanager.h"
#include "sessiondialog.h"

class SessionManagerDialog : public QDialog
{
    Q_OBJECT
public:
    explicit SessionManagerDialog(SessionManager *manager, QWidget *parent = nullptr);

private slots:
    void addSession();
    void editSession();
    void deleteSession();
    void connectToSession();
    void onSelectionChanged();

signals:
    void sessionSelected(const SessionInfo &session);

private:
    SessionManager *sessionManager;
    QListWidget *sessionListWidget;
    QPushButton *connectButton;
    QPushButton *editButton;
    QPushButton *deleteButton;
    
    void loadSessions();
};

#endif // SESSIONMANAGERDIALOG_H 