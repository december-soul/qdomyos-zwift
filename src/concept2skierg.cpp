#include "concept2skierg.h"
#include "ftmsbike.h"
#include "ios/lockscreen.h"
#include "virtualtreadmill.h"
#include <QBluetoothLocalDevice>
#include <QDateTime>
#include <QFile>
#include <QMetaEnum>
#include <QSettings>

#include <QThread>
#include <math.h>

#ifdef Q_OS_ANDROID
#include <QLowEnergyConnectionParameters>
#endif
#include "keepawakehelper.h"
#include <chrono>

using namespace std::chrono_literals;

concept2skierg::concept2skierg(bool noWriteResistance, bool noHeartService) {
    m_watt.setType(metric::METRIC_WATT);
    Speed.setType(metric::METRIC_SPEED);
    refresh = new QTimer(this);
    this->noWriteResistance = noWriteResistance;
    this->noHeartService = noHeartService;
    initDone = false;
    connect(refresh, &QTimer::timeout, this, &concept2skierg::update);
    refresh->start(200ms);
}

void concept2skierg::writeCharacteristic(uint8_t *data, uint8_t data_len, const QString &info, bool disable_log,
                                         bool wait_for_response) {
    QEventLoop loop;
    QTimer timeout;
    if (wait_for_response) {
        connect(gattFTMSService, &QLowEnergyService::characteristicChanged, &loop, &QEventLoop::quit);
        timeout.singleShot(300ms, &loop, &QEventLoop::quit);
    } else {
        connect(gattFTMSService, &QLowEnergyService::characteristicWritten, &loop, &QEventLoop::quit);
        timeout.singleShot(300ms, &loop, &QEventLoop::quit);
    }

    gattFTMSService->writeCharacteristic(gattWriteCharControlPointId, QByteArray((const char *)data, data_len));

    if (!disable_log) {
        emit debug(QStringLiteral(" >> ") + QByteArray((const char *)data, data_len).toHex(' ') +
                   QStringLiteral(" // ") + info);
    }

    loop.exec();
}

