#include "client.h"

#include <QMimeDatabase>
#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QDomDocument>
#include <QCoreApplication>
#include <QNetworkProxy>
#include <QFile>
#include <QMetaType>
#include <QThread>

// Static Methods
const QString Client::humanReadableSize(const quint64 &size, int precision)
{
    double sizeAsDouble = size;

    static QStringList measures;

    if (measures.isEmpty())
        measures << QCoreApplication::translate("QInstaller", "bytes")
                 << QCoreApplication::translate("QInstaller", "KiB")
                 << QCoreApplication::translate("QInstaller", "MiB")
                 << QCoreApplication::translate("QInstaller", "GiB")
                 << QCoreApplication::translate("QInstaller", "TiB")
                 << QCoreApplication::translate("QInstaller", "PiB")
                 << QCoreApplication::translate("QInstaller", "EiB")
                 << QCoreApplication::translate("QInstaller", "ZiB")
                 << QCoreApplication::translate("QInstaller", "YiB");

    QStringListIterator it(measures);
    QString measure(it.next());

    while (sizeAsDouble >= 1024.0 && it.hasNext())
    {
        measure = it.next();
        sizeAsDouble /= 1024.0;
    }

    QString sizeString = QString::fromLatin1("%1").arg(sizeAsDouble, 0, 'f', precision);

    if (sizeString[sizeString.length() - 1] == "0") sizeString.chop(1);
    if (sizeString[sizeString.length() - 1] == "0") sizeString.chop(1);
    if (sizeString[sizeString.length() - 1] == ".") sizeString.chop(1);

    return sizeString + " " + measure;
}

// Constructor
Client::Client() :
    _manager(new QNetworkAccessManager(this)),
    _thread(new QThread),
    _jobQueue(new JobQueue<Job>),
    _workQueue(new WorkerQueue<Job>(6))
{
//    qDebug() << "Client before Thread:" << this->thread();

    _registerMetaType();

    _manager->setProxy(QNetworkProxy::NoProxy);

    connect(_manager, &QNetworkAccessManager::finished, this, &Client::_requestFinished);

    connect(_thread, &QThread::finished, this, &QObject::deleteLater);

    this->moveToThread(_thread);

    _thread->start();

//    qDebug() << "Client after Thread:" << this->thread();
}

// Destructor
Client::~Client()
{
    qDebug() << "Execute Client::~Client()";

    _manager->disconnect();
    _manager->deleteLater();
    _manager = nullptr;

    _thread->quit();
    _thread->requestInterruption();
    _thread = nullptr;

    _workQueue->clear();
    delete _workQueue;
    _workQueue = nullptr;

    _jobQueue->clear();
    delete _jobQueue;
    _jobQueue = nullptr;
}

// Public Methods
void Client::setAccount(const Account &account)
{
//    qDebug() << "setAccount Thread:" << this->thread();

    _account = account;

    qDebug() << _account.endpoint;
    qDebug() << _account.accessKey;
    qDebug() << _account.accessSecret;
}

void Client::setBucket(const QString &bucket)
{
    _bucket = bucket;
}

QString Client::getBucket() const
{
    return _bucket;
}

void Client::resetAccount()
{
    _account.reset();

    qDebug() << "account reseted";
}

const QString Client::encodeObjectKey(const QString &objectKey) const
{
    QString encodeKey = QUrl::toPercentEncoding(objectKey);
    encodeKey.replace("~", "%7E").replace("%2F", "/");

    return encodeKey;
}

// Public Slots
void Client::listBucket()
{
//    qDebug() << "listBucket Thread:" << this->thread();

    qDebug() << "Invoke listBucket()";

    QStringHash headers = {{ HEADER_HOST, _account.endpoint }};

    QByteArray body;
    QStringHash resources;

    _sendRequest(METHOD_GET, headers, body, listBucketAction, resources, listBucketOperation);
}

void Client::listObject(const ListObjectParams &params)
{
    qDebug() << "Invoke listObject";

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QByteArray body;

    QStringHash resources = {{ "bucket", _bucket }};

    if (params.prefix != "") resources.insert("prefix", params.prefix);
    if (params.marker != "") resources.insert("marker", params.marker);
    if (params.delimiter != "") resources.insert("delimiter", params.delimiter);
    if (params.maxKeys != "") resources.insert("max-keys", params.maxKeys);

    qDebug() << "listObject resources:" << resources;

    _sendRequest(METHOD_GET, headers, body, bucketAction, resources, listObjectOperation);
}

void Client::getObject(const GetObjectParams &params)
{
    if (params.objectKey.isEmpty())
    {
        emit errorResponse("读取对象内容: objectKey 不得为空");

        return;
    }

    Job job(getObjectOperation, QVariant::fromValue<GetObjectParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::headObject(const HeadObjectParams &params)
{
    if (params.objectKey.isEmpty())
    {
        emit errorResponse("获取对象信息: objectKey 不得为空");

        return;
    }

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    if (params.ifModifiedSince != "") headers.insert(HEADER_IF_MODIFIED_SINCE, params.ifModifiedSince);

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) }
    };

    qDebug() << "headObject resources: " << resources;

    QStringHash extras = {
        { "objectKey", params.objectKey },
        { "filePath", params.filePath },
        { "fileSize", QString::number(params.fileSize) }
    };

    _sendRequest(METHOD_HEAD, headers, body, objectAction, resources, headObjectOperation, extras);
}

