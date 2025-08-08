#include "device_discovery.h"
#include <QTimer>
#include <QDebug>
#include <QNetworkInterface>


namespace CastIt
{
	DeviceDiscovery::DeviceDiscovery(QObject* parent) : QObject(parent), udpSocket(nullptr), queryTimer(nullptr)
	{
		udpSocket = new QUdpSocket(this);
		queryTimer = new QTimer(this);

		// Print network interfaces for debugging
		printNetworkInterfaces();

		// Bind to any address on port 5353 for mDNS
		if (!udpSocket->bind(QHostAddress::AnyIPv4, 5353, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint))
		{
			QString errorMessage = "Failed to bind UDP socket for mDNS: " + udpSocket->errorString();
			qDebug() << errorMessage;
			emit discoveryError(errorMessage);
			return;
		}

		// Join multicast group for mDNS on all suitable interfaces
		joinMulticastGroups();

		connect(udpSocket, &QUdpSocket::readyRead, this, &DeviceDiscovery::processResponse);
		connect(queryTimer, &QTimer::timeout, this, &DeviceDiscovery::sendQuery);
		
		qDebug() << "DeviceDiscovery initialized successfully";
	}

	DeviceDiscovery::~DeviceDiscovery()
	{
		if (udpSocket)
		{
			udpSocket->leaveMulticastGroup(QHostAddress("224.0.0.251"));
		}
		delete udpSocket;
		delete queryTimer;
	}

