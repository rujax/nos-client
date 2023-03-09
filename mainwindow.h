#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QString;
class QStringList;
template <typename T> class QVector;
template <class Key, class T> class QHash;
class QHBoxLayout;
class QVBoxLayout;
class QTableWidget;
class QTableWidgetItem;
class QMenu;
class QAction;
class QActionGroup;
class QPushButton;
class QKeyEvent;
//class QDragEnterEvent;
//class QDropEvent;
class QEvent;
class QLabel;
class QTimer;
class QSettings;
class QWinTaskbarButton;
class QWinTaskbarProgress;
QT_END_NAMESPACE

#include "qstringmap.h"
#include "qstringhash.h"
#include "qstringvector.h"

#include "client.h"
#include "cdn.h"

#include "account.h"
#include "config.h"

class Logger;
class OTableWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setTaskbarHandle();

signals:
    void writeLog(const QString &bucket, const QString &action, const QString &status, const QString &msg);
    void listBucket();
    void listObject(const ListObjectParams &params);
    void headObject(const HeadObjectParams &params);
    void putObject(const PutObjectParams &params);
    void getObject(const GetObjectParams &params);
    void deleteObject(const DeleteObjectParams &params);
    void copyObject(const CopyObjectParams &params);
    void moveObject(const MoveObjectParams &params);
    void listDomain(const ListDomainParams &params);
    void purge(const PurgeParams &params);

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void changeEvent(QEvent *event) override;
#if 0
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
#endif

private:
    enum Status
    {
        success,
        failure
    };

    enum DirMode
    {
        moveDir,
        copyDir,
        deleteDir,
        downloadDir,
    };

    typedef struct task
    {
        Client::Operation operation;
        QVariant params;

        task(Client::Operation pOperation, const QVariant &pParams) : operation(pOperation), params(pParams) {}
    } Task;

    typedef struct dirAction
    {
        DirMode dirMode;
        QStringHash dirOptions;

        dirAction(
                DirMode pDirMode,
                QStringHash pDirOptions = QStringHash()
        ) : dirMode(pDirMode),
            dirOptions(pDirOptions) {}
    } DirAction;

    Client *_client;
    CDN *_cdn;
    Logger *_logger;

    Config _config;
    Account _currentAccount;
    Account _emptyAccount; // 重置 _currentAccount 时使用

    JobQueue<Task> *_jobQueue;
    WorkerQueue<Task> *_workQueue;

    // Data
    QStringList _paths = { "" };
    QStringVector _storeDirs;

    QVector<File> _storeFiles;
    QVector<Domain> _domains;

    QHash<QString, QTableWidgetItem*> _objectItemHash;
    QHash<QString, QTableWidgetItem*> _taskItemHash;
    QHash<QString, qint64> _uploadBytesHash;
    QHash<QString, DirAction> _dirActions;
    QHash<QString, QVector<File>> _transferFiles;

    QMutex _objectReadMutex;
    QMutex _objectWriteMutex;
    QMutex _taskReadMutex;
    QMutex _taskWriteMutex;

    Qt::SortOrder _lastSortOrder = Qt::AscendingOrder;

    bool _isReady = false;

    int _currentPathIndex = 0;
    int _objectRowCount = 0;
    int _lastSortColumn = 0;
    int _doneTaskCount = 0;
    int _totalTaskCount = 0;

    QString _editMode = "new";
    QString _lastOpenPath = "";
    QString _currentDomain = "";

    QTimer *_taskTimer;
    QTimer *_progressTimer;

    QSettings *_settings;

    // UI
    QWidget *_container;

    QVBoxLayout *_bucketList;
    QHBoxLayout *_pathsButtons;

    OTableWidget *_objectTable;
    QTableWidget *_taskTable;

    QMenu *_accountMenu;
    QMenu *_cdnMenu;
    QMenu *_helpMenu;

    QActionGroup *_accountGroup;
    QActionGroup *_skipOlderGroup;
    QActionGroup *_cdnGroup;

    QAction *_newAccountAction;
    QAction *_editAccountAction;
    QAction *_deleteAccountAction;
    QAction *_notSkipAction;
    QAction *_skipAction;
    QAction *_listDomainAction;
    QAction *_listCacheAction;
    QAction *_aboutAction;

    QPushButton *_uploadFileButton;
    QPushButton *_uploadDirButton;
    QPushButton *_downloadButton;
    QPushButton *_deleteButton;
    QPushButton *_newDirButton;
    QPushButton *_refreshButton;

    QLabel *_objectCountLabel;
    QLabel *_taskCountLabel;

    QWinTaskbarButton *_winTaskbarButton;
    QWinTaskbarProgress *_winTaskbarProgress;

    void _initUI();
    void _initMenu();
    void _connectSlots();
    bool _initAccount();
    void _setTitleWithAccount();
    void _setAccountsMenu();
    void _resetUI(bool withBucket = true);
    void _setCDNMenu();

    void _openAccountWindow(const QString &mode);
    void _openRefreshWindow();
    void _setCurrentAccount();
    void _writeConfigToJSON();
    void _updatePathButtons();
    const QString _getSetting(const QString &key) const;
    void _setLastOpenPath(const QString &path);
    void _setLastSortOrder();
    void _log(Client::Operation operation, Status status, const QString &msg);
    void _perform();
    void _resortObjects();
    void _sortDirs(QStringVector &dirs);
    void _sortFiles(QVector<File> &files);
    void _listDirs(int startRow, int totalRowCount);
    void _listFiles(int startRow, int totalRowCount);

    void _listBucket();
    void _listObject(const ListObjectParams &params);
    void _listCurrentObject();

    void _headObject(const QString &objectKey, const QString &filePath, qint64 fileSize);

    void _addPutObjectTask(const QString &objectKey, const QString &filePath, qint64 fileSize = 0);
    void _putObject(const Task &task);

    void _addDownloadObjectTask(const QString &objectKey, const QString &filePath);
    void _downloadObject(const Task &task);

    void _addDeleteObjectTask(const QString &objectKey);
    void _deleteObject(const Task &task);

    void _addCopyObjectTask(const QString &sourceObjectKey, const QString &destinationObjectKey);
    void _copyObject(const Task &task);

    void _addMoveObjectTask(const QString &sourceObjectKey, const QString &destinationObjectKey);
    void _moveObject(const Task &task);

    void _listDomain();
    void _purge(const PurgeParams &params);

    void _uploadDir(const QString &dirPath);

    void _insertTask(const QString &action, const QString &name, const QString &size, const QString &status);
    void _updateTask(const QString &action, const QString &name, const QString &status);
    void _removeTask(const QString &name);
    void _updateProgress(const QString &name, const QString &part, qint64 bytesSent, qint64 bytesTotal);
    void _updateObject(const QString &objectKey, const QString &name);
    void _removeObject(const QString &objectKey);
    void _removeUpload(const QString &objectKey, const QString &fileSize);
    void _updateTotal();
    void _checkWorkDone();
    void _checkWorkDoneAndReload();