void Client::putObject(const PutObjectParams &params)
{
    if (params.objectKey.isEmpty())
    {
        emit errorResponse("上传对象至桶: objectKey 不得为空");

        return;
    }

    if (!params.objectKey.endsWith("/") && params.filePath.isEmpty())
    {
        emit errorResponse("上传对象至桶: filePath 不得为空");

        return;
    }

    Job job(putObjectOperation, QVariant::fromValue<PutObjectParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::putBigObject(const PutBigObjectParams &params)
{
    if (params.objectKey.isEmpty() || params.filePath.isEmpty())
    {
        emit errorResponse("上传大对象至桶: objectKey 或 filePath 不得为空");

        return;
    }

    initiateMultipartUpload(params);
}

void Client::deleteObject(const DeleteObjectParams &params)
{
    if (params.objectKey.isEmpty())
    {
        emit errorResponse("删除对象: objectKey 不得为空");

        return;
    }

    Job job(deleteObjectOperation, QVariant::fromValue<DeleteObjectParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::deleteObjects(const DeleteObjectsParams &params)
{
    if (params.objectKeys.isEmpty())
    {
        emit errorResponse("删除多个对象: objectKeys 不得为空");

        return;
    }

    Job job(deleteObjectsOperation, QVariant::fromValue<DeleteObjectsParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::copyObject(const CopyObjectParams &params)
{
    qDebug() << "Invoke copyObject";

    if (params.sourceBucketName.isEmpty() ||
            params.sourceObjectKey.isEmpty() ||
            params.destinationBucketName.isEmpty() ||
            params.destinationObjectKey.isEmpty())
    {
        emit errorResponse("拷贝对象: 参数缺失，请检查参数");

        return;
    }

    Job job(copyObjectOperation, QVariant::fromValue<CopyObjectParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::moveObject(const MoveObjectParams &params)
{
    qDebug() << "Invoke moveObject";

    if (params.sourceBucketName.isEmpty() ||
            params.sourceObjectKey.isEmpty() ||
            params.destinationBucketName.isEmpty() ||
            params.destinationObjectKey.isEmpty())
    {
        emit errorResponse("移动对象: 参数缺失，请检查参数");

        return;
    }

    Job job(moveObjectOperation, QVariant::fromValue<MoveObjectParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::initiateMultipartUpload(const PutBigObjectParams &params)
{
    qint64 bodySize = 0;

    QFile file(params.filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        file.close();

        emit errorResponse("文件: " + params.filePath + " 无法打开");

        _workQueue->pop();

        return;
    }

    bodySize = file.size();

    file.close();

    QStringHash headers = {
        { HEADER_HOST, _bucket + "." + _account.endpoint },
        { HEADER_CONTENT_LENGTH, QString::number(bodySize) }
    };

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) },
        { "uploads", "" }
    };

    qDebug() << "initiateMultipartUpload resources: " << resources;

    QStringHash extras = {
        { "objectKey", params.objectKey },
        { "filePath", params.filePath },
        { "fileSize", QString::number(params.fileSize) }
    };

    _sendRequest(METHOD_POST, headers, body, objectAction, resources, initiateMultipartUploadOperation, extras);
}

void Client::uploadPart(const UploadPartParams &params)
{
    Job job(uploadPartOperation, QVariant::fromValue<UploadPartParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::completeMultipartUpload(const CompleteMultipartUploadParams &params)
{
    Job job(completeMultipartUploadOperation, QVariant::fromValue<CompleteMultipartUploadParams>(params));

    _jobQueue->push(job);

    _work();
}

void Client::listMultipartUploads(const ListMultipartUploadsParams &params)
{
    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    if (!params.keyMarker.isEmpty()) headers.insert(HEADER_KEY_MARKER, params.keyMarker);
    if (!params.maxUploads.isEmpty()) headers.insert(HEADER_MAX_UPLOADS, params.maxUploads);

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "uploads", "" }
    };

    qDebug() << "listMultipartUploads resources: " << resources;

    _sendRequest(METHOD_GET, headers, body, bucketAction, resources, listMultipartUploadsOperation);
}

void Client::abortMultipartUpload(const AbortMultipartUploadParams &params)
{
    if (params.objectKey.isEmpty() || params.uploadId.isEmpty())
    {
        emit errorResponse("中断大对象上传至桶: objectKey 和 uploadId 不得为空");

        return;
    }

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) },
        { "uploadId", params.uploadId }
    };

    qDebug() << "abortMultipartUpload resources: " << resources;

    _sendRequest(METHOD_DELETE, headers, body, objectAction, resources, abortMultipartUploadOperation);
}

void Client::listParts(const ListPartsParams &params)
{
    if (params.objectKey.isEmpty() || params.uploadId.isEmpty())
    {
        emit errorResponse("大对象上传分块列表: objectKey 和 uploadId 不得为空");

        return;
    }

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) },
        { "uploadId", params.uploadId }
    };

    if (!params.maxParts.isEmpty()) resources.insert("max-parts", params.maxParts);
    if (!params.partNumberMarker.isEmpty()) resources.insert("part-number-marker", params.partNumberMarker);

    qDebug() << "listParts resources: " << resources;

    QStringHash extras = {{ "objectKey", params.objectKey }};

    _sendRequest(METHOD_GET, headers, body, objectAction, resources, listPartsOperation, extras);
}

// Private Methods
void Client::_registerMetaType() const
{
    qRegisterMetaType<ListObjectParams>("ListObjectParams");
    qRegisterMetaType<GetObjectParams>("GetObjectParams");
    qRegisterMetaType<HeadObjectParams>("HeadObjectParams");
    qRegisterMetaType<PutObjectParams>("PutObjectParams");
    qRegisterMetaType<DeleteObjectParams>("DeleteObjectParams");
    qRegisterMetaType<DeleteObjectsParams>("DeleteObjectsParams");
    qRegisterMetaType<CopyObjectParams>("CopyObjectParams");
    qRegisterMetaType<MoveObjectParams>("MoveObjectParams");
    qRegisterMetaType<ListMultipartUploadsParams>("ListMultipartUploadsParams");
    qRegisterMetaType<AbortMultipartUploadParams>("AbortMultipartUploadParams");
    qRegisterMetaType<ListPartsParams>("ListPartsParams");
    qRegisterMetaType<File>("File");
    qRegisterMetaType<QVector<File>>("QVector<File>");
    qRegisterMetaType<Operation>("Operation");
}

void Client::_work()
{
    if (_workQueue->isFull()) return;
    if (_jobQueue->isEmpty()) return;

    Job job = _jobQueue->pop();

    _workQueue->push(job);

    switch (job.operation)
    {
    case getObjectOperation: _getObject(job); break;
    case putObjectOperation: _putObject(job); break;
    case deleteObjectOperation: _deleteObject(job); break;
    case deleteObjectsOperation: _deleteObjects(job); break;
    case copyObjectOperation: _copyObject(job); break;
    case moveObjectOperation: _moveObject(job); break;
    case uploadPartOperation: _uploadPart(job); break;
    case completeMultipartUploadOperation: _completeMultipartUpload(job); break;
    default: qDebug() << "Unknown operation";
    }
}

QNetworkReply* Client::_sendRequest(const QString &method,
                                    const QStringHash &headers,
                                    const QByteArray &body,
                                    const Action &action,
                                    const QStringHash &resources,
                                    const Operation &operation,
                                    const QStringHash &extras)
{
    QNetworkRequest request;

    QString url = "http://" + _account.endpoint + "/";

    if (action == objectAction) url += resources["object"];

    request.setUrl(url);

    QDateTime dateTime = QDateTime::currentDateTimeUtc();
    QLocale locale = QLocale::English;
    QString format = "ddd, dd MMM yyyy HH:mm:ss";

    auto date = locale.toString(dateTime, format) + " GMT";

    request.setRawHeader(HEADER_DATE, date.toUtf8());

    QStringMap nosHeaders;

    QStringHash::const_iterator ci;

    for (ci = headers.cbegin(); ci != headers.cend(); ++ci)
    {
        QString key = ci.key();
        QString value = ci.value();

        if (value != "") {
            request.setRawHeader(key.toUtf8(), value.toUtf8());

            if (key.startsWith("x-nos-")) nosHeaders.insert(key, value);
        }
    }

    if (body.size() > 0)
    {
        request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

        QByteArray bodyHash = QCryptographicHash::hash(body, QCryptographicHash::Md5);

        request.setRawHeader(HEADER_CONTENT_MD5, bodyHash.toHex());
    }

    QString contentType = request.header(QNetworkRequest::ContentTypeHeader).toString();

    if (contentType.isEmpty() && action == objectAction)
    {
        QString filename = resources["object"].split("/").last();
//        qDebug() << "filename:" << filename;
        QMimeDatabase db;
        QMimeType mimeType = db.mimeTypeForFile(filename, QMimeDatabase::MatchExtension);
        contentType = mimeType.name();
//        qDebug() << "mimeType.name():" << mimeType.name();

        if (contentType.isEmpty()) contentType = "application/octet-stream";

//        qDebug() << "contentType:" << contentType;

        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    }

    _signRequest(method, request, nosHeaders, action, resources);

    qDebug() << "Build requeset success";

    QNetworkReply *reply;

    if (method == METHOD_GET) reply = _manager->get(request);
    if (method == METHOD_POST) reply = _manager->post(request, body);
    if (method == METHOD_PUT) reply = _manager->put(request, body);
    if (method == METHOD_DELETE) reply = _manager->deleteResource(request);
    if (method == METHOD_HEAD) reply = _manager->head(request);

    _operationMap.insert(reply, operation);

    QStringHash params;

    if (action == objectAction)
    {
        params.insert("objectKey", resources["object"]);

        if (operation == uploadPartOperation)
        {
            params.insert("partNumber", resources["partNumber"]);
            params.insert("uploadId", resources["uploadId"]);
        }

        _objectHash.insert(reply, params);
    }

    if (extras.size() > 0) _extraHash.insert(reply, extras);

    connect(reply, &QNetworkReply::uploadProgress, [this, operation, params, extras](qint64 bytesSent, qint64 bytesTotal) {
        qDebug() << "Receive QNetworkReply::uploadProgress, bytesSent:" << bytesSent << "bytesTotal:" << bytesTotal;
        qDebug() << "operation:" << operation;
        qDebug() << "params:" << params;
        qDebug() << "extras:" << extras;

        if (bytesSent > 0) emit this->updateProgressResponse(operation, params, extras, bytesSent);
    });

    qDebug() << "Sended requeset";

    return reply;
}

void Client::_signRequest(const QString &method,
                          QNetworkRequest &request,
                          const QStringMap &nosHeaders,
                          const Action &action,
                          const QStringHash &resources)
{
    QStringList sign = {
        method,
        request.rawHeader(HEADER_CONTENT_MD5),
        request.header(QNetworkRequest::ContentTypeHeader).toString(),
        request.rawHeader(HEADER_DATE)
    };

    QStringMap::const_iterator nci;

    if (nosHeaders.size() > 0)
    {
        QString headerString;

        for (nci = nosHeaders.cbegin(); nci != nosHeaders.cend(); ++nci)
            headerString += nci.key() + ":" + nci.value() + "\n";

        sign.append(headerString);
    }
    else
        sign.append("");

    QString resource;

    switch (action) {
        case listBucketAction:
            resource = "/";
            break;

        case bucketAction: resource = "/" + resources["bucket"] + "/"; break;
        case objectAction:
            QString object = resources["object"];
            object.replace("/", "%2F");

            resource = "/" + resources["bucket"] + "/" + object;

            break;
    }

    QStringHash::const_iterator rci;
    QStringMap params;

    for (rci = resources.cbegin(); rci != resources.cend(); ++rci)
    {
        QString key = rci.key();

        if (key != "bucket" && key != "object")
        {
            params.insert(key, rci.value());
        }
    }

    QString url = request.url().toString();

    if (params.size() > 0)
    {
        QStringList subResourceList = { "acl", "location", "uploadId", "uploads", "partNumber", "delete" };

        QStringList subResources;
        QStringList query;
        QStringMap::const_iterator pci;

        for (pci = params.cbegin(); pci != params.cend(); ++pci)
        {
            QString subQuery = pci.key();
            if (!pci.value().isEmpty()) subQuery += "=" + pci.value();

            query.append(subQuery);

            if (subResourceList.contains(pci.key())) subResources.append(subQuery);
        }

        if (subResources.size() > 0) resource += "?" + subResources.join("&");

        url += "?" + query.join("&");

        request.setUrl(QUrl(url));
    }

//    qDebug() << "request url:" << request.url();

    QString signString = sign.join("\n") + resource;

//    qDebug() << "sign:" << signString;

    QString signature = QMessageAuthenticationCode::hash(signString.toUtf8(),
                                                         _account.accessSecret.toUtf8(),
                                                         QCryptographicHash::Sha256).toBase64();

//    qDebug() << "signature:" << signature;

    request.setRawHeader(HEADER_AUTHORIZATION, ("NOS " + _account.accessKey + ":" + signature).toUtf8());
}

// _getObject
void Client::_getObject(const Job &job)
{
    GetObjectParams params = job.params.value<GetObjectParams>();

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    if (params.range != "") headers.insert(HEADER_RANGE, "bytes=" + params.range);
    if (params.ifModifiedSince != "") headers.insert(HEADER_IF_MODIFIED_SINCE, params.ifModifiedSince);

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) }
    };

    if (params.download != "") resources.insert("download", QUrl::toPercentEncoding(params.download));
    if (params.ifNotFound != "") resources.insert("ifNotFound", QUrl::toPercentEncoding(params.ifNotFound));

    qDebug() << "getObject resources: " << resources;

    QStringHash extras = {
        { "objectKey", params.objectKey },
        { "filePath", params.filePath }
    };

    _sendRequest(METHOD_GET, headers, body, objectAction, resources, getObjectOperation, extras);
}

