#ifndef CLIENT_H
#define CLIENT_H

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

typedef struct file
{
    QString key;
    quint64 size;
    QString lastModified;

    friend QDebug operator<<(QDebug stream, const file &f)
    {
        stream << QString("File(key: %1, size: %2, lastModified: %3)").arg(f.key, QString::number(f.size), f.lastModified);

        return stream;
    }
} File;

typedef struct listObjectParams
{
    QString prefix;
    QString marker;
    QString delimiter;
    QString maxKeys;

    listObjectParams(
        const QString &pPrefix = "",
        const QString &pMarker = "",
        const QString &pDelimiter = "/",
        const QString &pMaxKeys = "1000"
    ) : prefix(pPrefix),
        marker(pMarker),
        delimiter(pDelimiter),
        maxKeys(pMaxKeys) {}

    friend QDebug operator<<(QDebug stream, const listObjectParams &lop)
    {
        stream << QString("ListObjectParams(prefix: %1, marker: %2, delimiter: %3, maxKeys: %4)")
                  .arg(lop.prefix, lop.marker, lop.delimiter, lop.maxKeys);

        return stream;
    }
} ListObjectParams;

typedef struct getObjectParams
{
    QString objectKey;
    QString filePath;
    QString range;
    QString ifModifiedSince;
    QString download;
    QString ifNotFound;

    getObjectParams(
        const QString &pObjectKey = "",
        const QString &pFilePath = "",
        const QString &pRange = "",
        const QString &pIfModifiedSince = "",
        const QString &pDownload = "",
        const QString &pIfNotFound = ""
    ) : objectKey(pObjectKey),
        filePath(pFilePath),
        range(pRange),
        ifModifiedSince(pIfModifiedSince),
        download(pDownload),
        ifNotFound(pIfNotFound) {}
} GetObjectParams;

Q_DECLARE_METATYPE(GetObjectParams);

typedef struct headObjectParams
{
    QString objectKey;
    QString filePath;
    qint64 fileSize;
    QString ifModifiedSince;

    headObjectParams(
        const QString &pObjectKey = "",
        const QString &pFilePath = "",
        qint64 pFileSize = 0,
        const QString &pIfModifiedSince = ""
    ) : objectKey(pObjectKey),
        filePath(pFilePath),
        fileSize(pFileSize),
        ifModifiedSince(pIfModifiedSince) {}
} HeadObjectParams;

typedef struct
{
    QString objectKey;
    QString filePath;
    qint64 fileSize;
} PutObjectParams, PutBigObjectParams;

Q_DECLARE_METATYPE(PutObjectParams);

typedef struct deleteObjectParams
{
    QString objectKey;

    friend QDebug operator<<(QDebug stream, const deleteObjectParams &dop)
    {
        stream << QString("DeleteObjectParams(objectKey: %1)").arg(dop.objectKey);

        return stream;
    }
} DeleteObjectParams;

Q_DECLARE_METATYPE(DeleteObjectParams);

typedef struct
{
    QStringList objectKeys;
} DeleteObjectsParams;

Q_DECLARE_METATYPE(DeleteObjectsParams);

typedef struct transferObjectParams
{
    QString sourceBucketName;
    QString sourceObjectKey;
    QString destinationBucketName;
    QString destinationObjectKey;

    transferObjectParams(
        const QString &pSourceBucketName = "",
        const QString &pSourceObjectKey = "",
        const QString &pDestinationBucketName = "",
        const QString &pDestinationObjectKey = ""
    ) : sourceBucketName(pSourceBucketName),
        sourceObjectKey(pSourceObjectKey),
        destinationBucketName(pDestinationBucketName),
        destinationObjectKey(pDestinationObjectKey) {}
} CopyObjectParams, MoveObjectParams;

Q_DECLARE_METATYPE(CopyObjectParams);

typedef struct
{
    QString objectKey;
    QByteArray body;
    int partNumber;
    QString uploadId;
    QStringHash extras;
} UploadPartParams;

Q_DECLARE_METATYPE(UploadPartParams);

