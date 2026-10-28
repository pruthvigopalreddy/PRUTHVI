#include "mainwindow.h"
#include <QApplication>
#include <QLocale>
#include <QTranslator>
#include <QFile>
#include <QMap>
#include <QString>
#include <QDebug>
#include "crypto.h"
#include "license.h"
#include <QMessageBox>
#include <QCryptographicHash>

QMap<QString, QString> globalVariables;

QString calculateAppChecksum()
{
    QString appImagePath = qEnvironmentVariable("APPIMAGE");

    // If not running as an AppImage, fall back to the executable.
    if (appImagePath.isEmpty())
        appImagePath = QCoreApplication::applicationFilePath();

    QFile file(appImagePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open:" << appImagePath;
        return QString();
    }

    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(&file);

    return hash.result().toHex();
}

void loadVariablesFromBinaryString(const QString &data) {
    // Split the input string into lines
    QStringList lines = data.split('\n');

    // Iterate over each line
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines.at(i);  // .at() is const, no detach

        if (line.isEmpty() || line.startsWith("#") || line.startsWith("//")) {
            continue;
        }
        // Split the line by comma
        QStringList parts = line.split(',', Qt::SkipEmptyParts);

        // Check if we have exactly two parts (variable and value)
        if (parts.size() == 2) {
            QString variable = parts[0].trimmed(); // Variable name
            QString value = parts[1].trimmed();     // Value

            // Store in the global variable map
            globalVariables[variable] = value;
            //qDebug()<<variable<<","<<value;
        }
    }
}

QString decryptLoginFile(QString filePath)
{
    //32 byte salt
    QByteArray  salt = QByteArray::fromHex("414b4153482d5345442d2d4543494c2d2d414b4153482d2d5345442d4543494c"); // "AKASH-SED--ECIL--AKASH--SED-ECIL"

    crypto *cipher=new crypto();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        delete cipher;   // prevent leak
        return ""; // Exit if the file cannot be opened
    }

    QByteArray combinedBinaryString = file.readAll();
    file.close();

    bool validation=true;
    // Validate if the input data contains only '0' and '1'
    const char *data = combinedBinaryString.constData();
    for (int i = 0; i < combinedBinaryString.size(); ++i) {
        char c = data[i];
        if (c != '0' && c != '1') {
            validation = false;
            break;   // no need to continue, already invalid
        }
    }
    if(!validation)
    {
        delete cipher;   // prevent leak
        return "";
    }

    // Convert binary string back to QByteArray
    QByteArray interleavedData =  cipher->fromBinaryString(combinedBinaryString);

    // Extract key and ivAndCiphertext
    int keySize = 32; // Assuming 256-bit key (32 bytes)
    int interleavedSize = interleavedData.size();

    QByteArray xorKey;
    QByteArray ivAndCiphertext;

    // Extract the XOR-ed key and encrypted data
    for (int i = 0; i < keySize && i < interleavedSize; ++i) {
        xorKey.append(interleavedData[2 * i]);
        ivAndCiphertext.append(interleavedData[2 * i + 1]);
    }

    // Append remaining part of ivAndCiphertext if it was longer
    if (interleavedSize > 2 * keySize) {
        ivAndCiphertext.append(interleavedData.mid(2 * keySize));
    }

    // Append remaining part of key if it was longer
    if (interleavedSize < 2 * keySize) {
        xorKey.append(interleavedData.mid(2 * keySize));
    }

    // Reconstruct the original key by XORing with salt
    QByteArray aesKey = xorKey;
    for (int i = 0; i < aesKey.size(); ++i) {
        aesKey[i] = aesKey[i] ^ salt[i % salt.size()];
    }

    QString output = cipher->decrypt(ivAndCiphertext, aesKey);

    return output;
}

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QString appImagePath = qEnvironmentVariable("APPIMAGE");

    QString configPath;
    if (!appImagePath.isEmpty()) {
        QFileInfo fi(appImagePath);
        configPath = fi.absolutePath() + "/config/limit.bin";
    } else {
        // Running normally (not AppImage)
        configPath = QCoreApplication::applicationDirPath() + "/../config/limit.bin";
    }

    qDebug() << configPath;

    license *licenseCheck_obj=new license();
    QString limitFile=decryptLoginFile(configPath);

    if(limitFile.isEmpty())
    {
        QMessageBox counterMsg;
        counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint & Qt::FramelessWindowHint);
        counterMsg.setWindowTitle("Configuration Error!!");
        counterMsg.setText("Supporting Configuration File is Missing/Corrupted!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
        counterMsg.setIcon(QMessageBox::Warning);
        counterMsg.setStandardButtons(QMessageBox::Ok);
        counterMsg.exec();
        delete licenseCheck_obj;
        return 0;
    }

    loadVariablesFromBinaryString(limitFile);

    QString expected = licenseCheck_obj->checksum();
    QString actual = calculateAppChecksum();

    qDebug()<<actual;

    //remove ! in final code a
    if(expected==actual)
    {
        //remove ! in final code
        if(licenseCheck_obj->warningBox())
        {
            MainWindow *manualWindow_obj = new MainWindow();
            manualWindow_obj->rdacs_serial=licenseCheck_obj->matchedType;
            manualWindow_obj->showFullScreen();
            return a.exec();
        }
        else
        {
            return 0;
        }
    }
    else
    {
        QMessageBox counterMsg;

        counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint & Qt::FramelessWindowHint);
        counterMsg.setWindowTitle("Checksum Error!!");
        counterMsg.setText("Application Software is either Corrupted or Version has been Changed!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
        counterMsg.setIcon(QMessageBox::Warning);
        counterMsg.setStandardButtons(QMessageBox::Ok);
        counterMsg.exec();
        return 0;
    }
}
