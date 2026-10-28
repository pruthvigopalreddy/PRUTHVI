#ifndef INSTRCONNECTION_H
#define INSTRCONNECTION_H

#include <QObject>
#include "visatype.h"
#include "drivers.h"

enum class PowerSupplyType
{
    EXT,
    STATUS,
    INT,
    PYRO,
    UNKNOWN
};

class InstrConnection : public QObject
{
    Q_OBJECT    
public:

    struct InstrumentInfo
    {
        QString name;
        QString visa;
        QString model;
        bool isPowerSupply;
        bool selfTest;
    };

    QString failedInstrument="";
    QString failedSelfTestInstrument="";
    QString failedSelfTestInstrumentStatus="";

    QString akash="";
    void systemCheck();

    InstrConnection();
    QString getIPByName(QString &instrumentName);

    ViSession session, resourceManager =VI_NULL;
    ViUInt32 count =0;
    ViChar responseStr[256]={0};
    ViChar responseESRStr[256]={0};
    ViChar responseVAmpStr[256]={0};
    ViChar responsePWidthStr[256]={0};
    ViStatus status = 0;
    QVector<InstrumentInfo> instruments;
    Drivers *driver_obj;

    bool modelMismatch=false;
    bool selfTestCheck=false;
    QString mismatch="";

public slots:
    void verifyList();
    bool verifyPS();
    PowerSupplyType expectedPowerSupply(const QString &name);
    PowerSupplyType detectPowerSupply(double volt);
    bool validatePowerSupply(const QString &name);
    QString psToString(PowerSupplyType ps);
    QString verifyInstrumentConnection(QString VISA_ADDRESS);
    QString verifyInstrumentSelfTestConnection(QString VISA_ADDRESS);
    QString connectionOpen(QString VISA_ADDRESS);
    void connectionClose();
    int disconnectedInstr(QString msg);
    void errorHandler(ViStatus status, ViSession session);
    double pollingQueryCapture(ViSession session, ViConstString query);
    void pollingCmdCapture(ViSession session,ViConstString command);
    void earth2NeutralInIt();
    double earth2Neutral();
    double earth2NeutralDVM();
    void firingPulseLeftInIt();
    void firingPluseCaptureLeft();
    void firingPulseRightInIt();
    void firingPluseCaptureRight();
    void firingPluseClear();
    void screenClear();
    void selfTest();
    void setInstrument(QString Instrument);
    void setInstrumentOutON();
    double readInstrumentVolt();
    double readInstrumentCurrent();
    void setInstrumentOutOFF();


signals:
    void updateInstrStatus(QString msg, QString failedInstrument);
    void selfTestSignal(QString instr,bool status);
};

#endif // INSTRCONNECTION_H
