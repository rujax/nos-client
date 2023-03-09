#ifndef ACCOUNTWINDOW_H
#define ACCOUNTWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QLineEdit;
class QPushButton;
QT_END_NAMESPACE

#include "qstringhash.h"

#include "account.h"

class AccountWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit AccountWindow(QWidget *parent = nullptr);
#if 0
    ~AccountWindow();
#endif

    void setAccount(const Account &account);

signals:
    void sendAccountData(const QStringHash &accountData);

private:
    Account _account;

    QLineEdit *_nameInput;
    QLineEdit *_endpointInput;
    QLineEdit *_accessKeyInput;
    QLineEdit *_accessSecretInput;

    QPushButton *_saveButton;

    void _buildUI();
    void _connectSlots() const;

private slots:
    void _saveAccountInfo();
};

#endif // ACCOUNTWINDOW_H
