#include "mainwindow.h"
#include "qtranslator.h"
#include "ui_mainwindow.h"
#include <QFile>
#include <QDebug>
#include <QByteArray>
#include <QMessageBox>
#include <QFileDialog>
#include <QRandomGenerator>
#include <QTreeWidgetItem>
#include <QTimer>
#include "adduserdialog.h"
#include "userlistdialog.h"
#include <QMouseEvent>
#include <QPdfWriter>
#include <QTextDocument>
#include <QDesktopServices>
#include <QUrl>
#include <QtConcurrent/QtConcurrentRun>
#include <QMetaObject>
#include <QNetworkInterface>
#include <QPrinter>
#include <QPrintDialog>
#include <unistd.h>
#include <string.h>

extern QMap<QString, QString> globalVariables;

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->retranslateUi(this);

    ui->CMDRpushButton->click();

    ui->appStackWidget->setCurrentIndex(0);

    systemCheck();

    rusTranslator=new QTranslator();

    connect(ui->language, &QCheckBox::toggled,this, &MainWindow::setLanguage);
    connect(ui->checkBox, &QCheckBox::toggled, this, &MainWindow::onbyPassToggled);

    crypt = new crypto();
    createlogin_obj = new CreateLogin();
    log_history_obj= new LogHistory();
    user_history_obj= new UserHistory();
    instr_obj=new InstrConnection();
    driver_obj=new Drivers();

    //emit updateInstrStatus() signal from instrObject is connected here to update instr status
    connect (instr_obj, &InstrConnection::updateInstrStatus, this, &MainWindow::updateInstrStatus);

    //default instruments map status
    instruments.insert(globalVariables["EXT-PS_HOST_NAME"],0);
    instruments.insert(globalVariables["STATUS-PS_HOST_NAME"],0);
    instruments.insert(globalVariables["INT-PS_HOST_NAME"],0);
    instruments.insert(globalVariables["PYRO-PS_HOST_NAME"],0);
    if(akash=="prime")
    {
        instruments.insert(globalVariables["SEEKER-PS_HOST_NAME"],0);
    }
    instruments.insert(globalVariables["PWR-MTR_HOST_NAME"],0);
    instruments.insert(globalVariables["SIG-GEN_HOST_NAME"],0);
    instruments.insert(globalVariables["UNI-CTR_HOST_NAME"],0);
    instruments.insert(globalVariables["WAVE-GEN_HOST_NAME"],0);
    instruments.insert(globalVariables["DSO_HOST_NAME"],0);

    //32 byte salt
    salt = QByteArray::fromHex("414b4153482d5345442d2d4543494c2d2d414b4153482d2d5345442d4543494c"); // "AKASH-SED--ECIL--AKASH--SED-ECIL"

    verifyNetworkList();

    // Create and configure QTreeWidget
    treeWidget = new QTreeWidget(ui->widget);
    treeWidget->setGeometry(ui->widget->geometry().x(),ui->widget->geometry().y(),ui->widget->geometry().width()-25,ui->widget->geometry().height()-25);
    treeWidget->setHeaderHidden(true);  // Optionally hide the header
    treeWidget->setIconSize(QSize(32, 32));  // Adjust icon size if needed

    // Connect the Open action to the slot
    connect(ui->open, &QPushButton::clicked, this, &MainWindow::onOpenFile);
    connect(treeWidget, &QTreeWidget::itemDoubleClicked, this, &MainWindow::treeWidget_itemClicked);

    // Populate the QTreeWidget
    populateFileSystem();
    treeWidget->expandToDepth(1);

    QPixmap pixmap1(":/images/folder.png");
    if (pixmap1.isNull()) {
        //qWarning() << "folder Icon not loaded correctly.";
    }
    QPixmap pixmap2(":/images/file.png");
    if (pixmap2.isNull()) {
        //qWarning() << "file Icon not loaded correctly.";
    }

    // Set up a QTimer to update the date and time every second
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::updateDateTime);
    timer->start(1000); // Update every second

    // Initial update
    updateDateTime();

    ui->stackedWidget->setCurrentIndex(globalVariables["FILE_SYSTEM_CHECK_PAGE"].toInt());
    ui->user_stackedWidget->setCurrentIndex(globalVariables["FILE_SYSTEM_TREE_PAGE"].toInt());
    ui->tableWidget->resizeColumnsToContents();

    connect(ui->LoginpushButton, &QPushButton::clicked, this, &MainWindow::userLogin);
    connect(ui->ClosepushButton, &QPushButton::clicked, this, &MainWindow::closeWindow);
    connect(this, &MainWindow::pwdChangeSignal, createlogin_obj, &CreateLogin::pwdChange);
    connect(createlogin_obj, &CreateLogin::showParent, this, &MainWindow::showLogin);

    // Create worker thread and SerialCOMM object
    serialThread = new QThread(this);
    serialcomm_obj = new SerialCOMM();          // create in main thread for now
    serialcomm_obj->moveToThread(serialThread); // move to worker thread

    // When the thread starts, initialize SerialCOMM inside that thread
    connect(serialThread, &QThread::started, serialcomm_obj, &SerialCOMM::init);
    // Cleanup when thread finishes
    connect(serialThread, &QThread::finished, serialcomm_obj, &QObject::deleteLater);
    connect(serialThread, &QThread::finished, serialThread, &QObject::deleteLater);
    connect(serialcomm_obj, &SerialCOMM::updateProgressBarSignal, this, &MainWindow::updateProgressBar,Qt::QueuedConnection);
    connect(serialcomm_obj, &SerialCOMM::obpCommunicationSignal, this, &MainWindow::obpCommunication,Qt::QueuedConnection);
    connect(serialcomm_obj, &SerialCOMM::responseReady, this, &MainWindow::onResponse,Qt::QueuedConnection);

    // Start worker thread
    serialThread->start();

    ui->OBPtextBrowser->document()->setMaximumBlockCount(2000);
    QString status=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
    if(status=="OPEN")
    {
        instr_obj->screenClear();
        instr_obj->earth2NeutralInIt();
        sleep(1);
        instr_obj->connectionClose();
    }

    loadingMovie = new QMovie(":/images/loading.gif", QByteArray(), this);
    ui->loadingLabel->setMovie(loadingMovie);
    ui->loadingLabel->setVisible(false);
    ui->loadLabel->setMovie(loadingMovie);
    ui->loadLabel->setVisible(false);

    connect(this, &MainWindow::mpsAutoSignal, this, &MainWindow::on_mpsAutoSignal);
    connect(this, &MainWindow::statusAutoSignal, this, &MainWindow::on_statusAutoSignal);
    connect(this, &MainWindow::obpAutoSignal, this, &MainWindow::on_obpAutoSignal);
    connect(this, &MainWindow::scuAutoSignal, this, &MainWindow::on_scuAutoSignal);
    connect(this, &MainWindow::rpfAutoSignal, this, &MainWindow::on_rpfAutoSignal);
    connect(this, &MainWindow::pyroAutoSignal, this, &MainWindow::on_pyroAutoSignal);
    connect(this, &MainWindow::selfTestAutoSignal, this, &MainWindow::on_selfTestAutoSignal);
    connect(instr_obj, &InstrConnection::selfTestSignal, this, &MainWindow::selfTestSignal);

    mwInIt();

    ui->monSimSlider->installEventFilter(this);
    ui->txrxSlider->installEventFilter(this);
    ui->sideSlider->installEventFilter(this);
    ui->relaySlider->installEventFilter(this);

    //NE ARM RELAY off
    driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

    //NE SAFE RELAY on
    driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

    if(akash=="export")
    {
        ui->skrPSLabel->setStyleSheet("color: white;");
        ui->skrSPMLabel->setStyleSheet("color: white;");
        ui->skrHTRLabel->setStyleSheet("color: white;");
        ui->skrTXLabel->setStyleSheet("color: white;");
    }
    else if(akash=="prime")
    {
        ui->skrPSLabel->setStyleSheet("color: black;");
        ui->skrSPMLabel->setStyleSheet("color: black;");
        ui->skrHTRLabel->setStyleSheet("color: black;");
        ui->skrTXLabel->setStyleSheet("color: black;");
    }
    else
    {
        ui->skrPSLabel->setStyleSheet("color: white;");
        ui->skrSPMLabel->setStyleSheet("color: white;");
        ui->skrHTRLabel->setStyleSheet("color: white;");
        ui->skrTXLabel->setStyleSheet("color: white;");
    }
}

MainWindow::~MainWindow()
{
    delete crypt;
    delete createlogin_obj;
    delete log_history_obj;
    delete user_history_obj;
    delete instr_obj;
    delete ui;

    //all opto do close
    mpsRelayOFF();
    statusRelayOFF();
    obpRelayOFF();
    pyroRelayOFF();
    mpsOutOFF();

    // Ask worker to shutdown (queued to worker thread)
        if (serialcomm_obj) {
            QMetaObject::invokeMethod(serialcomm_obj, "shutdown", Qt::QueuedConnection);
        }

        // Stop thread cleanly
        if (serialThread) {
            serialThread->quit();
            if (!serialThread->wait(2000)) {
                serialThread->terminate(); // last resort
                serialThread->wait();
            }
        }
}

void MainWindow::systemCheck()
{
    system = globalVariables["SYSTEM"];
    QString title= "Simulator for "+system+" Version: 1.0";
    this->setWindowTitle(title);
    ui->TitleLabel->setText(title);
    qDebug()<<title;

    if(system=="MSMC-P/MCF-P")
    {
        akash="prime";
        if(ui->language->isChecked())
        {
            ui->rpfDfsLabel->setText("Проверка DFS");
            ui->rpfCheckLabel->setText("Проверка RPF");
        }
        else
        {
           ui->rpfDfsLabel->setText("DFS Check");
           ui->rpfCheckLabel->setText("RPF Check");
        }

        ui->mpsRPFLabel->setText("DFS");
    }
    else if(system=="MSMC-E/MCF-E")
    {
        akash="export";
        if(ui->language->isChecked())
        {
            ui->rpfDfsLabel->setText("Проверка DFS");
            ui->rpfCheckLabel->setText("Проверка RPF");
        }
        else
        {
         ui->rpfDfsLabel->setText("RPF Check");
         ui->rpfCheckLabel->setText("RPF Check");
        }
        ui->mpsRPFLabel->setText("RPF");
    }
    else
    {
        akash="export";
        if(ui->language->isChecked())
        {
            ui->rpfDfsLabel->setText("Проверка DFS");
            ui->rpfCheckLabel->setText("Проверка RPF");
        }
        else
        {
         ui->rpfDfsLabel->setText("RPF Check");
         ui->rpfCheckLabel->setText("RPF Check");
        }
        ui->mpsRPFLabel->setText("RPF");
    }
}

void MainWindow::setLanguage(bool checked)
{
    if(checked)
    {
        setLanguageRussian();
    }
    else
    {
        setLanguageEnglish();
    }
}

void MainWindow::setLanguageRussian()
{
    if (QFile::exists(":/languages/rus.qm"))
    {
        bool load=rusTranslator->load(":/languages/rus.qm");
        if(load)
        qApp->installTranslator(rusTranslator);
        ui->retranslateUi(this);
        systemCheck();
        QString title= "Симулятор для " + system + " Версия: 1.0";
        this->setWindowTitle(title);
        ui->TitleLabel->setText(title);
    }
    else
    {
        QMessageBox::warning(this, "Error","Russian translation file not found! <br> Файл перевода на русский не найден!");
    }
}

void MainWindow::setLanguageEnglish()
{
    qApp->removeTranslator(rusTranslator);
    ui->retranslateUi(this);
    systemCheck();
}

void MainWindow::onbyPassToggled(bool checked)
{
    if(checked)
    {
        mpsByPassStatus=true;
        mpsCheckStatus=true;
    }
    else
    {
        mpsByPassStatus=false;
        mpsCheckStatus=false;
    }
}

QString MainWindow::loadExpectedChecksum()
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

void MainWindow::showLogin()
{
    createlogin_obj->hide();
    ui->appStackWidget->setCurrentIndex(0);
    this->showFullScreen();
}

void MainWindow::updateDateTime()
{
    QString currentDateTime = QDateTime::currentDateTime().toString("dd-MM-yyyy HH:mm:ss");
    ui->time->setText(currentDateTime);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    if(event)
    {
        //dummy
    }
    treeWidget->setGeometry(ui->widget->geometry().x(),ui->widget->geometry().y(),ui->widget->geometry().width()-15,ui->widget->geometry().height()-15);
}

void MainWindow::closeWindow()
{
    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    delete crypt;
    delete createlogin_obj;
    delete log_history_obj;
    delete user_history_obj;
    delete instr_obj;
    delete ui;

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    //all opto do close
    mpsRelayOFF();
    statusRelayOFF();
    obpRelayOFF();
    pyroRelayOFF();
    mpsOutOFF();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    // Ask worker to shutdown (queued to worker thread)
        if (serialcomm_obj) {
            QMetaObject::invokeMethod(serialcomm_obj, "shutdown", Qt::QueuedConnection);
        }

        // Stop thread cleanly
        if (serialThread) {
            serialThread->quit();
            if (!serialThread->wait(2000)) {
                serialThread->terminate(); // last resort
                serialThread->wait();
            }
        }
    // After heavy operations finish:
    if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
    ui->loadLabel->setVisible(false);
    this->close();
}

void MainWindow::on_ADMINpushButton_clicked()
{
    QPalette pal = ui->ADMINpushButton->palette();
    pal.setColor(QPalette::Button, QColor(255, 165, 0));
    ui->ADMINpushButton->setAutoFillBackground(true);
    ui->ADMINpushButton->setPalette(pal);
    ui->ADMINpushButton->update();
    ui->LoginWidget->setStyleSheet("background-color : orange");
    ui->CMDRpushButton->setStyleSheet("background-color : grey");
    loginSelection=globalVariables["ADMIN"].toInt();
}

void MainWindow::on_CMDRpushButton_clicked()
{
    QPalette pal = ui->CMDRpushButton->palette();
    pal.setColor(QPalette::Button, QColor(0,128,0));
    ui->CMDRpushButton->setAutoFillBackground(true);
    ui->CMDRpushButton->setPalette(pal);
    ui->CMDRpushButton->update();
    ui->LoginWidget->setStyleSheet("background-color : green");
    ui->ADMINpushButton->setStyleSheet("background-color : grey");
    loginSelection=globalVariables["COMMANDER"].toInt();
}

void MainWindow::userLogin()
{
    if(loginSelection>=globalVariables["ADMIN"].toInt())
    {
        bool loginStatusCheck=validateUserLogin((loginSelection == globalVariables["ADMIN"].toInt()) ? false : (loginSelection == globalVariables["COMMANDER"].toInt()) ? true : false);
        if (loginStatusCheck)
        {
            // Show spinner
            ui->loadLabel->setVisible(true);
            if (loadingMovie) loadingMovie->start();
            ui->LoginpushButton->setEnabled(false);

            // Give event loop one iteration so spinner is painted
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

            QTimer::singleShot(100, this, [=]() {
                this->setUserChanges();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50); // let spinner advance

                this->home_pressed();
                QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

                // After heavy operations finish:
                if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
                ui->loadLabel->setVisible(false);
                ui->LoginpushButton->setEnabled(true);

                ui->LoginWidget->setStyleSheet("background-color: rgb(192, 191, 188)");
                ui->LoginpushButton->setStyleSheet("background-color : grey");
                loginStatus="login";
                logHistoryEntry();
                ui->appStackWidget->setCurrentIndex(1);
                ui->user_stackedWidget->setCurrentIndex(globalVariables["AAKASH_IMAGE_PAGE"].toInt());
            });
        }
        else
        {
            if(corrupt)
            {
                //
                corrupt=false;
            }
            else
            {
                ui->LoginpushButton->setStyleSheet("background-color : red");
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this, "Ошибка", "Неверные учетные данные");
                }
                else
                {
                     QMessageBox::warning(this, "Login Error", "Incorrect Credentials Provided!!!");
                }
            }
        }
    }
    else
    {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Нажмите на иконку типа пользователя");
        }
        else
        {
            QMessageBox::warning(this, "Error", "Click on User Type Icon.");
        }
    }
    //ui->LoginIDText->clear();
    //ui->LoginPWDText->clear();
}

bool MainWindow::validateUserLogin(bool checkCMDR)
{
    QString loginID  = ui->LoginIDText->text().trimmed();
    QString loginPWD = ui->LoginPWDText->text();  // don't trim passwords

    corrupt=false;

    bool loginStatusCheck = false;

    if(loginID!="" && loginPWD!="")
    {
        filePath=globalVariables["CREDENTIALS_BIN_PATH"];

        if (filePath.isEmpty())
        {
            if(ui->language->isChecked())
            {
                QMessageBox::warning(this, "Ошибка", "Файл учетных данных не найден");
            }
            else
            {
                corrupt=true;
                QMessageBox::warning(this, "Configuration Error!!", "Supporting Configuration File is Corrupted!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            return loginStatusCheck; // Exit if no file is selected
        }

        //decrypt file
        QString output= decryptLoginFile(filePath);
        if(output!="")
        {
            int authority = 0;

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
                    if(ui->language->isChecked())
                    {
                        QMessageBox::warning(this, "Ошибка", "Неправильный формат файла");
                    }
                    else
                    {
                        corrupt=true;
                        QMessageBox::warning(this, "Configuration Error!!", "Supporting Credential File is Corrupted!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                        //QMessageBox::warning(this, "Error", "File format is incorrect.");
                    }
                    return loginStatusCheck; // Exit if the file format is incorrect
                }

                QString fileID = parts[0].remove('"');  // Remove any quotes
                QString filePWD = parts[1].remove('"'); // Remove any quotes
                int fileAuthority = parts[2].remove('"').toInt(); // Remove quotes and convert to int

                auto sameCI = [](const QString &a, const QString &b) {
                    return QString::compare(a, b, Qt::CaseInsensitive) == 0;
                };

                if (checkCMDR)
                {
                    if ( sameCI(loginID, fileID) &&
                         sameCI(loginPWD, filePWD) &&
                         fileAuthority == globalVariables["COMMANDER"].toInt() &&
                         loginSelection == fileAuthority )
                    {
                        authority = fileAuthority;
                        loginStatusCheck = true;
                        break;
                    }
                }
                else
                {
                    if ( sameCI(loginID, fileID) &&
                         sameCI(loginPWD, filePWD) &&
                         loginSelection == fileAuthority )
                    {
                        authority = fileAuthority;
                        loginStatusCheck = true;
                        break;
                    }
                }
            }
            if (loginStatusCheck)
            {
                if (authority == globalVariables["ADMIN"].toInt())
                {
                    if(ui->language->isChecked())
                    {
                        ui->statusbar->showMessage("Подключено как Оператор...", 5000);
                    }
                    else
                    {
                        ui->statusbar->showMessage("Connecting as Admin...", 5000);
                    }
                } else if (authority == globalVariables["COMMANDER"].toInt())
                {
                    if(ui->language->isChecked())
                    {
                        ui->statusbar->showMessage("Подключено как Командир...", 5000);
                    }
                    else
                    {
                        ui->statusbar->showMessage("Connecting as Commander...", 5000);
                    }
                } else
                {
                    if(ui->language->isChecked())
                    {
                        ui->statusbar->showMessage("Неизвестный уровень полномочий...", 5000);
                    }
                    else
                    {
                        ui->statusbar->showMessage("Unknown authority level...", 5000);
                    }
                }
                this->authority=authority;
                this->loginID=loginID;
                this->loginPWD=loginPWD;

                updateUser();
            }
            return loginStatusCheck;
        }
    }
    else
    {
        return loginStatusCheck;
    }
    return loginStatusCheck;
}

QString MainWindow::decryptLoginFile(QString filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Не удается открыть файл входа");
        }
        else
        {
            corrupt=true;
            QMessageBox::warning(this, "Configuration Error!!", "Supporting Credential File is Missing!!! <br> <br> *Refer Operation Manual for further Assistance!!!");
        }
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
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Содержимое файла не соответствует ожидаемому формату");
        }
        else
        {
            corrupt=true;
            QMessageBox::warning(this, "Configuration Error!!", "Supporting Credential File is Corrupted!!!");
            //QMessageBox::warning(this, "Error", "The file content is not in the expected format.");
        }
        return "";
    }

    // Convert binary string back to QByteArray
    QByteArray interleavedData =  createlogin_obj->crypt_obj->fromBinaryString(combinedBinaryString);

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

void MainWindow::on_createLogin_clicked()
{
    if(validateUserLogin(true))
    {
        this->hide();
        createlogin_obj->showFullScreen();
        ui->LoginIDText->clear();
        ui->LoginPWDText->clear();
    }
    else
    {
        ui->LoginpushButton->setStyleSheet("background-color : red");
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Введите учетные данные для создания пользователя");
        }
        else
        {
            QMessageBox::warning(this, "Error", "Enter Credentials to create a User.");
        }
    }
}

void MainWindow::pwdChange(QString loginID,int authority)
{
    emit pwdChangeSignal(loginID, authority);
    this->hide();
    createlogin_obj->showFullScreen();
}

void MainWindow::addUser(QString loginID, QString loginPWD, int authority)
{
    createlogin_obj->addUser(loginID, loginPWD, authority);
}

void MainWindow::logHistoryEntry()
{
    LogEntry newEntry;
    newEntry.userID = this->loginID;

    if(loginSelection==globalVariables["ADMIN"].toInt())
    {
        newEntry.authorityLevel = "Operator";
    }
    else if(loginSelection==globalVariables["COMMANDER"].toInt())
    {
        newEntry.authorityLevel = "Commander";
    }

    // Get the current date and time
    QDateTime current = QDateTime::currentDateTime();
    newEntry.date = current.toString("dd-MM-yyyy"); // Format: DD-MM-YYYY
    newEntry.time = current.toString("HH:mm:ss") + " IST"; // Format: HH:MM:SS with IST timezone

    newEntry.action = loginStatus;
    newEntry.authorisedBy = "--";

    log_history_obj->saveLoginData(newEntry); // Save the log entry
}


void MainWindow::verifyNetworkList()
{
    //default instruments map status
    instruments[globalVariables["EXT-PS_HOST_NAME"]]=0;
    instruments[globalVariables["STATUS-PS_HOST_NAME"]]=0;
    instruments[globalVariables["INT-PS_HOST_NAME"]]=0;
    instruments[globalVariables["PYRO-PS_HOST_NAME"]]=0;
    if(akash=="prime")
    {
        instruments[globalVariables["SEEKER-PS_HOST_NAME"]]=0;
    }
    instruments[globalVariables["PWR-MTR_HOST_NAME"]]=0;
    instruments[globalVariables["SIG-GEN_HOST_NAME"]]=0;
    instruments[globalVariables["UNI-CTR_HOST_NAME"]]=0;
    instruments[globalVariables["WAVE-GEN_HOST_NAME"]]=0;
    instruments[globalVariables["DSO_HOST_NAME"]]=0;

    instr_obj->verifyList();

    if (!firstFailedInstrument.isEmpty())
    {
        QString msg = firstFailedInstrument;

        int ret=instr_obj->disconnectedInstr(msg);

        if(ret)
        {
            instrumentsStatus = false;

            //ui->testEquiplineEdit->setText(  ui->language->isChecked() ? "HE ГО" : "BY PASSED" );

            // remove '//' in final code
            exit(0);
        }
        else
        {
            instrumentsStatus = false;

            //ui->testEquiplineEdit->setText( ui->language->isChecked() ? "HE ГО" : "NO GO" );

            // remove '//' in final code
            exit(0);
        }
        exit(0);
    }
    else
    {
        instrumentsStatus = true;

        //ui->testEquiplineEdit->setText( ui->language->isChecked() ? "ГО" : "GO" );
    }
}


void MainWindow::updateInstrStatus(QString msg, QString failedInstrument)
{
    firstFailedInstrument = failedInstrument;

    ui->tableWidget->clearContents();

    if (msg.isEmpty())
        return;

    QStringList msgList = msg.split('\n', Qt::SkipEmptyParts);
    ui->tableWidget->setRowCount(msgList.size());
    ui->tableWidget->setColumnCount(4);

    for (int i = 0; i < msgList.size(); ++i)
    {
        QStringList fields = msgList[i].split(',');
        if (fields.size() < 4)
            continue;

        instruments[fields[0]] = (fields[3] == "PASS") ? 1 : 0;

        for (int j = 0; j < fields.size(); ++j)
        {
            ui->tableWidget->setItem(i, j,
                new QTableWidgetItem(fields[j]));
        }
    }
}


void MainWindow::home_pressed()
{
    on_home_clicked();
}

void MainWindow::on_home_clicked()
{
    mwOFF();

    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    ui->browser->clear();
    ui->lineEdit->clear();
    ui->selection->clear();

    on_reload_clicked();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

       //all opto do close
       mpsRelayOFF();
       statusRelayOFF();
       obpRelayOFF();
       pyroRelayOFF();

       // Give event loop one iteration so spinner is painted
       QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

       //if(instrumentsStatus)
       {
            mpsOutOFF();
       }

    //clear all buttons
    lt_missile_flag=0;
    rt_missile_flag=0;

    reset();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    checkconfigFiles();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    countAcromagInstances();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(authority==globalVariables["ADMIN"].toInt())
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["FILE_SYSTEM_CHECK_PAGE"].toInt());
        ui->user_stackedWidget->setCurrentIndex(globalVariables["AAKASH_IMAGE_PAGE"].toInt());
    }
    else
    {
        if(ui->language->isChecked())
        {
             //add not below in final code
             if (!(acromagCheck))
             {
                 // Create the message box
                 QMessageBox msgBox(this);
                 msgBox.setIcon(QMessageBox::Critical);
                 msgBox.setWindowTitle("DAC Configuration Error");
                 msgBox.setText("<p>Current DAC Configuration is <br>"
                                "1) AI : " + QString::number(ap323Count) + "<br> 2) DI : "+ QString::number(ap441Count) + "<br> 3) DO : " + QString::number(ap445Count) + " <br> <br> *Please contact OEM for further Assistance!!! </p>");

                 // Set fixed size for the message box
                 msgBox.setFixedSize(600, 600);  // Adjust width and height as needed

                 // Show the message box and wait for user input
                 msgBox.exec();
                 // Close the application after the dialog is acknowledged
                 exit(0);
             }
        }
        else
        {
            //add not below in final code
            if (!(acromagCheck))
            {
                // Create the message box
                QMessageBox msgBox(this);
                msgBox.setIcon(QMessageBox::Critical);
                msgBox.setWindowTitle("DAC Configuration Error");
                msgBox.setText("<p>Current DAC Configuration is <br>"
                               "1) AI : " + QString::number(ap323Count) + "<br> 2) DI : "+ QString::number(ap441Count) + "<br> 3) DO : " + QString::number(ap445Count) + " <br> <br> *Please contact OEM for further Assistance!!!</p>");

                // Set fixed size for the message box
                msgBox.setFixedSize(600, 600);  // Adjust width and height as needed

                // Show the message box and wait for user input
                msgBox.exec();
                // Close the application after the dialog is acknowledged
                exit(0);
            }
        }

        ui->stackedWidget->setCurrentIndex(globalVariables["MISSILE_SELECT_PAGE"].toInt());
        ui->user_stackedWidget->setCurrentIndex(globalVariables["AAKASH_IMAGE_PAGE"].toInt());
    }

    // After heavy operations finish:
    if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
    ui->loadLabel->setVisible(false);
}

void MainWindow::onOpenFile()
{
    filePath = QFileDialog::getOpenFileName(this, "Open File", "", "Binary Files (*.bin);;All Files (*)");
    if (filePath.isEmpty()) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Нет выбранного файла.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "No file selected.");
        }
    } else {
        ui->lineEdit->setText(filePath);
    }
}

