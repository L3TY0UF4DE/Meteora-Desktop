// SPDX-License-Identifier: MIT
#include "notifications.h"
#include <QDBusConnection>
#include <QDBusMessage>

namespace Meteora {
Notifications::Notifications(QObject *parent) : QObject(parent) {
    auto bus=QDBusConnection::sessionBus();
    // Never replace another desktop's notification server in an ordinary preview.
    ownsService=bus.registerService("org.freedesktop.Notifications");
    if (ownsService) ownsService=bus.registerObject("/org/freedesktop/Notifications",this,QDBusConnection::ExportAllSlots | QDBusConnection::ExportScriptableSignals);
}
QStringList Notifications::GetCapabilities() const { return {"body"}; }
QString Notifications::GetServerInformation(QString &vendor, QString &version, QString &specVersion) const {
    vendor="Meteora"; version="0.2.0"; specVersion="1.2"; return "Meteora Desktop";
}
uint Notifications::Notify(const QString &appName,uint replacesId,const QString &appIcon,const QString &summary,const QString &body,const QStringList &actions,const QVariantMap &hints,int expireTimeout) {
    Q_UNUSED(appIcon); Q_UNUSED(actions); Q_UNUSED(hints);
    const uint id=active.contains(replacesId) ? replacesId : nextId++;
    for (qsizetype i=notices.size(); i>0; --i) if (notices[i-1].id==id) notices.removeAt(i-1);
    notices.prepend({id,appName.left(120),summary.left(300),body.left(2000),QDateTime::currentDateTime()});
    while (notices.size()>50) notices.removeLast();
    // Limit active persistent notifications as well as visible history.
    if (!active.contains(id) && active.size()>=50) close(active.constBegin().key(),1);
    auto *timer=active.value(id,nullptr);
    if (!timer) {
        timer=new QTimer(this); timer->setSingleShot(true); active.insert(id,timer);
        connect(timer,&QTimer::timeout,this,[this,id] { close(id,1); });
    }
    timer->stop();
    if (expireTimeout != 0) timer->start(expireTimeout<0 ? 6000 : qBound(1,expireTimeout,3600000));
    emit historyChanged(); emit received(id); return id;
}
void Notifications::close(uint id,uint reason) {
    if (auto *timer=active.take(id)) {
        timer->stop(); timer->deleteLater();
        // Explicit D-Bus signal: internal UI signals are not exposed to clients.
        auto message=QDBusMessage::createSignal("/org/freedesktop/Notifications","org.freedesktop.Notifications","NotificationClosed");
        message << id << reason; QDBusConnection::sessionBus().send(message);
        emit NotificationClosed(id,reason);
    }
}
void Notifications::CloseNotification(uint id) { close(id,3); }
void Notifications::clearHistory() {
    const auto ids=active.keys(); for (auto id:ids) close(id,2);
    notices.clear(); emit historyChanged();
}
}
