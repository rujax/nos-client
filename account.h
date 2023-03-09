#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QString>
#include <QJsonObject>

typedef struct account
{
    QString name;
    QString endpoint;
    QString accessKey;
    QString accessSecret;

    account(const QString &pName = "", const QString &pEndpoint = "", const QString &pAccessKey = "", const QString &pAccessSecret = "") :
        name(pName), endpoint(pEndpoint), accessKey(pAccessKey), accessSecret(pAccessSecret) {}

    QJsonObject toJSON() const {
        return { { "name", name }, { "endpoint", endpoint }, { "accessKey", accessKey }, { "accessSecret", accessSecret } };
    }

    void reset() {
        this->name = "";
        this->endpoint = "";
        this->accessKey = "";
        this->accessSecret = "";
    }
} Account;

#endif // ACCOUNT_H