// _putObject
void Client::_putObject(const Job &job)
{
    PutObjectParams params = job.params.value<PutObjectParams>();

    QByteArray body;

    if (!params.filePath.isEmpty())
    {
        QFile file(params.filePath);

        if (file.size() > PartSize)
        {
            PutBigObjectParams bigParams(params);

            putBigObject(bigParams);

            return;
        }

        if (!file.open(QIODevice::ReadOnly))
        {
            file.close();

            emit errorResponse("文件: " + params.filePath + " 无法打开");

            return;
        }

        body = file.readAll();

        file.close();
    }

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) }
    };

    qDebug() << "putObject resources: " << resources;

    QStringHash extras = {
        { "objectKey", params.objectKey },
        { "filePath", params.filePath },
        { "fileSize", QString::number(params.fileSize) }
    };

    _sendRequest(METHOD_PUT, headers, body, objectAction, resources, putObjectOperation, extras);
}

// _deleteObject
void Client::_deleteObject(const Job &job)
{
    DeleteObjectParams params = job.params.value<DeleteObjectParams>();

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QByteArray body;

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) }
    };

    qDebug() << "deleteObject resources: " << resources;

    QStringHash extras = {{ "objectKey", params.objectKey }};

    _sendRequest(METHOD_DELETE, headers, body, objectAction, resources, deleteObjectOperation, extras);
}

