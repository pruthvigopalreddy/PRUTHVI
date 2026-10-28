#ifndef USERHISTORY_H
#define USERHISTORY_H

#include <QWidget>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QList>
#include "crypto.h"

class UserEntry {
public:
    QString userID;
    QString authorityLevel;
    QString date;
    QString time;
    QString action;
    QString inFile;
    QString outFile;
};

class UserHistory : public QWidget
{
    Q_OBJECT
public:
    explicit UserHistory(QWidget *parent = nullptr);
    QString generateReport();
    void loadUserData(QString loginID);
    void saveUserData(const UserEntry &entry);

private:
    void encryptLoginFile(QString filePath, QString output);
    QString decryptLoginFile(QString filePath);
    QByteArray toBinaryString(const QByteArray &data);
    QByteArray fromBinaryString(const QByteArray &binaryString);
    crypto *crypt;
    QByteArray salt;
    QString filePath;
    QString loginID;
    QList<UserEntry> user_run_logs;
};

#endif // USERHISTORY_H
