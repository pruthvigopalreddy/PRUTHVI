#ifndef LOGHISTORY_H
#define LOGHISTORY_H

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QList>
#include "crypto.h"

class LogEntry {
public:
    QString userID;
    QString authorityLevel;
    QString date;
    QString time;
    QString action;
    QString authorisedBy;
};

class LogHistory : public QWidget
{
    Q_OBJECT
public:
    explicit LogHistory(QWidget *parent = nullptr);
    QString generateReport();
    void loadLoginData();
    void saveLoginData(const LogEntry &entry);
    QString decryptLoginFile(QString filePath);

private:
    void encryptLoginFile(QString filePath, QString output);
    QByteArray toBinaryString(const QByteArray &data);
    QByteArray fromBinaryString(const QByteArray &binaryString);
    crypto *crypt;
    QByteArray salt;
    QString filePath;
    QList<LogEntry> logs;
};

#endif // LOGHISTORY_H
