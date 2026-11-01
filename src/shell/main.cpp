// SPDX-License-Identifier: MIT
#include "desktop.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QStandardPaths>
#include <QTextStream>
#include <unistd.h>

int main(int argc,char **argv) {
    QApplication app(argc,argv);
    app.setApplicationName("Meteora Desktop"); app.setApplicationVersion("0.2.0"); app.setQuitOnLastWindowClosed(false);
    Meteora::applyPalette();
    QCommandLineParser parser; parser.setApplicationDescription("Lightweight native Meteora desktop shell"); parser.addHelpOption(); parser.addVersionOption();
    parser.addOption({"preview","Open an ordinary preview window without changing your desktop."});
    parser.addOption({"toggle-launcher","Toggle the running shell's application launcher."});
    parser.addOption({"settings","Open Meteora Settings."});
    parser.addOption({"status","Print the running shell's status as JSON."});
    parser.addOption({"quit","End the running Meteora shell."});
    parser.addOption({"capture","Save a screenshot of the running shell.","file"});
    parser.process(app);
    const bool preview=parser.isSet("preview");
    const QByteArray display=qgetenv("DISPLAY")+qgetenv("WAYLAND_DISPLAY")+(preview ? "-preview" : "-session");
    const QString socketName=QString("meteora-%1-%2").arg(getuid()).arg(QString::fromLatin1(QCryptographicHash::hash(display,QCryptographicHash::Sha256).toHex().left(16)));
    QJsonObject request;
    QString command;
    for(const QString name:{"toggle-launcher","settings","status","quit","capture"}) if(parser.isSet(name)) command=name;
    request.insert("command",command.isEmpty() ? "toggle-launcher" : command);
    if(command=="capture") request.insert("path",QFileInfo(parser.value("capture")).absoluteFilePath());
    QLocalSocket client; client.connectToServer(socketName);
    if(client.waitForConnected(150)) {
        client.write(QJsonDocument(request).toJson(QJsonDocument::Compact)+'\n'); client.waitForBytesWritten(1000);
        QByteArray answer; while(!answer.contains('\n') && client.waitForReadyRead(3000)) answer+=client.readAll();
        if(answer.isEmpty()) { QTextStream(stderr)<<"Meteora did not respond.\n"; return 1; }
        QTextStream(stdout)<<answer;
        return QJsonDocument::fromJson(answer).object().value("ok").toBool(false) ? 0 : 1;
    }
    if(!command.isEmpty()) { QTextStream(stderr)<<"No Meteora shell is running on this display"<<(preview ? " in preview mode" : "")<<".\n"; return 1; }
    if(!preview && QGuiApplication::platformName()!="xcb") {
        QTextStream(stderr)<<"The desktop session currently requires X11. Use --preview for a windowed preview.\n"; return 1;
    }
    QLockFile lock(QStandardPaths::writableLocation(QStandardPaths::TempLocation)+"/"+socketName+".lock");
    if(!lock.tryLock(0)) { QTextStream(stderr)<<"Another Meteora shell is starting.\n"; return 1; }
    QLocalServer::removeServer(socketName);
    QLocalServer server; server.setSocketOptions(QLocalServer::UserAccessOption);
    if(!server.listen(socketName)) { QTextStream(stderr)<<server.errorString()<<'\n'; return 1; }
    Meteora::Desktop desktop(preview); desktop.show();
    QObject::connect(&server,&QLocalServer::newConnection,&app,[&] {
        while(auto *peer=server.nextPendingConnection()) {
            auto data=std::make_shared<QByteArray>();
            auto *deadline=new QTimer(peer); deadline->setSingleShot(true); deadline->start(3000);
            QObject::connect(deadline,&QTimer::timeout,peer,&QLocalSocket::abort);
            QObject::connect(peer,&QLocalSocket::disconnected,peer,&QObject::deleteLater);
            QObject::connect(peer,&QLocalSocket::readyRead,&app,[&,peer,data,deadline] {
                *data+=peer->readAll();
                if(data->size()>16384) { peer->abort(); return; }
                if(!data->contains('\n')) return;
                deadline->stop(); const auto msg=QJsonDocument::fromJson(*data).object(); const auto cmd=msg.value("command").toString();
                QJsonObject reply{{"ok",true}};
                if(cmd=="toggle-launcher") desktop.toggleLauncher();
                else if(cmd=="settings") desktop.openSettings();
                else if(cmd=="status") reply.insert("status",desktop.status());
                else if(cmd=="capture") reply.insert("ok",desktop.capture(msg.value("path").toString()));
                else if(cmd=="quit") QTimer::singleShot(100,&app,&QApplication::quit);
                else reply={{"ok",false},{"error","Unknown command"}};
                peer->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n'); peer->flush(); peer->disconnectFromServer();
            });
        }
    });
    return app.exec();
}