// _deleteObjects
void Client::_deleteObjects(const Job &job)
{
    DeleteObjectsParams params = job.params.value<DeleteObjectsParams>();

    QStringHash headers;
    headers.insert(HEADER_HOST, _bucket + "." + _account.endpoint);
    headers.insert(HEADER_CONTENT_TYPE, "application/xml");

    QByteArray body = "<Delete><Quiet>true</Quiet>";

    foreach (auto objectKey, params.objectKeys)
        body.append("<Object><Key>" + objectKey + "</Key></Object>");

    body.append("</Delete>");

    QStringHash resources = {
        { "bucket", _bucket },
        { "delete", "" }
    };

    qDebug() << "deleteObject resources: " << resources;

    QStringHash extras;

    _sendRequest(METHOD_POST, headers, body, bucketAction, resources, deleteObjectsOperation, extras);
}

// _copyObject
void Client::_copyObject(const Job &job)
{
    CopyObjectParams params = job.params.value<CopyObjectParams>();

    QStringHash headers = {
        { HEADER_HOST, params.destinationBucketName + "." + _account.endpoint },
        { HEADER_X_NOS_COPY_SOURCE, QUrl::toPercentEncoding("/" + params.sourceBucketName + "/" + params.sourceObjectKey) }
    };

    QByteArray body;

    QStringHash resources = {
        { "bucket", params.sourceBucketName },
        { "object", encodeObjectKey(params.destinationObjectKey) }
    };

    qDebug() << "copyObject resources: " << resources;

    QStringHash extras = {
        { "sourceObjectKey", params.sourceObjectKey },
        { "destinationObjectKey", params.destinationObjectKey }
    };

    _sendRequest(METHOD_PUT, headers, body, objectAction, resources, copyObjectOperation, extras);
}

