#ifndef CONFIG_H
#define CONFIG_H

#include <QString>
#include <QList>

#include "account.h"

typedef struct
{
    QList<Account> accounts;
    QString currentAccount;
    bool skipOlder;
} Config;

#endif // CONFIG_H
