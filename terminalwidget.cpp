#include "terminalwidget.h"
#include <QScrollBar>
#include <QToolBar>
#include <QAction>
#include <QKeyEvent>
#include <QFontDialog>
#include <QColorDialog>
#include <QSettings>
#include <QDebug>
#include <QTimer>
#include <QClipboard>
#include <QDateTime>
#include "sshclient.h"
#include "sshconnectionthread.h"
#include <QApplication>
#include <QRegularExpression>

// 定义ANSI颜色转义序列映射表
typedef struct {
    QString colStr;
    QColor col;
} ColorMapping;

static const ColorMapping G_colorMappings[] = {
    {"\033[0m", QColor()}, /* Default color - will be replaced with textColor */
    {"\033[1m", QColor()}, /* Bold - will be handled specially */

    /* Standard foreground colors */
    {"\033[30m", Qt::black},
    {"\033[31m", Qt::red},
    {"\033[32m", Qt::green},
    {"\033[33m", Qt::yellow},
    {"\033[34m", Qt::blue},
    {"\033[35m", Qt::magenta},
    {"\033[36m", Qt::cyan},
    {"\033[37m", Qt::white},

    /* Bright foreground colors */
    {"\033[90m", QColor(128, 128, 128)}, /* Bright black (gray) */
    {"\033[91m", QColor(255, 0, 0)},     /* Bright red */
    {"\033[92m", QColor(0, 255, 0)},     /* Bright green */
    {"\033[93m", QColor(255, 255, 0)},   /* Bright yellow */
    {"\033[94m", QColor(0, 0, 255)},     /* Bright blue */
    {"\033[95m", QColor(255, 0, 255)},   /* Bright magenta */
    {"\033[96m", QColor(0, 255, 255)},   /* Bright cyan */
    {"\033[97m", QColor(255, 255, 255)}, /* Bright white */

    /* File type specific colors from 'ls' output */
    {"\033[01;34m", QColor(0, 0, 255)},      /* Directory - bright blue */
    {"\033[01;36m", QColor(0, 255, 255)},    /* Symlink - bright cyan */
    {"\033[01;32m", QColor(0, 255, 0)},      /* Executable - bright green */
    {"\033[01;35m", QColor(255, 0, 255)},    /* Image file - bright magenta */
    {"\033[01;31m", QColor(255, 0, 0)}       /* Archive - bright red */
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent),
    m_connected(false), m_connectionThread(nullptr),
    historyPosition(-1), m_ansiEscapeMode(true), m_bold(false)
{
    // 默认终端设置
    terminalFont = QFont("Consolas", 10);
    backgroundColor = QColor("#1E1E1E");
    textColor = QColor("#DCDCDC");

    // 初始化 ANSI 颜色
    initAnsiColors();
    m_currentFgColor = textColor;
    m_currentBgColor = backgroundColor;

    // 加载保存的设置
    loadSettings();

    setupUI();

    // 初始化命令历史
    commandHistory.clear();

    // 初始化提示符
    m_currentPrompt = "> ";
}

TerminalWidget::~TerminalWidget()
{
    saveSettings();

    // 清理连接线程
    if (m_connectionThread) {
        if (m_connectionThread->isRunning()) {
            m_connectionThread->terminate();
            m_connectionThread->wait();
        }
        delete m_connectionThread;
        m_connectionThread = nullptr;
    }
}

void TerminalWidget::setupUI()
{
    layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    terminalOutput = new QTextEdit(this);
    terminalOutput->setReadOnly(false);
    terminalOutput->setAcceptRichText(true);
    terminalOutput->setUndoRedoEnabled(false);
    terminalOutput->setContextMenuPolicy(Qt::CustomContextMenu);
    terminalOutput->document()->setDefaultStyleSheet("");

    // 设置终端样式
    updateTerminalStyle();

    // 安装事件过滤器以捕获按键
    terminalOutput->installEventFilter(this);

    // 连接自定义上下文菜单
    connect(terminalOutput, &QTextEdit::customContextMenuRequested, this, &TerminalWidget::showContextMenu);

    layout->addWidget(terminalOutput);

    // 显示初始提示符
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(m_currentPrompt + " ");
    terminalOutput->setTextCursor(cursor);

    // 设置焦点
    terminalOutput->setFocus();
}

