#include "toorxtreadmill.h"
#include <QMetaEnum>
#include <QSettings>
#include <QBluetoothLocalDevice>

toorxtreadmill::toorxtreadmill()
{
    m_watt.setType(metric::METRIC_WATT);
    refresh = new QTimer(this);
    initDone = false;
    connect(refresh, SIGNAL(timeout()), this, SLOT(update()));
    refresh->start(1000);
}

void toorxtreadmill::deviceDiscovered(const QBluetoothDeviceInfo &device)
{
    debug("Found new device: " + device.name() + " (" + device.address().toString() + ')');
    if(device.name().startsWith("TRX ROUTE KEY"))
    {
        bluetoothDevice = device;

        // Create a discovery agent and connect to its signals
        discoveryAgent = new QBluetoothServiceDiscoveryAgent(this);
        connect(discoveryAgent, SIGNAL(serviceDiscovered(QBluetoothServiceInfo)),
                this, SLOT(serviceDiscovered(QBluetoothServiceInfo)));

        // Start a discovery
        qDebug() << "toorxtreadmill::deviceDiscovered";
        discoveryAgent->start(QBluetoothServiceDiscoveryAgent::FullDiscovery);
        return;
    }
}

// In your local slot, read information about the found devices
void toorxtreadmill::serviceDiscovered(const QBluetoothServiceInfo &service)
{
    // this treadmill has more serial port, just the first one is the right one.
    if(socket != nullptr)
    {
        qDebug() << "toorxtreadmill::serviceDiscovered socket already initialized";
        return;
    }

    qDebug() << "toorxtreadmill::serviceDiscovered" << service;
    if(service.device().address() == bluetoothDevice.address())
    {
        debug("Found new service: " + service.serviceName()
                 + '(' + service.serviceUuid().toString() + ')');

        if(service.serviceName().startsWith("SerialPort") || service.serviceName().startsWith("Serial Port"))
        {
            debug("Serial port service found");
            discoveryAgent->stop();

            serialPortService = service;
            socket = new QBluetoothSocket(QBluetoothServiceInfo::RfcommProtocol);

            connect(socket, &QBluetoothSocket::readyRead, this, &toorxtreadmill::readSocket);
            connect(socket, &QBluetoothSocket::connected, this, QOverload<>::of(&toorxtreadmill::rfCommConnected));
            connect(socket, &QBluetoothSocket::disconnected, this, &toorxtreadmill::disconnected);
            connect(socket, QOverload<QBluetoothSocket::SocketError>::of(&QBluetoothSocket::error),
                    this, &toorxtreadmill::onSocketErrorOccurred);

#ifdef Q_OS_ANDROID
            socket->setPreferredSecurityFlags(QBluetooth::NoSecurity);
#endif

            debug("Create socket");
            socket->connectToService(serialPortService);
            debug("ConnectToService done");
        }
    }
}

void toorxtreadmill::update()
{
    static int8_t start_phase = -1;

    if(initDone)
    {
        // ******************************************* virtual treadmill init *************************************
        if(!virtualTreadMill)
        {
            QSettings settings;
            bool virtual_device_enabled = settings.value("virtual_device_enabled", true).toBool();
            if(virtual_device_enabled)
            {
                debug("creating virtual treadmill interface...");
                virtualTreadMill = new virtualtreadmill(this, true);
                connect(virtualTreadMill,&virtualtreadmill::debug ,this,&toorxtreadmill::debug);
            }
        }
        // ********************************************************************************************************

        const char poll[] = {0x55, 0x17, 0x01, 0x01};
        socket->write(poll, sizeof(poll));
        debug("write poll");

        if(requestStart != -1)
        {
           debug("starting...");
           const char start[] = {0x55, 0x0a, 0x01, 0x02};
           socket->write(start, sizeof(start));
           start_phase = 0;
           requestStart = -1;
           emit tapeStarted();
        }
        else if(start_phase != -1)
        {
            switch (start_phase) {
            case 0:
            {
                const char start[] = {0x55, 0x01, 0x06, 0x1d, 0x00, 0x3c, 0x00, 0xaa, 0x00};
                socket->write(start, sizeof(start));
                start_phase++;
                break;
            }
            case 1:
            {
                const char start1[] = {0x55, 0x15, 0x01, 0x00};
                socket->write(start1, sizeof(start1));
                start_phase++;
                break;
            }
            case 2:
            {
                const char start2[] = {0x55, 0x0f, 0x02, 0x01, 0x00};
                socket->write(start2, sizeof(start2));
                start_phase++;
                break;
            }
            case 3:
            {
                const char start3[] = {0x55, 0x11, 0x01, 0x01};
                socket->write(start3, sizeof(start3));
                start_phase++;
                break;
            }
            case 4:
            {
                const char start4[] = {0x55, 0x08, 0x01, 0x01};
                socket->write(start4, sizeof(start4));
                start_phase = -1;
                break;
            }
            }
            qDebug() << " start phase " << start_phase;
        }
    }
}

