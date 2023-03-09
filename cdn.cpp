#include "cdn.h"

#include <QNetworkProxy>
#include <QThread>
#include <QMessageAuthenticationCode>
#include <QJsonDocument>
#include <QJsonArray>

// Constructor
CDN::CDN() :
    _manager(new QNetworkAccessManager(this)),
    _thread(new QThread),
    _jobQueue(new JobQueue<Job>),
    _workQueue(new WorkerQueue<Job>(6))
{
    _registerMetaType();

    _manager->setProxy(QNetworkProxy::NoProxy);

    connect(_manager, &QNetworkAccessManager::finished, this, &CDN::_requestFinished);

    connect(_thread, &QThread::finished, this, &QObject::deleteLater);

    this->moveToThread(_thread);

    _thread->start();
}

// Destructor
CDN::~CDN()
{
    qDebug() << "Execute CDN::~CDN()";

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
void CDN::setAccount(const Account &account)
{
    qDebug() << "enter CDN::setAccount";

    _account = account;

    qDebug() << "_account.accessKey:" << _account.accessKey;
    qDebug() << "_account.accessSecret:" << _account.accessSecret;
}

void CDN::resetAccount()
{
    _account.reset();
}

// Public Slots
void CDN::listDomain(const ListDomainParams &params)
{
    qDebug() << "Invoke listDomain";

    QByteArray body;

    QStringHash resources = {
        { "sortField", params.sortField },
        { "orderType", params.orderType },
        { "serviceType", params.serviceType },
        { "status", params.status },
        { "workStatus", params.workStatus },
        { "pageNum", QString::number(params.pageNum) },
        { "pageSize", QString::number(params.pageSize) }
    };

//    qDebug() << "listDomain resources:" << resources;

    _sendRequest(METHOD_GET, body, resources, listDomainOperation);
}

void CDN::purge(const PurgeParams &params)
{
    if (params.domain.isEmpty())
    {
        emit errorResponse("创建缓存刷新请求: 缓存地址不得为空");

        return;
    }

    if (params.files.isEmpty() && params.dirs.isEmpty())
    {
        emit errorResponse("创建缓存刷新请求: 文件或文件夹必须存在一个");

        return;
    }

    Job job(purgeOperation, QVariant::fromValue<PurgeParams>(params));

    _jobQueue->push(job);

    _work();
}

void CDN::getPurgeStatus(const GetPurgeStatusParams &params)
{
    qDebug() << "Invoke getPurgeStatus";

    if (params.domain.isEmpty())
    {
        emit errorResponse("创建缓存刷新请求: 缓存地址不得为空");

        return;
    }

    QByteArray body;

    QStringHash resources = {
        { "domain", params.domain },
        { "id", params.id }
    };

//    qDebug() << "getPurgeStatus resources:" << resources;

    _sendRequest(METHOD_GET, body, resources, getPurgeStatusOperation);
}

void CDN::listPurge(const ListPurgeParams &params)
{
    qDebug() << "Invoke listPurge";

    if (params.domain.isEmpty())
    {
        emit errorResponse("创建缓存刷新请求: 缓存地址不得为空");

        return;
    }

    QByteArray body;

    QStringHash resources = {
        { "domain", params.domain },
        { "pageNum", QString::number(params.pageNum) },
        { "pageSize", QString::number(params.pageSize) }
    };

//    qDebug() << "listPurge resources:" << resources;

    _sendRequest(METHOD_GET, body, resources, listPurgeOperation);
}

// Private Methods
void CDN::_registerMetaType() const
{
    qRegisterMetaType<ListDomainParams>("ListDomainParams");
    qRegisterMetaType<PurgeParams>("PurgeParams");
    qRegisterMetaType<GetPurgeStatusParams>("GetPurgeStatusParams");
    qRegisterMetaType<ListPurgeParams>("ListPurgeParams");
    qRegisterMetaType<Domain>("Domain");
    qRegisterMetaType<QVector<Domain>>("QVector<Domain>");
    qRegisterMetaType<Purge>("Purge");
    qRegisterMetaType<QVector<Purge>>("QVector<Purge>");
    qRegisterMetaType<PurgeStatus>("PurgeStatus");
    qRegisterMetaType<QVector<PurgeStatus>>("QVector<PurgeStatus>");
    qRegisterMetaType<Operation>("Operation");
}

void CDN::_work()
{
    if (_workQueue->isFull()) return;
    if (_jobQueue->isEmpty()) return;

    Job job = _jobQueue->pop();

    _workQueue->push(job);

    switch (job.operation)
    {
    case purgeOperation: _purge(job); break;
#if 0
    case preheatOperation: _preheat(job); break;
#endif
    default: qDebug() << "Unknown operation";
    }
}

QNetworkReply* CDN::_sendRequest(const QString &method,
                                 const QByteArray &body,
                                 const QStringHash &resources,
                                 const Operation &operation)
{
    QNetworkRequest request;

    request.setUrl(QUrl("http://ncdn-eastchina1.126.net"));

    QDateTime dateTime = QDateTime::currentDateTimeUtc();
    QLocale locale = QLocale::English;
    QString format = "ddd, dd MMM yyyy HH:mm:ss";

    QString date = locale.toString(dateTime, format) + " GMT";

    request.setRawHeader(HEADER_DATE, date.toUtf8());

    request.setRawHeader(HEADER_ACCEPT, "application/json");

    if (body.size() > 0)
    {
//        qDebug() << "body.size():" << body.size();

        request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());

        // 文档里说需要提供，但实际上不提供也可以正常使用
//        QByteArray bodyHash = QCryptographicHash::hash(body, QCryptographicHash::Md5);

//        request.setRawHeader(HEADER_CONTENT_MD5, bodyHash.toHex());

        request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json;charset=UTF-8");
    }

    _signRequest(method, request, operation, resources);

    qDebug() << "Build requeset success";

    QNetworkReply *reply;

    if (method == METHOD_GET) reply = _manager->get(request);
    if (method == METHOD_POST) reply = _manager->post(request, body);
    if (method == METHOD_PUT) reply = _manager->put(request, body);

    _operationHash.insert(reply, operation);

    qDebug() << "Sended requeset";

    return reply;
}

