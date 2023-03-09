#include "logger.h"

#include <QThread>
#include <QDebug>

// Constructor
Logger::Logger(QObject *parent) : QObject(parent), _thread(new QThread)
{
    connect(_thread, &QThread::finished, this, &QObject::deleteLater);

    this->moveToThread(_thread);

    _thread->start();
}

// Destructor
Logger::~Logger()
{
    qDebug() << "Execute Logger::~Logger()";

    _thread->quit();
    _thread->requestInterruption();
    _thread = nullptr;
}

// Public Slots
void Logger::writeLog(const QString &bucket, const QString &action, const QString &status, const QString &msg)
{
//    qDebug() << "Logger Thread:" << this->thread();

    QString message = QString("%1 [%2] %3: %4").arg(bucket).arg(action).arg(status).arg(msg);

    qInfo() << message;
}