typedef struct
{
    QString objectKey;
    QString uploadId;
    QMap<int, QString> parts;
    QStringHash extras;
} CompleteMultipartUploadParams;

Q_DECLARE_METATYPE(CompleteMultipartUploadParams);

typedef struct listMultipartUploadsParams
{
    QString keyMarker;
    QString maxUploads;

    listMultipartUploadsParams(const QString pKeyMarker = "", const QString pMaxUploads = "1000") :
        keyMarker(pKeyMarker), maxUploads(pMaxUploads) {}
} ListMultipartUploadsParams;

typedef struct
{
    QString objectKey;
    QString uploadId;
} AbortMultipartUploadParams;

typedef struct listPartsParams
{
    QString objectKey;
    QString uploadId;
    QString maxParts;
    QString partNumberMarker;

    listPartsParams(
        const QString &pObjectKey = "",
        const QString &pUploadId = "",
        const QString &pMaxParts = "1000",
        const QString &pPartNumberMarker = ""
    ) : objectKey(pObjectKey),
        uploadId(pUploadId),
        maxParts(pMaxParts),
        partNumberMarker(pPartNumberMarker) {}
} ListPartsParams;

class Client : public QObject
{
    Q_OBJECT

public:
    enum Action {
        listBucketAction,
        bucketAction,
        objectAction
    };

    enum Operation
    {
        noOperation,                            // 0
        listBucketOperation,                    // 1
        listObjectOperation,                    // 2
        getObjectOperation,                     // 3
        headObjectOperation,                    // 4
        putObjectOperation,                     // 5
        deleteObjectOperation,                  // 6
        deleteObjectsOperation,                 // 7
        copyObjectOperation,                    // 8
        moveObjectOperation,                    // 9
        initiateMultipartUploadOperation,       // 10
        uploadPartOperation,                    // 11
        completeMultipartUploadOperation,       // 12
        abortMultipartUploadOperation,          // 13
        listMultipartUploadsOperation,          // 14
        listPartsOperation                      // 15
    };

    typedef struct job
    {
        Operation operation;
        QVariant params;

        job(Operation pOperation, const QVariant &pParams) : operation(pOperation), params(pParams) {}
    } Job;

    static const qint64 PartSize = 10485760; // 10M

    static const QString humanReadableSize(const quint64 &size, int precision);

    explicit Client();
    ~Client();

    void setAccount(const Account &account);
    void setBucket(const QString &bucket);
    QString getBucket() const;
    void resetAccount();

    const QString encodeObjectKey(const QString &objectKey) const;

public slots:
    void listBucket();
    void listObject(const ListObjectParams &params);
    void getObject(const GetObjectParams &params);
    void headObject(const HeadObjectParams &params);
    void putObject(const PutObjectParams &params);
    void putBigObject(const PutBigObjectParams &params);
    void deleteObject(const DeleteObjectParams &params);
    void deleteObjects(const DeleteObjectsParams &params);
    void copyObject(const CopyObjectParams &params);
    void moveObject(const CopyObjectParams &params);
    void initiateMultipartUpload(const PutBigObjectParams &params);
    void uploadPart(const UploadPartParams &params);
    void completeMultipartUpload(const CompleteMultipartUploadParams &params);
    void listMultipartUploads(const ListMultipartUploadsParams &params);
    void abortMultipartUpload(const AbortMultipartUploadParams &params);
    void listParts(const ListPartsParams &params);

signals:
    void listBucketResponse(QNetworkReply::NetworkError error, const QStringList &buckets);
    void listObjectResponse(QNetworkReply::NetworkError error,
                            const QStringHash &params,
                            const QStringVector &dirs,
                            const QVector<File> &files);
    void getObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QByteArray &data);
    void headObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers);
    void putObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers);
    void deleteObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);
    void deleteObjectsResponse(QNetworkReply::NetworkError error, const QStringHash &params, const QStringHash &headers);
    void copyObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);
    void moveObjectResponse(QNetworkReply::NetworkError error, const QStringHash &params);
    void listMultipartUploadsResponse(QNetworkReply::NetworkError error,
                                      const QStringHash &params,
                                      const QList<QHash<QString, QVariant>> &uploads);
    void abortMultipartUploadResponse(QNetworkReply::NetworkError error);
    void listPartsResponse(QNetworkReply::NetworkError error,
                           const QHash<QString, QVariant> &params,
                           const QList<QHash<QString, QVariant>> &parts);

    void updateProgressResponse(Operation operation, const QStringHash &params, const QStringHash &extras, qint64 bytesSent);

    void errorResponse(const QString &message);

