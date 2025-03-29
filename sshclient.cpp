#include "sshclient.h"
#include <QDebug>
#include <QHostAddress>
#include <QNetworkProxy>
#include <QTimer>

// 禁用 gethostbyname 弃用警告
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#endif

SSHClient::SSHClient(QObject *parent)
    : QObject(parent), m_connected(false), m_session(nullptr), 
      m_socketDescriptor(INVALID_SOCKET), m_wsaInitialized(false),
      m_channel(nullptr), m_shellActive(false)
{
    initLibssh2();
}

SSHClient::~SSHClient()
{
    if (m_connected) {
        disconnect();
    }
    
    cleanupLibssh2();
    cleanupWsa();
}

bool SSHClient::initLibssh2()
{
#ifdef Q_OS_WIN
    // 初始化 Windows 套接字
    WSADATA wsadata;
    int err = WSAStartup(MAKEWORD(2, 0), &wsadata);
    if (err != 0) {
        qDebug() << "WSAStartup failed with error:" << err;
        return false;
    }
#endif
    
    int rc = libssh2_init(0);
    if (rc != 0) {
        qDebug() << "Failed to initialize libssh2:" << rc;
#ifdef Q_OS_WIN
        WSACleanup();
#endif
        return false;
    }
    return true;
}

void SSHClient::cleanupLibssh2()
{
    libssh2_exit();
    
#ifdef Q_OS_WIN
    // 清理 Windows 套接字
    WSACleanup();
#endif
}

bool SSHClient::initWsa()
{
#ifdef Q_OS_WIN
    if (!m_wsaInitialized) {
        WSADATA wsadata;
        int err = WSAStartup(MAKEWORD(2, 0), &wsadata);
        if (err != 0) {
            qDebug() << "WSAStartup failed with error:" << err;
            return false;
        }
        m_wsaInitialized = true;
    }
    return true;
#else
    return true; // 在非 Windows 平台上不需要初始化 WSA
#endif
}

void SSHClient::cleanupWsa()
{
#ifdef Q_OS_WIN
    if (m_wsaInitialized) {
        WSACleanup();
        m_wsaInitialized = false;
    }
#endif
}

