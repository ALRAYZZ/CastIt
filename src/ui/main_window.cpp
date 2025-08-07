#include "main_window.h"
#include "ui_main_window.h"

namespace CastIt
{
	MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent), ui(new Ui::MainWindow)
	{
		ui->setupUi(this);
		setWindowTitle("CastIt Media Casting App");
		resize(500, 400);
		initializeDiscovery();
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
}