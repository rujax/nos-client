#include "mainwindow.h"

#include <QApplication>
#include <QBoxLayout>
#include <QMessageBox>
#include <QDebug>
#include <QIcon>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QList>
#include <QActionGroup>
#include <QLabel>
#include <QSizePolicy>
#include <QFileDialog>
#include <QStandardPaths>
#include <QTimer>
#include <QInputDialog>
#include <QDirIterator>
#include <QDesktopServices>
#include <QMimeData>
#include <QPushButton>
#include <QHeaderView>
#include <QMenuBar>
#include <QSettings>
#include <QTimer>
#include <QClipboard>
#include <QProgressBar>
#include <QWinTaskbarButton>
#include <QWinTaskbarProgress>
#include <QVariant>

#include "logger.h"
#include "otablewidget.h"
#include "accountwindow.h"
#include "transferwindow.h"
#include "refreshwindow.h"

// Constructor
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    _client(new Client),
    _cdn(new CDN),
    _logger(new Logger),
    _jobQueue(new JobQueue<Task>),
    _workQueue(new WorkerQueue<Task>(6)),
    _taskTimer(new QTimer(this)),
    _progressTimer(new QTimer(this)),
    _winTaskbarButton(new QWinTaskbarButton(this))
{
//    qDebug() << "Main Thread:" << this->thread();

    qRegisterMetaType<QStringHash>("QStringHash");
    qRegisterMetaType<QStringMap>("QStringMap");
    qRegisterMetaType<QStringVector>("QStringVector");

    setWindowTitle("NOS Client");
    setWindowIcon(QIcon(":/nos-client.ico"));
    setAcceptDrops(false);

    QString settingPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/nos.ini";

    _settings = new QSettings(settingPath, QSettings::IniFormat, this);

    _lastOpenPath = _getSetting("lastOpenPath");

    QString sortOrder = _getSetting("lastSortOrder");

    if (sortOrder.isEmpty())
        _setLastSortOrder();
    else
    {
        QStringList columnAndOrder = sortOrder.split("|");
        _lastSortColumn = columnAndOrder.first().toInt();
        _lastSortOrder = static_cast<Qt::SortOrder>(columnAndOrder.last().toInt());
    }

    _initUI();
    _connectSlots();
    _initAccount();
    _setAccountsMenu();

    setFocusPolicy(Qt::StrongFocus);

#if 1
    if (!_config.currentAccount.isEmpty())
    {
        _listBucket();
        _listDomain();
    }
#endif
}

// Destructor
MainWindow::~MainWindow()
{
    qDebug() << "Execute MainWindow::~MainWindow()";

    if (_taskTimer->isActive()) _taskTimer->stop();
    delete _taskTimer;
    _taskTimer = nullptr;

    if (_progressTimer->isActive()) _progressTimer->stop();
    delete _progressTimer;
    _progressTimer = nullptr;

    _client->deleteLater();
    _client = nullptr;

    _cdn->deleteLater();
    _cdn = nullptr;

    _logger->deleteLater();
    _logger = nullptr;

    _workQueue->clear();
    delete _workQueue;
    _workQueue = nullptr;

    _jobQueue->clear();
    delete _jobQueue;
    _jobQueue = nullptr;
}

// Public Methods
void MainWindow::setTaskbarHandle()
{
//    qDebug() << "windowHandle():" << windowHandle();

    _winTaskbarButton->setWindow(windowHandle());

    _winTaskbarProgress = _winTaskbarButton->progress();
    _winTaskbarProgress->setRange(0, 100);
    _winTaskbarProgress->setValue(0);
    _winTaskbarProgress->show();
}

// Protected Methods
void MainWindow::keyPressEvent(QKeyEvent *event)
{
    if (!_config.currentAccount.isEmpty() && event->key() == Qt::Key_F5) _refreshListClicked();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);

    if (event->type() == QEvent::WindowStateChange &&
            (this->windowState() == Qt::WindowMaximized || this->windowState() == Qt::WindowNoState))
        _objectTable->setFirstColumnWidth(width(), height());
}

#if 0
void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
//    qDebug() << "event->mimeData():" << event->mimeData();

    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();

    if (urls.isEmpty()) return;

    qDebug() << "urls" << urls;

    if (urls.isEmpty()) return;

    QString path = urls.first().toLocalFile();

    if (path.isEmpty()) return;

    qDebug() << "path:" << path;

    QFileInfo fileInfo(path);

    if (fileInfo.isDir()) uploadDir(path);
    else if (fileInfo.isFile())
    {
        QString objectKey = paths.last() + fileInfo.fileName();

        insertTask("上传", objectKey, Client::humanReadableSize(fileInfo.size(), 2), "上传中...");

        _putObject(objectKey, path);
    }
}
#endif

// Private Methods
void MainWindow::_initUI()
{
    _container = new QWidget(this);
    _container->setObjectName("container");
    this->setCentralWidget(_container);
    this->layout()->setSpacing(0);

    QMargins noMargin;

    // 上布局
    QHBoxLayout *topLayout = new QHBoxLayout;

    // 侧边栏
    QWidget *topLeftWidget = new QWidget(_container);
    topLeftWidget->setObjectName("sidebar");
    topLeftWidget->setFixedWidth(200);
    topLeftWidget->setAcceptDrops(false);

    // 侧边栏布局
    QVBoxLayout *topLeftLayout = new QVBoxLayout(topLeftWidget);
    topLeftLayout->setSpacing(0);
    topLeftLayout->setContentsMargins(noMargin);
    topLeftLayout->setAlignment(Qt::AlignTop);

    // 桶操作按钮布局
    QHBoxLayout *bucketButtons = new QHBoxLayout;
    bucketButtons->setSpacing(0);
    bucketButtons->setContentsMargins(noMargin);

    QPushButton *newBucket = new QPushButton("新建桶", topLeftWidget);
    newBucket->setObjectName("new-bucket-button");

    QPushButton *deleteBucket = new QPushButton("删除桶", topLeftWidget);
    deleteBucket->setObjectName("delete-bucket-button");

    bucketButtons->addWidget(newBucket);
    bucketButtons->addWidget(deleteBucket);

    auto newOrDeleteBucket = [this] {
        if (!_isReady) return;
        if (_client->getBucket().isEmpty()) return;

        QMessageBox::warning(this, "警告", "请前往网页操作新增/删除桶功能！");
    };

    connect(newBucket, &QPushButton::clicked, newOrDeleteBucket);
    connect(deleteBucket, &QPushButton::clicked, newOrDeleteBucket);

    topLeftLayout->addLayout(bucketButtons);

    // 桶列表布局
    _bucketList = new QVBoxLayout;
    _bucketList->setSpacing(0);
    _bucketList->setContentsMargins(noMargin);

    topLeftLayout->addLayout(_bucketList);

    // 文件区域
    QWidget *topRightWidget = new QWidget(_container);

    // 文件区域布局
    QVBoxLayout *topRightLayout = new QVBoxLayout(topRightWidget);
    topRightLayout->setSpacing(0);
    topRightLayout->setContentsMargins(noMargin);
    topRightLayout->setAlignment(Qt::AlignTop);

    // 路径面包屑
    QWidget *pathsBreadcrumb = new QWidget(topRightWidget);
    pathsBreadcrumb->setObjectName("paths");
    pathsBreadcrumb->setFixedHeight(36);

    QHBoxLayout *pathsLayout = new QHBoxLayout(pathsBreadcrumb);
    pathsLayout->setSpacing(1);
    pathsLayout->setContentsMargins(10, 0, 0, 0);
    pathsLayout->setAlignment(Qt::AlignLeft);

    QLabel *label = new QLabel("路径: ", topRightWidget);
    label->setObjectName("path-label");
    label->setFixedSize(40, 36);

    pathsLayout->addWidget(label);

    _pathsButtons = new QHBoxLayout;
    _pathsButtons->setSpacing(6);

    _updatePathButtons();

    pathsLayout->addLayout(_pathsButtons);

    pathsLayout->addStretch();

    // 刷新缓存按钮
    QPushButton *refreshCacheButton = new QPushButton("刷新缓存", topRightWidget);
    refreshCacheButton->setObjectName("refresh-cache-button");

    connect(refreshCacheButton, &QPushButton::clicked, [this] {
        if (_currentDomain.isEmpty())
        {
            QMessageBox::warning(this, "警告", "请先前往网页创建加速域名！");

            return;
        }

        QString url = "https://" + _currentDomain + "/" + _paths.last();

        QMessageBox::StandardButton button = QMessageBox::warning(this,
                                                                  "确认刷新缓存",
                                                                  url,
                                                                  QMessageBox::Cancel|QMessageBox::Ok,
                                                                  QMessageBox::Cancel);

        if (button != QMessageBox::Ok) return;

        PurgeParams params = { .domain = _currentDomain, .files = {}, .dirs = { url } };

        _purge(params);
    });

    pathsLayout->addWidget(refreshCacheButton);

    // 复制路径
    QPushButton *copyPathButton = new QPushButton("复制路径", topRightWidget);
    copyPathButton->setObjectName("copy-path-button");

    connect(copyPathButton, &QPushButton::clicked, [this] {
        QClipboard *clipboard = QApplication::clipboard();

        QString url;

        if (!_currentDomain.isEmpty()) url = "https://" + _currentDomain;

        url += "/" + _paths.last();

        clipboard->setText(url);

        QMessageBox::information(this, "提示", "复制路径成功！");
    });

    pathsLayout->addWidget(copyPathButton);

    topRightLayout->addWidget(pathsBreadcrumb);

    // 文件列表
    _objectTable = new OTableWidget(0, 3, topRightWidget);
    _objectTable->horizontalHeader()->setSortIndicatorShown(true);
    _objectTable->horizontalHeader()->setSortIndicator(_lastSortColumn, _lastSortOrder);

    topRightLayout->addWidget(_objectTable);

    // 文件操作按钮
    QWidget *actionButtons = new QWidget(topRightWidget);
    actionButtons->setObjectName("action-buttons");
    actionButtons->setFixedHeight(60);

    QHBoxLayout *actionButtonsLayout = new QHBoxLayout(actionButtons);
    actionButtonsLayout->setContentsMargins(10, 0, 10, 0);
    actionButtonsLayout->setAlignment(Qt::AlignLeft);

    _uploadFileButton = new QPushButton(QIcon(":/upload-20.png"), "上传文件", actionButtons);
    _uploadFileButton->setObjectName("upload-file-button");
    _uploadFileButton->setContentsMargins(20, 0, 20, 0);

    actionButtonsLayout->addWidget(_uploadFileButton);

    _uploadDirButton = new QPushButton(QIcon(":/open-dir-20.png"), "上传文件夹", actionButtons);
    _uploadDirButton->setObjectName("upload-dir-button");

    actionButtonsLayout->addWidget(_uploadDirButton);

    _downloadButton = new QPushButton(QIcon(":/download-20.png"), "下载", actionButtons);
    _downloadButton->setObjectName("download-button");
    _downloadButton->setEnabled(false);

    actionButtonsLayout->addWidget(_downloadButton);

    _deleteButton = new QPushButton(QIcon(":/delete-20.png"), "删除", actionButtons);
    _deleteButton->setObjectName("delete-button");
    _deleteButton->setEnabled(false);

    actionButtonsLayout->addWidget(_deleteButton);

    _newDirButton = new QPushButton(QIcon(":/new-dir-20.png"), "新建文件夹", actionButtons);
    _newDirButton->setObjectName("new-dir-button");

    actionButtonsLayout->addWidget(_newDirButton);

    _refreshButton = new QPushButton(QIcon(":/refresh-20.png"), "刷新", actionButtons);
    _refreshButton->setObjectName("refresh-button");

    actionButtonsLayout->addWidget(_refreshButton);

    actionButtonsLayout->addStretch();

    _objectCountLabel = new QLabel(actionButtons);
    _objectCountLabel->setObjectName("object-count-label");
    _objectCountLabel->setAlignment(Qt::AlignCenter);
    _objectCountLabel->setText("文件夹: 0 文件: 0 合计: 0");

    actionButtonsLayout->addWidget(_objectCountLabel);

    topRightLayout->addWidget(actionButtons);

    topLayout->addWidget(topLeftWidget);
    topLayout->addWidget(topRightWidget);

    // 下布局
    QHBoxLayout *bottomLayout = new QHBoxLayout;

    QWidget *tasksWidget = new QWidget(_container);
    tasksWidget->setObjectName("tasks");
    tasksWidget->setFixedHeight(220);
    tasksWidget->setAcceptDrops(false);

    QVBoxLayout *tasksLayout = new QVBoxLayout(tasksWidget);
    tasksLayout->setSpacing(0);
    tasksLayout->setContentsMargins(noMargin);
    tasksLayout->setAlignment(Qt::AlignTop);

    // 任务、查看日志
    QWidget *taskTabs = new QWidget(tasksWidget);
    taskTabs->setObjectName("task-tabs");
    taskTabs->setFixedHeight(40);

    QHBoxLayout *taskTabsLayout = new QHBoxLayout(taskTabs);
    taskTabsLayout->setSpacing(0);
    taskTabsLayout->setContentsMargins(0, 0, 10, 0);
    taskTabsLayout->setAlignment(Qt::AlignLeft);

    QLabel *taskLabel = new QLabel("任务", tasksWidget);
    taskLabel->setObjectName("task-label");
    taskLabel->setFixedSize(100, 40);
    taskLabel->setAlignment(Qt::AlignCenter);

    taskTabsLayout->addWidget(taskLabel);

    QPushButton *logButton = new QPushButton("查看日志", tasksWidget);
    logButton->setObjectName("log-button");
    logButton->setFixedSize(100, 40);

    connect(logButton, &QPushButton::clicked, [this] {
        if (!_isReady) return;

        QString logDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/log";

        QDesktopServices::openUrl(QUrl("file:///" + logDirPath));
    });

    taskTabsLayout->addWidget(logButton);

    taskTabsLayout->addStretch();

    _taskCountLabel = new QLabel(tasksWidget);
    _taskCountLabel->setObjectName("task-count-label");
    _taskCountLabel->setAlignment(Qt::AlignCenter);
    _taskCountLabel->setText("当前任务: 0");

    taskTabsLayout->addWidget(_taskCountLabel);

    tasksLayout->addWidget(taskTabs);

    // 任务列表
    _taskTable = new QTableWidget(0, 4, tasksWidget);
    _taskTable->setColumnWidth(0, 100);
    _taskTable->setColumnWidth(2, 100);
    _taskTable->setColumnWidth(3, 120); // 单元格宽度太短会导致进度条显示不完整超框
    _taskTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _taskTable->setSelectionMode(QAbstractItemView::NoSelection);
    _taskTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _taskTable->setFocusPolicy(Qt::NoFocus);

    QStringList taskHeaderLabels = { "操作", "名称", "大小", "状态" };

    _taskTable->setHorizontalHeaderLabels(taskHeaderLabels);

    QHeaderView *taskTableHeader = _taskTable->horizontalHeader();

    taskTableHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    taskTableHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    taskTableHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    taskTableHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    taskTableHeader->setSectionsClickable(false);

    _taskTable->verticalHeader()->hide();
    _taskTable->verticalHeader()->setDefaultSectionSize(20);

    tasksLayout->addWidget(_taskTable);

    bottomLayout->addWidget(tasksWidget);

    // 整体布局
    QVBoxLayout *containerLayout = new QVBoxLayout;
    containerLayout->setSpacing(0);
    containerLayout->setContentsMargins(noMargin);

    containerLayout->addLayout(topLayout);
    containerLayout->addLayout(bottomLayout);

    _container->setLayout(containerLayout);

    _initMenu();
}

