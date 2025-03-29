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
#include "sessioninfo.h"

class SSHConnectionThread;

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

    void setupUI();
    void updateTerminalStyle();
    void appendToTerminal(const QString &text);
    void saveSettings();
    void loadSettings();
    void addToHistory(const QString &command);
    void initAnsiColors();
};

#endif // TERMINALWIDGET_H
