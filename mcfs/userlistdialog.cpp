#include "userlistdialog.h"
#include "ui_userlistdialog.h"
#include <QMessageBox>
#include <QInputDialog>
#include <QFile>
#include "createlogin.h"
#include <QSpinBox>

extern QMap<QString, QString> globalVariables;

UserListDialog::UserListDialog(const QString &loginID, const int authority, const QString &filePath, QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::UserListDialog)
{
    ui->setupUi(this);
    log_history_object=new LogHistory();

    ui->tableWidget->setColumnCount(3);
    ui->tableWidget->setHorizontalHeaderLabels({"User", "Password", "Authority Level"});
    ui->tableWidget->horizontalHeader()->setStretchLastSection(true);
    ui->users->setText(globalVariables.value("ADMIN")+"-> Admin<br>"+globalVariables.value("COMMANDER")+"-> Commander");
    this->filePath=filePath;
    this->loginID=loginID;
    this->authority=authority;
    loadFile();
}

UserListDialog::~UserListDialog()
{
    delete ui;
}

void UserListDialog::loadFile()
{
    pwd.clear();

    //decrypt file
    QString output= log_history_object->decryptLoginFile(filePath);
    ui->tableWidget->setRowCount(0);  // Clear existing rows
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
                return; // Exit if the file format is incorrect
            }

            int rowPosition = ui->tableWidget->rowCount();
            ui->tableWidget->insertRow(rowPosition);

            // Check if this is the row for the current loginID
            bool isCurrentUser = (parts.at(0) == loginID);

            for (int column = 0; column < parts.size(); ++column)
            {
                QTableWidgetItem *item = new QTableWidgetItem(parts.at(column));
                item->setTextAlignment(Qt::AlignCenter);

                if (column == 1) { // Password column
                    pwd.append(parts.at(column));
                    item->setText( QString('*').repeated(5)); // Replace password with stars
                } else {
                    item->setText(parts.at(column)); // For other columns
                }

                // Set the item to be non-editable if it's the current user
                if (isCurrentUser) {
                    item->setBackground(Qt::yellow);  // Set background color to yellow
                }

                ui->tableWidget->setItem(rowPosition, column, item);
            }
        }
    }
}

void UserListDialog::on_editPWD_clicked()
{
    bool ok;
    QString newPassword;

    if (ui->tableWidget->currentRow() >= 0)
    {
        QTableWidgetItem* item = ui->tableWidget->item(ui->tableWidget->currentRow(), 0);
        if(item->background()!=Qt::yellow)
        {
            selectedUser = ui->tableWidget->item(ui->tableWidget->currentRow(), 0)->text();
            newPassword = QInputDialog::getText(this, "Change Password",
                                                QString("Enter new password for %1").arg(selectedUser),
                                                QLineEdit::Password, QString(), &ok);

            if (ok && !newPassword.isEmpty())
            {
                pwd[ui->tableWidget->currentRow()]=newPassword;
                updateList_clicked("Password Changed");
            }
        }
        else
        {
            QMessageBox::warning(this, "Incorrect Selection", "Can not change the password for current selection from user list.");
        }

    }
    else
    {
        QMessageBox::warning(this, "No Selection", "Please select a user to change the password.");
    }
}

