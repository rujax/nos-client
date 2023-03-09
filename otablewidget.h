#ifndef OTABLEWIDGET_H
#define OTABLEWIDGET_H

#include <QTableWidget>

QT_BEGIN_NAMESPACE
class QEvent;
class QColor;
class QMouseEvent;
class QDragEnterEvent;
class QDragMoveEvent;
class QDropEvent;
class QPaintEvent;
//class QString;
class QPoint;
class QStringList;
QT_END_NAMESPACE

class OTableWidget : public QTableWidget
{
    Q_OBJECT

public:
    explicit OTableWidget(int rows, int columns, QWidget *parent = nullptr);

    void toggleRowHidden(bool hide);
    void setFirstColumnWidth(int width, int height);
    QVector<int> currentRows() const;
    void clearSelectedRows();

public slots:
    void cellEntered(int row, int column);

signals:
    void receiveDropEvent(const QStringList &dropPaths);
    void selectedRowsChanged();

protected:
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override; // 用来实现从客户端拖拽到本地
    void mouseMoveEvent(QMouseEvent *event) override; // 用来实现从客户端拖拽到本地
    void mouseReleaseEvent(QMouseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override; // 必须实现，否则不触发 dropEvent
    void dropEvent(QDropEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    QColor _defaultColor = QColor(0xff, 0xff, 0xff);
    QColor _hoverColor = QColor(0xfc, 0xf8, 0xe3);
    int _previousRow = -1;
    QVector<int> _selectedRows;

    bool _isPressRow = false;
//    QString _dragText;
    QPoint _dragStartPoint;
    QPoint _dragEndPoint;
//    QPoint _dragPointOffset;

    void _setRowColor(int row, const QColor &color) const;
//    void _drag();
};

#endif // OTABLEWIDGET_H