void MainWindow::encrypt_clicked()
{
    ui->browser->clear();

    if (ui->lineEdit->text().isEmpty()) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Файл для шифрования не выбран.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "No file selected for encryption.");
        }
        ui->lineEdit->clear();
        return;
    }

    QFile textFile(filePath);
    if (!textFile.open(QIODevice::ReadOnly)) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Не удалось открыть входной файл.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "Failed to open input file.");
        }
        ui->lineEdit->clear();
        return;
    }
    QByteArray inputData = textFile.readAll();
    textFile.close();

    bool validation=true;
    // Validate if the input data contains only '0' and '1'
    for (int i = 0; i < inputData.size(); ++i)
    {
        char c = inputData.at(i);
        if (c != '0' && c != '1')
        {
            validation= validation & false;
        }
        else
        {
            validation= validation & true;
        }
    }
    if(validation)
    {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Содержимое файла не соответствует ожидаемому формату.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "The file content is not in the expected format.");
        }

        ui->lineEdit->clear();
        return;
    }

    QString inputKey = inputData.toHex();
    aesKey = crypt->deriveKey(inputKey, salt, 10000);

    if (aesKey.isEmpty()) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Ключ пуст!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "Key is empty!");
        }

        ui->lineEdit->clear();
        ui->browser->clear();
        return;
    }

    QByteArray ivAndCiphertext = crypt->encrypt(QString::fromUtf8(inputData), aesKey);

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
    QByteArray combinedBinaryString =crypt->toBinaryString(interleavedData);

    // Show a confirmation message
    QMessageBox::StandardButton reply;
    if(ui->language->isChecked())
    {
        reply = QMessageBox::information(this, "Шифрование завершено", "Шифрование завершено. Выберите файл для сохранения зашифрованных данных.", QMessageBox::Ok | QMessageBox::Cancel);
    }
    else
    {
        reply = QMessageBox::information(this, "Encryption Complete", "Encryption is done. Select a file to save the encrypted data.", QMessageBox::Ok | QMessageBox::Cancel);
    }

    if (reply == QMessageBox::Ok) {
        QString outputFileName = QFileDialog::getSaveFileName(this, "Save Encrypted File", "", "Binary Files (*.bin)");
        if (outputFileName.isEmpty()) {
            if(ui->language->isChecked())
            {
                QMessageBox::warning(this, "Ошибка", "Не удалось сохранить зашифрованный файл.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to save the encrypted file.");
            }

            ui->lineEdit->clear();
            return;
        }

        //ui->browser->append("Auto Generated Key:<br> " + aesKey.toHex() + "<br>");
        ui->browser->append("Input:<br> " + inputData.toHex() + "<br>");

        // Ensure the file extension is .bin
        if (!outputFileName.endsWith(".bin", Qt::CaseInsensitive)) {
            outputFileName += ".bin";
        }

        QFile encryptFile(outputFileName);
        if (!encryptFile.open(QIODevice::WriteOnly)) {
            if(ui->language->isChecked())
            {
                QMessageBox::warning(this, "Ошибка", "Не удалось открыть выходной файл.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to open output file.");
            }

            ui->lineEdit->clear();
            return;
        }
        encryptFile.write(combinedBinaryString);
        encryptFile.close();

        //user log entry for encryption
        userlogEntry("Encryption",filePath,outputFileName);
    }
    else
    {
        // User clicked Cancel
        ui->lineEdit->clear();
        ui->browser->clear();
    }
}

void MainWindow::decrypt_clicked()
{
    ui->browser->clear();

    if (ui->lineEdit->text().isEmpty()) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Файл для расшифровки не выбран.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "No file selected for decryption.");
        }

        ui->lineEdit->clear();
        return;
    }

    QFile encryptFile(filePath);
    if (!encryptFile.open(QIODevice::ReadOnly)) {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Не удалось открыть зашифрованный файл.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "Failed to open encrypted file.");
        }

        ui->lineEdit->clear();
        return;
    }

    QByteArray combinedBinaryString = encryptFile.readAll();
    encryptFile.close();

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
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Содержимое файла не соответствует ожидаемому формату.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "The file content is not in the expected format.");
        }

        ui->lineEdit->clear();
        ui->browser->clear();
        return;
    }

    // Convert binary string back to QByteArray
    QByteArray interleavedData = crypt->fromBinaryString(combinedBinaryString);

    ui->browser->append("Retrieved Combined Encrypt:<br> " + interleavedData.toHex() + "<br>");

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

    // Show a confirmation message
    QMessageBox::StandardButton reply;
    if(ui->language->isChecked())
    {
        reply = QMessageBox::information(this, "Расшифровка завершена", "Расшифровка завершена. Выберите файл для сохранения расшифрованных данных.", QMessageBox::Ok | QMessageBox::Cancel);
    }
    else
    {
        reply = QMessageBox::information(this, "Decryption Complete", "Decryption is done. Select a file to save the decrypted data.", QMessageBox::Ok | QMessageBox::Cancel);
    }

    if (reply == QMessageBox::Ok) {
        QString outputFileName = QFileDialog::getSaveFileName(this, "Save Decrypted File", "", "Binary Files (*.bin)");
        if (outputFileName.isEmpty()) {
            if(ui->language->isChecked())
            {
                QMessageBox::warning(this, "Ошибка", "Не удалось сохранить расшифрованный файл.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to save the decrypted file.");
            }

            ui->lineEdit->clear();
            return;
        }

        //ui->browser->append("Retrieved Key:<br> " + aesKey.toHex() + "<br>");
        ui->browser->append("Retrieved Encrypted Data: <br>" + ivAndCiphertext.toHex() + "<br>");
        ui->browser->append("Decrypted Output: <br>" + output + "<br>");

        // Ensure the file extension is .bin
        if (!outputFileName.endsWith(".bin", Qt::CaseInsensitive)) {
            outputFileName += ".bin";
        }

        QFile textFile(outputFileName);
        if (!textFile.open(QIODevice::WriteOnly)) {
            if(ui->language->isChecked())
            {
                QMessageBox::warning(this, "Ошибка", "Не удалось открыть выходной файл.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "Failed to open output file.");
            }

            ui->lineEdit->clear();
            return;
        }
        textFile.write(output.toUtf8());
        textFile.close();

        //user log entry for encryption
        userlogEntry("Decryption",filePath,outputFileName);

    }
    else
    {
        // User clicked Cancel
        ui->lineEdit->clear();
        ui->browser->clear();
    }
}


void MainWindow::on_clear_clicked()
{
    ui->lineEdit->clear();
    ui->browser->clear();
}

void MainWindow::populateFileSystem()
{
    treeWidget->clear();  // Clear existing items

    QString rootPath = QDir::currentPath();  // Get the current working directory

    // Create the root item for the tree widget
    rootItem = new QTreeWidgetItem(treeWidget, QStringList(rootPath));
    rootItem->setData(0, Qt::UserRole, QVariant(rootPath));  // Store current path

    // Populate tree with current path
    populateTree(rootItem, rootPath);

    on_back_clicked();
}

void MainWindow::populateTree(QTreeWidgetItem *parentItem, const QString &path, int levelCount)
{
    if(levelCount > 2) return;  // Limit depth to 3

    QDir dir(path);

    // Check if directory exists
    if (!dir.exists()) {
        qWarning() << "Directory does not exist:" << path;
        return;
    }

    QStringList subDirs = dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    QStringList files = dir.entryList(QDir::Files, QDir::Name);

    // Loop through subdirectories with index
    for (int i = 0; i < subDirs.size(); ++i)
    {
        const QString &subDir = subDirs.at(i);
        QString fullPath = dir.filePath(subDir);
        QTreeWidgetItem *dirItem = new QTreeWidgetItem(parentItem, QStringList(subDir));
        dirItem->setData(0, Qt::UserRole, QVariant(fullPath));
        dirItem->setIcon(0, QIcon(":/images/folder.png"));
        populateTree(dirItem, fullPath, levelCount + 1);
    }

    // Loop through files with index
    for (int i = 0; i < files.size(); ++i)
    {
        const QString &file = files.at(i);
        if (file.endsWith(".bin", Qt::CaseInsensitive))
        {
            QTreeWidgetItem *fileItem = new QTreeWidgetItem(parentItem, QStringList(file));
            fileItem->setData(0, Qt::UserRole, QVariant(dir.filePath(file)));
            fileItem->setIcon(0, QIcon(":/images/file.png"));
        }
    }

}

void MainWindow::treeWidget_itemClicked(QTreeWidgetItem *item, int column)
{
    if(column)
    {
        //dummy
    }
    // Get the path from the item data
    QString path = item->data(0, Qt::UserRole).toString();

    // Check if the clicked item is a file
    QFileInfo fileInfo(path);
    if (fileInfo.isFile())
    {
        // Check if the file has a .bin extension
        if (fileInfo.suffix().toLower() == "bin")
        {
            filePath = QFileDialog::getOpenFileName(this, "Open File", path, "Binary Files (*.bin);;All Files (*)");
            if (filePath.isEmpty())
            {
                if(ui->language->isChecked())
                {
                    QMessageBox::warning(this, "Ошибка", "Файл не выбран.");
                }
                else
                {
                    QMessageBox::warning(this, "Error", "No file selected.");
                }

            } else
            {
                ui->lineEdit->setText(filePath);
            }
        }
    }
    else
    {
        ui->lineEdit->setText("");
    }
}

void MainWindow::on_back_clicked()
{
    QString path = rootItem->data(0, Qt::UserRole).toString();

    // Navigate to the parent directory
    QString parentPath = QFileInfo(path).absolutePath();

    if(parentPath!="/")
    {
        treeWidget->clear();
        // // Create the root item for the tree widget
        rootItem = new QTreeWidgetItem(treeWidget, QStringList(parentPath));
        rootItem->setData(0, Qt::UserRole, QVariant(parentPath));  // Store current path
        // Populate tree with current path
        populateTree(rootItem, parentPath);
    }
    treeWidget->expandToDepth(1);
}

void MainWindow::on_forward_clicked()
{
    QTreeWidgetItem *item=treeWidget->currentItem();
    if(item)
    {
        QString path = item->data(0, Qt::UserRole).toString();
        // Check if the clicked item is a file
        QFileInfo fileInfo(path);
        if (!fileInfo.isFile())
        {
            treeWidget->clear();
            // // Create the root item for the tree widget
            rootItem = new QTreeWidgetItem(treeWidget, QStringList(path));
            rootItem->setData(0, Qt::UserRole, QVariant(path));  // Store current path
            // Populate tree with current path
            populateTree(rootItem, path);
        }
        treeWidget->expandToDepth(1);
    }
    else
    {
        if(ui->language->isChecked())
        {
            QMessageBox::warning(this, "Ошибка", "Папка не выбрана.");
        }
        else
        {
            QMessageBox::warning(this, "Error", "No folder selected.");
        }
    }
}

void MainWindow::on_reload_clicked()
{
    QString path = rootItem->data(0, Qt::UserRole).toString();
    treeWidget->clear();
    // // Create the root item for the tree widget
    rootItem = new QTreeWidgetItem(treeWidget, QStringList("Current Dir"));
    rootItem->setData(0, Qt::UserRole, QVariant(path));  // Store current path
    // Populate tree with current path
    populateTree(rootItem, path);
    treeWidget->expandToDepth(1);
}

void MainWindow::on_logout_clicked()
{
    ui->statusbar->showMessage("Logging OUT...", 5000);

    mwOFF();

    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    mpsRelayOFF();
    statusRelayOFF();
    obpRelayOFF();
    pyroRelayOFF();

    //if(instrumentsStatus)
    {
         mpsOutOFF();
    }

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    ui->appStackWidget->setCurrentIndex(0);

    // After heavy operations finish:
    if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
    ui->loadLabel->setVisible(false);

    exit(0);
}

void MainWindow::updateUser()
{
    if(ui->language->isChecked())
    {
        ui->user_name->setText("Добро пожаловать: <b><u>"+ loginID +"</u></b>");
    }
    else
    {
        ui->user_name->setText("Welcome: <b><u>"+ loginID +"</u></b>");
    }

    if(authority==globalVariables["ADMIN"].toInt())
    {
        // Load image from resource file
        QPixmap pixmap(":/images/admin_icon.png");

        if (pixmap.isNull()) {
            if(ui->language->isChecked())
            {
                ui->user_logo->setText("Не удалось загрузить изображение оператора");
            }
            else
            {
                ui->user_logo->setText("Failed to load Operator image");
            }
        } else {
            // Set the pixmap to the label
            ui->user_logo->setPixmap(pixmap);
            ui->user_logo->resize(pixmap.size());
        }
    }
    else if(authority ==globalVariables["COMMANDER"].toInt())
    {
        // Load image from resource file
        QPixmap pixmap(":/images/commander_icon.png");

        if (pixmap.isNull()) {
            if(ui->language->isChecked())
            {
                ui->user_logo->setText("Не удалось загрузить изображение CMDR");
            }
            else
            {
                ui->user_logo->setText("Failed to load CMDR image");
            }

        } else {
            // Set the pixmap to the label
            ui->user_logo->setPixmap(pixmap);
            ui->user_logo->resize(pixmap.size());
        }
    }
}

void MainWindow::on_pwdChange_clicked()
{
    logHistoryEntry();
    ui->browser->clear();
    pwdChange(loginID, authority);
}

void MainWindow::on_addUser_clicked()
{
    QString loginID="";
    QString loginPWD="";
    int authority=0;

    AddUserDialog dialog;
    if (dialog.exec() == QDialog::Accepted)
    {
        loginID= dialog.loginID();
        loginPWD= dialog.loginPWD();
        authority= dialog.authority();

        LogEntry newEntry;
        newEntry.userID = loginID;

        if(authority==0)
        {
            authority=globalVariables["ADMIN"].toInt();
            newEntry.authorityLevel = "Operator";
        }
        else if(authority==1)
        {
            authority=globalVariables["COMMANDER"].toInt();
            newEntry.authorityLevel = "Commander";
        }

        // Get the current date and time
        QDateTime current = QDateTime::currentDateTime();
        newEntry.date = current.toString("dd-MM-yyyy"); // Format: DD-MM-YYYY
        newEntry.time = current.toString("HH:mm:ss") + " IST"; // Format: HH:MM:SS with IST timezone

        newEntry.action = "Add User";
        newEntry.authorisedBy = this->loginID;

        addUser(loginID, loginPWD, authority);
        log_history_obj->saveLoginData(newEntry); // Save the log entry
    }
}

void MainWindow::setUserChanges()
{
    if(authority==globalVariables["COMMANDER"].toInt())
    {
        ui->addUser->setEnabled(false);
        ui->userList->setEnabled(false);
        ui->log->setEnabled(false);
        ui->open->setEnabled(false);
        ui->userLog->setEnabled(false);
        ui->reload->setEnabled(false);
        ui->forward->setEnabled(false);
        ui->back->setEnabled(false);
        ui->txtBrowser->setEnabled(false);
        ui->clear->setEnabled(false);
        ui->pwdChange->setEnabled(false);
        ui->home->setEnabled(false);
        ui->checkBox->setEnabled(false);
        ui->selfTest->setEnabled(false);
        ui->checkBox->setText("");
        ui->mwTest->setEnabled(false);
        // ui->mpsStartPushButton->setEnabled(false);
        // ui->statusStartPushButton->setEnabled(false);
        // ui->obpStartPushButton->setEnabled(false);
        // ui->scuStartPushButton->setEnabled(false);
        // ui->rpfStartPushButton->setEnabled(false);
        // ui->pyroStartPushButton->setEnabled(false);
    }
    else if(authority==globalVariables["ADMIN"].toInt())
    {
        if(akash=="prime")
        {
            ui->userLog->setEnabled(true);
            ui->addUser->setEnabled(true);
            ui->userList->setEnabled(true);
            ui->log->setEnabled(true);
            ui->open->setEnabled(true);
            ui->txtBrowser->setEnabled(true);
            ui->clear->setEnabled(true);
            ui->pwdChange->setEnabled(true);
        }
        else if(akash=="export")
        {
            // ui->userLog->setEnabled(true);
            // ui->addUser->setEnabled(true);
            // ui->userList->setEnabled(true);
            // ui->log->setEnabled(true);
            // ui->open->setEnabled(true);
            // ui->txtBrowser->setEnabled(true);
            // ui->clear->setEnabled(true);
            // ui->pwdChange->setEnabled(true);
        }
        else
        {
            // ui->userLog->setEnabled(true);
            // ui->addUser->setEnabled(true);
            // ui->userList->setEnabled(true);
            // ui->log->setEnabled(true);
            // ui->open->setEnabled(true);
            // ui->txtBrowser->setEnabled(true);
            // ui->clear->setEnabled(true);
            // ui->pwdChange->setEnabled(true);
        }

        ui->reload->setEnabled(true);
        ui->forward->setEnabled(true);
        ui->back->setEnabled(true);

        ui->checkBox->setEnabled(true);
        ui->selfTest->setEnabled(true);
        ui->checkBox->setText("ByPass MPS CHECK");
        ui->mwTest->setEnabled(true);
    }
}

void MainWindow::on_userList_clicked()
{
    filePath=globalVariables["CREDENTIALS_BIN_PATH"];

    if (!filePath.isEmpty()) {
        UserListDialog *dialog = new UserListDialog(loginID, authority,filePath, this);
        dialog->exec();
    }
}

void MainWindow::on_userLog_clicked()
{
    user_history_obj->loadUserData(loginID);
    QString report=user_history_obj->generateReport();
    on_clear_clicked();
    ui->browser->setHtml(report);

    QMessageBox::StandardButton reply;
    if(ui->language->isChecked())
    {
        reply = QMessageBox::question(nullptr, "Решение", "Хотите сгенерировать отчет журнала?",
                                      QMessageBox::Yes | QMessageBox::No);
    }
    else
    {
        reply = QMessageBox::question(nullptr, "Decision", "Do you want to generate log report?",
                                      QMessageBox::Yes | QMessageBox::No);
    }

    if (reply == QMessageBox::Yes)
    {
        QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
        QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/UserReports/"+loginID;
        QString filePath = dirPath + "/" + timestamp + ".pdf";

        // Check if the directory exists, and create it if it does not
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qWarning() << "Failed to create directory:" << dirPath;
                return;
            }
        }

        filePath=convertRelativeToAbsolute(filePath);

        QPdfWriter pdfWriter(filePath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(300);

        // Create a QTextDocument
        QTextDocument textDoc;
        textDoc.setHtml(report);

        // Print the QTextDocument to the PDF writer
        textDoc.print(&pdfWriter);

        // Open the generated PDF with the default viewer
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qWarning() << "Failed to open PDF viewer for:" << filePath;
        }
    }
}

void MainWindow::userlogEntry(QString action,QString inFile,QString outFile)
{
    UserEntry newEntry;
    newEntry.userID = loginID;

    if(authority==globalVariables["ADMIN"].toInt())
    {
        newEntry.authorityLevel = "Operator";
    }
    else if(authority==globalVariables["COMMANDER"].toInt())
    {
        newEntry.authorityLevel = "Commander";
    }

    // Get the current date and time
    QDateTime current = QDateTime::currentDateTime();
    newEntry.date = current.toString("dd-MM-yyyy"); // Format: DD-MM-YYYY
    newEntry.time = current.toString("HH:mm:ss") + " IST"; // Format: HH:MM:SS with IST timezone

    newEntry.action = action;
    newEntry.inFile = inFile;
    newEntry.outFile = outFile;

    user_history_obj->saveUserData(newEntry); // Save the log entry
}

void MainWindow::on_log_clicked()
{
    log_history_obj->loadLoginData();
    QString report=log_history_obj->generateReport();
    on_clear_clicked();
    ui->browser->setHtml(report);

    QMessageBox::StandardButton reply;
    if(ui->language->isChecked())
    {
        reply = QMessageBox::question(nullptr, "Решение", "Хотите сгенерировать отчет журнала?",
                                      QMessageBox::Yes | QMessageBox::No);
    }
    else
    {
        reply = QMessageBox::question(nullptr, "Decision", "Do you want to generate log report?",
                                      QMessageBox::Yes | QMessageBox::No);
    }

    if (reply == QMessageBox::Yes)
    {
        QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
        QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/LogReports";
        QString filePath = dirPath + "/" + timestamp + ".pdf";

        // Check if the directory exists, and create it if it does not
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                qWarning() << "Failed to create directory:" << dirPath;
                return;
            }
        }


        filePath=convertRelativeToAbsolute(filePath);

        QPdfWriter pdfWriter(filePath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(300);

        // Create a QTextDocument
        QTextDocument textDoc;
        textDoc.setHtml(report);

        // Print the QTextDocument to the PDF writer
        textDoc.print(&pdfWriter);

        // Open the generated PDF with the default viewer
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qWarning() << "Failed to open PDF viewer for:" << filePath;
        }
    }
}

void MainWindow::on_txtBrowser_clicked()
{
     ui->stackedWidget->setCurrentIndex(globalVariables["BROWSER_PAGE"].toInt());
}

std::vector<std::string> MainWindow::checkBinaryFilesExist(const std::string& folderPath, const std::vector<std::string>& fileNames)
{
    std::vector<std::string> results;
    configFileCount=0;
    for (const auto& fileName : fileNames)
    {
        std::filesystem::path filePath = std::filesystem::path(folderPath) / fileName;

        if (std::filesystem::exists(filePath) && std::filesystem::is_regular_file(filePath))
        {
            results.push_back(fileName + "-> FOUND");
            configFileCount+=1;
        }
        else
        {
            results.push_back(fileName + "-> NOT FOUND");
        }
    }
    return results;
}

void MainWindow::checkconfigFiles()
{
    const std::string folderPath = globalVariables["CONFIG_DIR_PATH"].toStdString();
    const std::vector<std::string> fileNames = {
        "credentials.bin",
        "limit.bin",
        "log.bin"
    };

    auto results = checkBinaryFilesExist(folderPath, fileNames);

    // Update UI with results
    ui->credentialslineEdit->setText(QString::fromStdString(results.size() > 0 ? results[0] : ""));
    ui->limitlineEdit->setText(QString::fromStdString(results.size() > 1 ? results[1] : ""));
    ui->appLoglineEdit->setText(QString::fromStdString(results.size() > 2 ? results[2] : ""));
    //ui->userLoglineEdit->setText(QString::fromStdString(results.size() > 3 ? results[3] : ""));

    // Check if all files exist
    configFileCheck = (results.size() == fileNames.size());
}

void MainWindow::countAcromagInstances()
{

    const char* command = "lspci";
    FILE* pipe = popen(command, "r");
    if (!pipe)
    {
        // Optionally handle the error (e.g., log or display a message)
        return;
    }

    char buffer[256];
    ap323Count = 0;
    ap441Count = 0;
    ap445Count = 0;

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        if (strstr(buffer, "AP323")) {
            ap323Count++;
        } else if (strstr(buffer, "AP441")) {
            ap441Count++;
        } else if (strstr(buffer, "AP445")) {
            ap445Count++;
        }
    }
    pclose(pipe);

    ui->AP323lineEdit->setText(QString::number(ap323Count) + " -> FOUND");
    ui->AP441lineEdit->setText(QString::number(ap441Count) + " -> FOUND");
    ui->AP445lineEdit->setText(QString::number(ap445Count) + " -> FOUND");

    int total = ap323Count + ap441Count + ap445Count;

    acromagCheck = (total == 7);
}

void MainWindow::on_checklistFileSystemPushButton_clicked()
{

    //remove below not in final code
    if ((acromagCheck))
    {
        ui->user_stackedWidget->setCurrentIndex(0);
        ui->stackedWidget->setCurrentIndex(1);
    }
    else {
        if(ui->language->isChecked())
        {
            // Create the message box
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setWindowTitle("DAC Configuration Error");
            msgBox.setText("<p>Current DAC Configuration is <br>"
                           "1) AI : " + QString::number(ap323Count) + "<br> 2) DI : "+ QString::number(ap441Count) + "<br> 3) DO : " + QString::number(ap445Count) + " <br> <br> *Please contact OEM for further Assistance!!!</p>");

            // Set fixed size for the message box
            msgBox.setFixedSize(600, 600);  // Adjust width and height as needed

            // Show the message box and wait for user input
            msgBox.exec();
            // Close the application after the dialog is acknowledged
            exit(0);
        }
        else
        {
            // Create the message box
            QMessageBox msgBox(this);
            msgBox.setIcon(QMessageBox::Critical);
            msgBox.setWindowTitle("DAC Configuration Error");
            msgBox.setText("<p>Current DAC Configuration is <br>"
                           "1) AI : " + QString::number(ap323Count) + "<br> 2) DI : "+ QString::number(ap441Count) + "<br> 3) DO : " + QString::number(ap445Count) + " <br> <br> *Please contact OEM for further Assistance!!!</p>");

            // Set fixed size for the message box
            msgBox.setFixedSize(600, 600);  // Adjust width and height as needed

            // Show the message box and wait for user input
            msgBox.exec();
            // Close the application after the dialog is acknowledged
            exit(0);
        }
    }
}

void MainWindow::on_checklistNetworkPushButton_clicked()
{
    sleep(1);
    ui->stackedWidget->setCurrentIndex(globalVariables["MISSILE_SELECT_PAGE"].toInt());
}

