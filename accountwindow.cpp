#include "accountwindow.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QFont>
#include <QDebug>
#include <QMessageBox>
#include <QLineEdit>
#include <QPushButton>

// Constructor
AccountWindow::AccountWindow(QWidget *parent) : QMainWindow(parent)
{    
    setWindowTitle("帐户信息");

    _buildUI();
    _connectSlots();
}

#if 0
// Destructor
AccountWindow::~AccountWindow()
{
    qDebug() << "Execute AccountWindow::~AccountWindow()";
}
#endif

// Public Methods
void AccountWindow::setAccount(const Account &account)
{
    _account = account;

    _nameInput->setText(_account.name);
    _nameInput->setDisabled(true);
    _endpointInput->setText(_account.endpoint);
    _accessKeyInput->setText(_account.accessKey);
    _accessSecretInput->setText(_account.accessSecret);
}

// Private Methods
void AccountWindow::_buildUI()
{
    QWidget *widget = new QWidget(this);
    this->setCentralWidget(widget);

    QVBoxLayout *vbLayout = new QVBoxLayout;

    QLabel *nameLabel = new QLabel("名称", widget);
    nameLabel->setFixedHeight(24);
    vbLayout->addWidget(nameLabel);

    _nameInput = new QLineEdit(widget);
    _nameInput->setTextMargins(2, 2, 2, 2);
    _nameInput->setFixedHeight(28);
    _nameInput->setPlaceholderText("任意内容，方便记忆");

    vbLayout->addWidget(_nameInput);

    QLabel *endpointLabel = new QLabel("端点", widget);
    endpointLabel->setFixedHeight(24);
    vbLayout->addWidget(endpointLabel);

    _endpointInput = new QLineEdit(widget);
    _endpointInput->setTextMargins(2, 2, 2, 2);
    _endpointInput->setFixedHeight(28);
    _endpointInput->setPlaceholderText("Endpoint");

    vbLayout->addWidget(_endpointInput);

    QLabel *accessKeyLabel = new QLabel("访问钥匙", widget);
    accessKeyLabel->setFixedHeight(24);
    vbLayout->addWidget(accessKeyLabel);

    _accessKeyInput = new QLineEdit(widget);
    _accessKeyInput->setTextMargins(2, 2, 2, 2);
    _accessKeyInput->setFixedHeight(28);
    _accessKeyInput->setPlaceholderText("Access Key");

    vbLayout->addWidget(_accessKeyInput);

    QLabel *accessSecretLabel = new QLabel("访问密钥", widget);
    accessSecretLabel->setFixedHeight(24);
    vbLayout->addWidget(accessSecretLabel);

    _accessSecretInput = new QLineEdit(widget);
    _accessSecretInput->setTextMargins(2, 2, 2, 2);
    _accessSecretInput->setFixedHeight(28);
    _accessSecretInput->setPlaceholderText("Access Secret");

    vbLayout->addWidget(_accessSecretInput);

    vbLayout->addStretch();

    _saveButton = new QPushButton(widget);
    _saveButton->setText("确定");
    _saveButton->setFixedSize(262, 30);

    QFont font = _saveButton->font();
    font.setBold(true);
    font.setPixelSize(14);
    _saveButton->setFont(font);

    vbLayout->addWidget(_saveButton);

    vbLayout->addStretch();

    vbLayout->setMargin(10);
    vbLayout->setSpacing(2);
    vbLayout->setContentsMargins(9, 5, 10, 0);

    widget->setLayout(vbLayout);
}

void AccountWindow::_connectSlots() const
{
    connect(_saveButton, &QPushButton::clicked, this, &AccountWindow::_saveAccountInfo);
}

// Private Slots
void AccountWindow::_saveAccountInfo()
{
    if (_nameInput->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "名称必须输入");

        return;
    }

    if (_endpointInput->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "端点必须输入");

        return;
    }

    if (_accessKeyInput->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "访问钥匙必须输入");

        return;
    }

    if (_accessSecretInput->text().isEmpty()) {
        QMessageBox::warning(this, "警告", "访问密钥必须输入");

        return;
    }

    QStringHash accountData = {
        { "name", _nameInput->text() },
        { "endpoint", _endpointInput->text() },
        { "accessKey", _accessKeyInput->text() },
        { "accessSecret", _accessSecretInput->text() }
    };

    qDebug() << "accountData:" << accountData;

    emit sendAccountData(accountData);

    close();
}
