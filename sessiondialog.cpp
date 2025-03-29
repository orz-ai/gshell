#include "sessiondialog.h"
#include <QVBoxLayout>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFontDialog>
#include <QColorDialog>
#include <QGroupBox>

SessionDialog::SessionDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Session Information"));
    resize(400, 300);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    
    tabWidget = new QTabWidget(this);
    mainLayout->addWidget(tabWidget);
    
    setupConnectionTab();
    setupTerminalTab();
    setupAppearanceTab();
    
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    QPushButton *okButton = new QPushButton(tr("OK"), this);
    QPushButton *cancelButton = new QPushButton(tr("Cancel"), this);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    connect(okButton, &QPushButton::clicked, this, &QDialog::accept);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    connect(authTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SessionDialog::onAuthTypeChanged);
    connect(browseButton, &QPushButton::clicked, this, &SessionDialog::browseKeyFile);
    connect(fontButton, &QPushButton::clicked, this, &SessionDialog::selectFont);
    connect(bgColorButton, &QPushButton::clicked, this, &SessionDialog::selectBackgroundColor);
    connect(textColorButton, &QPushButton::clicked, this, &SessionDialog::selectTextColor);
    
    // Set default values
    selectedFont = QFont("Consolas", 10);
    selectedBgColor = QColor("#1E1E1E");
    selectedTextColor = QColor("#DCDCDC");
    updateFontDisplay();
    updateColorDisplays();
}

SessionDialog::~SessionDialog()
{
    // 析构函数实现
}

void SessionDialog::setupConnectionTab()
{
    QWidget *connectionTab = new QWidget(tabWidget);
    QFormLayout *formLayout = new QFormLayout(connectionTab);
    
    nameEdit = new QLineEdit(tr("New Session"), connectionTab);
    formLayout->addRow(tr("Name:"), nameEdit);
    
    hostEdit = new QLineEdit(connectionTab);
    formLayout->addRow(tr("Host:"), hostEdit);
    
    portEdit = new QSpinBox(connectionTab);
    portEdit->setRange(1, 65535);
    portEdit->setValue(22);
    formLayout->addRow(tr("Port:"), portEdit);
    
    usernameEdit = new QLineEdit(connectionTab);
    formLayout->addRow(tr("Username:"), usernameEdit);
    
    passwordEdit = new QLineEdit(connectionTab);
    passwordEdit->setEchoMode(QLineEdit::Password);
    formLayout->addRow(tr("Password:"), passwordEdit);
    
    authTypeCombo = new QComboBox(connectionTab);
    authTypeCombo->addItem(tr("Password"));
    authTypeCombo->addItem(tr("Key"));
    formLayout->addRow(tr("Auth Type:"), authTypeCombo);
    
    QHBoxLayout *keyFileLayout = new QHBoxLayout();
    keyFileEdit = new QLineEdit(connectionTab);
    keyFileEdit->setEnabled(false);
    browseButton = new QPushButton(tr("..."), connectionTab);
    browseButton->setEnabled(false);
    browseButton->setMaximumWidth(30);
    keyFileLayout->addWidget(keyFileEdit);
    keyFileLayout->addWidget(browseButton);
    formLayout->addRow(tr("Key File:"), keyFileLayout);
    
    connect(authTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SessionDialog::onAuthTypeChanged);
    connect(browseButton, &QPushButton::clicked, this, &SessionDialog::browseKeyFile);
    
    tabWidget->addTab(connectionTab, tr("Connection"));
}

void SessionDialog::setupTerminalTab()
{
    QWidget *terminalTab = new QWidget(tabWidget);
    QFormLayout *formLayout = new QFormLayout(terminalTab);
    
    // Terminal type
    terminalTypeCombo = new QComboBox(terminalTab);
    terminalTypeCombo->addItem("xterm");
    terminalTypeCombo->addItem("xterm-256color");
    terminalTypeCombo->addItem("vt100");
    formLayout->addRow(tr("Terminal Type:"), terminalTypeCombo);
    
    // Encoding
    encodingCombo = new QComboBox(terminalTab);
    encodingCombo->addItem("UTF-8");
    encodingCombo->addItem("ISO-8859-1");
    encodingCombo->addItem("Windows-1252");
    formLayout->addRow(tr("Encoding:"), encodingCombo);
    
    // Keep alive
    keepAliveCheck = new QCheckBox(tr("Enable keep alive"), terminalTab);
    keepAliveCheck->setChecked(true);
    formLayout->addRow("", keepAliveCheck);
    
    keepAliveIntervalSpin = new QSpinBox(terminalTab);
    keepAliveIntervalSpin->setRange(1, 300);
    keepAliveIntervalSpin->setValue(60);
    keepAliveIntervalSpin->setSuffix(tr(" seconds"));
    formLayout->addRow(tr("Keep alive interval:"), keepAliveIntervalSpin);
    
    tabWidget->addTab(terminalTab, tr("Terminal"));
}

