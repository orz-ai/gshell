#ifndef SESSIONMANAGER_H
#define SESSIONMANAGER_H

#include <QObject>
#include <QMap>
#include <QString>
#include <QCryptographicHash>
#include <QByteArray>
#include "sessioninfo.h"

class SessionManager : public QObject
{
    Q_OBJECT
public:
    explicit SessionManager(QObject *parent = nullptr);
    
    QList<SessionInfo> getSessions() const;
    SessionInfo getSession(const QString &id) const;
    void saveSession(const SessionInfo &session);
    void deleteSession(const QString &id);
    void loadSessions();
    void saveSessions();
    
private:
    QString encryptPassword(const QString &password);
    QString decryptPassword(const QString &encryptedPassword);
    
    QMap<QString, SessionInfo> m_sessions;
    const QByteArray m_encryptionKey = "GShellEncryptionKey"; // 简单的加密密钥
    
signals:
    void sessionsChanged();
};

#endif // SESSIONMANAGER_H 