void MainWindow::on_leftMissile_clicked()
{
    ui->statusbar->showMessage("Connecting Left Side...", 5000);

    ui->rightMissile->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");

    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(ui->language->isChecked())
    {
         ui->selection->setText("Выбрано справа");
    }
    else
    {
        ui->selection->setText("LEFT Selected");
    }
        ui->leftMissile->setStyleSheet("QPushButton { background-color: orange; color: white; }");

        lt_missile_flag =1;
        rt_missile_flag =0;

        //g2n check
        earth2NeutralCheckDVM();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        //pneumatic left check
        pneumaticCheck();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        //QMetaObject::invokeMethod(this, [this]() {
            if(gnd2neutralStatus && pneumaticCheckStatus)
            {
                ui->leftMissile->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                //ui->leftMissile->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        //});

        mpsInIt();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        // After heavy operations finish:
        if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
        ui->loadLabel->setVisible(false);

        if(instr_obj->verifyPS())
        ui->stackedWidget->setCurrentIndex(globalVariables["TERMS_CONDITIONS_PAGE"].toInt());
}

void MainWindow::on_rightMissile_clicked()
{
     ui->statusbar->showMessage("Connecting Right Side...", 5000);

    ui->leftMissile->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");

    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(ui->language->isChecked())
    {
         ui->selection->setText("Выбрано справа");
    }
    else
    {
        ui->selection->setText("RIGHT Selected");
    }
        ui->rightMissile->setStyleSheet("QPushButton { background-color: orange; color: white; }");

        lt_missile_flag =0;
        rt_missile_flag =1;

        //g2n check
        earth2NeutralCheckDVM();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        //pneumatic left check
        pneumaticCheck();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        //QMetaObject::invokeMethod(this, [this]() {
            if(gnd2neutralStatus && pneumaticCheckStatus)
            {
                ui->rightMissile->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                //ui->rightMissile->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        //});

        mpsInIt();

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        // After heavy operations finish:
        if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
        ui->loadLabel->setVisible(false);

        if(instr_obj->verifyPS())
        ui->stackedWidget->setCurrentIndex(globalVariables["TERMS_CONDITIONS_PAGE"].toInt());
}

void MainWindow::on_acceptChecklistPushButton_clicked()
{       
    if(ui->acceptChecklistCheckBox->isChecked())
    {
        if ((airPressure.toDouble() >= globalVariables["PNEUMATIC_MIN"].toDouble() && airPressure.toDouble() <= globalVariables["PNEUMATIC_MAX"].toDouble()))
        {
            ui->pneumaticlineEdit->setStyleSheet("QLineEdit { color: green;}");
            pneumaticCheckStatus=true;
            if(ui->language->isChecked())
            {
                ui->lineEdit_AirPressure->setText("ГО");
                //ui->pneumaticlineEdit->setText("ГО");
            }
            else
            {
                ui->lineEdit_AirPressure->setText("GO");
                //ui->pneumaticlineEdit->setText("GO");
            }
        }
        else
        {
            // QMessageBox::StandardButton reply;

            // if(ui->language->isChecked())
            // {
            //     reply = QMessageBox::question(
            //         this,
            //         "Предупреждение о пневматическом давлении",
            //         "Пневматическое давление выходит за допустимые пределы.\n"
            //         "Вы хотите ОБОЙТИ?",
            //         QMessageBox::Ok | QMessageBox::Cancel );
            // }
            // else
            // {
            //     reply = QMessageBox::question(
            //         this,
            //         "Pneumatic Pressure Warning",
            //         "Pneumatic Pressure is out of Limit Range.\n"
            //         "Do you want to BYPASS?",
            //         QMessageBox::Ok | QMessageBox::Cancel );
            // }

            // if (reply == QMessageBox::Ok)
            // {
            //     pneumaticCheckStatus=true;
            //     if(ui->language->isChecked())
            //     {
            //         ui->lineEdit_AirPressure->setText("обойдена");
            //         ui->pneumaticlineEdit->setText("обойдена");
            //     }
            //     else
            //     {
            //         ui->lineEdit_AirPressure->setText("BY PASSED");
            //         ui->pneumaticlineEdit->setText("BY PASSED");
            //     }
            // }
            // else
            {

                ui->pneumaticlineEdit->setStyleSheet("QLineEdit { color: red;}");
                pneumaticCheckStatus=false;
                if(ui->language->isChecked())
                {
                    ui->lineEdit_AirPressure->setText("HE ГО");
                    //ui->pneumaticlineEdit->setText("HE ГО");
                }
                else
                {
                    ui->lineEdit_AirPressure->setText("NO GO");
                    //ui->pneumaticlineEdit->setText("NO GO");
                }
                //return;
            }
        }

        if(g2nVolt.toDouble()<globalVariables["G2N_VOLT_MAX"].toDouble())
        {
            if(authority==globalVariables["ADMIN"].toInt())
            {
                sleep(1);
                if(mpsByPassStatus)
                {
                    ui->stackedWidget->setCurrentIndex(globalVariables["STATUS_CHECK_PAGE"].toInt());
                }
                else
                {
                    ui->stackedWidget->setCurrentIndex(globalVariables["MPS_CHECK_PAGE"].toInt());
                }
            }
            else if (authority==globalVariables["COMMANDER"].toInt())
            {
                ui->stackedWidget->setCurrentIndex(globalVariables["AUTO_SIMULATOR_PAGE"].toInt());
            }
            gnd2neutralStatus=true;
        }
        else
        {
            QMessageBox::StandardButton reply;

            if(ui->language->isChecked())
            {
                reply = QMessageBox::question(
                    this,
                    "Предупреждение о напряжении между землей и нейтралью",
                    "Напряжение между землей и нейтралью выходит за допустимые пределы.\n"
                    "Вы хотите ОБОЙТИ?",
                    QMessageBox::Ok | QMessageBox::Cancel );
            }
            else
            {
                reply = QMessageBox::question(
                    this,
                    "Ground to Neutral Voltage Warning",
                    "Ground to Neutral voltage is out of Limit Range.\n"
                    "Do you want to BYPASS?",
                    QMessageBox::Ok | QMessageBox::Cancel );
            }

            if (reply == QMessageBox::Ok)
            {
                if(authority==globalVariables["ADMIN"].toInt())
                {
                    sleep(1);
                    if(mpsByPassStatus)
                    {
                        ui->stackedWidget->setCurrentIndex(globalVariables["STATUS_CHECK_PAGE"].toInt());
                    }
                    else
                    {
                        ui->stackedWidget->setCurrentIndex(globalVariables["MPS_CHECK_PAGE"].toInt());
                    }
                }
                else if (authority==globalVariables["COMMANDER"].toInt())
                {
                    ui->stackedWidget->setCurrentIndex(globalVariables["AUTO_SIMULATOR_PAGE"].toInt());
                }

                if(ui->language->isChecked())
                {
                    ui->g2nVoltStatus->setText("обойдена");
                }
                else
                {
                    ui->g2nVoltStatus->setText("BY PASSED");
                }
                gnd2neutralStatus=true;
            }
            else
            {
                if(ui->language->isChecked())
                {
                    ui->g2nVoltStatus->setText("HE ГО");
                }
                else
                {
                    ui->g2nVoltStatus->setText("NO GO");
                }
                gnd2neutralStatus=false;
                //return;
            }
        }
    }
    else
    {
        QMessageBox::warning(this, "Error", "Checklist CheckBox not selected.");
    }
}

void MainWindow::on_acceptPowerCheckPushButton_clicked()
{
    //remove below not in final code
    if(1)//mpsCheckStatus)
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["STATUS_CHECK_PAGE"].toInt());
    }
    else
    {
        QMessageBox::warning(this, "Error", "MPS Check Failed.");
    }
}

void MainWindow::on_acceptStatusCheckPushButton_clicked()
{
    //remove below not in final code
    if(1)//statusCheckStatus)
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["OBP_CHECK_PAGE"].toInt());
    }
    else
    {
        QMessageBox::warning(this, "Error", "STATUS Check Failed.");
    }
}

void MainWindow::on_acceptOBPCheckPushButton_clicked()
{
    //remove below not in final code
    if(1)//obpCheckStatus)
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["SCU_CHECK_PAGE"].toInt());
    }
    else
    {
        QMessageBox::warning(this, "Error", "OBP Check Failed.");
    }
}

void MainWindow::on_acceptSCUCheckPushButton_clicked()
{
    //remove below not in final code
    if(1)//scuCheckStatus)
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["RPF_CHECK_PAGE"].toInt());
    }
    else
    {
       QMessageBox::warning(this, "Error", "SCU Check Failed.");
    }
}

void MainWindow::on_acceptRPFCheckPushButton_clicked()
{
    //remove below not in final code
    if(1)//rpfCheckStatus)
    {
        ui->stackedWidget->setCurrentIndex(globalVariables["PYRO_CHECK_PAGE"].toInt());
    }
    else
    {
      QMessageBox::warning(this, "Error", "RPF Check Failed.");
    }
}

