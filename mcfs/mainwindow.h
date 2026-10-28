#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QByteArray>
#include "crypto.h"
#include <QTreeWidget>
#include <QFileSystemModel>
#include <QDir>
#include <QThread>
#include "loghistory.h"
#include "userhistory.h"
#include "instrconnection.h"
#include "serialcomm.h"
#include "qtranslator.h"
#include "drivers.h"
#include "createlogin.h"
#include <QProcess>
#include <QMovie>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

public:
    QString akash="";
    QString system="";
    QString rdacs_serial="";

    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    void setLanguage(bool checked);
    void setLanguageRussian();
    void setLanguageEnglish();

    void systemCheck();
    void onbyPassToggled(bool checked);
    QString loadExpectedChecksum();
    void showLogin();
    void updateDateTime();
    void updateUser();
    void setUserChanges();

    void home_pressed();

    void pwdChange(QString loginID, int authority);
    void addUser(QString loginID, QString loginPWD, int authority);
    QString decryptLoginFile(QString filePath);

    void enableADMINLogin();
    void enableCMDRLogin();
    void userLogin();
    bool validateUserLogin(bool checkCMDR);
    void closeWindow();
    void on_createLogin_clicked();

    crypto *crypt;
    int authority=0;
    QString loginID="";
    QString loginPWD="";
    QString checksum="";
    int ap323Count = 0;
    int ap441Count = 0;
    int ap445Count = 0;
    bool acromagCheck=false;
    int configFileCount=0;
    bool configFileCheck=false;
    int lt_missile_flag,rt_missile_flag=0;
    QString bytes_recv="--";
    bool ioStatus=false;
    bool instrumentsStatus;
    bool instrumentsSelfTest=false;
    int loginSelection=0;
    QString loginStatus="";

signals:
    void pwdChangeSignal(QString loginID, int authority);

    void mpsAutoSignal();

    void statusAutoSignal();

    void obpAutoSignal();

    void scuAutoSignal();

    void rpfAutoSignal();

    void pyroAutoSignal();

    void selfTestAutoSignal();

private slots:

    void verifyNetworkList();
    void updateInstrStatus(QString msg , QString failedInstrument);
    void encrypt_clicked();
    void decrypt_clicked();
    void populateFileSystem();
    void populateTree(QTreeWidgetItem *parentItem, const QString &path, int levelCount = 0);
    void treeWidget_itemClicked(QTreeWidgetItem *item, int column);
    void logHistoryEntry();
    void userlogEntry(QString action,QString inFile,QString outFile);
    void on_back_clicked();
    void on_forward_clicked();
    void on_reload_clicked();
    void on_clear_clicked();
    void onOpenFile();
    void on_log_clicked();
    void on_pwdChange_clicked();
    void on_addUser_clicked();
    void on_userList_clicked();
    void on_userLog_clicked();
    void on_logout_clicked();
    void on_txtBrowser_clicked();

    std::vector<std::string> checkBinaryFilesExist(const std::string& folderPath, const std::vector<std::string>& fileNames);
    void checkconfigFiles();
    void countAcromagInstances();

    void on_checklistFileSystemPushButton_clicked();
    void on_checklistNetworkPushButton_clicked();
    void on_acceptChecklistPushButton_clicked();
    void on_acceptPowerCheckPushButton_clicked();
    void on_acceptStatusCheckPushButton_clicked();
    void on_acceptOBPCheckPushButton_clicked();
    void on_acceptSCUCheckPushButton_clicked();
    void on_acceptRPFCheckPushButton_clicked();

    void on_leftMissile_clicked();
    void on_rightMissile_clicked();

    void on_reportGenerate_clicked();
    QString generateReport();
    QString encryptReport(QString inputKey);
    QString generateCmdrReport();

    void mpsInIt();
    void mpsCheck();
    void mpsRelayOFF();
    void mpsOutOFF();
    void statusCheck();
    void statusRelayOFF();
    void obpCheck();
    void scuCheck();
    void rpfCheck();
    void pyroCheck();
    void obpRelayOFF();
    void pyroRelayOFF();
    void pneumaticCheck();
    void earth2NeutralCheck();
    void earth2NeutralCheckDVM();

    void reset();

    void on_startMPSPushButton_clicked();

    void on_startStatusPushButton_clicked();

    void on_startOBPPushButton_clicked();

    void on_startSCUPushButton_clicked();

    void on_startRPFPushButton_clicked();

    void on_startPYROPushButton_clicked();

    void on_mpsStartPushButton_clicked();

    void on_statusStartPushButton_clicked();

    void on_obpStartPushButton_clicked();

    void on_scuStartPushButton_clicked();

    void on_rpfStartPushButton_clicked();

    void on_pyroStartPushButton_clicked();

    void on_home_clicked();

    void on_generateAdminReport_clicked();

    QString convertRelativeToAbsolute(const QString &relativePath);

    void on_autoSimulationPushButton_clicked();

    void updateProgressBar(int i);

    void obpCommunication(const QString &msg);

    void onResponse(const QByteArray &response);

    void on_pushButton_clicked();

    void on_ADMINpushButton_clicked();

    void on_CMDRpushButton_clicked();

    void on_mpsAutoSignal();

    void on_statusAutoSignal();

    void on_obpAutoSignal();

    void on_scuAutoSignal();

    void on_rpfAutoSignal();

    void on_pyroAutoSignal();

    void on_selfTestAutoSignal();

    void on_selfTestStart();

    void selfTestSignal(QString instr, bool status);

    void on_selfTest_clicked();

    void on_mwTest_clicked();

    // void on_switch1_clicked();

    // void on_switch2_clicked();

    // void on_switch3_clicked();

    // void on_switch4_clicked();

    void on_sideSlider_valueChanged(int value);

    void on_txrxSlider_valueChanged(int value);

    void on_monSimSlider_valueChanged(int value);

    void on_relaySlider_valueChanged(int value);

    void mwInIt();

    void mwOFF();

    void mwRelayCheck(int card, int channel, bool status);

    void on_clearMW_clicked();

    void on_monSimSlider_sliderPressed();

