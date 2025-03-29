#ifndef SSHCONNECTIONTHREAD_H
#define SSHCONNECTIONTHREAD_H

#include <QThread>
#include <QString>
#include "sshclient.h"

class SSHConnectionThread : public QThread
{
    Q_OBJECT

public:
    explicit SSHConnectionThread(QObject *parent = nullptr);
    ~SSHConnectionThread();

    void setConnectionParams(const QString &host, int port, const QString &username, const QString &password);
    void setKeyConnectionParams(const QString &host, int port, const QString &username, 
                               const QString &privateKeyFile, const QString &passphrase);
    SSHClient* getSSHClient() const { return m_sshClient; }

signals:
    void connectionEstablished();
    void connectionFailed(const QString &errorMessage);

protected:
    void run() override;

private:
    SSHClient *m_sshClient;
    QString m_host;
    int m_port;
    QString m_username;
    QString m_password;
    QString m_privateKeyFile;
    QString m_passphrase;
    bool m_useKey;
};

#endif // SSHCONNECTIONTHREAD_H 