void MainWindow::_initMenu()
{
    // 帐户
    _accountMenu = menuBar()->addMenu("帐户");

    _newAccountAction = new QAction("添加帐户");
    _accountMenu->addAction(_newAccountAction);

    _editAccountAction = new QAction("修改帐户");
    _editAccountAction->setEnabled(false);
    _accountMenu->addAction(_editAccountAction);

    _deleteAccountAction = new QAction("删除帐户");
    _deleteAccountAction->setEnabled(false);
    _accountMenu->addAction(_deleteAccountAction);

    _accountMenu->addSeparator();

    _skipOlderGroup = new QActionGroup(_accountMenu);
    _skipOlderGroup->setExclusive(true);

    _notSkipAction = new QAction("不跳过旧文件", _skipOlderGroup);
    _notSkipAction->setCheckable(true);

    _accountMenu->addAction(_notSkipAction);

    _skipAction = new QAction("跳过旧文件", _skipOlderGroup);
    _skipAction->setCheckable(true);

    _accountMenu->addAction(_skipAction);

    _accountMenu->addSeparator();

    _accountGroup = new QActionGroup(_accountMenu);
    _accountGroup->setExclusive(true);

    // CDN
    _cdnMenu = menuBar()->addMenu("CDN");

    _listDomainAction = new QAction("刷新域名");
    _cdnMenu->addAction(_listDomainAction);

    _listCacheAction = new QAction("缓存刷新列表");
    _cdnMenu->addAction(_listCacheAction);

    _cdnMenu->addSeparator();

    _cdnGroup = new QActionGroup(_cdnMenu);
    _cdnGroup->setExclusive(true);

    // 帮助
    _helpMenu = menuBar()->addMenu("帮助");

    _aboutAction = new QAction("关于");
    _helpMenu->addAction(_aboutAction);
}

void MainWindow::_connectSlots()
{
    // NOS
    connect(this, &MainWindow::listBucket, _client, &Client::listBucket, Qt::QueuedConnection);
    connect(this, &MainWindow::listObject, _client, &Client::listObject, Qt::QueuedConnection);
    connect(this, &MainWindow::headObject, _client, &Client::headObject, Qt::QueuedConnection);
    connect(this, &MainWindow::putObject, _client, &Client::putObject, Qt::QueuedConnection);
    connect(this, &MainWindow::getObject, _client, &Client::getObject, Qt::QueuedConnection);
    connect(this, &MainWindow::deleteObject, _client, &Client::deleteObject, Qt::QueuedConnection);
    connect(this, &MainWindow::copyObject, _client, &Client::copyObject, Qt::QueuedConnection);
    connect(this, &MainWindow::moveObject, _client, &Client::moveObject, Qt::QueuedConnection);

    connect(_client, &Client::listBucketResponse, this, &MainWindow::_listBucketResponse, Qt::QueuedConnection);
    connect(_client, &Client::listObjectResponse, this, &MainWindow::_listObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::getObjectResponse, this, &MainWindow::_getObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::headObjectResponse, this, &MainWindow::_headObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::putObjectResponse, this, &MainWindow::_putObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::deleteObjectResponse, this, &MainWindow::_deleteObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::copyObjectResponse, this, &MainWindow::_copyObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::moveObjectResponse, this, &MainWindow::_moveObjectResponse, Qt::QueuedConnection);
    connect(_client, &Client::updateProgressResponse, this, &MainWindow::_updateProgressResponse, Qt::QueuedConnection);
    connect(_client, &Client::errorResponse, this, &MainWindow::_errorResponse, Qt::QueuedConnection);

    // CDN
    connect(this, &MainWindow::listDomain, _cdn, &CDN::listDomain, Qt::QueuedConnection);
    connect(this, &MainWindow::purge, _cdn, &CDN::purge, Qt::QueuedConnection);

    connect(_cdn, &CDN::listDomainResponse, this, &MainWindow::_listDomainResponse, Qt::QueuedConnection);
    connect(_cdn, &CDN::purgeResponse, this, &MainWindow::_purgeResponse, Qt::QueuedConnection);
    connect(_cdn, &CDN::errorResponse, this, &MainWindow::_errorResponse, Qt::QueuedConnection);

    // Log
    connect(this, &MainWindow::writeLog, _logger, &Logger::writeLog, Qt::QueuedConnection);

    // Menu
    connect(_newAccountAction, &QAction::triggered, [this] {
        _openAccountWindow("new");
    });

    connect(_editAccountAction, &QAction::triggered, [this] {
        _openAccountWindow("edit");
    });

    connect(_deleteAccountAction, &QAction::triggered, this, &MainWindow::_deleteAccount);

    connect(_notSkipAction, &QAction::triggered, [this] {
        _changeSkipOlder(false);
        _writeConfigToJSON();
    });

    connect(_skipAction, &QAction::triggered, [this] {
        _changeSkipOlder(true);
        _writeConfigToJSON();
    });

    connect(_listDomainAction, &QAction::triggered, [this] {
        _listDomain();
    });

    connect(_listCacheAction, &QAction::triggered, [this] {
        _openRefreshWindow();
    });

    connect(_aboutAction, &QAction::triggered, [this] {
        QString text = "<h4>NOS Client - (Netease Object Storage Client)</h4>";
        text += "<pre>Author:      Rujax Chen</pre>";
        text += "<pre>App Version: " + QString::fromLocal8Bit(APP_VERSION) + "</pre>";
        text += "<pre>Qt Version:  " + QString::fromLocal8Bit(QT_VERSION_STR) + "</pre>";

        QMessageBox box(QMessageBox::Information, "关于", text, QMessageBox::Ok, this);

        box.setTextFormat(Qt::RichText);
        box.button(QMessageBox::Ok)->setText("确定");

        box.exec();
    });

    // Table
    connect(_objectTable, &QTableWidget::cellDoubleClicked, this, &MainWindow::_cellDoubleClicked);
    connect(_objectTable, &QTableWidget::customContextMenuRequested, this, &MainWindow::_showObjectTableMenu);
    connect(_objectTable, &OTableWidget::receiveDropEvent, this, &MainWindow::_handleDropEvent);
    connect(_objectTable, &OTableWidget::selectedRowsChanged, this, &MainWindow::_selectedRowsChange);

    connect(_objectTable->horizontalHeader(), &QHeaderView::sectionClicked, this, &MainWindow::_sortByColumn);

    // Action
    connect(_uploadFileButton, &QPushButton::clicked, this, &MainWindow::_uploadFileClicked);
    connect(_uploadDirButton, &QPushButton::clicked, this, &MainWindow::_uploadDirClicked);
    connect(_downloadButton, &QPushButton::clicked, this, &MainWindow::_downloadObjectClicked);
    connect(_deleteButton, &QPushButton::clicked, this, &MainWindow::_deleteObjectClicked);
    connect(_newDirButton, &QPushButton::clicked, this, &MainWindow::_newDirClicked);
    connect(_refreshButton, &QPushButton::clicked, this, &MainWindow::_refreshListClicked);

    // Timer
    connect(_taskTimer, &QTimer::timeout, this, &MainWindow::_updateTaskCount);
    connect(_progressTimer, &QTimer::timeout, this, &MainWindow::_updateTaskbarProgress);
}