QString MainWindow::generateCmdrReport()
{
    QString version = globalVariables["VERSION"];
    QString html="";

    if(ui->language->isChecked())
    {
        QString side_selected = lt_missile_flag ? "левая" : rt_missile_flag ? "правая" : "неизвестная";

        // Generate HTML table
        html = "<html><head><style>"
                       "body { font-family: Arial, sans-serif; background-color: #f4f4f4; }" // Background for the body
                       ".success {color: green; text-align: center; font-weight: bold; }"
                       ".fail {color: red; text-align: center; font-weight: bold; }"
                       ".bypassed {color: orange; text-align: center; font-weight: bold; }"
                       "table { border-collapse: collapse; width: 100%; "
                       "background: url(':/images/MCFS.png') no-repeat center; "
                       "background-size: cover; }"
                       "th { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: #007BFF; color: white; font-size: 1.2em; }" // Header styling
                       "td { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: rgba(255, 255, 255, 0.9); color: #333; }" // Cell styling
                       "tr:nth-child(even) { background-color: #f2f2f2; }" // Zebra striping
                       "tr:hover { background-color: #d1ecf1; }" // Highlight row on hover
                       ".banner { background-color: #28a745; color: white; text-align: center; "
                       "padding: 15px; font-size: 1.5em; font-weight: bold; margin-bottom: 20px; }" // Banner styling
                       "</style></head><body>"
                       // Banner Section
                       "<div class='banner'>"
                       "<img src=':/images/exportBannerRus.png' alt='Banner Image' />"
                       "</div>"
                       ""+QString("*").repeated(100)+""
                       "<h2 style='color: #007BFF;'> Метаданные выполнения симулятора "+ system +"</h2>"
                       "<p>СИСТЕМА: " + system +" , RDACS: RDACU-" + rdacs_serial +  " , ВЕРСИЯ ПО: V-" + version +"</p>"
                       "<p>ОПЕРАТОР: " + ui->user_name->text().split(":")[1] +
                       ", СТОРОНА ВЫБРАНА: " + side_selected + "</p>"
                        "<p>ДАТА: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") +
                        " , ВРЕМЯ: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                        ""+QString("*").repeated(100)+""
                        "<p>КОНТРОЛЬНАЯ СУММА: " + loadExpectedChecksum() + "</p>";

        //summary table
        html+=QString("*").repeated(100)+"";
        html +="<h3>НАЧАЛО СВОДНОГО ОТЧЁТА</h3>"
                "<table>"
                "<tr><th>№ п/п ID</th><th> проверить </th><th> СТАТУС </th></tr>";
        // Create a list of pairs with entry names and corresponding line edits
        QList<QPair<QString, QString>> summary_entries = {
            { "Проверка напряжения E-N", (ui->g2nVoltStatus->text() == "обойдена") ? "<div class='bypassed'> обойдена </div>" : (gnd2neutralStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" :  "<div class='success'> УСПЕХ </div>"},
            { "Проверка MPS", (mpsCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка STATUS", (statusCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка OBP", (obpCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка SCU", (scuCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка RPF", (rpfCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка PYRO", (pyroCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            { "Проверка Пневматический", (ui->pneumaticlineEdit->text() == "обойдена") ? "<div class='bypassed'> обойдена </div>" : (pneumaticCheckStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"},
            {  (selfTest) ? "TEST EQUIPMENT Check with SELF TEST" :"TEST EQUIPMENT Check without SELF TEST", (ui->testEquiplineEdit->text() == "обойдена") ? "<div class='bypassed'> обойдена </div>" : (instrumentsStatus== 0) ? "<div class='fail'> ОТКАЗ </div>" : "<div class='success'> УСПЕХ </div>"}
        };
        // Iterate through the entries to build the table rows
        int serialNo = 1;
        for (const auto &entry : summary_entries) {
            QString measurementName = entry.first;
            QString status = entry.second;
            // Append a new row to the HTML table
            html += "<tr>"
                    "<td>" + QString::number(serialNo++) + "</td>"
                                "<td>" + measurementName + "</td>"
                                "<td>" + status + "</td>"
                               "</tr>";
        }
        html += "</table> <br>";
        QString status="";
        if (gnd2neutralStatus && mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus &&
            rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus)
        {
            if (mpsByPassStatus || (ui->g2nVoltStatus->text() == "BY PASSED"))
            {
                status = "<div class='bypassed' style='text-align: left;'> > " + system +
                         " СОСТОЯНИЕ СИСТЕМЫ: ИСПРАВНО (MPS/ЗЕМЛЯ–НЕЙТРАЛЬ ОБОЙДЕНЫ)</div>";
            }
            else
            {
                status = "<div class='success' style='text-align: left;'> > " + system +
                         " СОСТОЯНИЕ СИСТЕМЫ: ИСПРАВНО</div>";
            }
        }
        else
        {
            status = "<div class='fail' style='text-align: left;'> > " + system +
                     " СОСТОЯНИЕ СИСТЕМЫ: НЕИСПРАВНО</div>";
        }
        html += status +"";
        html+=QString("*").repeated(100)+"";

        html += "</body></html>";
        return html;
    }
    else
    {
        QString side_selected = lt_missile_flag ? "LEFT" : rt_missile_flag ? "RIGHT" : "UNKNOWN";

        // Generate HTML table
        html = "<html><head><style>"
                       "body { font-family: Arial, sans-serif; background-color: #f4f4f4; }" // Background for the body
                       ".success {color: green; text-align: center; font-weight: bold; }"
                       ".fail {color: red; text-align: center; font-weight: bold; }"
                       ".bypassed {color: orange; text-align: center; font-weight: bold; }"
                       "table { border-collapse: collapse; width: 100%; "
                       "background: url(':/images/MCFS.png') no-repeat center; "
                       "background-size: cover; }"
                       "th { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: #007BFF; color: white; font-size: 1.2em; }" // Header styling
                       "td { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: rgba(255, 255, 255, 0.9); color: #333; }" // Cell styling
                       "tr:nth-child(even) { background-color: #f2f2f2; }" // Zebra striping
                       "tr:hover { background-color: #d1ecf1; }" // Highlight row on hover
                       ".banner { background-color: #28a745; color: white; text-align: center; "
                       "padding: 15px; font-size: 1.5em; font-weight: bold; margin-bottom: 20px; }" // Banner styling
                       "</style></head><body>"
                       // Banner Section
                       "<div class='banner'>"
                       "<img src=':/images/exportBannerEng.png' alt='Banner Image' />"
                       "</div>"
                       ""+QString("*").repeated(100)+""
                       "<h2 style='color: #007BFF;'>"+ system +" Simulator Execution Metadata</h2>"
                       "<p>SYSTEM: " + system +" , RDACS: RDACU-" + rdacs_serial +  " , Version No: V-" + version +"</p>"
                       "<p>OPERATOR: " + ui->user_name->text().split(":")[1] +
                       ", SIDE SELECTED: " + side_selected + "</p>"
                        "<p>Date: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") +
                        " , Time: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                        ""+QString("*").repeated(100)+""
                        "<p>CheckSum: " + loadExpectedChecksum() + "</p>";

        //summary table
        html+=QString("*").repeated(100)+"";
        html +="<h3>SUMMARY REPORT</h3>"
                "<table>"
                "<tr><th>S.No. ID</th><th> Check </th><th> STATUS </th></tr>";
        // Create a list of pairs with entry names and corresponding line edits
        QList<QPair<QString, QString>> summary_entries = {
            { "E-N Voltage Check",(ui->g2nVoltStatus->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" : (gnd2neutralStatus== 0) ? "<div class='fail'> FAIL </div>" :  "<div class='success'> SUCCESS </div>"},
            { "MPS Check", (mpsCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "STATUS Check", (statusCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "OBP Check", (obpCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "SCU Check", (scuCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "RPF Check", (rpfCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "PYRO Check", (pyroCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            { "PNEUMATIC Check", (ui->pneumaticlineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" : (pneumaticCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
            {  (selfTest) ? "TEST EQUIPMENT Check with SELF TEST" :"TEST EQUIPMENT Check without SELF TEST", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" : (instrumentsStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"}
        };
        // Iterate through the entries to build the table rows
        int serialNo = 1;
        for (const auto &entry : summary_entries) {
            QString measurementName = entry.first;
            QString status = entry.second;
            // Append a new row to the HTML table
            html += "<tr>"
                    "<td>" + QString::number(serialNo++) + "</td>"
                                "<td>" + measurementName + "</td>"
                                "<td>" + status + "</td>"
                               "</tr>";
        }
        html += "</table> <br>";
        QString status="";
        if(gnd2neutralStatus && mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus &&
            rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus)
        {
            if(mpsByPassStatus || (ui->g2nVoltStatus->text() == "BY PASSED"))// || (ui->pneumaticlineEdit->text() == "BY PASSED"))
            {
                status="<div class='bypassed' style='text-align: left;'> > "+ system +" HEALTH STATUS : SUCCESS with MPS/Earth2Neutral BYPASS</div>";
            }
            else
            {
                status="<div class='success' style='text-align: left;'> > "+ system +" HEALTH STATUS : SUCCESS </div>";
            }
        }
        else
        {
            status="<div class='fail' style='text-align: left;'> > "+ system +" HEALTH STATUS : FAIL </div>";
        }
        html += status +"";
        html+=QString("*").repeated(100)+"";

        html += "</body></html>";
        return html;
    }
}

void MainWindow::on_reportGenerate_clicked()
{
    QString htmlCmdr="";

    QTextDocument textDoc;

    if(akash=="prime")
    {
        htmlCmdr=generateReport();
    }
    else if(akash=="export")
    {
        //cmdr pdf
        htmlCmdr=generateCmdrReport();
        QString html="";
        html=generateReport();
        QString crypt_html=encryptReport(html);

        //encrypted bin
        QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
        QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/SimulatorReports/"+loginID;
        QString filePath = dirPath + "/" + timestamp + ".bin";

        // Check if the directory exists, and create it if it does not
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this,"Error","Failed to create directory:"+dirPath);
                return;
            }
        }

        QFile encryptFile(filePath);
        if (!encryptFile.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "Error", "Failed to open output file. Create a folder with USER name at ../reports/SimulatorReports/");
            return;
        }
        encryptFile.write(crypt_html.toUtf8());
        encryptFile.close();
    }
    else
    {
        //cmdr pdf
        htmlCmdr=generateCmdrReport();
        QString html="";
        html=generateReport();
        QString crypt_html=encryptReport(html);

        //encrypted bin
        QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
        QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/SimulatorReports/"+loginID;
        QString filePath = dirPath + "/" + timestamp + ".bin";

        // Check if the directory exists, and create it if it does not
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this,"Error","Failed to create directory:"+dirPath);
                return;
            }
        }

        QFile encryptFile(filePath);
        if (!encryptFile.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "Error", "Failed to open output file. Create a folder with USER name at ../reports/SimulatorReports/");
            return;
        }
        encryptFile.write(crypt_html.toUtf8());
        encryptFile.close();
    }

    QMessageBox::StandardButton reply;

    if(ui->language->isChecked())
    {
        reply = QMessageBox::question(nullptr, "Решение", "Вы хотите сохранить сгенерированный отчёт?",
                                          QMessageBox::Yes | QMessageBox::No);
    }
    else
    {
        reply = QMessageBox::question(nullptr, "Decision", "Do you want to save generated report?",
                                          QMessageBox::Yes | QMessageBox::No);
    }

    if (reply == QMessageBox::Yes)
    {
        QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
        QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/SimulatorReports/"+loginID;
        filePath = dirPath + "/" + timestamp + ".pdf";

        // Check if the directory exists, and create it if it does not
        QDir dir(dirPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                QMessageBox::warning(this,"Error","Failed to create directory:"+dirPath);
                return;
            }
        }

        filePath=convertRelativeToAbsolute(filePath);

        QPdfWriter pdfWriter(filePath);
        pdfWriter.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter.setResolution(300);

        textDoc.setHtml(htmlCmdr);

        // Print the QTextDocument to the PDF writer
        textDoc.print(&pdfWriter);

        // Open the generated PDF with the default viewer
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qWarning() << "Failed to open PDF viewer for:" << filePath;
        }
    }

    if (reply == QMessageBox::Yes)
    {
        QPrinter printer;
        printer.setPageSize(QPageSize(QPageSize::A4));  // Set the page size
        printer.setResolution(300);  // Set the resolution (same as for PDF)

        // Show the print dialog to select a printer
        QPrintDialog printDialog(&printer, nullptr);
        if (printDialog.exec() == QDialog::Accepted)
        {
            // Print the document to the selected printer
            textDoc.print(&printer);
        } else
        {
            //qWarning() << "Printing cancelled";
        }
    }
}

QString MainWindow::encryptReport(QString inputKey)
{
    aesKey = crypt->deriveKey(inputKey, salt, 10000);

    QByteArray ivAndCiphertext = crypt->encrypt(QString::fromUtf8(inputKey.toUtf8()), aesKey);

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
    QByteArray combinedBinaryString =crypt->toBinaryString(interleavedData);
    QString cmdrStr = QString::fromUtf8(combinedBinaryString);

    return cmdrStr;
}

QString MainWindow::generateReport()
{
    QString side_selected = lt_missile_flag ? "LEFT" : rt_missile_flag ? "RIGHT" : "UNKNOWN";
    QString version = globalVariables["VERSION"];
    QString html="";
    QString RpfDfs="";

    if(akash=="prime")
    {
        RpfDfs="DFS";
    }
    else if(akash=="export")
    {
        RpfDfs="RPF";
    }
    else
    {
        RpfDfs="RPF";
    }

    if(akash=="prime")
    {
        // Generate HTML table
        html = "<html><head><style>"
                       "body { font-family: Arial, sans-serif; background-color: #f4f4f4; }" // Background for the body
                       ".success {color: green; text-align: center; font-weight: bold; }"
                       ".fail {color: red; text-align: center; font-weight: bold; }"
                       ".bypassed {color: orange; text-align: center; font-weight: bold; }"
                       "table { border-collapse: collapse; width: 100%; "
                       "background: url(':/images/MCFS.png') no-repeat center; "
                       "background-size: cover; }"
                       "th { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: #007BFF; color: white; font-size: 1.2em; }" // Header styling
                       "td { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: rgba(255, 255, 255, 0.9); color: #333; }" // Cell styling
                       "tr:nth-child(even) { background-color: #f2f2f2; }" // Zebra striping
                       "tr:hover { background-color: #d1ecf1; }" // Highlight row on hover
                       ".banner { background-color: #28a745; color: white; text-align: center; "
                       "padding: 15px; font-size: 1.5em; font-weight: bold; margin-bottom: 20px; }" // Banner styling
                       "</style></head><body>"
                       // Banner Section
                       "<div class='banner'>"
                       "<img src=':/images/primeBannerEng.png' alt='Banner Image' />"
                       "</div>"
                       ""+QString("*").repeated(100)+""
                       "<h2 style='color: #007BFF;'>"+ system +" Simulator Execution Metadata</h2>"
                       "<p>SYSTEM: " + system +" , RDACS: RDACU-" + rdacs_serial +  " , Version No: V-" + version +"</p>"
                       "<p>OPERATOR: " + ui->user_name->text().split(":")[1] +
                       ", SIDE SELECTED: " + side_selected + "</p>"
                        "<p>Date: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") +
                        " , Time: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                        ""+QString("*").repeated(100)+""
                        "<p>CheckSum: " + loadExpectedChecksum() + "</p>";
    }
    else if(akash=="export")
    {
        // Generate HTML table
        html = "<html><head><style>"
                       "body { font-family: Arial, sans-serif; background-color: #f4f4f4; }" // Background for the body
                       ".success {color: green; text-align: center; font-weight: bold; }"
                       ".fail {color: red; text-align: center; font-weight: bold; }"
                       ".bypassed {color: orange; text-align: center; font-weight: bold; }"
                       "table { border-collapse: collapse; width: 100%; "
                       "background: url(':/images/MCFS.png') no-repeat center; "
                       "background-size: cover; }"
                       "th { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: #007BFF; color: white; font-size: 1.2em; }" // Header styling
                       "td { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: rgba(255, 255, 255, 0.9); color: #333; }" // Cell styling
                       "tr:nth-child(even) { background-color: #f2f2f2; }" // Zebra striping
                       "tr:hover { background-color: #d1ecf1; }" // Highlight row on hover
                       ".banner { background-color: #28a745; color: white; text-align: center; "
                       "padding: 15px; font-size: 1.5em; font-weight: bold; margin-bottom: 20px; }" // Banner styling
                       "</style></head><body>"
                       // Banner Section
                       "<div class='banner'>"
                       "<img src=':/images/exportBannerEng.png' alt='Banner Image' />"
                       "</div>"
                       ""+QString("*").repeated(100)+""
                       "<h2 style='color: #007BFF;'>"+ system +" Simulator Execution Metadata</h2>"
                       "<p>SYSTEM: " + system +" , RDACS: RDACU-" + rdacs_serial +  " , Version No: V-" + version +"</p>"
                       "<p>OPERATOR: " + ui->user_name->text().split(":")[1] +
                       ", SIDE SELECTED: " + side_selected + "</p>"
                        "<p>Date: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") +
                        " , Time: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                        ""+QString("*").repeated(100)+""
                        "<p>CheckSum: " + loadExpectedChecksum() + "</p>";
    }
    else
    {
        // Generate HTML table
        html = "<html><head><style>"
                       "body { font-family: Arial, sans-serif; background-color: #f4f4f4; }" // Background for the body
                       ".success {color: green; text-align: center; font-weight: bold; }"
                       ".fail {color: red; text-align: center; font-weight: bold; }"
                       ".bypassed {color: orange; text-align: center; font-weight: bold; }"
                       "table { border-collapse: collapse; width: 100%; "
                       "background: url(':/images/MCFS.png') no-repeat center; "
                       "background-size: cover; }"
                       "th { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: #007BFF; color: white; font-size: 1.2em; }" // Header styling
                       "td { border: 1px solid #007BFF; padding: 2px; text-align: center; "
                       "background-color: rgba(255, 255, 255, 0.9); color: #333; }" // Cell styling
                       "tr:nth-child(even) { background-color: #f2f2f2; }" // Zebra striping
                       "tr:hover { background-color: #d1ecf1; }" // Highlight row on hover
                       ".banner { background-color: #28a745; color: white; text-align: center; "
                       "padding: 15px; font-size: 1.5em; font-weight: bold; margin-bottom: 20px; }" // Banner styling
                       "</style></head><body>"
                       // Banner Section
                       "<div class='banner'>"
                       "<img src=':/images/exportBannerEng.png' alt='Banner Image' />"
                       "</div>"
                       ""+QString("*").repeated(100)+""
                       "<h2 style='color: #007BFF;'>"+ system +" Simulator Execution Metadata</h2>"
                       "<p>SYSTEM: " + system +" , RDACS: RDACU-" + rdacs_serial +  " , Version No: V-" + version +"</p>"
                       "<p>OPERATOR: " + ui->user_name->text().split(":")[1] +
                       ", SIDE SELECTED: " + side_selected + "</p>"
                        "<p>Date: " + QDateTime::currentDateTime().toString("dd-MM-yyyy") +
                        " , Time: " + QDateTime::currentDateTime().toString("HH:mm:ss") + "</p>"
                        ""+QString("*").repeated(100)+""
                        "<p>CheckSum: " + loadExpectedChecksum() + "</p>";
    }

    //summary table
    html+=QString("*").repeated(100)+"";
    html +="<h3>SUMMARY REPORT</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Check </th><th> STATUS </th></tr>";
    // Create a list of pairs with entry names and corresponding line edits
    QList<QPair<QString, QString>> summary_entries = {
        { "E-N Voltage Check",(ui->g2nVoltStatus->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" : (gnd2neutralStatus== 0) ? "<div class='fail'> FAIL </div>" :  "<div class='success'> SUCCESS </div>"},
        { "MPS Check", (mpsByPassStatus == 1) ? "<div class='bypassed'> BYPASSED </div>" : (mpsCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { "STATUS Check", (statusCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { "OBP Check", (obpCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { "SCU Check", (scuCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { RpfDfs+" Check", (rpfCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { "PYRO Check", (pyroCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { "PNEUMATIC Check", (ui->pneumaticlineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" : (pneumaticCheckStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"},
        { (selfTest) ? "TEST EQUIPMENT Check with SELF TEST" :"TEST EQUIPMENT Check without SELF TEST", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instrumentsStatus== 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>"}
    };
    // Iterate through the entries to build the table rows
    int serialNo = 1;
    for (const auto &entry : summary_entries) {
        QString measurementName = entry.first;
        QString status = entry.second;
        // Append a new row to the HTML table
        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                            "<td>" + measurementName + "</td>"
                            "<td>" + status + "</td>"
                           "</tr>";
    }
    html += "</table> <br>";
    QString status="";
    if(gnd2neutralStatus && mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus &&
        rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus)
    {
        if(mpsByPassStatus || (ui->g2nVoltStatus->text() == "BY PASSED"))// || (ui->pneumaticlineEdit->text() == "BY PASSED"))
        {
            status="<div class='bypassed' style='text-align: left;'> > "+ system +" HEALTH STATUS : SUCCESS with MPS/Earth2Neutral BYPASS</div>";
        }
        else
        {
            status="<div class='success' style='text-align: left;'> > "+ system +" HEALTH STATUS : SUCCESS </div>";
        }
    }
    else
    {
        status="<div class='fail' style='text-align: left;'> > "+ system +" HEALTH STATUS : FAIL </div>";
    }
    html += status +"";
    html+=QString("*").repeated(100)+"";

    html +="<h3>DETAILED REPORT</h3>";

    //general check table
    html+=QString("*").repeated(100)+"";
    html +="<h3>EARTH to NEUTRAL CHECK</h3>"
    "<table>"
    "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";
    // Create a list of pairs with entry names and corresponding line edits
    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> general_entries = {
    { "E2N Voltage (VOLT)", {{{globalVariables["G2N_VOLT_MIN"], globalVariables["G2N_VOLT_MAX"]}, g2nVolt}, ui->g2nVoltStatus->text()} }
    };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (const auto &entry : general_entries) {
        QString measurementName = entry.first;

        // Access the inner pairs to get min and max values
        QString minValue = entry.second.first.first.first;  // globalVariables["Min_Value"]
        QString maxValue = entry.second.first.first.second; // globalVariables["Max_Value"]
        QString measuredValue = entry.second.first.second;
        QString status = entry.second.second;
        // Append a new row to the HTML table
        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                            "<td>" + measurementName + "</td>"
                            "<td>" + minValue + "</td>"
                            "<td>" + maxValue + "</td>"
                            "<td>" + measuredValue + "</td>"
                            "<td>" + status + "</td>"
                           "</tr>";
    }

    html+="</table>";
    html+=QString("*").repeated(100)+"<br><br><br><br>";

    //mps check table
    html +="<h3>MPS CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";

    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> mps_entries;

    if(akash=="prime")
    {
        mps_entries = {
            { "External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_LOCAL_VOLT_MIN"], globalVariables["EXT-PS_LOCAL_VOLT_MAX"]}, ui->extPSVlineEdit->text()}, extPSVolt}},
            { "External-PS Curr (AMP)", {{{globalVariables["EXT-PS_LOCAL_CURR_MIN"], globalVariables["EXT-PS_LOCAL_CURR_MAX"]},  ui->extPSClineEdit->text()}, extPSCurr} },
            { "Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_LOCAL_VOLT_MIN"], globalVariables["STATUS-PS_LOCAL_VOLT_MAX"]}, ui->statusPSVlineEdit->text()}, statusPSVolt} },
            { "Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_LOCAL_VOLT_MIN"], globalVariables["INT-PS_LOCAL_VOLT_MAX"]},  ui->intPSVlineEdit->text()}, intPSVolt} },
            { "Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_LOCAL_VOLT_MIN"], globalVariables["PYRO-PS_LOCAL_VOLT_MAX"]},  ui->pyroPSVlineEdit->text()}, pyroPSVolt} },
            { "SKR-PS Volt (VOLT)", {{{globalVariables["SEEKER-PS_LOCAL_VOLT_MIN"], globalVariables["SEEKER-PS_LOCAL_VOLT_MAX"]}, ui->skrPSVlineEdit->text()}, skrPSVolt} },
            { "RMT External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_RMT_VOLT_MIN"], globalVariables["EXT-PS_RMT_VOLT_MAX"]}, ui->RMTextPSVlineEdit->text()}, rmtExtPSVolt} },
            { "RMT Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_RMT_VOLT_MIN"], globalVariables["STATUS-PS_RMT_VOLT_MAX"]}, ui->RMTstatusPSVlineEdit->text()}, rmtStatusPSVolt} },
            { "RMT Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_RMT_VOLT_MIN"], globalVariables["INT-PS_RMT_VOLT_MAX"]}, ui->RMTintPSVlineEdit->text()}, rmtIntPSVolt} },
            { "RMT Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_RMT_VOLT_MIN"], globalVariables["PYRO-PS_RMT_VOLT_MAX"]}, ui->RMTpyroPSVlineEdit->text()}, rmtPyroPSVolt} },
            { "RMT SKR-PS Volt (VOLT)", {{{globalVariables["SEEKER-PS_RMT_VOLT_MIN"], globalVariables["SEEKER-PS_RMT_VOLT_MAX"]}, ui->RMTskrPSVlineEdit->text()}, rmtSkrPSVolt} },
            { "LOAD TM-PS Curr (AMP)", {{{globalVariables["TM_LOCAL_LOAD_CURR_MIN"], globalVariables["TM_LOCAL_LOAD_CURR_MAX"]}, ui->LOADtmPSClineEdit->text()},loadTmPSCurr} },
            { "LOAD OBP-PS Curr (AMP)", {{{globalVariables["OBP_LOCAL_LOAD_CURR_MIN"], globalVariables["OBP_LOCAL_LOAD_CURR_MAX"]}, ui->LOADobpPSClineEdit->text()}, loadObpPSCurr} },
            { "LOAD SCU-PS Curr (AMP)", {{{globalVariables["SCU_LOCAL_LOAD_CURR_MIN"], globalVariables["SCU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADscuPSClineEdit->text()}, loadScuPSCurr} },
            { "LOAD CGU-PS Curr (AMP)", {{{globalVariables["CGU_LOCAL_LOAD_CURR_MIN"], globalVariables["CGU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADcguPSClineEdit->text()}, loadCguPSCurr} },
            { "LOAD "+RpfDfs+"-PS Curr (AMP)", {{{globalVariables["RPF_LOCAL_LOAD_CURR_MIN"], globalVariables["RPF_LOCAL_LOAD_CURR_MAX"]}, ui->LOADrpfPSClineEdit->text()}, loadRpfPSCurr} },
            { "LOAD SKR-Heater-PS Curr (AMP)", {{{globalVariables["SKR_HEATER_LOCAL_LOAD_CURR_MIN"], globalVariables["SKR_HEATER_LOCAL_LOAD_CURR_MAX"]}, ui->LOADskrHeaterPSClineEdit->text()}, loadSkrHeaterPSCurr}, },
            { "LOAD SKR-SPM-PS Curr (AMP)", {{{globalVariables["SKR_SPM_LOCAL_LOAD_CURR_MIN"], globalVariables["SKR_SPM_LOCAL_LOAD_CURR_MAX"]}, ui->LOADskrSpmPSClineEdit->text()}, loadSkrSpmPSCurr} },
            { "LOAD SKR-TX-PS Curr (AMP)", {{{globalVariables["SKR_TX_LOCAL_LOAD_CURR_MIN"], globalVariables["SKR_TX_LOCAL_LOAD_CURR_MAX"]}, ui->LOADskrTXPSClineEdit->text()}, loadSkrTxPSCurr} },
            { "LOAD TM-PS Volt (VOLT)", {{{globalVariables["TM_LOCAL_LOAD_VOLT_MIN"], globalVariables["TM_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADtmPSVlineEdit->text()}, loadTmPSVolt} },
            { "LOAD OBP-PS Volt (VOLT)", {{{globalVariables["OBP_LOCAL_LOAD_VOLT_MIN"], globalVariables["OBP_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADobpPSVlineEdit->text()}, loadObpPSVolt} },
            { "LOAD SCU-PS Volt (VOLT)", {{{globalVariables["SCU_LOCAL_LOAD_VOLT_MIN"], globalVariables["SCU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADscuPSVlineEdit->text()}, loadScuPSVolt} },
            { "LOAD CGU-PS Volt (VOLT)", {{{globalVariables["CGU_LOCAL_LOAD_VOLT_MIN"], globalVariables["CGU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADcguPSVlineEdit->text()}, loadCguPSVolt} },
            { "LOAD "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_LOCAL_LOAD_VOLT_MIN"], globalVariables["RPF_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADrpfPSVlineEdit->text()}, loadRpfPSVolt} },
            { "LOAD SKR-Heater-PS Volt (VOLT)", {{{globalVariables["SKR_HEATER_LOCAL_LOAD_VOLT_MIN"], globalVariables["SKR_HEATER_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADskrHeaterPSVlineEdit->text()}, loadSkrHeaterPSVolt} },
            { "LOAD SKR-SPM-PS Volt (VOLT)", {{{globalVariables["SKR_SPM_LOCAL_LOAD_VOLT_MIN"], globalVariables["SKR_SPM_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADskrSpmPSVlineEdit->text()}, loadSkrSpmPSVolt} },
            { "LOAD SKR-TX-PS Volt (VOLT)", {{{globalVariables["SKR_TX_LOCAL_LOAD_VOLT_MIN"], globalVariables["SKR_TX_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADskrTXPSVlineEdit->text()}, loadSkrTxPSVolt} },
            { "LOAD RMT TM-PS Volt (VOLT)", {{{globalVariables["TM_RMT_LOAD_VOLT_MIN"], globalVariables["TM_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTtmPSVlineEdit->text()}, loadRmtTmPSVolt} },
            { "LOAD RMT OBP-PS Volt (VOLT)", {{{globalVariables["OBP_RMT_LOAD_VOLT_MIN"], globalVariables["OBP_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTobpPSVlineEdit->text()}, loadRmtObpPSVolt} },
            { "LOAD RMT SCU-PS Volt (VOLT)", {{{globalVariables["SCU_RMT_LOAD_VOLT_MIN"], globalVariables["SCU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTscuPSVlineEdit->text()}, loadRmtScuPSVolt} },
            { "LOAD RMT CGU-PS Volt (VOLT)", {{{globalVariables["CGU_RMT_LOAD_VOLT_MIN"], globalVariables["CGU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTcguPSVlineEdit->text()}, loadRmtCguPSVolt} },
            { "LOAD RMT "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_RMT_LOAD_VOLT_MIN"], globalVariables["RPF_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTrpfPSVlineEdit->text()}, loadRmtRpfPSVolt} },
            { "LOAD RMT SKR-Heater-PS Volt (VOLT)", {{{globalVariables["SKR_HEATER_RMT_LOAD_VOLT_MIN"], globalVariables["SKR_HEATER_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTskrHeaterPSVlineEdit->text()}, loadRmtSkrHeaterPSVolt} },
            { "LOAD RMT SKR-SPM-PS Volt (VOLT)", {{{globalVariables["SKR_SPM_RMT_LOAD_VOLT_MIN"], globalVariables["SKR_SPM_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTskrSpmPSVlineEdit->text()}, loadRmtSkrSpmPSVolt} },
            { "LOAD RMT SKR-TX-PS Volt (VOLT)", {{{globalVariables["SKR_TX_RMT_LOAD_VOLT_MIN"], globalVariables["SKR_TX_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTskrTxPSVlineEdit->text()}, loadRmtSkrTxPSVolt} }
        };
    }
    else if(akash=="export")
    {
        mps_entries = {
            { "External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_LOCAL_VOLT_MIN"], globalVariables["EXT-PS_LOCAL_VOLT_MAX"]}, ui->extPSVlineEdit->text()}, extPSVolt}},
            { "External-PS Curr (AMP)", {{{globalVariables["EXT-PS_LOCAL_CURR_MIN"], globalVariables["EXT-PS_LOCAL_CURR_MAX"]},  ui->extPSClineEdit->text()}, extPSCurr} },
            { "Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_LOCAL_VOLT_MIN"], globalVariables["STATUS-PS_LOCAL_VOLT_MAX"]}, ui->statusPSVlineEdit->text()}, statusPSVolt} },
            { "Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_LOCAL_VOLT_MIN"], globalVariables["INT-PS_LOCAL_VOLT_MAX"]},  ui->intPSVlineEdit->text()}, intPSVolt} },
            { "Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_LOCAL_VOLT_MIN"], globalVariables["PYRO-PS_LOCAL_VOLT_MAX"]},  ui->pyroPSVlineEdit->text()}, pyroPSVolt} },
            { "RMT External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_RMT_VOLT_MIN"], globalVariables["EXT-PS_RMT_VOLT_MAX"]}, ui->RMTextPSVlineEdit->text()}, rmtExtPSVolt} },
            { "RMT Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_RMT_VOLT_MIN"], globalVariables["STATUS-PS_RMT_VOLT_MAX"]}, ui->RMTstatusPSVlineEdit->text()}, rmtStatusPSVolt} },
            { "RMT Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_RMT_VOLT_MIN"], globalVariables["INT-PS_RMT_VOLT_MAX"]}, ui->RMTintPSVlineEdit->text()}, rmtIntPSVolt} },
            { "RMT Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_RMT_VOLT_MIN"], globalVariables["PYRO-PS_RMT_VOLT_MAX"]}, ui->RMTpyroPSVlineEdit->text()}, rmtPyroPSVolt} },
            { "LOAD TM-PS Curr (AMP)", {{{globalVariables["TM_LOCAL_LOAD_CURR_MIN"], globalVariables["TM_LOCAL_LOAD_CURR_MAX"]}, ui->LOADtmPSClineEdit->text()},loadTmPSCurr} },
            { "LOAD OBP-PS Curr (AMP)", {{{globalVariables["OBP_LOCAL_LOAD_CURR_MIN"], globalVariables["OBP_LOCAL_LOAD_CURR_MAX"]}, ui->LOADobpPSClineEdit->text()}, loadObpPSCurr} },
            { "LOAD SCU-PS Curr (AMP)", {{{globalVariables["SCU_LOCAL_LOAD_CURR_MIN"], globalVariables["SCU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADscuPSClineEdit->text()}, loadScuPSCurr} },
            { "LOAD CGU-PS Curr (AMP)", {{{globalVariables["CGU_LOCAL_LOAD_CURR_MIN"], globalVariables["CGU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADcguPSClineEdit->text()}, loadCguPSCurr} },
            { "LOAD "+RpfDfs+"-PS Curr (AMP)", {{{globalVariables["RPF_LOCAL_LOAD_CURR_MIN"], globalVariables["RPF_LOCAL_LOAD_CURR_MAX"]}, ui->LOADrpfPSClineEdit->text()}, loadRpfPSCurr} },
            { "LOAD TM-PS Volt (VOLT)", {{{globalVariables["TM_LOCAL_LOAD_VOLT_MIN"], globalVariables["TM_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADtmPSVlineEdit->text()}, loadTmPSVolt} },
            { "LOAD OBP-PS Volt (VOLT)", {{{globalVariables["OBP_LOCAL_LOAD_VOLT_MIN"], globalVariables["OBP_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADobpPSVlineEdit->text()}, loadObpPSVolt} },
            { "LOAD SCU-PS Volt (VOLT)", {{{globalVariables["SCU_LOCAL_LOAD_VOLT_MIN"], globalVariables["SCU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADscuPSVlineEdit->text()}, loadScuPSVolt} },
            { "LOAD CGU-PS Volt (VOLT)", {{{globalVariables["CGU_LOCAL_LOAD_VOLT_MIN"], globalVariables["CGU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADcguPSVlineEdit->text()}, loadCguPSVolt} },
            { "LOAD "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_LOCAL_LOAD_VOLT_MIN"], globalVariables["RPF_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADrpfPSVlineEdit->text()}, loadRpfPSVolt} },
            { "LOAD RMT TM-PS Volt (VOLT)", {{{globalVariables["TM_RMT_LOAD_VOLT_MIN"], globalVariables["TM_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTtmPSVlineEdit->text()}, loadRmtTmPSVolt} },
            { "LOAD RMT OBP-PS Volt (VOLT)", {{{globalVariables["OBP_RMT_LOAD_VOLT_MIN"], globalVariables["OBP_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTobpPSVlineEdit->text()}, loadRmtObpPSVolt} },
            { "LOAD RMT SCU-PS Volt (VOLT)", {{{globalVariables["SCU_RMT_LOAD_VOLT_MIN"], globalVariables["SCU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTscuPSVlineEdit->text()}, loadRmtScuPSVolt} },
            { "LOAD RMT CGU-PS Volt (VOLT)", {{{globalVariables["CGU_RMT_LOAD_VOLT_MIN"], globalVariables["CGU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTcguPSVlineEdit->text()}, loadRmtCguPSVolt} },
            { "LOAD RMT "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_RMT_LOAD_VOLT_MIN"], globalVariables["RPF_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTrpfPSVlineEdit->text()}, loadRmtRpfPSVolt} }
        };
    }
    else
    {
        mps_entries = {
            { "External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_LOCAL_VOLT_MIN"], globalVariables["EXT-PS_LOCAL_VOLT_MAX"]}, ui->extPSVlineEdit->text()}, extPSVolt}},
            { "External-PS Curr (AMP)", {{{globalVariables["EXT-PS_LOCAL_CURR_MIN"], globalVariables["EXT-PS_LOCAL_CURR_MAX"]},  ui->extPSClineEdit->text()}, extPSCurr} },
            { "Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_LOCAL_VOLT_MIN"], globalVariables["STATUS-PS_LOCAL_VOLT_MAX"]}, ui->statusPSVlineEdit->text()}, statusPSVolt} },
            { "Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_LOCAL_VOLT_MIN"], globalVariables["INT-PS_LOCAL_VOLT_MAX"]},  ui->intPSVlineEdit->text()}, intPSVolt} },
            { "Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_LOCAL_VOLT_MIN"], globalVariables["PYRO-PS_LOCAL_VOLT_MAX"]},  ui->pyroPSVlineEdit->text()}, pyroPSVolt} },
            { "RMT External-PS Volt (VOLT)", {{{globalVariables["EXT-PS_RMT_VOLT_MIN"], globalVariables["EXT-PS_RMT_VOLT_MAX"]}, ui->RMTextPSVlineEdit->text()}, rmtExtPSVolt} },
            { "RMT Status-PS Volt (VOLT)", {{{globalVariables["STATUS-PS_RMT_VOLT_MIN"], globalVariables["STATUS-PS_RMT_VOLT_MAX"]}, ui->RMTstatusPSVlineEdit->text()}, rmtStatusPSVolt} },
            { "RMT Internal-PS Volt (VOLT)", {{{globalVariables["INT-PS_RMT_VOLT_MIN"], globalVariables["INT-PS_RMT_VOLT_MAX"]}, ui->RMTintPSVlineEdit->text()}, rmtIntPSVolt} },
            { "RMT Pyro-PS Volt (VOLT)", {{{globalVariables["PYRO-PS_RMT_VOLT_MIN"], globalVariables["PYRO-PS_RMT_VOLT_MAX"]}, ui->RMTpyroPSVlineEdit->text()}, rmtPyroPSVolt} },
            { "LOAD TM-PS Curr (AMP)", {{{globalVariables["TM_LOCAL_LOAD_CURR_MIN"], globalVariables["TM_LOCAL_LOAD_CURR_MAX"]}, ui->LOADtmPSClineEdit->text()},loadTmPSCurr} },
            { "LOAD OBP-PS Curr (AMP)", {{{globalVariables["OBP_LOCAL_LOAD_CURR_MIN"], globalVariables["OBP_LOCAL_LOAD_CURR_MAX"]}, ui->LOADobpPSClineEdit->text()}, loadObpPSCurr} },
            { "LOAD SCU-PS Curr (AMP)", {{{globalVariables["SCU_LOCAL_LOAD_CURR_MIN"], globalVariables["SCU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADscuPSClineEdit->text()}, loadScuPSCurr} },
            { "LOAD CGU-PS Curr (AMP)", {{{globalVariables["CGU_LOCAL_LOAD_CURR_MIN"], globalVariables["CGU_LOCAL_LOAD_CURR_MAX"]}, ui->LOADcguPSClineEdit->text()}, loadCguPSCurr} },
            { "LOAD "+RpfDfs+"-PS Curr (AMP)", {{{globalVariables["RPF_LOCAL_LOAD_CURR_MIN"], globalVariables["RPF_LOCAL_LOAD_CURR_MAX"]}, ui->LOADrpfPSClineEdit->text()}, loadRpfPSCurr} },
            { "LOAD TM-PS Volt (VOLT)", {{{globalVariables["TM_LOCAL_LOAD_VOLT_MIN"], globalVariables["TM_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADtmPSVlineEdit->text()}, loadTmPSVolt} },
            { "LOAD OBP-PS Volt (VOLT)", {{{globalVariables["OBP_LOCAL_LOAD_VOLT_MIN"], globalVariables["OBP_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADobpPSVlineEdit->text()}, loadObpPSVolt} },
            { "LOAD SCU-PS Volt (VOLT)", {{{globalVariables["SCU_LOCAL_LOAD_VOLT_MIN"], globalVariables["SCU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADscuPSVlineEdit->text()}, loadScuPSVolt} },
            { "LOAD CGU-PS Volt (VOLT)", {{{globalVariables["CGU_LOCAL_LOAD_VOLT_MIN"], globalVariables["CGU_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADcguPSVlineEdit->text()}, loadCguPSVolt} },
            { "LOAD "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_LOCAL_LOAD_VOLT_MIN"], globalVariables["RPF_LOCAL_LOAD_VOLT_MAX"]}, ui->LOADrpfPSVlineEdit->text()}, loadRpfPSVolt} },
            { "LOAD RMT TM-PS Volt (VOLT)", {{{globalVariables["TM_RMT_LOAD_VOLT_MIN"], globalVariables["TM_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTtmPSVlineEdit->text()}, loadRmtTmPSVolt} },
            { "LOAD RMT OBP-PS Volt (VOLT)", {{{globalVariables["OBP_RMT_LOAD_VOLT_MIN"], globalVariables["OBP_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTobpPSVlineEdit->text()}, loadRmtObpPSVolt} },
            { "LOAD RMT SCU-PS Volt (VOLT)", {{{globalVariables["SCU_RMT_LOAD_VOLT_MIN"], globalVariables["SCU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTscuPSVlineEdit->text()}, loadRmtScuPSVolt} },
            { "LOAD RMT CGU-PS Volt (VOLT)", {{{globalVariables["CGU_RMT_LOAD_VOLT_MIN"], globalVariables["CGU_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTcguPSVlineEdit->text()}, loadRmtCguPSVolt} },
            { "LOAD RMT "+RpfDfs+"-PS Volt (VOLT)", {{{globalVariables["RPF_RMT_LOAD_VOLT_MIN"], globalVariables["RPF_RMT_LOAD_VOLT_MAX"]}, ui->LOADRMTrpfPSVlineEdit->text()}, loadRmtRpfPSVolt} }
        };
    }

    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = mps_entries.constBegin(); it != mps_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(mpsByPassStatus)
    {
     status="<div class='bypassed'style='text-align: left;'> > MPS CHECK STATUS: BYPASSED </div>";
    }
    else if(mpsCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > MPS CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > MPS CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    //status check table
    html +="<h3>STATUS CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";

    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> status_entries;

    if(akash=="prime")
    {
        status_entries = {
            { "Sadam Relay Status (0/1)", {{{"-", "-"}, sadamRelayStatus}, ui->sadamRelayStatusLineEdit->text()} },
            { "Nozzle Mating Voltage (VOLT)", {{{globalVariables["NE_MATING_VOLT_MIN"], globalVariables["NE_MATING_VOLT_MAX"]}, ui->nozzleMatingVoltageLineEdit->text()}, nozzleMatingVoltage} },
            { "Sustainer Fire Voltage (VOLT)", {{{globalVariables["SI_MATING_VOLT_MIN"], globalVariables["SI_MATING_VOLT_MAX"]}, ui->sustainerFireVoltageLineEdit->text()}, sustainerFireVoltage} },
            { "NE Relay Status (0/1)", {{{"-", "-"}, neRelayStatus}, ui->neRelayStatusLineEdit->text()} },
            { "Umbilical 1 Status (0/1)", {{{"-", "-"}, umbilical1Status}, ui->umbilical1StatusLineEdit->text()} },
            { "Umbilical 2 Status (0/1)", {{{"-", "-"}, umbilical2Status}, ui->umbilical2StatusLineEdit->text()} },
            { "MIS TM Status (0/1)", {{{"-", "-"}, misTMStatus}, ui->misTMStatusLineEdit->text()} },
            { "Seeker STB Relay Status (0/1)", {{{"-", "-"}, seekerSTBRelayStatus}, ui->seekerSTBRelayStatusLineEdit->text()} },
            { "Seeker MIS Status (0/1)", {{{"-", "-"}, seekerMISStatus}, ui->seekerMISStatusLineEdit->text()} }
            };
    }
    else if(akash=="export")
    {
        status_entries = {
            { "Sadam Relay Status (0/1)", {{{"-", "-"}, sadamRelayStatus}, ui->sadamRelayStatusLineEdit->text()} },
            { "Nozzle Mating Voltage (VOLT)", {{{globalVariables["NE_MATING_VOLT_MIN"], globalVariables["NE_MATING_VOLT_MAX"]}, ui->nozzleMatingVoltageLineEdit->text()}, nozzleMatingVoltage} },
            { "Sustainer Fire Voltage (VOLT)", {{{globalVariables["SI_MATING_VOLT_MIN"], globalVariables["SI_MATING_VOLT_MAX"]}, ui->sustainerFireVoltageLineEdit->text()}, sustainerFireVoltage} },
            { "NE Relay Status (0/1)", {{{"-", "-"}, neRelayStatus}, ui->neRelayStatusLineEdit->text()} },
            { "Umbilical 1 Status (0/1)", {{{"-", "-"}, umbilical1Status}, ui->umbilical1StatusLineEdit->text()} },
            { "Umbilical 2 Status (0/1)", {{{"-", "-"}, umbilical2Status}, ui->umbilical2StatusLineEdit->text()} },
            { "MIS TM Status (0/1)", {{{"-", "-"}, misTMStatus}, ui->misTMStatusLineEdit->text()} }
            };
    }
    else
    {
        status_entries = {
            { "Sadam Relay Status (0/1)", {{{"-", "-"}, sadamRelayStatus}, ui->sadamRelayStatusLineEdit->text()} },
            { "Nozzle Mating Voltage (VOLT)", {{{globalVariables["NE_MATING_VOLT_MIN"], globalVariables["NE_MATING_VOLT_MAX"]}, ui->nozzleMatingVoltageLineEdit->text()}, nozzleMatingVoltage} },
            { "Sustainer Fire Voltage (VOLT)", {{{globalVariables["SI_MATING_VOLT_MIN"], globalVariables["SI_MATING_VOLT_MAX"]}, ui->sustainerFireVoltageLineEdit->text()}, sustainerFireVoltage} },
            { "NE Relay Status (0/1)", {{{"-", "-"}, neRelayStatus}, ui->neRelayStatusLineEdit->text()} },
            { "Umbilical 1 Status (0/1)", {{{"-", "-"}, umbilical1Status}, ui->umbilical1StatusLineEdit->text()} },
            { "Umbilical 2 Status (0/1)", {{{"-", "-"}, umbilical2Status}, ui->umbilical2StatusLineEdit->text()} },
            { "MIS TM Status (0/1)", {{{"-", "-"}, misTMStatus}, ui->misTMStatusLineEdit->text()} }
            };
    }

    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = status_entries.constBegin(); it != status_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(statusCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > STATUS CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > STATUS CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    int byteCount = serialcomm_obj->response.toUtf8().size();
    //obp check table
    html +="<h3>OBP CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> Bytes Sent </th><th> Bytes Received </th><th> LINK STATUS </th></tr>";
    //<th> Bytes Recieved </th>
    QList<QPair<QString, QPair<QPair< QString, QString>, QString>>> obp_entries = {
        { "OBP LINK STATUS", {{globalVariables["OBP_BYTES_SENT"], QString::number(byteCount)}, obpStatus} }
    };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = obp_entries.constBegin(); it != obp_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &bytesSent     = it->second.first.first;
        const QString &bytesReceived = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + bytesSent + "</td>"
                "<td>" + bytesReceived + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(obpCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > OBP CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > OBP CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    //scu check table
    html +="<h3>SCU CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";
    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> scu_entries = {
        { "Cage Status (VOLT)", {{{globalVariables["CAGE_VOLT_MIN"], globalVariables["CAGE_VOLT_MAX"]}, ui->cageLineEdit->text()}, cageStatus} }
        };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = scu_entries.constBegin(); it != scu_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(scuCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > SCU CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > SCU CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    //rpf check table
    html +="<h3>"+RpfDfs+" CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";
    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> rpf_entries = {
        { "FIR Pulse (VOLT)", {{{globalVariables["FIRING_PULSE_MIN"], globalVariables["FIRING_PULSE_MAX"]}, ui->firingPulse->text()}, firingPulseStatus} },
        { "FIR Pulse TM (VOLT)", {{{globalVariables["FIRING_PULSE_TM_MIN"], globalVariables["FIRING_PULSE_TM_MAX"]}, ui->firingPulseTM->text()}, firingPulseTMStatus} },
        { "FIR Pulse (SADM) Amplitude (VOLT)", {{{globalVariables["RPF_AMP_MIN"], globalVariables["RPF_AMP_MAX"]}, ui->amplitudeLineEdit->text()}, firPulseAmplitude} },
        { "FIR Pulse (SADM) Width (sec)", {{{globalVariables["RPF_WIDTH_MIN"], globalVariables["RPF_WIDTH_MAX"]}, ui->widthLineEdit->text()}, firPulseWidth} }
        };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = rpf_entries.constBegin(); it != rpf_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(rpfCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > "+RpfDfs+" CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > "+RpfDfs+" CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    //pyro check table
    html +="<h3>PYRO CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";
    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> pyro_entries = {
        { "Ground Relay 1 Status (0/1)", {{{"-", "-"}, groundRelay1}, ui->gndRelay1LineEdit->text()} },
        { "Air Bottle Squib Fire Relay Status (0/1)", {{{"-", "-"}, airBottleSquibFireRelayStatus}, ui->airBottleSquibFireRelayLineEdit->text()} },
        { "Thermal Battery Squib Fire Relay Status (0/1)", {{{"-", "-"}, thermalBatterySquibFireRelayStatus}, ui->thermalBatterySquibFireRelayLineEdit->text()} },
        { "Ground Relay 2 Status (0/1)", {{{"-", "-"}, groundRelay2}, ui->gndRelay2LineEdit->text()} },
        { "Booster Pyro Relay Status (0/1)", {{{"-", "-"}, boosterPyroRelayStatus}, ui->boosterPyroRelayStatusLineEdit->text()} },
        { "Ground Pyro Relay Status (0/1)", {{{"-", "-"}, groundPyroRelayStatus}, ui->groundPyroRelayStatusLineEdit->text()} }
    };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = pyro_entries.constBegin(); it != pyro_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(pyroCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > PYRO CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > PYRO CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    //pneumatic check table
    html +="<h3>PNEUMATIC CHECK</h3>"
            "<table>"
            "<tr><th>S.No. ID</th><th> Parameter </th><th> MIN Value </th><th> MAX Value </th><th> Measured Value </th><th> STATUS </th></tr>";
    QList<QPair<QString, QPair<QPair<QPair<QString, QString>, QString>, QString>>> pneumatic_entries = {
        { "Air Pressure (KSC)", {{{globalVariables["PNEUMATIC_MIN"], globalVariables["PNEUMATIC_MAX"]}, airPressure}, ui->lineEdit_AirPressure->text()} },
        };
    // Iterate through the entries to build the table rows
    serialNo = 1;
    for (auto it = pneumatic_entries.constBegin(); it != pneumatic_entries.constEnd(); ++it)
    {
        const QString &measurementName = it->first;

        const QString &minValue      = it->second.first.first.first;
        const QString &maxValue      = it->second.first.first.second;
        const QString &measuredValue = it->second.first.second;
        const QString &status        = it->second.second;

        html += "<tr>"
                "<td>" + QString::number(serialNo++) + "</td>"
                "<td>" + measurementName + "</td>"
                "<td>" + minValue + "</td>"
                "<td>" + maxValue + "</td>"
                "<td>" + measuredValue + "</td>"
                "<td>" + status + "</td>"
                "</tr>";
    }
    html += "</table>";
    status="";
    if(ui->pneumaticlineEdit->text() == "BY PASSED")
    {
     status="<div class='bypassed'style='text-align: left;'> > PNEUMATIC CHECK STATUS: BYPASSED </div>";
    }
    else if(pneumaticCheckStatus)
    {
        status="<div class='success'style='text-align: left;'> > PNEUMATIC CHECK STATUS: SUCCESS </div>";
    }
    else
    {
        status="<div class='fail'style='text-align: left;'> > PNEUMATIC CHECK STATUS: FAIL </div>";
    }
    html += status + "";
    html+=QString("*").repeated(100)+"";

    if(selfTest)
    {
        QString pwrmtr="";
        QString wavegen="";
        QString unictr="";
        QString dso="";
        QString siggen="";

        for (int i = 0; i < instr_obj->instruments.size(); ++i)
        {
            auto &instr = instr_obj->instruments[i];

            if(instr.name=="PWR-MTR")
            {
                if(instr.selfTest)
                pwrmtr="SUCCESS";
                else
                pwrmtr=instr_obj->failedSelfTestInstrumentStatus;
            }
            if(instr.name=="WAVE-GEN")
            {
                if(instr.selfTest)
                 wavegen="SUCCESS";
                else
                wavegen=instr_obj->failedSelfTestInstrumentStatus;
            }

            if(instr.name=="SIG-GEN")
            {
                if(instr.selfTest)
                siggen="SUCCESS";
                else
                siggen=instr_obj->failedSelfTestInstrumentStatus;
            }

            if(instr.name=="UNI-CTR")
            {
                if(instr.selfTest)
                unictr="SUCCESS";
                else
                unictr=instr_obj->failedSelfTestInstrumentStatus;
            }

            if(instr.name=="DSO")
            {
                if(instr.selfTest)
                dso="SUCCESS";
                else
                dso=instr_obj->failedSelfTestInstrumentStatus;
            }
        }
        //test equipment check table
        html +="<h3>TEST EQUIPMENTS SELF TEST CHECK</h3>"
                "<table>"
                "<tr><th>S.No. ID</th><th> Parameter </th><th> CONNECTION STATUS </th><th> SELF TEST STATUS </th></tr>";
        // Populate test_equip_entries using the initialized instruments
        QList<QPair<QString, QPair<QString,QString>>> test_equip_entries = {
            { "DSO", {(ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["DSO_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>", ( dso== "SUCCESS") ? "<div class='success'> SUCCESS </div>":"<div class='fail'> "+dso+"</div>"}},
            { "POWER METER", {(ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PWR-MTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>",( pwrmtr== "SUCCESS") ? "<div class='success'> SUCCESS </div>":"<div class='fail'> "+pwrmtr+"</div>"}},
            { "WAVEFORM GENEATOR", {(ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["WAVE-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>",( wavegen== "SUCCESS") ? "<div class='success'> SUCCESS </div>":"<div class='fail'> "+wavegen+"</div>"}},
            { "UNIVERSAL FREQ COUNTER", {(ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["UNI-CTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>",( unictr== "SUCCESS") ? "<div class='success'> SUCCESS </div>":"<div class='fail'> "+unictr+"</div>"}},
            { "SIGNAL GENERATOR", {(ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["SIG-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>",( siggen== "SUCCESS") ? "<div class='success'> SUCCESS </div>":"<div class='fail'> "+siggen+"</div>"}}
        };
        // Iterate through the entries to build the table rows
        serialNo = 1;
        for (auto it = test_equip_entries.constBegin(); it != test_equip_entries.constEnd(); ++it)
        {
            const QString &measurementName = it->first;
            const QString &status = it->second.first;
            const QString &selftest = it->second.second;

            html += "<tr>"
                    "<td>" + QString::number(serialNo++) + "</td>"
                    "<td>" + measurementName + "</td>"
                    "<td>" + status + "</td>"
                    "<td>" + selftest + "</td>"
                    "</tr>";
        }
        html += "</table>";
        status="";
        if(instrumentsStatus)
        {
            status="<div class='success'style='text-align: left;'> > TEST EQUIPMENT CHECK with SELF TEST STATUS: SUCCESS </div>";
        }
        else
        {
            status="<div class='fail'style='text-align: left;'> > TEST EQUIPMENT CHECK with SELF TEST STATUS: FAIL </div>";
        }
    }
    else
    {
        //test equipment check table
        html +="<h3>TEST EQUIPMENT CHECK</h3>"
                "<table>"
                "<tr><th>S.No. ID</th><th> Parameter </th><th> CONNECTION STATUS </th></tr>";

        // Populate test_equip_entries using the initialized instruments
        QList<QPair<QString, QString>> test_equip_entries;

        if(akash=="prime")
        {
            test_equip_entries = {
                { "EXTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["EXT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "STATUS POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["STATUS-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "INTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["INT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "PYRO POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PYRO-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "SEEKER POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["SEEKER-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "DSO", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["DSO_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "POWER METER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PWR-MTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "WAVEFORM GENEATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["WAVE-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "UNIVERSAL FREQ COUNTER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["UNI-CTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "SIGNAL GENERATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["SIG-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" }
            };
        }
        else if(akash=="export")
        {
            test_equip_entries = {
                { "EXTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["EXT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "STATUS POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["STATUS-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "INTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["INT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "PYRO POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PYRO-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "DSO", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["DSO_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "POWER METER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PWR-MTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "WAVEFORM GENEATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["WAVE-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "UNIVERSAL FREQ COUNTER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["UNI-CTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "SIGNAL GENERATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["SIG-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" }
            };
        }
        else
        {
            test_equip_entries = {
                { "EXTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["EXT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "STATUS POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["STATUS-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "INTERNAL POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["INT-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "PYRO POWER SUPPLY", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PYRO-PS_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "DSO", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["DSO_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "POWER METER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["PWR-MTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "WAVEFORM GENEATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["WAVE-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "UNIVERSAL FREQ COUNTER", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["UNI-CTR_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" },
                { "SIGNAL GENERATOR", (ui->testEquiplineEdit->text() == "BY PASSED") ? "<div class='bypassed'> BYPASSED </div>" :(instruments[globalVariables["SIG-GEN_HOST_NAME"]] == 0) ? "<div class='fail'> FAIL </div>" : "<div class='success'> SUCCESS </div>" }
            };
        }

        // Iterate through the entries to build the table rows
        serialNo = 1;
        for (auto it = test_equip_entries.constBegin(); it != test_equip_entries.constEnd(); ++it)
        {
            const QString &measurementName = it->first;
            const QString &status = it->second;

            html += "<tr>"
                    "<td>" + QString::number(serialNo++) + "</td>"
                    "<td>" + measurementName + "</td>"
                    "<td>" + status + "</td>"
                    "</tr>";
        }
        html += "</table>";
        status="";
        if(instrumentsStatus)
        {
            status="<div class='success'style='text-align: left;'> > TEST EQUIPMENT CHECK without SELF TEST STATUS: SUCCESS </div>";
        }
        else
        {
            status="<div class='fail'style='text-align: left;'> > TEST EQUIPMENT CHECK without SELF TEST STATUS: FAIL </div>";
        }
    }

    html += status;
    html+=QString("*").repeated(100);

    html += "</body></html>";
    return html;
}


void MainWindow::mpsInIt()
{
    //ext ps set
    instr_obj->connectionOpen(globalVariables["EXT-PS_VISA_ADDRESS"]);
    instr_obj->setInstrument(globalVariables["EXT-PS_HOST_NAME"]);
    instr_obj->setInstrumentOutON();

    //stauts ps set
    instr_obj->connectionOpen(globalVariables["STATUS-PS_VISA_ADDRESS"]);
    instr_obj->setInstrument(globalVariables["STATUS-PS_HOST_NAME"]);
    instr_obj->setInstrumentOutON();

    //internal ps set
    instr_obj->connectionOpen(globalVariables["INT-PS_VISA_ADDRESS"]);
    instr_obj->setInstrument(globalVariables["INT-PS_HOST_NAME"]);
    instr_obj->setInstrumentOutON();

    //pyro ps set
    instr_obj->connectionOpen(globalVariables["PYRO-PS_VISA_ADDRESS"]);
    instr_obj->setInstrument(globalVariables["PYRO-PS_HOST_NAME"]);
    instr_obj->setInstrumentOutON();

    if(akash=="prime")
    {
        //seeker ps set
        instr_obj->connectionOpen(globalVariables["SEEKER-PS_VISA_ADDRESS"]);
        instr_obj->setInstrument(globalVariables["SEEKER-PS_HOST_NAME"]);
        instr_obj->setInstrumentOutON();
    }
    sleep(3);
}

void MainWindow::mpsCheck()
{
    QMetaObject::invokeMethod(this, [this]() {
    ui->mpslineEdit->setStyleSheet("QLineEdit { color: black; }");
    ui->extPSClineEdit->clear();
    ui->extPSVlineEdit->clear();
    ui->statusPSVlineEdit->clear();
    ui->intPSVlineEdit->clear();
    ui->pyroPSVlineEdit->clear();
    ui->skrPSVlineEdit->clear();

    ui->RMTextPSVlineEdit->clear();
    ui->RMTstatusPSVlineEdit->clear();
    ui->RMTintPSVlineEdit->clear();
    ui->RMTpyroPSVlineEdit->clear();
    ui->RMTskrPSVlineEdit->clear();

    ui->LOADtmPSClineEdit->clear();
    ui->LOADobpPSClineEdit->clear();
    ui->LOADscuPSClineEdit->clear();
    ui->LOADcguPSClineEdit->clear();
    ui->LOADrpfPSClineEdit->clear();
    ui->LOADskrHeaterPSClineEdit->clear();
    ui->LOADskrSpmPSClineEdit->clear();

    ui->LOADtmPSVlineEdit->clear();
    ui->LOADobpPSVlineEdit->clear();
    ui->LOADscuPSVlineEdit->clear();
    ui->LOADcguPSVlineEdit->clear();
    ui->LOADrpfPSVlineEdit->clear();
    ui->LOADskrHeaterPSVlineEdit->clear();
    ui->LOADskrSpmPSVlineEdit->clear();

    ui->LOADRMTtmPSVlineEdit->clear();
    ui->LOADRMTobpPSVlineEdit->clear();
    ui->LOADRMTscuPSVlineEdit->clear();
    ui->LOADRMTcguPSVlineEdit->clear();
    ui->LOADRMTrpfPSVlineEdit->clear();
    ui->LOADRMTskrHeaterPSVlineEdit->clear();
    ui->LOADRMTskrSpmPSVlineEdit->clear();

    ui->LOADskrTXPSClineEdit->clear();
    ui->LOADskrTXPSVlineEdit->clear();
    ui->LOADRMTskrTxPSVlineEdit->clear();

    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса MPS -> ");
    }
    else
    {
        ui->progressLabel->setText("MPS Check Progress -> ");
    }

    ui->progressBar->setValue(0);
    });

    mpsCheckStatus=true;

    double local_volt,remote_volt=0;
    double local_curr=0;

    //ext ps set
    instr_obj->connectionOpen(globalVariables["EXT-PS_VISA_ADDRESS"]);
    //instr_obj->setInstrument(globalVariables["EXT-PS_HOST_NAME"]);
    //instr_obj->setInstrumentOutON();

    //local ext ps readings
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["EXT-PS_LOCAL_VOLT_MIN"].toInt() && local_volt <= globalVariables["EXT-PS_LOCAL_VOLT_MAX"].toInt())
    {
        extPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->extPSVlineEdit->setText(extPSVolt);
        extPSVolt="GO";
        });
    }
    else
    {
        extPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->extPSVlineEdit->setText(extPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        extPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["EXT-PS_LOCAL_CURR_MIN"].toDouble() && local_curr <= globalVariables["EXT-PS_LOCAL_CURR_MAX"].toDouble())
    {
        extPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->extPSClineEdit->setText(extPSCurr);
        extPSCurr="GO";
        });
    }
    else
    {
        extPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->extPSClineEdit->setText(extPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        extPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }
    //open  for testing
    //instr_obj->connectionClose();

    usleep(1000);
    remote_volt=0;

    //remote ext ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);
    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["EXT-PS_RMT_VOLT_MIN"].toInt() && remote_volt <= globalVariables["EXT-PS_RMT_VOLT_MAX"].toInt())
    {
        rmtExtPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTextPSVlineEdit->setText(rmtExtPSVolt);
        rmtExtPSVolt="GO";
        });
    }
    else
    {
        rmtExtPSVolt=QString::number(remote_volt);

        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTextPSVlineEdit->setText("NO GO");
        //ui->mpslineEdit->setText("NO GO");
        rmtExtPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(20);
    });

    //stauts ps set
    instr_obj->connectionOpen(globalVariables["STATUS-PS_VISA_ADDRESS"]);
    //instr_obj->setInstrument(globalVariables["STATUS-PS_HOST_NAME"]);
    //instr_obj->setInstrumentOutON();

    //local status ps readings
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["STATUS-PS_LOCAL_VOLT_MIN"].toDouble() && local_volt <= globalVariables["STATUS-PS_LOCAL_VOLT_MAX"].toDouble())
    {
        statusPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->statusPSVlineEdit->setText(statusPSVolt);
        statusPSVolt="GO";
        });
    }
    else
    {
        statusPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->statusPSVlineEdit->setText(statusPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        statusPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }

    //open for testing
    //instr_obj->connectionClose();

    usleep(1000);
    remote_volt=0;

    //remote status ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["STATUS-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["STATUS-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["STATUS-PS_RMT_VOLT_MIN"].toDouble() && remote_volt <= globalVariables["STATUS-PS_RMT_VOLT_MAX"].toDouble())
    {
        rmtStatusPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTstatusPSVlineEdit->setText(rmtStatusPSVolt);
        rmtStatusPSVolt="GO";
        });
    }
    else
    {
        rmtStatusPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTstatusPSVlineEdit->setText("NO GO");
        //ui->mpslineEdit->setText("NO GO");
        rmtStatusPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(30);
    });

    //internal ps set
    instr_obj->connectionOpen(globalVariables["INT-PS_VISA_ADDRESS"]);
    //instr_obj->setInstrument(globalVariables["INT-PS_HOST_NAME"]);
    //instr_obj->setInstrumentOutON();

    //local int ps readings
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["INT-PS_LOCAL_VOLT_MIN"].toInt() && local_volt <= globalVariables["INT-PS_LOCAL_VOLT_MAX"].toInt())
    {
        intPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->intPSVlineEdit->setText(intPSVolt);
        intPSVolt="GO";
        });
    }
    else
    {
        intPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->intPSVlineEdit->setText(intPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        intPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }

    //open for testing
    //instr_obj->connectionClose();

    usleep(1000);
    remote_volt=0;

    //remote status ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["INT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["INT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["INT-PS_RMT_VOLT_MIN"].toInt() && remote_volt <= globalVariables["INT-PS_RMT_VOLT_MAX"].toInt())
    {
        rmtIntPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTintPSVlineEdit->setText(rmtIntPSVolt);
        rmtIntPSVolt="GO";
        });
    }
    else
    {
        rmtIntPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTintPSVlineEdit->setText("NO GO");
        //ui->mpslineEdit->setText("NO GO");
        rmtIntPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(40);
    });

    //pyro ps set
    instr_obj->connectionOpen(globalVariables["PYRO-PS_VISA_ADDRESS"]);
    //instr_obj->setInstrument(globalVariables["PYRO-PS_HOST_NAME"]);
    //instr_obj->setInstrumentOutON();

    //local pyro ps readings
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["PYRO-PS_LOCAL_VOLT_MIN"].toInt() && local_volt <= globalVariables["PYRO-PS_LOCAL_VOLT_MAX"].toInt())
    {
        pyroPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->pyroPSVlineEdit->setText(pyroPSVolt);
        pyroPSVolt="GO";
        });

    }
    else
    {
        pyroPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->pyroPSVlineEdit->setText(pyroPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        pyroPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }
    //open for testing
    //instr_obj->connectionClose();

    usleep(1000);
    remote_volt=0;

    //remote status ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["PYRO-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["PYRO-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["PYRO-PS_RMT_VOLT_MIN"].toInt() && remote_volt <= globalVariables["PYRO-PS_RMT_VOLT_MAX"].toInt())
    {
        rmtPyroPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTpyroPSVlineEdit->setText(rmtPyroPSVolt);
        rmtPyroPSVolt="GO";
        });
    }
    else
    {
        rmtPyroPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTpyroPSVlineEdit->setText(rmtPyroPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        rmtPyroPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(50);
    });

    if(akash=="prime")
    {
    //seeker ps set
    instr_obj->connectionOpen(globalVariables["SEEKER-PS_VISA_ADDRESS"]);
    instr_obj->setInstrument(globalVariables["SEEKER-PS_HOST_NAME"]);
    instr_obj->setInstrumentOutON();

    //local seeker ps readings
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["SEEKER-PS_LOCAL_VOLT_MIN"].toInt() && local_volt <= globalVariables["SEEKER-PS_LOCAL_VOLT_MAX"].toInt())
    {
        skrPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->skrPSVlineEdit->setText(skrPSVolt);
        skrPSVolt="GO";
        });
    }
    else
    {
        skrPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->skrPSVlineEdit->setText(skrPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        skrPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //close for testing
        instr_obj->connectionClose();
        //return;
    }
    //open for testing
    //instr_obj->connectionClose();

    remote_volt=0;
    usleep(1000);

    //remote seeker ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["SEEKER-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["SEEKER-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_SKR_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["SEEKER-PS_RMT_VOLT_MIN"].toInt() && remote_volt <= globalVariables["SEEKER-PS_RMT_VOLT_MAX"].toInt())
    {
        rmtSkrPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTskrPSVlineEdit->setText(rmtSkrPSVolt);
        rmtSkrPSVolt="GO";
        });
    }
    else
    {
        rmtSkrPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->RMTskrPSVlineEdit->setText("NO GO");
        //ui->mpslineEdit->setText("NO GO");
        rmtSkrPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        if(remote_volt>300)
        {
            QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            on_home_clicked();
            exit(0);
        }
        //return;
    }
    }
    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(60);
    });

    ///////////////////////////////
    //LOAD SECTION
    //////////////////////////////

    //TM - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["TM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["TM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //ext ps set
    instr_obj->connectionOpen(globalVariables["EXT-PS_VISA_ADDRESS"]);

    //local ext ps readings with TM load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["TM_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["TM_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadTmPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADtmPSVlineEdit->setText(loadTmPSVolt);
        loadTmPSVolt="GO";
        });
    }
    else
    {
        loadTmPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADtmPSVlineEdit->setText(loadTmPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadTmPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["TM_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["TM_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadTmPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADtmPSClineEdit->setText(loadTmPSCurr);
        loadTmPSCurr="GO";
        });
    }
    else
    {
        loadTmPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADtmPSClineEdit->setText(loadTmPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadTmPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;
    //remote ext ps readings with TM load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);
    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["TM_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["TM_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtTmPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTtmPSVlineEdit->setText(loadRmtTmPSVolt);
        loadRmtTmPSVolt="GO";
        });
    }
    else
    {
        loadRmtTmPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTtmPSVlineEdit->setText(loadRmtTmPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtTmPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(65);
    });

    //OBP - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["OBP_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["OBP_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    usleep(1000);

    //local ext ps readings with TM + OBP load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["OBP_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["OBP_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadObpPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADobpPSVlineEdit->setText(loadObpPSVolt);
        loadObpPSVolt="GO";
        });
    }
    else
    {
        loadObpPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADobpPSVlineEdit->setText(loadObpPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadObpPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["OBP_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["OBP_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadObpPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADobpPSClineEdit->setText(loadObpPSCurr);
        loadObpPSCurr="GO";
        });
    }
    else
    {
        loadObpPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADobpPSClineEdit->setText(loadObpPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadObpPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for  testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;

    //remote ext ps readings with TM + OBP load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);
    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["OBP_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["OBP_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtObpPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTobpPSVlineEdit->setText(loadRmtObpPSVolt);
        loadRmtObpPSVolt="GO";
        });
    }
    else
    {
        loadRmtObpPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTobpPSVlineEdit->setText(loadRmtObpPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtObpPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(70);
    });

    //SCU - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SCU_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SCU_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //local ext ps readings with TM + OBP + SCU load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["SCU_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["SCU_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadScuPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADscuPSVlineEdit->setText(loadScuPSVolt);
        loadScuPSVolt="GO";
        });
    }
    else
    {
        loadScuPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADscuPSVlineEdit->setText(loadScuPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadScuPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["SCU_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["SCU_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadScuPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADscuPSClineEdit->setText(loadScuPSCurr);
        loadScuPSCurr="GO";
        });
    }
    else
    {
        loadScuPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADscuPSClineEdit->setText(loadScuPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadScuPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for teting
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;

    //remote ext ps readings with TM +OBP + SCU load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["SCU_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["SCU_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtScuPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTscuPSVlineEdit->setText(loadRmtScuPSVolt);
        loadRmtScuPSVolt="GO";
        });
    }
    else
    {
        loadRmtScuPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTscuPSVlineEdit->setText(loadRmtScuPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtScuPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(75);
    });

    //CGU - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["CGU_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["CGU_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //local ext ps readings with TM + OBP + SCU + CGU load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["CGU_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["CGU_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadCguPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADcguPSVlineEdit->setText(loadCguPSVolt);
        loadCguPSVolt="GO";
        });
    }
    else
    {
        loadCguPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADcguPSVlineEdit->setText(loadCguPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadCguPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["CGU_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["CGU_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadCguPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADcguPSClineEdit->setText(loadCguPSCurr);
        loadCguPSCurr="GO";
        });
    }
    else
    {
        loadCguPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADcguPSClineEdit->setText(loadCguPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadCguPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;
    //remote ext ps readings with TM +OBP + SCU + CGU load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);
    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["CGU_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["CGU_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtCguPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTcguPSVlineEdit->setText(loadRmtCguPSVolt);
        loadRmtCguPSVolt="GO";
        });
    }
    else
    {
        loadRmtCguPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTcguPSVlineEdit->setText(loadRmtCguPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtCguPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(80);
    });

    //RPF - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["RPF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["RPF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //local ext ps readings with TM + OBP + SCU + CGU + RPF load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["RPF_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["RPF_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadRpfPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADrpfPSVlineEdit->setText(loadRpfPSVolt);
        loadRpfPSVolt="GO";
        });
    }
    else
    {
        loadRpfPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADrpfPSVlineEdit->setText(loadRpfPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRpfPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["RPF_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["RPF_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadRpfPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADrpfPSClineEdit->setText(loadRpfPSCurr);
        loadRpfPSCurr="GO";
        });
    }
    else
    {
        loadRpfPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADrpfPSClineEdit->setText(loadRpfPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadRpfPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;
    //remote ext ps readings with TM +OBP + SCU + CGU + RPF load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["RPF_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["RPF_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtRpfPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTrpfPSVlineEdit->setText(loadRmtRpfPSVolt);
        loadRmtRpfPSVolt="GO";
        });
    }
    else
    {
        loadRmtRpfPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTrpfPSVlineEdit->setText(loadRmtRpfPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtRpfPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(85);
    });

    if(akash=="prime")
    {
    //SKR HEATER - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SKR-HEATER_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SKR-HEATER_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //local ext ps readings with TM + OBP + SCU + CGU + RPF + SKR Heater load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["SKR_HEATER_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["SKR_HEATER_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadSkrHeaterPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrHeaterPSVlineEdit->setText(loadSkrHeaterPSVolt);
        loadSkrHeaterPSVolt="GO";
        });
    }
    else
    {
        loadSkrHeaterPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrHeaterPSVlineEdit->setText(loadSkrHeaterPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadSkrHeaterPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["SKR_HEATER_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["SKR_HEATER_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadSkrHeaterPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrHeaterPSClineEdit->setText(loadSkrHeaterPSCurr);
        loadSkrHeaterPSCurr="GO";
        });
    }
    else
    {
        loadSkrHeaterPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrHeaterPSClineEdit->setText(loadSkrHeaterPSCurr);
        //ui->mpslineEdit->setText(" NO GO");
        loadSkrHeaterPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;

    //remote ext ps readings with TM +OBP + SCU + CGU +RPF + SKR Heater load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);
    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["SKR_HEATER_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["SKR_HEATER_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtSkrHeaterPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrHeaterPSVlineEdit->setText(loadRmtSkrHeaterPSVolt);
        loadRmtSkrHeaterPSVolt="GO";
        });
    }
    else
    {
        loadRmtSkrHeaterPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrHeaterPSVlineEdit->setText(loadRmtSkrHeaterPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtSkrHeaterPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        ui->progressLabel->setText("MPS Check Progress -> ");
        ui->progressBar->setValue(90);
    });

    //SEEKER SP&M - OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SKR-SPM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SKR-SPM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //local ext ps readings with TM + OBP + SCU + CGU + RPF + SKR Heater + SKR SP&M load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["SKR_SPM_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["SKR_SPM_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadSkrSpmPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrSpmPSVlineEdit->setText(loadSkrSpmPSVolt);
        loadSkrSpmPSVolt="GO";
        });
    }
    else
    {
        loadSkrSpmPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrSpmPSVlineEdit->setText(loadSkrSpmPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadSkrSpmPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["SKR_SPM_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["SKR_SPM_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadSkrSpmPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrSpmPSClineEdit->setText(loadSkrSpmPSCurr);
        loadSkrSpmPSCurr="GO";
        });
    }
    else
    {
        loadSkrSpmPSCurr=QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrSpmPSClineEdit->setText(loadSkrSpmPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadSkrSpmPSCurr="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    usleep(1000);
    remote_volt =0;

    //remote ext ps readings with TM +OBP + SCU + CGU +RPF + SKR Heater + SKR SP&M load
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["EXT-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["SKR_SPM_RMT_LOAD_VOLT_MIN"].toInt() && remote_volt <= globalVariables["SKR_SPM_RMT_LOAD_VOLT_MAX"].toInt())
    {
        loadRmtSkrSpmPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrSpmPSVlineEdit->setText(loadRmtSkrSpmPSVolt);
        loadRmtSkrSpmPSVolt="GO";
        });
    }
    else
    {
        loadRmtSkrSpmPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrSpmPSVlineEdit->setText(loadRmtSkrSpmPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtSkrSpmPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    instr_obj->connectionClose();

    QMetaObject::invokeMethod(this, [this]() {
        ui->progressLabel->setText("MPS Check Progress -> ");
        ui->progressBar->setValue(95);
    });

    // SKR-TX OPTODO ON
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SEEKER-TX_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SEEKER-TX_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    // //seeker ps set
    instr_obj->connectionOpen(globalVariables["SEEKER-PS_VISA_ADDRESS"]);

    //local SEEKER ps readings with load
    local_volt=instr_obj->readInstrumentVolt();
    if(local_volt >= globalVariables["SKR_TX_LOCAL_LOAD_VOLT_MIN"].toInt() && local_volt <= globalVariables["SKR_TX_LOCAL_LOAD_VOLT_MAX"].toInt())
    {
        loadSkrTxPSVolt=QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrTXPSVlineEdit->setText(loadSkrTxPSVolt);
        loadSkrTxPSVolt="GO";
        });
    }
    else
    {
        loadSkrTxPSVolt =QString::number(local_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrTXPSVlineEdit->setText(loadSkrTxPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadSkrTxPSVolt="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    local_curr=instr_obj->readInstrumentCurrent();
    if(local_curr >= globalVariables["SKR_TX_LOCAL_LOAD_CURR_MIN"].toDouble() && local_curr <= globalVariables["SKR_TX_LOCAL_LOAD_CURR_MAX"].toDouble())
    {
        loadSkrTxPSCurr =QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrTXPSClineEdit->setText(loadSkrTxPSCurr);
        loadSkrTxPSCurr ="GO";
        });
    }
    else
    {
        loadSkrTxPSCurr =QString::number(local_curr);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADskrTXPSClineEdit->setText(loadSkrTxPSCurr);
        //ui->mpslineEdit->setText("NO GO");
        loadSkrTxPSCurr ="NO GO";
        });
        mpsCheckStatus=false;

        //open for testing
        //instr_obj->connectionClose();
        //return;
    }

    remote_volt=0;
    usleep(1000);

    //remote seeker ps readings
    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["SEEKER-PS_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["SEEKER-PS_AI_CHANNEL"].toInt());
    }

    usleep(1000);

     remote_volt *=globalVariables["DIFF_VOLT_SKR_MULTIPLIER"].toInt();

    if(remote_volt >= globalVariables["SEEKER-PS_RMT_VOLT_MIN"].toInt() && remote_volt <= globalVariables["SEEKER-PS_RMT_VOLT_MAX"].toInt())
    {
        loadRmtSkrTxPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrTxPSVlineEdit->setText(loadRmtSkrTxPSVolt);
        loadRmtSkrTxPSVolt="GO";
        });
    }
    else
    {
        loadRmtSkrTxPSVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->LOADRMTskrTxPSVlineEdit->setText(loadRmtSkrTxPSVolt);
        //ui->mpslineEdit->setText("NO GO");
        loadRmtSkrTxPSVolt="NO GO";
        });
        mpsCheckStatus=false;
        if(remote_volt>300)
        {
            QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    instr_obj->connectionClose();
    }

    instr_obj->connectionClose();

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса MPS -> ");
        }
        else
        {
            ui->progressLabel->setText("MPS Check Progress -> ");
        }
        ui->progressBar->setValue(100);
    });

    if(mpsCheckStatus)
    {
        ui->mpslineEdit->setStyleSheet("QLineEdit { color: green; }");
        if(ui->language->isChecked())
        {
             ui->mpslineEdit->setText("ГО");
        }
        else
        {
            ui->mpslineEdit->setText("GO");
        }
    }
    else
    {
        ui->mpslineEdit->setStyleSheet("QLineEdit { color: red; }");
        if(ui->language->isChecked())
        {
             ui->mpslineEdit->setText("HE ГО");
        }
        else
        {
            ui->mpslineEdit->setText("NO GO");
        }
    }
}

void MainWindow::mpsRelayOFF()
{
    //SKR SP&M REPLAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SKR-SPM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SKR-SPM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //SKR HEATER OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SKR-HEATER_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SKR-HEATER_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //RPF RELAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["RPF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["RPF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //CGU RELAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["CGU_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["CGU_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //SCU RELAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SCU_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SCU_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //OBP RELAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["OBP_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["OBP_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //TM RELAY OFF
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["TM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["TM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    // SKR-TX relay off
    //if(lt_missile_flag)
    {
    driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SEEKER-TX_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else
    {
    driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SEEKER-TX_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
}

void MainWindow::statusRelayOFF()
{
    //if(lt_missile_flag)
    {
        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

    }
    // else if(rt_missile_flag)
    {
        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
}

void MainWindow::obpRelayOFF()
{
    //obp reset relay off
    //if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_LEFT"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_RIGHT"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
}
void MainWindow::pyroRelayOFF()
{
    //if(lt_missile_flag)
    {
        //PYRO - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_REALY2_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //INT PS - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    //else if(rt_missile_flag)
    {
        //PYRO - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_REALY2_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //INT PS - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
}

void MainWindow::mpsOutOFF()
{
    //ext ps out off
    instr_obj->connectionOpen(globalVariables["EXT-PS_VISA_ADDRESS"]);
    instr_obj->setInstrumentOutOFF();
    instr_obj->connectionClose();

    //status ps out off
    instr_obj->connectionOpen(globalVariables["STATUS-PS_VISA_ADDRESS"]);
    instr_obj->setInstrumentOutOFF();
    instr_obj->connectionClose();

    //int ps out off
    instr_obj->connectionOpen(globalVariables["INT-PS_VISA_ADDRESS"]);
    instr_obj->setInstrumentOutOFF();
    instr_obj->connectionClose();

    //pyro ps out off
    instr_obj->connectionOpen(globalVariables["PYRO-PS_VISA_ADDRESS"]);
    instr_obj->setInstrumentOutOFF();
    instr_obj->connectionClose();

    if(akash=="prime")
    {
        //seeker ps out off
        instr_obj->connectionOpen(globalVariables["SEEKER-PS_VISA_ADDRESS"]);
        instr_obj->setInstrumentOutOFF();
        instr_obj->connectionClose();
    }
}

void MainWindow::statusCheck()
{
    QMetaObject::invokeMethod(this, [this]() {
    ui->statuslineEdit->setStyleSheet("QLineEdit { color: black; }");
    ui->sadamRelayStatusLineEdit->setText("");
    ui->nozzleMatingVoltageLineEdit->setText("");
    ui->sustainerFireVoltageLineEdit->setText("");
    ui->neRelayStatusLineEdit->setText("");
    ui->umbilical1StatusLineEdit->setText("");
    ui->umbilical2StatusLineEdit->setText("");
    ui->misTMStatusLineEdit->setText("");
    ui->seekerSTBRelayStatusLineEdit->setText("");
    ui->seekerMISStatusLineEdit->setText("");

    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса STATUS -> ");
    }
    else
    {
        ui->progressLabel->setText("STATUS Check Progress -> ");
    }

    ui->progressBar->setValue(0);
    });

    statusCheckStatus=true;

    double volt=0;

    //SADAM RELAY COIL - OPTODO ON
    if(lt_missile_flag)
    {
        //SADAM relay on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //5v STATUS RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        int channel = globalVariables["SADAM_REALY_STATUS_CHANNEL_LEFT"].toInt();
        int point = channel % 8;
        int port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        sleep(1);
        int value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());

        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            sadamRelayStatus=QString::number(1);
            ui->sadamRelayStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            sadamRelayStatus=QString::number(0);
            ui->sadamRelayStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }

                on_home_clicked();
                exit(0);
            }
        }

        usleep(1000);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(10);
        });

        volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["NE_MATING_STATUS_AI_CHANNEL"].toInt());

        volt *=globalVariables["STS_VOLT_MULTIPLIER"].toInt();
        usleep(1000);

        if(volt >= globalVariables["NE_MATING_VOLT_MIN"].toInt() && volt <= globalVariables["NE_MATING_VOLT_MAX"].toInt())
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            nozzleMatingVoltage=QString::number(volt);
            ui->nozzleMatingVoltageLineEdit->setText(nozzleMatingVoltage);
            nozzleMatingVoltage="MATED";
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            nozzleMatingVoltage=QString::number(volt);
            ui->nozzleMatingVoltageLineEdit->setText(nozzleMatingVoltage);
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            nozzleMatingVoltage="NO GO";
            });

            if(volt>300)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
            //return;
        }

        usleep(1000);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(20);
        });

        volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["SI_FIRE_RELAY_STATUS_AI_CHANNEL"].toInt());

        volt *=globalVariables["STS_VOLT_MULTIPLIER"].toInt();
        usleep(1000);
        //qDebug()<<"sus volt"<<volt;

        if(volt >= globalVariables["SI_MATING_VOLT_MIN"].toInt() && volt <= globalVariables["SI_MATING_VOLT_MAX"].toInt())
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            sustainerFireVoltage=QString::number(volt);
            ui->sustainerFireVoltageLineEdit->setText(sustainerFireVoltage);
            sustainerFireVoltage="ARMED";
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            sustainerFireVoltage=QString::number(volt);
            ui->sustainerFireVoltageLineEdit->setText(sustainerFireVoltage);
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            sustainerFireVoltage="NO GO";
            });

            if(volt>300)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
            //return;
        }

        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(30);
        });

        channel = globalVariables["NE_RELAY_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        //NE relay on
        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            neRelayStatus=QString::number(1);
            ui->neRelayStatusLineEdit->setText("ARMED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            neRelayStatus=QString::number(0);
            ui->neRelayStatusLineEdit->setText("SAFE");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(40);
        });

        channel = globalVariables["UMB1_MATING_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);
        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical1Status=QString::number(1);
            ui->umbilical1StatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical1Status=QString::number(0);
            ui->umbilical1StatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(50);
        });

        channel = globalVariables["UMB2_MATING_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical2Status=QString::number(1);
            ui->umbilical2StatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical2Status=QString::number(0);
            ui->umbilical2StatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(60);
        });

        channel = globalVariables["MIS_TM_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            misTMStatus=QString::number(1);
            ui->misTMStatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            misTMStatus=QString::number(0);
            ui->misTMStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(70);
        });

        if(akash=="prime")
        {
        channel =globalVariables["SEEKER_STB_RELAY_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerSTBRelayStatus=QString::number(1);
            ui->seekerSTBRelayStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerSTBRelayStatus=QString::number(0);
            ui->seekerSTBRelayStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            ui->progressLabel->setText("STATUS Check Progress -> ");
            ui->progressBar->setValue(80);
        });

        channel =globalVariables["SEEKER_MISSILE_STATUS_CHANNEL_LEFT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerMISStatus=QString::number(1);
            ui->seekerMISStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerMISStatus=QString::number(0);
            ui->seekerMISStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                on_home_clicked();
                exit(0);
            }
        }
        }

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY oN
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(100);
        });

    }
    else if(rt_missile_flag)
    {
        //SADAM relay on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //5v STATUS RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        int channel = globalVariables["SADAM_REALY_STATUS_CHANNEL_RIGHT"].toInt();
        int point = channel % 8;
        int port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        int value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            sadamRelayStatus=QString::number(1);
            ui->sadamRelayStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            sadamRelayStatus=QString::number(0);
            ui->sadamRelayStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        usleep(1000);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(10);
        });

        volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["NE_MATING_STATUS_AI_CHANNEL"].toInt());

        volt *=globalVariables["STS_VOLT_MULTIPLIER"].toInt();
        usleep(1000);


        if(volt >= globalVariables["NE_MATING_VOLT_MIN"].toInt() && volt <= globalVariables["NE_MATING_VOLT_MAX"].toInt())
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            nozzleMatingVoltage=QString::number(volt);
            ui->nozzleMatingVoltageLineEdit->setText(nozzleMatingVoltage);
            nozzleMatingVoltage="MATED";
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            nozzleMatingVoltage=QString::number(volt);
            ui->nozzleMatingVoltageLineEdit->setText(nozzleMatingVoltage);
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            nozzleMatingVoltage="NO GO";
            });

            if(volt>300)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
            //return;
        }

        usleep(1000);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(20);
        });

        volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["SI_FIRE_RELAY_STATUS_AI_CHANNEL"].toInt());

        volt *=globalVariables["STS_VOLT_MULTIPLIER"].toInt();
        usleep(1000);

        if(volt >= globalVariables["SI_MATING_VOLT_MIN"].toInt() && volt <= globalVariables["SI_MATING_VOLT_MAX"].toInt())
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            sustainerFireVoltage=QString::number(volt);
            ui->sustainerFireVoltageLineEdit->setText(sustainerFireVoltage);
            sustainerFireVoltage="ARMED";
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this, &volt]() {
            sustainerFireVoltage=QString::number(volt);
            ui->sustainerFireVoltageLineEdit->setText(sustainerFireVoltage);
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            sustainerFireVoltage="NO GO";
            });

            if(volt>300)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
            //return;
        }

        //SADAM relay off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SADAM_RELAY_COIL_CONTROL_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(30);
        });

        channel = globalVariables["NE_RELAY_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        //NE relay on
        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            neRelayStatus=QString::number(1);
            ui->neRelayStatusLineEdit->setText("ARMED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            neRelayStatus=QString::number(0);
            ui->neRelayStatusLineEdit->setText("SAFE");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(40);
        });

        channel = globalVariables["UMB1_MATING_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);
        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical1Status=QString::number(1);
            ui->umbilical1StatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical1Status=QString::number(0);
            ui->umbilical1StatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(50);
        });

        channel = globalVariables["UMB2_MATING_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical2Status=QString::number(1);
            ui->umbilical2StatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            umbilical2Status=QString::number(0);
            ui->umbilical2StatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(60);
        });

        channel = globalVariables["MIS_TM_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            misTMStatus=QString::number(1);
            ui->misTMStatusLineEdit->setText("MATED");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            misTMStatus=QString::number(0);
            ui->misTMStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            //ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                if(ui->language->isChecked())
                {
                     QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр цифровой DI карты!!");
                }
                else
                {
                    QMessageBox::warning(this,"Critical Warning","Failed to get Digital DI card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                }
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(70);
        });

        if(akash=="prime")
        {
        channel =globalVariables["SEEKER_STB_RELAY_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerSTBRelayStatus=QString::number(1);
            ui->seekerSTBRelayStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerSTBRelayStatus=QString::number(0);
            ui->seekerSTBRelayStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Digital IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                on_home_clicked();
                exit(0);
            }
        }

        QMetaObject::invokeMethod(this, [this]() {
            ui->progressLabel->setText("STATUS Check Progress -> ");
            ui->progressBar->setValue(80);
        });

        channel =globalVariables["SEEKER_MISSILE_STATUS_CHANNEL_RIGHT"].toInt();
        point = channel % 8;
        port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);

        value=driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        if(value==1)
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerMISStatus=QString::number(1);
            ui->seekerMISStatusLineEdit->setText("GO");
            });
        }
        else
        {
            QMetaObject::invokeMethod(this, [this]() {
            seekerMISStatus=QString::number(0);
            ui->seekerMISStatusLineEdit->setText("NO GO");
            statusCheckStatus=false;
            ui->statuslineEdit->setText("NO GO");
            });

            if(value>90)
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Digital IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
                on_home_clicked();
                exit(0);
            }
        }
        }

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE ARM RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_ARM_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //NE SAFE RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["NE_SAFE_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса STATUS -> ");
            }
            else
            {
                ui->progressLabel->setText("STATUS Check Progress -> ");
            }
            ui->progressBar->setValue(100);
        });
    }

    if(statusCheckStatus)
    {
        ui->statuslineEdit->setStyleSheet("QLineEdit { color: green; }");
        if(ui->language->isChecked())
        {
             ui->statuslineEdit->setText("ГО");
        }
        else
        {
            ui->statuslineEdit->setText("GO");
        }
    }
    else
    {
        ui->statuslineEdit->setStyleSheet("QLineEdit { color: red; }");
        if(ui->language->isChecked())
        {
             ui->statuslineEdit->setText("HE ГО");
        }
        else
        {
            ui->statuslineEdit->setText("NO GO");
        }
    }
}

void MainWindow::obpCheck()
{
    ui->obplineEdit->setStyleSheet("QLineEdit { color: black; }");
    //obp reset relay on
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_LEFT"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_RIGHT"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса OBP -> ");
        }
        else
        {
            ui->progressLabel->setText("OBP Check Progress -> ");
        }

        ui->progressBar->setValue(0);
    });

    obpCheckStatus=false;

    //INT PS - OPTODO ON
    if(lt_missile_flag)
    {
        QMetaObject::invokeMethod(this, [this]() {
        ui->OBPtextBrowser->append("S COM for Left Missile Selected \n");
        });

        usleep(1000);
        //serialcomm_obj->connectPort(globalVariables["MIS_LEFT"].toInt());
        bool invoked = QMetaObject::invokeMethod(serialcomm_obj, "connectPort",
                                                     Qt::QueuedConnection,
                                                     Q_ARG(int, globalVariables["MIS_LEFT"].toInt()));
            if (!invoked) {
                ui->OBPtextBrowser->append("Failed to invoke connectPort");
                //qDebug()<<"Failed to invoke connectPort";
            } else
            {
                ui->OBPtextBrowser->append("connectPort invoked (queued to worker thread)");
                //qDebug()<<"connectPort invoked (queued to worker thread)";
            }
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        QMetaObject::invokeMethod(this, [this]() {
        ui->OBPtextBrowser->append("S COM for Right Missile Selected \n");
        });
        usleep(1000);
        //serialcomm_obj->connectPort(globalVariables["MIS_RIGHT"].toInt());
        bool invoked = QMetaObject::invokeMethod(serialcomm_obj, "connectPort",Qt::QueuedConnection,Q_ARG(int, globalVariables["MIS_RIGHT"].toInt()));
            if (!invoked) {
                ui->OBPtextBrowser->append("Failed to invoke connectPort");
                //qDebug()<<"Failed to invoke connectPort";
            } else
            {
                ui->OBPtextBrowser->append("connectPort invoked (queued to worker thread)");
                //qDebug()<<"connectPort invoked (queued to worker thread)";
            }
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    sleep(2);

    //obp  relay off
    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_LEFT"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
         driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_1"].toInt(),globalVariables["OBP_RST_RELAY_CHANNEL_RIGHT"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
         driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
}

void MainWindow::onResponse(const QByteArray &response)
{
    //QString bytes_hex = QString::fromLatin1(response.toHex());
    QString bytes_hex = QString::fromLatin1(response.toHex(' ').toUpper());
    //qDebug() << "Return Response (hex): " << bytes_hex;

    obpCheckStatus = !response.isEmpty();

    if (response.isEmpty())
    {
        ui->OBPtextBrowser->append("OBP Check: TIMEOUT");
        obpStatus = "LINK FAIL";
        ui->obplineEdit->setStyleSheet("QLineEdit { color: red; }");
        ui->obplineEdit->setText(ui->language->isChecked() ? QStringLiteral("НЕ ГО") : QStringLiteral("NO GO"));
        ui->obpStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
    }
    else
    {
        QString msg=QStringLiteral("Response received (ASCII): %1").arg(bytes_hex);
        ui->OBPtextBrowser->append(msg);
        obpStatus = "LINK SUCCESS";
        ui->obplineEdit->setStyleSheet("QLineEdit { color: green; }");
        ui->obplineEdit->setText(ui->language->isChecked() ? QStringLiteral("ГО") : QStringLiteral("GO"));
        ui->obpStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");

    }
    ui->progressBar->setValue(100);
}

void MainWindow::updateProgressBar(int i)
{
    QMetaObject::invokeMethod(this, [this,i]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса OBP -> ");
        }
        else
        {
            ui->progressLabel->setText("OBP Check Progress -> ");
        }
        ui->progressBar->setValue(i);
    });
}

void MainWindow::obpCommunication(const QString &msg)
{
    //ui->OBPtextBrowser->append(msg);
    if(loginSelection==globalVariables["ADMIN"].toInt())
    {
     qDebug()<<msg;
    }
    if (msg.contains("Timeout"))
    {
        ui->OBPtextBrowser->append("OBP Check: TIMEOUT");
        obpStatus = "LINK FAIL";
        ui->obplineEdit->setStyleSheet("QLineEdit { color: red; }");
        ui->obplineEdit->setText(ui->language->isChecked() ? QStringLiteral("НЕ ГО") : QStringLiteral("NO GO"));
        ui->obpStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");

        if(ui->language->isChecked())
        {
             ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
        }
        else
        {
            ui->status->setText("MSMC-P/MCF-P Health Status: NO GO");
        }

        ui->status->setStyleSheet("QLabel { color: red; }");
        ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");

        ui->progressBar->setValue(100);
    }
}

void MainWindow::scuCheck()
{
    QMetaObject::invokeMethod(this, [this]() {
        ui->sculineEdit->setStyleSheet("QLineEdit { color: black; }");
        ui->cageLineEdit->setText("");
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса SCU -> ");
        }
        else
        {
            ui->progressLabel->setText("SCU Check Progress -> ");
        }
        ui->progressBar->setValue(0);
    });

    scuCheckStatus=false;

    double remote_volt=0;

    if(lt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["SCU_CAGE_AI_CHANNEL"].toInt());
         //driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["SCU_CAGE_AI_CHANNEL"].toInt());
         //driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toInt();

    if (remote_volt >= globalVariables["CAGE_VOLT_MIN"].toInt() && remote_volt <= globalVariables["CAGE_VOLT_MAX"].toInt())
    {
        scuCheckStatus=true;
        cageStatus="UN-CAGED";
        QString cageStatusVolt=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this, cageStatusVolt]() {
        ui->cageLineEdit->setText(cageStatusVolt);
        ui->sculineEdit->setStyleSheet("QLineEdit { color: green; }");
        if(ui->language->isChecked())
        {
             ui->sculineEdit->setText("ГО");
        }
        else
        {
            ui->sculineEdit->setText("GO");
        }
        });
    }
    else
    {
        scuCheckStatus=false;
        cageStatus="CAGED";
        cageStatus=QString::number(remote_volt);
        QMetaObject::invokeMethod(this, [this]() {
        ui->cageLineEdit->setText("CAGED");
        ui->sculineEdit->setStyleSheet("QLineEdit { color: red; }");
        if(ui->language->isChecked())
        {
            ui->sculineEdit->setText("HE ГО");
        }
        else
        {
            ui->sculineEdit->setText("NO GO");
        }
        });

        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса SCU -> ");
        }
        else
        {
            ui->progressLabel->setText("SCU Check Progress -> ");
        }
        ui->progressBar->setValue(100);
    });
}

void MainWindow::rpfCheck()
{
    QMetaObject::invokeMethod(this, [this]() {

        ui->rpflineEdit->setStyleSheet("QLineEdit { color: black; }");

        //DSO set
        QString status=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
        if(status=="OPEN")
        {
            if(lt_missile_flag)
            {
                instr_obj->firingPulseLeftInIt();
            }
            else if(rt_missile_flag)
            {
                instr_obj->firingPulseRightInIt();
            }
            instr_obj->connectionClose();
        }
    });

    //sleep(2);

    if(lt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
         //driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
         //driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    QMetaObject::invokeMethod(this, [this]() {
    ui->amplitudeLineEdit->setText("");
    ui->firingPulse->setText("");
    ui->firingPulseTM->setText("");
    ui->widthLineEdit->setText("");
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса RPF -> ");
    }
    else
    {
        ui->progressLabel->setText("RPF Check Progress -> ");
    }
    ui->progressBar->setValue(0);
    });

    rpfCheckStatus=false;

    double remote_volt=0;

     usleep(5000);

    if(lt_missile_flag)
    {
         //driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
         remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["FIRING_PULSE_AI_CHANNEL"].toInt());
    }
    else if(rt_missile_flag)
    {
         //driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["FIRING_PULSE_AI_CHANNEL"].toInt());
    }

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toDouble();

    if (remote_volt >= globalVariables["FIRING_PULSE_MIN"].toDouble() && remote_volt <= globalVariables["FIRING_PULSE_MAX"].toDouble())
    {
        rpfCheckStatus=true;
        QMetaObject::invokeMethod(this, [this, &remote_volt]() {
        firingPulseStatus=QString::number(remote_volt);
        ui->firingPulse->setText(firingPulseStatus);
        firingPulseStatus="GO";
        //ui->rpflineEdit->setText("GO");
        });
    }
    else
    {
        rpfCheckStatus=false;
        QMetaObject::invokeMethod(this, [this, &remote_volt]() {
        firingPulseStatus=QString::number(remote_volt);
        ui->firingPulse->setText(firingPulseStatus);
        firingPulseStatus="NO GO";
        //ui->rpflineEdit->setText("NO GO");
        });

        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса RPF -> ");
        }
        else
        {
            ui->progressLabel->setText("RPF Check Progress -> ");
        }
        ui->progressBar->setValue(20);
    });

     usleep(5000);

    if(lt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["FIRING_PULSE_TM_AI_CHANNEL"].toInt());
        //driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        remote_volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["FIRING_PULSE_TM_AI_CHANNEL"].toInt());
        //driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    remote_volt *=globalVariables["DIFF_VOLT_MULTIPLIER"].toDouble();

    if (remote_volt >= globalVariables["FIRING_PULSE_TM_MIN"].toDouble() && remote_volt <= globalVariables["FIRING_PULSE_TM_MAX"].toDouble())
    {
        rpfCheckStatus=true;
        QMetaObject::invokeMethod(this, [this, &remote_volt]() {
        firingPulseTMStatus=QString::number(remote_volt);
        ui->firingPulseTM->setText(firingPulseTMStatus);
        firingPulseTMStatus="GO";
        //ui->rpflineEdit->setText("GO");
        });
    }
    else
    {
        rpfCheckStatus=false;
        QMetaObject::invokeMethod(this, [this, &remote_volt]() {
        firingPulseTMStatus=QString::number(remote_volt);
        ui->firingPulseTM->setText(firingPulseTMStatus);
        firingPulseTMStatus="NO GO";
        //ui->rpflineEdit->setText("NO GO");
        });

        if(remote_volt>300)
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this,"Критическое предупреждение","Не удалось получить экземпляр аналоговой входной карты!!");
            }
            else
            {
                QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
            }
            on_home_clicked();
            exit(0);
        }
        //return;
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса RPF -> ");
        }
        else
        {
            ui->progressLabel->setText("RPF Check Progress -> ");
        }
        ui->progressBar->setValue(40);
    });

     usleep(5000);

    //DSO set
    QString statusCapture=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
    if(statusCapture=="OPEN")
    {
        if(lt_missile_flag)
        {
            instr_obj->firingPluseCaptureLeft();
        }
        else if(rt_missile_flag)
        {
            instr_obj->firingPluseCaptureRight();
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса RPF -> ");
            }
            else
            {
                ui->progressLabel->setText("RPF Check Progress -> ");
            }
            ui->progressBar->setValue(60);
        });

        usleep(5000);

        const char* val=instr_obj->responsePWidthStr;
        const char* res="1.12104e-44";
        if(strcmp(val,res)==0)
        {
            if(lt_missile_flag)
            {
                instr_obj->firingPluseCaptureLeft();
            }
            else if(rt_missile_flag)
            {
                instr_obj->firingPluseCaptureRight();
            }
        }

        float vAmp = std::atof(instr_obj->responseVAmpStr);
        float pWidth = std::atof(instr_obj->responsePWidthStr);

        if (vAmp >= globalVariables["RPF_AMP_MIN"].toDouble() && vAmp <= globalVariables["RPF_AMP_MAX"].toDouble())
        {
            rpfCheckStatus=true;
            QMetaObject::invokeMethod(this, [this, &vAmp]() {
            firPulseAmplitude=instr_obj->responseVAmpStr;//QString::number(vAmp);
            ui->amplitudeLineEdit->setText(firPulseAmplitude);
            firPulseAmplitude="GO";
            if(loginSelection==globalVariables["ADMIN"].toInt())
            {
                qDebug()<<vAmp;
            }
            });
        }
        else
        {
            rpfCheckStatus=false;
            QMetaObject::invokeMethod(this, [this, &vAmp]() {
            firPulseAmplitude=instr_obj->responseVAmpStr;//QString::number(vAmp);
            ui->amplitudeLineEdit->setText("NO GO");
            firPulseAmplitude="NO GO";
            //ui->rpflineEdit->setText("NO GO");
            if(loginSelection==globalVariables["ADMIN"].toInt())
            {
                qDebug()<<vAmp;
            }
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса RPF -> ");
            }
            else
            {
                ui->progressLabel->setText("RPF Check Progress -> ");
            }
            ui->progressBar->setValue(80);
        });

        usleep(5000);

        if ((pWidth >= globalVariables["RPF_WIDTH_MIN"].toDouble() && pWidth <= globalVariables["RPF_WIDTH_MAX"].toDouble()) && (rpfCheckStatus==true))
        {
            rpfCheckStatus=true;
            QMetaObject::invokeMethod(this, [this, &pWidth]() {
            firPulseWidth=instr_obj->responsePWidthStr;//QString::number(pWidth);
            ui->widthLineEdit->setText(firPulseWidth);
            firPulseWidth="GO";
            //ui->rpflineEdit->setText("GO");
            if(loginSelection==globalVariables["ADMIN"].toInt())
            {
                qDebug()<<pWidth;
            }
            });
        }
        else
        {
            rpfCheckStatus=false;
            QMetaObject::invokeMethod(this, [this, &pWidth]() {
            firPulseWidth=instr_obj->responsePWidthStr;//QString::number(pWidth);
            ui->widthLineEdit->setText("NO GO");
            firPulseWidth="NO GO";
            //ui->rpflineEdit->setText("NO GO");
            if(loginSelection==globalVariables["ADMIN"].toInt())
            {
                qDebug()<<pWidth;
            }
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса RPF -> ");
            }
            else
            {
                ui->progressLabel->setText("RPF Check Progress -> ");
            }
            ui->progressBar->setValue(100);
        });

        if(rpfCheckStatus)
        {
            ui->rpflineEdit->setStyleSheet("QLineEdit { color: green; }");
            if(ui->language->isChecked())
            {
                ui->rpflineEdit->setText("ГО");
            }
            else
            {
                ui->rpflineEdit->setText("GO");
            }
        }
        else
        {
            ui->rpflineEdit->setStyleSheet("QLineEdit { color: red; }");
            if(ui->language->isChecked())
            {
                ui->rpflineEdit->setText("HE ГО");
            }
            else
            {
                ui->rpflineEdit->setText("NO GO");
            }
        }
    }
    else
    {
        firPulseWidth="NO GO";
        firPulseAmplitude="NO GO";
        rpfCheckStatus=false;
        QMetaObject::invokeMethod(this, [this]() {
            ui->rpflineEdit->setStyleSheet("QLineEdit { color: red; }");
            if(ui->language->isChecked())
            {
                ui->rpflineEdit->setText("HE ГО");
            }
            else
            {
                ui->rpflineEdit->setText("NO GO");
            }
        });
    }

}

void MainWindow::pyroCheck()
{
    QMetaObject::invokeMethod(this, [this]() {
    ui->pyrolineEdit->setStyleSheet("QLineEdit { color: black; }");
    ui->gndRelay1LineEdit->setText("");
    ui->airBottleSquibFireRelayLineEdit->setText("");
    ui->thermalBatterySquibFireRelayLineEdit->setText("");
    ui->gndRelay2LineEdit->setText("");
    ui->groundPyroRelayStatusLineEdit->setText("");
    ui->boosterPyroRelayStatusLineEdit->setText("");
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса PYRO -> ");
    }
    else
    {
        ui->progressLabel->setText("PYRO Check Progress -> ");
    }
    ui->progressBar->setValue(0);
    });

    pyroCheckStatus=false;

    double volt=0;
    bool air_bottle_check =false;
    bool thermal_battery_check = false;
    bool booster_ign_check=false;

    if(lt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
        ui->gndRelay1LineEdit->setText("FIRED");
        });

        sleep(1);
        groundRelay1= QString::number(1);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(10);
        });

        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        airBottleSquibFireRelayStatus= QString::number(driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        airBottleSquibFireRelayStatus=QString::number(1);
        if(airBottleSquibFireRelayStatus.toDouble())
        {
            air_bottle_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->airBottleSquibFireRelayLineEdit->setText("FIRED");
            });
        }
        else
        {
            airBottleSquibFireRelayStatus=QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->airBottleSquibFireRelayLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(30);
        });

        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        thermalBatterySquibFireRelayStatus= QString::number(driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        thermalBatterySquibFireRelayStatus=QString::number(1);
        if(thermalBatterySquibFireRelayStatus.toDouble())
        {
            thermal_battery_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->thermalBatterySquibFireRelayLineEdit->setText("FIRED");
            });
        }
        else
        {
            thermalBatterySquibFireRelayStatus=QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->thermalBatterySquibFireRelayLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(50);
        });

        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_RELAY2_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        ui->gndRelay2LineEdit->setText("FIRED");
        sleep(1);
        groundRelay2= QString::number(1);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(60);
        });

        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        boosterPyroRelayStatus= QString::number(driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        boosterPyroRelayStatus= QString::number(1);
        if( boosterPyroRelayStatus.toDouble())
        {
            booster_ign_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->boosterPyroRelayStatusLineEdit->setText("FIRED");
            });
        }
        else
        {
            boosterPyroRelayStatus= QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->boosterPyroRelayStatusLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(75);
        });

        //5v STATUS RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY on
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());


        int channel = globalVariables["GND_FIRE_RELAY_CHANNEL_1_2_LEFT"].toInt();
        int point = channel % 8;
        int port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);
        volt =driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());
        usleep(1000);

        //PRESSURE SWITCH RELAY ON
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PRESURE_SWITCH_RELAY"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        //PRESSURE SWITCH RELAY OFF
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PRESURE_SWITCH_RELAY"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //PYRO - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_RELAY2_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //INT PS - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(90);
        });

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

    }
    else if(rt_missile_flag)
    {
         driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        QMetaObject::invokeMethod(this, [this]() {
        ui->gndRelay1LineEdit->setText("FIRED");
        });
        sleep(1);
        groundRelay1=  QString::number(1);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(10);
        });

        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        airBottleSquibFireRelayStatus=QString::number(driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        airBottleSquibFireRelayStatus=QString::number(1);
        if( airBottleSquibFireRelayStatus.toDouble())
        {
            air_bottle_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->airBottleSquibFireRelayLineEdit->setText("FIRED");
            });
        }
        else
        {
            airBottleSquibFireRelayStatus=QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->airBottleSquibFireRelayLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(30);
        });

        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        thermalBatterySquibFireRelayStatus= QString::number(driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        thermalBatterySquibFireRelayStatus= QString::number(1);
        if(thermalBatterySquibFireRelayStatus.toDouble())
        {
            thermal_battery_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->thermalBatterySquibFireRelayLineEdit->setText("FIRED");
            });
        }
        else
        {
            thermalBatterySquibFireRelayStatus= QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->thermalBatterySquibFireRelayLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(50);
        });

        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_RELAY2_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        QMetaObject::invokeMethod(this, [this]() {
        ui->gndRelay2LineEdit->setText("FIRED");
        });
        sleep(1);
        groundRelay2=  QString::number(1);

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(60);
        });

        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        boosterPyroRelayStatus=QString::number(driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_READ_POINT_MODE"].toInt()));
        boosterPyroRelayStatus= QString::number(1);
        if( boosterPyroRelayStatus.toDouble())
        {
            booster_ign_check=true;
            QMetaObject::invokeMethod(this, [this]() {
            ui->boosterPyroRelayStatusLineEdit->setText("FIRED");
            });
        }
        else
        {
            boosterPyroRelayStatus= QString::number(0);
            QMetaObject::invokeMethod(this, [this]() {
            ui->boosterPyroRelayStatusLineEdit->setText("NO GO");
            //ui->pyrolineEdit->setText("NO GO");
            });
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(75);
        });

        //5v STATUS RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY on
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        int channel = globalVariables["GND_FIRE_RELAY_CHANNEL_1_2_RIGHT"].toInt();
        int point = channel % 8;
        int port=0;
        if(channel >7)
        {
            port=1;
        }
        if(channel >15)
        {
            port=2;
        }
        if(channel>23)
        {
            port=3;
        }

        usleep(1000);
        volt =driver_obj->optoDI(globalVariables["ap441_0"].toInt(),port,point,globalVariables["DI_READ_POINT_MODE"].toInt());

        //PRESSURE SWITCH RELAY ON
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PRESURE_SWITCH_RELAY"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        usleep(1000);
        //PRESSURE SWITCH RELAY OFF
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PRESURE_SWITCH_RELAY"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //PYRO - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["BOOSTER_IGN_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_REALY2_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["THERMAL_BATTERY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["AIR_BOTTLE_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["GND_RELAY1_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PYRO_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //INT PS - OPTODO OFF
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["INT_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 ui->progressLabel->setText("Проверка прогресса PYRO -> ");
            }
            else
            {
                ui->progressLabel->setText("PYRO Check Progress -> ");
            }
            ui->progressBar->setValue(90);
        });

        //5v STATUS RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["5V_STATUS_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

        //LIFT OFF RELAY off
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["LIFT_OFF_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());

    }

    bool gndPyroStatus=false;
    if(volt >= globalVariables["GND_PYRO_STATUS_MIN"].toInt() && volt <= globalVariables["GND_PYRO_STATUS_MAX"].toInt())
    {
        gndPyroStatus=true;
        QMetaObject::invokeMethod(this, [this, &volt]() {
        groundPyroRelayStatus=QString::number(volt);
        ui->groundPyroRelayStatusLineEdit->setText("FIRED");
        });
    }
    else
    {
        gndPyroStatus=false;
        QMetaObject::invokeMethod(this, [this, &volt]() {
        groundPyroRelayStatus=QString::number(volt);
        ui->groundPyroRelayStatusLineEdit->setText("NO GO");
        });
    }

    //if all are fired then pyro pass
    if (air_bottle_check && thermal_battery_check && booster_ign_check && gndPyroStatus)
    {
        pyroCheckStatus=true;
        QMetaObject::invokeMethod(this, [this]() {
            ui->pyrolineEdit->setStyleSheet("QLineEdit { color: green; }");
            if(ui->language->isChecked())
            {
                ui->pyrolineEdit->setText("ГО");
            }
            else
            {
                ui->pyrolineEdit->setText("GO");
            }
        });
    }
    else
    {
        pyroCheckStatus=false;
        QMetaObject::invokeMethod(this, [this]() {
            ui->pyrolineEdit->setStyleSheet("QLineEdit { color: red; }");
            if(ui->language->isChecked())
            {
                ui->pyrolineEdit->setText("HE ГО");
            }
            else
            {
                ui->pyrolineEdit->setText("NO GO");
            }
        });
    }

    //DSO set
    QString status=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
    if(status=="OPEN")
    {
        instr_obj->screenClear();
        instr_obj->connectionClose();
    }

    QMetaObject::invokeMethod(this, [this]() {
        if(ui->language->isChecked())
        {
             ui->progressLabel->setText("Проверка прогресса PYRO -> ");
        }
        else
        {
            ui->progressLabel->setText("PYRO Check Progress -> ");
        }
        ui->progressBar->setValue(100);
    });
}

