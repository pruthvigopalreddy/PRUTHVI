#include "license.h"
#include <QNetworkInterface>
#include <QMessageBox>
#include <QCryptographicHash>
#include <QFile>

extern QMap<QString, QString> globalVariables;

license::license() {}


QStringList license::getEthernetMacAddresses()
{
    // List to hold the MAC addresses
    QStringList macAddresses;

    // Get all network interfaces on the system
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();

    for (int i = 0; i < interfaces.size(); ++i)
    {
        const QNetworkInterface &interface = interfaces.at(i);
        if (interface.type() == QNetworkInterface::Ethernet &&
            !(interface.flags() & QNetworkInterface::IsLoopBack))
        {
            QString macAddress = interface.hardwareAddress();
            if (!macAddress.isEmpty())
            {
                macAddresses.append(macAddress);
            }
        }
    }

    return macAddresses;  // Return the list of MAC addresses
}

bool license::compareMacAddress(QString &macAddress, QString &targetMac)
{
    // Normalize the MAC address by removing ':' and converting to lowercase
    QString normalizedMac = macAddress.replace(":", "").toLower();
    QString normalizedTargetMac = targetMac.replace(":", "").toLower();

    return normalizedMac == normalizedTargetMac;
}

bool license::warningBox()
{
    QString licenseType = globalVariables["LICENSE_TYPE"];

    if(licenseType=="MAC")
    {
        // Get system MAC addresses (a list of Ethernet MAC addresses)
        QStringList systemMacList = getEthernetMacAddresses();

        // Flag to indicate if a match is found
        bool macFound = false;

        for (auto it = globalVariables.begin(); it != globalVariables.end(); ++it)
        {
            QString key = it.key();

            if (key.startsWith("RDACS_") && key.endsWith("_MAC"))
            {
                QString licensedMac = it.value();

                for (QString &systemMac : systemMacList)
                {
                    if (compareMacAddress(systemMac, licensedMac))
                    {
                        macFound = true;

                        // RDACS_001_MAC -> extract 001
                        QString serial = key;
                        serial.remove("RDACS_");
                        serial.remove("_MAC");

                        matchedType = serial;

                        break;
                    }
                }
            }

            if (macFound)
                break;
        }

        //add ! in final code
        if (!macFound)
        {
            QMessageBox counterMsg;
            counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint & Qt::FramelessWindowHint);
            counterMsg.setWindowTitle("Software License Error!!");
            counterMsg.setText("Software is not Licensed for this System!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            counterMsg.setIcon(QMessageBox::Warning);
            counterMsg.setStandardButtons(QMessageBox::Ok);
            counterMsg.exec();
        }

        return macFound;
    }
    else if(licenseType=="HASH")
    {
        QString currentHash = getSystemFingerprint();

        bool hashFound = false;

        for (auto it = globalVariables.begin(); it != globalVariables.end(); ++it)
        {
            QString key = it.key();

            // Find all RDACS_xxx_HASH entries
            if (key.startsWith("RDACS_") && key.endsWith("_HASH"))
            {
                QString licensedHash = it.value();

                if (currentHash.compare(licensedHash, Qt::CaseInsensitive) == 0)
                {
                    hashFound = true;

                    // RDACS_001_HASH -> extract 001
                    QString serial = key;
                    serial.remove("RDACS_");
                    serial.remove("_HASH");

                    matchedType = serial;

                    break;
                }
            }
        }

        if (!hashFound)
        {
            QMessageBox counterMsg;
            counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
            counterMsg.setWindowTitle("Software License Error!!");
            counterMsg.setText("Software is not Licensed for this System!!!<br><br>*Refer Operation Manual for further Assistance!!!");
            counterMsg.setIcon(QMessageBox::Warning);
            counterMsg.setStandardButtons(QMessageBox::Ok);
            counterMsg.exec();
        }

        return hashFound;
    }
    else if(licenseType=="NONE")
    {
        QStringList systemMacList = getEthernetMacAddresses();

        bool macFound = false;

        qDebug() << systemMacList;

        for (auto it = globalVariables.begin(); it != globalVariables.end() && !macFound; ++it)
        {
            QString key = it.key();

            if (key.startsWith("RDACS_") && key.endsWith("_MAC"))
            {
                QStringList macList = it.value().split(";");

                for (QString &licensedMac : macList)
                {
                    for (QString &systemMac : systemMacList)
                    {
                        if (compareMacAddress(systemMac, licensedMac))
                        {
                            macFound = true;

                            // RDACS_001_MAC -> 001
                            QString serial = key;
                            serial.remove("RDACS_");
                            serial.remove("_MAC");

                            matchedType = serial;

                            break;
                        }
                    }

                    if (macFound)
                        break;
                }
            }
        }
        if (!macFound)
        {
           matchedType = "--";
        }
        return true;
    }
    return true;
}

QString license::getSystemFingerprint()
{
    QString productUUID = getProductUUID();
    QString machineId = getMachineId();

    QString data = productUUID + "|" + machineId;
    QByteArray hash =
        QCryptographicHash::hash(data.toUtf8(),
                                 QCryptographicHash::Sha256);

    qDebug()<<hash.toHex();
    return hash.toHex();
}

QString license::getProductUUID()
{
    QFile file("/sys/class/dmi/id/product_uuid");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug()<<"Need HASH Permissions!!";
        return QString();
    }

    return QString(file.readAll()).trimmed();
}

QString license::getMachineId()
{
    QFile file("/etc/machine-id");
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug()<<"Need HASH Permissions!!";
        return QString();
    }

    return QString(file.readAll()).trimmed();
}

QString license::checksum()
{
    QString appImagePath = qEnvironmentVariable("APPIMAGE");

    if (!appImagePath.isEmpty())
    {
        return globalVariables["APPIMAGE_CHECKSUM"];
    }
    else
    {
        return globalVariables["EXE_CHECKSUM"];
    }
    return globalVariables["EXE_CHECKSUM"];
}
