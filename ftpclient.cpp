#include "ftpclient.h"
#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QThread>
#include <QCoreApplication>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include <libssh2.h>
#include <libssh2_sftp.h>

class FTPClientPrivate {
public:
    LIBSSH2_SESSION *session;
    LIBSSH2_SFTP *sftp_session;
    SOCKET sock;
    bool connected;
    QString currentPath;
    
    bool wsaInitialized;
};

FTPClient::FTPClient(QObject *parent) : QObject(parent), m_connected(false), m_session(nullptr)
{
    d = new FTPClientPrivate;
    d->session = nullptr;
    d->sftp_session = nullptr;
    d->connected = false;
    d->wsaInitialized = false;
    d->currentPath = "/";
    
    // Initialize libssh2
    initLibssh2();
}

FTPClient::~FTPClient()
{
    if (m_connected) {
        disconnect();
    }
    
    // Cleanup libssh2
    cleanupLibssh2();
    
    delete d;
}

bool FTPClient::initLibssh2()
{
#ifdef _WIN32
    WSADATA wsadata;
    if (WSAStartup(MAKEWORD(2, 0), &wsadata) != 0) {
        emit error("WSAStartup failed");
        return false;
    }
    d->wsaInitialized = true;
#endif

    if (libssh2_init(0) != 0) {
        emit error("libssh2 initialization failed");
        return false;
    }
    
    return true;
}

void FTPClient::cleanupLibssh2()
{
    libssh2_exit();
    
#ifdef _WIN32
    if (d->wsaInitialized) {
        WSACleanup();
        d->wsaInitialized = false;
    }
#endif
}

bool FTPClient::connect(const QString &host, int port, const QString &username, const QString &password)
{
    if (m_connected) {
        disconnect();
    }
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    // Resolve hostname
    struct hostent *hostEntry = gethostbyname(host.toStdString().c_str());
    if (!hostEntry) {
        emit error("Failed to resolve hostname");
        return false;
    }
    
    memcpy(&sin.sin_addr, hostEntry->h_addr_list[0], hostEntry->h_length);
    
    // Create socket
    d->sock = socket(AF_INET, SOCK_STREAM, 0);
    
#ifdef _WIN32
    if (d->sock == INVALID_SOCKET) {
        emit error("Failed to create socket");
        return false;
    }
#else
    if (d->sock == -1) {
        emit error("Failed to create socket");
        return false;
    }
#endif
    
    // Connect to server
    if (::connect(d->sock, (struct sockaddr*)(&sin), sizeof(struct sockaddr_in)) != 0) {
        emit error("Failed to connect to host");
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        return false;
    }
    
    // Create SSH session
    d->session = libssh2_session_init();
    if (!d->session) {
        emit error("Failed to create SSH session");
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        return false;
    }
    
    // Set blocking mode
    libssh2_session_set_blocking(d->session, 1);
    
    // Handshake
    if (libssh2_session_handshake(d->session, d->sock)) {
        emit error("SSH handshake failed");
        libssh2_session_free(d->session);
        d->session = nullptr;
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        return false;
    }
    
    // Authenticate with password
    if (libssh2_userauth_password(d->session, username.toStdString().c_str(), password.toStdString().c_str())) {
        emit error("Authentication failed");
        libssh2_session_disconnect(d->session, "Authentication failed");
        libssh2_session_free(d->session);
        d->session = nullptr;
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        return false;
    }
    
    // Initialize SFTP session
    d->sftp_session = libssh2_sftp_init(d->session);
    if (!d->sftp_session) {
        emit error("Failed to initialize SFTP session");
        libssh2_session_disconnect(d->session, "SFTP init failed");
        libssh2_session_free(d->session);
        d->session = nullptr;
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        return false;
    }
    
    m_connected = true;
    d->connected = true;
    d->currentPath = "/";
    
    emit connected();
    return true;
}

