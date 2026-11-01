// SPDX-License-Identifier: MIT
#include "shell/desktop.h"
#include <QApplication>
#include <QDir>
#include <QTimer>
int main(int argc,char **argv) {
    QApplication app(argc,argv); app.setApplicationName("Meteora Render Test");
    Meteora::applyPalette(); Meteora::Desktop desktop(true); desktop.show();
    const QString path=argc>1 ? QString::fromLocal8Bit(argv[1]) : "meteora-render.png";
    QTimer::singleShot(100,&app,[&] {
        desktop.toggleLauncher();
        QTimer::singleShot(100,&app,[&] { app.exit(desktop.capture(path) ? 0 : 1); });
    });
    return app.exec();
}
