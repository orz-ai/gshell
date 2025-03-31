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
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QCryptographicHash>
#include <QThread>

// ZMODEM detection sequences
#define ZMODEM_DETECT_HEADER "\x18\x2a\x45"
#define ZMODEM_DETECT_HEADER_LEN 3

// ZMODEM protocol control characters and states - repeated for compilation convenience
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
    historyPosition(-1), m_ansiEscapeMode(true), m_bold(false),
    m_zmodemActive(false), m_zmodemUploadStarted(false), m_zmodemErrorCount(0), m_zmodemCancel(false),
    m_zmodemProcessing(false)
{
    // 默认终端设置
    terminalFont = QFont("Consolas", 10);
    backgroundColor = QColor("#1E1E1E");
    textColor = QColor("#DCDCDC");

    // 初始化 ANSI 颜色
    initAnsiColors();
    m_currentFgColor = textColor;
    m_currentBgColor = backgroundColor;
    
    // 清除 ZMODEM 缓冲区
    m_zmodemBuffer.clear();
    
    // 设置 ZMODEM 参数
    m_zmodemPacketSize = 512;  // Smaller packet size for better reliability
    m_zmodemState = 0;
    
    // Setup ZMODEM timer for timeout handling
    connect(&m_zmodemTimer, &QTimer::timeout, this, &TerminalWidget::zmodemTransferTimeout);

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
    // First check for ZMODEM protocol - but only if not already active
    if (!m_zmodemActive && !m_zmodemUploadStarted && detectZmodem(data)) {
        m_zmodemActive = true; // Set flag immediately to prevent multiple detections
        handleZmodemDetected();
        return;
    }
    
    // If ZMODEM is active but we're not handling the data, just accumulate it
    if (m_zmodemActive && !m_zmodemUploadStarted) {
        m_zmodemBuffer.append(data);
        return;
    }
    
    // If ZMODEM transfer is in progress, process the response data
    if (m_zmodemUploadStarted) {
        m_zmodemBuffer.append(data);
        processZmodemResponse();
        return;
    }
    
    QString text = QString::fromUtf8(data);
    qDebug() << "Received data from SSH: " << text;

    // 检查是否是命令回显（以命令和\r\n开头）
    static QString lastCommand = "";
    static bool expectingOutput = false;

    if (expectingOutput && text.startsWith(lastCommand)) {
        // Remove just the command part (not requiring \r\n which might be split in packets)
        text = text.mid(lastCommand.length());

        // If there's a leading \r\n, skip it too
        if (text.startsWith("\r\n")) {
            text = text.mid(2);
        }

        expectingOutput = false;
    }
    else if (text.endsWith("\n$ ") || text.endsWith("\n# ")) {
        // Get the command from the prompt, more reliably
        QRegularExpression promptRegex("\\[(.*?)\\]# $");
        QRegularExpressionMatch match = promptRegex.match(text);

        if (match.hasMatch()) {
            // Don't store the entire text as lastCommand, just the command itself
            expectingOutput = true;
            // Extract just the command, not the whole prompt
            lastCommand = match.captured(1);
        } else {
            expectingOutput = false;
        }
    }

    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);

    QTextCharFormat defaultFormat = cursor.charFormat();
    QTextCharFormat currentFormat = defaultFormat;
    QColor textColor = this->textColor;
    QColor backgroundColor = this->backgroundColor; // 添加默认背景色

    int lastPos = 0;

    // 更完整的ANSI转义序列正则表达式
    QRegularExpression ansiRegex(
        // ESC序列的开始
        "\\x1B"
        // 后面跟着的可能模式
        "("
        // CSI序列 (Control Sequence Introducer) - ESC [
        "\\["
        // 参数部分 - 可选的数字序列，用分号分隔
        "(?:"
        "[\\d;:=?]+"
        ")?"
        // 中间字符 - 可选
        "(?:"
        "[ !\"#$%&'()*+,\\-./]+"
        ")?"
        // 终止字符 - 一个在 @ 到 ~ 范围内的字符
        "[@A-Za-z`-~]"
        "|"
        // OSC序列 (Operating System Command) - ESC ]
        "\\]"
        // 参数和字符串内容
        "(?:"
        "\\d+;.*?"
        ")"
        // OSC终止 (BEL或ESC\)
        "(?:"
        "\\x07|\\x1B\\\\"
        ")"
        "|"
        // 字符集选择和其他简单序列
        "\\([0-9A-Za-z]"  // 字符集选择
        "|"
        // 单字符序列
        "[A-Za-z<=>]"
        "|"
        // 可能的其他控制序列
        "\\]\\d+;.*?(?:\\x07|\\x1B\\\\)"  // 通用OSC
        "|"
        // 任何其他以ESC开始的序列
        ".[\\x20-\\x7E]*"
        ")"
        );

    QRegularExpressionMatchIterator it = ansiRegex.globalMatch(text);

    while (it.hasNext()) {
        QRegularExpressionMatch match = it.next();
        int start = match.capturedStart();
        int end = match.capturedEnd();
        QString captured = match.captured(0);

        // 输出匹配到的ANSI之前的普通文本
        QString plainText = text.mid(lastPos, start - lastPos);
        if (!plainText.isEmpty()) {
            cursor.insertText(plainText, currentFormat);
        }

        // 处理特殊的ANSI序列
        if (captured.startsWith("\x1B[?")) {
            // 处理私有模式设置，如 \x1B[?25l 隐藏光标
            lastPos = end;
            continue;
        } else if (captured == "\x1B=" || captured == "\x1B>") {
            // 处理应用键盘模式
            lastPos = end;
            continue;
        } else if (captured == "\x1B[H") {
            // 光标移动到Home位置
            // 在简单实现中可以等同于清屏
            terminalOutput->clear();
            cursor = terminalOutput->textCursor();
            lastPos = end;
            continue;
        } else if (captured == "\x1B[2J") {
            // 清屏
            terminalOutput->clear();
            cursor = terminalOutput->textCursor();
            lastPos = end;
            continue;
        } else if (captured == "\x1B[K") {
            // 清除从光标到行尾的内容
            // 在简化实现中我们忽略这个
            lastPos = end;
            continue;
        } else if (captured.startsWith("\x1B[") && captured.endsWith("m")) {
            // 处理SGR (Select Graphic Rendition) 参数
            QString paramPart = captured.mid(2, captured.length() - 3);
            QStringList codes = paramPart.isEmpty() ? QStringList("0") : paramPart.split(";");

            for (const QString &code : codes) {
                bool ok;
                int value = code.toInt(&ok);
                if (!ok && code.isEmpty()) {
                    value = 0;  // \x1B[m 等同于 \x1B[0m
                }

                switch (value) {
                    case 0: // 重置所有属性
                        currentFormat = defaultFormat;
                        textColor = this->textColor;
                        backgroundColor = this->backgroundColor;
                        currentFormat.setForeground(textColor);
                        currentFormat.setBackground(backgroundColor);
                        currentFormat.setFontWeight(QFont::Normal);
                        currentFormat.setFontItalic(false);
                        currentFormat.setFontUnderline(false);
                        break;
                    case 1: // 加粗
                        currentFormat.setFontWeight(QFont::Bold);
                        break;
                    case 3: // 斜体
                        currentFormat.setFontItalic(true);
                        break;
                    case 4: // 下划线
                        currentFormat.setFontUnderline(true);
                        break;
                    case 7: // 反显
                    {
                        QColor temp = textColor;
                        textColor = backgroundColor.isValid() ? backgroundColor : this->backgroundColor;
                        backgroundColor = temp;
                        currentFormat.setForeground(textColor);
                        currentFormat.setBackground(backgroundColor);
                        break;
                    }
                    case 22: // 取消加粗
                        currentFormat.setFontWeight(QFont::Normal);
                        break;
                    case 23: // 取消斜体
                        currentFormat.setFontItalic(false);
                        break;
                    case 24: // 取消下划线
                        currentFormat.setFontUnderline(false);
                        break;
                    case 27: // 取消反显
                    {
                        QColor temp = backgroundColor;
                        backgroundColor = textColor;
                        textColor = temp;
                        currentFormat.setForeground(textColor);
                        currentFormat.setBackground(backgroundColor);
                        break;
                    }
                    case 39: // 默认前景色
                        textColor = this->textColor;
                        currentFormat.setForeground(textColor);
                        break;
                    case 49: // 默认背景色
                        backgroundColor = this->backgroundColor;
                        currentFormat.setBackground(backgroundColor);
                        break;

                        // 标准前景色 (30-37)
                    case 30: case 31: case 32: case 33: case 34: case 35: case 36: case 37:
                        textColor = G_colorMappings[value - 30].col;
                        currentFormat.setForeground(textColor);
                        break;

                        // 标准背景色 (40-47)
                    case 40: case 41: case 42: case 43: case 44: case 45: case 46: case 47:
                        backgroundColor = G_colorMappings[value - 40].col;
                        currentFormat.setBackground(backgroundColor);
                        break;

                        // 亮色前景 (90-97)
                    case 90: case 91: case 92: case 93: case 94: case 95: case 96: case 97:
                        textColor = G_colorMappings[value - 90 + 8].col; // 亮色索引偏移8
                        currentFormat.setForeground(textColor);
                        break;

                        // 亮色背景 (100-107)
                    case 100: case 101: case 102: case 103: case 104: case 105: case 106: case 107:
                        backgroundColor = G_colorMappings[value - 100 + 8].col; // 亮色索引偏移8
                        currentFormat.setBackground(backgroundColor);
                        break;

                        // 8位颜色支持 (38;5;n 和 48;5;n)
                    case 38:
                        if (codes.size() > 2 && codes[1] == "5") {
                            int colorCode = codes[2].toInt(&ok);
                            if (ok && colorCode >= 0 && colorCode < 256) {
                                // 转换8位颜色到RGB
                                if (colorCode < 16) {
                                    // 使用基本16色
                                    textColor = G_colorMappings[colorCode % 16].col;
                                } else if (colorCode < 232) {
                                    // 使用6x6x6色彩立方体
                                    int r = (colorCode - 16) / 36 * 51;
                                    int g = ((colorCode - 16) % 36) / 6 * 51;
                                    int b = ((colorCode - 16) % 6) * 51;
                                    textColor = QColor(r, g, b);
                                } else {
                                    // 使用灰度色调
                                    int gray = (colorCode - 232) * 10 + 8;
                                    textColor = QColor(gray, gray, gray);
                                }
                                currentFormat.setForeground(textColor);
                            }
                        }
                        break;
                    case 48:
                        if (codes.size() > 2 && codes[1] == "5") {
                            int colorCode = codes[2].toInt(&ok);
                            if (ok && colorCode >= 0 && colorCode < 256) {
                                // 转换8位颜色到RGB
                                if (colorCode < 16) {
                                    // 使用基本16色
                                    backgroundColor = G_colorMappings[colorCode % 16].col;
                                } else if (colorCode < 232) {
                                    // 使用6x6x6色彩立方体
                                    int r = (colorCode - 16) / 36 * 51;
                                    int g = ((colorCode - 16) % 36) / 6 * 51;
                                    int b = ((colorCode - 16) % 6) * 51;
                                    backgroundColor = QColor(r, g, b);
                                } else {
                                    // 使用灰度色调
                                    int gray = (colorCode - 232) * 10 + 8;
                                    backgroundColor = QColor(gray, gray, gray);
                                }
                                currentFormat.setBackground(backgroundColor);
                            }
                        }
                        break;

                    default:
                        qDebug() << "Unhandled ANSI code:" << value;
                        break;
                }
            }
            lastPos = end;
        } else if (captured.startsWith("\x1B]0;") && captured.endsWith("\x07")) {
            // 设置终端标题
            QString title = captured.mid(4, captured.length() - 5);
            // 如果需要，这里可以发出信号更新窗口标题
            // emit titleChanged(title);
            lastPos = end;
        } else {
            // 其他未处理的ANSI序列
            qDebug() << "Unhandled ANSI sequence:" << captured;
            lastPos = end;
        }
    }

    // 处理剩余的普通文本
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
    // If we just completed a ZMODEM transfer, handle gracefully
    if (m_zmodemActive || m_zmodemUploadStarted) {
        resetZmodemState();
    }

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

    // Cancel any active ZMODEM transfer
    if (m_zmodemActive || m_zmodemUploadStarted) {
        if (m_zmodemFile.isOpen()) {
            m_zmodemFile.close();
        }
        m_zmodemTimer.stop();
        m_zmodemCancel = true;
        m_zmodemActive = false;
        m_zmodemUploadStarted = false;
        m_zmodemBuffer.clear();
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

bool TerminalWidget::detectZmodem(const QByteArray &data)
{
    // If already in ZMODEM mode, don't need to detect again
    if (m_zmodemActive || m_zmodemUploadStarted) {
        return true;
    }

    // Add data to buffer for detection
    m_zmodemBuffer.append(data);
    
    // Keep buffer at a reasonable size
    if (m_zmodemBuffer.size() > 1024) {
        m_zmodemBuffer = m_zmodemBuffer.right(1024);
    }
    
    // Convert to string for text-based detection
    QString bufferText = QString::fromUtf8(m_zmodemBuffer);
    
    // First check: exact match for "rz" command at the end of a prompt
    if (bufferText.contains(QRegularExpression("[$#>]\\s*rz\\s*[\\r\\n]"))) {
        qDebug() << "ZMODEM detected: 'rz' command found";
        return true;
    }
    
    // More specific check for the shell command pattern we're seeing
    if (bufferText.contains("[root@bigdata01 ~]# rz")) {
        qDebug() << "ZMODEM detected: specific shell 'rz' command found";
        return true;
    }
    
    // Second check: "rz waiting to receive" message
    if (bufferText.contains("rz waiting to receive")) {
        qDebug() << "ZMODEM detected: rz waiting pattern found";
        return true;
    }
    
    // Third check: Look for ZMODEM header sequence (more specific matching)
    // Look for: ZPAD + ZDLE + ZBIN
    QByteArray zmodemHeader;
    zmodemHeader.append(ZPAD);
    zmodemHeader.append(ZDLE);
    zmodemHeader.append(ZBIN);
    
    if (m_zmodemBuffer.contains(zmodemHeader)) {
        qDebug() << "ZMODEM detected: header sequence found";
        return true;
    }
    
    // No ZMODEM detected
    return false;
}

void TerminalWidget::handleZmodemDetected()
{
    // If we're already processing a detection, ignore duplicates
    if (m_zmodemProcessing) {
        qDebug() << "Already processing ZMODEM detection, ignoring duplicate";
        return;
    }
    
    // Set the processing flag
    m_zmodemProcessing = true;
    
    qDebug() << "ZMODEM protocol detected, initiating file transfer";
    
    // Show status message
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(Qt::blue);
    format.setFontWeight(QFont::Bold);
    
    cursor.insertText("\n\n", format);
    cursor.insertText("*** ZMODEM file transfer request detected ***\n", format);
    cursor.insertText("    Opening file selection dialog...\n", format);
    
    terminalOutput->setTextCursor(cursor);
    
    // Make sure UI is updated
    QApplication::processEvents();
    
    // Start ZMODEM file transfer
    QTimer::singleShot(100, this, [this]() {
        startZmodemUpload();
        m_zmodemProcessing = false;
    });
}

void TerminalWidget::startZmodemUpload()
{
    // Set the flag to indicate we're in the file upload process now
    m_zmodemUploadStarted = true;
    
    // Show file dialog to select a file
    QString fileName = QFileDialog::getOpenFileName(this, 
        tr("Select File for ZMODEM Upload"), QDir::homePath(), tr("All Files (*)"));
    
    if (fileName.isEmpty()) {
        // User cancelled the dialog
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.insertText("\nFile transfer cancelled. No file selected.\n", format);
        terminalOutput->setTextCursor(cursor);
        
        // Cancel ZMODEM transfer
        sendZmodemCancel();
        return;
    }
    
    // Initialize ZMODEM file transfer
    m_zmodemFilePath = fileName;
    m_zmodemFile.setFileName(fileName);
    
    if (!m_zmodemFile.open(QIODevice::ReadOnly)) {
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.insertText("\nFailed to open file: " + fileName + "\n", format);
        terminalOutput->setTextCursor(cursor);
        
        // Cancel ZMODEM transfer
        sendZmodemCancel();
        return;
    }
    
    // Set up file transfer parameters
    m_zmodemFileSize = m_zmodemFile.size();
    m_zmodemFilePos = 0;
    m_zmodemState = 0;
    m_zmodemErrorCount = 0;
    m_zmodemCancel = false;
    
    // Display transfer info
    QFileInfo fileInfo(fileName);
    QString baseName = fileInfo.fileName();
    
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    QTextCharFormat format;
    format.setForeground(Qt::blue);
    
    cursor.insertText("\n", format);
    cursor.insertText("=== ZMODEM File Transfer ===\n", format);
    cursor.insertText("File: " + baseName + "\n", format);
    cursor.insertText("Size: " + QString::number(m_zmodemFileSize) + " bytes\n", format);
    cursor.insertText("Status: Starting transfer...\n\n", format);
    terminalOutput->setTextCursor(cursor);
    
    // Start ZMODEM transfer - if it fails, handle the error
    if (!startZmodemFileTransfer()) {
        // Handle initialization failure
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat errorFormat;
        errorFormat.setForeground(Qt::red);
        cursor.insertText("Failed to initialize ZMODEM transfer.\n", errorFormat);
        terminalOutput->setTextCursor(cursor);
        
        // Close file and cleanup
        m_zmodemFile.close();
        m_zmodemUploadStarted = false;
        
        // Cancel ZMODEM transfer
        sendZmodemCancel();
    }
}

bool TerminalWidget::startZmodemFileTransfer()
{
    if (!m_connected || !m_zmodemUploadStarted) {
        return false;
    }
    
    // Get SSHClient
    SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
    if (!sshClient || !sshClient->isConnected()) {
        return false;
    }
    
    // Initialize file position
    m_zmodemFilePos = 0;
    
    // Use smaller packet size for better reliability
    m_zmodemPacketSize = 512;
    
    // Start timeout timer
    m_zmodemTimer.start(10000); // 10 second timeout
    
    // Wait for the server to be ready
    QThread::msleep(500);
    
    // Start the transfer process by sending the first packet
    QTimer::singleShot(100, this, &TerminalWidget::uploadNextZmodemPacket);
    
    return true;
}

void TerminalWidget::uploadNextZmodemPacket()
{
    if (!m_connected || !m_zmodemUploadStarted || m_zmodemCancel) {
        return;
    }
    
    // Get SSHClient
    SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
    if (!sshClient || !sshClient->isConnected()) {
        completeZmodemTransfer(false);
        return;
    }
    
    qDebug() << "ZMODEM: Uploading packet, file pos:" << m_zmodemFilePos << "of" << m_zmodemFileSize;
    
    // If starting transfer, send ZFILE header
    if (m_zmodemFilePos == 0) {
        // File information header
        QFileInfo fileInfo(m_zmodemFilePath);
        QString fileName = fileInfo.fileName();
        QByteArray fileInfoData = fileName.toUtf8() + '\0' + 
                                  QByteArray::number(m_zmodemFileSize) + ' ' + 
                                  QByteArray::number(fileInfo.lastModified().toSecsSinceEpoch()) + ' ' +
                                  "0 0 0";
        
        qDebug() << "ZMODEM: Sending ZFILE header with file info:" << fileInfoData;
        
        // Send ZFILE header
        QByteArray header = createZmodemHeader(ZFILE);
        QByteArray data = escapeZmodemData(fileInfoData);
        
        // Create a proper frame end
        QByteArray frameEnd;
        frameEnd.append(ZDLE);
        frameEnd.append(ZCRCW);
        
        // Calculate CRC for data + frame end type
        QByteArray dataForCrc = fileInfoData;
        dataForCrc.append(ZCRCW);
        quint16 crc = calculateCRC16(dataForCrc);
        
        // Create escaped CRC
        QByteArray crcBytes;
        crcBytes.append((crc >> 8) & 0xFF);
        crcBytes.append(crc & 0xFF);
        QByteArray escapedCrc = escapeZmodemData(crcBytes);
        
        // Send header + data + frame end + CRC
        sshClient->sendData(header + data + frameEnd + escapedCrc);
        
        // Reset timer
        m_zmodemTimer.start(10000);
        
        // Move to the next step immediately - start sending file data
        // Set file position to 1 to force moving to data transfer
        m_zmodemFilePos = 1;
        
        // Schedule sending the first data packet after a short delay
        QTimer::singleShot(500, this, &TerminalWidget::uploadNextZmodemPacket);
        return;
    }
    
    // Read data from file
    if (!m_zmodemFile.isOpen()) {
        completeZmodemTransfer(false);
        return;
    }
    
    // Check if we've sent the whole file
    if (m_zmodemFilePos >= m_zmodemFileSize) {
        // Send ZEOF header to indicate end of file
        QByteArray header = createZmodemHeader(ZEOF, m_zmodemFileSize);
        sshClient->sendData(header);
        
        qDebug() << "ZMODEM: Sending ZEOF, file complete";
        
        // Reset timer
        m_zmodemTimer.start(10000);
        
        // Wait a moment before proceeding to completion to give the server time to process
        QTimer::singleShot(1000, [this]() {
            completeZmodemTransfer(true);
        });
        
        return;
    }
    
    // Reset position to file start for the first data packet
    if (m_zmodemFilePos == 1) {
        m_zmodemFilePos = 0;
    }
    
    // Read next chunk of data - use smaller chunks for better reliability
    int chunkSize = qMin(m_zmodemPacketSize, 1024);
    m_zmodemFile.seek(m_zmodemFilePos);
    QByteArray chunk = m_zmodemFile.read(chunkSize);
    
    if (chunk.isEmpty()) {
        completeZmodemTransfer(false);
        return;
    }
    
    qDebug() << "ZMODEM: Sending data packet, size:" << chunk.size() << 
                "pos:" << m_zmodemFilePos << "of" << m_zmodemFileSize;
    
    // Send ZDATA header first
    QByteArray dataHeader = createZmodemHeader(ZDATA, m_zmodemFilePos);
    sshClient->sendData(dataHeader);
    
    // Give the server a moment to process the header
    QThread::msleep(20);
    
    // Create and send data packet
    QByteArray escapedData = escapeZmodemData(chunk);
    
    // Determine frame end type (ZCRCW for last packet, ZCRCG for others)
    // Use ZCRCW more frequently to get more acknowledgements
    bool isLastChunk = (m_zmodemFilePos + chunk.size() >= m_zmodemFileSize);
    bool useAckFrame = isLastChunk || (m_zmodemFilePos % (chunkSize * 10) < chunkSize);
    char frameEndType = useAckFrame ? ZCRCW : ZCRCG;
    
    // Calculate CRC for data + frame end type
    QByteArray dataForCrc = chunk;
    dataForCrc.append(frameEndType);
    quint16 crc = calculateCRC16(dataForCrc);
    
    // Frame end
    QByteArray frameEnd;
    frameEnd.append(ZDLE);
    frameEnd.append(frameEndType);
    
    // Create escaped CRC
    QByteArray crcBytes;
    crcBytes.append((crc >> 8) & 0xFF);
    crcBytes.append(crc & 0xFF);
    QByteArray escapedCrc = escapeZmodemData(crcBytes);
    
    // Send data + frame end + CRC
    sshClient->sendData(escapedData + frameEnd + escapedCrc);
    
    // Update position
    m_zmodemFilePos += chunk.size();
    
    // Update progress
    updateZmodemProgress(m_zmodemFilePos, m_zmodemFileSize);
    
    // Reset timer
    m_zmodemTimer.start(10000);
    
    // Schedule next packet with appropriate delay - use variable delay based on packet size
    int delay = useAckFrame ? 300 : 200;
    QTimer::singleShot(delay, this, &TerminalWidget::uploadNextZmodemPacket);
}

QByteArray TerminalWidget::createZmodemHeader(int frameType, quint32 pos)
{
    QByteArray header;
    
    // ZPAD + ZDLE + frameType
    header.append(ZPAD);
    header.append(ZDLE);
    header.append(ZBIN);
    
    // Frame type
    header.append(frameType);
    
    // Position/flags (4 bytes)
    header.append((pos >> 0) & 0xFF);
    header.append((pos >> 8) & 0xFF);
    header.append((pos >> 16) & 0xFF);
    header.append((pos >> 24) & 0xFF);
    
    // Calculate CRC
    QByteArray dataForCrc;
    dataForCrc.append(frameType);
    dataForCrc.append((pos >> 0) & 0xFF);
    dataForCrc.append((pos >> 8) & 0xFF);
    dataForCrc.append((pos >> 16) & 0xFF);
    dataForCrc.append((pos >> 24) & 0xFF);
    
    quint16 crc = calculateCRC16(dataForCrc);
    
    header.append((crc >> 8) & 0xFF);
    header.append(crc & 0xFF);
    
    return header;
}

QByteArray TerminalWidget::escapeZmodemData(const QByteArray &data)
{
    QByteArray escaped;
    
    for (char c : data) {
        if (c == ZDLE) {
            escaped.append(ZDLE);
            escaped.append(ZDLEE);
        } else if ((c & 0x60) == 0) {
            // Control characters need escaping
            escaped.append(ZDLE);
            escaped.append(c ^ 0x40);
        } else {
            escaped.append(c);
        }
    }
    
    return escaped;
}

QByteArray TerminalWidget::createZmodemDataPacket(const QByteArray &data, bool last)
{
    QByteArray packet;
    
    // Start with ZDATA header
    packet.append(createZmodemHeader(ZDATA, m_zmodemFilePos));
    
    // Add escaped data
    packet.append(escapeZmodemData(data));
    
    // Add frame end marker
    packet.append(ZDLE);
    
    // Either ZCRCW (wait for ACK) or ZCRCG (no wait)
    packet.append(last ? ZCRCW : ZCRCG);
    
    return packet;
}

quint16 TerminalWidget::calculateCRC16(const QByteArray &data)
{
    static const quint16 crc16Table[256] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50a5, 0x60c6, 0x70e7,
        0x8108, 0x9129, 0xa14a, 0xb16b, 0xc18c, 0xd1ad, 0xe1ce, 0xf1ef,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52b5, 0x4294, 0x72f7, 0x62d6,
        0x9339, 0x8318, 0xb37b, 0xa35a, 0xd3bd, 0xc39c, 0xf3ff, 0xe3de,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64e6, 0x74c7, 0x44a4, 0x5485,
        0xa56a, 0xb54b, 0x8528, 0x9509, 0xe5ee, 0xf5cf, 0xc5ac, 0xd58d,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76d7, 0x66f6, 0x5695, 0x46b4,
        0xb75b, 0xa77a, 0x9719, 0x8738, 0xf7df, 0xe7fe, 0xd79d, 0xc7bc,
        0x48c4, 0x58e5, 0x6886, 0x78a7, 0x0840, 0x1861, 0x2802, 0x3823,
        0xc9cc, 0xd9ed, 0xe98e, 0xf9af, 0x8948, 0x9969, 0xa90a, 0xb92b,
        0x5af5, 0x4ad4, 0x7ab7, 0x6a96, 0x1a71, 0x0a50, 0x3a33, 0x2a12,
        0xdbfd, 0xcbdc, 0xfbbf, 0xeb9e, 0x9b79, 0x8b58, 0xbb3b, 0xab1a,
        0x6ca6, 0x7c87, 0x4ce4, 0x5cc5, 0x2c22, 0x3c03, 0x0c60, 0x1c41,
        0xedae, 0xfd8f, 0xcdec, 0xddcd, 0xad2a, 0xbd0b, 0x8d68, 0x9d49,
        0x7e97, 0x6eb6, 0x5ed5, 0x4ef4, 0x3e13, 0x2e32, 0x1e51, 0x0e70,
        0xff9f, 0xefbe, 0xdfdd, 0xcffc, 0xbf1b, 0xaf3a, 0x9f59, 0x8f78,
        0x9188, 0x81a9, 0xb1ca, 0xa1eb, 0xd10c, 0xc12d, 0xf14e, 0xe16f,
        0x1080, 0x00a1, 0x30c2, 0x20e3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83b9, 0x9398, 0xa3fb, 0xb3da, 0xc33d, 0xd31c, 0xe37f, 0xf35e,
        0x02b1, 0x1290, 0x22f3, 0x32d2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xb5ea, 0xa5cb, 0x95a8, 0x8589, 0xf56e, 0xe54f, 0xd52c, 0xc50d,
        0x34e2, 0x24c3, 0x14a0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xa7db, 0xb7fa, 0x8799, 0x97b8, 0xe75f, 0xf77e, 0xc71d, 0xd73c,
        0x26d3, 0x36f2, 0x0691, 0x16b0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xd94c, 0xc96d, 0xf90e, 0xe92f, 0x99c8, 0x89e9, 0xb98a, 0xa9ab,
        0x5844, 0x4865, 0x7806, 0x6827, 0x18c0, 0x08e1, 0x3882, 0x28a3,
        0xcb7d, 0xdb5c, 0xeb3f, 0xfb1e, 0x8bf9, 0x9bd8, 0xabbb, 0xbb9a,
        0x4a75, 0x5a54, 0x6a37, 0x7a16, 0x0af1, 0x1ad0, 0x2ab3, 0x3a92,
        0xfd2e, 0xed0f, 0xdd6c, 0xcd4d, 0xbdaa, 0xad8b, 0x9de8, 0x8dc9,
        0x7c26, 0x6c07, 0x5c64, 0x4c45, 0x3ca2, 0x2c83, 0x1ce0, 0x0cc1,
        0xef1f, 0xff3e, 0xcf5d, 0xdf7c, 0xaf9b, 0xbfba, 0x8fd9, 0x9ff8,
        0x6e17, 0x7e36, 0x4e55, 0x5e74, 0x2e93, 0x3eb2, 0x0ed1, 0x1ef0
    };

    quint16 crc = 0;
    for (int i = 0; i < data.size(); i++) {
        crc = (crc << 8) ^ crc16Table[((crc >> 8) ^ (unsigned char)data.at(i)) & 0xFF];
    }
    
    return crc;
}

