#pragma once

#include <QObject>
#include <QStringList>
#include <QUdpSocket>
#include <QTimer>
#include <QDataStream>
#include <QHostAddress>
#include <QThread>
#include <QMap>
#include <QNetworkAccessManager>


namespace CastIt
{
	class DlnaDiscovery : public QObject
	{
		Q_OBJECT

	public:
		explicit DlnaDiscovery(QObject* parent = nullptr);
		~DlnaDiscovery() override;

		void startDiscovery();


	signals:
		void renderersUpdated(const QStringList& renderers); // Renderer names
		void rendererUrlsUpdated(const QMap<QString, QString>& rendererUrls); // Name to control URL
		void discoveryError(const QString& errorMessage);

	private slots:
		void sendSearch();
		void processResponse();

	private:
		QUdpSocket* udpSocket;
		QTimer* searchTimer;
		QStringList discoveredRenderers;
		QMap<QString, QString> rendererControlUrls;
		int searchCount = 0;
		QNetworkAccessManager* networkManager;

		void joinMulticastGroups();
		void parseDeviceDescription(const QString& locationUrl, const QString& ipAddress);
		QString extractDeviceName(const QByteArray& xml);
		QString extractControlUrl(const QByteArray& xml, const QString& baseUrl); // Fetch and parse XML
	};

}