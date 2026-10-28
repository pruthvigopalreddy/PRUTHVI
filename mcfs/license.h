#ifndef LICENSE_H
#define LICENSE_H

#include <QObject>

class license
{
public:
    license();

    bool warningBox();

    QStringList getEthernetMacAddresses();

    bool compareMacAddress(QString &macAddress, QString &targetMac);

    QString checksum();

    QString matchedType="";

    QString getSystemFingerprint();

    QString getProductUUID();

    QString getMachineId();
};

#endif // LICENSE_H
