#include "cast_controller.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QUrl>
#include <QFileInfo>
#include <QDebug>
#include <QTcpServer>

namespace CastIt
{
	CastController::CastController(QObject* parent) : QObject(parent), networkManager(new QNetworkAccessManager(this)),
		webSocket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this)),
		mediaServer(new QHttpServer(this))
	{
		connect(webSocket, &QWebSocket::connected, this, &CastController::onWebSocketConnected);
		connect(webSocket, &QWebSocket::disconnected, this, &CastController::onWebSocketDisconnected);
		connect(webSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::error), this, &CastController::onWebSocketError);
		connect(webSocket, &QWebSocket::textMessageReceived, this, &CastController::onWebSocketTextMessageReceived);
	}

	CastController::~CastController()
	{
		if (webSocket->isValid())
		{
			webSocket->close();
		}
	}

	void CastController::startMediaServer(const QString& filePath)
	{
		QFileInfo fileInfo(filePath);
		QString fileName = fileInfo.fileName();
		QString localIp = QHostAddress(QHostAddress::LocalHost).toString();
		quint16 port = 8000;

		mediaServer->route("/", [=](const QHttpServerRequest& request)
			{
				QFile file(filePath);
				if (file.open(QIODevice::ReadOnly))
				{
					return QHttpServerResponse("video/mp4", file.readAll());
				}
				return QHttpServerResponse(QHttpServerResponse::StatusCode::NotFound);
			});

		QTcpServer* tcpServer = new QTcpServer();

		if (!tcpServer->listen(QHostAddress::Any, port))
		{
			qWarning() << "Failed to start TCP server:" << tcpServer->errorString();
			delete tcpServer;
			return;
		}

		if (!mediaServer->bind(tcpServer))
		{
			qWarning() << "Failed to bind HTTP server to TCP server";
			delete tcpServer;
			return;
		}

		// Construct the local URL
		QString localUrl = QString("http://%1:%2/%3").arg(localIp).arg(tcpServer->serverPort()).arg(fileName);
		qDebug() << "Media server started at:" << localUrl;
	}

	void CastController::castMedia(const QHostAddress& deviceIp, const QString& mediaUrl)
	{
		launchReceiver(deviceIp);

		loadMedia(mediaUrl);
	}

	void CastController::launchReceiver(const QHostAddress& deviceIp)
	{
		QUrl url(QString("http://%1:8008/apps/YouTube").arg(deviceIp.toString())); //  Example URL for YouTube app
		QNetworkRequest request(url);
		networkManager->post(request, QByteArray());
	}

	void CastController::loadMedia(const QString& mediaUrl)
	{
		if (webSocket->isValid())
		{
			QJsonObject payload;
			payload["type"] = "LOAD";
			payload["media"] = QJsonObject{
				{"contentId", mediaUrl},
				{"streamType", "BUFFERED"},
				{"contentType", "video/mp4"}
			};
			payload["requestId"] = 1; // Unique request ID

			QJsonObject message;
			message["namespace"] = "urn:x-cast:com.google.cast.media";
			message["payload"] = payload;
			webSocket->sendTextMessage(QJsonDocument(message).toJson(QJsonDocument::Compact));
		}
	}

	// Control methods
	void CastController::play()
	{
		// Send play via WebSocket
	}
	void CastController::pause()
	{
		// Send pause via WebSocket
	}
	void CastController::stop()
	{
		// Send stop via WebSocket
	}

	// WebSocket callbacks
	void CastController::onWebSocketConnected()
	{
		qDebug() << "WebSocket connected";
		emit castingStatus("Connected to cast device");
	}
	void CastController::onWebSocketDisconnected()
	{
		qDebug() << "WebSocket disconnected";
		emit castingStatus("Disconnected from cast device");
	}
	void CastController::onWebSocketError(QAbstractSocket::SocketError error)
	{
		qDebug() << "WebSocket error:" << webSocket->errorString();
		emit castingError(webSocket->errorString());
	}
	void CastController::onWebSocketTextMessageReceived(const QString& message)
	{
		QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
	}
}