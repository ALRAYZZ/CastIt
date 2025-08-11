#include "device_discovery.h"
#include <QTimer>
#include <QNetworkInterface>
#include <QVariant>
#include <QHostAddress>
#include <QDataStream>
#include <QBuffer>
#include <QThread>
#include <QDebug>


namespace CastIt
{
    // device_discovery.cpp

    DeviceDiscovery::DeviceDiscovery(QObject* parent)
        : QObject(parent),
        udpSocket(new QUdpSocket(this)),
        queryTimer(new QTimer(this)),
        discoveryThread(new QThread(this))
    {
        moveToThread(discoveryThread);

        connect(discoveryThread, &QThread::started, [this]()
            {
                printNetworkInterfaces();

                if (!udpSocket->bind(QHostAddress::AnyIPv4, 5353,
                    QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
                {
                    QString errorMessage = "Failed to bind UDP socket for mDNS: " + udpSocket->errorString();
                    qDebug() << errorMessage;
                    emit discoveryError(errorMessage);
                    return;
                }

                udpSocket->setSocketOption(QAbstractSocket::MulticastTtlOption,
                    QVariant::fromValue<quint32>(255));
                udpSocket->setSocketOption(QAbstractSocket::MulticastLoopbackOption,
                    QVariant::fromValue<bool>(true));

                QHostAddress local = getLocalAddress();
                if (!local.isNull() && local != QHostAddress::LocalHost) {
                    QList<QNetworkInterface> ifs = QNetworkInterface::allInterfaces();
                    for (const QNetworkInterface& iface : ifs) {
                        for (const QNetworkAddressEntry& entry : iface.addressEntries()) {
                            if (entry.ip() == local) {
                                udpSocket->setMulticastInterface(iface);
                                qDebug() << "Set multicast interface to" << iface.name();
                                break;
                            }
                        }
                    }
                }

                joinMulticastGroups();

                connect(udpSocket, &QUdpSocket::readyRead, this, &DeviceDiscovery::processResponse);
                connect(queryTimer, &QTimer::timeout, this, &DeviceDiscovery::sendQuery);

                qDebug() << "DeviceDiscovery initialized successfully";
            });
    }

    DeviceDiscovery::~DeviceDiscovery()
    {
        stopDiscovery();
    }

    void DeviceDiscovery::startDiscovery()
    {
        discoveryThread->start();
        queryTimer->start(2000);
    }

    void DeviceDiscovery::stopDiscovery()
    {
        queryTimer->stop();
        if (discoveryThread->isRunning()) {
            discoveryThread->quit();
            discoveryThread->wait();
        }
    }

    void DeviceDiscovery::sendQuery()
    {
        QStringList serviceTypes = {
            "_googlecast._tcp.local.",
            "_airplay._tcp.local."
        };

        sendServiceQueriesWithDelay(serviceTypes, 0);
    }


    void DeviceDiscovery::sendMdnsQuery(const QString& serviceType, quint16 qtype)
    {
        QByteArray query;
        QDataStream stream(&query, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::BigEndian);

        stream << (quint16)0 << (quint16)0x0000 << (quint16)1 << (quint16)0
            << (quint16)0 << (quint16)0;

        const QStringList labels = serviceType.split('.', Qt::SkipEmptyParts);
        for (const QString& label : labels) {
            QByteArray labelUtf8 = label.toUtf8();
            stream << (quint8)labelUtf8.size();
            stream.writeRawData(labelUtf8.data(), labelUtf8.size());
        }
        stream << (quint8)0;
        stream << qtype << (quint16)1;

        qDebug() << "Sending mDNS query for" << serviceType << "qtype" << qtype;
        qDebug() << "Outgoing mDNS packet (hex):" << query.toHex();

        udpSocket->writeDatagram(query, QHostAddress("224.0.0.251"), 5353);
    }

	void DeviceDiscovery::sendServiceQueriesWithDelay(const QStringList& list, int index)
	{
		if (index >= list.size())
		{
			sendInstanceQueriesWithDelay(0);
			return;
		}

		sendMdnsQuery(list[index], 12); // Default PTR query

		// Async timer to avoid blocking the event loop
        QTimer::singleShot(100, this, [this, list, index]()
            {
                sendServiceQueriesWithDelay(list, index + 1);
            });
	}

    void DeviceDiscovery::sendInstanceQueriesWithDelay(int index)
    {
        if (index >= discoveredDevices.size()) return;

        const QString& instance = discoveredDevices[index];
        sendMdnsQuery(instance, 33);

		// SingleShot async timer to avoid blocking the event loop
        QTimer::singleShot(50, this, [this, instance, index]() {
            sendMdnsQuery(instance, 16);

            QTimer::singleShot(50, this, [this, index]() {
                sendInstanceQueriesWithDelay(index + 1);
                });
            });
    }
    void DeviceDiscovery::processResponse()
    {
        while (udpSocket->hasPendingDatagrams()) {
            QByteArray datagram;
            datagram.resize(udpSocket->pendingDatagramSize());
            QHostAddress sender;
            quint16 senderPort;
            udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

            QHostAddress localAddr = getLocalAddress();
            if (sender == localAddr || datagram.size() < 20) {
                qDebug() << "Skipping response from" << sender.toString();
                continue;
            }

            qDebug() << "Received mDNS response from" << sender.toString();
            qDebug() << "Incoming datagram (hex):" << datagram.toHex();

            parseDnsResponse(datagram, sender);
        }
    }

    void DeviceDiscovery::parseDnsResponse(const QByteArray& data, const QHostAddress& sender)
    {
        QBuffer buffer;
        buffer.setData(data);
        buffer.open(QIODevice::ReadOnly);
        QDataStream stream(&buffer);
        stream.setByteOrder(QDataStream::BigEndian);

        quint16 transactionId, flags, qdCount, anCount, nsCount, arCount;
        stream >> transactionId >> flags >> qdCount >> anCount >> nsCount >> arCount;

		std::function<QString(QDataStream&, const QByteArray&)> readDnsName;

        readDnsName = [&](QDataStream& s, const QByteArray& packet) -> QString {
            QString name;
            quint8 len;
            while (true) {
                s >> len;
                if (len == 0) break;
                if ((len & 0xC0) == 0xC0) {
                    quint8 offsetByte;
                    s >> offsetByte;
                    quint16 offset = ((len & 0x3F) << 8) | offsetByte;
                    QDataStream subStream(packet.mid(offset));
                    subStream.setByteOrder(QDataStream::BigEndian);
                    name += readDnsName(subStream, packet);
                    break;
                }
                QByteArray label(len, 0);
                s.readRawData(label.data(), len);
                name += QString::fromUtf8(label) + ".";
            }
            return name;
            };

        auto skipQuestions = [&](int count) {
            for (int i = 0; i < count; ++i) {
                readDnsName(stream, data);
                quint16 qtype, qclass;
                stream >> qtype >> qclass;
            }
            };
        skipQuestions(qdCount);

        int totalRecords = anCount + nsCount + arCount;
        for (int i = 0; i < totalRecords; ++i) {
            QString name = readDnsName(stream, data);
            quint16 type, rclass, rdlength;
            quint32 ttl;
            stream >> type >> rclass >> ttl >> rdlength;

            if (type == 12) { // PTR
                QString serviceName = readDnsName(stream, data);
                qDebug() << "PTR ->" << serviceName;
                if (isCastingService(name, serviceName)) {
                    QString deviceName = extractDeviceName(serviceName);
                    if (!deviceName.isEmpty() && !discoveredDevices.contains(deviceName)) {
                        discoveredDevices.append(deviceName);
                        qDebug() << "*** Device:" << deviceName << "at" << sender.toString();
                        emit devicesUpdated(discoveredDevices);
                    }
                    sendMdnsQuery(serviceName, 33);
                    sendMdnsQuery(serviceName, 16);
                }
            }
            else if (type == 33) { // SRV
                quint16 priority, weight, port;
                stream >> priority >> weight >> port;
                QString target = readDnsName(stream, data);
                qDebug() << "SRV -> target:" << target << "port:" << port;
                sendMdnsQuery(target, 1);
                sendMdnsQuery(target, 28);
            }
            else if (type == 1) { // A
                QByteArray addrBytes(rdlength, 0);
                stream.readRawData(addrBytes.data(), rdlength);
                if (addrBytes.size() == 4) {
                    QString ipStr = QString("%1.%2.%3.%4")
                        .arg((quint8)addrBytes[0])
                        .arg((quint8)addrBytes[1])
                        .arg((quint8)addrBytes[2])
                        .arg((quint8)addrBytes[3]);
                    qDebug() << "A ->" << ipStr;
                }
            }
            else if (type == 16) { // TXT
                QByteArray txtData(rdlength, 0);
                stream.readRawData(txtData.data(), rdlength);
                QList<QString> txtEntries;
                int pos = 0;
                while (pos < txtData.size()) {
                    quint8 len = (quint8)txtData.at(pos++);
                    txtEntries.append(QString::fromUtf8(txtData.mid(pos, len)));
                    pos += len;
                }
                qDebug() << "TXT ->" << txtEntries;
            }
            else {
                stream.skipRawData(rdlength);
            }
        }
    }

    void DeviceDiscovery::printNetworkInterfaces()
    {
        // Print available network interfaces for debugging
        const auto ifs = QNetworkInterface::allInterfaces();
        for (const auto& iface : ifs) {
            qDebug() << "Interface:" << iface.humanReadableName();
        }
    }

    void DeviceDiscovery::joinMulticastGroups()
    {
        // Join multicast groups on all IPv4 interfaces
        const auto ifs = QNetworkInterface::allInterfaces();
        for (const auto& iface : ifs) {
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    udpSocket->joinMulticastGroup(QHostAddress("224.0.0.251"), iface);
                    qDebug() << "Joined multicast group on interface:" << iface.humanReadableName();
                }
            }
        }
    }

    QHostAddress DeviceDiscovery::getLocalAddress() const
    {
        // Return first non-localhost IPv4 address
        const auto ifs = QNetworkInterface::allInterfaces();
        for (const auto& iface : ifs) {
            for (const auto& entry : iface.addressEntries()) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
                    entry.ip() != QHostAddress::LocalHost) {
                    return entry.ip();
                }
            }
        }
        return QHostAddress();
    }

    bool DeviceDiscovery::isCastingService(const QString& serviceType, const QString& serviceName) const
    {
        // Check if the serviceType matches known casting services
        return serviceType.startsWith("_googlecast._tcp.local.") || serviceType.startsWith("_airplay._tcp.local.");
    }

    QString DeviceDiscovery::extractDeviceName(const QString& fullName) const
    {
        // Extract device name from full service name, e.g. "MyDevice._googlecast._tcp.local." => "MyDevice"
        QStringList parts = fullName.split('.');
        if (!parts.isEmpty()) {
            return parts.first();
        }
        return QString();
    }



} // namespace CastIt
