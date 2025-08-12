#include "dlna_controller.h"
#include <QDebug>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QFile>
#include <QHostAddress>
#include <QNetworkInterface>

namespace CastIt
{
    DlnaController::DlnaController(QObject* parent) : QObject(parent), networkManager(new QNetworkAccessManager(this))
    {
    }

    DlnaController::~DlnaController()
    {
    }

    void DlnaController::castMedia(const QString& controlUrl, const QString& mediaPath)
    {
        startLocalServer(mediaPath);

        // Set AVTransportURI
        QString setUriBody = "<u:SetAVTransportURI xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
            "<InstanceID>0</InstanceID>"
            "<CurrentURI>" + localMediaUrl + "</CurrentURI>"
            "<CurrentURIMetaData></CurrentURIMetaData>"
            "</u:SetAVTransportURI>";
        
        sendSoapAction(controlUrl, "SetAVTransportURI", setUriBody);

        // Play SOAP action body
        QString playBody = "<u:Play xmlns:u=\"urn:schemas-upnp-org:service:AVTransport:1\">"
            "<InstanceID>0</InstanceID>"
            "<Speed>1</Speed>"
            "</u:Play>";
        
        sendSoapAction(controlUrl, "Play", playBody);
    }

    void DlnaController::startLocalServer(const QString& mediaPath)
    {
        QTcpServer* server = new QTcpServer(this);
        
        // Get the first available network interface IP
        QString localIp = "127.0.0.1";
        QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
        for (const QNetworkInterface& interface : interfaces)
        {
            if (interface.flags() & QNetworkInterface::IsUp && 
                interface.flags() & QNetworkInterface::IsRunning &&
                !(interface.flags() & QNetworkInterface::IsLoopBack))
            {
                QList<QNetworkAddressEntry> entries = interface.addressEntries();
                for (const QNetworkAddressEntry& entry : entries)
                {
                    if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol)
                    {
                        localIp = entry.ip().toString();
                        break;
                    }
                }
                if (localIp != "127.0.0.1") break;
            }
        }

        if (server->listen(QHostAddress::Any, 0))
        {
            localMediaUrl = QString("http://%1:%2/media").arg(localIp).arg(server->serverPort());
            qDebug() << "Started local media server at:" << localMediaUrl;
            
            connect(server, &QTcpServer::newConnection, [server, mediaPath, this]()
            {
                QTcpSocket* socket = server->nextPendingConnection();
                
                connect(socket, &QTcpSocket::readyRead, [socket, mediaPath]()
                {
                    QByteArray request = socket->readAll();
                    qDebug() << "HTTP request:" << request.left(200);
                    
                    QFile file(mediaPath);
                    if (file.open(QIODevice::ReadOnly))
                    {
                        QString mimeType = "video/mp4"; // Default
                        if (mediaPath.endsWith(".mp3", Qt::CaseInsensitive))
                            mimeType = "audio/mpeg";
                        else if (mediaPath.endsWith(".mkv", Qt::CaseInsensitive))
                            mimeType = "video/x-matroska";
                        else if (mediaPath.endsWith(".avi", Qt::CaseInsensitive))
                            mimeType = "video/x-msvideo";

                        QString header = QString("HTTP/1.1 200 OK\r\n"
                            "Content-Type: %1\r\n"
                            "Content-Length: %2\r\n"
                            "Accept-Ranges: bytes\r\n"
                            "Connection: close\r\n"
                            "\r\n").arg(mimeType).arg(file.size());
                        
                        socket->write(header.toUtf8());
                        socket->write(file.readAll());
                        file.close();
                    }
                    else
                    {
                        QString response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
                        socket->write(response.toUtf8());
                    }
                    socket->disconnectFromHost();
                });
            });
        }
        else
        {
            emit castingError("Failed to start local server: " + server->errorString());
        }
    }

    void DlnaController::sendSoapAction(const QString& controlUrl, const QString& action, const QString& body)
    {
        QNetworkRequest request((QUrl(controlUrl)));
        request.setHeader(QNetworkRequest::ContentTypeHeader, "text/xml; charset=\"utf-8\"");
        request.setRawHeader("SOAPAction", QString("\"urn:schemas-upnp-org:service:AVTransport:1#%1\"").arg(action).toUtf8());
        request.setRawHeader("User-Agent", "CastIt/1.0");

        QString soapEnvelope = "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
            "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" "
            "s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
            "<s:Body>" + body + "</s:Body></s:Envelope>";

        qDebug() << "Sending SOAP action:" << action << "to" << controlUrl;
        qDebug() << "SOAP envelope:" << soapEnvelope;

        QNetworkReply* reply = networkManager->post(request, soapEnvelope.toUtf8());
        connect(reply, &QNetworkReply::finished, [reply, action, this]()
        {
            if (reply->error() == QNetworkReply::NoError)
            {
                qDebug() << "SOAP action" << action << "successful";
                emit castingStatus(QString("SOAP action %1 successful").arg(action));
            }
            else
            {
                qDebug() << "SOAP action" << action << "failed:" << reply->errorString();
                qDebug() << "Response:" << reply->readAll();
                emit castingError(QString("SOAP action %1 failed: %2").arg(action, reply->errorString()));
            }
            reply->deleteLater();
        });
    }
}