void TerminalWidget::updateTerminalStyle()
{
    // 设置字体
    terminalOutput->setFont(terminalFont);

    // 设置颜色
    QString styleSheet = QString(
                             "QTextEdit { background-color: %1; color: %2; border: none; }"
                             "QTextEdit::cursor { background-color: %2; }"
                             ).arg(backgroundColor.name(), textColor.name());

    terminalOutput->setStyleSheet(styleSheet);
}

bool TerminalWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == terminalOutput && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        // 如果没有连接，忽略按键
        if (!m_connected) {
            return QWidget::eventFilter(obj, event);
        }

        // 特殊按键处理
        int key = keyEvent->key();
        Qt::KeyboardModifiers modifiers = keyEvent->modifiers();

        // 处理回车键
        if (key == Qt::Key_Return || key == Qt::Key_Enter) {
            processCommand();
            return true;
        }

        // 处理上下键历史导航
        if (key == Qt::Key_Up) {
            handleCommandHistoryUp();
            return true;
        } else if (key == Qt::Key_Down) {
            handleCommandHistoryDown();
            return true;
        }

        // 处理退格键
        if (key == Qt::Key_Backspace) {
            // 获取当前行文本
            QTextCursor cursor = terminalOutput->textCursor();
            cursor.movePosition(QTextCursor::StartOfLine);
            cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
            QString currentLine = cursor.selectedText();

            // 检查是否已经在提示符后面
            if (currentLine == m_currentPrompt ||
                currentLine == m_currentPrompt + " " ||
                currentLine.length() <= m_currentPrompt.length() + 1) {
                return true;
            }
        }

        // 处理 Ctrl+C
        if (key == Qt::Key_C && modifiers == Qt::ControlModifier) {
            SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
            if (sshClient && sshClient->isConnected()) {
                // 发送 Ctrl+C (ASCII 3)
                sshClient->sendData(QByteArray(1, 3));
                return true;
            }
        }
    }

    return QWidget::eventFilter(obj, event);
}

void TerminalWidget::connectToSession(const SessionInfo &sessionInfo)
{
    if (m_connected) {
        disconnectFromSession();
    }

    m_host = sessionInfo.host;
    m_port = sessionInfo.port;
    m_username = sessionInfo.username;

    // 创建连接线程
    m_connectionThread = new SSHConnectionThread(this);

    // 手动设置连接参数
    if (sessionInfo.authType == 0) { // 0 = password authentication
        // 使用密码认证
        m_connectionThread->setConnectionParams(
            sessionInfo.host,
            sessionInfo.port,
            sessionInfo.username,
            sessionInfo.password
        );
    } else { // 1 = key authentication
        // 使用密钥认证
        m_connectionThread->setKeyConnectionParams(
            sessionInfo.host,
            sessionInfo.port,
            sessionInfo.username,
            sessionInfo.keyFile,
            "" // 如果需要密钥密码，这里可以添加
        );
    }

    // 连接信号和槽
    connect(m_connectionThread, &SSHConnectionThread::connectionEstablished, this, &TerminalWidget::handleConnectionEstablished);
    connect(m_connectionThread, &SSHConnectionThread::connectionFailed, this, &TerminalWidget::handleConnectionFailed);

    // 启动线程
    m_connectionThread->start();

    // 显示连接信息
    appendToTerminal(tr("Connecting to %1@%2:%3...\n").arg(sessionInfo.username).arg(sessionInfo.host).arg(sessionInfo.port));
}

