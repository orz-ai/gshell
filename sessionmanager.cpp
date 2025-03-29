#include "sessionmanager.h"
#include <QSettings>
#include <QCryptographicHash>
#include <QDebug>

SessionManager::SessionManager(QObject *parent) : QObject(parent)
{
    loadSessions();
}

QList<SessionInfo> SessionManager::getSessions() const
{
    return m_sessions.values();
}

SessionInfo SessionManager::getSession(const QString &id) const
{
    return m_sessions.value(id);
}

QString SessionManager::encryptPassword(const QString &password)
{
    if (password.isEmpty()) {
        return QString();
    }
    
    // 一个非常简单的 XOR 加密，仅用于基本混淆
    // 注意：这不是真正的安全加密，只是为了避免明文存储
    QByteArray passwordBytes = password.toUtf8();
    QByteArray result;
    
    for (int i = 0; i < passwordBytes.size(); ++i) {
        result.append(passwordBytes[i] ^ m_encryptionKey[i % m_encryptionKey.size()]);
    }
    
    return result.toBase64();
}

QString SessionManager::decryptPassword(const QString &encryptedPassword)
{
    if (encryptedPassword.isEmpty()) {
        return QString();
    }
    
    // 解密
    QByteArray encryptedBytes = QByteArray::fromBase64(encryptedPassword.toUtf8());
    QByteArray result;
    
    for (int i = 0; i < encryptedBytes.size(); ++i) {
        result.append(encryptedBytes[i] ^ m_encryptionKey[i % m_encryptionKey.size()]);
    }
    
    return QString::fromUtf8(result);
}

void SessionManager::saveSession(const SessionInfo &session)
{
    QSettings settings;
    settings.beginGroup("Sessions");
    
    // 生成唯一的会话 ID
    QString sessionId = session.name.isEmpty() ? 
        QString("%1@%2:%3").arg(session.username, session.host).arg(session.port) : 
        session.name;
    
    settings.beginGroup(sessionId);
    settings.setValue("name", session.name);
    settings.setValue("host", session.host);
    settings.setValue("port", session.port);
    settings.setValue("username", session.username);
    // 保存加密后的密码
    settings.setValue("password", encryptPassword(session.password));
    settings.setValue("authType", session.authType);
    settings.setValue("keyFile", session.keyFile);
    // 保存加密后的密钥密码
    settings.setValue("keyPassphrase", encryptPassword(session.password));
    
    // 保存终端外观设置
    settings.setValue("fontName", session.fontName);
    settings.setValue("fontSize", session.fontSize);
    settings.setValue("backgroundColor", session.backgroundColor);
    settings.setValue("textColor", session.textColor);
    
    // 保存终端设置
    settings.setValue("terminalType", session.terminalType);
    settings.setValue("encoding", session.encoding);
    settings.setValue("keepAlive", session.keepAlive);
    settings.setValue("keepAliveInterval", session.keepAliveInterval);
    
    settings.endGroup();
    settings.endGroup();
    
    // 更新内存中的会话列表
    m_sessions[sessionId] = session;
    
    emit sessionsChanged();
}

void SessionManager::deleteSession(const QString &id)
{
    if (!m_sessions.contains(id)) {
        return;
    }
    
    QSettings settings;
    settings.beginGroup("Sessions");
    settings.remove(id);
    settings.endGroup();
    
    m_sessions.remove(id);
    
    emit sessionsChanged();
}

