#include "userhistory.h"
#include <QMessageBox>
#include <QFileDialog>

extern QMap<QString, QString> globalVariables;

UserHistory::UserHistory(QWidget *parent)
    : QWidget{parent}
{
    crypt=new crypto();
    //32 byte salt
    salt = QByteArray::fromHex("414b4153482d5345442d2d4543494c2d2d414b4153482d2d5345442d4543494c"); // "AKASH-SED--ECIL--AKASH--SED-ECIL"
}

// Helper functions to convert to/from binary string
QByteArray UserHistory::toBinaryString(const QByteArray &data) {
    QByteArray binaryString;
    for (char byte : data) {
        binaryString.append(QString::number(static_cast<unsigned char>(byte), 2).rightJustified(8, '0').toUtf8());
    }
    return binaryString;
}

QByteArray UserHistory::fromBinaryString(const QByteArray &binaryString) {
    QByteArray data;
    for (int i = 0; i < binaryString.size(); i += 8) {
        bool ok;
        data.append(static_cast<char>(binaryString.mid(i, 8).toInt(&ok, 2)));
    }
    return data;
}

void UserHistory::loadUserData(QString loginID)
{
    this->loginID=loginID;

    filePath=globalVariables["USER_LOG_BIN_PATH"];

    if (filePath.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No file selected.");
        return; // Exit if no file is selected
    }

    user_run_logs.clear();

    //decrypt file
    QString output= decryptLoginFile(filePath);
    if(output!="")
    {
        QStringList lines=output.split('\n');

        // Read file line by line
        for(int i=0;i<lines.count();i++)
        {
            QString line = lines[i].trimmed();

            // Skip empty lines
            if (line.isEmpty())
                continue;

            QList<QByteArray> parts;

            QStringList partsList = line.split(',');

            for (int i = 0; i < partsList.size(); ++i) {
                parts.append(partsList.at(i).toUtf8());
            }

            if(loginID==QString(parts[0]).trimmed())
            {
                if (parts.size() == 7) { // Ensure we have all 7 parts
                    UserEntry entry;
                    entry.userID = QString(parts[0]).trimmed();
                    entry.authorityLevel = QString(parts[1]).trimmed();
                    entry.date = QString(parts[2]).trimmed();
                    entry.time = QString(parts[3]).trimmed();
                    entry.action = QString(parts[4]).trimmed();
                    entry.inFile = QString(parts[5]).trimmed();
                    entry.outFile = QString(parts[6]).trimmed();
                    user_run_logs.append(entry);
                }
            }
        }
    }
}

QString UserHistory::generateReport()
{
    // Generate HTML table
    QString html = "<html><head><style>"
                   "table { border-collapse: collapse; width: 100%; }"
                   "th, td { border: 1px solid black; padding: 8px; text-align: left; }"
                   "th { background-color: #f2f2f2; }"
                   "</style></head><body>"
                   "<h2>User Execution History Report</h2>"
                   "<p>Date: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") + "</p>"
                   "<p>Time: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                   "<table>"
                   "<tr><th>User ID</th><th>Authority Level</th><th>Date</th><th>Time</th><th>Action</th><th>In File</th><th>Out File</th></tr>";

    for (int i = 0; i < user_run_logs.size(); ++i) {
        const UserEntry &log = user_run_logs.at(i);
        html += "<tr>"
                "<td>" + log.userID + "</td>"
                               "<td>" + log.authorityLevel + "</td>"
                                       "<td>" + log.date + "</td>"
                             "<td>" + log.time + "</td>"
                             "<td>" + log.action + "</td>"
                               "<td>" + log.inFile + "</td>"
                               "<td>" + log.outFile + "</td>"
                                "</tr>";
    }

    html += "</table></body></html>";

    return html;
}

void UserHistory::saveUserData(const UserEntry &entry)
{
    QString dirPath = globalVariables["CONFIG_DIR_PATH"];
    filePath=globalVariables["USER_LOG_BIN_PATH"];

    // Check if the directory exists, and create it if it does not
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            qWarning() << "Failed to create directory:" << dirPath;
            return;
        }
    }

    if (filePath.isEmpty())
    {
        QMessageBox::warning(this, "Error", "No file selected.");
        return; // Exit if no file is selected
    }

    //decrypt file
    QString output= decryptLoginFile(filePath);

    // Format user data as "user","password","authority"
    QString out = entry.userID + "," + entry.authorityLevel + "," + entry.date +
                  "," + entry.time + "," + entry.action + "," + entry.inFile + "," + entry.outFile +"\n";
    output +=out;

    encryptLoginFile(filePath, output);
}

void UserHistory::encryptLoginFile(QString filePath, QString output)
{
    QByteArray aesKey = crypt->deriveKey(output, salt, 10000);

    if (aesKey.isEmpty()) {
        QMessageBox::warning(this, "Error", "Key is empty!");
        return;
    }

    QByteArray ivAndCiphertext = crypt->encrypt(output,aesKey);//QString::fromUtf8(output), aesKey);

    // XOR key with salt
    QByteArray xorKey = aesKey;
    for (int i = 0; i < xorKey.size(); ++i) {
        xorKey[i] = xorKey[i] ^ salt[i % salt.size()];
    }

    // Interleave the XOR-ed key bytes with the encrypted data
    QByteArray interleavedData;
    int keySize = xorKey.size();
    int dataSize = ivAndCiphertext.size();

    // Ensure that the keySize is not greater than the dataSize
    int minSize = qMin(keySize, dataSize);

    for (int i = 0; i < minSize; ++i) {
        interleavedData.append(xorKey[i]);
        interleavedData.append(ivAndCiphertext[i]);
    }

    // Append remaining part of ivAndCiphertext if it's larger
    if (dataSize > keySize) {
        interleavedData.append(ivAndCiphertext.mid(keySize));
    }
    else
    {
        interleavedData.append(xorKey.mid(dataSize));
    }

    // Convert to 0/1 string
    QByteArray combinedBinaryString = toBinaryString(interleavedData);

    QFile encryptFile(filePath);

    if (!encryptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, "Error", "Cannot open file for writing.");
        return; // Exit if the file cannot be opened
    }

    encryptFile.write(combinedBinaryString);
    encryptFile.close();

}

QString UserHistory::decryptLoginFile(QString filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "Cannot open login file.");
        return ""; // Exit if the file cannot be opened
    }

    QByteArray combinedBinaryString = file.readAll();
    file.close();

    bool validation=true;
    // Validate if the input data contains only '0' and '1'
    for (int i = 0; i < combinedBinaryString.size(); ++i)
    {
        char c = combinedBinaryString.at(i);
        if (c != '0' && c != '1')
        {
            validation= validation & false;
        }
        else
        {
            validation= validation & true;
        }
    }
    if(!validation)
    {
        QMessageBox::warning(this, "Error", "The file content is not in the expected format.");
        return "";
    }

    // Convert binary string back to QByteArray
    QByteArray interleavedData = fromBinaryString(combinedBinaryString);

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

    QString output = crypt->decrypt(ivAndCiphertext, aesKey);

    return output;
}