void TerminalWidget::processCommand()
{
    // 获取当前行文本
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    QString currentLine = cursor.selectedText();

    // 提取命令
    QString command;

    // 多层次策略提取命令
    bool commandExtracted = false;

    QRegExp promptRegex("\\[[^\\]]+@[^\\]]+\\s+[^\\]]+\\][$#]\\s*");
    if (promptRegex.indexIn(currentLine) >= 0) {
        QString detectedPrompt = promptRegex.cap(0);
        int promptIndex = currentLine.indexOf(detectedPrompt);
        int commandStart = promptIndex + detectedPrompt.length();
        command = currentLine.mid(commandStart);

        // 更新当前提示符
        commandExtracted = true;
    }


    if (!commandExtracted) {
        command = currentLine.trimmed();
    }

    qDebug() << "final command:" << command;

    // 如果命令不为空，处理它
    if (!command.isEmpty()) {
        // 添加到历史记录
        addToHistory(command);

        // 添加换行
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n");
        terminalOutput->setTextCursor(cursor);

        // 发送命令到服务器
        SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
        if (sshClient && sshClient->isConnected()) {
            qDebug() << "Send to server command is: " << command;
            sshClient->sendData(command.toUtf8() + "\n");
        } else {
            qDebug() << "Can not connect to SSH client.";
        }
    } else {
        // 如果是空命令，只发送换行
        SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
        if (sshClient && sshClient->isConnected()) {
            sshClient->sendData(QByteArray(1, '\n'));
        }

        // 添加换行
        cursor.movePosition(QTextCursor::End);
        cursor.insertText("\n");
        terminalOutput->setTextCursor(cursor);
    }
}

void TerminalWidget::handleCommandHistoryUp()
{
    if (commandHistory.isEmpty()) {
        return;
    }

    // 保存当前命令，如果是第一次按上键
    if (historyPosition == -1) {
        // 获取当前行文本
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        QString currentLine = cursor.selectedText();

        // 提取当前命令
        if (currentLine.startsWith(m_currentPrompt + " ")) {
            m_savedCommand = currentLine.mid(m_currentPrompt.length() + 1);
        } else if (currentLine.startsWith(m_currentPrompt)) {
            m_savedCommand = currentLine.mid(m_currentPrompt.length());
        } else {
            m_savedCommand = "";
        }
    }

    // 移动到历史中的上一个命令
    if (historyPosition < commandHistory.size() - 1) {
        historyPosition++;

        // 替换当前行
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::StartOfLine);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        cursor.removeSelectedText();
        cursor.insertText(m_currentPrompt + " " + commandHistory[commandHistory.size() - 1 - historyPosition]);
        terminalOutput->setTextCursor(cursor);
    }
}

void TerminalWidget::handleCommandHistoryDown()
{
    if (historyPosition == -1) {
        return;
    }

    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::StartOfLine);
    cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
    cursor.removeSelectedText();

    if (historyPosition > 0) {
        historyPosition--;
        cursor.insertText(m_currentPrompt + " " + commandHistory[commandHistory.size() - 1 - historyPosition]);
    } else {
        // 回到保存的命令
        historyPosition = -1;
        cursor.insertText(m_currentPrompt + " " + m_savedCommand);
    }

    terminalOutput->setTextCursor(cursor);
}

