#include "transferwindow.h"

#include <QLayout>
#include <QMessageBox>
#include <QLabel>
#include <QTreeWidget>
#include <QPushButton>

// Constructor
TransferWindow::TransferWindow(QWidget *parent) : QMainWindow(parent), _client(new Client)
{
    _buildUI();
    _connectSlots();
}

// Destructor
TransferWindow::~TransferWindow()
{
    qDebug() << "Execute TransferWindow::~TransferWindow()";

    _client->deleteLater();
    _client = nullptr;
}

// Public Methods
void TransferWindow::setAccount(const Account &account)
{
    _client->setAccount(account);
}

void TransferWindow::setBucket(const QString &bucket)
{
    _client->setBucket(bucket);
}

void TransferWindow::setDirMode(const QString &dirMode)
{
    _dirMode = dirMode;
}

// Private Methods
void TransferWindow::_buildUI()
{
    _icon.addFile(":/dir-o-20.png", QSize(20, 20), QIcon::Normal, QIcon::On);
    _icon.addFile(":/dir-20.png", QSize(20, 20), QIcon::Normal, QIcon::Off);

    QWidget *container = new QWidget(this);
    this->setCentralWidget(container);

    QVBoxLayout *vbLayout = new QVBoxLayout(container);
    vbLayout->setSpacing(0);
    vbLayout->setContentsMargins(0, 0, 0, 0);

    QWidget *pathsContainer = new QWidget(container);
    pathsContainer->setFixedHeight(30);

    QHBoxLayout *pathsLayout = new QHBoxLayout(pathsContainer);
    pathsLayout->setContentsMargins(10, 0, 10, 0);
    pathsLayout->setAlignment(Qt::AlignLeft);

    QLabel *label = new QLabel("当前选择路径: ", pathsContainer);
    label->setFixedSize(110, 30);

    pathsLayout->addWidget(label);

    _currentPathLabel = new QLabel("/", pathsContainer);
    _currentPathLabel->setFixedHeight(30);

    pathsLayout->addWidget(_currentPathLabel);

    vbLayout->addWidget(pathsContainer);

    _tree = new QTreeWidget(container);
    _tree->setFixedSize(600, 320);
    _tree->setHeaderHidden(true);
    _tree->setColumnCount(1);
    _tree->setFrameStyle(QFrame::NoFrame);
    _tree->setRootIsDecorated(false);
    _tree->setFocusPolicy(Qt::NoFocus);

    QTreeWidgetItem *root = new QTreeWidgetItem(_tree, QStringList("/"));
    root->setData(0, Qt::UserRole, QVariant(""));
    root->setIcon(0, _icon);
    _tree->insertTopLevelItem(0, root);

    QHBoxLayout *topLayout = new QHBoxLayout;

    topLayout->addWidget(_tree);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(10, 10, 10, 10);

    bottomLayout->addStretch();

    _saveButton = new QPushButton("确定", container);
    _saveButton->setFixedSize(100, 30);

    QFont font = _saveButton->font();
    font.setBold(true);
    font.setPixelSize(14);
    _saveButton->setFont(font);

    bottomLayout->addWidget(_saveButton);

    _cancelButton = new QPushButton("取消", container);
    _cancelButton->setFixedSize(100, 30);

    bottomLayout->addWidget(_cancelButton);

    vbLayout->addLayout(topLayout);
    vbLayout->addLayout(bottomLayout);
}

void TransferWindow::_connectSlots() const
{
    connect(_tree, &QTreeWidget::itemClicked, this, &TransferWindow::_itemClicked);

    connect(this, &TransferWindow::listObject, _client, &Client::listObject, Qt::QueuedConnection);
    connect(_client, &Client::listObjectResponse, this, &TransferWindow::_listObjectResponse, Qt::QueuedConnection);

    connect(_saveButton, &QPushButton::clicked, this, &TransferWindow::_saveButtonClicked);
    connect(_cancelButton, &QPushButton::clicked, this, &TransferWindow::_cancelButtonClicked);
}

void TransferWindow::_listObject(const ListObjectParams &params)
{
    emit listObject(params);
}

// Private Slots
void TransferWindow::_listObjectResponse(QNetworkReply::NetworkError error,
                                         const QStringHash &params,
                                         const QStringVector &dirs,
                                         const QVector<File> &files)
{
    Q_UNUSED(files);

    qDebug() << "receive TransferWindow listObjectResponse";

    if (error != QNetworkReply::NoError)
    {
        QMessageBox::warning(this, "警告", "获取对象列表失败");

        return;
    }

    qDebug() << "params:" << params;
    qDebug() << "dirs:" << dirs;

    _storeDirs += dirs;
    _storeDirs.removeOne(params["prefix"]);

    if (params["isTruncated"] == "true")
    {
        ListObjectParams listParams(params["prefix"], params["nextMarker"]);

        _listObject(listParams);

        return;
    }

    QTreeWidgetItem *item = _tree->currentItem();
    QList<QTreeWidgetItem*> children;

    foreach (auto dir, _storeDirs)
    {
        QString text = _path == "" ? dir : dir.replace(dir.indexOf(_path), _path.size(), "");

        QTreeWidgetItem *child = new QTreeWidgetItem(item, QStringList(text));
        child->setData(0, Qt::UserRole, QVariant(_path + text));
        child->setIcon(0, _icon);

        children.append(child);
    }

    item->addChildren(children);
    item->setExpanded(true);

    _storeDirs.clear();
}

void TransferWindow::_itemClicked(QTreeWidgetItem *item, int)
{
    QString fullPath = item->data(0, Qt::UserRole).toString();

    qDebug() << "fullPath:" << fullPath;

    _path = fullPath;
    _currentPathLabel->setText("/" + _path);

    if (item->childCount() == 0 && !item->isExpanded())
    {
        ListObjectParams params(_path);

        _listObject(params);
    }
    else
        item->setExpanded(true);
}

void TransferWindow::_saveButtonClicked()
{
    emit transferPathSelected(_dirMode, _path);

    close();
}

void TransferWindow::_cancelButtonClicked()
{
//    emit transferCanceled();

    close();
}
