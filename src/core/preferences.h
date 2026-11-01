// SPDX-License-Identifier: MIT
#pragma once
#include <QFileSystemWatcher>
#include <QSettings>
#include <QTimer>

namespace Meteora {
QString assetPath(const QString &name);
QString styleSheet();
void applyPalette();
class Preferences : public QObject {
    Q_OBJECT
public:
    explicit Preferences(QObject *parent = nullptr);
    QVariant value(const QString &key, const QVariant &fallback = {}) const;
    bool setValue(const QString &key, const QVariant &value);
    QString fileName() const;
signals:
    void changed();
private:
    void watch();
    QFileSystemWatcher watcher;
    QTimer debounce;
};
}
