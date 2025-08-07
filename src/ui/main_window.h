#pragma once
#include <QMainWindow>
#include "core/device_discovery.h"

namespace Ui
{
	class MainWindow; // Forward declaration of the UI class
}

namespace CastIt
{
	class MainWindow : public QMainWindow
	{
		Q_OBJECT // Enable Qt's signals and slots

	public:
		explicit MainWindow(QWidget* parent = nullptr);
		~MainWindow() override;

	private slots:
		void updateDeviceList(const QStringList& devices); // Slot for device updates

	private:
		Ui::MainWindow* ui; // Pointer to the UI object from .ui
		DeviceDiscovery* deviceDiscovery; // Pointer to the device discovery object
		void initializeDiscovery();
	};
}