void MainWindow::pneumaticCheck()
{
    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса PNEUMATIC -> ");
    }
    else
    {
        ui->progressLabel->setText("PNEUMATIC Check Progress -> ");
    }

    ui->progressBar->setValue(0);
    //});

    pneumaticCheckStatus=false;
    double volt=0;

    if(lt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PNEUMATIC_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        volt=driver_obj->analogIN(globalVariables["ap323_0"].toInt(),globalVariables["PNEUMATIC_AI_CHANNEL"].toInt());
        driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["PNEUMATIC_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }
    else if(rt_missile_flag)
    {
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PNEUMATIC_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        volt=driver_obj->analogIN(globalVariables["ap323_1"].toInt(),globalVariables["PNEUMATIC_AI_CHANNEL"].toInt());
        driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["PNEUMATIC_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
    }

    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса PNEUMATIC -> ");
    }
    else
    {
        ui->progressLabel->setText("PNEUMATIC Check Progress -> ");
    }
        ui->progressBar->setValue(50);
    //});

    if(volt==99)
    {
        volt=-1;
    }
    else
    {
        volt *= globalVariables["PNEUMATIC_VOLT_MULTIPLIER"].toDouble();
    }

    if(volt>300)
    {
        QMessageBox::warning(this,"Critical Warning","Failed to get Analog IN card instance!! <br> <br> *Refer Operation Manual for further Assistance!!!");
        on_home_clicked();
        exit(0);
    }

    //if all are fired then pyro pass
    if ((volt >= globalVariables["PNEUMATIC_MIN"].toDouble() && volt <= globalVariables["PNEUMATIC_MAX"].toDouble()))
    {
        airPressure=QString::number(volt);
        pneumaticCheckStatus=true;
        //QMetaObject::invokeMethod(this, [this]() {
        ui->pneumaticlineEdit->setStyleSheet("QLineEdit { color: green;}");
        if(ui->language->isChecked())
        {
            ui->lineEdit_AirPressure->setText("GO");
            //ui->pneumaticlineEdit->setText("ГО");
        }
        else
        {
            ui->lineEdit_AirPressure->setText("GO");
            //ui->pneumaticlineEdit->setText("GO");
        }

        ui->lineEdit_AirPressure_Volt->setText(airPressure);
        ui->lineEdit_AirPressure->setStyleSheet("QPushButton { background-color: green; color: white; }");
        //});
    }
    else
    {
        airPressure=QString::number(volt);
        pneumaticCheckStatus=false;
        //QMetaObject::invokeMethod(this, [this, &volt]() {
        ui->pneumaticlineEdit->setStyleSheet("QLineEdit { color: red;}");
        if(ui->language->isChecked())
        {
            ui->lineEdit_AirPressure->setText("NO GO");
            //ui->pneumaticlineEdit->setText("HE ГО");
        }
        else
        {
            ui->lineEdit_AirPressure->setText("NO GO");
            //ui->pneumaticlineEdit->setText("NO GO");
        }

        if(volt>300)
        {
            ui->lineEdit_AirPressure_Volt->setText("NO GO");
        }
        else
        {
            ui->lineEdit_AirPressure_Volt->setText(airPressure);
        }
        ui->lineEdit_AirPressure->setStyleSheet("QPushButton { background-color: red; color: white; }");
        //});

    }

    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса PNEUMATIC -> ");
    }
    else
    {
        ui->progressLabel->setText("PNEUMATIC Check Progress -> ");
    }
        ui->progressBar->setValue(100);
    //});
}

