#include "instrconnection.h"
#include <QTimer>
#include <QDebug>
#include <QMessageBox>
#include <QObject>
#include <QMapIterator>
#include <QProcess>
#include <cstring>
#include <unistd.h>
#include "visa.h"
#include <QRegularExpression>

extern QMap<QString, QString> globalVariables;

InstrConnection::InstrConnection()
{
    systemCheck();

    driver_obj=new Drivers();

    instruments.push_back({ globalVariables["EXT-PS_HOST_NAME"], globalVariables["EXT-PS_VISA_ADDRESS"], globalVariables["EXT-PS_MODEL"],true, false });
    instruments.push_back({ globalVariables["STATUS-PS_HOST_NAME"], globalVariables["STATUS-PS_VISA_ADDRESS"], globalVariables["STATUS-PS_MODEL"],true, false });
    instruments.push_back({ globalVariables["INT-PS_HOST_NAME"], globalVariables["INT-PS_VISA_ADDRESS"], globalVariables["INT-PS_MODEL"],true, false });
    instruments.push_back({ globalVariables["PYRO-PS_HOST_NAME"], globalVariables["PYRO-PS_VISA_ADDRESS"], globalVariables["PYRO-PS_MODEL"],true, false });
    if(akash=="prime")
    {
        instruments.push_back({ globalVariables["SEEKER-PS_HOST_NAME"], globalVariables["SEEKER-PS_VISA_ADDRESS"], globalVariables["SEEKER-PS_MODEL"],true, false });
    }
    instruments.push_back({ globalVariables["PWR-MTR_HOST_NAME"], globalVariables["PWR-MTR_VISA_ADDRESS"], globalVariables["PWR-MTR_MODEL"],false, false });
    instruments.push_back({ globalVariables["SIG-GEN_HOST_NAME"], globalVariables["SIG-GEN_VISA_ADDRESS"], globalVariables["SIG-GEN_MODEL"],false, false });
    instruments.push_back({ globalVariables["UNI-CTR_HOST_NAME"], globalVariables["UNI-CTR_VISA_ADDRESS"], globalVariables["UNI-CTR_MODEL"],false, false });
    instruments.push_back({ globalVariables["WAVE-GEN_HOST_NAME"], globalVariables["WAVE-GEN_VISA_ADDRESS"], globalVariables["WAVE-GEN_MODEL"],false, false });
    instruments.push_back({ globalVariables["DSO_HOST_NAME"], globalVariables["DSO_VISA_ADDRESS"], globalVariables["DSO_MODEL"],false, false });
}

void InstrConnection::systemCheck()
{
    QString system = globalVariables["SYSTEM"];

    if(system=="MSMC-P/MCF-P")
    {
        akash="prime";
    }
    else if(system=="MSMC-E/MCF-E")
    {
        akash="export";
    }
    else
    {
        akash="export";
    }
}

QString InstrConnection::getIPByName(QString &instrumentName)
{
    for (auto &instr : instruments)
    {
        if (instr.name.compare(instrumentName, Qt::CaseInsensitive) == 0)
        {
            // Extract IP from VISA
            QRegularExpression re(R"(TCPIP\d*::([\d\.]+)::)");
            auto match = re.match(instr.visa);
            return match.hasMatch() ? match.captured(1) : "Unknown IP";
        }
    }
    return "Unknown IP";
}

void InstrConnection::verifyList()
{
    QString msg;
    failedInstrument.clear();
    modelMismatch=false;
    mismatch="";

    for (int i = 0; i < instruments.size(); ++i)
    {
        const auto &instr = instruments[i];

        QString responseStr = verifyInstrumentConnection(instr.visa);

        // ---------- VISA / SESSION FAILURE ----------
        if (responseStr == "MGR NOT OPEN" || responseStr == "SESSION NOT OPEN" ||  responseStr.isEmpty())
        {
            msg.append(instr.name + "," + instr.visa + "," + instr.model + ",Connection FAIL\n");

            failedInstrument = instr.name;

            //qDebug() << instr.visa << " -> " << instr.name << " -> "<< "Connection FAIL";

            break;   // ✅ stop on FIRST failure
        }

        // ---------- MODEL MISMATCH ----------
        if (!responseStr.contains(instr.model, Qt::CaseInsensitive))
        {
            msg.append(instr.name + "," + instr.visa + "," + instr.model + ",Instrument Model Mismatch\n");

            modelMismatch=true;
            QString name=instr.name;
            mismatch="Connected Instrument at "+ getIPByName(name) +" is NOT a Registered "+instr.name;

            //qDebug() << responseStr << " -> Wrong instrument connected for " << instr.name;

            failedInstrument = instr.name;

            break;   // ✅ stop on FIRST failure
        }

        // ---------- PASS ----------
        msg.append(instr.name + "," + instr.visa + "," + instr.model + ",PASS\n");

        //qDebug() << responseStr  << " -> " << instr.name  << " -> "<< "CONNECTED";
    }

    emit updateInstrStatus(msg, failedInstrument);
}

