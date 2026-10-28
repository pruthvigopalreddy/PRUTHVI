#pragma once
#include <QObject>
#include <QSerialPort>
#include <QTimer>
#include <QByteArray>

class SerialCOMM : public QObject
{
    Q_OBJECT
public:
    explicit SerialCOMM(QObject *parent = nullptr);
    ~SerialCOMM();
    QString response="";

public slots:
    void init();
    void connectPort(int missile_select);
    void shutdown();

signals:
    void obpCommunicationSignal(const QString &msg);
    void updateProgressBarSignal(int value);
    void responseReady(const QByteArray &response);
    void finished();

private slots:
    void onReadyRead();
    void onSerialError(QSerialPort::SerialPortError err);
    void onTimeoutInitialA();
    void onTimeoutResponse();

private:
    void sendPacket();
    void finishWithResponse(const QString &msg);

    QSerialPort *serial = nullptr;
    QTimer *initialATimer = nullptr;
    QTimer *responseTimer = nullptr;

    QByteArray buffer;

    enum class State { Idle, WaitingForA, SentPacketWaitingResponse, Done };
    State state = State::Idle;
};
