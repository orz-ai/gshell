#ifndef SESSIONINFO_H
#define SESSIONINFO_H

#include <QString>

struct SessionInfo {
    QString name;
    QString host;
    int port;
    QString username;
    QString password;
    bool savePassword;
    QString privateKeyFile;
    int authType; // 0 = password, 1 = key
    QString keyFile;
    
    // Terminal settings
    QString terminalType;
    QString encoding;
    bool keepAlive;
    int keepAliveInterval;
    
    // Appearance settings
    QString fontName;
    int fontSize;
    QString backgroundColor;
    QString textColor;
    
    SessionInfo() : port(22), savePassword(false), authType(0), keepAlive(true), 
                   keepAliveInterval(60), fontName("Consolas"), fontSize(10), 
                   backgroundColor("#1E1E1E"), textColor("#DCDCDC") {}
};

#endif // SESSIONINFO_H 