private:
    const QString METHOD_GET = "GET";
    const QString METHOD_POST = "POST";
    const QString METHOD_PUT = "PUT";
    const QString METHOD_DELETE = "DELETE";
    const QString METHOD_HEAD = "HEAD";

    // 大小写都无所谓
    const QByteArray HEADER_DATE = "date";
    const QString HEADER_HOST = "host";
    const QByteArray HEADER_CONTENT_MD5 = "content-md5";
    const QString HEADER_CONTENT_LENGTH = "content-length";
    const QString HEADER_CONTENT_TYPE = "content-type";
    const QByteArray HEADER_AUTHORIZATION = "authorization";
    const QString HEADER_RANGE = "range";
    const QString HEADER_KEY_MARKER = "key-marker";
    const QString HEADER_MAX_UPLOADS = "max-uploads";
    const QString HEADER_IF_MODIFIED_SINCE = "if-modified-since";
    const QString HEADER_X_NOS_ENTITY_TYPE = "x-nos-entity-type";
    const QString HEADER_X_NOS_COPY_SOURCE = "x-nos-copy-source";
    const QString HEADER_X_NOS_MOVE_SOURCE = "x-nos-move-source";
    const QString HEADER_OBJECT_KEY = "object-key";

    Account _account;
    QString _bucket;

    QNetworkAccessManager *_manager;
    QThread *_thread;
    JobQueue<Client::Job> *_jobQueue;
    WorkerQueue<Client::Job> *_workQueue;

    // 区分操作
    QMap<QNetworkReply*, Operation> _operationMap;

    // 暂存数据
    QHash<QNetworkReply*, QStringHash> _objectHash;
    QHash<QNetworkReply*, QStringHash> _extraHash;
    QHash<QString, QMap<int, QString>> _partsHash;

    void _registerMetaType() const;

    void _work();

    QNetworkReply* _sendRequest(const QString &method,
                                const QStringHash &headers,
                                const QByteArray &body,
                                const Action &action,
                                const QStringHash &resources,
                                const Operation &operation,
                                const QStringHash &extras = QStringHash());

    void _signRequest(const QString &method,
                      QNetworkRequest &request,
                      const QStringMap &nosHeaders,
                      const Action &action,
                      const QStringHash &resources);

    void _getObject(const Job &job);
    void _putObject(const Job &job);
    void _deleteObject(const Job &job);
    void _deleteObjects(const Job &job);
    void _copyObject(const Job &job);
    void _moveObject(const Job &job);
    void _uploadPart(const Job &job);
    void _completeMultipartUpload(const Job &job);

    void _listBucketHandler(QNetworkReply *reply);
    void _listObjectHandler(QNetworkReply *reply);
    void _getObjectHandler(QNetworkReply *reply);
    void _headObjectHandler(QNetworkReply *reply);
    void _putObjectHandler(QNetworkReply *reply);
    void _deleteObjectHandler(QNetworkReply *reply);
    void _deleteObjectsHandler(QNetworkReply *reply);
    void _copyObjectHandler(QNetworkReply *reply);
    void _moveObjectHandler(QNetworkReply *reply);
    void _initiateMultipartUploadHandler(QNetworkReply *reply);
    void _uploadPartHandler(QNetworkReply *reply);
    void _completeMultipartUploadHandler(QNetworkReply *reply);
    void _listMultipartUploadsHandler(QNetworkReply *reply);
    void _abortMultipartUploadHandler(QNetworkReply *reply);
    void _listPartsHandler(QNetworkReply *reply);

private slots:
    void _requestFinished(QNetworkReply *reply);
};

#endif // CLIENT_H