void CDN::_signRequest(const QString &method,
                       QNetworkRequest &request,
                       const Operation &operation,
                       const QStringHash &resources)
{
    QStringList sign = {
        method,
        request.rawHeader(HEADER_CONTENT_MD5),
        request.header(QNetworkRequest::ContentTypeHeader).toString(),
        request.rawHeader(HEADER_DATE)
    };

    QString url = request.url().toString();
    QString resource = "/domain/";

    switch (operation) {
    case purgeOperation: case listPurgeOperation: resource += resources["domain"] + "/purge"; break;
    case getPurgeStatusOperation: resource += resources["domain"] + "/purge/" + resources["id"]; break;
    default:;
    }

    url += resource;

    QStringHash::const_iterator rci;
    QStringMap params;

    for (rci = resources.cbegin(); rci != resources.cend(); ++rci)
    {
        QString key = rci.key();

        if (key != "domain" && key != "id") params.insert(key, rci.value());
    }

    if (params.size() > 0)
    {
        QStringList subResourceList = {
            "enable", "disable", "pageSize", "pageNum", "dateFrom", "dateTo", "type", "orderType", "sortField", "serviceType",
            "status", "isEnabled"
        };

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
    }

    request.setUrl(QUrl(url));

//    qDebug() << "request url:" << request.url();

    sign.append(resource);

    QString signString = sign.join("\n");

//    qDebug() << "sign:" << signString;

    QString signature = QMessageAuthenticationCode::hash(signString.toUtf8(),
                                                         _account.accessSecret.toUtf8(),
                                                         QCryptographicHash::Sha256).toBase64();

//    qDebug() << "signature:" << signature;

    request.setRawHeader(HEADER_AUTHORIZATION, ("NCDN " + _account.accessKey + ":" + signature).toUtf8());
}

// _purge
void CDN::_purge(const Job &job)
{
    PurgeParams params = job.params.value<PurgeParams>();

    QJsonObject root;
    QJsonArray fileUrls;
    QJsonArray dirUrls;

    foreach (auto file, params.files)
    {
        fileUrls.append(file);
    }

    foreach (auto dir, params.dirs)
    {
        dirUrls.append(dir);
    }

    root.insert("file-url", fileUrls);
    root.insert("dir-url", dirUrls);

    QJsonDocument jsonDoc(root);
    QByteArray body = jsonDoc.toJson(QJsonDocument::Compact);

    qDebug() << "body:" << body;

    QStringHash resources = {{ "domain", params.domain }};

    _sendRequest(METHOD_POST, body, resources, purgeOperation);
}

// listDomain
void CDN::_listDomainHandler(QNetworkReply *reply)
{
    bool isTruncated = false;
    int quantity = 0;
    QVector<Domain> domains;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listDomainResponse(reply->error(), isTruncated, quantity, domains);

        return;
    }

    QByteArray data = reply->readAll();

//    qDebug() << "_listDomainHandler data:" << data;

    QJsonParseError jsonError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        emit listDomainResponse(QNetworkReply::InternalServerError, isTruncated, quantity, domains);

        return;
    }

    QJsonObject root = jsonDoc.object();

    isTruncated = root["is-truncated"].toBool();
    quantity = root["quantity"].toInt();

    foreach (const QJsonValue &value, root["domain-summary"].toArray())
    {
        auto obj = value.toObject();

        Domain domain = {
            .productID = obj["product-id"].toString(),
            .id = obj["domain-id"].toString(),
            .name = obj["domain-name"].toString(),
            .serviceType = obj["service-type"].toString(),
            .serviceAreas = obj["service-areas"].toString(),
            .originType = obj["origin-type"].toString(),
            .originIPs = {},
            .defaultOriginHostHeader = obj["default-origin-host-header"].toString(),
            .comment = obj["comment"].toString(),
            .workStatus = obj["work-status"].toString(),
            .status = obj["status"].toString(),
            .cname = obj["cname"].toString()
        };

        domain.createTime = obj["create-time"].toDouble();
        domain.updateTime = obj["update-time"].toDouble();

        foreach (const QJsonValue &ip, obj["origin-ips"].toArray())
        {
            domain.originIPs.append(ip.toString());
        }

        domains.append(domain);
    }

