#include "adduserdialog.h"
#include "ui_adduserdialog.h"

extern QMap<QString, QString> globalVariables;

AddUserDialog::AddUserDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::AddUserDialog)
{
    ui->setupUi(this);
}

AddUserDialog::~AddUserDialog()
{
    delete ui;
}
QString AddUserDialog::loginID() const
{
    return ui->loginID->text();
}

QString AddUserDialog::loginPWD() const
{
    return ui->loginPWD->text();
}

int AddUserDialog::authority() const
{
    return ui->authority->currentIndex();
}

void AddUserDialog::on_buttonBox_accepted()
{

}