void TerminalWidget::handleSSHData(const QByteArray &data)
{
    QString text = QString::fromUtf8(data);
    qDebug() << "Received data from SSH: " << text;

    text.replace("\r", "");  // 过滤回车符，避免重复行

    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat defaultFormat = cursor.charFormat();
    QTextCharFormat currentFormat = defaultFormat;
    QColor textColor = this->textColor;
    QColor backgroundColor;

    int lastPos = 0;

    QRegularExpression ansiRegex(
        "\\x1B\\[[0-9;]*[mHJfABCDsuK]|" // ANSI 控制序列
        "\\x1B\\]0;.*?\\x07|"           // OSC 标题
        "\\x1B\\(B|"                    // 字符集切换
        "\\x1B>|"                       // 模式切换
        "\\x1B\\[\\?[0-9]*[hl]"         // 终端模式
        );

    QRegularExpressionMatchIterator it = ansiRegex.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();
        QString captured = match.captured(0);
        QString codeSeq = match.captured(1);

        QString plainText = text.mid(lastPos, start - lastPos);
        if (!plainText.isEmpty()) {
            cursor.insertText(plainText, currentFormat);
        }

        if (captured == "\x1B[H" || captured == "\x1B[2J") {
            terminalOutput->clear(); // 清屏
            cursor = terminalOutput->textCursor();
            lastPos = end;
        } else if (captured == "\x1B(K" || captured.startsWith("\x1B[")) {
            lastPos = end; // 忽略 ANSI 码
        } else if (captured.endsWith("m")) {
            QStringList codes = codeSeq.isEmpty() ? QStringList("") : codeSeq.split(";");
            for (const QString &code : codes) {
                bool ok;
                int value = code.toInt(&ok);
                if (!ok && code.isEmpty()) value = 0; // \x1B[m 等同于 \x1B[0m

                switch (value) {
                case 0: // 重置样式
                    currentFormat = defaultFormat;
                    textColor = this->textColor;
                    backgroundColor = QColor();
                    currentFormat.setForeground(textColor);
                    currentFormat.setBackground(QBrush());
                    currentFormat.setFontWeight(QFont::Normal);
                    currentFormat.setFontUnderline(false);
                    break;
                case 1: // 加粗
                    currentFormat.setFontWeight(QFont::Bold);
                    break;
                case 4: // 下划线
                    currentFormat.setFontUnderline(true);
                    break;
                case 7: // 反显
                    currentFormat.setBackground(textColor);
                    currentFormat.setForeground(backgroundColor.isValid() ? backgroundColor : this->textColor);
                    break;
                case 39: // 重置前景色
                    textColor = this->textColor;
                    currentFormat.setForeground(textColor);
                    break;
                case 49: // 重置背景色
                    backgroundColor = QColor();
                    currentFormat.setBackground(QBrush());
                    break;
                // 标准前景色 (30-37)
                case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
                    textColor = G_colorMappings[value - 30 + 2].col;
                    currentFormat.setForeground(textColor);
                    break;
                // 标准背景色 (40-47)
                case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
                    backgroundColor = G_colorMappings[value - 40 + 10].col;
                    currentFormat.setBackground(backgroundColor);
                    break;
                // 亮色前景 (90-97)
                case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
                    textColor = G_colorMappings[value - 90 + 10].col;
                    currentFormat.setForeground(textColor);
                    break;
                // 亮色背景 (100-107)
                case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
                    backgroundColor = G_colorMappings[value - 100 + 18].col;
                    currentFormat.setBackground(backgroundColor);
                    break;
                default:
                    qDebug() << "Unhandled ANSI code:" << value;
                    break;
                }
            }
            lastPos = end;
        } else {
            lastPos = end;
        }
    }

    QString remainingText = text.mid(lastPos);
    if (!remainingText.isEmpty()) {
        cursor.insertText(remainingText, currentFormat);
    }

    terminalOutput->setTextCursor(cursor);
    terminalOutput->ensureCursorVisible();
}



void TerminalWidget::handleSSHError(const QString &error)
{
    appendToTerminal("Error: " + error + "\n");
}

void TerminalWidget::handleSSHDisconnected()
{
    appendToTerminal("Disconnected from server.\n");

    // 重置提示符
    m_currentPrompt = "> ";

    // 显示提示符
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n" + m_currentPrompt + " ");
    terminalOutput->setTextCursor(cursor);

    // 更新连接状态
    m_connected = false;
}

void TerminalWidget::handleSSHConnected()
{
    appendToTerminal(tr("SSH connection established.\n"));
    appendToTerminal(tr("Authenticating...\n"));
}

