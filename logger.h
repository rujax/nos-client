#ifndef LOGGER_H
#define LOGGER_H

#include <QObject>

QT_BEGIN_NAMESPACE
class QThread;
QT_END_NAMESPACE

class Logger : public QObject
{
    Q_OBJECT

public:
    explicit Logger(QObject *parent = nullptr);
    ~Logger();

public slots:
    void writeLog(const QString &bucket, const QString &action, const QString &status, const QString &msg);

private:
    QThread *_thread;
};

#endif // LOGGER_H
