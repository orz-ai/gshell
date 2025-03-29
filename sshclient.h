#ifndef SSHCLIENT_H
#define SSHCLIENT_H

#include <QObject>
#include <QString>
#include <QByteArray>
#include <libssh2.h>
#include <QTcpSocket>

class SSHClient : public QObject
{
    Q_OBJECT
public:
    explicit SSHClient(QObject *parent = nullptr);
    ~SSHClient();

    bool connect(const QString &host, int port, const QString &username, const QString &password);
    bool connectWithKey(const QString &host, int port, const QString &username, const QString &privateKeyFile, const QString &passphrase);
    void disconnect();
    bool isConnected() const;

    bool executeCommand(const QString &command);
    bool startShell();
    bool sendData(const QByteArray &data);

signals:
    void connected();
    void disconnected();
    void error(const QString &errorMessage);
    void dataReceived(const QByteArray &data);

private:
    bool m_connected;
    LIBSSH2_SESSION *m_session;
    SOCKET m_socketDescriptor;
    bool m_wsaInitialized;  // 跟踪 WSA 是否已初始化
    LIBSSH2_CHANNEL *m_channel;
    bool m_shellActive;
    
    bool initLibssh2();
    void cleanupLibssh2();
    bool initWsa();  // 新方法专门用于初始化 WSA
    void cleanupWsa();  // 新方法专门用于清理 WSA
    bool authenticateWithPassword(const QString &username, const QString &password);
    bool authenticateWithKey(const QString &username, const QString &privateKeyFile, const QString &passphrase);
    bool waitSocket(int timeout_ms);
    void readChannel();

};

#endif // SSHCLIENT_H