void TerminalWidget::sendZmodemCancel()
{
    // Set cancel flag to prevent further processing
    m_zmodemCancel = true;
    
    // Get SSHClient
    SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
    if (!sshClient || !sshClient->isConnected()) {
        // Even if we can't send the cancel sequence, reset state
        resetZmodemState();
        return;
    }
    
    qDebug() << "ZMODEM: Sending cancellation sequence";
    
    // Send cancel sequence (5 CAN characters is usually enough)
    QByteArray cancelSequence;
    for (int i = 0; i < 5; i++) {
        cancelSequence.append('\x18');
    }
    
    // Send the cancel sequence with a small delay between parts to avoid buffer overflow
    for (int i = 0; i < cancelSequence.size(); i++) {
        sshClient->sendData(QByteArray(1, cancelSequence.at(i)));
        QThread::msleep(10);
    }
    
    // Wait a moment before resetting state
    QThread::msleep(300);
    
    // Reset all ZMODEM state
    resetZmodemState();
    
    // Display cancellation message
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    QTextCharFormat format;
    format.setForeground(Qt::red);
    cursor.insertText("\nZMODEM transfer cancelled.\n", format);
    terminalOutput->setTextCursor(cursor);
    
    // Send a newline to restore the prompt, but do it after a short delay
    QTimer::singleShot(1000, [sshClient]() {
        if (sshClient && sshClient->isConnected()) {
            sshClient->sendData(QByteArray(1, '\n'));
        }
    });
}

