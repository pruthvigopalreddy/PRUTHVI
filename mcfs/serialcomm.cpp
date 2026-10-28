#include "serialcomm.h"
#include <QDebug>
#include <QThread>
#include <unistd.h>

SerialCOMM::SerialCOMM(QObject *parent) : QObject(parent) {}

SerialCOMM::~SerialCOMM()
{
    if (serial) {
        if (serial->isOpen()) serial->close();
        delete serial;
    }
    delete initialATimer;
    delete responseTimer;
}

void SerialCOMM::init()
{
    serial = new QSerialPort(this);
    initialATimer = new QTimer(this);
    responseTimer = new QTimer(this);

    initialATimer->setSingleShot(true);
    responseTimer->setSingleShot(true);

    connect(serial, &QSerialPort::readyRead, this, &SerialCOMM::onReadyRead);
    connect(serial, &QSerialPort::errorOccurred, this, &SerialCOMM::onSerialError);
    connect(initialATimer, &QTimer::timeout, this, &SerialCOMM::onTimeoutInitialA);
    connect(responseTimer, &QTimer::timeout, this, &SerialCOMM::onTimeoutResponse);

    QString msg=QStringLiteral("SerialCOMM initialized in thread %1").arg(reinterpret_cast<quintptr>(QThread::currentThreadId()));
    emit obpCommunicationSignal(msg);
}

void SerialCOMM::shutdown()
{
    if (initialATimer && initialATimer->isActive()) initialATimer->stop();
    if (responseTimer && responseTimer->isActive()) responseTimer->stop();
    if (serial && serial->isOpen()) serial->close();
    state = State::Done;
    emit finished();
}

void SerialCOMM::connectPort(int missile_select)
{
    if (!serial) {
        emit obpCommunicationSignal("Error: SerialCOMM not initialized");
        emit responseReady(QByteArray());
        return;
    }

    QString portName;
    if (missile_select == 0)
        portName = "/dev/ttyS0";  // left
    else
        portName = "/dev/ttyS1";  // right

    emit obpCommunicationSignal(QStringLiteral("Requesting to open port: %1").arg(portName));
    emit updateProgressBarSignal(5);

    if (serial->isOpen()) serial->close();

    serial->setPortName(portName);
    serial->setBaudRate(QSerialPort::Baud9600);
    serial->setDataBits(QSerialPort::Data8);
    serial->setParity(QSerialPort::NoParity);
    serial->setStopBits(QSerialPort::OneStop);
    serial->setFlowControl(QSerialPort::NoFlowControl);

    if (!serial->open(QIODevice::ReadWrite)) {
        emit obpCommunicationSignal(
            QStringLiteral("Unable to open serial port: %1").arg(serial->errorString()));
        emit updateProgressBarSignal(0);
        state = State::Done;
        emit responseReady(QByteArray());
        return;
    }

    buffer.clear();
    state = State::WaitingForA;
    emit obpCommunicationSignal(QStringLiteral("Port opened successfully: %1").arg(portName));
    emit updateProgressBarSignal(10);
    initialATimer->start(5000); // wait up to 5s for 'A'
}

void SerialCOMM::onReadyRead()
{
    QByteArray data = serial->readAll();
    if (data.isEmpty()) return;

    buffer += data;
    QString dataHex=QString::fromLatin1(data.toHex(' ').toUpper());
    QString bufferHex=QString::fromLatin1(buffer.toHex(' ').toUpper());
    QString msg= QStringLiteral("onReadyRead: got bytes: %1 buffer now: %2").arg(dataHex, bufferHex);
    emit obpCommunicationSignal(msg);

    if (state == State::WaitingForA) {
        if (buffer.contains('A')) {
            if (initialATimer->isActive()) initialATimer->stop();
            emit obpCommunicationSignal(QStringLiteral("'A' received from board"));
            emit updateProgressBarSignal(20);
            sendPacket();
        }
    } else if (state == State::SentPacketWaitingResponse) {
        if (responseTimer->isActive()) responseTimer->stop();
        emit updateProgressBarSignal(100);
        response=QString::fromLatin1(buffer.toHex(' ').toUpper());
        msg=QStringLiteral("Response received (HEX): %1").arg(bufferHex);
        finishWithResponse(msg);
    }
}

void SerialCOMM::sendPacket()
{
    QByteArray send_pkt = QByteArray::fromHex("5504534E4C46");
    //QByteArray send_pkt = QByteArray::fromHex("55044E554C48");
    qint64 written = serial->write(send_pkt);

    if (written == -1) {
        emit obpCommunicationSignal(
            QStringLiteral("Error writing data to port: %1").arg(serial->errorString()));
        if (serial->isOpen()) serial->close();
        state = State::Done;
        emit responseReady(QByteArray());
        return;
    }

    if (!serial->waitForBytesWritten(1000)) {
        emit obpCommunicationSignal("Warning: bytesWritten not confirmed within 1s");
    }
    else
    {
        QString msg=QStringLiteral("Data written successfully, bytes: %1").arg(written);
        emit obpCommunicationSignal(msg);
    }

    emit updateProgressBarSignal(60);
    state = State::SentPacketWaitingResponse;
    buffer.clear();
    usleep(100000);
    responseTimer->start(5000);
}

void SerialCOMM::onTimeoutInitialA()
{
    if (state != State::WaitingForA) return;
    if (serial && serial->isOpen()) serial->close();
    state = State::Done;
    emit obpCommunicationSignal("Timeout waiting for 'A'");
    //emit responseReady(QByteArray());
}

void SerialCOMM::onTimeoutResponse()
{
    if (state != State::SentPacketWaitingResponse) return;

    QString bufferHex=QString::fromLatin1(buffer.toHex(' ').toUpper());

    QString msg = buffer.isEmpty()
                      ? QStringLiteral("Timeout waiting for response")
                      : QStringLiteral("Timeout; partial response: %1").arg(bufferHex);
    if (serial && serial->isOpen()) serial->close();
    state = State::Done;
    emit obpCommunicationSignal(msg);
    //emit responseReady(buffer);
}

void SerialCOMM::onSerialError(QSerialPort::SerialPortError err)
{
    Q_UNUSED(err)
    if (!serial) return;
    QString e = QStringLiteral("Serial port error: %1").arg(serial->errorString());
    //qDebug() << e;
    if (initialATimer->isActive()) initialATimer->stop();
    if (responseTimer->isActive()) responseTimer->stop();
    if (serial->isOpen()) serial->close();
    state = State::Done;
    emit obpCommunicationSignal(e);
    //emit responseReady(QByteArray());
}

void SerialCOMM::finishWithResponse(const QString &msg)
{
    if (serial && serial->isOpen()) serial->close();
    state = State::Done;
    emit obpCommunicationSignal(msg);
    emit responseReady(buffer);
}
