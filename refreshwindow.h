#ifndef REFRESHWINDOW_H
#define REFRESHWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QPushButton;
class QTableWidget;
class QHBoxLayout;
class QShowEvent;
class QCloseEvent;
QT_END_NAMESPACE

#include "cdn.h"

#include "account.h"

class RefreshWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit RefreshWindow(QWidget *parent = nullptr);
    ~RefreshWindow();

    void setAccount(const Account &account);
    void setDomain(const QString &domain);

signals:
    void listPurge(const ListPurgeParams &params);

protected:
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    CDN *_cdn;
    QString _domain;
    int _page = 1;
    QStringHash _statusText;
    QHash<QString, QColor> _statusColor;
    bool _inRequest = false;

    QPushButton *_refreshButton;
    QTableWidget *_refreshTable;
    QHBoxLayout *_paginationLayout;

    void _buildUI();
    void _connectSlots() const;
    void _updatePagination(int quantity);

    void _listPurge();

private slots:
    void _listPurgeResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Purge> &purges);

    void _cellEntered(int row, int column);
    void _refreshButtonClicked();
    void _pageClicked(const QString &page);
};

#endif // REFRESHWINDOW_H