bool MainWindow::_initAccount()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config.json";

//    qDebug() << "QFile::exists(configPath):" << QFile::exists(configPath);

    QFile configFile(configPath);

    if (!configFile.exists())
    {
        _openAccountWindow("new");

        return false;
    }

    if (!configFile.open(QIODevice::ReadOnly|QIODevice::Text))
    {
        configFile.close();

        QMessageBox::warning(this, "警告", "配置文件已损坏，请删除文件后重新打开软件");

        return false;
    }

    QByteArray configData = configFile.readAll();

    configFile.close();

    QJsonParseError jsonError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(configData, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        QMessageBox::warning(this, "警告", "解析配置文件失败，请删除文件后重新打开软件");

        return false;
    }

    QJsonObject root = jsonDoc.object();

    foreach (const QJsonValue &value, root["accounts"].toArray())
    {
        auto obj = value.toObject();

        Account account(obj["name"].toString(), obj["endpoint"].toString(), obj["accessKey"].toString(), obj["accessSecret"].toString());

        _config.accounts.append(account);
    }

    _config.currentAccount = root["currentAccount"].toString();

    if (root["skipOlder"].isNull()) _config.skipOlder = false;
    else _config.skipOlder = root["skipOlder"].toBool();

    _changeSkipOlder(_config.skipOlder);

    if (_config.accounts.length() == 0)
    {
        _openAccountWindow("new");

        return false;
    }

    if (_config.currentAccount == "")
    {
        _currentAccount = _config.accounts.first();

        _config.currentAccount = _currentAccount.name;

        _setTitleWithAccount();

        _client->setAccount(_currentAccount);
        _cdn->setAccount(_currentAccount);
    }
    else
    {
        foreach (auto account, _config.accounts)
        {
            if (_config.currentAccount == account.name)
            {
                _currentAccount = account;

                _client->setAccount(_currentAccount);
                _cdn->setAccount(_currentAccount);

                break;
            }
        }

        _setTitleWithAccount();
    }

    _isReady = true;

    return true;
}

void MainWindow::_setTitleWithAccount()
{
    qDebug() << "_config.currentAccount:" << _config.currentAccount;

    if (_config.currentAccount.isEmpty())
        setWindowTitle("NOS Client");
    else
        setWindowTitle("NOS Client - " + _config.currentAccount);
}

void MainWindow::_setAccountsMenu()
{
    foreach (auto action, _accountGroup->actions())
    {
        action->disconnect();
        _accountGroup->removeAction(action);
        _accountMenu->removeAction(action);
    }

    if (_config.accounts.length() > 0)
    {
        _editAccountAction->setEnabled(true);
        _deleteAccountAction->setEnabled(true);

        auto actions = _accountGroup->actions();

        for (int i = 0; i < _config.accounts.length(); ++i)
        {
            QString name = _config.accounts.at(i).name;

            QAction *action = new QAction(name, _accountGroup);
            action->setCheckable(true);

            _accountMenu->addAction(action);

            connect(action, &QAction::triggered, [this, name] {
                _changeAccount(name);
            });

            if (name == _config.currentAccount) action->setChecked(true);
        }
    }
    else
    {
        _editAccountAction->setEnabled(false);
        _deleteAccountAction->setEnabled(false);
    }
}

void MainWindow::_resetUI(bool withBucket)
{
    _objectTable->clearContents();
    _objectTable->setRowCount(0);

    _taskTable->clearContents();
    _taskTable->setRowCount(0);

    if (withBucket)
    {
        QLayoutItem *bucket;

        while ((bucket = _bucketList->takeAt(0)) != 0)
        {
            if (bucket->widget())
            {
                bucket->widget()->disconnect();
                bucket->widget()->setParent(nullptr);
            }

            delete bucket;
        }
    }

    _paths.clear();
    _paths.append("");

    _currentPathIndex = 0;

    _updatePathButtons();
}

void MainWindow::_setCDNMenu()
{
    foreach (auto action, _cdnGroup->actions())
    {
        action->disconnect();
        _cdnGroup->removeAction(action);
        _cdnMenu->removeAction(action);
    }

    if (_domains.length() > 0)
    {
        auto actions = _cdnGroup->actions();

        for (int i = 0; i < _domains.length(); ++i)
        {
            QString name = _domains.at(i).name;

            QAction *action = new QAction(name, _cdnGroup);
            action->setCheckable(true);

            _cdnMenu->addAction(action);

            connect(action, &QAction::triggered, [this, name] {
                _changeDomain(name);
            });

            if (i == 0)
            {
                action->setChecked(true);
                _currentDomain = name;
            }
        }
    }
    else
    {
        _currentDomain = "";
    }
}

void MainWindow::_openAccountWindow(const QString &mode)
{
    _editMode = mode;

    AccountWindow *aw = new AccountWindow(this);
    aw->setFixedSize(280, 290);
    aw->setWindowFlags(aw->windowFlags() & ~(Qt::WindowMinMaxButtonsHint|Qt::WindowMaximizeButtonHint));
    aw->setWindowModality(Qt::ApplicationModal);
    aw->setAttribute(Qt::WA_DeleteOnClose);
    aw->show();

    connect(aw, &AccountWindow::sendAccountData, this, &MainWindow::_receiveAccountData);

    if (_editMode == "edit") aw->setAccount(_currentAccount);
}

void MainWindow::_openRefreshWindow()
{
    if (_currentDomain.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请前往网页配置加速域名");

        return;
    }

    RefreshWindow *rw = new RefreshWindow();

    rw->setFixedSize(960, 670);
    rw->setAttribute(Qt::WA_QuitOnClose, false);
    rw->setWindowTitle("缓存刷新列表 - " + _currentDomain);
    rw->setAccount(_currentAccount);
    rw->setDomain(_currentDomain);

    rw->show();
}

void MainWindow::_setCurrentAccount()
{
    foreach (auto account, _config.accounts)
    {
        if (_config.currentAccount == account.name)
        {
            _currentAccount = account;

            _client->setAccount(_currentAccount);
            _cdn->setAccount(_currentAccount);

            break;
        }
    }
}

void MainWindow::_writeConfigToJSON()
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QDir dir(dirPath);

    if (!dir.exists())
    {
        bool isOK = dir.mkpath(dirPath);

        if (!isOK)
        {
            QMessageBox::warning(this, "警告", "保存配置文件失败，请重试");

            _isReady = false;

            return;
        }
    }

    QString configPath = dirPath + "/config.json";

    QFile configFile(configPath);

    if (configFile.open(QIODevice::WriteOnly))
    {
        QJsonObject root;
        QJsonArray accounts;

        foreach (auto account, _config.accounts)
        {
            QJsonObject obj = {
                { "name", account.name },
                { "endpoint", account.endpoint },
                { "accessKey", account.accessKey },
                { "accessSecret", account.accessSecret }
            };

            accounts.append(obj);
        }

        root.insert("accounts", accounts);
        root.insert("currentAccount", _config.currentAccount);
        root.insert("skipOlder", _config.skipOlder);

        QJsonDocument jsonDoc(root);
        QByteArray jsonData = jsonDoc.toJson(QJsonDocument::Compact);

        configFile.write(jsonData);
    }

    configFile.close();

    _isReady = true;
}

void MainWindow::_updatePathButtons()
{
    QLayoutItem *item;

    while ((item = _pathsButtons->takeAt(0)) != 0)
    {
        if (item->widget())
        {
            item->widget()->disconnect();
            item->widget()->setParent(nullptr);
        }

        delete item;
    }

    QWidget *parentWidget = _pathsButtons->parentWidget();

    for (int i = 0; i < _paths.length(); ++i)
    {
        QPushButton *button = new QPushButton(parentWidget);

        QString path;

        if (_paths.at(i).length() == 0)
        {
            button->setText("/");
        }
        else
        {
            path = _paths.at(i).mid(0, _paths.at(i).length() - 1);
            QStringList pathsList = path.split("/");

            button->setText(pathsList.last() + "/");
        }

        QFontMetrics metrics(button->font());
        int width = metrics.boundingRect(button->text()).width() + 10;
        if (width < 20) width = 20;

        button->setFixedSize(width, 20);

        QString className = "path-button";
        className += (i == _currentPathIndex ? " current": "");

        button->setProperty("class", className);

        connect(button, &QPushButton::clicked, [this, i] {
            _pathClicked(i);
        });

        _pathsButtons->addWidget(button);
    }
}

const QString MainWindow::_getSetting(const QString &key) const
{
    return _settings->value(key).toString();
}

void MainWindow::_setLastOpenPath(const QString &path)
{
    if (_lastOpenPath == path) return;

    _lastOpenPath = path;

    _settings->setValue("lastOpenPath", path);
}

void MainWindow::_setLastSortOrder()
{
    QString sortOrder = QString::number(_lastSortColumn) + "|" + QString::number(_lastSortOrder);

//    qDebug() << "sortOrder:" << sortOrder;

    _settings->setValue("lastSortOrder", sortOrder);
}

void MainWindow::_log(Client::Operation operation, Status status, const QString &msg)
{
    QString action;
    QString stat;

    switch (operation)
    {
    case Client::putObjectOperation: action = "上传"; break;
    case Client::getObjectOperation: action = "下载"; break;
    case Client::deleteObjectOperation: action = "删除"; break;
    case Client::copyObjectOperation: action = "复制"; break;
    case Client::moveObjectOperation: action = "移动"; break;
    case Client::headObjectOperation: action = "跳过"; break;
    default: action = "未知";
    }

    switch (status)
    {
    case success: stat = "成功"; break;
    case failure: stat = "失败"; break;
    }

    emit writeLog(_client->getBucket(), action, stat, msg);
}

void MainWindow::_perform()
{
    if (_workQueue->isFull()) return;
    if (_jobQueue->isEmpty()) return;

    Task task = _jobQueue->pop();

    _workQueue->push(task);

    switch (task.operation)
    {
    case Client::getObjectOperation: _downloadObject(task); break;
    case Client::putObjectOperation: _putObject(task); break;
    case Client::deleteObjectOperation: _deleteObject(task); break;
    case Client::copyObjectOperation: _copyObject(task); break;
    case Client::moveObjectOperation: _moveObject(task); break;
    default: qDebug() << "Unknown operation";
    }
}

