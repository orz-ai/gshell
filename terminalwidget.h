#ifndef TERMINALWIDGET_H
#define TERMINALWIDGET_H

#include <QWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QMenu>
#include <QStringList>
#include <QSettings>
#include <QFontDialog>
#include <QColorDialog>
#include <QMap>
#include <QFile>
#include <QTimer>
#include "sessioninfo.h"

class SSHConnectionThread;

// ZMODEM protocol control characters and states
#define ZPAD            '*'    // Padding character
#define ZDLE            0x18   // Escape character
#define ZDLEE           0x58   // Escaped ZDLE
#define ZBIN            'A'    // Binary header
#define ZHEX            'B'    // Hex header
#define ZBIN32          'C'    // Binary header with 32-bit CRC

// ZMODEM frame types
#define ZRQINIT         0      // Request init
#define ZRINIT          1      // Receive init
#define ZSINIT          2      // Send init
#define ZACK            3      // Acknowledge
#define ZFILE           4      // File name
#define ZSKIP           5      // Skip file
#define ZNAK            6      // Error
#define ZABORT          7      // Abort
#define ZFIN            8      // Finish
#define ZRPOS           9      // Resume position
#define ZDATA           10     // Data
#define ZEOF            11     // End of file
#define ZFERR           12     // File error
#define ZCRC            13     // CRC
#define ZCHALLENGE      14     // Challenge
#define ZCOMPL          15     // Complete
#define ZCAN            16     // Cancel
#define ZFREECNT        17     // Free bytes
#define ZCOMMAND        18     // Command
#define ZSTDERR         19     // Standard error

// ZMODEM frame end types
#define ZCRCE           'h'    // CRC next, frame ends, header follows
#define ZCRCG           'i'    // CRC next, frame continues nonstop
#define ZCRCQ           'j'    // CRC next, frame continues, ZACK expected
#define ZCRCW           'k'    // CRC next, frame ends, ZACK expected

// ZMODEM carrier classes
#define C0              0      // Command channels
#define C1              1      // Communications channels
#define C2              2      // Printer channels
#define C3              3      // User channels

class TerminalWidget : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalWidget(QWidget *parent = nullptr);
    ~TerminalWidget();

    void connectToSession(const SessionInfo &sessionInfo);
    void disconnectFromSession();
    bool isConnected() const { return m_connected; }
    bool eventFilter(QObject *obj, QEvent *event) override;

public slots:
    void handleSSHData(const QByteArray &data);
    void handleSSHError(const QString &error);
    void handleSSHDisconnected();
    void handleSSHConnected();
    void handleConnectionEstablished();
    void handleConnectionFailed(const QString &errorMessage);

private slots:
    void showContextMenu(const QPoint &pos);
    void copySelectedText();
    void pasteClipboard();
    void clearTerminal();
    void changeFont();
    void changeBackgroundColor();
    void changeTextColor();
    void processCommand();
    void handleCommandHistoryUp();
    void handleCommandHistoryDown();
    
    // ZMODEM specific slots
    void startZmodemUpload();
    void uploadNextZmodemPacket();
    void zmodemTransferTimeout();

private:
    QVBoxLayout *layout;
    QTextEdit *terminalOutput;

    QFont terminalFont;
    QColor backgroundColor;
    QColor textColor;

    bool m_connected;
    QString m_host;
    int m_port;
    QString m_username;

    SSHConnectionThread *m_connectionThread;

    QStringList commandHistory;
    int historyPosition;
    QString m_savedCommand;
    
    // 当前提示符
    QString m_currentPrompt;

    // ANSI 转义序列处理
    bool m_ansiEscapeMode;
    QString m_ansiEscapeBuffer;
    QMap<int, QColor> m_ansiColors;
    QColor m_currentFgColor;
    QColor m_currentBgColor;
    bool m_bold;

    // ZMODEM protocol support
    bool m_zmodemActive;
    QByteArray m_zmodemBuffer;
    
    // ZMODEM file transfer
    QString m_zmodemFilePath;
    QFile m_zmodemFile;
    qint64 m_zmodemFileSize;
    qint64 m_zmodemFilePos;
    int m_zmodemState;
    QTimer m_zmodemTimer;
    bool m_zmodemUploadStarted;
    int m_zmodemPacketSize;
    int m_zmodemErrorCount;
    bool m_zmodemCancel;
    bool m_zmodemProcessing;  // Flag to track processing state
    
    void setupUI();
    void updateTerminalStyle();
    void appendToTerminal(const QString &text);
    void saveSettings();
    void loadSettings();
    void addToHistory(const QString &command);
    void initAnsiColors();
    
    // ZMODEM methods
    bool detectZmodem(const QByteArray &data);
    void handleZmodemDetected();
    bool startZmodemFileTransfer();
    QByteArray createZmodemHeader(int frameType, quint32 pos = 0);
    QByteArray escapeZmodemData(const QByteArray &data);
    quint16 calculateCRC16(const QByteArray &data);
    QByteArray createZmodemDataPacket(const QByteArray &data, bool last = false);
    void sendZmodemCancel();
    void resetZmodemState();
    void completeZmodemTransfer(bool success);
    void updateZmodemProgress(qint64 sent, qint64 total);
    void processZmodemResponse();
};

#endif // TERMINALWIDGET_H