void concept2skierg::forceResistance(int8_t requestResistance) {

    uint8_t write[] = {FTMS_SET_INDOOR_BIKE_SIMULATION_PARAMS, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    write[3] = ((uint16_t)requestResistance * 100) & 0xFF;
    write[4] = ((uint16_t)requestResistance * 100) >> 8;

    writeCharacteristic(write, sizeof(write), QStringLiteral("forceResistance ") + QString::number(requestResistance));
}

void concept2skierg::update() {
    if (m_control->state() == QLowEnergyController::UnconnectedState) {

        emit disconnected();
        return;
    }

    if (initRequest) {

        initRequest = false;
    } else if (bluetoothDevice.isValid() &&

               m_control->state() == QLowEnergyController::DiscoveredState //&&
                                                                           // gattCommunicationChannelService &&
                                                                           // gattWriteCharacteristic.isValid() &&
                                                                           // gattNotify1Characteristic.isValid() &&
               /*initDone*/) {

        update_metrics(true, watts());

        // updating the treadmill console every second
        if (sec1Update++ == (500 / refresh->interval())) {

            sec1Update = 0;
            // updateDisplay(elapsed);
        }

        if (requestResistance != -1) {
            if (requestResistance > 100) {
                requestResistance = 100;
            } // TODO, use the bluetooth value
            else if (requestResistance == 0) {
                requestResistance = 1;
            }

            if (requestResistance != currentResistance().value()) {
                emit debug(QStringLiteral("writing resistance ") + QString::number(requestResistance));

                forceResistance(requestResistance);
            }
            requestResistance = -1;
        }
        if (requestStart != -1) {
            emit debug(QStringLiteral("starting..."));

            // btinit();

            requestStart = -1;
            emit bikeStarted();
        }
        if (requestStop != -1) {
            emit debug(QStringLiteral("stopping..."));

            // writeCharacteristic(initDataF0C800B8, sizeof(initDataF0C800B8), "stop tape");
            requestStop = -1;
        }
    }
}

void concept2skierg::serviceDiscovered(const QBluetoothUuid &gatt) {
    emit debug(QStringLiteral("serviceDiscovered ") + gatt.toString());
}

void concept2skierg::characteristicChanged(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {

    // qDebug() << "characteristicChanged" << characteristic.uuid() << newValue << newValue.length();
    Q_UNUSED(characteristic);
    QSettings settings;
    QString heartRateBeltName =
        settings.value(QStringLiteral("heart_rate_belt_name"), QStringLiteral("Disabled")).toString();

    qDebug() << QStringLiteral(" << ") << characteristic.uuid() << " " << newValue.toHex(' ');

    lastPacket = newValue;

    if (characteristic.uuid() == QBluetoothUuid(QStringLiteral("ce060031-43e5-11e4-916c-0800200c9a66")) &&
        newValue.length() >= 19) {

        uint32_t elapsed_centsec = ((((uint32_t)newValue.at(2)) << 16) | ((uint32_t)((uint16_t)newValue.at(1)) << 8) |
                                    (uint32_t)((uint8_t)newValue.at(0)));
        uint32_t distance_dm = ((((uint32_t)newValue.at(5)) << 16) | ((uint32_t)((uint16_t)newValue.at(4)) << 8) |
                                (uint32_t)((uint8_t)newValue.at(3)));

        Distance = distance_dm / 10000.0;
        emit debug(QStringLiteral("Current Distance: ") + QString::number(Distance.value()));

        uint8_t workout_type = newValue.at(6);
        uint8_t interval_type = newValue.at(7);
        uint8_t workout_state = newValue.at(8);
        uint8_t rowing_state = newValue.at(9);
        uint8_t stroke_state = newValue.at(10);
        uint32_t total_work_distance =
            ((((uint32_t)newValue.at(13)) << 16) | ((uint32_t)((uint16_t)newValue.at(12)) << 8) |
             (uint32_t)((uint8_t)newValue.at(11)));
        uint32_t workout_duration =
            ((((uint32_t)newValue.at(16)) << 16) | ((uint32_t)((uint16_t)newValue.at(15)) << 8) |
             (uint32_t)((uint8_t)newValue.at(14)));
        uint8_t workout_duration_type = newValue.at(17);
        uint8_t drag_factor = newValue.at(18);
    } else if (characteristic.uuid() == QBluetoothUuid(QStringLiteral("ce060032-43e5-11e4-916c-0800200c9a66")) &&
               newValue.length() >= 17) {
        uint32_t elapsed_centsec = ((((uint32_t)newValue.at(2)) << 16) | ((uint32_t)((uint16_t)newValue.at(1)) << 8) |
                                    (uint32_t)((uint8_t)newValue.at(0)));
        // 0.001 m/s
        uint16_t speed_ms = (((uint16_t)((uint16_t)newValue.at(4)) << 8) | (uint16_t)((uint8_t)newValue.at(3)));
        Speed = speed_ms * 0.0036;
        emit debug(QStringLiteral("Current Speed: ") + QString::number(Speed.value()));

        uint8_t stroke_rate = newValue.at(5);
        Cadence = stroke_rate;

        StrokesCount += (Cadence.value()) *
                        ((double)lastRefreshCharacteristicChanged.msecsTo(QDateTime::currentDateTime())) / 600000;

        emit debug(QStringLiteral("Strokes Count: ") + QString::number(StrokesCount.value()));

        uint8_t heart_rate = newValue.at(6);

#ifdef Q_OS_ANDROID
        if (settings.value("ant_heart", false).toBool())
            Heart = (uint8_t)KeepAwakeHelper::heart();
        else
#endif
        {
            if (heart_rate != 0xFF)
                Heart = heart_rate;
            emit debug(QStringLiteral("Current Heart: ") + QString::number(Heart.value()));
        }

        // 0.01 sec
        uint16_t current_pace = (((uint16_t)((uint16_t)newValue.at(8)) << 8) | (uint16_t)((uint8_t)newValue.at(7)));
        // 0.01 sec
        uint16_t average_pace = (((uint16_t)((uint16_t)newValue.at(10)) << 8) | (uint16_t)((uint8_t)newValue.at(9)));
        uint16_t rest_distance = (((uint16_t)((uint16_t)newValue.at(12)) << 8) | (uint16_t)((uint8_t)newValue.at(11)));
        uint32_t rest_time = ((((uint32_t)newValue.at(15)) << 16) | ((uint32_t)((uint16_t)newValue.at(14)) << 8) |
                              (uint32_t)((uint8_t)newValue.at(13)));
        uint8_t erg_machine_type = newValue.at(16);
    } else if (characteristic.uuid() == QBluetoothUuid(QStringLiteral("ce060033-43e5-11e4-916c-0800200c9a66")) &&
               newValue.length() >= 20) {
        uint32_t elapsed_centsec = ((((uint32_t)newValue.at(2)) << 16) | ((uint32_t)((uint16_t)newValue.at(1)) << 8) |
                                    (uint32_t)((uint8_t)newValue.at(0)));
        uint8_t interval_count = newValue.at(3);
        uint16_t average_power = (((uint16_t)((uint16_t)newValue.at(5)) << 8) | (uint16_t)((uint8_t)newValue.at(4)));
        uint16_t total_calories = (((uint16_t)((uint16_t)newValue.at(7)) << 8) | (uint16_t)((uint8_t)newValue.at(6)));
        KCal = total_calories;
        emit debug(QStringLiteral("Current KCal: ") + QString::number(KCal.value()));
        // ...
    }

    if (Cadence.value() > 0) {

        CrankRevs++;
        LastCrankEventTime += (uint16_t)(1024.0 / (((double)(Cadence.value())) / 60.0));
    }

    lastRefreshCharacteristicChanged = QDateTime::currentDateTime();

    if (heartRateBeltName.startsWith(QStringLiteral("Disabled"))) {

#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
        lockscreen h;
        long appleWatchHeartRate = h.heartRate();
        h.setKcal(KCal.value());
        h.setDistance(Distance.value());
        Heart = appleWatchHeartRate;
        debug("Current Heart from Apple Watch: " + QString::number(appleWatchHeartRate));
#endif
#endif
    }

#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
    bool cadence = settings.value("bike_cadence_sensor", false).toBool();
    bool ios_peloton_workaround = settings.value("ios_peloton_workaround", true).toBool();
    if (ios_peloton_workaround && cadence && h && firstStateChanged) {

        h->virtualbike_setCadence(currentCrankRevolutions(), lastCrankEventTime());
        h->virtualbike_setHeartRate((uint8_t)metrics_override_heartrate());
    }
#endif
#endif

    emit debug(QStringLiteral("Current CrankRevs: ") + QString::number(CrankRevs));
    emit debug(QStringLiteral("Last CrankEventTime: ") + QString::number(LastCrankEventTime));

    if (m_control->error() != QLowEnergyController::NoError) {
        qDebug() << QStringLiteral("QLowEnergyController ERROR!!") << m_control->errorString();
    }
}

void concept2skierg::stateChanged(QLowEnergyService::ServiceState state) {

    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyService::ServiceState>();
    emit debug(QStringLiteral("BTLE stateChanged ") + QString::fromLocal8Bit(metaEnum.valueToKey(state)));

    for (QLowEnergyService *s : qAsConst(gattCommunicationChannelService)) {
        qDebug() << QStringLiteral("stateChanged") << s->serviceUuid() << s->state();
        if (s->state() != QLowEnergyService::ServiceDiscovered && s->state() != QLowEnergyService::InvalidService) {
            qDebug() << QStringLiteral("not all services discovered");

            return;
        }
    }

    qDebug() << QStringLiteral("all services discovered!");

    for (QLowEnergyService *s : qAsConst(gattCommunicationChannelService)) {
        if (s->state() == QLowEnergyService::ServiceDiscovered) {

            // establish hook into notifications
            connect(s, &QLowEnergyService::characteristicChanged, this, &concept2skierg::characteristicChanged);
            connect(s, &QLowEnergyService::characteristicWritten, this, &concept2skierg::characteristicWritten);
            connect(s, &QLowEnergyService::characteristicRead, this, &concept2skierg::characteristicRead);
            connect(
                s, static_cast<void (QLowEnergyService::*)(QLowEnergyService::ServiceError)>(&QLowEnergyService::error),
                this, &concept2skierg::errorService);
            connect(s, &QLowEnergyService::descriptorWritten, this, &concept2skierg::descriptorWritten);
            connect(s, &QLowEnergyService::descriptorRead, this, &concept2skierg::descriptorRead);

            qDebug() << s->serviceUuid() << QStringLiteral("connected!");

            auto characteristics_list = s->characteristics();
            for (const QLowEnergyCharacteristic &c : qAsConst(characteristics_list)) {
                qDebug() << "char uuid" << c.uuid() << QStringLiteral("handle") << c.handle();
                auto descriptors_list = c.descriptors();
                for (const QLowEnergyDescriptor &d : qAsConst(descriptors_list)) {
                    qDebug() << QStringLiteral("descriptor uuid") << d.uuid() << QStringLiteral("handle") << d.handle();
                }

                if ((c.properties() & QLowEnergyCharacteristic::Notify) == QLowEnergyCharacteristic::Notify) {

                    QByteArray descriptor;
                    descriptor.append((char)0x01);
                    descriptor.append((char)0x00);
                    if (c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).isValid()) {
                        s->writeDescriptor(c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration), descriptor);
                    } else {
                        qDebug() << QStringLiteral("ClientCharacteristicConfiguration") << c.uuid()
                                 << c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).uuid()
                                 << c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).handle()
                                 << QStringLiteral(" is not valid");
                    }

                    qDebug() << s->serviceUuid() << c.uuid() << QStringLiteral("notification subscribed!");
                } else if ((c.properties() & QLowEnergyCharacteristic::Indicate) ==
                           QLowEnergyCharacteristic::Indicate) {
                    QByteArray descriptor;
                    descriptor.append((char)0x02);
                    descriptor.append((char)0x00);
                    if (c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).isValid()) {
                        s->writeDescriptor(c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration), descriptor);
                    } else {
                        qDebug() << QStringLiteral("ClientCharacteristicConfiguration") << c.uuid()
                                 << c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).uuid()
                                 << c.descriptor(QBluetoothUuid::ClientCharacteristicConfiguration).handle()
                                 << QStringLiteral(" is not valid");
                    }

                    qDebug() << s->serviceUuid() << c.uuid() << QStringLiteral("indication subscribed!");
                } else if ((c.properties() & QLowEnergyCharacteristic::Read) == QLowEnergyCharacteristic::Read) {
                    // s->readCharacteristic(c);
                    // qDebug() << s->serviceUuid() << c.uuid() << "reading!";
                }

                QBluetoothUuid _gattWriteCharControlPointId((quint16)0x2AD9);
                if (c.properties() & QLowEnergyCharacteristic::Write && c.uuid() == _gattWriteCharControlPointId) {
                    qDebug() << QStringLiteral("FTMS service and Control Point found");

                    gattWriteCharControlPointId = c;
                    gattFTMSService = s;
                }
            }
        }
    }

    // ******************************************* virtual bike init *************************************
    if (!firstStateChanged && !virtualTreadmill
#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
        && !h
#endif
#endif
    ) {

        QSettings settings;
        bool virtual_device_enabled = settings.value(QStringLiteral("virtual_device_enabled"), true).toBool();
#ifdef Q_OS_IOS
#ifndef IO_UNDER_QT
        bool cadence = settings.value("bike_cadence_sensor", false).toBool();
        bool ios_peloton_workaround = settings.value("ios_peloton_workaround", true).toBool();
        if (ios_peloton_workaround && cadence) {

            qDebug() << "ios_peloton_workaround activated!";
            h = new lockscreen();
            h->virtualbike_ios();
        } else

#endif
#endif
            if (virtual_device_enabled) {
            emit debug(QStringLiteral("creating virtual bike interface..."));

            virtualTreadmill = new virtualtreadmill(this, noHeartService);
            connect(virtualTreadmill, &virtualtreadmill::debug, this, &concept2skierg::debug);
            // connect(virtualTreadmill, &virtualtreadmill::changeInclination, this,
            //        &domyostreadmill::changeInclinationRequested);
        }
    }
    firstStateChanged = 1;
    // ********************************************************************************************************
}

