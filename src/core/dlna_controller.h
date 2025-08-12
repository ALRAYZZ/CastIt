#pragma once

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

namespace CastIt
{
	class DlnaController : public QObject
	{
		Q_OBJECT

	public:
		explicit DlnaController(QObject* parent = nullptr);
		~DlnaController() override;

		void castMedia(const QString& controlUrl, const QString& mediaPath); // Cast to DLNA

	signals:
		void castingStatus(const QString& status);
		void castingError(const QString& error);

	private:
		QNetworkAccessManager* networkManager;
		QString localMediaUrl;

		void startLocalServer(const QString& mediaPath);
		void sendSoapAction(const QString& controlUrl, const QString& action, const QString& body);

	};

}