bool InstrConnection::verifyPS()
{
    return true;
    for (int i = 0; i < instruments.size(); ++i)
    {
        const auto &instr = instruments[i];

        if (instr.isPowerSupply)
        {
            //qDebug()<<"checking ps -> "<<instr.name;
            if (!validatePowerSupply(instr.name))
            {
                return false;
            }
        }
    }
    return true;
}

bool InstrConnection::validatePowerSupply(const QString &name)
{
    double volt=0;

    if (name == globalVariables["EXT-PS_HOST_NAME"])
            volt = driver_obj->analogIN(
            globalVariables["ap323_0"].toInt(),
            globalVariables["EXT-PS_AI_CHANNEL"].toInt()
            );
    else if (name == globalVariables["STATUS-PS_HOST_NAME"])
        volt = driver_obj->analogIN(
        globalVariables["ap323_0"].toInt(),
        globalVariables["STATUS-PS_AI_CHANNEL"].toInt()
        );
    else if (name == globalVariables["INT-PS_HOST_NAME"])
        volt = driver_obj->analogIN(
            globalVariables["ap323_0"].toInt(),
            globalVariables["INT-PS_AI_CHANNEL"].toInt()
            );
    else if (name == globalVariables["PYRO-PS_HOST_NAME"])
        volt = driver_obj->analogIN(
        globalVariables["ap323_0"].toInt(),
        globalVariables["PYRO-PS_AI_CHANNEL"].toInt()
        );

    usleep(1000);
    volt *= globalVariables["DIFF_VOLT_MULTIPLIER"].toDouble();

    PowerSupplyType expected = expectedPowerSupply(name);
    PowerSupplyType detected = detectPowerSupply(volt);

    if (detected == PowerSupplyType::UNKNOWN)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle("Power Supply Error");
        msgBox.setText("Power Supply Validation Failed");
        msgBox.setInformativeText(
            "Either Unknown power supply connected to " + name +
            "\n or \n AI reading is out of range.\n\n"
            "Please verify connections. \n \n  *Refer Operation Manual for further Assistance!!!"
            );
        msgBox.exec();
        return false;
    }

    if (detected != expected)
    {
        QMessageBox msgBox;
        msgBox.setIcon(QMessageBox::Critical);
        msgBox.setWindowTitle("Power Supply Address Error");
        msgBox.setText("Power Supply Address Interchange Detected");
        msgBox.setInformativeText(
            QString("Expected: %1\n"
                    "Detected: %2\n\n"
                    "Please verify IP Addresses of all power supplies.\n\n"
                    "*Refer Operation Manual for further Assistance!!!")
                .arg(psToString(expected), psToString(detected))
            );
        msgBox.exec();
        return false;
    }

    return true;
}

PowerSupplyType InstrConnection::expectedPowerSupply(const QString &name)
{
    if (name == globalVariables["EXT-PS_HOST_NAME"])
        return PowerSupplyType::EXT;

    if (name == globalVariables["STATUS-PS_HOST_NAME"])
        return PowerSupplyType::STATUS;

    if (name == globalVariables["INT-PS_HOST_NAME"])
        return PowerSupplyType::INT;

    if (name == globalVariables["PYRO-PS_HOST_NAME"])
        return PowerSupplyType::PYRO;

    return PowerSupplyType::UNKNOWN;
}

