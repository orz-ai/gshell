#ifndef FTPCLIENT_H
#define FTPCLIENT_H

#include <QObject>
#include <QString>
#include <QFile>

// Forward declaration of private class
class FTPClientPrivate;

class FTPClient : public QObject
{
    Q_OBJECT
public:
    explicit FTPClient(QObject *parent = nullptr);
    ~FTPClient();
    
    bool connect(const QString &host, int port, const QString &username, const QString &password);
    void disconnect();
    bool isConnected() const;
    
    bool uploadFile(const QString &localPath, const QString &remotePath);
    bool downloadFile(const QString &remotePath, const QString &localPath);
    bool listDirectory(const QString &remotePath);
    bool createDirectory(const QString &remotePath);
    bool removeFile(const QString &remotePath);
    bool removeDirectory(const QString &remotePath);

signals:
    void connected();
    void disconnected();
    void error(const QString &errorMessage);
    void transferProgress(qint64 bytesSent, qint64 bytesTotal);
    void directoryListed(const QStringList &entries);
    void transferCompleted();

private:
    bool m_connected;
    void *m_session; // Keep for backward compatibility
    FTPClientPrivate *d;
    
    bool initLibssh2();
    void cleanupLibssh2();
};

#endif // FTPCLIENT_H 