void FTPClient::disconnect()
{
    if (m_connected && d->session) {
        if (d->sftp_session) {
            libssh2_sftp_shutdown(d->sftp_session);
            d->sftp_session = nullptr;
        }
        
        libssh2_session_disconnect(d->session, "Normal Shutdown");
        libssh2_session_free(d->session);
        d->session = nullptr;
        
#ifdef _WIN32
        closesocket(d->sock);
#else
        ::close(d->sock);
#endif
        
        m_connected = false;
        d->connected = false;
        
        emit disconnected();
    }
}

bool FTPClient::isConnected() const
{
    return m_connected;
}

bool FTPClient::uploadFile(const QString &localPath, const QString &remotePath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Open local file
    QFile localFile(localPath);
    if (!localFile.open(QIODevice::ReadOnly)) {
        emit error("Failed to open local file: " + localPath);
        return false;
    }
    
    // Get file size
    qint64 fileSize = localFile.size();
    
    // Create remote file
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(d->sftp_session, remotePath.toStdString().c_str(),
                                                        LIBSSH2_FXF_WRITE | LIBSSH2_FXF_CREAT | LIBSSH2_FXF_TRUNC,
                                                        LIBSSH2_SFTP_S_IRUSR | LIBSSH2_SFTP_S_IWUSR |
                                                        LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IROTH);
    
    if (!sftp_handle) {
        emit error("Failed to open remote file: " + remotePath);
        localFile.close();
        return false;
    }
    
    // Upload file data
    char buffer[8192];
    qint64 totalSent = 0;
    
    while (!localFile.atEnd()) {
        qint64 bytesRead = localFile.read(buffer, sizeof(buffer));
        if (bytesRead < 0) {
            emit error("Failed to read from local file");
            libssh2_sftp_close(sftp_handle);
            localFile.close();
            return false;
        }
        
        char *ptr = buffer;
        ssize_t bytesWritten;
        do {
            bytesWritten = libssh2_sftp_write(sftp_handle, ptr, bytesRead);
            if (bytesWritten < 0) {
                emit error("Failed to write to remote file");
                libssh2_sftp_close(sftp_handle);
                localFile.close();
                return false;
            }
            
            ptr += bytesWritten;
            bytesRead -= bytesWritten;
            totalSent += bytesWritten;
            
            // Send progress signal
            emit transferProgress(totalSent, fileSize);
            
            // Allow event loop to process
            QThread::msleep(1);
            QCoreApplication::processEvents();
            
        } while (bytesRead > 0);
    }
    
    // Close files
    libssh2_sftp_close(sftp_handle);
    localFile.close();
    
    emit transferCompleted();
    return true;
}

bool FTPClient::downloadFile(const QString &remotePath, const QString &localPath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Open remote file
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_open(d->sftp_session, remotePath.toStdString().c_str(),
                                                        LIBSSH2_FXF_READ, 0);
    
    if (!sftp_handle) {
        emit error("Failed to open remote file: " + remotePath);
        return false;
    }
    
    // Get file attributes
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    if (libssh2_sftp_fstat(sftp_handle, &attrs) < 0) {
        emit error("Failed to get file attributes");
        libssh2_sftp_close(sftp_handle);
        return false;
    }
    
    // Create local file
    QFile localFile(localPath);
    if (!localFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit error("Failed to create local file: " + localPath);
        libssh2_sftp_close(sftp_handle);
        return false;
    }
    
    // Download file data
    char buffer[8192];
    qint64 totalReceived = 0;
    
    while (totalReceived < static_cast<qint64>(attrs.filesize)) {
        ssize_t bytesRead = libssh2_sftp_read(sftp_handle, buffer, sizeof(buffer));
        if (bytesRead < 0) {
            emit error("Failed to read from remote file");
            libssh2_sftp_close(sftp_handle);
            localFile.close();
            return false;
        }
        
        if (bytesRead == 0) {
            break; // EOF
        }
        
        qint64 bytesWritten = localFile.write(buffer, bytesRead);
        if (bytesWritten != bytesRead) {
            emit error("Failed to write to local file");
            libssh2_sftp_close(sftp_handle);
            localFile.close();
            return false;
        }
        
        totalReceived += bytesRead;
        
        // Send progress signal
        emit transferProgress(totalReceived, static_cast<qint64>(attrs.filesize));
        
        // Allow event loop to process
        QThread::msleep(1);
        QCoreApplication::processEvents();
    }
    
    // Close files
    libssh2_sftp_close(sftp_handle);
    localFile.close();
    
    emit transferCompleted();
    return true;
}

