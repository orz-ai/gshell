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

TerminalWidget::TerminalWidget(QWidget *parent) : QWidget(parent),
    m_connected(false), m_connectionThread(nullptr),
    historyPosition(-1), m_ansiEscapeMode(false), m_bold(false)
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

    // 1. 尝试精确匹配提示符
    if (!commandExtracted) {
        if (currentLine.startsWith(m_currentPrompt + " ")) {
            command = currentLine.mid(m_currentPrompt.length() + 1);
            commandExtracted = true;
            qDebug() << "方法1: 精确匹配提示符+空格";
        } else if (currentLine.startsWith(m_currentPrompt)) {
            command = currentLine.mid(m_currentPrompt.length());
            commandExtracted = true;
            qDebug() << "方法1: 精确匹配提示符";
        }
    }

    // 2. 尝试在行中查找提示符
    if (!commandExtracted) {
        int promptIndex = currentLine.indexOf(m_currentPrompt);
        if (promptIndex >= 0) {
            int commandStart = promptIndex + m_currentPrompt.length();
            if (commandStart < currentLine.length() && currentLine.at(commandStart) == ' ') {
                commandStart++;
            }
            command = currentLine.mid(commandStart);
            commandExtracted = true;
            qDebug() << "方法2: 在行中查找提示符";
        }
    }

    // 3. 尝试使用正则表达式匹配常见的提示符格式
    if (!commandExtracted) {
        QRegExp promptRegex("\\[[^\\]]+@[^\\]]+\\s+[^\\]]+\\][$#]\\s*");
        if (promptRegex.indexIn(currentLine) >= 0) {
            QString detectedPrompt = promptRegex.cap(0);
            int promptIndex = currentLine.indexOf(detectedPrompt);
            int commandStart = promptIndex + detectedPrompt.length();
            command = currentLine.mid(commandStart);

            // 更新当前提示符
            m_currentPrompt = detectedPrompt.trimmed();
            commandExtracted = true;
            qDebug() << "方法3: 正则表达式匹配提示符";
        }
    }

    // 4. 如果所有尝试都失败，假设整行是命令
    if (!commandExtracted) {
        command = currentLine.trimmed();
        qDebug() << "方法4: 假设整行是命令";
    }

    qDebug() << "提取的命令:" << command;

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
    // 处理接收到的数据
    QString text = QString::fromUtf8(data);

    // 过滤掉可能导致显示问题的控制序列
    // 过滤终端标题序列
    int start = text.indexOf("\033]0;");
    while (start != -1) {
        int end = text.indexOf("\007", start);
        if (end != -1) {
            text.remove(start, end - start + 1);
        } else {
            break;
        }
        start = text.indexOf("\033]0;", start);
    }

    // 添加到终端
    appendToTerminal(text);

    // 检测提示符
    QRegExp promptRegex("\\[[^\\]]+@[^\\]]+\\s+[^\\]]+\\][$#]\\s*$");
    if (promptRegex.indexIn(text) >= 0) {
        QString detectedPrompt = promptRegex.cap(0).trimmed();
        if (!detectedPrompt.isEmpty()) {
            m_currentPrompt = detectedPrompt;
            qDebug() << "检测到新提示符:" << m_currentPrompt;
        }
    }
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

void TerminalWidget::appendToTerminal(const QString &text)
{
    QTextCursor cursor = terminalOutput->textCursor();
    
    // Store current position
    int currentPosition = cursor.position();
    
    // Process ANSI escape sequences if needed
    QString processedText = m_ansiEscapeMode ? processAnsiEscapeSequences(text) : text;
    
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

QString TerminalWidget::processAnsiEscapeSequences(const QString &text)
{
    QString result = text;
    int escapeStart = 0;
    
    // Find escape sequences
    while ((escapeStart = result.indexOf("\033[", escapeStart)) != -1) {
        int escapeEnd = escapeStart + 2;  // Skip "\033["
        
        // Find the end of the escape sequence (a letter)
        while (escapeEnd < result.length() && 
               !result[escapeEnd].isLetter()) {
            escapeEnd++;
        }
        
        if (escapeEnd < result.length()) {
            // We found a complete escape sequence
            QString escapeSequence = result.mid(escapeStart, escapeEnd - escapeStart + 1);
            QStringList parameters = escapeSequence.mid(2, escapeSequence.length() - 3).split(';');
            
            // Process the escape sequence based on the terminating character
            QChar command = result[escapeEnd];
            
            if (command == 'm') {  // SGR - Select Graphic Rendition
                // Handle color and text formatting
                if (parameters.isEmpty() || parameters[0] == "0") {
                    // Reset attributes
                    m_currentFgColor = textColor;
                    m_currentBgColor = backgroundColor;
                    m_bold = false;
                } else {
                    for (const QString &param : parameters) {
                        int code = param.toInt();
                        
                        if (code == 1) {
                            // Bold
                            m_bold = true;
                        } else if (code == 22) {
                            // Not bold
                            m_bold = false;
                        } else if (code >= 30 && code <= 37) {
                            // Foreground color
                            m_currentFgColor = m_ansiColors[code - 30];
                        } else if (code >= 40 && code <= 47) {
                            // Background color
                            m_currentBgColor = m_ansiColors[code - 40];
                        } else if (code >= 90 && code <= 97) {
                            // Bright foreground color
                            m_currentFgColor = m_ansiColors[code - 90 + 8];
                        } else if (code >= 100 && code <= 107) {
                            // Bright background color
                            m_currentBgColor = m_ansiColors[code - 100 + 8];
                        }
                    }
                }
            }
            
            // Remove the escape sequence from the result
            result.remove(escapeStart, escapeEnd - escapeStart + 1);
        } else {
            // Incomplete escape sequence, skip it
            escapeStart += 2;
        }
    }
    
    return result;
}
