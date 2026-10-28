#ifndef DRIVERS_H
#define DRIVERS_H

#include <QObject>
#include "apcommon.h"
#include "ap323.h"
#include "ap441.h"
#include "ap445.h"

class Drivers : public QObject
{
    Q_OBJECT
public:
    explicit Drivers(QObject *parent = nullptr);
    double analogIN(int ap_instance, int channel);
    double optoDI(int ap_instance, int port, int point, int mode);
    double optoDO(int ap_instance, int point, int value, long mode);
};

#endif // DRIVERS_H