bool FTPClient::listDirectory(const QString &remotePath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Open directory
    LIBSSH2_SFTP_HANDLE *sftp_handle = libssh2_sftp_opendir(d->sftp_session, remotePath.toStdString().c_str());
    
    if (!sftp_handle) {
        emit error("Failed to open directory: " + remotePath);
        return false;
    }
    
    QStringList entries;
    char buffer[512];
    LIBSSH2_SFTP_ATTRIBUTES attrs;
    
    while (true) {
        int rc = libssh2_sftp_readdir(sftp_handle, buffer, sizeof(buffer), &attrs);
        if (rc <= 0) {
            break; // EOF or error
        }
        
        QString name = QString::fromUtf8(buffer, rc);
        
        // Skip "." and ".."
        if (name == "." || name == "..") {
            continue;
        }
        
        QString entryType;
        if (LIBSSH2_SFTP_S_ISDIR(attrs.permissions)) {
            entryType = "directory";
            name += "/";
        } else {
            entryType = "file";
        }
        
        QString fileSize;
        if (entryType == "file") {
            if (attrs.filesize < 1024) {
                fileSize = QString("%1 B").arg(attrs.filesize);
            } else if (attrs.filesize < 1024 * 1024) {
                fileSize = QString("%1 KB").arg(attrs.filesize / 1024.0, 0, 'f', 1);
            } else if (attrs.filesize < 1024 * 1024 * 1024) {
                fileSize = QString("%1 MB").arg(attrs.filesize / (1024.0 * 1024.0), 0, 'f', 1);
            } else {
                fileSize = QString("%1 GB").arg(attrs.filesize / (1024.0 * 1024.0 * 1024.0), 0, 'f', 1);
            }
        }
        
        QString modTime;
        if (attrs.mtime != 0) {
            QDateTime dateTime = QDateTime::fromSecsSinceEpoch(attrs.mtime);
            modTime = dateTime.toString("yyyy-MM-dd HH:mm:ss");
        }
        
        // Combine information, using | as separator
        entries << QString("%1|%2|%3|%4").arg(name, fileSize, entryType, modTime);
    }
    
    // Close directory
    libssh2_sftp_closedir(sftp_handle);
    
    // Save current path
    d->currentPath = remotePath;
    
    // Send directory list signal
    emit directoryListed(entries);
    
    return true;
}

bool FTPClient::createDirectory(const QString &remotePath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Create directory
    int rc = libssh2_sftp_mkdir(d->sftp_session, remotePath.toStdString().c_str(),
                              LIBSSH2_SFTP_S_IRWXU | LIBSSH2_SFTP_S_IRGRP | LIBSSH2_SFTP_S_IXGRP |
                              LIBSSH2_SFTP_S_IROTH | LIBSSH2_SFTP_S_IXOTH);
    
    if (rc != 0) {
        emit error("Failed to create directory: " + remotePath);
        return false;
    }
    
    return true;
}

bool FTPClient::removeFile(const QString &remotePath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Remove file
    int rc = libssh2_sftp_unlink(d->sftp_session, remotePath.toStdString().c_str());
    
    if (rc != 0) {
        emit error("Failed to remove file: " + remotePath);
        return false;
    }
    
    return true;
}

bool FTPClient::removeDirectory(const QString &remotePath)
{
    if (!m_connected || !d->sftp_session) {
        emit error("Not connected to SFTP server");
        return false;
    }
    
    // Remove directory
    int rc = libssh2_sftp_rmdir(d->sftp_session, remotePath.toStdString().c_str());
    
    if (rc != 0) {
        emit error("Failed to remove directory: " + remotePath);
        return false;
    }
    
    return true;
} 