#include "main_window.h"
#include "ui_main_window.h"
#include <QDebug>
#include <QFileDialog>

namespace CastIt
{
	MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
	{
		ui->setupUi(this);
		setWindowTitle("CastIt Media Casting App");
		resize(500, 400);
		initializeDiscovery();
		dlnaDiscovery = new DlnaDiscovery(this);
		dlnaDiscovery->startDiscovery(); // Start DLNA discovery
		dlnaController = new DlnaController(this);

		// Connect button signals
		connect(ui->selectMediaButton, &QPushButton::clicked, this, &MainWindow::onSelectedMediaButtonClicked);
		connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
		connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseButtonClicked);
		connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
		connect(ui->deviceList, &QListWidget::itemSelectionChanged, this, &MainWindow::onDeviceSelectionChanged);
		connect(dlnaDiscovery, &DlnaDiscovery::renderersUpdated, this, &MainWindow::onRenderersUpdated);
		connect(dlnaDiscovery, &DlnaDiscovery::rendererUrlsUpdated, this, &MainWindow::onRendererUrlsUpdated);

		castController = new CastController(this);
		connect(castController, &CastController::castingStatus, this, [](const QString& status)
			{
				qDebug() << "Casting status:" << status;
			});
		connect(castController, &CastController::castingError, this, [](const QString& error)
			{
				qDebug() << "Casting error:" << error;
			});
		connect(deviceDiscovery, &DeviceDiscovery::deviceIpsUpdated, this, &MainWindow::onDeviceIpsUpdated);

	}

	MainWindow::~MainWindow()
	{
		delete ui;
		delete deviceDiscovery;
	}

	void MainWindow::initializeDiscovery()
	{
		deviceDiscovery = new DeviceDiscovery(this);
		connect(deviceDiscovery, &DeviceDiscovery::devicesUpdated, this, &MainWindow::updateDeviceList);
		deviceDiscovery->startDiscovery();
	}

	void MainWindow::updateDeviceList(const QStringList& devices)
	{
		ui->deviceList->clear(); // Clear existing items
		ui->deviceList->addItems(devices); // Add new devices
	}

	void MainWindow::onSelectedMediaButtonClicked()
	{
		QString filePath = QFileDialog::getOpenFileName(this, "Select Media File", "", "Media Files (*.mp4 *.mp3 *.mkv *.avi)");
		if (!filePath.isEmpty())
		{
			selectedMediaPath = filePath;
			qDebug() << "Selected media file:" << selectedMediaPath;
		}
	}

	void MainWindow::onRenderersUpdated(const QStringList& renderers)
	{
		for (const QString& renderer : renderers)
		{
			ui->deviceList->addItem("DLNA: " + renderer);
		}
	}

	void MainWindow::onRendererUrlsUpdated(const QMap<QString, QString>& urls)
	{
		dlnaUrls = urls;
	}
	
	void MainWindow::onPlayButtonClicked()
	{
		QString selectedDevice = ui->deviceList->currentItem() ? ui->deviceList->currentItem()->text() : "None";
		if (selectedMediaPath.isEmpty() || selectedDevice == "None")
		{
			qDebug() << "No media or device selected";
			return;
		}

		QHostAddress ip = deviceIps.value(selectedDevice);
		if (ip.isNull())
		{
			qDebug() << "No IP for selected device";
			return;
		}
		if (selectedDevice.startsWith("DLNA: "))
		{
			selectedDeviceType = "DLNA";
			QString rendererName = selectedDevice.mid(6); // Remove "DLNA: " prefix
			QString controlUrl = dlnaUrls[rendererName];
			dlnaController->castMedia(controlUrl, selectedMediaPath);
		}
		else if (selectedDevice.startsWith("Chromecast: "))
		{
			castController->startMediaServer(selectedMediaPath);
			castController->castMedia(ip, castController->getLocalUrl()); // Local URL from server
		}

	}

	void MainWindow::onPauseButtonClicked()
	{
		QString selectedDevice = ui->deviceList->currentItem() ? ui->deviceList->currentItem()->text() : "None";
		qDebug() << "Pause requested for device: " << selectedDevice;
	}

	void MainWindow::onStopButtonClicked()
	{
		QString selectedDevice = ui->deviceList->currentItem() ? ui->deviceList->currentItem()->text() : "None";
		qDebug() << "Stop requested for device: " << selectedDevice;
	}

	void MainWindow::onDeviceSelectionChanged()
	{
		QString selectedDevice = ui->deviceList->currentItem() ? ui->deviceList->currentItem()->text() : "None";
		qDebug() << "Device selected: " << selectedDevice;
	}

	void MainWindow::onDeviceIpsUpdated(const QMap<QString, QHostAddress>& ips)
	{
		deviceIps = ips;
	}
}