PowerSupplyType InstrConnection::detectPowerSupply(double volt)
{
    if (volt >= globalVariables["EXT-PS_RMT_VOLT_MIN"].toDouble() &&
        volt <= globalVariables["EXT-PS_RMT_VOLT_MAX"].toDouble())
        return PowerSupplyType::EXT;

    if (volt >= globalVariables["STATUS-PS_RMT_VOLT_MIN"].toDouble() &&
        volt <= globalVariables["STATUS-PS_RMT_VOLT_MAX"].toDouble())
        return PowerSupplyType::STATUS;

    if (volt >= globalVariables["INT-PS_RMT_VOLT_MIN"].toDouble() &&
        volt <= globalVariables["INT-PS_RMT_VOLT_MAX"].toDouble())
        return PowerSupplyType::INT;

    if (volt >= globalVariables["PYRO-PS_RMT_VOLT_MIN"].toDouble() &&
        volt <= globalVariables["PYRO-PS_RMT_VOLT_MAX"].toDouble())
        return PowerSupplyType::PYRO;

    return PowerSupplyType::UNKNOWN;
}

QString InstrConnection::psToString(PowerSupplyType ps)
{
    switch (ps)
    {
    case PowerSupplyType::EXT: return "EXT-PS";
    case PowerSupplyType::STATUS: return "STATUS-PS";
    case PowerSupplyType::INT: return "INT-PS";
    case PowerSupplyType::PYRO: return "PYRO-PS";
    default: return "UNKNOWN";
    }
}

//remote TCP/IP connection to instrument function
QString InstrConnection::verifyInstrumentConnection(QString VISA_ADDRESS)
{
    memset(responseStr,0,sizeof(responseStr));

    // Open a VISA resource Manager.
    status =viOpenDefaultRM(&resourceManager);
    if (status < VI_SUCCESS)
    {
        return "MGR NOT OPEN";
    }

    // Open a TCP/IP connection (session) to the instrument resource at VISA_ADDRESS.
    status = viOpen(resourceManager, VISA_ADDRESS.toLatin1().data(), VI_NULL,VI_NULL, &session);

    if (status >= VI_SUCCESS)
    {
        errorHandler(viQueryf(session, "*IDN?\n", "%t", responseStr), session);
        viClose(session);
        viClose(resourceManager);
        return responseStr;
    }
    else
    {
        viClose(session);
        viClose(resourceManager);
        return "SESSION NOT OPEN";
    }
}

//remote TCP/IP connection to instrument function
QString InstrConnection::verifyInstrumentSelfTestConnection(QString VISA_ADDRESS)
{
    memset(responseStr,0,sizeof(responseStr));

    // Open a VISA resource Manager.
    status =viOpenDefaultRM(&resourceManager);
    if (status < VI_SUCCESS)
    {
        return "MGR NOT OPEN";
    }

    // Open a TCP/IP connection (session) to the instrument resource at VISA_ADDRESS.
    status = viOpen(resourceManager, VISA_ADDRESS.toLatin1().data(), VI_NULL,VI_NULL, &session);

    if (status >= VI_SUCCESS)
    {
        viSetAttribute(session, VI_ATTR_TMO_VALUE, 240000);
        errorHandler(viQueryf(session,"*TST?\n", "%s", responseStr), session);

        viClose(session);
        viClose(resourceManager);
        return responseStr;
    }
    else
    {
        viClose(session);
        viClose(resourceManager);
        return "SESSION NOT OPEN";
    }
}

//remote TCP/IP connection to instrument function
QString InstrConnection::connectionOpen(QString VISA_ADDRESS)
{
    // Open a VISA resource Manager.
    status =viOpenDefaultRM(&resourceManager);

    if (status < VI_SUCCESS)
    {
        return "MGR NOT OPEN";
    }

    // Open a TCP/IP connection (session) to the instrument resource at VISA_ADDRESS.
    status = viOpen(resourceManager, VISA_ADDRESS.toLatin1().data(), VI_NULL,VI_NULL, &session);

    if (status < VI_SUCCESS)
    {
        viClose(session);
        viClose(resourceManager);
        return "SESSION NOT OPEN";
    }
    return "OPEN";
}

//remote TCP/IP connection to instrument function
void InstrConnection::connectionClose()
{
    viClose(session);
    viClose(resourceManager);
}