void TerminalWidget::handleConnectionEstablished()
{
    m_connected = true;
    appendToTerminal("Connection established.\n");

    // 获取SSH客户端
    SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
    if (sshClient) {
        // 连接信号
        connect(sshClient, &SSHClient::dataReceived, this, &TerminalWidget::handleSSHData);
        connect(sshClient, &SSHClient::error, this, &TerminalWidget::handleSSHError);
        connect(sshClient, &SSHClient::disconnected, this, &TerminalWidget::handleSSHDisconnected);
        connect(sshClient, &SSHClient::connected, this, &TerminalWidget::handleSSHConnected);

        // 启动shell
        sshClient->startShell();
    }
}

void TerminalWidget::handleConnectionFailed(const QString &errorMessage)
{
    appendToTerminal("Connection failed: " + errorMessage + "\n");

    // 显示提示符
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n" + m_currentPrompt + " ");
    terminalOutput->setTextCursor(cursor);
}

void TerminalWidget::disconnectFromSession()
{
    if (!m_connected) {
        return;
    }

    // 断开SSH连接
    if (m_connectionThread) {
        SSHClient *sshClient = m_connectionThread->getSSHClient();
        if (sshClient) {
            // 断开连接前发送退出命令
            sshClient->sendData("exit\n");

            // 等待一小段时间让命令执行
            QThread::msleep(100);

            // 断开连接
            sshClient->disconnect();
        }

        // 停止线程
        if (m_connectionThread->isRunning()) {
            m_connectionThread->terminate();
            m_connectionThread->wait();
        }

        delete m_connectionThread;
        m_connectionThread = nullptr;
    }

    // 更新连接状态
    m_connected = false;

    // 显示断开连接信息
    appendToTerminal("\nDisconnected from server.\n");

    // 重置提示符
    m_currentPrompt = "> ";

    // 显示提示符
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText("\n" + m_currentPrompt + " ");
    terminalOutput->setTextCursor(cursor);
}

void TerminalWidget::showContextMenu(const QPoint &pos)
{
    QMenu menu(this);

    QAction *copyAction = menu.addAction(tr("Copy"));
    QAction *pasteAction = menu.addAction(tr("Paste"));
    menu.addSeparator();
    QAction *clearAction = menu.addAction(tr("Clear"));
    menu.addSeparator();
    QAction *fontAction = menu.addAction(tr("Change Font..."));
    QAction *bgColorAction = menu.addAction(tr("Change Background Color..."));
    QAction *textColorAction = menu.addAction(tr("Change Text Color..."));

    // 只有在有选中文本时才启用复制
    copyAction->setEnabled(terminalOutput->textCursor().hasSelection());

    // 只有在有剪贴板内容时才启用粘贴
    pasteAction->setEnabled(!QApplication::clipboard()->text().isEmpty());

    QAction *selectedAction = menu.exec(terminalOutput->mapToGlobal(pos));

    if (selectedAction == copyAction) {
        copySelectedText();
    } else if (selectedAction == pasteAction) {
        pasteClipboard();
    } else if (selectedAction == clearAction) {
        clearTerminal();
    } else if (selectedAction == fontAction) {
        changeFont();
    } else if (selectedAction == bgColorAction) {
        changeBackgroundColor();
    } else if (selectedAction == textColorAction) {
        changeTextColor();
    }
}

void TerminalWidget::copySelectedText()
{
    terminalOutput->copy();
}

void TerminalWidget::pasteClipboard()
{
    QString clipboardText = QApplication::clipboard()->text();
    if (!clipboardText.isEmpty()) {
        // 将剪贴板内容插入到当前光标位置
        terminalOutput->insertPlainText(clipboardText);
    }
}

void TerminalWidget::clearTerminal()
{
    terminalOutput->clear();

    // 显示提示符
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(m_currentPrompt + " ");
    terminalOutput->setTextCursor(cursor);
}

void TerminalWidget::changeFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, terminalFont, this);
    if (ok) {
        terminalFont = font;
        updateTerminalStyle();
        saveSettings();
    }
}

void TerminalWidget::changeBackgroundColor()
{
    QColor color = QColorDialog::getColor(backgroundColor, this);
    if (color.isValid()) {
        backgroundColor = color;
        updateTerminalStyle();
        saveSettings();
    }
}