void SessionManager::loadSessions()
{
    m_sessions.clear();
    
    QSettings settings;
    settings.beginGroup("Sessions");
    
    QStringList sessionIds = settings.childGroups();
    for (const QString &sessionId : sessionIds) {
        settings.beginGroup(sessionId);
        
        SessionInfo session;
        session.name = settings.value("name").toString();
        session.host = settings.value("host").toString();
        session.port = settings.value("port", 22).toInt();
        session.username = settings.value("username").toString();
        session.password = decryptPassword(settings.value("password").toString());
        session.authType = settings.value("authType", 0).toInt();
        session.keyFile = settings.value("keyFile").toString();
        
        // 如果是密钥认证，加载密钥密码
        if (session.authType == 1) {
            session.password = decryptPassword(settings.value("keyPassphrase").toString());
        }
        
        // 加载终端外观设置
        session.fontName = settings.value("fontName", "Consolas").toString();
        session.fontSize = settings.value("fontSize", 10).toInt();
        session.backgroundColor = settings.value("backgroundColor", "#1E1E1E").toString();
        session.textColor = settings.value("textColor", "#DCDCDC").toString();
        
        // 加载终端设置
        session.terminalType = settings.value("terminalType", "xterm").toString();
        session.encoding = settings.value("encoding", "UTF-8").toString();
        session.keepAlive = settings.value("keepAlive", true).toBool();
        session.keepAliveInterval = settings.value("keepAliveInterval", 60).toInt();
        
        m_sessions[sessionId] = session;
        
        settings.endGroup();
    }
    
    settings.endGroup();
    
    emit sessionsChanged();
}

void SessionManager::saveSessions()
{
    QSettings settings("SSHFTPClient", "Sessions");
    settings.beginWriteArray("sessions");
    
    int i = 0;
    QMapIterator<QString, SessionInfo> it(m_sessions);
    while (it.hasNext()) {
        it.next();
        settings.setArrayIndex(i);
        
        const SessionInfo &session = it.value();
        settings.setValue("name", session.name);
        settings.setValue("host", session.host);
        settings.setValue("port", session.port);
        settings.setValue("username", session.username);
        settings.setValue("privateKeyFile", session.privateKeyFile);
        settings.setValue("savePassword", session.savePassword);
        
        if (session.savePassword && !session.password.isEmpty()) {
            settings.setValue("password", encryptPassword(session.password));
        }
        i++;
    }
    
    settings.endArray();
}

// 这个函数已经在上面定义过了，所以这里注释掉
/*
bool SessionManager::saveSession(const SessionInfo &session)
{
    QSettings settings;
    settings.beginGroup("Sessions");
    settings.beginGroup(session.name);
    
    settings.setValue("Host", session.host);
    settings.setValue("Port", session.port);
    settings.setValue("Username", session.username);
    settings.setValue("AuthType", session.authType);
    settings.setValue("KeyFile", session.keyFile);
    
    // Save terminal settings
    settings.setValue("TerminalType", session.terminalType);
    settings.setValue("Encoding", session.encoding);
    settings.setValue("KeepAlive", session.keepAlive);
    settings.setValue("KeepAliveInterval", session.keepAliveInterval);
    
    // Save appearance settings
    settings.setValue("FontName", session.fontName);
    settings.setValue("FontSize", session.fontSize);
    settings.setValue("BackgroundColor", session.backgroundColor);
    settings.setValue("TextColor", session.textColor);
    
    // Only save password if requested
    if (session.savePassword) {
        settings.setValue("Password", session.password);
    } else {
        settings.remove("Password");
    }
    
    settings.endGroup();
    settings.endGroup();
    return true;
}
*/

// 这个函数已经在上面定义过了，所以这里注释掉
/*
SessionInfo SessionManager::getSession(const QString &name) const
{
    SessionInfo session;
    session.name = name;
    
    QSettings settings;
    settings.beginGroup("Sessions");
    settings.beginGroup(name);
    
    session.host = settings.value("Host").toString();
    session.port = settings.value("Port", 22).toInt();
    session.username = settings.value("Username").toString();
    session.password = settings.value("Password").toString();
    session.authType = settings.value("AuthType", 0).toInt();
    session.keyFile = settings.value("KeyFile").toString();
    
    // Load terminal settings
    session.terminalType = settings.value("TerminalType", "xterm").toString();
    session.encoding = settings.value("Encoding", "UTF-8").toString();
    session.keepAlive = settings.value("KeepAlive", true).toBool();
    session.keepAliveInterval = settings.value("KeepAliveInterval", 60).toInt();
    
    // Load appearance settings
    session.fontName = settings.value("FontName", "Consolas").toString();
    session.fontSize = settings.value("FontSize", 10).toInt();
    session.backgroundColor = settings.value("BackgroundColor", "#1E1E1E").toString();
    session.textColor = settings.value("TextColor", "#DCDCDC").toString();
    
    settings.endGroup();
    settings.endGroup();
    
    return session;
}
*/ 