void TerminalWidget::resetZmodemState()
{
    // Reset all ZMODEM related flags and state
    m_zmodemActive = false;
    m_zmodemUploadStarted = false;
    m_zmodemBuffer.clear();
    m_zmodemCancel = false;
    m_zmodemProcessing = false;
    
    // Close file if open
    if (m_zmodemFile.isOpen()) {
        m_zmodemFile.close();
    }
    
    // Stop timer
    m_zmodemTimer.stop();
}

void TerminalWidget::zmodemTransferTimeout()
{
    m_zmodemErrorCount++;
    
    if (m_zmodemErrorCount >= 3) {
        // Too many timeouts, cancel transfer
        sendZmodemCancel();
        
        QTextCursor cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.insertText("\nZMODEM transfer timed out.\n", format);
        terminalOutput->setTextCursor(cursor);
    } else {
        // Try to continue transfer
        uploadNextZmodemPacket();
    }
}

void TerminalWidget::completeZmodemTransfer(bool success)
{
    // Stop timer
    m_zmodemTimer.stop();
    
    // Close file
    if (m_zmodemFile.isOpen()) {
        m_zmodemFile.close();
    }
    
    // Display completion message
    QTextCursor cursor = terminalOutput->textCursor();
    cursor.movePosition(QTextCursor::End);
    
    if (success) {
        QTextCharFormat format;
        format.setForeground(Qt::green);
        cursor.insertText("\nZMODEM file transfer completed successfully!\n", format);
    } else {
        QTextCharFormat format;
        format.setForeground(Qt::red);
        cursor.insertText("\nZMODEM file transfer failed.\n", format);
    }
    
    terminalOutput->setTextCursor(cursor);
    
    // Get SSHClient
    SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
    
    if (sshClient && sshClient->isConnected() && success) {
        // Send ZMODEM termination sequence
        QByteArray termSequence;
        
        // Send the Over-and-Out (OO) sequence that properly terminates ZMODEM
        QByteArray oo = QByteArray("OO");
        
        // First send a proper ZFIN header to indicate we're done
        QByteArray zfin = createZmodemHeader(ZFIN);
        sshClient->sendData(zfin);
        
        qDebug() << "ZMODEM: Sent ZFIN to terminate transfer";
        
        // Then send the Over-and-Out bytes (can be just the raw 'O' characters)
        QThread::msleep(500);  // Wait for server to process ZFIN
        sshClient->sendData(oo);
        
        qDebug() << "ZMODEM: Sent Over-and-Out (OO) sequence";
        
        // Send additional ZMODEM cancellation to ensure clean exit
        // but in a gentler way than a full cancel
        QThread::msleep(500);
        QByteArray gentleCancel;
        gentleCancel.append('\x18');  // Just one CAN character
        gentleCancel.append('\x18');  // One more for good measure
        sshClient->sendData(gentleCancel);
        
        cursor = terminalOutput->textCursor();
        cursor.movePosition(QTextCursor::End);
        QTextCharFormat infoFormat;
        infoFormat.setForeground(Qt::blue);
        cursor.insertText("\nSending termination sequence to end ZMODEM session...\n", infoFormat);
        terminalOutput->setTextCursor(cursor);
    }
    
    // Reset ZMODEM state
    resetZmodemState();
    
    // After a reasonable delay, try to refresh the connection with a newline
    // This will help get back to normal shell prompt
    QTimer::singleShot(2000, [this, sshClient]() {
        if (sshClient && sshClient->isConnected()) {
            // Send Ctrl+C followed by newline to interrupt any remaining rz process
            sshClient->sendData(QByteArray(1, 3));  // Ctrl+C
            QThread::msleep(100);
            sshClient->sendData(QByteArray(1, '\n'));
            
            QTextCursor cursor = terminalOutput->textCursor();
            cursor.movePosition(QTextCursor::End);
            QTextCharFormat infoFormat;
            infoFormat.setForeground(Qt::blue);
            cursor.insertText("\nZMODEM session terminated, returning to shell.\n", infoFormat);
            terminalOutput->setTextCursor(cursor);
        } else if (!m_connected) {
            QTextCursor cursor = terminalOutput->textCursor();
            cursor.movePosition(QTextCursor::End);
            QTextCharFormat format;
            format.setForeground(Qt::blue);
            cursor.insertText("\nConnection lost after transfer. You may need to reconnect.\n", format);
            terminalOutput->setTextCursor(cursor);
        }
    });
}