void TerminalWidget::changeTextColor()
{
    QColor color = QColorDialog::getColor(textColor, this);
    if (color.isValid()) {
        textColor = color;
        updateTerminalStyle();
        saveSettings();
    }
}

void TerminalWidget::saveSettings()
{
    QSettings settings;
    settings.beginGroup("Terminal");
    settings.setValue("FontFamily", terminalFont.family());
    settings.setValue("FontSize", terminalFont.pointSize());
    settings.setValue("BackgroundColor", backgroundColor.name());
    settings.setValue("TextColor", textColor.name());
    settings.endGroup();
}

void TerminalWidget::loadSettings()
{
    QSettings settings;
    settings.beginGroup("Terminal");

    QString fontFamily = settings.value("FontFamily", terminalFont.family()).toString();
    int fontSize = settings.value("FontSize", terminalFont.pointSize()).toInt();
    terminalFont = QFont(fontFamily, fontSize);

    backgroundColor = QColor(settings.value("BackgroundColor", backgroundColor.name()).toString());
    textColor = QColor(settings.value("TextColor", textColor.name()).toString());

    settings.endGroup();
}

void TerminalWidget::addToHistory(const QString &command)
{
    // 不添加空命令或与最后一个命令相同的命令
    if (command.isEmpty() || (!commandHistory.isEmpty() && commandHistory.last() == command)) {
        return;
    }

    commandHistory.append(command);

    // 限制历史大小
    while (commandHistory.size() > 100) {
        commandHistory.removeFirst();
    }

    historyPosition = -1;
}

void TerminalWidget::appendToTerminal(const QString &processedText)
{
    QTextCursor cursor = terminalOutput->textCursor();

    // Store current position
    int currentPosition = cursor.position();

    // Move cursor to end for appending
    cursor.movePosition(QTextCursor::End);

    // Insert text with current formatting
    QTextCharFormat format;
    format.setForeground(m_currentFgColor);
    format.setBackground(m_currentBgColor);
    if (m_bold) {
        format.setFontWeight(QFont::Bold);
    } else {
        format.setFontWeight(QFont::Normal);
    }
    cursor.insertText(processedText, format);

    // Restore cursor if it was in the editing area
    if (currentPosition > terminalOutput->document()->characterCount() - processedText.length()) {
        cursor.movePosition(QTextCursor::End);
        terminalOutput->setTextCursor(cursor);
    }

    // Ensure the cursor is visible
    terminalOutput->ensureCursorVisible();
}

void TerminalWidget::initAnsiColors()
{
    // Standard ANSI colors
    m_ansiColors[0] = QColor(0, 0, 0);         // Black
    m_ansiColors[1] = QColor(170, 0, 0);       // Red
    m_ansiColors[2] = QColor(0, 170, 0);       // Green
    m_ansiColors[3] = QColor(170, 85, 0);      // Yellow
    m_ansiColors[4] = QColor(0, 0, 170);       // Blue
    m_ansiColors[5] = QColor(170, 0, 170);     // Magenta
    m_ansiColors[6] = QColor(0, 170, 170);     // Cyan
    m_ansiColors[7] = QColor(170, 170, 170);   // White

    // Bright ANSI colors
    m_ansiColors[8] = QColor(85, 85, 85);      // Bright Black (Gray)
    m_ansiColors[9] = QColor(255, 85, 85);     // Bright Red
    m_ansiColors[10] = QColor(85, 255, 85);    // Bright Green
    m_ansiColors[11] = QColor(255, 255, 85);   // Bright Yellow
    m_ansiColors[12] = QColor(85, 85, 255);    // Bright Blue
    m_ansiColors[13] = QColor(255, 85, 255);   // Bright Magenta
    m_ansiColors[14] = QColor(85, 255, 255);   // Bright Cyan
    m_ansiColors[15] = QColor(255, 255, 255);  // Bright White
}
