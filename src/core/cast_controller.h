#pragma once

#include <QHostAddress>
#include <QObject>
#include <QNetworkAccessManager>
#include <QWebSocket>
#include <QHttpServer>


namespace CastIt
{

	class CastController : public QObject
	{
		Q_OBJECT

	public:
		explicit CastController(QObject* parent = nullptr);
		~CastController();

		void startMediaServer(const QString& filePath); // Starts local HTTP server to serve media files
		void castMedia(const QHostAddress& deviceIp, const QString& mediaUrl); // Sends cast command
		void play();
		void pause();
		void stop();

		QString getLocalUrl() const { return localUrl; }

	signals:
		void castingStatus(const QString& status);
		void castingError(const QString& error);

	private slots:
		void onWebSocketConnected();
		void onWebSocketDisconnected();
		void onWebSocketError(QAbstractSocket::SocketError error);
		void onWebSocketTextMessageReceived(const QString& message);

	private:
		QNetworkAccessManager* networkManager;
		QWebSocket* webSocket;
		QHttpServer* mediaServer;
		QString localUrl;
		QString sessionId;
		QString transportId;

		void launchReceiver(const QHostAddress& deviceIp); // Launches receiver app on the cast device
		void loadMedia(const QString& mediaUrl); // Loads media on the cast device
	};
} // namespace CastIt