void TerminalWidget::updateZmodemProgress(qint64 sent, qint64 total)
{
    // Calculate percentage
    int percentage = (total > 0) ? static_cast<int>((sent * 100) / total) : 0;
    
    // Create progress bar text
    QTextCursor cursor = terminalOutput->textCursor();
    
    // Find the last line with "Progress:"
    QString currentText = terminalOutput->toPlainText();
    int progressPos = currentText.lastIndexOf("Progress: [");
    
    if (progressPos >= 0) {
        // Replace existing progress bar
        cursor.setPosition(progressPos);
        cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
        
        // Create progress bar
        const int totalSteps = 20;
        int completedSteps = (totalSteps * percentage) / 100;
        
        QString progressBar = "Progress: [";
        for (int i = 0; i < totalSteps; i++) {
            progressBar += (i < completedSteps) ? "■" : "□";
        }
        progressBar += QString("] %1%").arg(percentage);
        
        // Set progress bar text
        QTextCharFormat format;
        format.setForeground(Qt::blue);
        cursor.insertText(progressBar, format);
    } else {
        // Create new progress bar
        cursor.movePosition(QTextCursor::End);
        
        const int totalSteps = 20;
        int completedSteps = (totalSteps * percentage) / 100;
        
        QString progressBar = "Progress: [";
        for (int i = 0; i < totalSteps; i++) {
            progressBar += (i < completedSteps) ? "■" : "□";
        }
        progressBar += QString("] %1%").arg(percentage);
        
        // Set progress bar text
        QTextCharFormat format;
        format.setForeground(Qt::blue);
        cursor.insertText(progressBar + "\n", format);
    }
    
    terminalOutput->setTextCursor(cursor);
    
    // Process events to update UI
    QApplication::processEvents();
}

