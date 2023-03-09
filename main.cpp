#include "mainwindow.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QMutex>
#include <QTextCodec>
#include <QStandardPaths>
#include <QDir>

#include <stdio.h>
#include <stdlib.h>

void logHandler(QtMsgType type, const QMessageLogContext &, const QString &msg)
{
    if (type == QtMsgType::QtDebugMsg)
    {
#ifndef QT_NO_DEBUG_OUTPUT
        QByteArray message = msg.toLocal8Bit();

        fprintf(stdout, "%s\n", message.constData());
        fflush(stdout);
#endif

        return;
    }

    static QMutex mutex;

    mutex.lock();

    QString today = QDate::currentDate().toString("yyyyMMdd");
    QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    QString message = QString("[%1] %2").arg(now, msg);

    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QFile file(dirPath + "/log/nos-" + today + ".log");

    if (file.open(QIODevice::WriteOnly|QIODevice::Append))
    {
        QTextStream textStream(&file);
        textStream.setCodec(QTextCodec::codecForName("utf-8"));
        textStream << message << endl;

        file.flush();
        file.close();
    }

    mutex.unlock();
}

void mkLogDir()
{
    QString dirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

    QDir dir(dirPath + "/log");

    if (!dir.exists()) dir.mkpath(dir.path());
}

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);

    QApplication a(argc, argv);

    mkLogDir();
    qInstallMessageHandler(logHandler);

    QFont font("等线");
    a.setFont(font);

    QFile qss(":/nos-client.qss");

    if (qss.open(QFile::ReadOnly))
    {
//        qDebug() << "Open nos-client.qss success";

        QString style = QLatin1String(qss.readAll());
        a.setStyleSheet(style);

//        qDebug() << "Set nos-client.qss success";

        qss.close();

//        qDebug() << "Close nos-client.qss success";
    }
    else
    {
        qWarning() << "Open nos-client.qss failure";
    }

//    qDebug() << qApp->font().family();
//    qDebug() << qApp->font().defaultFamily();
//    qDebug() << qApp->font().styleName();
//    qDebug() << qApp->font().toString();
//    qDebug() << qApp->font().key();

    MainWindow w;

    w.resize(1200, 800);
    w.move((QApplication::desktop()->width() - w.width()) / 2, (QApplication::desktop()->height() - w.height()) / 2);
    w.show();

    w.setTaskbarHandle();

    return a.exec();
}
