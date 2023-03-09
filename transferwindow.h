#ifndef TRANSFERWINDOW_H
#define TRANSFERWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QIcon;
QT_END_NAMESPACE

#include "client.h"

#include "account.h"

class TransferWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit TransferWindow(QWidget *parent = nullptr);
    ~TransferWindow();

    void setAccount(const Account &account);
    void setBucket(const QString &bucket);
    void setDirMode(const QString &dirMode);

signals:
    void listObject(const ListObjectParams &params);

    void transferPathSelected(const QString &dirMode, const QString &path);
//    void transferCanceled();

private:
    Client *_client;

    QString _dirMode;
    QString _path = "";
    QStringVector _storeDirs;

    QLabel *_currentPathLabel;
    QTreeWidget *_tree;
    QPushButton *_saveButton;
    QPushButton *_cancelButton;
    QIcon _icon;

    void _buildUI();
    void _connectSlots() const;

    void _listObject(const ListObjectParams &params);

private slots:
    void _listObjectResponse(QNetworkReply::NetworkError error,
                             const QStringHash &params,
                             const QStringVector &dirs,
                             const QVector<File> &files);
    void _itemClicked(QTreeWidgetItem *item, int column);
    void _saveButtonClicked();
    void _cancelButtonClicked();
};

#endif // TRANSFERWINDOW_H
