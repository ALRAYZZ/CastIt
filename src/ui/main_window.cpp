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

		// Connect button signals
		connect(ui->selectMediaButton, &QPushButton::clicked, this, &MainWindow::onSelectedMediaButtonClicked);
		connect(ui->playButton, &QPushButton::clicked, this, &MainWindow::onPlayButtonClicked);
		connect(ui->pauseButton, &QPushButton::clicked, this, &MainWindow::onPauseButtonClicked);
		connect(ui->stopButton, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
		connect(ui->deviceList, &QListWidget::itemSelectionChanged, this, &MainWindow::onDeviceSelectionChanged);
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
	
	void MainWindow::onPlayButtonClicked()
	{
		// Getting the string name of the selected device
		QString selectedDevice = ui->deviceList->currentItem() ? ui->deviceList->currentItem()->text() : "None";
		if (selectedMediaPath.isEmpty())
		{
			qDebug() << "Play requested but no media file selected";
		}
		else if (selectedDevice == "None")
		{
			qDebug() << "Play requested but no device selected";
		}
		else
		{
			qDebug() << "Play requested for device:" << selectedDevice << "with media:" << selectedMediaPath;
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
}