// _moveObject
void Client::_moveObject(const Job &job)
{
    MoveObjectParams params = job.params.value<MoveObjectParams>();

    QStringHash headers = {
        { HEADER_HOST, params.destinationBucketName + "." + _account.endpoint },
        { HEADER_X_NOS_MOVE_SOURCE, QUrl::toPercentEncoding("/" + params.sourceBucketName + "/" + params.sourceObjectKey) }
    };

    QByteArray body;

    QStringHash resources = {
        { "bucket", params.sourceBucketName },
        { "object", encodeObjectKey(params.destinationObjectKey) }
    };

    qDebug() << "moveObject resources: " << resources;

    QStringHash extras = {
        { "sourceObjectKey", params.sourceObjectKey },
        { "destinationObjectKey", params.destinationObjectKey }
    };

    _sendRequest(METHOD_PUT, headers, body, objectAction, resources, moveObjectOperation, extras);
}

// _uploadPart
void Client::_uploadPart(const Job &job)
{
    UploadPartParams params = job.params.value<UploadPartParams>();

    QStringHash headers = {{ HEADER_HOST, _bucket + "." + _account.endpoint }};

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) },
        { "partNumber", QString::number(params.partNumber) },
        { "uploadId", params.uploadId }
    };

    qDebug() << "uploadPart resources: " << resources;

    _partsHash[params.objectKey].insert(params.partNumber, "");

    _sendRequest(METHOD_PUT, headers, params.body, objectAction, resources, uploadPartOperation, params.extras);
}

// _completeMultipartUpload
void Client::_completeMultipartUpload(const Job &job)
{
    CompleteMultipartUploadParams params = job.params.value<CompleteMultipartUploadParams>();

    QStringHash headers = {
        { HEADER_HOST, _bucket + "." + _account.endpoint },
        { HEADER_CONTENT_TYPE, "application/xml" }
    };

    QByteArray body = "<CompleteMultipartUpload>";

    QMap<int, QString>::const_iterator pci;

    for (pci = params.parts.cbegin(); pci != params.parts.cend(); ++pci)
        body.append("<Part><PartNumber>" + QString::number(pci.key()) + "</PartNumber><ETag>" + pci.value() + "</ETag></Part>");

    body.append("</CompleteMultipartUpload>");

    QStringHash resources = {
        { "bucket", _bucket },
        { "object", encodeObjectKey(params.objectKey) },
        { "uploadId", params.uploadId }
    };

    qDebug() << "completeMultipartUpload resources: " << resources;

    _sendRequest(METHOD_POST, headers, body, objectAction, resources, completeMultipartUploadOperation, params.extras);
}

// _listBucketHandler
void Client::_listBucketHandler(QNetworkReply *reply)
{
    QList<QString> buckets;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listBucketResponse(reply->error(), buckets);

        return;
    }

    QByteArray data = reply->readAll();

    qDebug() << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit listBucketResponse(reply->error(), buckets);

        return;
    }

    QDomElement root = doc.documentElement();

    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement() && node.nodeName() == "Buckets")
        {
            QDomNodeList childNodes = node.childNodes();

            for (int i = 0; i < childNodes.count(); ++i)
            {
                QDomNode child = childNodes.at(i);

                QDomNodeList bucketNodes = child.childNodes();

                for (int j = 0; j < bucketNodes.count(); ++j)
                {
                    QDomNode bucketNode = bucketNodes.at(j);

                    if (bucketNode.isElement() && bucketNode.nodeName() == "Name")
                        buckets.append(bucketNode.toElement().text());
                }
            }
        }

        node = node.nextSibling();
    }

    emit listBucketResponse(reply->error(), buckets);
}

// _listObjectHandler
void Client::_listObjectHandler(QNetworkReply *reply)
{
    QStringHash params;
    QStringVector dirs;
    QVector<File> files;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listObjectResponse(reply->error(), params, dirs, files);

        return;
    }

    QByteArray data = reply->readAll();

//    qDebug() << "listObjectHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit listObjectResponse(QNetworkReply::InternalServerError, params, dirs, files);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Prefix") params.insert("prefix", node.toElement().text());
            if (node.nodeName() == "Marker") params.insert("marker", node.toElement().text());
            if (node.nodeName() == "NextMarker") params.insert("nextMarker", node.toElement().text());
            if (node.nodeName() == "MaxKeys") params.insert("maxKeys", node.toElement().text());
            if (node.nodeName() == "IsTruncated") params.insert("isTruncated", node.toElement().text());

            if (node.nodeName() == "CommonPrefixes")
            {
                QDomNodeList childNodes = node.childNodes();
                QDomNode child = childNodes.at(0);

                if (child.isElement()) dirs.append(child.toElement().text());
            }

            if (node.nodeName() == "Contents")
            {
                QDomNodeList childNodes = node.childNodes();

                File file;

                for (int i = 0; i < childNodes.count(); ++i)
                {
                    QDomNode child = childNodes.at(i);

                    if (child.isElement())
                    {
                        if (child.nodeName() == "Key") file.key = child.toElement().text();
                        if (child.nodeName() == "Size") file.size = child.toElement().text().toUInt();
                        if (child.nodeName() == "LastModified")
                        {
                            QString lastModified = child.toElement().text();

                            file.lastModified = lastModified.replace("T", " ").replace(" +0800", "");
                        }
                    }
                }

                files.append(file);
            }
        }

        node = node.nextSibling();
    }