void MainWindow::_resortObjects()
{
    int rowCount = _objectTable->rowCount();

    _sortDirs(_storeDirs);
    _sortFiles(_storeFiles);

//    qDebug() << "sorted dirs:" << storeDirs;
//    qDebug() << "sorted files:" << storeFiles;

    if (_lastSortOrder == Qt::AscendingOrder)
    {
        _listDirs(0, rowCount);
        _listFiles(_storeDirs.count(), rowCount);
    }
    else
    {
        _listFiles(0, rowCount);
        _listDirs(_storeFiles.count(), rowCount);
    }
}

void MainWindow::_sortDirs(QStringVector &dirs)
{
    if (_lastSortColumn == 0 && _lastSortOrder == Qt::DescendingOrder)
    {
        std::sort(dirs.begin(), dirs.end(), [](const QString &dir1, const QString &dir2) {
            return (dir1 > dir2);
        });
    }
    else std::sort(dirs.begin(), dirs.end());
}

void MainWindow::_sortFiles(QVector<File> &files)
{
    std::sort(files.begin(), files.end(), [=](const File &file1, const File &file2) {
        if (_lastSortColumn == 0)
        {
            if (_lastSortOrder == Qt::AscendingOrder) return (file1.key < file2.key);
            else return (file1.key > file2.key);
        }
        else if (_lastSortColumn == 1)
        {
            if (_lastSortOrder == Qt::AscendingOrder) return (file1.size < file2.size);
            else return (file1.size > file2.size);
        }
        else
        {
            if (_lastSortOrder == Qt::AscendingOrder) return (file1.lastModified < file2.lastModified);
            else return (file1.lastModified > file2.lastModified);
        }
    });
}

void MainWindow::_listDirs(int startRow, int totalRowCount)
{
    QString path = _paths.last();

    for (int row = 0; row < _storeDirs.length(); ++row)
    {
        int currentRow = row + startRow;

        if (currentRow >= totalRowCount) _objectTable->insertRow(currentRow);

        for (int col = 0; col < _objectTable->columnCount(); ++col)
        {
            QString text = "";

            if (col == 0)
            {
                QString dir(_storeDirs.at(row));
                text = path == "" ? dir : dir.replace(dir.indexOf(path), path.size(), "");
            }

            QTableWidgetItem *item = _objectTable->item(currentRow, col);

            if (!item)
            {
                item = new QTableWidgetItem;

                _objectTable->setItem(currentRow, col, item);
            }

            if (col == 0) item->setIcon(QIcon(":/dir-20.png"));

            item->setText(text);
        }
    }
}

void MainWindow::_listFiles(int startRow, int totalRowCount)
{
    QString path = _paths.last();

    for (int row = 0; row < _storeFiles.length(); ++row)
    {
        int currentRow = row + startRow;

        if (currentRow >= totalRowCount) _objectTable->insertRow(currentRow);

        for (int col = 0; col < _objectTable->columnCount(); ++col)
        {
            QString text = "";

            switch (col)
            {
            case 0:
            {
                QString key(_storeFiles.at(row).key);
                text = path == "" ? key : key.replace(key.indexOf(path), path.size(), "");

                break;
            }
            case 1:
            {
                quint64 size = _storeFiles.at(row).size;

                text = Client::humanReadableSize(size, 2);

                break;
            }
            case 2:
                text = _storeFiles.at(row).lastModified;

                break;
            }

            QTableWidgetItem *item = _objectTable->item(currentRow, col);

            if (!item)
            {
                item = new QTableWidgetItem;

                _objectTable->setItem(currentRow, col, item);
            }

            if (col == 0) item->setIcon(QIcon(":/file-20.png"));
            if (col == 1 || col == 2) item->setTextAlignment(Qt::AlignCenter);

            item->setText(text);
        }
    }
}

void MainWindow::_listBucket()
{
    emit listBucket();
}

void MainWindow::_listObject(const ListObjectParams &params)
{
    // 需要清除数据的场景：
    // 1. 点击刷新按钮
    // 2. 进入其他目录
    QHash<QString, DirAction>::const_iterator ci = _dirActions.find(params.prefix);

    if (ci == _dirActions.end() && params.marker.isEmpty())
    {
        _storeDirs.clear();
        _storeFiles.clear();

        _objectTable->clearSelectedRows();
        _objectTable->setCurrentItem(nullptr);
        _objectTable->toggleRowHidden(true);
    }

    emit listObject(params);
}

void MainWindow::_listCurrentObject()
{
    ListObjectParams params(_paths.last());

    _listObject(params);
}

void MainWindow::_headObject(const QString &objectKey, const QString &filePath, qint64 fileSize)
{
    HeadObjectParams params(objectKey, filePath, fileSize);

    emit headObject(params);
}

void MainWindow::_addPutObjectTask(const QString &objectKey, const QString &filePath, qint64 fileSize)
{
    PutObjectParams params = { .objectKey = objectKey, .filePath = filePath, .fileSize = fileSize };

    Task task(Client::putObjectOperation, QVariant::fromValue<PutObjectParams>(params));

    _jobQueue->push(task);

    if (!_taskTimer->isActive()) _taskTimer->start(1000);
    if (!_progressTimer->isActive()) _progressTimer->start(1000);

    ++_totalTaskCount;

    _perform();
}

void MainWindow::_putObject(const Task &task)
{
    PutObjectParams params = task.params.value<PutObjectParams>();

    if (params.filePath.isEmpty())
        _insertTask("新建", params.objectKey, "-", "新建中...");
    else
    {
        _insertTask("上传", params.objectKey, Client::humanReadableSize(params.fileSize, 2), "上传中...");

        _uploadBytesHash.insert(params.objectKey, 0);
    }

    emit putObject(params);
}

void MainWindow::_addDownloadObjectTask(const QString &objectKey, const QString &filePath)
{
    GetObjectParams params(objectKey, filePath);

    Task task(Client::getObjectOperation, QVariant::fromValue<GetObjectParams>(params));

    _jobQueue->push(task);

    if (!_taskTimer->isActive()) _taskTimer->start(1000);
    if (!_progressTimer->isActive()) _progressTimer->start(1000);

    ++_totalTaskCount;

    _perform();
}

void MainWindow::_downloadObject(const Task &task)
{
    GetObjectParams params = task.params.value<GetObjectParams>();

    _insertTask("下载", params.objectKey, "", "下载中...");

    emit getObject(params);
}

void MainWindow::_addDeleteObjectTask(const QString &objectKey)
{
    DeleteObjectParams params = { .objectKey = objectKey };

    Task task(Client::deleteObjectOperation, QVariant::fromValue<DeleteObjectParams>(params));

    _jobQueue->push(task);

    if (!_taskTimer->isActive()) _taskTimer->start(1000);
    if (!_progressTimer->isActive()) _progressTimer->start(1000);

    ++_totalTaskCount;

    _perform();
}

void MainWindow::_deleteObject(const Task &task)
{
    DeleteObjectParams params = task.params.value<DeleteObjectParams>();

    _insertTask("删除", params.objectKey, "-", "删除中...");

//    qDebug() << "DeleteObjectParams:" << params;

    emit deleteObject(params);
}

void MainWindow::_addCopyObjectTask(const QString &sourceObjectKey, const QString &destinationObjectKey)
{
    CopyObjectParams params(_client->getBucket(), sourceObjectKey, _client->getBucket(), destinationObjectKey);

    Task task(Client::copyObjectOperation, QVariant::fromValue<CopyObjectParams>(params));

    _jobQueue->push(task);

    if (!_taskTimer->isActive()) _taskTimer->start(1000);
    if (!_progressTimer->isActive()) _progressTimer->start(1000);

    ++_totalTaskCount;

    _perform();
}

void MainWindow::_copyObject(const Task &task)
{
    CopyObjectParams params = task.params.value<CopyObjectParams>();

    _insertTask("复制", params.sourceObjectKey + " => " + params.destinationObjectKey, "-", "复制中...");

    emit copyObject(params);
}

void MainWindow::_addMoveObjectTask(const QString &sourceObjectKey, const QString &destinationObjectKey)
{
    MoveObjectParams params(_client->getBucket(), sourceObjectKey, _client->getBucket(), destinationObjectKey);

    Task task(Client::moveObjectOperation, QVariant::fromValue<MoveObjectParams>(params));

    _jobQueue->push(task);

    if (!_taskTimer->isActive()) _taskTimer->start(1000);
    if (!_progressTimer->isActive()) _progressTimer->start(1000);

    ++_totalTaskCount;

    _perform();
}

void MainWindow::_moveObject(const Task &task)
{
    MoveObjectParams params = task.params.value<MoveObjectParams>();

    _insertTask("移动", params.sourceObjectKey + " => " + params.destinationObjectKey, "-", "移动中...");

    emit moveObject(params);
}

void MainWindow::_listDomain()
{
    ListDomainParams params;

    emit listDomain(params);
}

void MainWindow::_purge(const PurgeParams &params)
{
//    qDebug() << "_purge params:" << params;

    emit purge(params);
}

void MainWindow::_uploadDir(const QString &dirPath)
{
    QString dirName = dirPath.split("/").last();

    qDebug() << "dirName:" << dirName;

    if (dirName.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请选择正确上传的文件夹");

        return;
    }

    QString path = _paths.last();

    _addPutObjectTask(path + dirName + "/", "", 0);

    QDirIterator dirIter(dirPath, QDir::Dirs|QDir::Files|QDir::NoSymLinks|QDir::NoDotAndDotDot, QDirIterator::Subdirectories);

    while (dirIter.hasNext())
    {
        dirIter.next();

        QString filePath = dirIter.filePath();

        qDebug() << "filePath:" << filePath;

        QFileInfo fileInfo(dirIter.fileInfo());

        if (fileInfo.isDir())
        {
            filePath.replace(filePath.indexOf(dirPath), dirPath.size(), "");

            _addPutObjectTask(path + dirName + filePath + "/", "", 0);

            continue;
        }

        QString objectKey = filePath;
        objectKey.replace(objectKey.indexOf(dirPath), dirPath.size(), "");

        objectKey = path + dirName + objectKey;

        qDebug() << "objectKey:" << objectKey;

        if (_config.skipOlder) _headObject(objectKey, filePath, fileInfo.size());
        else _addPutObjectTask(objectKey, filePath, fileInfo.size());
    }
}

void MainWindow::_insertTask(const QString &action, const QString &name, const QString &size, const QString &status)
{
    qDebug() << "_insertTask action:" << action << "name:" << name << "size:" << size << "status:" << status;

    int rowCount = _taskTable->rowCount();

    _taskTable->insertRow(rowCount);

    QTableWidgetItem *actionItem = new QTableWidgetItem(action);
    actionItem->setTextAlignment(Qt::AlignCenter);

    _taskTable->setItem(rowCount, 0, actionItem);
    _taskTable->setItem(rowCount, 1, new QTableWidgetItem(name));

    QTableWidgetItem *sizeItem = new QTableWidgetItem(size);
    sizeItem->setTextAlignment(Qt::AlignCenter);

    _taskTable->setItem(rowCount, 2, sizeItem);

    if (action == "上传")
    {
        QProgressBar *progressBar = new QProgressBar(_taskTable);
        progressBar->setRange(0, 100);
        progressBar->setValue(0);

        _taskTable->setCellWidget(rowCount, 3, progressBar);
    }
    else
    {
        QTableWidgetItem *statusItem = new QTableWidgetItem(status);
        statusItem->setTextAlignment(Qt::AlignCenter);

        _taskTable->setItem(rowCount, 3, statusItem);
    }

    _taskWriteMutex.lock();
    _taskItemHash.insert(name, actionItem);
    _taskWriteMutex.unlock();
}

