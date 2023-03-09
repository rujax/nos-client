#ifndef CDN_H
#define CDN_H

#include <QObject>
#include <QNetworkReply>

QT_BEGIN_NAMESPACE
class QNetworkAccessManager;
class QNetworkRequest;
class QThread;
QT_END_NAMESPACE

#include "account.h"

#include "jobqueue.h"
#include "workerqueue.h"

#include "qstringmap.h"
#include "qstringhash.h"
#include "qstringvector.h"

typedef struct domain
{
    QString productID;
    QString id;
    QString name;
    QString serviceType;
    QString serviceAreas;
    QString originType;
    QStringVector originIPs;
    QString defaultOriginHostHeader;
    QString comment;
    QString workStatus;
    QString status;
    QString cname;
    quint64 createTime;
    quint64 updateTime;

    friend QDebug operator<<(QDebug stream, const domain &d)
    {
        stream << QString("Domain(id: %1, name: %2, workStatus: %3, status: %4)").arg(d.id, d.name, d.workStatus, d.status);

        return stream;
    }
} Domain;

typedef struct purge
{
    QString id;
    QStringList urls;
    QString status;
    quint64 createTime;

    friend QDebug operator<<(QDebug stream, const purge &p)
    {
        stream << QString("Purge(id: %1, status: %2, createTime: %3, urls: [%4])")
                  .arg(p.id, p.status, QString::number(p.createTime), p.urls.join(", "));

        return stream;
    }
} Purge;

typedef struct purgeStatus
{
    QString path;
    QString status;
    int rate;

    friend QDebug operator<<(QDebug stream, const purgeStatus &ps)
    {
        stream << QString("PurgeStatus(path: %1, status: %2, rate: %3").arg(ps.path, ps.status, QString::number(ps.rate));

        return stream;
    }
} PurgeStatus;

typedef struct listDomainParams
{
    QString sortField;
    QString orderType;
    QString serviceType;
    QString status;
    QString workStatus;
    int pageNum;
    int pageSize;

    listDomainParams(
        const QString &pSortField = "update_time",
        const QString &pOrderType = "desc",
        const QString &pServiceType = "all",
        const QString &pStatus = "deployed",
        const QString &pWorkStatus = "enabled",
        int pPageNum = 1,
        int pPageSize = 20
    ) : sortField(pSortField),
        orderType(pOrderType),
        serviceType(pServiceType),
        status(pStatus),
        workStatus(pWorkStatus),
        pageNum(pPageNum),
        pageSize(pPageSize) {}
} ListDomainParams;

Q_DECLARE_METATYPE(ListDomainParams);

typedef struct purgeParams
{
    QString domain;
    QStringVector files;
    QStringVector dirs;

    friend QDebug operator<<(QDebug stream, const purgeParams &p)
    {
        stream << QString("PurgeParams(domain: %1, files: [%2], dirs: [%3]")
                  .arg(p.domain, p.files.toList().join(","), p.dirs.toList().join(","));

        return stream;
    }
} PurgeParams;

Q_DECLARE_METATYPE(PurgeParams);

typedef struct
{
    QString domain;
    QString id;
} GetPurgeStatusParams;

Q_DECLARE_METATYPE(GetPurgeStatusParams);

typedef struct listPurgeParams
{
    QString domain;
    int pageNum;
    int pageSize;

    listPurgeParams(const QString &pDomain = "", int pPageNum = 1, int pPageSize = 20) :
        domain(pDomain), pageNum(pPageNum), pageSize(pPageSize) {}
} ListPurgeParams;

class CDN : public QObject
{
    Q_OBJECT

public:
    enum Operation
    {
        noOperation,                    // 0
        listDomainOperation,            // 1
        purgeOperation,                 // 2
        getPurgeStatusOperation,        // 3
        listPurgeOperation,             // 4
#if 0
        preheatOperation,               // 5
        listPreheatOperation,           // 6
        getPreheatQuotaOperation,       // 7
#endif
    };

    typedef struct job
    {
        Operation operation;
        QVariant params;

        job(Operation pOperation, const QVariant &pParams) : operation(pOperation), params(pParams) {}
    } Job;

    explicit CDN();
    ~CDN();

    void setAccount(const Account &account);
    void resetAccount();

public slots:
    void listDomain(const ListDomainParams &params);
    void purge(const PurgeParams &params);
    void getPurgeStatus(const GetPurgeStatusParams &params);
    void listPurge(const ListPurgeParams &params);

signals:
    void listDomainResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Domain> &domains);
    void purgeResponse(QNetworkReply::NetworkError error, const QString &id);
    void getPurgeStatusResponse(QNetworkReply::NetworkError error, const QVector<PurgeStatus> &purgeStatuses);
    void listPurgeResponse(QNetworkReply::NetworkError error, bool isTruncated, int quantity, const QVector<Purge> &purges);

    void errorResponse(const QString &message);

private:
    const QString METHOD_GET = "GET";
    const QString METHOD_POST = "POST";
    const QString METHOD_PUT = "PUT";

    const QByteArray HEADER_DATE = "date";
    const QByteArray HEADER_HOST = "host";
    const QByteArray HEADER_ACCEPT = "accept";
    const QByteArray HEADER_CONTENT_MD5 = "content-md5";
    const QString HEADER_CONTENT_LENGTH = "content-length";
    const QString HEADER_CONTENT_TYPE = "content-type";
    const QByteArray HEADER_AUTHORIZATION = "authorization";

    Account _account;

    QNetworkAccessManager *_manager;
    QThread *_thread;
    JobQueue<Job> *_jobQueue;
    WorkerQueue<Job> *_workQueue;

    QHash<QNetworkReply*, Operation> _operationHash;

    void _registerMetaType() const;

    void _work();

    QNetworkReply* _sendRequest(const QString &method,
                                const QByteArray &body,
                                const QStringHash &resources,
                                const Operation &operation);

    void _signRequest(const QString &method,
                      QNetworkRequest &request,
                      const Operation &operation,
                      const QStringHash &resources);

    void _purge(const Job &job);
#if 0
    void _preheat(const Job &job);
#endif

    void _listDomainHandler(QNetworkReply *reply);
    void _purgeHandler(QNetworkReply *reply);
    void _getPurgeStatusHandler(QNetworkReply *reply);
    void _listPurgeHandler(QNetworkReply *reply);
#if 0
    void _preheatHandler(QNetworkReply *reply);
    void _listPreheatHandler(QNetworkReply *reply);
    void _getPreheatQuotaHandler(QNetworkReply *reply);
#endif

private slots:
    void _requestFinished(QNetworkReply *reply);
};

#endif // CDN_H
