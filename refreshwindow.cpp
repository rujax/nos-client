#include "refreshwindow.h"

#include <QLayout>
#include <QPushButton>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QDateTime>
#include <QToolTip>

// Constructor
RefreshWindow::RefreshWindow(QWidget *parent) : QMainWindow(parent), _cdn(new CDN)
{
    qDebug() << "enter RefreshWindow::RefreshWindow";

    _buildUI();
    _connectSlots();

    _statusText = {
        { "Success", "刷新完成" },
        { "InProgress", "刷新中" },
        { "Fail", "刷新失败" }
    };

    _statusColor = {
        { "Success", QColor(28, 184, 65) },
        { "InProgress", QColor(66, 184, 221) },
        { "Fail", QColor(202, 60, 60) }
    };
}

// Destructor
RefreshWindow::~RefreshWindow()
{
    qDebug() << "Execute RefreshWindow::~RefreshWindow()";

    _cdn->deleteLater();
    _cdn = nullptr;
}

// Public Methods
void RefreshWindow::setAccount(const Account &account)
{
    _cdn->setAccount(account);
}

void RefreshWindow::setDomain(const QString &domain)
{
    _domain = domain;
}

// Protected Methods
void RefreshWindow::showEvent(QShowEvent *event)
{
    qDebug() << "enter showEvent";

    QMainWindow::showEvent(event);

    _listPurge();
}

void RefreshWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "enter closeEvent";

    QMainWindow::closeEvent(event);

    this->~RefreshWindow();
}

// Private Methods
void RefreshWindow::_buildUI()
{
    QWidget *container = new QWidget(this);
    this->setCentralWidget(container);

    QVBoxLayout *vbLayout = new QVBoxLayout(container);
    vbLayout->setSpacing(0);
    vbLayout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(0);
    topLayout->setContentsMargins(0, 0, 0, 0);

    _refreshTable = new QTableWidget(0, 4, container);
    _refreshTable->setObjectName("refresh-table");
    _refreshTable->setColumnWidth(0, 100);
    _refreshTable->setColumnWidth(2, 80);
    _refreshTable->setColumnWidth(3, 160);
    _refreshTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    _refreshTable->setSelectionMode(QAbstractItemView::NoSelection);
    _refreshTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    _refreshTable->setFocusPolicy(Qt::NoFocus);
    _refreshTable->setMouseTracking(true);

    QStringList refreshHeaderLabels = { "刷新 ID", "刷新 URL", "状态", "创建时间" };

    _refreshTable->setHorizontalHeaderLabels(refreshHeaderLabels);

    QHeaderView *taskTableHeader = _refreshTable->horizontalHeader();

    taskTableHeader->setSectionResizeMode(0, QHeaderView::Fixed);
    taskTableHeader->setSectionResizeMode(1, QHeaderView::Stretch);
    taskTableHeader->setSectionResizeMode(2, QHeaderView::Fixed);
    taskTableHeader->setSectionResizeMode(3, QHeaderView::Fixed);
    taskTableHeader->setSectionsClickable(false);

    _refreshTable->verticalHeader()->hide();

    topLayout->addWidget(_refreshTable);

    QHBoxLayout *bottomLayout = new QHBoxLayout;
    bottomLayout->setContentsMargins(5, 5, 5, 5);

    _refreshButton = new QPushButton("刷新列表", this);
    _refreshButton->setObjectName("refresh-list-button");
    _refreshButton->setFixedSize(120, 30);

    bottomLayout->addWidget(_refreshButton, 0, Qt::AlignLeft);

    QWidget *pagination = new QWidget(container);
    pagination->setObjectName("pagination");
    pagination->setFixedHeight(30);

    _paginationLayout = new QHBoxLayout(pagination);
    _paginationLayout->setSpacing(0);
    _paginationLayout->setContentsMargins(0, 0, 0, 0);
    _paginationLayout->setAlignment(Qt::AlignCenter);

    bottomLayout->addWidget(pagination, 1, Qt::AlignRight);

    vbLayout->addLayout(topLayout);
    vbLayout->addLayout(bottomLayout);

    container->setLayout(vbLayout);
}

void RefreshWindow::_connectSlots() const
{
    connect(this, &RefreshWindow::listPurge, _cdn, &CDN::listPurge, Qt::QueuedConnection);
    connect(_cdn, &CDN::listPurgeResponse, this, &RefreshWindow::_listPurgeResponse, Qt::QueuedConnection);

    connect(_refreshTable, &QTableWidget::cellEntered, this, &RefreshWindow::_cellEntered);
    connect(_refreshButton, &QPushButton::clicked, this, &RefreshWindow::_refreshButtonClicked);
}

