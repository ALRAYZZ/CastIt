#pragma once
#include <QMainWindow>
#include <QString>
#include "core/device_discovery.h"
#include <core/cast_controller.h>
#include <core/dlna_discovery.h>
#include <core/dlna_controller.h>

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

		// Slots are functions that can be called in response to signals
	private slots:
		void updateDeviceList(const QStringList& devices); // Slot for device updates
		void onSelectedMediaButtonClicked(); // Handle media button selection
		void onPlayButtonClicked(); // Handle play button
		void onPauseButtonClicked(); // Handle pause button
		void onStopButtonClicked(); // Handle stop button
		void onDeviceSelectionChanged(); // Handle device selection
		void onDeviceIpsUpdated(const QMap<QString, QHostAddress>& ips); // Handle device IP updates

	private:
		// Setting pointers for the fields allows us to decuple the lifetime of the UI and discovery objects from the MainWindow
		Ui::MainWindow* ui; // Pointer to the UI object from .ui
		DeviceDiscovery* deviceDiscovery; // Pointer to the device discovery object
		QString selectedMediaPath; // Path to the selected media file
		CastController* castController; // Pointer to the cast controller
		QMap<QString, QHostAddress> deviceIps; // Map of device names to IP addresses
		void initializeDiscovery();

		DlnaDiscovery* dlnaDiscovery; // Pointer to the DLNA discovery object
		DlnaController* dlnaController; // Pointer to the DLNA controller object
		QMap<QString, QString> dlnaUrls;
		QString selectedDeviceType;

		void onRenderersUpdated(const QStringList& renderers); // Handle DLNA renderer updates
		void onRendererUrlsUpdated(const QMap<QString, QString>& urls); // Handle DLNA renderer URLs

	};
}
