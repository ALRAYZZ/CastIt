#pragma once
#include <QMainWindow>
#include <QString>
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

		// Slots are functions that can be called in response to signals
	private slots:
		void updateDeviceList(const QStringList& devices); // Slot for device updates
		void onSelectedMediaButtonClicked(); // Handle media button selection
		void onPlayButtonClicked(); // Handle play button
		void onPauseButtonClicked(); // Handle pause button
		void onStopButtonClicked(); // Handle stop button
		void onDeviceSelectionChanged(); // Handle device selection

	private:
		// Setting pointers for the fields allows us to decuple the lifetime of the UI and discovery objects from the MainWindow
		Ui::MainWindow* ui; // Pointer to the UI object from .ui
		DeviceDiscovery* deviceDiscovery; // Pointer to the device discovery object
		QString selectedMediaPath; // Path to the selected media file
		void initializeDiscovery();
	};
}