//    qDebug() << "listObjectHandler dirs:" << dirs;
//    qDebug() << "listObjectHandler files:" << files;

    emit listObjectResponse(reply->error(), params, dirs, files);
}

// _getObjectHandler
void Client::_getObjectHandler(QNetworkReply *reply)
{
    QByteArray data;
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    if (reply->error() != QNetworkReply::NoError)
    {
        emit getObjectResponse(reply->error(), params, data);

        return;
    }

    data = reply->readAll();

    emit getObjectResponse(reply->error(), params, data);
}

// _headObjectHandler
void Client::_headObjectHandler(QNetworkReply *reply)
{
    QStringHash headers;
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);
    params.insert("fileSize", extras["fileSize"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    if (reply->error() != QNetworkReply::NoError && reply->error() != QNetworkReply::ContentNotFoundError)
    {
        emit headObjectResponse(reply->error(), params, headers);

        return;
    }

    QList<QByteArray> rawHeaders = reply->rawHeaderList();

    foreach (QByteArray rawHeader, rawHeaders)
        headers.insert(rawHeader, reply->rawHeader(rawHeader));

    emit headObjectResponse(reply->error(), params, headers);
}

// _putObjectHandler
void Client::_putObjectHandler(QNetworkReply *reply)
{
    QStringHash headers;
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);
    params.insert("fileSize", extras["fileSize"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    if (reply->error() != QNetworkReply::NoError)
    {
        emit putObjectResponse(reply->error(), params, headers);

        return;
    }

    QList<QByteArray> rawHeaders = reply->rawHeaderList();

    foreach (QByteArray rawHeader, rawHeaders)
        headers.insert(rawHeader, reply->rawHeader(rawHeader));

    emit putObjectResponse(reply->error(), params, headers);
}

// _deleteObjectHandler
void Client::_deleteObjectHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    emit deleteObjectResponse(reply->error(), params);
}

// _deleteObjectsHandler
void Client::_deleteObjectsHandler(QNetworkReply *reply)
{
    QStringHash params;
    QStringHash headers;

    QByteArray data = reply->readAll();

    qDebug() << "deleteObjectsHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit deleteObjectsResponse(QNetworkReply::InternalServerError, params, headers);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Deleted")
            {
                QDomNodeList childNodes = node.childNodes();
                QDomNode child = childNodes.at(0);

                if (child.isElement()) params.insert(child.toElement().text(), "Deleted");
            }
            else if (node.nodeName() == "Error")
            {
                QDomNodeList childNodes = node.childNodes();

                QString objectKey;
                QString message;

                for (int i = 0; i < childNodes.count(); ++i)
                {
                    QDomNode child = childNodes.at(i);

                    if (child.isElement())
                    {
                        if (child.nodeName() == "Key") objectKey = child.toElement().text();
                        if (child.nodeName() == "Message") message = child.toElement().text();
                    }
                }

                params.insert(objectKey, "Error: " + message);
            }
        }

        node = node.nextSibling();
    }

    QList<QByteArray> rawHeaders = reply->rawHeaderList();

    foreach (QByteArray rawHeader, rawHeaders)
        headers.insert(rawHeader, reply->rawHeader(rawHeader));

    emit deleteObjectsResponse(reply->error(), params, headers);
}

// _copyObjectHandler
void Client::_copyObjectHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("sourceObjectKey", extras["sourceObjectKey"]);
    params.insert("destinationObjectKey", extras["destinationObjectKey"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    emit copyObjectResponse(reply->error(), params);
}

// _moveObjectHandler
void Client::_moveObjectHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("sourceObjectKey", extras["sourceObjectKey"]);
    params.insert("destinationObjectKey", extras["destinationObjectKey"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    emit moveObjectResponse(reply->error(), params);
}

// _initiateMultipartUploadHandler
void Client::_initiateMultipartUploadHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);
    params.insert("fileSize", extras["fileSize"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    QStringHash headers;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit putObjectResponse(reply->error(), params, headers);

        return;
    }

    QByteArray data = reply->readAll();

    qDebug() << "initiateMultipartUploadHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit putObjectResponse(QNetworkReply::InternalServerError, params, headers);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    QStringHash uploadParams;

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Bucket") uploadParams.insert("bucket", node.toElement().text());
            if (node.nodeName() == "Key") uploadParams.insert("key", node.toElement().text());
            if (node.nodeName() == "UploadId") uploadParams.insert("uploadId", node.toElement().text());
        }

        node = node.nextSibling();
    }

    QFile file(params["filePath"]);

    file.open(QIODevice::ReadOnly);

    int part = 1;

    while (!file.atEnd()) {
        extras.insert("part", QString::number(part));

        QByteArray partBody = file.read(PartSize);

        UploadPartParams uploadPartParams = {
            .objectKey = uploadParams["key"],
            .body = partBody,
            .partNumber = part,
            .uploadId = uploadParams["uploadId"],
            .extras = extras
        };

        uploadPart(uploadPartParams);

        ++part;
    }

    file.close();
}