void MainWindow::earth2NeutralCheck()
{
    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса G2N -> ");
    }
    else
    {
        ui->progressLabel->setText("G2N Check Progress -> ");
    }

        ui->progressBar->setValue(0);
    //});

    gnd2neutralStatus=false;
    double spikeVal=0;

    //DSO set
    QString status=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
    if(status=="OPEN")
    {
        spikeVal=instr_obj->earth2Neutral();
    }
    //check for range and set bool if true
    if(spikeVal<globalVariables["G2N_VOLT_MAX"].toInt())
    {
        gnd2neutralStatus=true;
        g2nVolt=QString::number(spikeVal);
        //QMetaObject::invokeMethod(this, [this]() {
        ui->g2nVolt->setText(g2nVolt);
        if(ui->language->isChecked())
        {
            ui->g2nVoltStatus->setText("ГО");
        }
        else
        {
            ui->g2nVoltStatus->setText("GO");
        }

        ui->g2nVoltStatus->setStyleSheet("QPushButton { background-color: green; color: white; }");
       //});
    }
    else
    {
            g2nVolt=QString::number(spikeVal);
            gnd2neutralStatus=false;
            //QMetaObject::invokeMethod(this, [this]() {
            ui->g2nVolt->setText(g2nVolt);
            if(ui->language->isChecked())
            {
                ui->g2nVoltStatus->setText("HE ГО");
            }
            else
            {
                ui->g2nVoltStatus->setText("NO GO");
            }

            ui->g2nVoltStatus->setStyleSheet("QPushButton { background-color: red; color: white; }");
            //});

    }

    sleep(1);

    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса G2N -> ");
    }
    else
    {
        ui->progressLabel->setText("G2N Check Progress -> ");
    }
        ui->progressBar->setValue(100);
    //});
}