//    qDebug() << "_listPurgeHandler domains:" << domains;

    emit listDomainResponse(reply->error(), isTruncated, quantity, domains);
}

// purge
void CDN::_purgeHandler(QNetworkReply *reply)
{
    QString id;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit purgeResponse(reply->error(), id);

        return;
    }

    QByteArray data = reply->readAll();

//    qDebug() << "_purgeHandler data:" << data;

    QJsonParseError jsonError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        emit purgeResponse(QNetworkReply::InternalServerError, id);

        return;
    }

    QJsonObject root = jsonDoc.object();

    id = root["purge-id"].toString();

    emit purgeResponse(reply->error(), id);
}

// getPurgeStatus
void CDN::_getPurgeStatusHandler(QNetworkReply *reply)
{
    QVector<PurgeStatus> purgeStatuses;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit getPurgeStatusResponse(reply->error(), purgeStatuses);

        return;
    }

    QByteArray data = reply->readAll();

//    qDebug() << "_getPurgeStatusHandler data:" << data;

    QJsonParseError jsonError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        emit getPurgeStatusResponse(QNetworkReply::InternalServerError, purgeStatuses);

        return;
    }

    QJsonObject root = jsonDoc.object();

    foreach (const QJsonValue &value, root["item"].toArray())
    {
        auto obj = value.toObject();

        PurgeStatus purgeStatus = {
            .path = obj["path"].toString(),
            .status = obj["status"].toString(),
            .rate = obj["rate"].toInt()
        };

        purgeStatuses.append(purgeStatus);
    }

//    qDebug() << "_getPurgeStatusHandler purgeStatuses:" << purgeStatuses;

    emit getPurgeStatusResponse(reply->error(), purgeStatuses);
}

// listPurge
void CDN::_listPurgeHandler(QNetworkReply *reply)
{
    bool isTruncated = false;
    int quantity = 0;
    QVector<Purge> purges;

    if (reply->error() != QNetworkReply::NoError)
    {
        emit listPurgeResponse(reply->error(), isTruncated, quantity, purges);

        return;
    }

    QByteArray data = reply->readAll();

//    qDebug() << "_listPurgeHandler data:" << data;

    QJsonParseError jsonError;

    QJsonDocument jsonDoc = QJsonDocument::fromJson(data, &jsonError);

    if (jsonError.error != QJsonParseError::NoError)
    {
        emit listPurgeResponse(QNetworkReply::InternalServerError, isTruncated, quantity, purges);

        return;
    }

    QJsonObject root = jsonDoc.object();

    isTruncated = root["is-truncated"].toBool();
    quantity = root["quantity"].toInt();

    foreach (const QJsonValue &value, root["items"].toArray())
    {
        auto obj = value.toObject();

        Purge purge = {
            .id = obj["purge-id"].toString(),
            .urls = {},
            .status = obj["status"].toString()
        };

        purge.createTime = obj["create-time"].toDouble();

        foreach (const QJsonValue &url, obj["purge-urls"].toArray())
        {
            purge.urls.append(url.toString());
        }

        purges.append(purge);
    }

//    qDebug() << "_listPurgeHandler purges:" << purges;

    emit listPurgeResponse(reply->error(), isTruncated, quantity, purges);
}

// Private Slots
void CDN::_requestFinished(QNetworkReply *reply)
{
//    qDebug() << "enter _requestFinished Thread:" << this->thread();

    QNetworkReply::NetworkError error = reply->error();

    Operation operation = _operationHash.value(reply);

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
            message = "内容不存在";
            qDebug() << "请求查询的对象不存在 operation:" << operation;
            break;
        default:
            message = "未知错误: " + reply->errorString();
            qDebug() << "未知错误:" << error << " " << reply->errorString() << "operation:" << operation;
        }

        emit errorResponse(message);
    }

    switch (operation)
    {
    case listDomainOperation: _listDomainHandler(reply); break;
    case purgeOperation: _workQueue->pop(); _purgeHandler(reply); break;
    case getPurgeStatusOperation: _getPurgeStatusHandler(reply); break;
    case listPurgeOperation: _listPurgeHandler(reply); break;
#if 0
    case preheatOperation: _workQueue->pop(); _preheatHandler(reply); break;
    case listPreheatOperation: _listPreheatHandler(reply); break;
    case getPreheatQuotaOperation: _getPreheatQuotaHandler(reply); break;
#endif
    default: qDebug() << "Connect to host success!";
    }

    _operationHash.remove(reply);

    reply->disconnect();
    reply->deleteLater();
    reply = nullptr;

    _work();
}
