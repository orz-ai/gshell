#ifndef SESSIONDIALOG_H
#define SESSIONDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QTabWidget>
#include <QColorDialog>
#include <QFontDialog>
#include "sessioninfo.h"

namespace Ui {
class SessionDialog;
}

class SessionDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SessionDialog(QWidget *parent = nullptr);
    ~SessionDialog();
    
    void setSessionInfo(const SessionInfo &info);
    SessionInfo getSessionInfo() const;

private slots:
    void onAuthTypeChanged(int index);
    void browseKeyFile();
    void selectFont();
    void selectBackgroundColor();
    void selectTextColor();

private:
    Ui::SessionDialog *ui;
    QTabWidget *tabWidget;
    
    // Connection tab
    QLineEdit *nameEdit;
    QLineEdit *hostEdit;
    QSpinBox *portEdit;
    QLineEdit *usernameEdit;
    QLineEdit *passwordEdit;
    QComboBox *authTypeCombo;
    QLineEdit *keyFileEdit;
    QPushButton *browseButton;
    
    // Terminal tab
    QComboBox *terminalTypeCombo;
    QComboBox *encodingCombo;
    QCheckBox *keepAliveCheck;
    QSpinBox *keepAliveIntervalSpin;
    
    // Appearance tab
    QLineEdit *fontDisplay;
    QPushButton *fontButton;
    QLineEdit *bgColorDisplay;
    QPushButton *bgColorButton;
    QLineEdit *textColorDisplay;
    QPushButton *textColorButton;
    
    QFont selectedFont;
    QColor selectedBgColor;
    QColor selectedTextColor;
    
    void setupConnectionTab();
    void setupTerminalTab();
    void setupAppearanceTab();
    void updateFontDisplay();
    void updateColorDisplays();
};

#endif // SESSIONDIALOG_H 