void MainWindow::earth2NeutralCheckDVM()
{
    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса G2N -> ");
    }
    else
    {
        ui->progressLabel->setText("G2N Check Progress -> ");
    }

        ui->progressBar->setValue(0);
    //});

    gnd2neutralStatus=false;
    double spikeVal=0;

    //DSO set
    QString status=instr_obj->connectionOpen(globalVariables["DSO_VISA_ADDRESS"]);
    if(status=="OPEN")
    {
        spikeVal=instr_obj->earth2NeutralDVM();
    }

    //check for range and set bool if true
    if(spikeVal<globalVariables["G2N_VOLT_MAX"].toInt())
    {
        gnd2neutralStatus=true;
        g2nVolt=QString::number(spikeVal);
        //QMetaObject::invokeMethod(this, [this]() {
        ui->g2nVolt->setText(g2nVolt);
        if(ui->language->isChecked())
        {
            ui->g2nVoltStatus->setText("GO");
        }
        else
        {
            ui->g2nVoltStatus->setText("GO");
        }

        ui->g2nVoltStatus->setStyleSheet("QPushButton { background-color: green; color: white; }");
       //});
    }
    else
    {
            g2nVolt=QString::number(spikeVal);
            gnd2neutralStatus=false;
            //QMetaObject::invokeMethod(this, [this]() {
            ui->g2nVolt->setText(g2nVolt);
            if(ui->language->isChecked())
            {
                ui->g2nVoltStatus->setText("NO GO");
            }
            else
            {
                ui->g2nVoltStatus->setText("NO GO");
            }

            ui->g2nVoltStatus->setStyleSheet("QPushButton { background-color: red; color: white; }");
            //});

    }

    sleep(1);

    //QMetaObject::invokeMethod(this, [this]() {
    if(ui->language->isChecked())
    {
         ui->progressLabel->setText("Проверка прогресса G2N -> ");
    }
    else
    {
        ui->progressLabel->setText("G2N Check Progress -> ");
    }
        ui->progressBar->setValue(100);
    //});
}

void MainWindow::reset()
{
    ui->AP323lineEdit->clear();
    ui->AP441lineEdit->clear();
    ui->AP445lineEdit->clear();

    ui->credentialslineEdit->clear();
    ui->limitlineEdit->clear();
    ui->appLoglineEdit->clear();
    //ui->userLoglineEdit->clear();

    ui->g2nVolt->clear();
    ui->lineEdit_AirPressure->clear();

    ui->mpslineEdit->clear();
    ui->statuslineEdit->clear();
    ui->obplineEdit->clear();
    ui->sculineEdit->clear();
    ui->rpflineEdit->clear();
    ui->pyrolineEdit->clear();
    ui->pneumaticlineEdit->clear();
    if(instrumentsStatus)
    {
        ui->testEquiplineEdit->clear();
    }

    ui->extPSClineEdit->clear();
    ui->extPSVlineEdit->clear();
    ui->statusPSVlineEdit->clear();
    ui->intPSVlineEdit->clear();
    ui->pyroPSVlineEdit->clear();
    ui->skrPSVlineEdit->clear();

    ui->RMTextPSVlineEdit->clear();
    ui->RMTstatusPSVlineEdit->clear();
    ui->RMTintPSVlineEdit->clear();
    ui->RMTpyroPSVlineEdit->clear();
    ui->RMTskrPSVlineEdit->clear();

    ui->LOADtmPSClineEdit->clear();
    ui->LOADobpPSClineEdit->clear();
    ui->LOADscuPSClineEdit->clear();
    ui->LOADcguPSClineEdit->clear();
    ui->LOADrpfPSClineEdit->clear();
    ui->LOADskrHeaterPSClineEdit->clear();
    ui->LOADskrSpmPSClineEdit->clear();

    ui->LOADtmPSVlineEdit->clear();
    ui->LOADobpPSVlineEdit->clear();
    ui->LOADscuPSVlineEdit->clear();
    ui->LOADcguPSVlineEdit->clear();
    ui->LOADrpfPSVlineEdit->clear();
    ui->LOADskrHeaterPSVlineEdit->clear();
    ui->LOADskrSpmPSVlineEdit->clear();

    ui->LOADRMTtmPSVlineEdit->clear();
    ui->LOADRMTobpPSVlineEdit->clear();
    ui->LOADRMTscuPSVlineEdit->clear();
    ui->LOADRMTcguPSVlineEdit->clear();
    ui->LOADRMTrpfPSVlineEdit->clear();
    ui->LOADRMTskrHeaterPSVlineEdit->clear();
    ui->LOADRMTskrSpmPSVlineEdit->clear();

    ui->sadamRelayStatusLineEdit->clear();
    ui->nozzleMatingVoltageLineEdit->clear();
    ui->sustainerFireVoltageLineEdit->clear();
    ui->neRelayStatusLineEdit->clear();
    ui->umbilical1StatusLineEdit->clear();
    ui->umbilical2StatusLineEdit->clear();
    ui->misTMStatusLineEdit->clear();
    ui->seekerSTBRelayStatusLineEdit->clear();
    ui->seekerMISStatusLineEdit->clear();

    ui->OBPtextBrowser->clear();

    ui->cageLineEdit->clear();

    ui->amplitudeLineEdit->clear();
    ui->widthLineEdit->clear();

    ui->gndRelay1LineEdit->clear();
    ui->airBottleSquibFireRelayLineEdit->clear();
    ui->thermalBatterySquibFireRelayLineEdit->clear();
    ui->gndRelay2LineEdit->clear();
    ui->boosterPyroRelayStatusLineEdit->clear();
    ui->groundPyroRelayStatusLineEdit->clear();

    //reset measurment variables
    g2nVolt = "--";
    airPressure = "--";

    extPSCurr = "--";
    extPSVolt = "--";
    statusPSVolt = "--";
    intPSVolt = "--";
    pyroPSVolt = "--";
    skrPSVolt = "--";

    rmtExtPSVolt = "--";
    rmtStatusPSVolt = "--";
    rmtIntPSVolt = "--";
    rmtPyroPSVolt = "--";
    rmtSkrPSVolt = "--";

    loadTmPSCurr = "--";
    loadObpPSCurr = "--";
    loadScuPSCurr = "--";
    loadCguPSCurr = "--";
    loadRpfPSCurr = "--";
    loadSkrHeaterPSCurr = "--";
    loadSkrSpmPSCurr = "--";

    loadTmPSVolt = "--";
    loadObpPSVolt = "--";
    loadScuPSVolt = "--";
    loadCguPSVolt = "--";
    loadRpfPSVolt = "--";
    loadSkrHeaterPSVolt = "--";
    loadSkrSpmPSVolt = "--";

    loadRmtTmPSVolt = "--";
    loadRmtObpPSVolt = "--";
    loadRmtScuPSVolt = "--";
    loadRmtCguPSVolt = "--";
    loadRmtRpfPSVolt = "--";
    loadRmtSkrHeaterPSVolt = "--";
    loadRmtSkrSpmPSVolt = "--";
    loadSkrTxPSCurr = "--";
    loadSkrTxPSVolt= "--";
    loadRmtSkrTxPSVolt= "--";

    sadamRelayStatus = "--";
    nozzleMatingVoltage = "--";
    sustainerFireVoltage ="--";
    neRelayStatus = "--";
    umbilical1Status = "--";
    umbilical2Status = "--";
    misTMStatus = "--";
    seekerSTBRelayStatus = "--";
    seekerMISStatus = "--";

    obpStatus="--";

    cageStatus = "--";

    firingPulseStatus = "--";
    firingPulseTMStatus = "--";
    firPulseAmplitude = "--";
    firPulseWidth = "--";

    groundRelay1 = "--";
    airBottleSquibFireRelayStatus = "--";
    thermalBatterySquibFireRelayStatus = "--";
    groundRelay2 = "--";
    boosterPyroRelayStatus = "--";
    groundPyroRelayStatus = "--";

    bytes_recv=0;

    ui->mpsStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->statusStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->obpStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->scuStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->rpfStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->pyroStartPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");

    ui->startMPSPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->startStatusPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->startOBPPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->startSCUPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->startRPFPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->startPYROPushButton->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");

    ui->leftMissile->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");
    ui->rightMissile->setStyleSheet("QPushButton { background-color: rgb(154, 153, 150); color: black; }");

    ui->g2nVolt->setStyleSheet("QPushButton { background-color: rgb(222, 221, 218); color: black; }");
    ui->lineEdit_AirPressure->setStyleSheet("QPushButton { background-color: rgb(222, 221, 218); color: black; }");
    ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: rgb(222, 221, 218); color: black; }");

    ui->progressBar->setValue(0);

    // mpsRunningStatus=false;
    // statusRunningStatus=false;
    // obpRunningStatus=false;
    // scuRunningStatus=false;
    // rpfRunningStatus=false;
    // pyroRunningStatus=false;
}

void MainWindow::on_startMPSPushButton_clicked()
{
    QFuture mps=QtConcurrent::run([this]() {
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        mpsRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->startMPSPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        mpsCheck();

        usleep(5000);

        //if(instrumentsStatus)
        {
            mpsRelayOFF();
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(mpsCheckStatus)
            {
                ui->startMPSPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startMPSPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });

        mpsRunningStatus=false;
    }
    });
}


void MainWindow::on_startStatusPushButton_clicked()
{
    QFuture status=QtConcurrent::run([this]() {
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        statusRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->startStatusPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        statusCheck();

        usleep(5000);

        QMetaObject::invokeMethod(this, [this]() {
            if(statusCheckStatus)
            {
                ui->startStatusPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startStatusPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });

        statusRunningStatus=false;
    }
    });
}


void MainWindow::on_startOBPPushButton_clicked()
{
    QFuture obp=QtConcurrent::run([this]() {

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        obpRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->startOBPPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        obpCheck();

        usleep(5000);

        QMetaObject::invokeMethod(this, [this]() {
            if(obpCheckStatus)
            {
                ui->startOBPPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startOBPPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });

        obpRunningStatus=false;
    }
    });
}


