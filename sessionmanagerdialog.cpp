#include "sessionmanagerdialog.h"
#include <QMessageBox>

SessionManagerDialog::SessionManagerDialog(SessionManager *manager, QWidget *parent)
    : QDialog(parent), sessionManager(manager)
{
    setWindowTitle(tr("Session Manager"));
    resize(400, 300);
    

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    

    sessionListWidget = new QListWidget(this);
    mainLayout->addWidget(sessionListWidget);
    

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    
    QPushButton *addButton = new QPushButton(tr("Add"), this);
    editButton = new QPushButton(tr("Edit"), this);
    deleteButton = new QPushButton(tr("Delete"), this);
    connectButton = new QPushButton(tr("Connect"), this);
    QPushButton *closeButton = new QPushButton(tr("Close"), this);
    
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(editButton);
    buttonLayout->addWidget(deleteButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(connectButton);
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    

    connect(addButton, &QPushButton::clicked, this, &SessionManagerDialog::addSession);
    connect(editButton, &QPushButton::clicked, this, &SessionManagerDialog::editSession);
    connect(deleteButton, &QPushButton::clicked, this, &SessionManagerDialog::deleteSession);
    connect(connectButton, &QPushButton::clicked, this, &SessionManagerDialog::connectToSession);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(sessionListWidget, &QListWidget::itemDoubleClicked, this, &SessionManagerDialog::connectToSession);
    connect(sessionListWidget, &QListWidget::itemSelectionChanged, this, &SessionManagerDialog::onSelectionChanged);
    

    loadSessions();
    

    onSelectionChanged();
}

void SessionManagerDialog::loadSessions()
{
    sessionListWidget->clear();
    
    QList<SessionInfo> sessions = sessionManager->getSessions();
    
    for (const SessionInfo &session : sessions) {
        QString displayName = session.name.isEmpty() ? 
            QString("%1@%2:%3").arg(session.username, session.host).arg(session.port) : 
            session.name;
        
        QListWidgetItem *item = new QListWidgetItem(displayName);
        item->setData(Qt::UserRole, session.name);
        sessionListWidget->addItem(item);
    }
}

void SessionManagerDialog::addSession()
{
    SessionDialog dialog(this);
    

    SessionInfo defaultSession;
    defaultSession.name = tr("New Session");
    defaultSession.port = 22;
    
    dialog.setSessionInfo(defaultSession);
    
    if (dialog.exec() == QDialog::Accepted) {
        SessionInfo session = dialog.getSessionInfo();
        sessionManager->saveSession(session);
        loadSessions();
    }
}

void SessionManagerDialog::editSession()
{
    QListWidgetItem *currentItem = sessionListWidget->currentItem();
    if (!currentItem) {
        return;
    }
    
    QString sessionName = currentItem->data(Qt::UserRole).toString();
    SessionInfo session = sessionManager->getSession(sessionName);
    
    SessionDialog dialog(this);
    dialog.setSessionInfo(session);
    
    if (dialog.exec() == QDialog::Accepted) {
        SessionInfo updatedSession = dialog.getSessionInfo();
        

        if (updatedSession.name != sessionName) {
            sessionManager->deleteSession(sessionName);
        }
        
        sessionManager->saveSession(updatedSession);
        loadSessions();
    }
}

void SessionManagerDialog::deleteSession()
{
    QListWidgetItem *currentItem = sessionListWidget->currentItem();
    if (!currentItem) {
        return;
    }
    
    QString sessionName = currentItem->data(Qt::UserRole).toString();
    
    if (QMessageBox::question(this, tr("Confirm Delete"), 
                             tr("Are you sure you want to delete session '%1'?").arg(sessionName),
                             QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {
        sessionManager->deleteSession(sessionName);
        loadSessions();
    }
}

void SessionManagerDialog::connectToSession()
{
    QListWidgetItem *currentItem = sessionListWidget->currentItem();
    if (!currentItem) {
        return;
    }
    
    QString sessionName = currentItem->data(Qt::UserRole).toString();
    SessionInfo session = sessionManager->getSession(sessionName);
    
    emit sessionSelected(session);
    accept();
}

void SessionManagerDialog::onSelectionChanged()
{
    bool hasSelection = !sessionListWidget->selectedItems().isEmpty();
    
    editButton->setEnabled(hasSelection);
    deleteButton->setEnabled(hasSelection);
    connectButton->setEnabled(hasSelection);
} 