	void DeviceDiscovery::printNetworkInterfaces()
	{
		qDebug() << "Available network interfaces:";
		QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
		for (const QNetworkInterface &interface : interfaces) {
			if (interface.flags() & QNetworkInterface::IsUp && 
				interface.flags() & QNetworkInterface::IsRunning &&
				!(interface.flags() & QNetworkInterface::IsLoopBack)) {
				
				qDebug() << "Interface:" << interface.name() << "(" << interface.humanReadableName() << ")";
				QList<QNetworkAddressEntry> entries = interface.addressEntries();
				for (const QNetworkAddressEntry &entry : entries) {
					if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
						qDebug() << "  IPv4:" << entry.ip().toString() << "Netmask:" << entry.netmask().toString();
					}
				}
			}
		}
	}

	void DeviceDiscovery::joinMulticastGroups()
	{
		QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
		bool joinedAtLeastOne = false;
		
		for (const QNetworkInterface &interface : interfaces) {
			if (interface.flags() & QNetworkInterface::IsUp && 
				interface.flags() & QNetworkInterface::IsRunning &&
				!(interface.flags() & QNetworkInterface::IsLoopBack) &&
				interface.flags() & QNetworkInterface::CanMulticast) {
				
				// Try to join multicast group on this interface
				if (udpSocket->joinMulticastGroup(QHostAddress("224.0.0.251"), interface)) {
					qDebug() << "Joined mDNS multicast group on interface:" << interface.name();
					joinedAtLeastOne = true;
				} else {
					qDebug() << "Failed to join mDNS multicast group on interface:" << interface.name() << udpSocket->errorString();
				}
			}
		}
		
		if (!joinedAtLeastOne) {
			// Fallback to default interface
			if (udpSocket->joinMulticastGroup(QHostAddress("224.0.0.251"))) {
				qDebug() << "Joined mDNS multicast group on default interface";
			} else {
				QString errorMessage = "Failed to join mDNS multicast group: " + udpSocket->errorString();
				qDebug() << errorMessage;
				emit discoveryError(errorMessage);
			}
		}
	}

	void DeviceDiscovery::startDiscovery()
	{
		discoveredDevices.clear();
		queryCount = 0;
		
		// Start with passive listening for a few seconds
		qDebug() << "Starting passive mDNS listening for 10 seconds...";
		
		// Send initial queries after a short delay to first listen for existing traffic
		QTimer::singleShot(2000, this, &DeviceDiscovery::sendQuery);
		
		// Set up periodic queries every 8 seconds (less aggressive)
		queryTimer->start(8000);
		
		qDebug() << "Started device discovery";
	}

	void DeviceDiscovery::sendQuery()
	{
		queryCount++;
		qDebug() << "Sending mDNS queries - attempt" << queryCount;
		
		// Filter sender IP to avoid processing our own queries
		QHostAddress localAddress = getLocalAddress();
		qDebug() << "Local address for filtering:" << localAddress.toString();
		
		// Send multiple types of queries for better discovery
		QStringList serviceTypes = {
			"_googlecast._tcp.local",
			"_chromecast._tcp.local", 
			"_casting._tcp.local",
			"_googlezone._tcp.local",
			"_airplay._tcp.local",  // Also try AirPlay devices
			"_services._dns-sd._udp.local"
		};

		for (const QString& serviceType : serviceTypes) {
			sendMdnsQuery(serviceType);
			// Small delay between queries to avoid overwhelming the network
			QThread::msleep(100);
		}
		
		// Stop after 8 attempts (about 1 minute total)
		if (queryCount >= 8) {
			queryTimer->stop();
			qDebug() << "Discovery timeout reached after" << queryCount << "attempts";
		}
	}

	QHostAddress DeviceDiscovery::getLocalAddress()
	{
		QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
		for (const QNetworkInterface &interface : interfaces) {
			if (interface.flags() & QNetworkInterface::IsUp && 
				interface.flags() & QNetworkInterface::IsRunning &&
				!(interface.flags() & QNetworkInterface::IsLoopBack)) {
				
				QList<QNetworkAddressEntry> entries = interface.addressEntries();
				for (const QNetworkAddressEntry &entry : entries) {
					if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol &&
						!entry.ip().isLoopback() && 
						entry.ip().toString().startsWith("192.168.") || 
						entry.ip().toString().startsWith("10.") ||
						entry.ip().toString().startsWith("172.")) {
						return entry.ip();
					}
				}
			}
		}
		return QHostAddress::LocalHost;
	}

	void DeviceDiscovery::sendMdnsQuery(const QString& serviceType)
	{
		QByteArray query;
		QDataStream stream(&query, QIODevice::WriteOnly);
		stream.setByteOrder(QDataStream::BigEndian);

		// DNS header with proper mDNS flags
		stream << quint16(0);        // Transaction ID
		stream << quint16(0x0000);   // Flags: standard query, recursion desired
		stream << quint16(1);        // Questions count
		stream << quint16(0);        // Answer RRs
		stream << quint16(0);        // Authority RRs
		stream << quint16(0);        // Additional RRs

		// Query name
		encodeDnsName(stream, serviceType);
		
		stream << quint16(12);       // Type: PTR
		stream << quint16(0x8001);   // Class: IN with cache flush bit for mDNS

		qint64 sent = udpSocket->writeDatagram(query, QHostAddress("224.0.0.251"), 5353);
		if (sent == -1) {
			qDebug() << "Failed to send mDNS query for" << serviceType << ":" << udpSocket->errorString();
		} else {
			qDebug() << "Sent mDNS query for" << serviceType << "(" << sent << "bytes)";
		}
	}

	void DeviceDiscovery::encodeDnsName(QDataStream& stream, const QString& name)
	{
		QStringList labels = name.split(".");
		for (const QString& label : labels)
		{
			if (!label.isEmpty())
			{
				QByteArray labelBytes = label.toUtf8();
				stream << quint8(labelBytes.length());
				stream.writeRawData(labelBytes.constData(), labelBytes.length());
			}
		}
		stream << quint8(0);
	}
	
	void DeviceDiscovery::processResponse()
	{
		while (udpSocket->hasPendingDatagrams())
		{
			QByteArray datagram;
			datagram.resize(udpSocket->pendingDatagramSize());
			QHostAddress sender;
			quint16 port;

			qint64 size = udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &port);

			if (size == -1)
			{
				qDebug() << "Failed to read mDNS response:" << udpSocket->errorString();
				continue;
			}

			// Skip responses from known gateway/router addresses and our own queries
			if (sender.toString() == "172.29.224.1" || 
				sender.toString().endsWith(".1") ||
				size < 50) { // Skip very small packets (likely our own queries)
				qDebug() << "Skipping response from" << sender.toString() << "(likely gateway or own query)";
				continue;
			}

			qDebug() << "Processing mDNS response from" << sender.toString() << "port" << port << "size" << size;

			// Parse DNS response
			parseDnsResponse(datagram, sender);
		}
	}

	void DeviceDiscovery::parseDnsResponse(const QByteArray& data, const QHostAddress& sender)
	{
		if (data.size() < 12)
		{
			qDebug() << "DNS response too short";
			return;
		}

		QDataStream stream(data);
		stream.setByteOrder(QDataStream::BigEndian);

		// Read DNS header
		quint16 transactionId, flags, questions, answers, authority, additional;
		stream >> transactionId >> flags >> questions >> answers >> authority >> additional;

		qDebug() << "DNS Response from" << sender.toString() << "- ID:" << transactionId 
				<< "Flags:" << QString::number(flags, 16) 
				<< "Q:" << questions << "A:" << answers << "Auth:" << authority << "Add:" << additional;

		// Only process responses that have actual data
		if (answers == 0 && authority == 0 && additional == 0) {
			qDebug() << "Empty response, skipping";
			return;
		}

		// Skip questions section
		for (int i = 0; i < questions; ++i)
		{
			skipDnsName(stream, data);
			quint16 qtype, qclass;
			stream >> qtype >> qclass;
		}

		// Process all sections (answers, authority, additional)
		int totalRecords = answers + authority + additional;
		for (int i = 0; i < totalRecords && !stream.atEnd(); ++i)
		{
			QString name = readDnsName(stream, data);
			quint16 type, rclass, rdlength;
			quint32 ttl;

			if (stream.atEnd()) break;
			stream >> type >> rclass >> ttl >> rdlength;

			qDebug() << "Record" << i+1 << ":" << name << "Type:" << type << "Class:" << rclass << "Length:" << rdlength;

			// Look for casting-related services
			if (type == 12) { // PTR record
				QString serviceName = readDnsName(stream, data);
				qDebug() << "PTR record points to:" << serviceName;
				
				if (isCastingService(name, serviceName)) {
					QString deviceName = extractDeviceName(serviceName);
					if (!deviceName.isEmpty() && !discoveredDevices.contains(deviceName)) {
						discoveredDevices.append(deviceName);
						qDebug() << "*** DISCOVERED CASTING DEVICE:" << deviceName << "at" << sender.toString();
						emit devicesUpdated(discoveredDevices);
					}
				}
			}
			else if (type == 16) { // TXT record - contains device info
				QByteArray txtData(rdlength, 0);
				stream.readRawData(txtData.data(), rdlength);
				qDebug() << "TXT record for" << name << ":" << txtData;
				
				if (isCastingService(name, name)) {
					QString deviceName = extractDeviceName(name);
					if (!deviceName.isEmpty() && !discoveredDevices.contains(deviceName)) {
						discoveredDevices.append(deviceName);
						qDebug() << "*** DISCOVERED CASTING DEVICE (TXT):" << deviceName << "at" << sender.toString();
						emit devicesUpdated(discoveredDevices);
					}
				}
			}
			else {
				// Skip other record types
				stream.skipRawData(rdlength);
			}
		}
	}

	bool DeviceDiscovery::isCastingService(const QString& name, const QString& serviceName) const
	{
		QStringList castingKeywords = {
			"googlecast", "chromecast", "casting", "googlezone", 
			"cast", "google", "chrome", "airplay"
		};
		
		QString combined = (name + " " + serviceName).toLower();
		for (const QString& keyword : castingKeywords) {
			if (combined.contains(keyword)) {
				return true;
			}
		}
		return false;
	}

	QString DeviceDiscovery::extractDeviceName(const QString& serviceName) const
	{
		QStringList parts = serviceName.split('.');
		if (!parts.isEmpty()) {
			QString deviceName = parts.first().trimmed();
			
			// Clean up common prefixes/suffixes
			if (deviceName.startsWith("Chromecast-")) {
				return deviceName;
			}
			
			return deviceName;
		}
		
		return QString();
	}

	QString DeviceDiscovery::readDnsName(QDataStream& stream, const QByteArray& packet)
	{
		QString name;
		QSet<int> visitedOffsets;

		while (!stream.atEnd())
		{
			quint8 length;
			stream >> length;

			if (length == 0)
			{
				break;
			}
			else if ((length & 0xC0) == 0xC0)
			{
				quint8 offset2;
				stream >> offset2;
				int offset = ((length & 0x3F) << 8) | offset2;

				if (visitedOffsets.contains(offset) || offset >= packet.size())
				{
					qDebug() << "Invalid DNS name compression pointer at offset" << offset;
					break;
				}
				visitedOffsets.insert(offset);

				int currentPos = stream.device()->pos();
				stream.device()->seek(offset);

				QString compressedPart = readDnsName(stream, packet);
				if (!name.isEmpty() && !compressedPart.isEmpty())
				{
					name += "." + compressedPart;
				}
				else if (!compressedPart.isEmpty())
				{
					name = compressedPart;
				}

				stream.device()->seek(currentPos);
				break;
			}
			else
			{
				if (length > 63 || stream.device()->pos() + length > packet.size())
				{
					qDebug() << "Invalid DNS label length:" << length;
					break;
				}

				QByteArray label(length, 0);
				stream.readRawData(label.data(), length);

				if (!name.isEmpty())
				{
					name += ".";
				}
				name += QString::fromUtf8(label);
			}
		}

		return name;
	}

	void DeviceDiscovery::skipDnsName(QDataStream& stream, const QByteArray& packet)
	{
		while (!stream.atEnd())
		{
			quint8 length;
			stream >> length;

			if (length == 0)
			{
				break;
			}
			else if ((length & 0xC0) == 0xC0)
			{
				stream.skipRawData(1);
				break;
			}
			else
			{
				stream.skipRawData(length);
			}
		}
	}

} // namespace CastIt