void concept2skierg::descriptorWritten(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue) {
    emit debug(QStringLiteral("descriptorWritten ") + descriptor.name() + " " + newValue.toHex(' '));

    initRequest = true;
    emit connectedAndDiscovered();
}

void concept2skierg::descriptorRead(const QLowEnergyDescriptor &descriptor, const QByteArray &newValue) {
    qDebug() << QStringLiteral("descriptorRead ") << descriptor.name() << descriptor.uuid() << newValue.toHex(' ');
}

void concept2skierg::characteristicWritten(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {

    Q_UNUSED(characteristic);
    emit debug(QStringLiteral("characteristicWritten ") + newValue.toHex(' '));
}

void concept2skierg::characteristicRead(const QLowEnergyCharacteristic &characteristic, const QByteArray &newValue) {
    qDebug() << QStringLiteral("characteristicRead ") << characteristic.uuid() << newValue.toHex(' ');
}

void concept2skierg::serviceScanDone(void) {
    emit debug(QStringLiteral("serviceScanDone"));

#ifdef Q_OS_ANDROID
    QLowEnergyConnectionParameters c;
    c.setIntervalRange(24, 40);
    c.setLatency(0);
    c.setSupervisionTimeout(420);
    m_control->requestConnectionUpdate(c);
#endif

    auto services_list = m_control->services();
    for (const QBluetoothUuid &s : qAsConst(services_list)) {
        gattCommunicationChannelService.append(m_control->createServiceObject(s));
        connect(gattCommunicationChannelService.constLast(), &QLowEnergyService::stateChanged, this,
                &concept2skierg::stateChanged);
        gattCommunicationChannelService.constLast()->discoverDetails();
    }
}

void concept2skierg::errorService(QLowEnergyService::ServiceError err) {

    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyService::ServiceError>();
    emit debug(QStringLiteral("concept2skierg::errorService") + QString::fromLocal8Bit(metaEnum.valueToKey(err)) +
               m_control->errorString());
}

void concept2skierg::error(QLowEnergyController::Error err) {

    QMetaEnum metaEnum = QMetaEnum::fromType<QLowEnergyController::Error>();
    emit debug(QStringLiteral("concept2skierg::error") + QString::fromLocal8Bit(metaEnum.valueToKey(err)) +
               m_control->errorString());
}

void concept2skierg::deviceDiscovered(const QBluetoothDeviceInfo &device) {
    emit debug(QStringLiteral("Found new device: ") + device.name() + QStringLiteral(" (") +
               device.address().toString() + ')');
    {
        bluetoothDevice = device;

        m_control = QLowEnergyController::createCentral(bluetoothDevice, this);
        connect(m_control, &QLowEnergyController::serviceDiscovered, this, &concept2skierg::serviceDiscovered);
        connect(m_control, &QLowEnergyController::discoveryFinished, this, &concept2skierg::serviceScanDone);
        connect(m_control,
                static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
                this, &concept2skierg::error);
        connect(m_control, &QLowEnergyController::stateChanged, this, &concept2skierg::controllerStateChanged);

        connect(m_control,
                static_cast<void (QLowEnergyController::*)(QLowEnergyController::Error)>(&QLowEnergyController::error),
                this, [this](QLowEnergyController::Error error) {
                    Q_UNUSED(error);
                    Q_UNUSED(this);
                    emit debug(QStringLiteral("Cannot connect to remote device."));
                    emit disconnected();
                });
        connect(m_control, &QLowEnergyController::connected, this, [this]() {
            Q_UNUSED(this);
            emit debug(QStringLiteral("Controller connected. Search services..."));
            m_control->discoverServices();
        });
        connect(m_control, &QLowEnergyController::disconnected, this, [this]() {
            Q_UNUSED(this);
            emit debug(QStringLiteral("LowEnergy controller disconnected"));
            emit disconnected();
        });

        // Connect
        m_control->connectToDevice();
        return;
    }
}

bool concept2skierg::connected() {
    if (!m_control) {

        return false;
    }
    return m_control->state() == QLowEnergyController::DiscoveredState;
}

void *concept2skierg::VirtualTreadmill() { return virtualTreadmill; }

void *concept2skierg::VirtualDevice() { return VirtualTreadmill(); }

uint16_t concept2skierg::watts() {
    if (currentCadence().value() == 0) {
        return 0;
    }

    return m_watt.value();
}

void concept2skierg::controllerStateChanged(QLowEnergyController::ControllerState state) {
    qDebug() << QStringLiteral("controllerStateChanged") << state;
    if (state == QLowEnergyController::UnconnectedState && m_control) {
        qDebug() << QStringLiteral("trying to connect back again...");

        initDone = false;
        m_control->connectToDevice();
    }
}
