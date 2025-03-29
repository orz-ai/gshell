#include "sshconnectionthread.h"

SSHConnectionThread::SSHConnectionThread(QObject *parent)
    : QThread(parent), m_sshClient(nullptr), m_port(22), m_useKey(false)
{
    m_sshClient = new SSHClient();
    
    // 连接信号以便在线程中转发
    connect(m_sshClient, &SSHClient::connected, this, &SSHConnectionThread::connectionEstablished);
    connect(m_sshClient, &SSHClient::error, this, &SSHConnectionThread::connectionFailed);
}

SSHConnectionThread::~SSHConnectionThread()
{
    // 确保线程停止
    if (isRunning()) {
        terminate();
        wait();
    }
    
    // 清理资源
    if (m_sshClient) {
        if (m_sshClient->isConnected()) {
            m_sshClient->disconnect();
        }
        delete m_sshClient;
        m_sshClient = nullptr;
    }
}

void SSHConnectionThread::setConnectionParams(const QString &host, int port, const QString &username, const QString &password)
{
    m_host = host;
    m_port = port;
    m_username = username;
    m_password = password;
    m_useKey = false;
}

void SSHConnectionThread::setKeyConnectionParams(const QString &host, int port, const QString &username, 
                                               const QString &privateKeyFile, const QString &passphrase)
{
    m_host = host;
    m_port = port;
    m_username = username;
    m_privateKeyFile = privateKeyFile;
    m_passphrase = passphrase;
    m_useKey = true;
}

void SSHConnectionThread::run()
{
    bool success = false;
    
    if (m_useKey) {
        success = m_sshClient->connectWithKey(m_host, m_port, m_username, m_privateKeyFile, m_passphrase);
    } else {
        success = m_sshClient->connect(m_host, m_port, m_username, m_password);
    }
    
    // 如果连接失败但没有发出错误信号，发出一个通用错误
    if (!success && !m_sshClient->isConnected()) {
        emit connectionFailed("Failed to establish SSH connection");
    }
} 