bool SSHClient::connect(const QString &host, int port, const QString &username, const QString &password)
{
    if (m_connected) {
        disconnect();
    }
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    // 使用 QHostAddress 处理 IP 地址
    QHostAddress hostAddress;
    if (hostAddress.setAddress(host)) {
        // 如果是有效的 IP 地址
        sin.sin_addr.s_addr = htonl(hostAddress.toIPv4Address());
    } else {
        // 使用现代的 getaddrinfo 函数进行 DNS 解析
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        int status = getaddrinfo(host.toUtf8().constData(), nullptr, &hints, &result);
        if (status != 0) {
            emit error(QString("Failed to resolve hostname: %1 - %2").arg(host).arg(gai_strerror(status)));
            return false;
        }
        
        // 复制解析到的地址
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
        sin.sin_addr = ipv4->sin_addr;
        
        freeaddrinfo(result);
    }
    
    m_socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketDescriptor == INVALID_SOCKET) {
        emit error(QString("Failed to create socket: %1").arg(WSAGetLastError()));
        return false;
    }
    
    if (::connect(m_socketDescriptor, (struct sockaddr*)(&sin), sizeof(sin)) == SOCKET_ERROR) {
        emit error(QString("Failed to connect to %1:%2 - %3").arg(host).arg(port).arg(WSAGetLastError()));
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Create session
    m_session = libssh2_session_init();
    if (!m_session) {
        emit error("Failed to initialize SSH session");
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Set blocking mode
    libssh2_session_set_blocking(m_session, 1);
    
    // Handshake
    int rc = libssh2_session_handshake(m_session, m_socketDescriptor);
    if (rc) {
        emit error(QString("SSH handshake failed: %1").arg(rc));
        libssh2_session_free(m_session);
        m_session = nullptr;
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Authenticate
    if (!authenticateWithPassword(username, password)) {
        libssh2_session_free(m_session);
        m_session = nullptr;
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    m_connected = true;
    emit connected();
    return true;
}

bool SSHClient::connectWithKey(const QString &host, int port, const QString &username, const QString &privateKeyFile, const QString &passphrase)
{
    if (m_connected) {
        disconnect();
    }
    
    struct sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    
    // 使用 QHostAddress 处理 IP 地址
    QHostAddress hostAddress;
    if (hostAddress.setAddress(host)) {
        // 如果是有效的 IP 地址
        sin.sin_addr.s_addr = htonl(hostAddress.toIPv4Address());
    } else {
        // 使用现代的 getaddrinfo 函数进行 DNS 解析
        struct addrinfo hints, *result;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        
        int status = getaddrinfo(host.toUtf8().constData(), nullptr, &hints, &result);
        if (status != 0) {
            emit error(QString("Failed to resolve hostname: %1 - %2").arg(host).arg(gai_strerror(status)));
            return false;
        }
        
        // 复制解析到的地址
        struct sockaddr_in *ipv4 = (struct sockaddr_in *)result->ai_addr;
        sin.sin_addr = ipv4->sin_addr;
        
        freeaddrinfo(result);
    }
    
    m_socketDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socketDescriptor == INVALID_SOCKET) {
        emit error(QString("Failed to create socket: %1").arg(WSAGetLastError()));
        return false;
    }
    
    // 连接到服务器
    if (::connect(m_socketDescriptor, (struct sockaddr*)(&sin), sizeof(sin)) == SOCKET_ERROR) {
        emit error(QString("Failed to connect to %1:%2 - %3").arg(host).arg(port).arg(WSAGetLastError()));
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Create session
    m_session = libssh2_session_init();
    if (!m_session) {
        emit error("Failed to initialize SSH session");
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Set blocking mode
    libssh2_session_set_blocking(m_session, 1);
    
    // Handshake
    int rc = libssh2_session_handshake(m_session, m_socketDescriptor);
    if (rc) {
        emit error(QString("SSH handshake failed: %1").arg(rc));
        libssh2_session_free(m_session);
        m_session = nullptr;
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    // Authenticate
    if (!authenticateWithKey(username, privateKeyFile, passphrase)) {
        libssh2_session_free(m_session);
        m_session = nullptr;
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
        return false;
    }
    
    m_connected = true;
    emit connected();
    return true;
}

bool SSHClient::authenticateWithPassword(const QString &username, const QString &password)
{
    int rc = libssh2_userauth_password(m_session, username.toUtf8().constData(), password.toUtf8().constData());
    if (rc) {
        emit error(QString("Authentication by password failed: %1").arg(rc));
        return false;
    }
    return true;
}

bool SSHClient::authenticateWithKey(const QString &username, const QString &privateKeyFile, const QString &passphrase)
{
    int rc = libssh2_userauth_publickey_fromfile(
        m_session,
        username.toUtf8().constData(),
        nullptr,  // publickey file, can be null if private key contains public key
        privateKeyFile.toUtf8().constData(),
        passphrase.toUtf8().constData()
    );
    
    if (rc) {
        emit error(QString("Authentication by key failed: %1").arg(rc));
        return false;
    }
    return true;
}

void SSHClient::disconnect()
{
    if (!m_connected) {
        return;
    }
    
    if (m_channel) {
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
    }
    
    if (m_session) {
        libssh2_session_disconnect(m_session, "Normal Shutdown");
        libssh2_session_free(m_session);
        m_session = nullptr;
    }
    
    if (m_socketDescriptor != INVALID_SOCKET) {
        closesocket(m_socketDescriptor);
        m_socketDescriptor = INVALID_SOCKET;
    }
    
    m_connected = false;
    m_shellActive = false;
    emit disconnected();
}

bool SSHClient::isConnected() const
{
    return m_connected;
}

bool SSHClient::executeCommand(const QString &command)
{
    if (m_shellActive && m_channel) {
        QByteArray cmdWithNewline = command.toUtf8() + "\n";
        return sendData(cmdWithNewline);
    }
    
    if (!m_connected || !m_session) {
        emit error("Not connected to server");
        return false;
    }
    
    LIBSSH2_CHANNEL *channel = libssh2_channel_open_session(m_session);
    if (!channel) {
        char *errmsg;
        int errlen;
        int err = libssh2_session_last_error(m_session, &errmsg, &errlen, 0);
        emit error(QString("Failed to open channel: %1 - %2").arg(err).arg(QString::fromUtf8(errmsg, errlen)));
        return false;
    }
    
    // 执行命令
    int rc = libssh2_channel_exec(channel, command.toUtf8().constData());
    if (rc != 0) {
        char *errmsg;
        int errlen;
        int err = libssh2_session_last_error(m_session, &errmsg, &errlen, 0);
        emit error(QString("Failed to execute command: %1 - %2").arg(err).arg(QString::fromUtf8(errmsg, errlen)));
        libssh2_channel_free(channel);
        return false;
    }
    
    // Read output
    QByteArray output;
    char buffer[1024];
    ssize_t bytesRead;
    
    while ((bytesRead = libssh2_channel_read(channel, buffer, sizeof(buffer))) > 0) {
        output.append(buffer, bytesRead);
    }
    
    // Read stderr
    QByteArray errorOutput;
    while ((bytesRead = libssh2_channel_read_stderr(channel, buffer, sizeof(buffer))) > 0) {
        errorOutput.append(buffer, bytesRead);
    }
    
    // Close channel
    libssh2_channel_send_eof(channel);
    libssh2_channel_wait_eof(channel);
    libssh2_channel_wait_closed(channel);
    libssh2_channel_free(channel);
    
    // Emit output
    if (!output.isEmpty()) {
        emit dataReceived(output);
    }
    
    // Emit error output
    if (!errorOutput.isEmpty()) {
        emit dataReceived(errorOutput);
    }
    
    return true;
}

bool SSHClient::startShell()
{
    if (!m_connected || !m_session) {
        emit error("Not connected to server");
        return false;
    }
    
    if (m_shellActive) {
        return true; // Shell already active
    }
    
    // Open a channel
    m_channel = libssh2_channel_open_session(m_session);
    if (!m_channel) {
        char *errmsg;
        int errlen;
        int err = libssh2_session_last_error(m_session, &errmsg, &errlen, 0);
        emit error(QString("Failed to open channel: %1 - %2").arg(err).arg(QString::fromUtf8(errmsg, errlen)));
        return false;
    }
    
    // Request a pseudo-terminal (PTY)
    if (libssh2_channel_request_pty(m_channel, "xterm") != 0) {
        emit error("Failed to request PTY");
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }
    
    // Start a shell on the remote host
    if (libssh2_channel_shell(m_channel) != 0) {
        emit error("Failed to start shell");
        libssh2_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }
    
    // Set the channel to non-blocking mode
    libssh2_channel_set_blocking(m_channel, 0);
    
    m_shellActive = true;
    
    // Start a timer to periodically read from the channel
    QTimer *timer = new QTimer(this);
    QObject::connect(timer, &QTimer::timeout, this, &SSHClient::readChannel);
    timer->start(100); // Check every 100ms
    
    return true;
}

bool SSHClient::sendData(const QByteArray &data)
{
    if (!m_connected || !m_session || !m_channel || !m_shellActive) {
        emit error("Shell not active");
        return false;
    }
    
    ssize_t bytesWritten = libssh2_channel_write(m_channel, data.constData(), data.size());
    if (bytesWritten < 0) {
        emit error(QString("Failed to send data: %1").arg(bytesWritten));
        return false;
    }
    
    return true;
}

void SSHClient::readChannel()
{
    if (!m_connected || !m_session || !m_channel || !m_shellActive) {
        return;
    }
    
    char buffer[4096];
    ssize_t bytesRead;
    
    // Read from the channel
    bytesRead = libssh2_channel_read(m_channel, buffer, sizeof(buffer));
    if (bytesRead > 0) {
        emit dataReceived(QByteArray(buffer, bytesRead));
    } else if (bytesRead < 0 && bytesRead != LIBSSH2_ERROR_EAGAIN) {
        // Error reading from channel
        emit error(QString("Error reading from channel: %1").arg(bytesRead));
    }
    
    // Check if the channel is EOF
    if (libssh2_channel_eof(m_channel)) {
        emit error("Remote host has closed the connection");
        disconnect();
    }
}

bool SSHClient::waitSocket(int timeout_ms)
{
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    
    fd_set fd;
    fd_set *writefd = nullptr;
    fd_set *readfd = nullptr;
    int dir = libssh2_session_block_directions(m_session);
    
    FD_ZERO(&fd);
    FD_SET(m_socketDescriptor, &fd);
    
    if (dir & LIBSSH2_SESSION_BLOCK_INBOUND) {
        readfd = &fd;
    }
    
    if (dir & LIBSSH2_SESSION_BLOCK_OUTBOUND) {
        writefd = &fd;
    }
    
    return select(m_socketDescriptor + 1, readfd, writefd, nullptr, &tv) > 0;
} 
