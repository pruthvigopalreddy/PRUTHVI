#ifndef CREATELOGIN_H
#define CREATELOGIN_H

#include <QWidget>
#include "crypto.h"

namespace Ui {
class CreateLogin;
}

class CreateLogin : public QWidget
{
    Q_OBJECT

public:
    explicit CreateLogin(QWidget *parent = nullptr);
    ~CreateLogin();
    void pwdChange(QString loginID, int authority);
    void addUser(QString loginID, QString loginPWD, int authority);
    void encryptLoginFile(QString filePath, QString output);
    bool checkLoginIDAvailability(QString output);

    crypto *crypt_obj;

signals:
    void showParent();

private slots:
    void enableADMINLogin();
    void enableCMDRLogin();
    void on_createLogin_clicked();
    void on_ClosepushButton_clicked();
    QString decryptLoginFile(QString filePath);
    QString editPwd(QString loginID, QString loginPWD, QString output);

private:
    Ui::CreateLogin *ui;
    QString filePath;
    bool newLoginAuthorisation=false;
    int loginSelection=0;
    bool pwdChangeCheck=false;
    int authority=0;
    QByteArray salt;
};

#endif // CREATELOGIN_H
