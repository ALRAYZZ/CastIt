#include "dlna_discovery.h"
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QXmlStreamReader>
#include <QNetworkInterface>
#include <QEventLoop>


namespace CastIt
{

	DlnaDiscovery::DlnaDiscovery(QObject* parent) : QObject(parent), udpSocket(new QUdpSocket(this)),
		searchTimer(new QTimer(this)), networkManager(new QNetworkAccessManager(this))
	{

		// Try to bind to a random port
		if (!udpSocket->bind(QHostAddress::AnyIPv4, 0, QUdpSocket::ShareAddress))
		{
			QString errorMessage = "Failed to bind UDP socket for SSDP: " + udpSocket->errorString();
			qDebug() << errorMessage;
			emit discoveryError(errorMessage);
			return;
		}

		qDebug() << "DLNA discovery bound to port:" << udpSocket->localPort();

		connect(udpSocket, &QUdpSocket::readyRead, this, &DlnaDiscovery::processResponse);
		connect(searchTimer, &QTimer::timeout, this, &DlnaDiscovery::sendSearch);
	}

	DlnaDiscovery::~DlnaDiscovery()
	{
		searchTimer->stop();
	}

	void DlnaDiscovery::startDiscovery()
	{
		discoveredRenderers.clear();
		rendererControlUrls.clear();
		searchCount = 0;
		sendSearch();
		searchTimer->start(5000); // Send search every 5 seconds
	}

	void DlnaDiscovery::joinMulticastGroups()
	{
		// Join 239.255.255.250 on all interfaces
		QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
		for (const QNetworkInterface& interface : interfaces)
		{
			if (interface.flags() & QNetworkInterface::CanMulticast)
			{
				udpSocket->joinMulticastGroup(QHostAddress("239.255.255.250"), interface);
			}
		}
	}

	void DlnaDiscovery::sendSearch()
	{
		searchCount++;
		QByteArray searchMessage = "M-SEARCH * HTTP/1.1\r\n"
        "HOST: 239.255.255.250:1900\r\n"
        "MAN: \"ssdp:discover\"\r\n"
        "ST: urn:schemas-upnp-org:device:MediaRenderer:1\r\n"
        "MX: 3\r\n"
		"\r\n";
		qint64 written = udpSocket->writeDatagram(searchMessage, QHostAddress("239.255.255.250"), 1900);
		qDebug() << "Sent SSDP M-SEARCH, bytes written: " << written;

		if (searchCount >= 8)
		{
			searchTimer->stop();
			qDebug() << "DLNA discovery search completed";
		}
	}

	void DlnaDiscovery::processResponse()
	{
		while (udpSocket->hasPendingDatagrams())
		{
			QByteArray datagram;
			datagram.resize(udpSocket->pendingDatagramSize());
			QHostAddress sender;
			quint16 port;
			udpSocket->readDatagram(datagram.data(), datagram.size(), &sender, &port);

			QString response = QString::fromUtf8(datagram);
			if (response.contains("HTTP/1.1 200 OK") &&
				(response.contains("ST: urn:schemas-upnp-org:device:MediaRenderer:1") ||
				 response.contains("NT: urn:schemas-upnp-org:device:MediaRenderer:1")))
			{
				// Extract the location URL
				QString location;
				QStringList lines = response.split("\r\n");
				for (const QString& line : lines)
				{
					if (line.startsWith("LOCATION:", Qt::CaseInsensitive) ||
						line.startsWith("Location:", Qt::CaseInsensitive))
					{
						location = line.mid(line.indexOf(':') + 1).trimmed();
						break;
					}
				}

				if (!location.isEmpty())
				{
					qDebug() << "Found location URL:" << location;
					parseDeviceDescription(location, sender.toString());
				}
			}
		}
	}

	void DlnaDiscovery::parseDeviceDescription(const QString& locationUrl, const QString& ipAddress)
	{
		QNetworkRequest request((QUrl(locationUrl)));
		request.setRawHeader("User-Agent", "CastIt/1.0");
		QNetworkReply* reply = networkManager->get(request);

		connect(reply, &QNetworkReply::finished, [this, reply, ipAddress]() {
			if (reply->error() != QNetworkReply::NoError)
			{
				qDebug() << "Network error fetching device description:" << reply->errorString();
				reply->deleteLater();
				return;
			}

			QByteArray xml = reply->readAll();
			qDebug() << "Device description XML:" << xml.left(500) << "...";

			QString deviceName = extractDeviceName(xml);
			QString controlUrl = extractControlUrl(xml, reply->url().toString());

			if (!controlUrl.isEmpty() && !deviceName.isEmpty())
			{
				if (!discoveredRenderers.contains(deviceName))
				{
					discoveredRenderers.append(deviceName);
					rendererControlUrls[deviceName] = controlUrl;
					qDebug() << "Added DLNA renderer:" << deviceName << "Control URL:" << controlUrl;
					emit renderersUpdated(discoveredRenderers);
					emit rendererUrlsUpdated(rendererControlUrls);
				}
			}
			reply->deleteLater();
			});
	}
	QString DlnaDiscovery::extractDeviceName(const QByteArray& xml)
	{
		QXmlStreamReader reader(xml);
		while (!reader.atEnd())
		{
			reader.readNext();
			if (reader.isStartElement())
			{
				if (reader.name() == "friendlyName")
				{
					return reader.readElementText();
				}
				else if (reader.name() == "modelName")
				{
					QString modelName = reader.readElementText();
					if (!modelName.isEmpty())
						return modelName;
				}
			}
		}
		return QString();
	}

	

	QString DlnaDiscovery::extractControlUrl(const QByteArray& xml, const QString& baseUrl)
	{
		QXmlStreamReader reader(xml);
		QUrl base(baseUrl);

		while (!reader.atEnd())
		{
			reader.readNext();
			if (reader.isStartElement() && reader.name() == "service")
			{
				QString serviceType;
				QString controlUrl;

				while (!reader.atEnd() && !(reader.isEndElement() && reader.name() == "service"))
				{
					reader.readNext();
					if (reader.isStartElement())
					{
						if (reader.name() == "serviceType")
						{
							serviceType = reader.readElementText();
						}
						else if (reader.name() == "controlURL")
						{
							controlUrl = reader.readElementText();
						}
					}
				}

				if (serviceType.contains("AVTransport") && !controlUrl.isEmpty())
				{
					QUrl fullUrl = base.resolved(QUrl(controlUrl));
					return fullUrl.toString();
				}
			}
		}
		return QString();
	}
}
