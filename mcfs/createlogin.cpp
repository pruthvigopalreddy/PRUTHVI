#include "createlogin.h"
#include "ui_createlogin.h"
#include <unistd.h>
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <QTextStream>
#include <QTimer>

extern QMap<QString, QString> globalVariables;

CreateLogin::CreateLogin(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CreateLogin)
{
    ui->setupUi(this);

    QString system = globalVariables["SYSTEM"];
    QString title= "Simulator for "+system+" Version: 1.0";
    this->setWindowTitle(title);
    ui->TitleLabel->setText(title);

    ui->LoginWidget->setEnabled(true);
    crypt_obj=new crypto();

    //32 byte salt
    salt = QByteArray::fromHex("414b4153482d5345442d2d4543494c2d2d414b4153482d2d5345442d4543494c"); // "AKASH-SED--ECIL--AKASH--SED-ECIL"

    connect(ui->ADMINpushButton, &QPushButton::clicked, this, &CreateLogin::enableADMINLogin);
    connect(ui->CMDRpushButton, &QPushButton::clicked, this, &CreateLogin::enableCMDRLogin);

}

CreateLogin::~CreateLogin()
{
    delete ui;
}

void CreateLogin::enableADMINLogin()
{
    //grey color background
    ui->createLogin->setStyleSheet("background-color : grey");
    ui->CMDRpushButton->setStyleSheet("background-color : grey");

    QPalette pal = ui->ADMINpushButton->palette();
    pal.setColor(QPalette::Button, QColor(255, 165, 0));
    ui->ADMINpushButton->setAutoFillBackground(true);
    ui->ADMINpushButton->setPalette(pal);
    ui->ADMINpushButton->update();

    ui->LoginWidget->setStyleSheet("background-color : orange");

    loginSelection=globalVariables["ADMIN"].toInt();
}

void CreateLogin::enableCMDRLogin()
{
    //grey color background
    ui->createLogin->setStyleSheet("background-color : grey");
    ui->ADMINpushButton->setStyleSheet("background-color : grey");

    QPalette pal = ui->CMDRpushButton->palette();
    pal.setColor(QPalette::Button, QColor(0,128,0));
    ui->CMDRpushButton->setAutoFillBackground(true);
    ui->CMDRpushButton->setPalette(pal);
    ui->CMDRpushButton->update();

    ui->LoginWidget->setStyleSheet("background-color : green");
    loginSelection=globalVariables["COMMANDER"].toInt();
}


void CreateLogin::on_ClosepushButton_clicked()
{
    this->close();
    enableADMINLogin();
    emit showParent();
}

void CreateLogin::on_createLogin_clicked()
{
    if(loginSelection>=globalVariables["ADMIN"].toInt())
    {
        QString loginID=ui->LoginIDText->text();
        QString loginPWD=ui->LoginPWDText->text();
        QString loginRepeatPWD=ui->LoginRepeatPWDText->text();

        if(loginPWD!=loginRepeatPWD)
        {
            QMessageBox::warning(this, "Matching Error", "Passwords dosen't match!!");
            ui->LoginPWDText->clear();
            ui->LoginRepeatPWDText->clear();
            return;
        }

        if(loginID!="" && loginPWD!="")
        {
            filePath=globalVariables["CREDENTIALS_BIN_PATH"];
            if (filePath.isEmpty()) {
                QMessageBox::warning(this, "Selection Error", "No File is Selected!!");
                ui->LoginIDText->clear();
                ui->LoginPWDText->clear();
                ui->LoginRepeatPWDText->clear();
                return; // Exit if no file is selected
            }

            //decrypt file
            QString output= decryptLoginFile(filePath);

            if(pwdChangeCheck)
            {
                QString out=editPwd(loginID,loginPWD,output);
                output=out;
                QMessageBox::information(this, "Success", loginID+" PWD updated successfully.");
            }
            else
            {
                //check if login ID already exists
                if(checkLoginIDAvailability(output))
                {
                    // Format user data as "user","password","authority"
                    QString out = loginID + "," + loginPWD + "," + QString::number(loginSelection) +"\n";
                    output +=out;
                    if(loginSelection==globalVariables["ADMIN"].toInt())
                    {
                        QMessageBox::information(this, "Success", "New Operator user added successfully.");
                    }
                    else if(loginSelection==globalVariables["COMMANDER"].toInt())
                    {
                        QMessageBox::information(this, "Success", "New Commander user added successfully.");
                    }
                }
                else
                {
                    QMessageBox::warning(this, "ERROR", "Login ID already exists.");
                    return;
                }
            }
            encryptLoginFile(filePath, output);
        }
        ui->LoginIDText->clear();
        ui->LoginPWDText->clear();
        ui->LoginRepeatPWDText->clear();

        if(!newLoginAuthorisation)
        {
            QMessageBox::information(this, "LogOut", loginID+" Logging Out.");
            emit showParent();
            newLoginAuthorisation=false;
        }

        ui->ADMINpushButton->setEnabled(true);
        ui->CMDRpushButton->setEnabled(true);
    }
    else
    {
        QMessageBox::warning(this, "Error", "Click on Icon for User Type Creation.");
        ui->LoginIDText->clear();
        ui->LoginPWDText->clear();
        ui->LoginRepeatPWDText->clear();
        return;
    }
}