void RefreshWindow::_updatePagination(int quantity)
{
    QLayoutItem *item;

    while ((item = _paginationLayout->takeAt(0)) != 0)
    {
        if (item->widget())
        {
            item->widget()->disconnect();
            item->widget()->setParent(nullptr);
        }

        delete item;
    }

    int per = 20;
    int maxPage;

    if (quantity < per) return;

    if (quantity % per == 0) maxPage = quantity / per;
    else maxPage = quantity / per + 1;

    QStringVector pages;

    if (_page > 1) pages.append("prev");
    if (_page - 2 > 0) pages.append(QString::number(_page - 2));
    if (_page - 1 > 0) pages.append(QString::number(_page - 1));
    pages.append(QString::number(_page));
    if (_page + 1 <= maxPage) pages.append(QString::number(_page + 1));
    if (_page + 2 <= maxPage) pages.append(QString::number(_page + 2));
    if (_page < maxPage) pages.append("next");

    QWidget *parentWidget = _paginationLayout->parentWidget();

    foreach (auto page, pages)
    {
        QPushButton *button = new QPushButton(parentWidget);

        if (page == "prev")
            button->setIcon(QIcon(":/prev-20.png"));
        else if (page == "next")
            button->setIcon(QIcon(":/next-20.png"));
        else
            button->setText(page);

        QString className = "pagination-button";
        className += (page == QString::number(_page) ? " current": "");

        button->setProperty("class", className);

        connect(button, &QPushButton::clicked, [this, page] {
            _pageClicked(page);
        });

        _paginationLayout->addWidget(button);
    }
}

void RefreshWindow::_listPurge()
{
    qDebug() << "enter _listPurge";

    for (int rowIndex = 0; rowIndex < _refreshTable->rowCount(); ++rowIndex)
        _refreshTable->setRowHidden(rowIndex, true);

    ListPurgeParams params(_domain, _page);

    qDebug() << "_domain:" << _domain;

    emit listPurge(params);
}

// Private Slots
void RefreshWindow::_listPurgeResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Purge> &purges)
{
    qDebug() << "receive RefreshWindow listPurgeResponse";

    Q_UNUSED(isTruncated);

    if (error != QNetworkReply::NoError)
    {
        _refreshButton->setEnabled(true);

        _inRequest = false;

        QMessageBox::warning(this, "警告", "获取缓存刷新列表失败");

        return;
    }

//    qDebug() << "isTruncated:" << isTruncated;
//    qDebug() << "quantity:" << quantity;
//    qDebug() << "purges:" << purges;

    int totalRowCount = _refreshTable->rowCount();

    for (int row = 0; row < purges.length(); ++row)
    {
        int currentRow = row;

        if (currentRow >= totalRowCount) _refreshTable->insertRow(currentRow);

        auto purge = purges.at(row);

        for (int col = 0; col < _refreshTable->columnCount(); ++col)
        {
            QString text;

            switch (col)
            {
            case 0: text = purge.id; break;
            case 1: text = purge.urls.join("\n"); break;
            case 2: text = _statusText[purge.status]; break;
            case 3: text = QDateTime::fromTime_t(purge.createTime / 1000).toString("yyyy-MM-dd hh:mm:ss"); break;
            }

            QTableWidgetItem *item = _refreshTable->item(currentRow, col);

            if (!item)
            {
                item = new QTableWidgetItem;

                _refreshTable->setItem(currentRow, col, item);
            }

            if (col != 1) item->setTextAlignment(Qt::AlignCenter);
            if (col == 2) item->setForeground(QBrush(_statusColor[purge.status]));

            item->setText(text);
        }
    }

    int totalCount = purges.size();

    while (_refreshTable->rowCount() > totalCount)
        _refreshTable->removeRow(totalCount);

    for (int rowIndex = 0; rowIndex < _refreshTable->rowCount(); ++rowIndex)
        _refreshTable->setRowHidden(rowIndex, false);

    _refreshButton->setEnabled(true);

    _updatePagination(quantity);

    _inRequest = false;
}

void RefreshWindow::_cellEntered(int row, int column)
{
    if (column != 1) return;

    QTableWidgetItem *item = _refreshTable->item(row, column);

    if (item->text().contains("\n")) QToolTip::showText(QCursor::pos(), item->text());
    else if (QToolTip::isVisible()) QToolTip::hideText();
}

void RefreshWindow::_refreshButtonClicked()
{
    _refreshButton->setEnabled(false);

    _listPurge();
}

void RefreshWindow::_pageClicked(const QString &page)
{
    if (_inRequest) return;

    _inRequest = true;

    if (page == "prev") --_page;
    else if (page == "next") ++_page;
    else _page = page.toInt();

    _listPurge();
}