//error handler function for handling errors during communication with oscilloscope
void InstrConnection::errorHandler(ViStatus status, ViSession session)
{
    if (status < VI_SUCCESS)
    {
        // To get more information about the error we call viStatusDesc
        ViChar errorMessage[256];
        viStatusDesc(session, status, errorMessage);
    }
}

//polling capture function acquires the query results from oscilloscope using "Blocking Method"
double InstrConnection::pollingQueryCapture(ViSession session, ViConstString query)
{
    memset(responseStr,0,sizeof(responseStr));

    errorHandler(viQueryf(session,query, "%s", responseStr), session);

    int responseESR=0;

    //loop till event status register (ESR)shows 1 at 1st bit (OPC bit)
    while((responseESR & 1) != 0)
    {
        memset(responseESRStr,0,sizeof(responseESRStr));
        errorHandler(viQueryf(session,"*ESR?\n", "%s", responseESRStr), session);
        sscanf (responseESRStr, "%d", &responseESR);
        if(responseESR == 1)
        {
            break;
        }
    }

    if(strcmp(responseStr,"+99E+36")==0)
    {
        return 0;
    }
    else
    {
        char *endptr;
        return strtod(responseStr,&endptr);
        //spikeValue +=atof(responseStr);
    }
}

//popup message box showing disconnected/not connected instruments list
int InstrConnection::disconnectedInstr(QString msg)
{
    int ret=0;
    if(msg!="")
    {
        QMessageBox counterMsg;
        counterMsg.setWindowFlags(Qt::WindowStaysOnTopHint & Qt::FramelessWindowHint);
        if(modelMismatch)
        {
            counterMsg.setWindowTitle("Instrument Mismatch Error!!");
        }
        else
        {
            counterMsg.setWindowTitle("Instrument Disconnect Error!!");
        }
        QString ip = getIPByName(msg);
        counterMsg.setIcon(QMessageBox::Warning);
        counterMsg.setStandardButtons(QMessageBox::Ok);
        QString sop="Check below mentioned SOP : <br> 1) Instrument is ON <br> 2) Instrument Ethernet Connetion is live at both ends <br> 3) Ethernet Switch is ON <br> 4) RDACS connection to Switch is Live at both ends <br> 5) All the Instruments and RDACS are ethernet configured to 192.168.1.*/255.255.255.0 <br> <br> *Refer Operation Manual for further Assistance!!!";
        // Use HTML to style the informative text
        if(modelMismatch)
        {
            QString informativeText = "<font size='2'>"
                                      + mismatch + "<br> <br> *Check the Model of the Connected Instrument!!!" + "<br><br>"
                                              "Msg Box closes in <b>15</b> seconds."
                                              "</font>";
            counterMsg.setText(informativeText);
        }
        else
        {
            QString expected=("Expected: "+msg+" at ("+ip+")");
            QString informativeText = "<font size='2'>"
                                      + sop + "<br><br>"
                                              "Msg Box closes in <b>15</b> seconds."
                                              "</font>";
            counterMsg.setText(expected +"<br><br>"+informativeText);
        }
        int count =15;

        QTimer cntDown;
        // Lambda function to update countdown label
        auto updateCountdown = [&]() {
            if (count > 0)
            {
                if(modelMismatch)
                {
                    QString informativeText = "<font size='2'>"
                                              + mismatch + "<br> <br> *Check the Model of the Connected Instrument!!!" +"<br><br>"
                                                      "Msg Box closes in <b>"+QString::number(count--)+"</b> seconds."
                                                                           "</font>";
                    counterMsg.setText(informativeText);
                }
                else
                {
                    QString expected=("Expected: "+msg+" at ("+ip+")");
                    QString informativeText = "<font size='2'>"
                                              + sop + "<br><br>"
                                                      "Msg Box closes in <b>"+QString::number(count--)+"</b> seconds."
                                                                           "</font>";
                    counterMsg.setText(expected+"<br><br>"+informativeText);
                }

            }
            else
            {
                cntDown.stop();
                counterMsg.close();
            }
        };

        // Connect timer to lambda function
        QObject::connect(&cntDown, &QTimer::timeout, updateCountdown);

        // Set timer interval to update every second
        cntDown.start(1000);

        // Show the message box
        ret=counterMsg.exec();
    }
     return ret;
}