void MainWindow::_updateTask(const QString &action, const QString &name, const QString &status)
{
    qDebug() << "_updateTask action:" << action << "name:" << name << "status:" << status;

    _taskReadMutex.lock();
    QTableWidgetItem *item = _taskItemHash.value(name);
    _taskReadMutex.unlock();

    if (item)
    {
        if (action == "上传")
        {
            _taskTable->removeCellWidget(item->row(), 3);

            QTableWidgetItem *statusItem = new QTableWidgetItem(status);
            statusItem->setTextAlignment(Qt::AlignCenter);

            _taskTable->setItem(item->row(), 3, statusItem);
        }
        else
        {
            QTableWidgetItem *statusItem = _taskTable->item(item->row(), 3);

            if (statusItem) statusItem->setText(status);
        }
    }

    _taskWriteMutex.lock();
    _taskItemHash.remove(name);
    _taskWriteMutex.unlock();
}

void MainWindow::_removeTask(const QString &name)
{
    qDebug() << "removeTask name:" << name;

    _taskReadMutex.lock();
    QTableWidgetItem *item = _taskItemHash.value(name);
    _taskReadMutex.unlock();

    if (item) _taskTable->removeRow(item->row());

    _taskWriteMutex.lock();
    _taskItemHash.remove(name);
    _taskWriteMutex.unlock();
}

void MainWindow::_updateProgress(const QString &name, const QString &part, qint64 bytesSent, qint64 bytesTotal)
{
    qDebug() << "updateProgress name:" << name << "part:" << part << "bytesSent:" << bytesSent << "bytesTotal:" << bytesTotal;

    _taskReadMutex.lock();
    QTableWidgetItem *item = _taskItemHash.value(name);
    _taskReadMutex.unlock();

    if (!item) return;

//    qDebug() << "item:" << item;

    QWidget *cellWidget = _taskTable->cellWidget(item->row(), 3);

    if (!cellWidget) return;

//    qDebug() << "cellWidget:" << cellWidget;

    if (part.isEmpty())
    {
        if (bytesSent <= _uploadBytesHash[name]) return;

        QProgressBar *progressBar = qobject_cast<QProgressBar*>(cellWidget);

        int progress = bytesSent * 100 / bytesTotal;
        if (progress > 100) progress = 100;

        progressBar->setValue(progress);

        _uploadBytesHash[name] = bytesSent;
    }
    else
    {
        qint64 bytesCompleted = 0;
        qint64 bytesPart = 0;
        int partNum = 1;

        while (bytesPart < bytesTotal)
        {
            QString partKey = name + "|" + QString::number(partNum);

            qDebug() << "partKey:" << partKey;

            qint64 bytesPartCompleted = _uploadBytesHash[partKey];

            if (part.toInt() == partNum && bytesSent > bytesPartCompleted)
            {
                _uploadBytesHash[partKey] = bytesSent;
                bytesPartCompleted = bytesSent;
            }

            qDebug() << "bytesPartCompleted:" << bytesPartCompleted;

            bytesCompleted += bytesPartCompleted;

            ++partNum;
            bytesPart += Client::PartSize;
        }

        qDebug() << "bytesCompleted:" << bytesCompleted;

        int progress = bytesCompleted * 100 / bytesTotal;
        if (progress > 100) progress = 100;

        qDebug() << "updateProgress progress:" << progress;

        QProgressBar *progressBar = qobject_cast<QProgressBar*>(cellWidget);

        if (progress > progressBar->value()) progressBar->setValue(progress);
    }
}

void MainWindow::_updateObject(const QString &objectKey, const QString &name)
{
    qDebug() << "updateObject objectKey:" << objectKey << "name:" << name;

    _objectReadMutex.lock();
    QTableWidgetItem *item = _objectItemHash.value(objectKey);
    _objectReadMutex.unlock();

//    qDebug() << "item:" << item;
//    qDebug() << "item->row():" << item->row();

    if (item) item->setText(name);

    _objectWriteMutex.lock();
    _objectItemHash.remove(objectKey);
    _objectWriteMutex.unlock();
}

void MainWindow::_removeObject(const QString &objectKey)
{
    qDebug() << "removeObject objectKey:" << objectKey;

    _objectReadMutex.lock();
    QTableWidgetItem *item = _objectItemHash.value(objectKey);
    _objectReadMutex.unlock();

//    qDebug() << "item:" << item;
//    qDebug() << "item->row():" << item->row();

    if (item) _objectTable->removeRow(item->row());

    _objectWriteMutex.lock();
    _objectItemHash.remove(objectKey);
    _objectWriteMutex.unlock();

    if (objectKey.endsWith("/"))
        _storeDirs.removeOne(objectKey);
    else
    {
        for (int i = 0; i < _storeFiles.length(); ++i)
        {
            if (_storeFiles.at(i).key == objectKey)
            {
                _storeFiles.removeAt(i);

                break;
            }
        }
    }
}

void MainWindow::_removeUpload(const QString &objectKey, const QString &fileSize)
{
    qint64 size = fileSize.toLongLong();

    if (size > Client::PartSize)
    {
        qint64 bytes = 0;
        int partNum = 1;

        while (bytes < size)
        {
            _uploadBytesHash.remove(objectKey + "|" + QString::number(partNum));

            ++partNum;
            bytes += Client::PartSize;
        }
    }

    _uploadBytesHash.remove(objectKey);
}

void MainWindow::_updateTotal()
{
    _objectCountLabel->setText("文件夹: " + QString::number(_storeDirs.count()) +
                               " 文件: " + QString::number(_storeFiles.count()) +
                               " 合计: " + QString::number(_storeDirs.count() + _storeFiles.count()));
}

void MainWindow::_checkWorkDone()
{
    if (_workQueue->count() > 0) return;

    _dirActions.clear();
    _transferFiles.clear();

    _updateTotal();
    _updateTaskCount();

    QApplication::alert(this);
}

void MainWindow::_checkWorkDoneAndReload()
{
    if (_workQueue->count() > 0) return;

    _dirActions.clear();
    _transferFiles.clear();

    _listCurrentObject();
    _updateTotal();
    _updateTaskCount();

    QApplication::alert(this);
}

// Private Slots
void MainWindow::_receiveAccountData(const QStringHash &accountData)
{
    QString name = accountData["name"];

    if (_editMode == "new")
    {
        bool existsName = false;

        foreach (auto account, _config.accounts)
        {
            if (account.name == name)
            {
                existsName = true;

                break;
            }
        }

        if (existsName)
        {
            QMessageBox::warning(this, "警告", "名称已存在");

            return;
        }

        _resetUI();

        Account newAccount(name, accountData["endpoint"], accountData["accessKey"], accountData["accessSecret"]);

        _config.accounts.append(newAccount);
        _config.currentAccount = name;

        _currentAccount = newAccount;

        _client->setAccount(_currentAccount);
        _cdn->setAccount(_currentAccount);

        _listBucket();
    }
    else
    {
        for (int i = 0; i < _config.accounts.length(); ++i)
        {
            if (_config.accounts[i].name == name)
            {
                _config.accounts[i].endpoint = accountData["endpoint"];
                _config.accounts[i].accessKey = accountData["accessKey"];
                _config.accounts[i].accessSecret = accountData["accessSecret"];

                _currentAccount = _config.accounts[i];

                _client->setAccount(_currentAccount);

                break;
            }
        }

        _resetUI();
        _listBucket();
    }

    _writeConfigToJSON();
    _setTitleWithAccount();
    _setAccountsMenu();
}

void MainWindow::_changeAccount(const QString &name)
{
    _config.currentAccount = name;

    _setTitleWithAccount();
    _setCurrentAccount();
    _writeConfigToJSON();
    _resetUI();
    _listBucket();
}

void MainWindow::_deleteAccount()
{
    QMessageBox::StandardButton button = QMessageBox::warning(this,
                                                              "警告",
                                                              "确认删除帐户 " + _config.currentAccount + " ？\n\n删除帐户不会影响 NOS 中的数据。",
                                                              QMessageBox::Cancel|QMessageBox::Ok,
                                                              QMessageBox::Cancel);

    if (button == QMessageBox::Ok)
    {
        int index = -1;

        for (int i = 0; i < _config.accounts.length(); ++i)
        {
            if (_config.accounts[i].name == _config.currentAccount)
            {
                index = i;
                break;
            }
        }

        _config.accounts.removeAt(index);

        _resetUI();

        if (_config.accounts.length() > 0)
        {
            _currentAccount = _config.accounts.first();
            _config.currentAccount = _currentAccount.name;

            _client->setAccount(_currentAccount);
            _cdn->setAccount(_currentAccount);

            _listBucket();
        }
        else
        {
            _currentAccount = _emptyAccount;
            _config.currentAccount = "";

            _client->resetAccount();
            _cdn->resetAccount();

            _openAccountWindow("new");
        }

        _writeConfigToJSON();
        _setTitleWithAccount();
        _setAccountsMenu();
    }
}

void MainWindow::_changeSkipOlder(bool skipOlder)
{
    if (skipOlder) _skipAction->setChecked(true);
    else _notSkipAction->setChecked(true);

    _config.skipOlder = skipOlder;
}

void MainWindow::_changeDomain(const QString &name)
{
    _currentDomain = name;
}

void MainWindow::_changeBucket(const QString &bucket)
{
    if (!_isReady) return;

    _resetUI(false);

    _client->setBucket(bucket);

    ListObjectParams params;

    _listObject(params);

    for (int i = 0; i < _bucketList->layout()->count(); ++i)
    {
        QLayoutItem *item = _bucketList->layout()->itemAt(i);

        QPushButton *button = qobject_cast<QPushButton*>(item->widget());

        QString className = "bucket";
        className += (button->text() == bucket ? " selected": "");

        button->setProperty("class", className);
    }
}

void MainWindow::_uploadFileClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    QString openPath = _lastOpenPath;

    if (_lastOpenPath.isEmpty()) openPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString filePath = QFileDialog::getOpenFileName(this,
                                                    "选择文件",
                                                    openPath,
                                                    "",
                                                    nullptr,
                                                    QFileDialog::ReadOnly|QFileDialog::DontResolveSymlinks);

    if (filePath.isEmpty()) return;

    qDebug() << "filePath:" << filePath;

    QFile file(filePath);
    QFileInfo fileInfo(file);

    qDebug() << "fileInfo.fileName():" << fileInfo.fileName();
    qDebug() << "fileInfo.dir():" << fileInfo.dir().path();

    QString objectKey = _paths.last() + fileInfo.fileName();

    if (_config.skipOlder) _headObject(objectKey, filePath, fileInfo.size());
    else _addPutObjectTask(objectKey, filePath, fileInfo.size());

    _setLastOpenPath(fileInfo.dir().path());
}

