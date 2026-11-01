// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QDBusContext>
#include <QVariantMap>
#include <QDateTime>
#include <QHash>
#include <QTimer>

namespace Meteora {
struct Notice { uint id; QString app, summary, body; QDateTime time; };
class Notifications : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")
public:
    explicit Notifications(QObject *parent = nullptr);
    bool registered() const { return ownsService; }
    QList<Notice> history() const { return notices; }
    void clearHistory();
public slots:
    QStringList GetCapabilities() const;
    QString GetServerInformation(QString &vendor, QString &version, QString &specVersion) const;
    uint Notify(const QString &appName, uint replacesId, const QString &appIcon,
                const QString &summary, const QString &body, const QStringList &actions,
                const QVariantMap &hints, int expireTimeout);
    void CloseNotification(uint id);
signals:
    void NotificationClosed(uint id, uint reason);
    void received(uint id);
    void historyChanged();
private:
    bool ownsService = false;
    uint nextId = 1;
    QList<Notice> notices;
    QHash<uint,QTimer *> active;
    void close(uint id, uint reason);
};
}