//remote TCP/IP connection to instrument function
void InstrConnection::setInstrument(QString Instrument)
{
    //setting oscilloscope to factory default settings
    errorHandler(viPrintf(session, "*CLS\n"), session);
    //errorHandler(viPrintf(session, "*RST\n"), session);

    if(Instrument==globalVariables["EXT-PS_HOST_NAME"])
    {
        //setting power supply settings
        ViConstString volt=("VOLT "+globalVariables["EXT-PS_VOLT_SUPLLY"]+"\n").toUtf8().constData();
        //ViConstString volt_limit=("VOLT "+globalVariables["EXT-PS_VOLT_LIMIT"]+"\n").toUtf8().constData();
        ViConstString curr_limit=("CURR 10\n");
        //ViConstString curr_limit=("CURR "+globalVariables["EXT-PS_CURR_LIMIT"]+"\n").toUtf8().constData();
        errorHandler(viPrintf(session, volt), session);
        usleep(1000);
        errorHandler(viPrintf(session, curr_limit), session);   //default on reset, sets limit to 1% rating
    }

    if(Instrument==globalVariables["STATUS-PS_HOST_NAME"])
    {
        //setting power supply settings
        ViConstString volt=("VOLT "+globalVariables["STATUS-PS_VOLT_SUPLLY"]+"\n").toUtf8().constData();
        //ViConstString volt_limit=("VOLT "+globalVariables["STATUS-PS_VOLT_LIMIT"]+"\n").toUtf8().constData();
        ViConstString curr_limit=("CURR 5\n");
        //ViConstString curr_limit=("CURR "+globalVariables["STATUS-PS_CURR_LIMIT"]+"\n").toUtf8().constData();
        errorHandler(viPrintf(session, volt), session);
        usleep(1000);
        errorHandler(viPrintf(session, curr_limit), session);   //default on reset, sets limit to 1% rating
    }

    if(Instrument==globalVariables["INT-PS_HOST_NAME"])
    {
        //setting power supply settings
        ViConstString volt=("VOLT "+globalVariables["INT-PS_VOLT_SUPLLY"]+"\n").toUtf8().constData();
        //ViConstString volt_limit=("VOLT "+globalVariables["INT-PS_VOLT_LIMIT"]+"\n").toUtf8().constData();
        ViConstString curr_limit=("CURR 5\n");
        //ViConstString curr_limit=("CURR "+globalVariables["INT-PS_CURR_LIMIT"]+"\n").toUtf8().constData();
        errorHandler(viPrintf(session, volt), session);
        usleep(1000);
        errorHandler(viPrintf(session, curr_limit), session);  //default on reset, sets limit to 1% rating
    }

    if(Instrument==globalVariables["PYRO-PS_HOST_NAME"])
    {
        //setting power supply settings
        ViConstString volt=("VOLT "+globalVariables["PYRO-PS_VOLT_SUPLLY"]+"\n").toUtf8().constData();
        //ViConstString volt_limit=("VOLT "+globalVariables["PYRO-PS_VOLT_LIMIT"]+"\n").toUtf8().constData();
        ViConstString curr_limit=("CURR 5\n");
        //ViConstString curr_limit=("CURR "+globalVariables["PYRO-PS_CURR_LIMIT"]+"\n").toUtf8().constData();
        errorHandler(viPrintf(session, volt), session);
        usleep(1000);
        errorHandler(viPrintf(session, curr_limit), session);   //default on reset, sets limit to 1% rating
    }

    if(Instrument==globalVariables["SEEKER-PS_HOST_NAME"])
    {
        //setting power supply settings
        ViConstString volt=("VOLT "+globalVariables["SEEKER-PS_VOLT_SUPLLY"]+"\n").toUtf8().constData();
        //ViConstString volt_limit=("VOLT "+globalVariables["SEEKER-PS_VOLT_LIMIT"]+"\n").toUtf8().constData();
        ViConstString curr_limit=("CURR 5\n");
        //ViConstString curr_limit=("CURR "+globalVariables["SEEKER-PS_CURR_LIMIT"]+"\n").toUtf8().constData();
        errorHandler(viPrintf(session, volt), session);
        usleep(1000);
        errorHandler(viPrintf(session, curr_limit), session);  //default on reset, sets limit to 1% rating
    }
}

