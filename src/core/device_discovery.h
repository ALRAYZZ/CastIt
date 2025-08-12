#pragma once

#include <QObject>
#include <QStringList>
#include <QUdpSocket>
#include <QTimer>
#include <QDataStream>
#include <QHostAddress>
#include <QThread>
#include <QMap>

namespace CastIt
{
    class DeviceDiscovery : public QObject
    {
        Q_OBJECT
    public:
        explicit DeviceDiscovery(QObject* parent = nullptr);
        ~DeviceDiscovery();

        void startDiscovery();
        void stopDiscovery();

    signals:
        void devicesUpdated(const QStringList& devices);
        void discoveryError(const QString& error);
        void deviceIpsUpdated(const QMap<QString, QHostAddress>& deviceIps);

    private slots:
        void onDiscoveryThreadStarted();
        void sendQuery();
        void processResponse();

		// Make them slots so QTimer::singleShot can invoke them
        void sendServiceQueriesWithDelay(const QStringList& list, int index);
        void sendInstanceQueriesWithDelay(int index);

    private:
        QUdpSocket* udpSocket;
        QTimer* queryTimer;
        QThread* discoveryThread;
        QStringList discoveredDevices;
        QMap<QString, QHostAddress> deviceIps;

        void sendMdnsQuery(const QString& serviceType, quint16 qtype = 12); // Default PTR
        void parseDnsResponse(const QByteArray& data, const QHostAddress& sender);


        void joinMulticastGroups();
        void printNetworkInterfaces();
        QHostAddress getLocalAddress() const;
        bool isCastingService(const QString& name, const QString& serviceName) const;
        QString extractDeviceName(const QString& serviceName) const;
    };
}