bool CreateLogin::checkLoginIDAvailability(QString output)
{
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

            // Parse the line assuming it's in the format "user","password","authority"
            QStringList parts = line.split(',');
            if (parts.size() != 3) {
                QMessageBox::warning(this, "Error", "File format is incorrect.");
                return false; // Exit if the file format is incorrect
            }
            // Check if the first part matches the ID to check
            if (parts.at(0) == ui->LoginIDText->text()) {
                return false; // ID found
            }
        }
    }
    return true; // ID not found
}

void CreateLogin::encryptLoginFile(QString filePath, QString output)
{
    QByteArray aesKey = crypt_obj->deriveKey(output, salt, 10000);

    if (aesKey.isEmpty()) {
        QMessageBox::warning(this, "Error", "Key is empty!");
        return;
    }

    QByteArray ivAndCiphertext = crypt_obj->encrypt(output,aesKey);//QString::fromUtf8(output), aesKey);

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
    QByteArray combinedBinaryString = crypt_obj->toBinaryString(interleavedData);

    QFile encryptFile(filePath);

    if (!encryptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(nullptr, "Error", "Cannot open file for writing.");
        return; // Exit if the file cannot be opened
    }

    encryptFile.write(combinedBinaryString);
    encryptFile.close();

}

QString CreateLogin::decryptLoginFile(QString filePath)
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
    QByteArray interleavedData = crypt_obj->fromBinaryString(combinedBinaryString);

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

    QString output = crypt_obj->decrypt(ivAndCiphertext, aesKey);

    return output;
}

void CreateLogin::pwdChange(QString loginID, int authority)
{
    ui->LoginIDText->setText(loginID);
    ui->LoginIDText->setEnabled(false);
    if(authority==globalVariables["ADMIN"].toInt())
    {
        ui->CMDRpushButton->setEnabled(false);
        enableADMINLogin();
        pwdChangeCheck=true;
    }
    else if(authority==globalVariables["COMMANDER"].toInt())
    {
        ui->ADMINpushButton->setEnabled(false);
        enableCMDRLogin();
        pwdChangeCheck=true;
    }
    this->authority=authority;
}

QString CreateLogin::editPwd(QString loginID,QString loginPWD, QString output)
{
    QStringList lines = output.split('\n'); // Split the file data into lines
    bool found = false;

    for (int i = 0; i < lines.size(); ++i) {
        QString line = lines[i];
        QStringList parts = line.split(',');
        if (parts.size() == 3 && parts[0] == loginID && parts[2]== QString::number(authority)) {
            parts[1] = loginPWD; // Update the password
            lines[i] = parts.join(','); // Join the updated parts back into a line
            found = true;
        }
    }

    // Convert the updated lines back into a single QString
    QString updatedFileData = lines.join('\n');

    return found ? updatedFileData : QString(); // Return the updated data or empty string if not found
}

void CreateLogin::addUser(QString loginID, QString loginPWD, int authority)
{
    //add user fn
    ui->LoginIDText->setText(loginID);
    ui->LoginPWDText->setText(loginPWD);
    ui->LoginRepeatPWDText->setText(loginPWD);
    loginSelection=authority;
    newLoginAuthorisation=true;
    on_createLogin_clicked();

}