void toorxtreadmill::rfCommConnected()
{
    debug("connected " + socket->peerName());

    const char init1[] = {0x55, 0x0c, 0x01, 0xff,
                          0x55, 0xbb, 0x01, 0xff,
                          0x55, 0x24, 0x01, 0xff};
    const char init2[] = {0x55, 0x25, 0x01, 0xff,
                          0x55, 0x26, 0x01, 0xff,
                          0x55, 0x27, 0x01, 0xff,
                          0x55, 0x02, 0x01, 0xff,
                          0x55, 0x03, 0x01, 0xff,
                          0x55, 0x04, 0x01, 0xff,
                          0x55, 0x06, 0x01, 0xff,
                          0x55, 0x1f, 0x01, 0xff,
                          0x55, 0xa0, 0x01, 0xff,
                          0x55, 0xb0, 0x01, 0xff,
                          0x55, 0xb2, 0x01, 0xff,
                          0x55, 0xb3, 0x01, 0xff,
                          0x55, 0xb4, 0x01, 0xff,
                          0x55, 0xb5, 0x01, 0xff,
                          0x55, 0xb6, 0x01, 0xff,
                          0x55, 0xb7, 0x01, 0xff,
                          0x55, 0xb8, 0x01, 0xff,
                          0x55, 0xb9, 0x01, 0xff,
                          0x55, 0xba, 0x01, 0xff,
                          0x55, 0x0b, 0x01, 0xff,
                          0x55, 0x18, 0x01, 0xff,
                          0x55, 0x19, 0x01, 0xff,
                          0x55, 0x1a, 0x01, 0xff,
                          0x55, 0x1b, 0x01, 0xff};

    socket->write(init1, sizeof(init1));
    qDebug() << " init1 write";
    socket->write(init2, sizeof(init2));
    qDebug() << " init2 write";
    initDone = true;
    emit connectedAndDiscovered();
}

void toorxtreadmill::readSocket()
{
    if (!socket)
        return;

    while (socket->bytesAvailable()) {
        QByteArray line = socket->readAll();
        qDebug() << " << " + line.toHex(' ');

        if(line.length() == 17)
        {
            elapsed = GetElapsedTimeFromPacket(line);
            Distance = GetDistanceFromPacket(line);
            KCal = GetCaloriesFromPacket(line);
            Speed = GetSpeedFromPacket(line);
            Inclination = GetInclinationFromPacket(line);
            Heart = GetHeartRateFromPacket(line);
        }
        else if(line.length() == 13)
        {
            const char init3[] = {0x55, 0x17, 0x01, 0x01,
                                  0x55, 0xb5, 0x01, 0xff};

            socket->write(init3, sizeof(init3));
        }
    }
}

uint8_t toorxtreadmill::GetHeartRateFromPacket(QByteArray packet)
{
    return packet.at(16);
}

uint8_t toorxtreadmill::GetInclinationFromPacket(QByteArray packet)
{
    return packet.at(15);
}

double toorxtreadmill::GetSpeedFromPacket(QByteArray packet)
{
    double convertedData = ((double)((double)((uint8_t)packet.at(13)) * 100.0) + ((double)packet.at(14))) / 100.0;
    return convertedData;
}

uint16_t toorxtreadmill::GetCaloriesFromPacket(QByteArray packet)
{
    uint16_t convertedData = (packet.at(11) << 8) | packet.at(12);
    return convertedData;
}


uint16_t toorxtreadmill::GetDistanceFromPacket(QByteArray packet)
{
    uint16_t convertedData = (packet.at(9) << 8) | packet.at(10);
    return convertedData;
}


uint16_t toorxtreadmill::GetElapsedTimeFromPacket(QByteArray packet)
{
    uint16_t convertedData = (packet.at(7) << 8) | packet.at(8);
    return convertedData;
}

void toorxtreadmill::onSocketErrorOccurred(QBluetoothSocket::SocketError error)
{
    debug("onSocketErrorOccurred " + QString::number(error));
}
