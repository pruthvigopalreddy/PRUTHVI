#ifndef ADDUSERDIALOG_H
#define ADDUSERDIALOG_H

#include <QDialog>

namespace Ui {
class AddUserDialog;
}

class AddUserDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddUserDialog(QWidget *parent = nullptr);
    ~AddUserDialog();
    QString loginID() const;
    QString loginPWD() const;
    int authority() const;

private slots:
    void on_buttonBox_accepted();

private:
    Ui::AddUserDialog *ui;
};

#endif // ADDUSERDIALOG_H