void MainWindow::_uploadDirClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    QString openPath = _lastOpenPath;

    if (_lastOpenPath.isEmpty()) openPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString dirPath = QFileDialog::getExistingDirectory(this, "选择文件夹", openPath, QFileDialog::ShowDirsOnly);

    if (dirPath.isEmpty()) return;

    qDebug() << "dirPath:" << dirPath;

    _uploadDir(dirPath);
    _setLastOpenPath(dirPath);
}

void MainWindow::_downloadObjectClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    QVector<int> selectedRows = _objectTable->currentRows();

    if (selectedRows.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先选择需要下载的文件/夹");

        return;
    }

    QString openPath = _lastOpenPath;

    if (_lastOpenPath.isEmpty()) openPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);

    QString downloadDirPath = QFileDialog::getExistingDirectory(this, "选择文件夹", openPath, QFileDialog::ShowDirsOnly);

    if (downloadDirPath.isEmpty()) return;

    if (!downloadDirPath.endsWith("/")) downloadDirPath += "/";

    qDebug() << "downloadDirPath:" << downloadDirPath;

    _setLastOpenPath(downloadDirPath);

    QString pathAtDownload = _paths.last();

    qDebug() << "pathAtDownload:" << pathAtDownload;

    for (int row : selectedRows)
    {
        QTableWidgetItem *item = _objectTable->item(row, 0);

        if (item == nullptr) continue;

        QString objectKey = _paths.last() + item->text();

        if (objectKey.endsWith("/"))
        {            
            QHash<QString, DirAction>::const_iterator ci = _dirActions.find(objectKey);

            if (ci != _dirActions.end())
            {
                QMessageBox::warning(this, "警告", "当前文件夹正在操作，请稍后重试！");

                return;
            }

            _isReady = false;

            QStringHash dirOptions = {
                { "downloadDirPath", downloadDirPath },
                { "pathAtDownload", pathAtDownload }
            };

            DirAction dirAction(downloadDir, dirOptions);

            _dirActions.insert(objectKey, dirAction);

            ListObjectParams params(objectKey, "", "");

            _listObject(params);
        }
        else
        {
            QString filePath = objectKey;
            filePath.replace(filePath.indexOf(pathAtDownload), pathAtDownload.size(), "");

            _addDownloadObjectTask(objectKey, downloadDirPath + filePath);
        }
    }
}

void MainWindow::_deleteObjectClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    QVector<int> selectedRows = _objectTable->currentRows();

    if (selectedRows.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请先选择需要删除的文件/夹");

        return;
    }

    QString message = "确认删除 ";

    if (selectedRows.length() == 1)
    {
        QTableWidgetItem *item = _objectTable->item(selectedRows.first(), 0);

        if (item == nullptr) return;

        QString objectKey = _paths.last() + item->text();

        message += _paths.last() + item->text() + " ？";
    }
    else
        message += QString::number(selectedRows.length()) + " 个文件/夹？";

    QMessageBox::StandardButton button = QMessageBox::warning(this,
                                                              "警告",
                                                              message,
                                                              QMessageBox::Cancel|QMessageBox::Ok,
                                                              QMessageBox::Cancel);

    if (button != QMessageBox::Ok) return;

    for (int row : selectedRows)
    {
        QTableWidgetItem *item = _objectTable->item(row, 0);

        if (item == nullptr) continue;

        QString objectKey = _paths.last() + item->text();

        qDebug() << "objectKey:" << objectKey;

        if (objectKey.endsWith("/"))
        {
            QHash<QString, DirAction>::const_iterator ci = _dirActions.find(objectKey);

            if (ci != _dirActions.end())
            {
                QMessageBox::warning(this, "警告", "当前文件夹正在操作，请稍后重试！");

                return;
            }

            _isReady = false;

            DirAction dirAction(deleteDir);

            _dirActions.insert(objectKey, dirAction);

            ListObjectParams params(objectKey, "", "");

            _listObject(params);
        }
        else
            _addDeleteObjectTask(objectKey);

        _objectWriteMutex.lock();
        _objectItemHash.insert(objectKey, item);
        _objectWriteMutex.unlock();
    }
}

void MainWindow::_newDirClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    bool okClicked;

    QString dirName = QInputDialog::getText(this,
                                            "新建文件夹",
                                            "请输入文件夹名称",
                                            QLineEdit::Normal,
                                            "",
                                            &okClicked,
                                            Qt::MSWindowsFixedSizeDialogHint);

    if (okClicked)
    {
        if (dirName.isEmpty())
            QMessageBox::warning(this, "警告", "请输入文件夹名称！");
        else
        {
            qDebug() << "dirName:" << dirName;

            QString objectKey = _paths.last() + dirName;

            if (!objectKey.endsWith("/")) objectKey += "/";

            _addPutObjectTask(objectKey, "", 0);
        }
    }
}

void MainWindow::_refreshListClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    _refreshButton->setEnabled(false);

    _listCurrentObject();
}

void MainWindow::_copyObjectClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    if (_objectTable->currentRow() < 0) {
        QMessageBox::warning(this, "警告", "请先选择需要复制的文件/夹");

        return;
    }

    TransferWindow *tw = new TransferWindow(this);

    tw->setFixedSize(600, 400);
    tw->setWindowFlags(tw->windowFlags() & ~(Qt::WindowMinMaxButtonsHint|Qt::WindowMaximizeButtonHint|Qt::WindowCloseButtonHint));
    tw->setWindowModality(Qt::ApplicationModal);
    tw->setAttribute(Qt::WA_DeleteOnClose);
    tw->setWindowTitle("复制文件");

    tw->show();

    connect(tw, &TransferWindow::transferPathSelected, this, &MainWindow::_transferPathSelected);
//    connect(tw, &TransferWindow::transferCanceled, this, &MainWindow::_transferCanceled);

    tw->setAccount(_currentAccount);
    tw->setBucket(_client->getBucket());
    tw->setDirMode("copy");
}

void MainWindow::_moveObjectClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    if (_objectTable->currentRow() < 0) {
        QMessageBox::warning(this, "警告", "请先选择移动复制的文件/夹");

        return;
    }

    TransferWindow *tw = new TransferWindow(this);

    tw->setFixedSize(600, 400);
    tw->setWindowFlags(tw->windowFlags() & ~(Qt::WindowMinMaxButtonsHint|Qt::WindowMaximizeButtonHint|Qt::WindowCloseButtonHint));
    tw->setWindowModality(Qt::ApplicationModal);
    tw->setAttribute(Qt::WA_DeleteOnClose);
    tw->setWindowTitle("移动文件");

    tw->show();

    connect(tw, &TransferWindow::transferPathSelected, this, &MainWindow::_transferPathSelected);
//    connect(tw, &TransferWindow::transferCanceled, this, &MainWindow::_transferCanceled);

    tw->setAccount(_currentAccount);
    tw->setBucket(_client->getBucket());
    tw->setDirMode("move");
}

void MainWindow::_renameObjectClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    int row = _objectTable->currentRow();

    if (row < 0) {
        QMessageBox::warning(this, "警告", "请先选择需要重命名的文件/夹");

        return;
    }

    QTableWidgetItem *item = _objectTable->item(row, 0);

    QString sourceObjectKey = _paths.last() + item->text();

    bool okClicked;

    QString newName = QInputDialog::getText(this,
                                            "重命名",
                                            "请输入新名称",
                                            QLineEdit::Normal,
                                            item->text(),
                                            &okClicked,
                                            Qt::MSWindowsFixedSizeDialogHint);

    if (okClicked)
    {
        if (newName.isEmpty())
            QMessageBox::warning(this, "警告", "请输入新名称！");
        else
        {
            qDebug() << "newName:" << newName;

            QString destinationObjectKey = _paths.last() + newName;

            if (sourceObjectKey.endsWith("/") && !destinationObjectKey.endsWith("/")) destinationObjectKey += "/";

            if (!sourceObjectKey.endsWith("/") && destinationObjectKey.endsWith("/"))
            {
                QMessageBox::warning(this, "警告", "文件类型不能重命名为文件夹类型！");

                return;
            }

            if (sourceObjectKey.endsWith("/"))
            {
                QHash<QString, DirAction>::const_iterator ci = _dirActions.find(sourceObjectKey);

                if (ci != _dirActions.end())
                {
                    QMessageBox::warning(this, "警告", "当前文件夹正在操作，请稍后重试！");

                    return;
                }

                _isReady = false;

                QStringHash dirOptions = {
                    { "transferDirPath", destinationObjectKey }
                };

                DirAction dirAction(moveDir, dirOptions);

                _dirActions.insert(sourceObjectKey, dirAction);

                ListObjectParams params(sourceObjectKey, "", "");

                _listObject(params);
            }
            else
                _addMoveObjectTask(sourceObjectKey, destinationObjectKey);

            _objectWriteMutex.lock();
            _objectItemHash.insert(sourceObjectKey, item);
            _objectWriteMutex.unlock();
        }
    }
}

void MainWindow::_copyPathClicked()
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    QClipboard *clipboard = QApplication::clipboard();

    QString url;

    if (!_currentDomain.isEmpty()) url = "https://" + _currentDomain;

    url += "/" + _paths.last();

    int row = _objectTable->currentRow();

    if (row > -1)
    {
        QTableWidgetItem *item = _objectTable->item(row, 0);

        if (item != 0) url += item->text();
    }

    clipboard->setText(url);

    QMessageBox::information(this, "提示", "复制路径成功！");
}

void MainWindow::_refreshCacheClicked()
{
    if (_currentDomain.isEmpty())
    {
        QMessageBox::warning(this, "警告", "请先前往网页创建加速域名！");

        return;
    }

    QString url = "https://" + _currentDomain + "/" + _paths.last();

    QVector<int> selectedRows = _objectTable->currentRows();

    QString message;

    if (selectedRows.length() > 1)
    {
        message = QString::number(selectedRows.length()) + " 个文件/夹";
    }
    else if (selectedRows.length() == 1)
    {
        QTableWidgetItem *item = _objectTable->item(selectedRows.first(), 0);

        if (item == nullptr) message = url;
        else message = url + item->text();
    }

    QMessageBox::StandardButton button = QMessageBox::warning(this,
                                                              "确认刷新缓存",
                                                              message,
                                                              QMessageBox::Cancel|QMessageBox::Ok,
                                                              QMessageBox::Cancel);

    if (button != QMessageBox::Ok) return;

    PurgeParams params = { .domain = _currentDomain, .files = {}, .dirs = {} };

    if (selectedRows.isEmpty())
    {
        if (url.endsWith("/")) params.dirs.append(url);
        else params.files.append(url);
    }
    else
    {
        for (int row : selectedRows)
        {
            QTableWidgetItem *item = _objectTable->item(row, 0);

            if (item == nullptr) continue;

            if (item->text().endsWith("/")) params.dirs.append(url + item->text());
            else params.files.append(url + item->text());
        }
    }

//    qDebug() << "params:" << params;
    _purge(params);
}

