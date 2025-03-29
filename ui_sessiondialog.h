/********************************************************************************
** Form generated from reading UI file 'sessiondialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SESSIONDIALOG_H
#define UI_SESSIONDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_SessionDialog
{
public:
    QVBoxLayout *verticalLayout;
    QGroupBox *groupBox;
    QFormLayout *formLayout;
    QLabel *label;
    QLineEdit *nameEdit;
    QLabel *label_2;
    QLineEdit *hostEdit;
    QLabel *label_3;
    QSpinBox *portSpinBox;
    QLabel *label_4;
    QLineEdit *usernameEdit;
    QLabel *label_5;
    QComboBox *authComboBox;
    QLabel *passwordLabel;
    QLineEdit *passwordEdit;
    QLabel *keyFileLabel;
    QHBoxLayout *horizontalLayout;
    QLineEdit *keyFileEdit;
    QPushButton *browseButton;
    QLabel *passphraseLabel;
    QLineEdit *passphraseEdit;
    QCheckBox *savePasswordCheckBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *SessionDialog)
    {
        if (SessionDialog->objectName().isEmpty())
            SessionDialog->setObjectName(QString::fromUtf8("SessionDialog"));
        SessionDialog->resize(400, 350);
        verticalLayout = new QVBoxLayout(SessionDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        groupBox = new QGroupBox(SessionDialog);
        groupBox->setObjectName(QString::fromUtf8("groupBox"));
        formLayout = new QFormLayout(groupBox);
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        label = new QLabel(groupBox);
        label->setObjectName(QString::fromUtf8("label"));

        formLayout->setWidget(0, QFormLayout::LabelRole, label);

        nameEdit = new QLineEdit(groupBox);
        nameEdit->setObjectName(QString::fromUtf8("nameEdit"));

        formLayout->setWidget(0, QFormLayout::FieldRole, nameEdit);

        label_2 = new QLabel(groupBox);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        formLayout->setWidget(1, QFormLayout::LabelRole, label_2);

        hostEdit = new QLineEdit(groupBox);
        hostEdit->setObjectName(QString::fromUtf8("hostEdit"));

        formLayout->setWidget(1, QFormLayout::FieldRole, hostEdit);

        label_3 = new QLabel(groupBox);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        formLayout->setWidget(2, QFormLayout::LabelRole, label_3);

        portSpinBox = new QSpinBox(groupBox);
        portSpinBox->setObjectName(QString::fromUtf8("portSpinBox"));
        portSpinBox->setMinimum(1);
        portSpinBox->setMaximum(65535);
        portSpinBox->setValue(22);

        formLayout->setWidget(2, QFormLayout::FieldRole, portSpinBox);

        label_4 = new QLabel(groupBox);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        formLayout->setWidget(3, QFormLayout::LabelRole, label_4);

        usernameEdit = new QLineEdit(groupBox);
        usernameEdit->setObjectName(QString::fromUtf8("usernameEdit"));

        formLayout->setWidget(3, QFormLayout::FieldRole, usernameEdit);

        label_5 = new QLabel(groupBox);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        formLayout->setWidget(4, QFormLayout::LabelRole, label_5);

        authComboBox = new QComboBox(groupBox);
        authComboBox->addItem(QString());
        authComboBox->addItem(QString());
        authComboBox->setObjectName(QString::fromUtf8("authComboBox"));

        formLayout->setWidget(4, QFormLayout::FieldRole, authComboBox);

        passwordLabel = new QLabel(groupBox);
        passwordLabel->setObjectName(QString::fromUtf8("passwordLabel"));

        formLayout->setWidget(5, QFormLayout::LabelRole, passwordLabel);

        passwordEdit = new QLineEdit(groupBox);
        passwordEdit->setObjectName(QString::fromUtf8("passwordEdit"));
        passwordEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(5, QFormLayout::FieldRole, passwordEdit);

        keyFileLabel = new QLabel(groupBox);
        keyFileLabel->setObjectName(QString::fromUtf8("keyFileLabel"));

        formLayout->setWidget(6, QFormLayout::LabelRole, keyFileLabel);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        keyFileEdit = new QLineEdit(groupBox);
        keyFileEdit->setObjectName(QString::fromUtf8("keyFileEdit"));

        horizontalLayout->addWidget(keyFileEdit);

        browseButton = new QPushButton(groupBox);
        browseButton->setObjectName(QString::fromUtf8("browseButton"));

        horizontalLayout->addWidget(browseButton);


        formLayout->setLayout(6, QFormLayout::FieldRole, horizontalLayout);

        passphraseLabel = new QLabel(groupBox);
        passphraseLabel->setObjectName(QString::fromUtf8("passphraseLabel"));

        formLayout->setWidget(7, QFormLayout::LabelRole, passphraseLabel);

        passphraseEdit = new QLineEdit(groupBox);
        passphraseEdit->setObjectName(QString::fromUtf8("passphraseEdit"));
        passphraseEdit->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(7, QFormLayout::FieldRole, passphraseEdit);

        savePasswordCheckBox = new QCheckBox(groupBox);
        savePasswordCheckBox->setObjectName(QString::fromUtf8("savePasswordCheckBox"));

        formLayout->setWidget(8, QFormLayout::FieldRole, savePasswordCheckBox);


        verticalLayout->addWidget(groupBox);

        buttonBox = new QDialogButtonBox(SessionDialog);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        retranslateUi(SessionDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), SessionDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), SessionDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(SessionDialog);
    } // setupUi

    void retranslateUi(QDialog *SessionDialog)
    {
        SessionDialog->setWindowTitle(QCoreApplication::translate("SessionDialog", "\344\274\232\350\257\235\350\256\276\347\275\256", nullptr));
        groupBox->setTitle(QCoreApplication::translate("SessionDialog", "\350\277\236\346\216\245\344\277\241\346\201\257", nullptr));
        label->setText(QCoreApplication::translate("SessionDialog", "\344\274\232\350\257\235\345\220\215\347\247\260:", nullptr));
        label_2->setText(QCoreApplication::translate("SessionDialog", "\344\270\273\346\234\272\345\234\260\345\235\200:", nullptr));
        label_3->setText(QCoreApplication::translate("SessionDialog", "\347\253\257\345\217\243:", nullptr));
        label_4->setText(QCoreApplication::translate("SessionDialog", "\347\224\250\346\210\267\345\220\215:", nullptr));
        label_5->setText(QCoreApplication::translate("SessionDialog", "\350\256\244\350\257\201\346\226\271\345\274\217:", nullptr));
        authComboBox->setItemText(0, QCoreApplication::translate("SessionDialog", "\345\257\206\347\240\201", nullptr));
        authComboBox->setItemText(1, QCoreApplication::translate("SessionDialog", "\345\257\206\351\222\245\346\226\207\344\273\266", nullptr));

        passwordLabel->setText(QCoreApplication::translate("SessionDialog", "\345\257\206\347\240\201:", nullptr));
        keyFileLabel->setText(QCoreApplication::translate("SessionDialog", "\345\257\206\351\222\245\346\226\207\344\273\266:", nullptr));
        browseButton->setText(QCoreApplication::translate("SessionDialog", "\346\265\217\350\247\210...", nullptr));
        passphraseLabel->setText(QCoreApplication::translate("SessionDialog", "\345\257\206\351\222\245\345\257\206\347\240\201:", nullptr));
        savePasswordCheckBox->setText(QCoreApplication::translate("SessionDialog", "\344\277\235\345\255\230\345\257\206\347\240\201", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SessionDialog: public Ui_SessionDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SESSIONDIALOG_H
