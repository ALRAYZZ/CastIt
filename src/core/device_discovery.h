#pragma once

#include <QObject>
#include <QStringList>

namespace CastIt
{

	class DeviceDiscovery : public QObject
	{
		Q_OBJECT


	public:
		// Note to self: explicit is use to prevent implicit converstion, must provide the correct data type for its parameters
		explicit DeviceDiscovery(QObject* parent = nullptr); 
		~DeviceDiscovery() override;

		void startDiscovery(); // Starts mDNS discovery

	signals:
		void devicesUpdated(const QStringList& devices); // Emits updated device list

	private:
		void simulateDiscovery(); // Mock mDNS for testing
	};
}