void MainWindow::_handleDropEvent(const QStringList &dropPaths)
{
    qDebug() << "enter handleDropEvent, dropPaths:" << dropPaths;

    foreach (QString dropPath, dropPaths)
    {
        qDebug() << "dropPath:" << dropPath;

        QFileInfo fileInfo(dropPath);

        if (fileInfo.isDir()) _uploadDir(dropPath);
        else if (fileInfo.isFile())
        {
            QString objectKey = _paths.last() + fileInfo.fileName();

            qDebug() << "objectKey:" << objectKey;
            qDebug() << "fileInfo.size():" << fileInfo.size();

            if (_config.skipOlder) _headObject(objectKey, dropPath, fileInfo.size());
            else _addPutObjectTask(objectKey, dropPath, fileInfo.size());
        }
    }
}

void MainWindow::_selectedRowsChange()
{
    if (_objectTable->currentRows().isEmpty())
    {
        _downloadButton->setEnabled(false);
        _deleteButton->setEnabled(false);
    }
    else
    {
        _downloadButton->setEnabled(true);
        _deleteButton->setEnabled(true);
    }
}

void MainWindow::_transferPathSelected(const QString &dirMode, const QString &path)
{
    qDebug() << "transferPathSelected path:" << path;

    QTableWidgetItem *item = _objectTable->item(_objectTable->currentRow(), 0);

    QString sourceObjectKey = _paths.last() + item->text();
    QString destinationObjectKey = path + item->text();

    qDebug() << QString("mode: %1, sourceObjectKey: %2, destinationObjectKey: %3")
                .arg(dirMode, sourceObjectKey, destinationObjectKey);

    if (sourceObjectKey.endsWith("/"))
    {
        QHash<QString, DirAction>::const_iterator ci = _dirActions.find(sourceObjectKey);

        if (ci != _dirActions.end())
        {
            QMessageBox::warning(this, "警告", "当前文件夹正在操作，请稍后重试！");

            return;
        }

        _isReady = false;

        QStringHash dirOptions = {
            { "transferDirPath", destinationObjectKey }
        };

        DirAction dirAction((dirMode == "move" ? moveDir : copyDir), dirOptions);

        _dirActions.insert(sourceObjectKey, dirAction);

        ListObjectParams params(sourceObjectKey, "", "");

        _listObject(params);

        if (dirMode == "move")
        {
            _objectWriteMutex.lock();
            _objectItemHash.insert(sourceObjectKey, item);
            _objectWriteMutex.unlock();
        }
    }
    else
    {
        if (dirMode == "copy") _addCopyObjectTask(sourceObjectKey, destinationObjectKey);

        if (dirMode == "move")
        {
            _addMoveObjectTask(sourceObjectKey, destinationObjectKey);

            _objectWriteMutex.lock();
            _objectItemHash.insert(sourceObjectKey, item);
            _objectWriteMutex.unlock();
        }
    }
}

//void MainWindow::_transferCanceled()
//{

//}

void MainWindow::_pathClicked(int index)
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;
    if (index == _currentPathIndex) return;

//    qDebug() << "pathClicked:" << index;

    _currentPathIndex = index;

    QString prefix = _paths[index];

//    qDebug() << "prefix:" << prefix;

    _paths = _paths.mid(0, index + 1);

//    qDebug() << "paths:" << paths;

    _updatePathButtons();

    ListObjectParams params(prefix);

    _listObject(params);
}

void MainWindow::_sortByColumn(int column)
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;
    if (_objectTable->rowCount() == 0) return;

    qDebug() << "sortByColumn column:" << column;

    _objectTable->setCurrentItem(nullptr);

    if (_lastSortColumn == column)
    {
        if (_lastSortOrder == Qt::AscendingOrder) _lastSortOrder = Qt::DescendingOrder;
        else _lastSortOrder = Qt::AscendingOrder;
    }
    else
    {
        _lastSortColumn = column;

        if (_lastSortColumn == 0) _lastSortOrder = Qt::AscendingOrder;
        else _lastSortOrder = Qt::DescendingOrder;
    }

    _objectTable->horizontalHeader()->setSortIndicator(_lastSortColumn, _lastSortOrder);

    qDebug() << "sortByColumn order:" << _lastSortOrder;

    _setLastSortOrder();
    _resortObjects();
}

void MainWindow::_cellDoubleClicked(int row, int column)
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    qDebug() << "cellDoubleClicked, row:" << row << "column:" << column;

    QTableWidgetItem *item = _objectTable->item(row, column);

    if (item->text().endsWith("/"))
    {
        qDebug() << "item->text():" << item->text();

        _currentPathIndex = _paths.length();

        _paths.append(_paths.last() + item->text());

        _updatePathButtons();

        qDebug() << "_paths:" << _paths;

        ListObjectParams params(_paths.last());

        _listObject(params);
    }
    else
    {
        if (_currentDomain.isEmpty())
        {
            QMessageBox::warning(this, "警告", "请前往网页配置加速域名后再尝试预览文件");

            return;
        }

        QString url = "https://" + _currentDomain + "/" + _paths.last() + item->text();

        QDesktopServices::openUrl(QUrl(url));
    }
}

void MainWindow::_showObjectTableMenu(const QPoint &)
{
    if (!_isReady) return;
    if (_client->getBucket().isEmpty()) return;

    int selectedRowCount = _objectTable->currentRows().length();

    QMenu *menu = new QMenu(_objectTable);

    if (selectedRowCount > 0)
    {
        if (selectedRowCount == 1)
        {
            QAction *copyAction = new QAction("复制到");
            QAction *moveAction = new QAction("移动到");
            QAction *renameAction = new QAction("重命名");

            connect(copyAction, &QAction::triggered, this, &MainWindow::_copyObjectClicked);
            connect(moveAction, &QAction::triggered, this, &MainWindow::_moveObjectClicked);
            connect(renameAction, &QAction::triggered, this, &MainWindow::_renameObjectClicked);

            menu->addAction(copyAction);
            menu->addAction(moveAction);
            menu->addAction(renameAction);
        }

        QAction *downloadAction = new QAction("下载");
        connect(downloadAction, &QAction::triggered, this, &MainWindow::_downloadObjectClicked);
        menu->addAction(downloadAction);

        QAction *deleteAction = new QAction("删除");
        connect(deleteAction, &QAction::triggered, this, &MainWindow::_deleteObjectClicked);
        menu->addAction(deleteAction);

        if (selectedRowCount == 1)
        {
            QAction *copyPathAction = new QAction("复制路径");
            connect(copyPathAction, &QAction::triggered, this, &MainWindow::_copyPathClicked);
            menu->addAction(copyPathAction);
        }

        QAction *refreshCacheAction = new QAction("刷新缓存");
        connect(refreshCacheAction, &QAction::triggered, this, &MainWindow::_refreshCacheClicked);
        menu->addAction(refreshCacheAction);
    }
    else
    {
        QAction *noAction = new QAction("请选择需要操作的文件/夹");
        noAction->setEnabled(false);

        menu->addAction(noAction);
    }

    menu->move(cursor().pos());
    menu->exec();

    menu->deleteLater();
    menu = nullptr;
}

void MainWindow::_updateTaskCount()
{
    qDebug() << "enter updateTaskCount";

    int jobCount = _jobQueue->count();
    int taskCount = _workQueue->count();

    qDebug() << "jobCount:" << jobCount;
    qDebug() << "taskCount:" << taskCount;

    if (taskCount == 0 && _taskTimer->isActive()) _taskTimer->stop();

    _taskCountLabel->setText("当前任务: " + QString::number(jobCount + taskCount));
}

void MainWindow::_updateTaskbarProgress()
{
    qDebug() << "enter updateTaskbarProgress";

    if (_doneTaskCount >= _totalTaskCount)
    {
        _doneTaskCount = 0;
        _totalTaskCount = 0;

        _progressTimer->stop();
        _winTaskbarProgress->setValue(0);
        _setTitleWithAccount();

        return;
    }

    qDebug() << "doneTaskCount:" << _doneTaskCount;
    qDebug() << "totalTaskCount:" << _totalTaskCount;

    int progress = _doneTaskCount * 100 / _totalTaskCount;

    if (progress > 100) progress = 100;

    qDebug() << "updateTaskbarProgress progress:" << progress;

    if (_winTaskbarProgress->value() > progress) return;

    _winTaskbarProgress->setValue(progress);

    QString windowTitle = QString::number(progress) + "% 上传中 - NOS Client";

    if (!_config.currentAccount.isEmpty()) windowTitle += " - " + _config.currentAccount;

    setWindowTitle(windowTitle);
}

void MainWindow::_listBucketResponse(QNetworkReply::NetworkError error, const QStringList &buckets)
{
    qDebug() << "receive listBucketResponse";

    if (error != QNetworkReply::NoError)
    {
        QMessageBox::warning(this, "警告", "获取桶列表失败");

        return;
    }

    qDebug() << "buckets:" << buckets;

    if (buckets.length() > 0)
    {
        _client->setBucket(buckets.first());

        ListObjectParams params;

        _listObject(params);

        QWidget *parentWidget = _bucketList->parentWidget();

        for (int i = 0; i < buckets.length(); ++i)
        {
            QString bucket = buckets.at(i);

            QPushButton *button = new QPushButton(bucket, parentWidget);

            QString className = "bucket";
            className += (i == 0 ? " selected": "");

            button->setProperty("class", className);

#if 1
            // TODO: 由于开发功能时，当前账号不允许新建桶，因此这里并没有实际测试过
            connect(button, &QPushButton::clicked, [this, bucket] {
                _changeBucket(bucket);
            });
#endif

            _bucketList->addWidget(button);
        }
    }
}

