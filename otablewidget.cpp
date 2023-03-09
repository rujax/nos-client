#include "otablewidget.h"

#include <QHeaderView>
#include <QDebug>
//#include <QMouseEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
//#include <QDrag>
#include <QPainter>
#include <QApplication>
#include <QMimeData>
#include <QFileInfo>
#include <QStringList>

// Constructor
OTableWidget::OTableWidget(int row, int column, QWidget *parent) : QTableWidget(row, column, parent)
{
    setMouseTracking(true);
    setAcceptDrops(true);

    setObjectName("object-table");

    setColumnWidth(0, 698);
    setColumnWidth(1, 100);
    setColumnWidth(2, 200);

    setSelectionBehavior(QAbstractItemView::SelectRows);
    setSelectionMode(QAbstractItemView::ExtendedSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setFocusPolicy(Qt::NoFocus);
    setContextMenuPolicy(Qt::CustomContextMenu);
    setFont(QFont("等线", 12));

    this->verticalHeader()->setDefaultSectionSize(36);

    QStringList objectHeaderLabels = { "文件", "大小", "上传时间" };

    setHorizontalHeaderLabels(objectHeaderLabels);

    QHeaderView *objectTableHeader = horizontalHeader();

    objectTableHeader->setSectionResizeMode(1, QHeaderView::Fixed);
    objectTableHeader->setSectionResizeMode(2, QHeaderView::Fixed);

    this->verticalHeader()->hide();

    connect(this, &QTableWidget::cellEntered, this, &OTableWidget::cellEntered);
}

// Public Methods
void OTableWidget::toggleRowHidden(bool hide)
{
    for (int rowIndex = 0; rowIndex < this->rowCount(); ++rowIndex)
        this->setRowHidden(rowIndex, hide);
}

void OTableWidget::setFirstColumnWidth(int width, int height)
{
    qDebug() << "setFirstColumnWidth width:" << width << ", height:" << height;
    qDebug() << "this->rowCount() * 36:" << this->rowCount() * 36; // 单行高度是 36（含下边框线）

    int scrollBarWidth = 0;

    // 24: 菜单栏高度, 36: 路径导航条高度, 30: 表头高度, 60: 操作按钮高度, 220: 任务列表高度
    if (this->rowCount() * 36 > height - 24 - 36 - 30 - 60 - 220) scrollBarWidth += 18; // 18: 滚动条宽度

    qDebug() << "scrollBarWidth:" << scrollBarWidth;

    setColumnWidth(0, width - 200 - 300 - 2 - scrollBarWidth);
}

QVector<int> OTableWidget::currentRows() const
{
    return _selectedRows;
}

void OTableWidget::clearSelectedRows()
{
    if (_selectedRows.isEmpty()) return;

    _selectedRows.clear();

    emit selectedRowsChanged();
}

// Public Slots
void OTableWidget::cellEntered(int row, int column)
{
//    qDebug() << "enter cellEntered, _previousRow:" << _previousRow;
//    qDebug() << "enter cellEntered, currentRow:" << row;

    QTableWidgetItem *item = nullptr;

    item = this->item(_previousRow, 0);

    if (item != nullptr) _setRowColor(_previousRow, _defaultColor);

    item = this->item(row, column);

    if (item != nullptr) _setRowColor(row, _hoverColor);

    _previousRow = row;
}

// Protected Methods
void OTableWidget::leaveEvent(QEvent *) // 只有鼠标离开 table 的区域时才会触发
{
//    qDebug() << "enter leaveEvent, _previousRow:" << _previousRow;

    QTableWidgetItem *item = this->item(_previousRow, 0);

    if (item) _setRowColor(_previousRow, _defaultColor);
}

void OTableWidget::mousePressEvent(QMouseEvent *event)
{
//    qDebug() << "enter mousePressEvent, currentRow:" << currentRow();

    QModelIndex rowIndex = indexAt(event->pos());

    if (event->button() == Qt::LeftButton)
    {
        _isPressRow = true;
        _dragStartPoint = event->pos();

        if (rowIndex.isValid())
        {
//            _dragText = item(rowIndex.row(), 0)->text();
//            _dragPointOffset = _dragPoint - QPoint(0, rowIndex.row() * 30);
        }
        else
        {
            for (int i = 0; i < rowCount(); ++i) _setRowColor(i, _defaultColor);

            if (!_selectedRows.isEmpty())
            {
                clearSelection();

                _selectedRows.clear();

                emit selectedRowsChanged();
            }
        }
    }

    if (event->button() == Qt::RightButton && rowIndex.isValid() && !_selectedRows.contains(rowIndex.row()))
    {
        clearSelection();
        _selectedRows.clear();
        _selectedRows.push_back(rowIndex.row());

        emit selectedRowsChanged();
    }

    QTableWidget::mousePressEvent(event);
}

void OTableWidget::mouseMoveEvent(QMouseEvent *event)
{
//    if (!_isPressRow) return;
//    if (!(event->buttons() & Qt::LeftButton)) return;
//    if ((event->pos() - _dragPoint).manhattanLength() < QApplication::startDragDistance()) return;

//    _drag();

//    _isPressRow = false;

    if (_isPressRow && event->buttons() == Qt::LeftButton)
    {
        _dragEndPoint = event->pos();

        viewport()->update();

        QModelIndex rowIndex = indexAt(event->pos());

        if (rowIndex.isValid()) selectRow(rowIndex.row());
    }

    QTableWidget::mouseMoveEvent(event);
}

void OTableWidget::mouseReleaseEvent(QMouseEvent *event)
{
//    qDebug() << "enter mouseReleaseEvent";

    QTableWidget::mouseReleaseEvent(event);

    _isPressRow = false;
    _dragStartPoint = QPoint();
    _dragEndPoint = QPoint();

    viewport()->update();

    int selectedRowsCount = _selectedRows.length();

    _selectedRows.clear();

    QModelIndexList selectedIndexes = this->selectedIndexes();

    QModelIndexList::const_iterator ci;

    for (ci = selectedIndexes.begin(); ci != selectedIndexes.end(); ++ci)
    {
        if (!_selectedRows.contains(ci->row())) _selectedRows.push_back(ci->row());
    }

//    qDebug() << "_selectedRows:" << _selectedRows;

    if (selectedRowsCount != _selectedRows.length()) emit selectedRowsChanged();
}

void OTableWidget::dragEnterEvent(QDragEnterEvent *event)
{
//    qDebug() << "enter dragEnterEvent event->mimeData():" << event->mimeData();

    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else
    {
        event->ignore();

        QTableWidget::dragEnterEvent(event);
    }
}

void OTableWidget::dragMoveEvent(QDragMoveEvent *event)
{
//    qDebug() << "enter dragMoveEvent event->mimeData():" << event->mimeData();

    if (event->mimeData()->hasUrls()) event->acceptProposedAction();
    else
    {
        event->ignore();

        QTableWidget::dragMoveEvent(event);
    }
}

void OTableWidget::dropEvent(QDropEvent *event)
{
    qDebug() << "enter dropEvent";

    QList<QUrl> urls = event->mimeData()->urls();

    if (urls.isEmpty()) return;

    qDebug() << "urls" << urls;

    if (urls.isEmpty()) return;

    QStringList dropPaths;

    foreach (QUrl url, urls)
    {
        QString dropPath = url.toLocalFile();

        if (dropPath.isEmpty()) continue;

        qDebug() << "dropPath:" << dropPath;

        dropPaths.append(dropPath);
    }

    if (!dropPaths.isEmpty())
        emit receiveDropEvent(dropPaths);
}

void OTableWidget::paintEvent(QPaintEvent *event)
{
    QTableWidget::paintEvent(event);

    if (_dragStartPoint.isNull() || _dragEndPoint.isNull()) return;

//    qDebug() << "enter draw rect";

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(QPen(QColor(0, 120, 231), 1));
    painter.setBrush(QColor(0, 120, 231, 60));

    QRect rect(_dragEndPoint, _dragStartPoint);

    painter.drawRect(rect);
}

// Private Methods
void OTableWidget::_setRowColor(int row, const QColor &color) const
{
    for (int column = 0; column < this->columnCount(); ++column)
    {
        QTableWidgetItem *item = this->item(row, column);

        if (item) item->setBackgroundColor(color);
    }
}

// 从客户端拖拽到本地：需要先创建一个临时文件，然后将该临时文件的绝对路径传给 mimeData
//void OTableWidget::_drag()
//{
//    qDebug() << "enter drag";

//    QDrag *drag = new QDrag(this);
//    QMimeData *mimeData = new QMimeData;
//    QFileInfo fileInfo("需要一个本地真实文件的地址");
//    QUrl url = QUrl::fromLocalFile(fileInfo.absoluteFilePath());
//    mimeData->setUrls(QList<QUrl>() << url);
//    drag->setMimeData(mimeData);
//    drag->setHotSpot(_dragPointOffset);

//    QPixmap dragImage(width(), 30);
//    dragImage.fill(QColor(255, 255, 255, 100));

//    QPainter painter(&dragImage);
//    painter.setPen(QColor(0, 0, 0, 200));
//    painter.drawText(QRectF(20, 0, width(), 30), _dragText, QTextOption(Qt::AlignVCenter));

//    drag->setPixmap(dragImage);

//    drag->exec(Qt::CopyAction);
//}
