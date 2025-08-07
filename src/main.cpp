#include <QApplication>
#include "ui/main_window.h"

int main(int argc, char* argv[])
{
	QApplication app(argc, argv);
	CastIt::MainWindow window;
	window.show();
	return app.exec();
}