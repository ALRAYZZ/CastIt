#include "device_discovery.h"
#include <QTimer>



namespace CastIt
{
	DeviceDiscovery::DeviceDiscovery(QObject* parent) : QObject(parent) {}

	DeviceDiscovery::~DeviceDiscovery() = default;

	void DeviceDiscovery::startDiscovery()
	{
		// Simulate mDNS discovery with a delay
		QTimer::singleShot(1000, this, &DeviceDiscovery::simulateDiscovery);
	}

	// Mock function to simulate mDNS discovery
	void DeviceDiscovery::simulateDiscovery()
	{
		QStringList devices = {
			"Chromecast Living Room",
			"Chromecast Bedroom",
			"Chromecast Kitchen"
		};
		emit devicesUpdated(devices);
	}
}