// _uploadPartHandler
void Client::_uploadPartHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    QString objectKey = params["objectKey"];

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);
    params.insert("fileSize", extras["fileSize"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    QStringHash headers;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit putObjectResponse(reply->error(), params, headers);

        return;
    }

    QString etag = reply->rawHeader("ETag");

    _partsHash[objectKey][params["partNumber"].toInt()] = etag;

    bool completeFlag = true;

    QMap<int, QString>::const_iterator pci;

    for (pci = _partsHash[objectKey].cbegin(); pci != _partsHash[objectKey].cend(); ++pci)
    {
//        qDebug() << pci.key() << "=" << pci.value();

        if (pci.value().isEmpty())
        {
            completeFlag = false;

            break;
        }
    }

    if (completeFlag)
    {
        CompleteMultipartUploadParams completeParams = {
            .objectKey = objectKey,
            .uploadId = params["uploadId"],
            .parts = _partsHash[objectKey],
            .extras = extras
        };

        completeMultipartUpload(completeParams);

        _partsHash.remove(objectKey);
    }
}

// _completeMultipartUploadHandler
void Client::_completeMultipartUploadHandler(QNetworkReply *reply)
{
    QStringHash params = _objectHash.value(reply);
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);
    params.insert("filePath", extras["filePath"]);
    params.insert("fileSize", extras["fileSize"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    QStringHash headers;

    QByteArray data = reply->readAll();

    qDebug() << "completeMultipartUploadHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit putObjectResponse(QNetworkReply::InternalServerError, params, headers);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Location") params.insert("location", node.toElement().text());
            if (node.nodeName() == "Bucket") params.insert("bucket", node.toElement().text());
            if (node.nodeName() == "Key") params.insert("key", node.toElement().text());
            if (node.nodeName() == "ETag") params.insert("etag", node.toElement().text());
        }

        node = node.nextSibling();
    }

    QList<QByteArray> rawHeaders = reply->rawHeaderList();

    foreach (QByteArray rawHeader, rawHeaders)
        headers.insert(rawHeader, reply->rawHeader(rawHeader));

    emit putObjectResponse(reply->error(), params, headers);
}

// _listMultipartUploadsHandler
void Client::_listMultipartUploadsHandler(QNetworkReply *reply)
{
    QStringHash params;
    QList<QHash<QString, QVariant>> uploads;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listMultipartUploadsResponse(reply->error(), params, uploads);

        return;
    }

    QByteArray data = reply->readAll();

    qDebug() << "listMultipartUploadsHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit listMultipartUploadsResponse(QNetworkReply::InternalServerError, params, uploads);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Bucket") params.insert("bucket", node.toElement().text());
            if (node.nodeName() == "NextKeyMarker") params.insert("nextKeyMarker", node.toElement().text());
            if (node.nodeName() == "IsTruncated") params.insert("isTruncated", node.toElement().text());

            if (node.nodeName() == "Upload")
            {
                QDomNodeList childNodes = node.childNodes();

                QHash<QString, QVariant> upload;

                for (int i = 0; i < childNodes.count(); ++i)
                {
                    QDomNode child = childNodes.at(i);

                    if (!child.isElement()) continue;

                    if (child.nodeName() == "Key") upload.insert("key", child.toElement().text());
                    if (child.nodeName() == "UploadId") upload.insert("uploadId", child.toElement().text());
                    if (child.nodeName() == "StorageClass") upload.insert("storageClass", child.toElement().text());
                    if (child.nodeName() == "Initiated") upload.insert("initiated", child.toElement().text());

                    if (child.nodeName() == "Owner")
                    {
                        QHash<QString, QVariant> owner;

                        QDomNodeList subChildNodes = child.childNodes();

                        for (int j = 0; j < subChildNodes.count(); ++j)
                        {
                            QDomNode subChild = subChildNodes.at(j);

                            if (!subChild.isElement()) continue;

                            if (subChild.nodeName() == "ID") owner.insert("id", subChild.toElement().text());
                            if (subChild.nodeName() == "DisplayName") owner.insert("displayName", subChild.toElement().text());
                        }

                        upload.insert("owner", owner);
                    }
                }

                uploads.append(upload);
            }
        }

        node = node.nextSibling();
    }

    emit listMultipartUploadsResponse(reply->error(), params, uploads);
}

// _abortMultipartUploadHandler
void Client::_abortMultipartUploadHandler(QNetworkReply *reply)
{
    if (reply->error() != QNetworkReply::NoError)
    {
        emit abortMultipartUploadResponse(reply->error());

        return;
    }

    QByteArray data = reply->readAll();

    qDebug() << "abortMultipartUploadHandler data:" << data;

    qDebug() << "rawHeaderList" << reply->rawHeaderList();

    emit abortMultipartUploadResponse(reply->error());
}