private slots:
    void _receiveAccountData(const QStringHash &accountData);
    void _changeAccount(const QString &name);
    void _deleteAccount();
    void _changeSkipOlder(bool skipOlder);
    void _changeDomain(const QString &name);
    void _changeBucket(const QString &bucket);
    void _uploadFileClicked();
    void _uploadDirClicked();
    void _downloadObjectClicked();
    void _deleteObjectClicked();
    void _newDirClicked();
    void _refreshListClicked();
    void _copyObjectClicked();
    void _moveObjectClicked();
    void _renameObjectClicked();
    void _copyPathClicked();
    void _refreshCacheClicked();

    void _handleDropEvent(const QStringList &dropPaths);
    void _selectedRowsChange();
    void _transferPathSelected(const QString &dirMode, const QString &path);
//    void _transferCanceled();
    void _pathClicked(int index);
    void _sortByColumn(int column);
    void _cellDoubleClicked(int row, int column);
    void _showObjectTableMenu(const QPoint &pos);
    void _updateTaskCount();
    void _updateTaskbarProgress();

    // NOS
    void _listBucketResponse(QNetworkReply::NetworkError error, const QStringList &buckets);
    void _listObjectResponse(QNetworkReply::NetworkError error,
                             const QStringHash &params,
                             const QStringVector &dirs,
                             const QVector<File> &files);
    void _getObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QByteArray &data);
    void _headObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers);
    void _putObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers);
    void _deleteObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);
    void _copyObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);
    void _moveObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);

    void _updateProgressResponse(Client::Operation operation, const QStringHash &params, const QStringHash &extras, qint64 bytesSent);

    // CDN
    void _listDomainResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Domain> &domains);
    void _purgeResponse(QNetworkReply::NetworkError error, const QString &id);

    void _errorResponse(const QString &message);
};

#endif // MAINWINDOW_H