void SessionDialog::setupAppearanceTab()
{
    QWidget *appearanceTab = new QWidget(tabWidget);
    QFormLayout *formLayout = new QFormLayout(appearanceTab);
    
    // Font selection
    QHBoxLayout *fontLayout = new QHBoxLayout();
    fontDisplay = new QLineEdit(appearanceTab);
    fontDisplay->setReadOnly(true);
    fontButton = new QPushButton(tr("Change..."), appearanceTab);
    fontLayout->addWidget(fontDisplay);
    fontLayout->addWidget(fontButton);
    formLayout->addRow(tr("Font:"), fontLayout);
    
    // Background color
    QHBoxLayout *bgColorLayout = new QHBoxLayout();
    bgColorDisplay = new QLineEdit(appearanceTab);
    bgColorDisplay->setReadOnly(true);
    bgColorButton = new QPushButton(tr("Change..."), appearanceTab);
    bgColorLayout->addWidget(bgColorDisplay);
    bgColorLayout->addWidget(bgColorButton);
    formLayout->addRow(tr("Background Color:"), bgColorLayout);
    
    // Text color
    QHBoxLayout *textColorLayout = new QHBoxLayout();
    textColorDisplay = new QLineEdit(appearanceTab);
    textColorDisplay->setReadOnly(true);
    textColorButton = new QPushButton(tr("Change..."), appearanceTab);
    textColorLayout->addWidget(textColorDisplay);
    textColorLayout->addWidget(textColorButton);
    formLayout->addRow(tr("Text Color:"), textColorLayout);
    
    connect(fontButton, &QPushButton::clicked, this, &SessionDialog::selectFont);
    connect(bgColorButton, &QPushButton::clicked, this, &SessionDialog::selectBackgroundColor);
    connect(textColorButton, &QPushButton::clicked, this, &SessionDialog::selectTextColor);
    
    tabWidget->addTab(appearanceTab, tr("Appearance"));
}

void SessionDialog::onAuthTypeChanged(int index)
{
    bool isKeyAuth = (index == 1);
    keyFileEdit->setEnabled(isKeyAuth);
    browseButton->setEnabled(isKeyAuth);
    passwordEdit->setEnabled(!isKeyAuth);
}

void SessionDialog::browseKeyFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, tr("Select Private Key File"), 
                                                   QDir::homePath(), tr("All Files (*)"));
    if (!fileName.isEmpty()) {
        keyFileEdit->setText(fileName);
    }
}

void SessionDialog::selectFont()
{
    bool ok;
    QFont font = QFontDialog::getFont(&ok, selectedFont, this, tr("Select Terminal Font"));
    if (ok) {
        selectedFont = font;
        updateFontDisplay();
    }
}

void SessionDialog::selectBackgroundColor()
{
    QColor color = QColorDialog::getColor(selectedBgColor, this, tr("Select Background Color"));
    if (color.isValid()) {
        selectedBgColor = color;
        updateColorDisplays();
    }
}

void SessionDialog::selectTextColor()
{
    QColor color = QColorDialog::getColor(selectedTextColor, this, tr("Select Text Color"));
    if (color.isValid()) {
        selectedTextColor = color;
        updateColorDisplays();
    }
}

void SessionDialog::updateFontDisplay()
{
    fontDisplay->setText(QString("%1, %2pt").arg(selectedFont.family()).arg(selectedFont.pointSize()));
    
    // Set the font of the display field to match the selected font
    QFont displayFont = selectedFont;
    displayFont.setPointSize(9); // Keep display font size reasonable
    fontDisplay->setFont(displayFont);
}

void SessionDialog::updateColorDisplays()
{
    // Set background color display
    QString bgColorStyle = QString("QLineEdit { background-color: %1; color: %2; }")
                          .arg(selectedBgColor.name(), selectedTextColor.name());
    bgColorDisplay->setStyleSheet(bgColorStyle);
    bgColorDisplay->setText(selectedBgColor.name());
    
    // Set text color display
    QString textColorStyle = QString("QLineEdit { background-color: %1; color: %2; }")
                            .arg(selectedBgColor.name(), selectedTextColor.name());
    textColorDisplay->setStyleSheet(textColorStyle);
    textColorDisplay->setText(selectedTextColor.name());
}

void SessionDialog::setSessionInfo(const SessionInfo &session)
{
    nameEdit->setText(session.name);
    hostEdit->setText(session.host);
    portEdit->setValue(session.port);
    usernameEdit->setText(session.username);
    passwordEdit->setText(session.password);
    
    if (session.authType == 0) {
        authTypeCombo->setCurrentIndex(0);
    } else {
        authTypeCombo->setCurrentIndex(1);
        keyFileEdit->setText(session.keyFile);
    }
    
    // Terminal settings
    int terminalTypeIndex = terminalTypeCombo->findText(session.terminalType);
    if (terminalTypeIndex >= 0) {
        terminalTypeCombo->setCurrentIndex(terminalTypeIndex);
    }
    
    int encodingIndex = encodingCombo->findText(session.encoding);
    if (encodingIndex >= 0) {
        encodingCombo->setCurrentIndex(encodingIndex);
    }
    
    keepAliveCheck->setChecked(session.keepAlive);
    keepAliveIntervalSpin->setValue(session.keepAliveInterval);
    
    // Appearance settings
    selectedFont = QFont(session.fontName, session.fontSize);
    selectedBgColor = QColor(session.backgroundColor);
    selectedTextColor = QColor(session.textColor);
    
    updateFontDisplay();
    updateColorDisplays();
    
    // Update UI based on auth type
    onAuthTypeChanged(session.authType);
}

SessionInfo SessionDialog::getSessionInfo() const
{
    SessionInfo info;
    info.name = nameEdit->text();
    info.host = hostEdit->text();
    info.port = portEdit->value();
    info.username = usernameEdit->text();
    info.password = passwordEdit->text();
    info.authType = authTypeCombo->currentIndex();
    info.keyFile = keyFileEdit->text();
    
    // Terminal settings
    info.terminalType = terminalTypeCombo->currentText();
    info.encoding = encodingCombo->currentText();
    info.keepAlive = keepAliveCheck->isChecked();
    info.keepAliveInterval = keepAliveIntervalSpin->value();
    
    // Appearance settings
    info.fontName = selectedFont.family();
    info.fontSize = selectedFont.pointSize();
    info.backgroundColor = selectedBgColor.name();
    info.textColor = selectedTextColor.name();
    
    return info;
} 