// _listPartsHandler
void Client::_listPartsHandler(QNetworkReply *reply)
{    
    QHash<QString, QVariant> params;
    QStringHash extras = _extraHash.value(reply);

    params.insert("objectKey", extras["objectKey"]);

    _extraHash.remove(reply);
    _objectHash.remove(reply);

    QList<QHash<QString, QVariant>> parts;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listPartsResponse(reply->error(), params, parts);

        return;
    }

    QByteArray data = reply->readAll();

    qDebug() << "listPartsHandler data:" << data;

    QDomDocument doc;

    if (!doc.setContent(data))
    {
        emit listPartsResponse(QNetworkReply::InternalServerError, params, parts);

        return;
    }

    QDomElement root = doc.documentElement();
    QDomNode node = root.firstChild();

    while (!node.isNull())
    {
        if (node.isElement())
        {
            if (node.nodeName() == "Bucket") params.insert("bucket", node.toElement().text());
            if (node.nodeName() == "Key") params.insert("key", node.toElement().text());
            if (node.nodeName() == "UploadId") params.insert("uploadId", node.toElement().text());
            if (node.nodeName() == "StorageClassName") params.insert("storageClassName", node.toElement().text());
            if (node.nodeName() == "PartNumberMarker") params.insert("partNumberMarker", node.toElement().text());
            if (node.nodeName() == "NextPartNumberMarker") params.insert("nextPartNumberMarker", node.toElement().text());
            if (node.nodeName() == "MaxParts") params.insert("maxParts", node.toElement().text());
            if (node.nodeName() == "IsTruncated") params.insert("isTruncated", node.toElement().text());

            if (node.nodeName() == "Owner")
            {
                QHash<QString, QVariant> owner;

                QDomNodeList childNodes = node.childNodes();

                for (int i = 0; i < childNodes.count(); ++i)
                {
                    QDomNode child = childNodes.at(i);

                    if (!child.isElement()) continue;

                    if (child.nodeName() == "ID") owner.insert("id", child.toElement().text());
                    if (child.nodeName() == "DisplayName") owner.insert("displayName", child.toElement().text());
                }

                params.insert("owner", owner);
            }

            if (node.nodeName() == "Part")
            {
                QDomNodeList childNodes = node.childNodes();

                QHash<QString, QVariant> part;

                for (int i = 0; i < childNodes.count(); ++i)
                {
                    QDomNode child = childNodes.at(i);

                    if (!child.isElement()) continue;

                    if (child.nodeName() == "PartNumber") part.insert("partNumber", child.toElement().text());
                    if (child.nodeName() == "LastModified") part.insert("lastModified", child.toElement().text());
                    if (child.nodeName() == "ETag") part.insert("etag", child.toElement().text());
                    if (child.nodeName() == "Size") part.insert("size", child.toElement().text());
                }

                parts.append(part);
            }
        }

        node = node.nextSibling();
    }

    emit listPartsResponse(reply->error(), params, parts);
}

// Private Slots
void Client::_requestFinished(QNetworkReply *reply)
{
//    qDebug() << "enter _requestFinished Thread:" << this->thread();

    QNetworkReply::NetworkError error = reply->error();

    Operation operation = _operationMap.value(reply);

    if (error != QNetworkReply::NoError) {
        QString message;

        switch (error)
        {
        case QNetworkReply::ConnectionRefusedError:
            message = "远程服务器拒绝连接";
            qDebug() << "远程服务器拒绝连接 operation:" << operation;
            break;
        case QNetworkReply::HostNotFoundError:
            message = "远程主机名未找到（无效主机名）";
            qDebug() << "远程主机名未找到（无效主机名） operation:" << operation;
            break;
        case QNetworkReply::TooManyRedirectsError:
            message = "请求超过了设定的最大重定向次数";
            qDebug() << "请求超过了设定的最大重定向次数 operation:" << operation;
            break;
        case QNetworkReply::ContentNotFoundError:
            if (operation != headObjectOperation)
            {
                message = "内容不存在";
                qDebug() << "请求查询的对象不存在 operation:" << operation;
            }

            break;
        default:
            message = "未知错误: " + reply->errorString();
            qDebug() << "未知错误:" << error << " " << reply->errorString() << "operation:" << operation;
        }

        if (error == QNetworkReply::ContentNotFoundError)
        {
            if (operation == getObjectOperation)
            {
                QStringHash extras = _extraHash.value(reply);

                // 处理历史遗留问题：上传文件夹时未在 NOS 创建对应的目录
                if (!extras["objectKey"].endsWith("/")) emit errorResponse(message);
            }
        }
        else
        {
            emit errorResponse(message);
        }
    }

    switch (operation)
    {
    case listBucketOperation: _listBucketHandler(reply); break;
    case listObjectOperation: _listObjectHandler(reply); break;
    case getObjectOperation: _workQueue->pop(); _getObjectHandler(reply); break;
    case headObjectOperation: _headObjectHandler(reply); break;
    case putObjectOperation: _workQueue->pop(); _putObjectHandler(reply); break;
    case deleteObjectOperation: _workQueue->pop(); _deleteObjectHandler(reply); break;
    case deleteObjectsOperation: _workQueue->pop(); _deleteObjectsHandler(reply); break;
    case copyObjectOperation: _workQueue->pop(); _copyObjectHandler(reply); break;
    case moveObjectOperation: _workQueue->pop(); _moveObjectHandler(reply); break;
    case initiateMultipartUploadOperation: _workQueue->pop(); _initiateMultipartUploadHandler(reply); break;
    case uploadPartOperation: _workQueue->pop(); _uploadPartHandler(reply); break;
    case completeMultipartUploadOperation: _workQueue->pop(); _completeMultipartUploadHandler(reply); break;
    case listMultipartUploadsOperation: _listMultipartUploadsHandler(reply); break;
    case abortMultipartUploadOperation: _abortMultipartUploadHandler(reply); break;
    case listPartsOperation: _listPartsHandler(reply); break;
    default: qDebug() << "Connect to host success!";
    }

    _operationMap.remove(reply);

    reply->disconnect();
    reply->deleteLater();
    reply = nullptr;

    _work();
}
