#ifndef USERLISTDIALOG_H
#define USERLISTDIALOG_H

#include <QDialog>
#include "loghistory.h"
namespace Ui {
class UserListDialog;
}

class UserListDialog : public QDialog
{
    Q_OBJECT

public:
    explicit UserListDialog(const QString &loginID,const int authority, const QString &filePath, QWidget *parent = nullptr);
    ~UserListDialog();

private slots:
    void updateList_clicked(QString action);
    void on_close_clicked();
    void on_deleteUser_clicked();
    void on_editAuthority_clicked();
    void on_editPWD_clicked();
    void logHistoryEntry(QString action);

private:
    QPushButton *changePasswordButton;
    void loadFile();

private:
    Ui::UserListDialog *ui;
    QString filePath;
    QString loginID;
    int authority;
    LogHistory *log_history_object;
    QString selectedUser="";
    QStringList pwd;
};

#endif // USERLISTDIALOG_H