//remote TCP/IP connection to instrument function to set out ON
void InstrConnection::setInstrumentOutON()
{
    //output ON
    errorHandler(viPrintf(session, "OUTP ON\n"), session);
    //sleep(1);
}

//remote TCP/IP connection to instrument function for reading volatage
double InstrConnection::readInstrumentVolt()
{
    double powerSupplyVoltage=0;
    usleep(1000);
    //long wait cmd to be run with operation complete command
    powerSupplyVoltage=pollingQueryCapture(session,"MEAS:VOLT?; *OPC\n");
    return powerSupplyVoltage;
}

//remote TCP/IP connection to instrument function for reading current
double InstrConnection::readInstrumentCurrent()
{
    usleep(1000);
    double powerSupplyCurrent=0;
    //long wait cmd to be run with operation complete command
    powerSupplyCurrent=pollingQueryCapture(session,"MEAS:CURR?; *OPC\n");
    return powerSupplyCurrent;
}

//remote TCP/IP connection to instrument function to set out OFF
void InstrConnection::setInstrumentOutOFF()
{
    //output OFF
    errorHandler(viPrintf(session, "OUTP OFF\n"), session);
}


void InstrConnection::earth2NeutralInIt()
{
    //DVM ACRMS display at oscilloscope screen
    errorHandler(viPrintf(session,":MEAS:CLEAr\n"), session);

    //DVM ACRMS display at oscilloscope screen
    errorHandler(viPrintf(session,":DVM:ENABle 1\n"), session);

    //DVM ACRMS display at oscilloscope screen
    errorHandler(viPrintf(session,":DVM:MODE ACRMs\n"), session);

    //DVM ACRMS display at oscilloscope screen
    ViConstString measure=(":DVM:SOURce CHANnel"+globalVariables["EARTH2NUETRAL_CHANNEL"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session,measure), session);

}

//intialise function to set the environment of oscilloscpe, ready for reading spike signal
double InstrConnection::earth2Neutral()
{
    double spikeValue=-1;

    memset(responseStr,0,sizeof(responseStr));

    ViConstString measure=(":MEASure:SOURce CHANnel"+globalVariables["EARTH2NUETRAL_CHANNEL"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, measure), session);

    //sleep(1);

    //measurments made at oscilloscope screen
    errorHandler(viPrintf(session,":MEASure:VRMS\n"), session);

    //amplitude measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":MEASure:VRMS?\n", "%s", responseStr), session);

    if(strcmp(responseStr,"+99E+36")==0)
    {
        return spikeValue ;
    }
    else
    {
        char *endptr;
        spikeValue = strtod(responseStr,&endptr);
        //spikeValue +=atof(responseStr);
    }

    return spikeValue;
}

//intialise function to set the environment of oscilloscpe, ready for reading spike signal
double InstrConnection::earth2NeutralDVM()
{
    double spikeValue=-1;

    memset(responseStr,0,sizeof(responseStr));

    sleep(1);

    //DVM measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":DVM:CURR?\n", "%s", responseStr), session);

    if(strcmp(responseStr,"+99E+36")==0)
    {
        return spikeValue ;
    }
    else
    {
        char *endptr;
        spikeValue = strtod(responseStr,&endptr);
        //spikeValue +=atof(responseStr);
    }

    return spikeValue;
}

