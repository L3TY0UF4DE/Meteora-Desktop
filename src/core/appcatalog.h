// SPDX-License-Identifier: MIT
#pragma once
#include <QFileSystemWatcher>
#include <QIcon>
#include <QTimer>

namespace Meteora {
struct AppEntry {
    QString id, name, description, categories, keywords, iconName;
};
class AppCatalog : public QObject {
    Q_OBJECT
public:
    explicit AppCatalog(QObject *parent = nullptr);
    const QList<AppEntry> &entries() const { return apps; }
    QList<AppEntry> matching(const QString &query, const QString &category = {}) const;
    bool launch(const QString &id, QString *error = nullptr) const;
    static QIcon icon(const AppEntry &entry);
public slots:
    void reload();
signals:
    void changed();
private:
    QList<AppEntry> apps;
    QFileSystemWatcher watcher;
    QTimer debounce;
    void watchDirectories();
};
}
