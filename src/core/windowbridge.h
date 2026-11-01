// SPDX-License-Identifier: MIT
#pragma once
#include <QObject>
#include <QIcon>
#include <QRect>
#include <memory>

namespace Meteora {
struct WindowInfo { unsigned long id; QString title; int desktop; bool minimized; QIcon icon; };
class WindowBridge : public QObject {
    Q_OBJECT
public:
    explicit WindowBridge(QObject *parent = nullptr);
    ~WindowBridge() override;
    bool available() const;
    QList<WindowInfo> windows() const;
    unsigned long activeWindow() const;
    int currentDesktop() const;
    int desktopCount() const;
    void setDesktop(int index);
    void activate(unsigned long id);
    void minimize(unsigned long id);
    void close(unsigned long id);
    void moveToDesktop(unsigned long id, int desktop);
    void markDesktop(unsigned long id);
    void markPanel(unsigned long id, const QRect &geometry, int rootHeight);
    void activatePopup(unsigned long id);
    void showDesktop(bool show);
signals:
    void windowsChanged();
    void desktopChanged();
    void superPressed();
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
}