void MainWindow::_listObjectResponse(QNetworkReply::NetworkError error,
                                     const QStringHash &params,
                                     const QStringVector &dirs,
                                     const QVector<File> &files)
{
    qDebug() << "receive MainWindow listObjectResponse";

    if (error != QNetworkReply::NoError)
    {
        _dirActions.remove(params["prefix"]);
        _transferFiles.remove(params["prefix"]);

        QMessageBox::warning(this, "警告", "获取对象列表失败");

        return;
    }

    qDebug() << "params:" << params;
    qDebug() << "dirs:" << dirs;
    qDebug() << "files:" << files;

    qDebug() << "_paths:" << _paths;

    QHash<QString, DirAction>::const_iterator ci = _dirActions.find(params["prefix"]);

    if (ci == _dirActions.end())
    {
        _storeDirs += dirs;
        _storeDirs.removeOne(_paths.last());
        _storeFiles += files;
    }
    else
    {
        QHash<QString, QVector<File>>::iterator fi = _transferFiles.find(params["prefix"]);

        if (fi == _transferFiles.end()) _transferFiles.insert(params["prefix"], files);
        else fi.value() += files;
    }

    if (params["isTruncated"] == "true")
    {
        ListObjectParams listParams(params["prefix"], params["nextMarker"]);

        if (ci != _dirActions.end()) listParams.delimiter = "";

        _listObject(listParams);

        return;
    }

    if (ci != _dirActions.end())
    {
        DirAction dirAction = ci.value();

        qDebug() << "files:" << _transferFiles[params["prefix"]];

        QString prefix;

        if (dirAction.dirMode == moveDir || dirAction.dirMode == copyDir) prefix = params["prefix"];
        if (dirAction.dirMode == downloadDir) prefix = dirAction.dirOptions["pathAtDownload"];

        foreach (auto file, _transferFiles[params["prefix"]])
        {
            QString objectKey = file.key;
            QString filePath = objectKey.replace(objectKey.indexOf(prefix), prefix.size(), "");

            switch (dirAction.dirMode)
            {
            case deleteDir:
//                qDebug() << "DELETE:" << file.key;
                _addDeleteObjectTask(file.key);
                break;
            case moveDir:
//                qDebug() << "MOVE:" << file.key << " => " << dirAction.dirOptions["transferDirPath"] + filePath;
                _addMoveObjectTask(file.key, dirAction.dirOptions["transferDirPath"] + filePath);
                break;
            case copyDir:
//                qDebug() << "COPY:" << file.key << " => " << dirAction.dirOptions["transferDirPath"] + filePath;
                _addCopyObjectTask(file.key, dirAction.dirOptions["transferDirPath"] + filePath);
                break;
            case downloadDir:
//                qDebug() << "DOWNLOAD:" << file.key << " => " << dirAction.dirOptions["downloadDirPath"] + filePath;
                _addDownloadObjectTask(file.key, dirAction.dirOptions["downloadDirPath"] + filePath);
                break;
            }
        }

        _isReady = true;

        return;
    }

    _resortObjects();

    int dirCount = _storeDirs.count();
    int fileCount = _storeFiles.count();
    int totalCount = dirCount + fileCount;

    while (_objectTable->rowCount() > totalCount)
        _objectTable->removeRow(totalCount);

    _objectTable->toggleRowHidden(false);

    if (_objectRowCount != _objectTable->rowCount())
    {
        _objectRowCount = _objectTable->rowCount();

        _objectTable->setFirstColumnWidth(width(), height());
    }

    _refreshButton->setEnabled(true);

    _updateTotal();
}

void MainWindow::_getObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QByteArray &data)
{
    qDebug() << "receive getObjectResponse";
    qDebug() << "params:" << params;

    QString taskName = params["objectKey"];
    QString filePath = params["filePath"];

    QString msg = "NOS " + params["objectKey"] + " => 本地 " + filePath;

    if (error != QNetworkReply::NoError && !taskName.endsWith("/"))
    {
        _updateTask("下载", taskName, "失败");

        _log(Client::getObjectOperation, failure, msg);
    }
    else
    {
        QStringList filePaths = filePath.split("/");
        filePaths.pop_back();

        qDebug() << "filePaths:" << filePaths;

        QString dirPath = filePaths.join("/");
        QDir dir(dirPath);

        qDebug() << "dirPath:" << dirPath;

        if (!dir.exists())
        {
            bool isOK = dir.mkpath(dirPath);

            if (!isOK)
            {
                _updateTask("下载", taskName, "失败");

                _log(Client::getObjectOperation, failure, "创建文件夹 " + dirPath + " 失败");

                ++_doneTaskCount;

                _workQueue->pop();

                _perform();

                _checkWorkDone();

                return;
            }
        }

        QFile file(filePath);

        if (file.open(QIODevice::WriteOnly))
        {
            file.write(data);
            file.close();
        }

        _removeTask(taskName);

        _log(Client::getObjectOperation, success, msg);
    }

    if (taskName.endsWith("/"))
    {
        _dirActions.remove(taskName);
        _transferFiles.remove(taskName);
    }

    ++_doneTaskCount;

    _workQueue->pop();

    _perform();

    _checkWorkDone();
}

void MainWindow::_headObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers)
{
    qDebug() << "receive headObjectResponse";
    qDebug() << "params:" << params;
    qDebug() << "headers:" << headers;

    QString objectKey = params["objectKey"];

    if (error != QNetworkReply::NoError && error != QNetworkReply::ContentNotFoundError)
    {
        QMessageBox::warning(this, "警告", "获取 " + objectKey + " 信息失败");

        return;
    }

    QString filePath = params["filePath"];
    qint64 fileSize = params["fileSize"].toLongLong();

    qDebug() << "fileSize:" << fileSize;

    QString lastModified = headers["Last-Modified"];

    if (lastModified.isEmpty())
    {
        _addPutObjectTask(objectKey, filePath, fileSize);

        return;
    }

    QFileInfo fileInfo(filePath);

    uint localTime = fileInfo.lastModified().toTime_t();

    lastModified.replace(" GMT", "");

    QLocale locale = QLocale::English;
    QString format = "ddd, dd MMM yyyy HH:mm:ss";
    QDateTime nosUploaded = locale.toDateTime(lastModified, format);
    nosUploaded.setTimeSpec(Qt::UTC);

    uint nosTime = nosUploaded.toTime_t();

    if (localTime > nosTime)
        _addPutObjectTask(objectKey, filePath, fileSize);
    else
        _log(Client::headObjectOperation, success, "本地 " + filePath + " => NOS " + objectKey);
}

void MainWindow::_putObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers)
{
    qDebug() << "receive putObjectResponse";

    qDebug() << "params:" << params;
    qDebug() << "headers:" << headers;

    QString objectKey = params["objectKey"];
    QString filePath = params["filePath"];
    QString fileSize = params["fileSize"];

    QString msg = filePath.isEmpty() ? "NOS " + objectKey : "本地 " + filePath + " => NOS " + objectKey;

    if (error != QNetworkReply::NoError)
    {
       _updateTask("上传", objectKey, "失败");
       _log(Client::putObjectOperation, failure, msg);
    }
    else
    {
        _removeTask(objectKey);
        _log(Client::putObjectOperation, success, msg);
    }

    _removeUpload(objectKey, fileSize);

    ++_doneTaskCount;

    _workQueue->pop();

    _perform();

    _checkWorkDoneAndReload();
}

void MainWindow::_deleteObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params)
{
    qDebug() << "receive deleteObjectResponse";

    QString objectKey = params["objectKey"];
    QString msg = "NOS " + objectKey;

    if (error != QNetworkReply::NoError)
    {
       _updateTask("删除", objectKey, "失败");
       _log(Client::deleteObjectOperation, failure, msg);
    }
    else
    {
        _removeTask(objectKey);
        _log(Client::deleteObjectOperation, success, msg);
    }

    if (objectKey.endsWith("/"))
    {
        _dirActions.remove(objectKey);
        _transferFiles.remove(objectKey);
    }

    _removeObject(objectKey);

    ++_doneTaskCount;

    _workQueue->pop();

    _perform();

    _objectTable->setCurrentItem(nullptr);

    _checkWorkDone();
}

void MainWindow::_copyObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params)
{
    qDebug() << "receive copyObjectResponse";
    qDebug() << "params:" << params;

    QString sourceObjectKey = params["sourceObjectKey"];
    QString destinationObjectKey = params["destinationObjectKey"];
    QString taskName = sourceObjectKey + " => " + destinationObjectKey;
    QString msg = "NOS " + sourceObjectKey + " => NOS " + destinationObjectKey;

    if (error != QNetworkReply::NoError)
    {
       _updateTask("复制", taskName, "失败");
       _log(Client::copyObjectOperation, failure, msg);
    }
    else
    {
        _removeTask(taskName);
        _log(Client::copyObjectOperation, success, msg);
    }

    if (sourceObjectKey.endsWith("/"))
    {
        _dirActions.remove(sourceObjectKey);
        _transferFiles.remove(sourceObjectKey);
    }

    ++_doneTaskCount;

    _workQueue->pop();

    _perform();

    _checkWorkDone();
}

void MainWindow::_moveObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params)
{
    qDebug() << "receive moveObjectResponse";
    qDebug() << "params:" << params;

    QString sourceObjectKey = params["sourceObjectKey"];
    QString destinationObjectKey = params["destinationObjectKey"];
    QString taskName = sourceObjectKey + " => " + destinationObjectKey;
    QString msg = "NOS " + sourceObjectKey + " => NOS " + destinationObjectKey;

    if (error != QNetworkReply::NoError)
    {
       _updateTask("移动", taskName, "失败");
       _removeObject(sourceObjectKey);

       _log(Client::moveObjectOperation, failure, msg);
    }
    else
    {
        _removeTask(taskName);

        if (sourceObjectKey.split("/").size() == destinationObjectKey.split("/").size())
        {
            // 排除掉这种特殊情况
            if (!destinationObjectKey.contains(sourceObjectKey))
            {
                QString path = _paths.last();
                _updateObject(sourceObjectKey, destinationObjectKey.replace(destinationObjectKey.indexOf(path), path.size(), ""));
            }
        }
        else
        {
            _objectTable->setCurrentItem(nullptr);
            _removeObject(sourceObjectKey);
        }

        _log(Client::moveObjectOperation, success, msg);
    }

    if (sourceObjectKey.endsWith("/"))
    {
        _dirActions.remove(sourceObjectKey);
        _transferFiles.remove(sourceObjectKey);
    }

    ++_doneTaskCount;

    _workQueue->pop();

    _perform();

    _checkWorkDone();
}

void MainWindow::_updateProgressResponse(Client::Operation operation,
                                         const QStringHash &params,
                                         const QStringHash &extras,
                                         qint64 bytesSent)
{
    qDebug() << "receive updateProgress";
    qDebug() << "operation:" << operation;
    qDebug() << "params:" << params;
    qDebug() << "extras:" << extras;
    qDebug() << "bytesSent:" << bytesSent;

    if (operation != Client::putObjectOperation && operation != Client::uploadPartOperation) return;
    if (bytesSent == 0) return;

    QString part = operation == Client::uploadPartOperation ? extras["part"] : "";

    _updateProgress(params["objectKey"], part, bytesSent, extras["fileSize"].toLongLong());
}

// CDN
void MainWindow::_listDomainResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Domain> &domains)
{
    qDebug() << "receive MainWindow listDomainResponse";

    if (error != QNetworkReply::NoError)
    {
        QMessageBox::warning(this, "警告", "获取加速域名列表失败");

        return;
    }

    qDebug() << "isTruncated:" << isTruncated;
    qDebug() << "quantity:" << quantity;
    qDebug() << "domains:" << domains;

    _domains = domains;

    _setCDNMenu();
}

void MainWindow::_purgeResponse(QNetworkReply::NetworkError error, const QString &id)
{
    qDebug() << "receive MainWindow purgeResponse";

    if (error != QNetworkReply::NoError)
    {
        QMessageBox::warning(this, "警告", "创建缓存刷新请求失败");

        return;
    }

    qDebug() << "id:" << id;

    QMessageBox::information(this, "提示", "创建缓存刷新请求成功！ID: " + id);
}

void MainWindow::_errorResponse(const QString &message)
{
//    qDebug() << "receive errorResponse";
//    qDebug() << "message:" << message;

    QMessageBox::warning(this, "警告", message);
}