void MainWindow::on_startSCUPushButton_clicked()
{
    QFuture scu=QtConcurrent::run([this]() {

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        scuRunningStatus=true;
        QMetaObject::invokeMethod(this, [this]() {
        ui->startSCUPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        scuCheck();

        QMetaObject::invokeMethod(this, [this]() {
            if(scuCheckStatus)
            {
                ui->startSCUPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startSCUPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });

        scuRunningStatus=false;
    }
    });
}


void MainWindow::on_startRPFPushButton_clicked()
{
    QFuture rpf=QtConcurrent::run([this]() {
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        rpfRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->startRPFPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        rpfCheck();

        QMetaObject::invokeMethod(this, [this]() {
            if(rpfCheckStatus)
            {
                ui->startRPFPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startRPFPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });

        rpfRunningStatus=false;
    }
    });
}


void MainWindow::on_startPYROPushButton_clicked()
{
    QFuture pyro=QtConcurrent::run([this]() {

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        });
    }
    else
    {
        pyroRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->startPYROPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        pyroCheck();

        QMetaObject::invokeMethod(this, [this]() {
            if(pyroCheckStatus)
            {
                ui->startPYROPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->startPYROPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            } 
        });

        pyroRunningStatus=false;
    }
    });

}


void MainWindow::on_mpsStartPushButton_clicked()
{
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
          scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }
    }
    else
    {
        mpsRunningStatus=true;
        ui->mpsStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        //QtConcurrent::run([this]() {

        mpsCheck();

        //if(instrumentsStatus)
        {
            mpsRelayOFF();
        }

        QMetaObject::invokeMethod(this, [this]() {
            if(mpsCheckStatus)
            {
                ui->mpsStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
            }
            else
            {
                ui->mpsStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
            }
        });
        //});
        mpsRunningStatus=false;
    }
}


void MainWindow::on_statusStartPushButton_clicked()
{
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }
    }
    else
    {
        statusRunningStatus=true;
        //remove below not in final code
        if(mpsCheckStatus)
        {
            ui->statusStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
            //QtConcurrent::run([this]() {

            statusCheck();

            QMetaObject::invokeMethod(this, [this]() {
                if(statusCheckStatus)
                {
                    ui->statusStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
                }
                else
                {
                    ui->statusStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
                }
            });
            //});
        }
        else
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Проверка MPS не прошла.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "MPS Check Failed.");
            }
        }
        statusRunningStatus=false;
    }
}


void MainWindow::on_obpStartPushButton_clicked()
{
    QFuture obp=QtConcurrent::run([this]() {

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMetaObject::invokeMethod(this, [this]() {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
            }
            else
            {
                QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
            }
        });
    }
    else
    {
        obpRunningStatus=true;

        QMetaObject::invokeMethod(this, [this]() {
        ui->obpStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");
        });

        obpCheck();

        obpRunningStatus=false;
    }
    });
}


void MainWindow::on_scuStartPushButton_clicked()
{
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }
    }
    else
    {
        scuRunningStatus=true;
        //remove below not in final code
        if(obpCheckStatus)
        {
            ui->scuStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");

            //
            //QtConcurrent::run([this]() {

            scuCheck();

            QMetaObject::invokeMethod(this, [this]() {
                if(scuCheckStatus)
                {
                    ui->scuStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
                }
                else
                {
                    ui->scuStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
                }
            });
            //
            //});
        }
        else
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Проверка OBP не прошла.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "OBP Check Failed.");
            }
        }
        scuRunningStatus=false;
    }
}


void MainWindow::on_rpfStartPushButton_clicked()
{
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }
    }
    else
    {
        rpfRunningStatus=true;
        //remove below not in final code
        if(scuCheckStatus)
        {
            ui->rpfStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");

            //QtConcurrent::run([this]() {

            rpfCheck();

            QMetaObject::invokeMethod(this, [this]() {
                if(rpfCheckStatus)
                {
                    ui->rpfStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
                }
                else
                {
                    ui->rpfStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
                }
            });
            //});
        }
        else
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Проверка SCU не прошла.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "SCU Check Failed.");
            }
        }
        rpfRunningStatus=false;
    }
}

void MainWindow::on_pyroStartPushButton_clicked()
{
    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }
    }
    else
    {
        pyroRunningStatus=true;
        //remove below not in final code
        if(rpfCheckStatus)
        {
            ui->pyroStartPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");

            //QtConcurrent::run([this]() {

            pyroCheck();

            QMetaObject::invokeMethod(this, [this]() {
                if(pyroCheckStatus)
                {
                    ui->pyroStartPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
                }
                else
                {
                    ui->pyroStartPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
                }
            });
            //});
        }
        else
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Проверка RPF не прошла.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "RPF Check Failed.");
            }
        }
        pyroRunningStatus=false;
    }
}

void MainWindow::on_generateAdminReport_clicked()
{
    QString html="";

    QTextDocument textDoc1;

    html=generateReport();

    //admin pdf
    QString timestamp = QDateTime::currentDateTime().toString("dd-MM-yyyy_HH-mm-ss");
    QString dirPath = globalVariables["EXE_DIR_PATH"]+"/reports/SimulatorReports/"+loginID;
    QString filePath = dirPath + "/" + timestamp + ".pdf";

    // Check if the directory exists, and create it if it does not
    QDir dir(dirPath);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            QMessageBox::warning(this,"Error","Failed to create directory:"+dirPath);
            return;
        }
    }

    filePath=convertRelativeToAbsolute(filePath);

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(nullptr, "Decision", "Do you want to save generated report?",
                                      QMessageBox::Yes | QMessageBox::No);

    if (reply == QMessageBox::Yes)
    {
        QPdfWriter pdfWriter1(filePath);
        pdfWriter1.setPageSize(QPageSize(QPageSize::A4));
        pdfWriter1.setResolution(300);

        textDoc1.setHtml(html);

        // Print the QTextDocument to the PDF writer
        textDoc1.print(&pdfWriter1);

        // Open the generated PDF with the default viewer
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(filePath))) {
            qWarning() << "Failed to open PDF viewer for:" << filePath;
        }

    }
    if (reply == QMessageBox::Yes)
    {
        QPrinter printer;
        printer.setPageSize(QPageSize(QPageSize::A4));  // Set the page size
        printer.setResolution(300);  // Set the resolution (same as for PDF)

        // Show the print dialog to select a printer
        QPrintDialog printDialog(&printer, nullptr);
        if (printDialog.exec() == QDialog::Accepted)
        {
            // Print the document to the selected printer
            textDoc1.print(&printer);
        } else
        {
            //qWarning() << "Printing cancelled";
        }
    }
}

QString MainWindow::convertRelativeToAbsolute(const QString &relativePath)
{
    QFileInfo fileInfo(relativePath);
    // Resolves the absolute path from the relative path
    QString absolutePath = fileInfo.absoluteFilePath();
    return absolutePath;
}

void MainWindow::on_autoSimulationPushButton_clicked()
{
     ui->testEquipLabel->setText("TEST EQUIPMENT Check without SELF TEST");
    ui->testEquipLabel->setStyleSheet("QLabel {background-color:rgb(154, 153, 150); color: black;}");
    ui->selfTestLabel->setText("");
    ui->testEquiplineEdit->setText("");

    if(ui->language->isChecked())
    {
         ui->status->setText("Статус работоспособности MSMC: ");
    }
    else
    {
        ui->status->setText(system+" Health Status: ");
    }

    ui->status->setStyleSheet("QLabel { color: yellow; }");
    ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color:rgb(222, 221, 218); color: white; }");

    ui->mpslineEdit->setText("");
    ui->statuslineEdit->setText("");
    ui->obplineEdit->setText("");
    ui->sculineEdit->setText("");
    ui->rpflineEdit->setText("");
    ui->pyrolineEdit->setText("");

    ui->mpsStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");
    ui->statusStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");
    ui->obpStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");
    ui->scuStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");
    ui->rpfStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");
    ui->pyroStartPushButton->setStyleSheet("QPushButton {background-color:rgb(154, 153, 150); color: black;}");

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
    }
    else
    {
        if(ui->language->isChecked())
        {
             ui->status->setText("Статус работоспособности MSMC: Запущено!");
        }
        else
        {
            ui->status->setText(system+" Health Status: Running!");
        }

        ui->status->setStyleSheet("QLabel { color: orange; }");
        ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: orange; color: white; }");

        emit mpsAutoSignal();
    }
}


void MainWindow::on_pushButton_clicked()
{
    QMessageBox::StandardButton reply;
    if(ui->language->isChecked())
    {
        reply = QMessageBox::warning(this, "Операция продувки выбрана", "Убедитесь, что клапан давления надёжно закреплён, и нажмите OK.", QMessageBox::Ok | QMessageBox::Cancel);

    }
    else
    {
        reply = QMessageBox::warning(this, "Air Blow Operation Selected", "Ensure pressure valve is tightly attached/caught and click on OK.", QMessageBox::Ok | QMessageBox::Cancel);

    }

    if (reply == QMessageBox::Ok)
    {
        if(lt_missile_flag)
        {
            driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SOLENOID_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
            sleep(3);
            driver_obj->optoDO(globalVariables["ap445_2"].toInt(),globalVariables["SOLENOID_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        }
        else if(rt_missile_flag)
        {
            driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SOLENOID_RELAY_CHANNEL"].toInt(),globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
            sleep(3);
            driver_obj->optoDO(globalVariables["ap445_3"].toInt(),globalVariables["SOLENOID_RELAY_CHANNEL"].toInt(),globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        }
    }
}


void MainWindow::on_mpsAutoSignal()
{
    on_mpsStartPushButton_clicked();

    sleep(1);

    if(mpsCheckStatus && instrumentsStatus)
    {
        emit statusAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }
    }
}

void MainWindow::on_statusAutoSignal()
{
    on_statusStartPushButton_clicked();

    sleep(1);

    if(mpsCheckStatus && statusCheckStatus && instrumentsStatus)
    {
        emit obpAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }
    }
}

void MainWindow::on_obpAutoSignal()
{
    QEventLoop loop;
    connect(serialcomm_obj, &SerialCOMM::responseReady, &loop, &QEventLoop::quit);
    on_obpStartPushButton_clicked();
    sleep(1);
    loop.exec();  // waits until responseReady emitted

    if(mpsCheckStatus && statusCheckStatus && obpCheckStatus && instrumentsStatus)
    {
        emit scuAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }
    }
}

void MainWindow::on_scuAutoSignal()
{
    on_scuStartPushButton_clicked();

    sleep(1);

    if(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && instrumentsStatus)
    {
        emit rpfAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }
    }
}

void MainWindow::on_rpfAutoSignal()
{
    on_rpfStartPushButton_clicked();

    sleep(1);

    if(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && instrumentsStatus)
    {
        emit pyroAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }
    }
}

void MainWindow::on_pyroAutoSignal()
{
    on_pyroStartPushButton_clicked();

    sleep(1);

    if(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus  && pyroCheckStatus && instrumentsStatus)
    {
        ui->pneumaticLabel->setStyleSheet("QLabel { background-color: orange; color: white; }");
        if(pneumaticCheckStatus)
        {
            if(ui->language->isChecked())
            {
                //ui->lineEdit_AirPressure->setText("ГО");
                ui->pneumaticlineEdit->setText("ГО");
            }
            else
            {
                //ui->lineEdit_AirPressure->setText("GO");
                ui->pneumaticlineEdit->setText("GO");
            }
        }
        else
        {
            if(ui->language->isChecked())
            {
                //ui->lineEdit_AirPressure->setText("HE ГО");
                ui->pneumaticlineEdit->setText("HE ГО");
            }
            else
            {
                //ui->lineEdit_AirPressure->setText("NO GO");
                ui->pneumaticlineEdit->setText("NO GO");
            }
        }
        ui->pneumaticLabel->setStyleSheet("QLabel {background-color:rgb(154, 153, 150); color: black;}");
        emit selfTestAutoSignal();
    }
    else
    {
        if(!(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus))
        {
            if(ui->language->isChecked())
            {
                 ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
            }
            else
            {
                ui->status->setText(system+" Health Status: NO GO");
            }

            ui->status->setStyleSheet("QLabel { color: red; }");
            ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
        }

    }
}

void MainWindow::on_selfTestAutoSignal()
{
    ui->statusbar->showMessage("Wating for User Selection...", 2000);

    // Show spinner
    ui->loadLabel->setVisible(true);
    if (loadingMovie) loadingMovie->start();

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    //QMetaObject::invokeMethod(this, [this]()
    //{
     ui->progressLabel->setText("SELF TEST Check Progress->");
     ui->browser->append("Instruments SELF TEST Check Initiated!!!");
     ui->progressBar->setValue(0);
     ui->testEquipLabel->setStyleSheet("QLabel { background-color: orange; color: white; }");
    //});

    QMessageBox counterMsg;
    counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint & Qt::FramelessWindowHint);
    counterMsg.setWindowTitle("Test Equipment Self Test CHECK!!");
    counterMsg.setText("Do you wan to run Self Test of all Test Instruments? <br> <br> *SELF TEST Check is optional and takes around additional 3 minutes!!");
    counterMsg.setIcon(QMessageBox::Question);
    counterMsg.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
    int ret=counterMsg.exec();

    if(ret==QMessageBox::Ok)
    {
        selfTest=true;
        ui->testEquipLabel->setText("TEST EQUIPMENT Check with SELF TEST");
        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

         ui->statusbar->showMessage("Self Test Operation In-Progress...");
        on_selfTestStart();

         ui->statusbar->showMessage("Self Test Operation Completed...", 2000);
        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        if(instr_obj->failedSelfTestInstrument!="")
        {
            ui->testEquiplineEdit->setStyleSheet("QLineEdit { color: red;}");
            instrumentsSelfTest=true;
            ui->testEquiplineEdit->setText("NO GO");
            ui->progressLabel->setText("SELF TEST Check Failed!!!");
            ui->browser->append("SELF TEST Check Failed!!!");
            ui->selfTestLabel->setText("Instruments SELF TEST Check Failed!!!");
            ui->progressBar->setValue(100);
            ui->testEquipLabel->setStyleSheet("QLabel {background-color:red; color: white;}");
            instrumentsStatus=false;
        }
        else
        {
            if(instrumentsStatus)
            {
             ui->testEquiplineEdit->setStyleSheet("QLineEdit { color: green;}");
             ui->testEquiplineEdit->setText("GO");
            }
            else
            {
             ui->testEquiplineEdit->setStyleSheet("QLineEdit { color: red;}");
             ui->testEquiplineEdit->setText("NO GO");
            }
            ui->progressLabel->setText("SELF TEST Check Completed!!!");
            ui->browser->append("SELF TEST Check Completed!!!");
            ui->selfTestLabel->setText("Instruments SELF TEST Check Completed!!!");
            ui->progressBar->setValue(100);
            ui->testEquipLabel->setStyleSheet("QLabel {background-color:green; color: white;}");
        }
        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    else
    {
        selfTest=false;
         ui->testEquipLabel->setText("TEST EQUIPMENT Check without SELF TEST");
        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

        instrumentsSelfTest=false;
        //bypassed
        if(instrumentsStatus)
        {
         ui->testEquiplineEdit->setStyleSheet("QLineEdit { color: green;}");
         ui->testEquiplineEdit->setText("GO");
        }
        else
        {
         ui->testEquiplineEdit->setStyleSheet("QLineEdit { color: red;}");
         ui->testEquiplineEdit->setText("NO GO");
        }
        ui->progressLabel->setText("SELF TEST Check BY PASSED!!!");
        ui->browser->append("SELF TEST Check BY PASSED!!!");
        ui->selfTestLabel->setText("Instruments SELF TEST Check BY PASSED!!!");
        ui->progressBar->setValue(100);
        ui->testEquipLabel->setStyleSheet("QLabel {background-color:rgb(154, 153, 150); color: black;}");
        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    sleep(1);

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(mpsCheckStatus && statusCheckStatus && obpCheckStatus && scuCheckStatus && rpfCheckStatus && pyroCheckStatus && pneumaticCheckStatus && instrumentsStatus)
    {
        if(ui->language->isChecked())
        {
             ui->status->setText("Статус работоспособности MSMC: ГО");
        }
        else
        {
            ui->status->setText(system+" Health Status: GO");
        }

        ui->status->setStyleSheet("QLabel { color: green; }");
        ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: green; color: white; }");
    }
    else
    {
        if(ui->language->isChecked())
        {
             ui->status->setText("Статус работоспособности MSMC: НЕ ГО");
        }
        else
        {
            ui->status->setText(system+" Health Status: NO GO");
        }

        ui->status->setStyleSheet("QLabel { color: red; }");
        ui->autoSimulationPushButton->setStyleSheet("QPushButton { background-color: red; color: white; }");
    }

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    // After heavy operations finish:
    if (loadingMovie && loadingMovie->state()==QMovie::Running) loadingMovie->stop();
    ui->loadLabel->setVisible(false);
}

void MainWindow::on_selfTestStart()
{
    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(mpsRunningStatus && statusRunningStatus && obpRunningStatus &&
        scuRunningStatus && rpfRunningStatus && pyroRunningStatus)
    {
        if(ui->language->isChecked())
        {
             QMessageBox::warning(this, "Ошибка", "Экземпляр модуля уже запущен!!");
        }
        else
        {
            QMessageBox::warning(this, "Error", "A Module Instance is already Running!!");
        }

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    else
    {
        selfTestRunningStatus=true;
        //remove below not in final code
        if(pyroCheckStatus | (loginSelection == globalVariables["ADMIN"].toInt()))
        {
            ui->testEquipLabel->setStyleSheet("QLabel { background-color: orange; color: white; }");
            selfTestProgess=0;

            // Give event loop one iteration so spinner is painted
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

            instr_obj->selfTest();

            // Give event loop one iteration so spinner is painted
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

            QMetaObject::invokeMethod(this, [this]() {
                if(instrumentsSelfTest)
                {
                    ui->testEquipLabel->setStyleSheet("QLabel { background-color: green; color: white; }");
                }
                else
                {
                    ui->testEquipLabel->setStyleSheet("QLabel { background-color: red; color: white; }");
                }
            });
            //});

            // Give event loop one iteration so spinner is painted
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
        }
        else
        {
            if(ui->language->isChecked())
            {
                 QMessageBox::warning(this, "Ошибка", "Проверка pyro не прошла.");
            }
            else
            {
                QMessageBox::warning(this, "Error", "pyro Check Failed.");
            }
        }
        selfTestRunningStatus=false;

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
}


void MainWindow::selfTestSignal(QString instr,bool status)
{
    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);

    if(status)
    {
        ui->progressLabel->setText(instr+" SELF TEST in Progress!!!");
        ui->browser->append("<br>"+instr+" SELF TEST Initiated!!!");
        ui->selfTestLabel->setText(instr+" SELF TEST in Progress!!!");

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    else
    {
        ui->progressLabel->setText(instr+" SELF TEST Completed!!!");
        ui->browser->append(instr+" SELF TEST Completed!!!");
        ui->selfTestLabel->setText(instr+" SELF TEST Completed!!!");
        selfTestProgess+=20;
        ui->progressBar->setValue(selfTestProgess);

        // Give event loop one iteration so spinner is painted
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    // Give event loop one iteration so spinner is painted
    QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
}

void MainWindow::on_selfTest_clicked()
{
    selfTest=true;
    mwOFF();

    //instrumentsSelfTest=true;

    ui->browser->clear();
    ui->progressBar->setValue(selfTestProgess);
    ui->stackedWidget->setCurrentIndex(globalVariables["BROWSER_PAGE"].toInt());
    //QFuture selfTest=QtConcurrent::run([this]()
    //{
        on_selfTestAutoSignal();
    //});

    ui->selfTestLabel->setText("");
    //ui->browser->clear();
}


void MainWindow::on_mwTest_clicked()
{
    ui->stackedWidget->setCurrentIndex(globalVariables["MW_PAGE"].toInt());
}

void MainWindow::on_sideSlider_valueChanged(int value)
{
    if(value)
    {
        ui->leftLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->rightLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //left
        mwRelayCheck(1,16,false);
        mwRelayCheck(1,17,false);
        mwRelayCheck(1,18,false);
        mwRelayCheck(1,19,false);
        //seeker
        mwRelayCheck(1,26,false);
        mwRelayCheck(1,27,false);
        mwRelayCheck(1,28,false);

        sleep(1);

        //right
        mwRelayCheck(1,20,true);
        mwRelayCheck(1,21,true);
        mwRelayCheck(1,22,true);
        mwRelayCheck(1,23,true);
        //seeker
        mwRelayCheck(1,30,true);
        mwRelayCheck(1,31,true);
        mwRelayCheck(1,29,true);
    }
    else
    {
        ui->rightLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->leftLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //right
        mwRelayCheck(1,20,false);
        mwRelayCheck(1,21,false);
        mwRelayCheck(1,22,false);
        mwRelayCheck(1,23,false);
        //seeker
        mwRelayCheck(1,30,false);
        mwRelayCheck(1,31,false);
        mwRelayCheck(1,29,false);

        sleep(1);

        //left
        mwRelayCheck(1,16,true);
        mwRelayCheck(1,17,true);
        mwRelayCheck(1,18,true);
        mwRelayCheck(1,19,true);
        //seeker
        mwRelayCheck(1,26,true);
        mwRelayCheck(1,27,true);
        mwRelayCheck(1,28,true);
    }

    on_txrxSlider_valueChanged(ui->txrxSlider->value());
}


void MainWindow::on_txrxSlider_valueChanged(int value)
{
    if(value)
    {
        ui->tx1rx1Label->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->tx2rx2Label->setStyleSheet("QLabel { background-color: green; color: white; }");

        mwRelayCheck(1,0,false);
        mwRelayCheck(1,1,false);
        mwRelayCheck(1,2,false);
        mwRelayCheck(1,3,false);
        mwRelayCheck(1,4,false);
        mwRelayCheck(1,5,false);
        mwRelayCheck(1,6,false);
        mwRelayCheck(1,7,false);

        mwRelayCheck(1,8,true);
        mwRelayCheck(1,9,true);
        mwRelayCheck(1,10,true);
        mwRelayCheck(1,11,true);
        mwRelayCheck(1,12,true);
        mwRelayCheck(1,13,true);
        mwRelayCheck(1,14,true);
        mwRelayCheck(1,15,true);

    }
    else
    {
        ui->tx2rx2Label->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->tx1rx1Label->setStyleSheet("QLabel { background-color: green; color: white; }");

        mwRelayCheck(1,8,false);
        mwRelayCheck(1,9,false);
        mwRelayCheck(1,10,false);
        mwRelayCheck(1,11,false);
        mwRelayCheck(1,12,false);
        mwRelayCheck(1,13,false);
        mwRelayCheck(1,14,false);
        mwRelayCheck(1,15,false);

        mwRelayCheck(1,0,true);
        mwRelayCheck(1,1,true);
        mwRelayCheck(1,2,true);
        mwRelayCheck(1,3,true);
        mwRelayCheck(1,4,true);
        mwRelayCheck(1,5,true);
        mwRelayCheck(1,6,true);
        mwRelayCheck(1,7,true);

    }
}


void MainWindow::on_monSimSlider_valueChanged(int value)
{
    //sleep(1);

    if(value)
    {
        ui->monitorLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->simLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //monitoring
        mwRelayCheck(1,24,false);
        mwRelayCheck(1,25,true);
    }
    else
    {
        ui->simLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->monitorLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //simulator
        mwRelayCheck(1,25,false);
        mwRelayCheck(1,24,true);
    }
}


void MainWindow::on_relaySlider_valueChanged(int value)
{
    if(value)
    {
        ui->relayOFFLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->relayONLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //relay on
        mwRelayCheck(2,9,true);
    }
    else
    {
        ui->relayONLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
        ui->relayOFFLabel->setStyleSheet("QLabel { background-color: green; color: white; }");

        //relay off
        mwRelayCheck(2,9,false);
    }
}


void MainWindow::mwInIt()
{
    ui->relayONLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
    ui->relayOFFLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");

    ui->simLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
    ui->monitorLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");

    ui->tx2rx2Label->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
    ui->tx1rx1Label->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");

    ui->rightLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");
    ui->leftLabel->setStyleSheet("QLabel { background-color: rgb(222, 221, 218); color: black; }");

    ui->sideSlider->setValue(0);
    ui->monSimSlider->setValue(0);
    ui->relaySlider->setValue(0);
    ui->txrxSlider->setValue(0);
}

void MainWindow::mwOFF()
{
    ui->mwBrowser->clear();
    ui->sideSlider->setValue(0);
    ui->monSimSlider->setValue(0);
    ui->relaySlider->setValue(0);
    ui->txrxSlider->setValue(0);

    mwRelayCheck(2,8,false);

    mwRelayCheck(1,0,false);
    mwRelayCheck(1,1,false);
    mwRelayCheck(1,2,false);
    mwRelayCheck(1,3,false);

    mwRelayCheck(1,4,false);
    mwRelayCheck(1,5,false);
    mwRelayCheck(1,6,false);
    mwRelayCheck(1,7,false);

    mwRelayCheck(1,8,false);
    mwRelayCheck(1,9,false);
    mwRelayCheck(1,10,false);
    mwRelayCheck(1,11,false);

    mwRelayCheck(1,12,false);
    mwRelayCheck(1,13,false);
    mwRelayCheck(1,14,false);
    mwRelayCheck(1,15,false);

    mwRelayCheck(1,16,false);
    mwRelayCheck(1,17,false);
    mwRelayCheck(1,18,false);
    mwRelayCheck(1,19,false);

    mwRelayCheck(1,20,false);
    mwRelayCheck(1,21,false);
    mwRelayCheck(1,22,false);
    mwRelayCheck(1,23,false);

    mwRelayCheck(1,24,false);
    mwRelayCheck(1,25,false);

    mwRelayCheck(1,26,false);
    mwRelayCheck(1,27,false);
    mwRelayCheck(1,28,false);
    mwRelayCheck(1,29,false);
    mwRelayCheck(1,30,false);
    mwRelayCheck(1,31,false);
}

void MainWindow::mwRelayCheck(int card,int channel, bool status)
{
    QString sts="";
    card-=1;
    if(status)
    {
        driver_obj->optoDO(card,channel,globalVariables["HIGH"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        sts="ON";
    }
    else
    {
        driver_obj->optoDO(card,channel,globalVariables["LOW"].toInt(),globalVariables["DO_WRITE_BINARY_MODE"].toInt());
        sts="OFF";
    }
    QString str="Relay "+sts+" -> DO"+QString::number(card)+" , Channel -> "+QString::number(channel);
    ui->mwBrowser->append("<br>");
    ui->mwBrowser->append(str);
}

void MainWindow::on_clearMW_clicked()
{
    ui->mwBrowser->clear();
}


void MainWindow::on_monSimSlider_sliderPressed()
{
    on_monSimSlider_valueChanged(ui->monSimSlider->value());
}

bool MainWindow::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {

        auto *slider = qobject_cast<QSlider *>(obj);
        if (!slider)
            return false;

        auto *mouseEvent = dynamic_cast<QMouseEvent *>(event);

        if (mouseEvent->button() != Qt::LeftButton)
            return false;

        // Get handle rect (style-safe)
        QStyleOptionSlider opt;
        opt.initFrom(slider);
        opt.orientation = slider->orientation();
        opt.minimum = slider->minimum();
        opt.maximum = slider->maximum();
        opt.sliderPosition = slider->value();
        opt.sliderValue = slider->value();

        QRect handle = slider->style()->subControlRect(
            QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, slider);

        // Click on groove → toggle
        if (!handle.contains(mouseEvent->pos())) {
            slider->setValue(slider->value() == 0 ? 1 : 0);
            return true;   // event handled
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