void InstrConnection::firingPulseLeftInIt()
{
    //Oscilloscope environment setup
    //clear the session
    viClear(session);

    //setting oscilloscope to factory default settings
    errorHandler(viPrintf(session, "*CLS\n"), session);
    //errorHandler(viPrintf(session, "*RST\n"), session);
    //errorHandler(viPrintf(session, ":STOP\n"), session);

    //auto scale auto sets channel on vertical axis (Volts); *RST cmd default settings contains channel1 scale to 5 volts
    //ViConstString channel=(":AUToscale CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+"\n").toUtf8().constData();
    //auto scale auto sets channel on vertical axis (Volts); *RST cmd default settings contains channel1 scale to 5 volts
    ViConstString channel1=(":CHANnel1:DISPlay 0\n");
    errorHandler(viPrintf(session, channel1), session);

    ViConstString channel3=(":CHANnel3:DISPlay 0\n");
    errorHandler(viPrintf(session, channel3), session);

    ViConstString channel4=(":CHANnel4:DISPlay 0\n");
    errorHandler(viPrintf(session, channel4), session);

    ViConstString channel=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+":DISPlay 1\n").toUtf8().constData();
    errorHandler(viPrintf(session, channel), session);

    ViConstString scale=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+":SCALe "+globalVariables["FIRINGPULSE_SCALE"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, scale), session);
    ViConstString coupling=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+":COUPling "+globalVariables["FIRINGPULSE_COUPLING"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, coupling), session);

    //setting time base (x axis) to 50ms
    ViConstString range=(":TIMebase:RANGe "+globalVariables["FIRINGPULSE_RANGE"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, range), session);

    //setting trigger acquire to normal and threshold level to 0.1 volts
    ViConstString trig=(":TRIGger:SOURce CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, trig), session);
    ViConstString sweep=(":TRIGger:SWEep "+globalVariables["FIRINGPULSE_SWEEP"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, sweep), session);
    ViConstString trigmode=(":TRIGger:MODE EDGE\n");
    errorHandler(viPrintf(session, trigmode), session);
    ViConstString trigslope=(":TRIGger:EDGE:SLOP POS\n");
    errorHandler(viPrintf(session, trigslope), session);
    ViConstString level=(":TRIGger:LEVel "+globalVariables["FIRINGPULSE_TRIGGER_LEVEL"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, level), session);

    //pollingCmdCapture(session,":RUN; *OPC\n");

}


//intialise function to set the environment of oscilloscpe, ready for reading firing pulse signal
void InstrConnection::firingPluseCaptureLeft()
{
    pollingCmdCapture(session,":SINGle; *OPC\n");

    memset(responseVAmpStr,0,sizeof(responseVAmpStr));
    memset(responsePWidthStr,0,sizeof(responsePWidthStr));
    //Measurement environment settings
    //errorHandler(viPrintf(session,":MEASure:DEFine THResholds,STANdard\n"), session);

    sleep(1);

    ViConstString measure=(":MEASure:SOURce CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_LEFT"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, measure), session);

    //measurments made at oscilloscope screen
    usleep(500);

    //errorHandler(viPrintf(session,":MEASure:VAMPlitude\n"), session);
    //errorHandler(viPrintf(session,":MEASure:PWIDth\n"), session);

    //usleep(500);

    //amplitude measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":MEASure:VAMPlitude?\n", "%s", responseVAmpStr), session);

    usleep(500);

    //pulse width measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":MEASure:PWIDth?", "%s", responsePWidthStr), session);
}