void UserListDialog::on_editAuthority_clicked()
{
    if (ui->tableWidget->currentRow() >= 0)
    {
        QTableWidgetItem* item = ui->tableWidget->item(ui->tableWidget->currentRow(), 0);
        if(item->background()!=Qt::yellow)
        {
            selectedUser = ui->tableWidget->item(ui->tableWidget->currentRow(), 0)->text();

            // Retrieve values from global variables, with default values if not found
            int commanderValue = globalVariables.value("COMMANDER", 0).toInt();
            int adminValue = globalVariables.value("ADMIN", 0).toInt();

            // Prepare the options for the dialog, associating display names with their values
            QStringList options;
            QStringList values;

            options << "Admin" << "Commander"; // Display names
            values << QString::number(adminValue) << QString::number(commanderValue); // Corresponding values

            // Open the input dialog for selection
            bool ok;
            QString selectedValue = QInputDialog::getItem(this,
                                                          "Change Authority",
                                                          QString("Select authority level for %1").arg(selectedUser),
                                                          options,
                                                          0, // Default index (first option)
                                                          false, // Editable
                                                          &ok); // Reference to a bool

            if (ok && !selectedValue.isEmpty()) {
                // Get the index of the selected option
                int index = options.indexOf(selectedValue);
                if (index != -1) {
                    // Retrieve the corresponding value based on the selected index
                    int newAuthority = values[index].toInt();

                    // Update the table widget with the new authority level
                    ui->tableWidget->item(ui->tableWidget->currentRow(), 2)->setText(QString::number(newAuthority));

                    // Optionally, provide feedback about the change
                    updateList_clicked("Authority Level Changed");
                }
                else
                {
                    QMessageBox::information(this, "Selection", "Invalid selection made.");
                }
            }
            else
            {
                QMessageBox::information(this, "Selection", "User canceled the selection or invalid selection made.");
            }
        }
        else
        {
            QMessageBox::warning(this, "Incorrect Selection", "Can not change the authority level for current selection from user list.");
        }
    }
    else
    {
        QMessageBox::warning(this, "No Selection", "Please select a user to change the authority level.");
    }
}

void UserListDialog::on_deleteUser_clicked()
{
    if (ui->tableWidget->currentRow() >= 0)
    {
        QTableWidgetItem* item = ui->tableWidget->item(ui->tableWidget->currentRow(), 0);
        if(item->background()!=Qt::yellow)
        {
            int row = ui->tableWidget->currentRow();
            ui->tableWidget->removeRow(row);
            pwd.removeAt(row); // Remove the password from the list
            updateList_clicked("");
        }
        else
        {
            QMessageBox::warning(this, "Incorrect Selection", "Can not delete current selection from user list.");
        }
    }
    else
    {
        QMessageBox::warning(this, "No Selection", "Please select a user to delete.");
    }
}

void UserListDialog::updateList_clicked(QString action)
{
    QString result;
    int rowCount = ui->tableWidget->rowCount();
    int columnCount = ui->tableWidget->columnCount();

    for (int row = 0; row < rowCount; ++row) {
        QStringList rowData;
        for (int col = 0; col < columnCount; ++col) {
            QTableWidgetItem *item = ui->tableWidget->item(row, col);
            if(col==1)
            {
                if (item) {
                    rowData << pwd[row];
                } else {
                    rowData << "";
                }
            }
            else
            {
                if (item) {
                    rowData << item->text();
                } else {
                    rowData << "";
                }
            }
        }
        result += rowData.join(',') + '\n';
    }

    CreateLogin login_obj;
    login_obj.encryptLoginFile(filePath, result);

    QMessageBox::information(this, "Success", "User list successfully updated.");
    if(action!="")
    {
        logHistoryEntry(action);
    }
}

void UserListDialog::logHistoryEntry(QString action)
{
    LogEntry newEntry;
    newEntry.userID = selectedUser;

    if(authority==globalVariables["ADMIN"].toInt())
    {
        newEntry.authorityLevel = "Operator";
        newEntry.authorisedBy = "--";
    }
    else if(authority==globalVariables["COMMANDER"].toInt())
    {
        newEntry.authorityLevel = "Commander";
        newEntry.authorisedBy = loginID;
    }

    // Get the current date and time
    QDateTime current = QDateTime::currentDateTime();
    newEntry.date = current.toString("dd-MM-yyyy"); // Format: DD-MM-YYYY
    newEntry.time = current.toString("HH:mm:ss") + " IST"; // Format: HH:MM:SS with IST timezone

    newEntry.action = action;

    log_history_object->saveLoginData(newEntry); // Save the log entry
}

void UserListDialog::on_close_clicked()
{
    this->hide();
}


