#pragma once

#include <QObject>
#include <QStringList>
#include <QUdpSocket>
#include <QTimer>
#include <QDataStream>
#include <QHostAddress>
#include <QThread>

namespace CastIt
{

	class DeviceDiscovery : public QObject
	{
		Q_OBJECT

	public:
		explicit DeviceDiscovery(QObject* parent = nullptr); 
		~DeviceDiscovery() override;

		void startDiscovery();

	signals:
		void devicesUpdated(const QStringList& devices);
		void discoveryError(const QString& errorMessage);

	private slots:
		void sendQuery();
		void processResponse();

	private:
		// Network setup methods
		void printNetworkInterfaces();
		void joinMulticastGroups();
		QHostAddress getLocalAddress();
		
		// DNS packet encoding/decoding methods
		void sendMdnsQuery(const QString& serviceType);
		void encodeDnsName(QDataStream& stream, const QString& name);
		void parseDnsResponse(const QByteArray& data, const QHostAddress& sender);
		QString readDnsName(QDataStream& stream, const QByteArray& packet);
		void skipDnsName(QDataStream& stream, const QByteArray& packet);
		
		// Helper methods for device identification
		bool isCastingService(const QString& name, const QString& serviceName) const;
		QString extractDeviceName(const QString& serviceName) const;

		// Member variables
		QUdpSocket* udpSocket;
		QTimer* queryTimer;
		QStringList discoveredDevices;
		int queryCount = 0;
	};
}