void InstrConnection::firingPulseRightInIt()
{
    //Oscilloscope environment setup
    //clear the session
    viClear(session);

    //setting oscilloscope to factory default settings
    errorHandler(viPrintf(session, "*CLS\n"), session);
    //errorHandler(viPrintf(session, "*RST\n"), session);
    //errorHandler(viPrintf(session, ":STOP\n"), session);

    //auto scale auto sets channel on vertical axis (Volts); *RST cmd default settings contains channel1 scale to 5 volts
    //ViConstString channel=(":AUToscale CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+"\n").toUtf8().constData();
    //errorHandler(viPrintf(session, channel), session);

    ViConstString channel1=(":CHANnel1:DISPlay 0\n");
    errorHandler(viPrintf(session, channel1), session);

    ViConstString channel2=(":CHANnel2:DISPlay 0\n");
    errorHandler(viPrintf(session, channel2), session);

    ViConstString channel4=(":CHANnel4:DISPlay 0\n");
    errorHandler(viPrintf(session, channel4), session);

    ViConstString channel=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+":DISPlay 1\n").toUtf8().constData();
    errorHandler(viPrintf(session, channel), session);

    ViConstString scale=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+":SCALe "+globalVariables["FIRINGPULSE_SCALE"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, scale), session);
    ViConstString coupling=(":CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+":COUPling "+globalVariables["FIRINGPULSE_COUPLING"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, coupling), session);

    //setting time base (x axis) to 50ms
    ViConstString range=(":TIMebase:RANGe "+globalVariables["FIRINGPULSE_RANGE"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, range), session);

    ViConstString trig=(":TRIGger:SOURce CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, trig), session);

    //setting trigger acquire to normal and threshold level to 0.1 volts
    ViConstString sweep=(":TRIGger:SWEep "+globalVariables["FIRINGPULSE_SWEEP"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, sweep), session);
    ViConstString trigmode=(":TRIGger:MODE EDGE\n");
    errorHandler(viPrintf(session, trigmode), session);
    ViConstString trigslope=(":TRIGger:EDGE:SLOP POS\n");
    errorHandler(viPrintf(session, trigslope), session);
    ViConstString level=(":TRIGger:LEVel "+globalVariables["FIRINGPULSE_TRIGGER_LEVEL"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, level), session);

    //pollingCmdCapture(session,":RUN; *OPC\n");
}

//intialise function to set the environment of oscilloscpe, ready for reading firing pulse signal
void InstrConnection::firingPluseCaptureRight()
{
    pollingCmdCapture(session,":SINGle; *OPC\n");

    memset(responseVAmpStr,0,sizeof(responseVAmpStr));
    memset(responsePWidthStr,0,sizeof(responsePWidthStr));
    //Measurement environment settings
    //errorHandler(viPrintf(session,":MEASure:DEFine THResholds,STANdard\n"), session);

    ViConstString measure=(":MEASure:SOURce CHANnel"+globalVariables["FIRINGPULSE_CHANNEL_RIGHT"]+"\n").toUtf8().constData();
    errorHandler(viPrintf(session, measure), session);

    //measurments made at oscilloscope screen
    usleep(500);

    //errorHandler(viPrintf(session,":MEASure:VAMPlitude\n"), session);
    //errorHandler(viPrintf(session,":MEASure:PWIDth\n"), session);

    //usleep(500);

    //amplitude measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":MEASure:VAMPlitude?\n", "%s", responseVAmpStr), session);

    usleep(500);

    //pulse width measurments done at oscilloscope and retrieved back
    errorHandler(viQueryf(session,":MEASure:PWIDth?", "%s", responsePWidthStr), session);

}

//polling capture function acquires the signal on oscilloscope screen using "Blocking Method"
void InstrConnection::pollingCmdCapture(ViSession session,ViConstString command)
{
    //long wait cmd to be run with operation complete command
    errorHandler(viPrintf(session,command), session);

    int responseESR=0;

    //loop till event status register (ESR)shows 1 at 1st bit (OPC bit)
    while((responseESR & 1) != 0)
    {
        memset(responseESRStr,0,sizeof(responseESRStr));
        errorHandler(viQueryf(session,"*ESR?\n", "%s", responseESRStr), session);
        sscanf (responseESRStr, "%d", &responseESR);
        if(responseESR == 1)
        {
            break;
        }
        //sleep for 50ms to avoid multiple ESR querries
        //usleep(5000);
    }
}

void InstrConnection::firingPluseClear()
{
    errorHandler(viPrintf(session, "*CLS\n"), session);
}

void InstrConnection::screenClear()
{
    errorHandler(viPrintf(session, "*RST\n"), session);
}


void InstrConnection::selfTest()
{
    //QString msg;
    failedSelfTestInstrument="";
    failedSelfTestInstrumentStatus="";

    for (int i = 0; i < instruments.size(); ++i)
    {
        auto &instr = instruments[i];

        if (!instr.isPowerSupply)
        {
            usleep(1000);
            emit selfTestSignal(instr.name,true);
            usleep(1000);
            if(1)//instr.name !="SIG-GEN")
            {
                QString responseStr = verifyInstrumentSelfTestConnection(instr.visa);

                if (!responseStr.contains("0", Qt::CaseInsensitive))
                {
                    //msg.append(instr.name + "," + instr.visa + "," + instr.model + ",Self Test FAIL\n");

                    instr.selfTest=false;
                    failedSelfTestInstrument=instr.name;
                    failedSelfTestInstrumentStatus = responseStr;

                    //qDebug() << instr.visa << " -> " << instr.name << " -> "<< failedSelfTestInstrumentStatus<< " , self test FAIL";

                    //break;   // ✅ stop on FIRST failure
                }
                else
                {
                    instr.selfTest=true;
                    //msg.append(instr.name + "," + instr.visa + "," + instr.model + ",Self Test PASS\n");
                    //qDebug() << instr.visa << " -> " << instr.name << " -> "<< "self test PASS";
                }
            }
            usleep(1000);
            emit selfTestSignal(instr.name,false);
            usleep(1000);
        }
    }
}