void TerminalWidget::processZmodemResponse()
{
    // Process any pending ZMODEM responses from the server
    if (m_zmodemBuffer.isEmpty()) {
        return;
    }
    
    qDebug() << "Processing ZMODEM response, buffer size:" << m_zmodemBuffer.size();
    
    // Dump buffer content in hex for debugging (limit to first 50 bytes)
    QString hexDump;
    for (int i = 0; i < qMin(m_zmodemBuffer.size(), 50); i++) {
        hexDump += QString("%1 ").arg((unsigned char)m_zmodemBuffer.at(i), 2, 16, QChar('0'));
    }
    qDebug() << "Buffer Hex:" << hexDump;
    
    // Check for heartbeat/status messages that indicate the server is still in ZMODEM mode
    bool hasHeartbeat = false;
    // Look for typical ZMODEM heartbeat pattern (e.g., **B0...)
    if (m_zmodemBuffer.contains("**B0")) {
        hasHeartbeat = true;
        qDebug() << "ZMODEM: Detected heartbeat pattern from server";
    }
    
    // Look for cancellation sequences in the data
    bool hasCancel = false;
    for (int i = 0; i < m_zmodemBuffer.size() - 3; i++) {
        // Look for multiple CAN characters in a row
        if ((unsigned char)m_zmodemBuffer.at(i) == 0x18 && 
            (unsigned char)m_zmodemBuffer.at(i+1) == 0x18 && 
            (unsigned char)m_zmodemBuffer.at(i+2) == 0x18) {
            hasCancel = true;
            break;
        }
    }
    
    // If we see a cancel from server side, handle it
    if (hasCancel) {
        qDebug() << "ZMODEM: Detected cancellation from server";
        QTimer::singleShot(500, this, [this]() {
            completeZmodemTransfer(false);
        });
    }
    // If we're getting heartbeats and transfer is complete, send termination sequence
    else if (hasHeartbeat && !m_zmodemFile.isOpen() && m_zmodemUploadStarted) {
        // Get SSHClient
        SSHClient *sshClient = m_connectionThread ? m_connectionThread->getSSHClient() : nullptr;
        
        if (sshClient && sshClient->isConnected()) {
            qDebug() << "ZMODEM: Sending additional termination sequence in response to heartbeat";
            
            // Send Ctrl+C to interrupt rz command
            sshClient->sendData(QByteArray(1, 3));
            QThread::msleep(100);
            
            // Send newline
            sshClient->sendData(QByteArray(1, '\n'));
            
            QTextCursor cursor = terminalOutput->textCursor();
            cursor.movePosition(QTextCursor::End);
            cursor.insertText("\n");
            terminalOutput->setTextCursor(cursor);
        }
    }
    
    // Clear buffer to prevent reprocessing
    m_zmodemBuffer.clear();
    
    // Update progress UI
    updateZmodemProgress(m_zmodemFilePos, m_zmodemFileSize);
}