private:
    bool selfTest=false;
    QString firstFailedInstrument="";
    QMovie *loadingMovie;
    QString pingStatus="";
    Ui::MainWindow *ui;
    QByteArray aesKey;
    QString filePath;
    bool corrupt=false;
    QTreeWidget *treeWidget;
    QTreeWidgetItem *rootItem;
    QByteArray salt;
    CreateLogin *createlogin_obj;
    LogHistory *log_history_obj;
    UserHistory *user_history_obj;
    InstrConnection *instr_obj;
    Drivers *driver_obj;
    QThread *serialThread = nullptr;
    SerialCOMM *serialcomm_obj;
    QThread *instrThread,*driverThread,*serialcommThread;
    QMap<QString, int> instruments;
    bool gnd2neutralStatus=false;
    bool pneumaticCheckStatus=false;
    bool mpsCheckStatus=false;
    bool statusCheckStatus=false;
    bool obpCheckStatus=false;
    bool scuCheckStatus=false;
    bool rpfCheckStatus=false;
    bool pyroCheckStatus=false;
    int selfTestProgess=0;

    QString obpResponse="";

    bool mpsRunningStatus=false;
    bool statusRunningStatus=false;
    bool obpRunningStatus=false;
    bool scuRunningStatus=false;
    bool rpfRunningStatus=false;
    bool pyroRunningStatus=false;
    bool selfTestRunningStatus=false;

    QString g2nVolt = "--";
    QString airPressure = "--";

    QString extPSCurr = "--";
    QString extPSVolt = "--";
    QString statusPSVolt = "--";
    QString intPSVolt = "--";
    QString pyroPSVolt = "--";
    QString skrPSVolt = "--";

    QString rmtExtPSVolt = "--";
    QString rmtStatusPSVolt = "--";
    QString rmtIntPSVolt = "--";
    QString rmtPyroPSVolt = "--";
    QString rmtSkrPSVolt = "--";

    QString loadTmPSCurr = "--";
    QString loadObpPSCurr = "--";
    QString loadScuPSCurr = "--";
    QString loadCguPSCurr = "--";
    QString loadRpfPSCurr = "--";
    QString loadSkrHeaterPSCurr = "--";
    QString loadSkrSpmPSCurr = "--";
    QString loadSkrTxPSCurr = "--";

    QString loadTmPSVolt = "--";
    QString loadObpPSVolt = "--";
    QString loadScuPSVolt = "--";
    QString loadCguPSVolt = "--";
    QString loadRpfPSVolt = "--";
    QString loadSkrHeaterPSVolt = "--";
    QString loadSkrSpmPSVolt = "--";
    QString loadSkrTxPSVolt= "--";

    QString loadRmtTmPSVolt = "--";
    QString loadRmtObpPSVolt = "--";
    QString loadRmtScuPSVolt = "--";
    QString loadRmtCguPSVolt = "--";
    QString loadRmtRpfPSVolt = "--";
    QString loadRmtSkrHeaterPSVolt = "--";
    QString loadRmtSkrSpmPSVolt = "--";
    QString loadRmtSkrTxPSVolt= "--";

    QString sadamRelayStatus = "--";
    QString nozzleMatingVoltage = "--";
    QString sustainerFireVoltage ="--";
    QString neRelayStatus = "--";
    QString umbilical1Status = "--";
    QString umbilical2Status = "--";
    QString misTMStatus = "--";
    QString seekerSTBRelayStatus = "--";
    QString seekerMISStatus = "--";

    QString obpStatus="--";

    QString cageStatus = "--";

    QString firingPulseStatus = "--";
    QString firingPulseTMStatus = "--";
    QString firPulseAmplitude = "--";
    QString firPulseWidth = "--";

    QString groundRelay1 = "--";
    QString airBottleSquibFireRelayStatus = "--";
    QString thermalBatterySquibFireRelayStatus = "--";
    QString groundRelay2 = "--";
    QString boosterPyroRelayStatus = "--";
    QString groundPyroRelayStatus = "--";

    QTimer *timer;
    QTimer *instrTimer;
    bool mpsByPassStatus=false;
    QTranslator *rusTranslator;
};

#endif // MAINWINDOW_H
