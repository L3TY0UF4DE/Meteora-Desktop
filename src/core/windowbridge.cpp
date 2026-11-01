// SPDX-License-Identifier: MIT
#include "windowbridge.h"
#include <QGuiApplication>
#include <QSocketNotifier>
#include <QTimer>
#include <QSet>
#include <QHash>
#include <QPixmap>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xutil.h>
#include <climits>

namespace Meteora {
// Clients can disappear between a property event and its read. Ignore those
// normal X11 races on our own connection; keep Qt's error handler elsewhere.
static Display *bridgeDisplay = nullptr;
static XErrorHandler previousHandler = nullptr;
static int onXError(Display *display, XErrorEvent *event) {
    if (display == bridgeDisplay && (event->error_code == BadWindow || event->error_code == BadDrawable || event->error_code == BadMatch)) return 0;
    return previousHandler ? previousHandler(display, event) : 0;
}
struct WindowBridge::Impl {
    Display *display = nullptr;
    Window root = 0;
    int xi = 0;
    KeyCode superL = 0, superR = 0;
    bool held = false, chord = false;
    QSet<Window> watched;
    QHash<Window, QIcon> icons;
    mutable QHash<QByteArray, Atom> atoms;
    QTimer changed;
    Atom atom(const char *name) const {
        auto i = atoms.find(name);
        if (i != atoms.end()) return i.value();
        const Atom a = XInternAtom(display, name, False); atoms.insert(name,a); return a;
    }
    QList<unsigned long> numbers(Window win, const char *name, long limit = 4096) const {
        Atom type; int format; unsigned long count, remaining; unsigned char *bytes = nullptr;
        QList<unsigned long> result;
        if (XGetWindowProperty(display,win,atom(name),0,limit,False,AnyPropertyType,&type,&format,&count,&remaining,&bytes) == Success && format == 32 && bytes) {
            auto *values = reinterpret_cast<unsigned long *>(bytes);
            for (unsigned long i=0; i<count; ++i) result.append(values[i]);
        }
        if (bytes) XFree(bytes);
        return result;
    }
    QString title(Window win) const {
        Atom type; int format; unsigned long count, remaining; unsigned char *bytes = nullptr;
        QString title;
        if (XGetWindowProperty(display,win,atom("_NET_WM_NAME"),0,1024,False,atom("UTF8_STRING"),&type,&format,&count,&remaining,&bytes) == Success && bytes && format == 8)
            title = QString::fromUtf8(reinterpret_cast<char *>(bytes),count);
        if (bytes) XFree(bytes);
        if (title.isEmpty()) { char *name = nullptr; if (XFetchName(display,win,&name) && name) { title = QString::fromLocal8Bit(name); XFree(name); } }
        return title.isEmpty() ? QStringLiteral("Application") : title;
    }
    void set(Window win, const char *name, Atom type, const QList<unsigned long> &values) {
        XChangeProperty(display,win,atom(name),type,32,PropModeReplace,reinterpret_cast<const unsigned char *>(values.constData()),values.size());
        XFlush(display);
    }
    void send(Window win, const char *type, long a=0, long b=0, long c=0, long e=0, long f=0) {
        XEvent event{}; event.xclient.type = ClientMessage; event.xclient.window = win;
        event.xclient.message_type = atom(type); event.xclient.format = 32;
        event.xclient.data.l[0]=a; event.xclient.data.l[1]=b; event.xclient.data.l[2]=c; event.xclient.data.l[3]=e; event.xclient.data.l[4]=f;
        XSendEvent(display,root,False,SubstructureRedirectMask | SubstructureNotifyMask,&event); XFlush(display);
    }
    QIcon icon(Window win) {
        if (icons.contains(win)) return icons.value(win);
        // Bound property reads and allocations, even for malformed app icons.
        auto raw = numbers(win,"_NET_WM_ICON",262144);
        QImage best; unsigned long bestSize = ULONG_MAX;
        for (qsizetype i=0; i+2<raw.size();) {
            unsigned long w=raw[i++], h=raw[i++];
            if (!w || !h || w>512 || h>512 || w*h>static_cast<unsigned long>(raw.size()-i)) break;
            unsigned long distance = w >= 32 ? w-32 : 1024+32-w;
            if (distance < bestSize) {
                bestSize=distance; best=QImage(int(w),int(h),QImage::Format_ARGB32);
                for (unsigned long y=0; y<h; ++y) for (unsigned long x=0; x<w; ++x) best.setPixel(int(x),int(y),static_cast<QRgb>(raw[i+y*w+x]));
            }
            i+=qsizetype(w*h);
        }
        QIcon result = best.isNull() ? QIcon::fromTheme("application-x-executable") : QIcon(QPixmap::fromImage(best.scaled(32,32,Qt::KeepAspectRatio,Qt::SmoothTransformation)));
        icons.insert(win,result); return result;
    }
};
WindowBridge::WindowBridge(QObject *parent) : QObject(parent), d(std::make_unique<Impl>()) {
    if (QGuiApplication::platformName() != "xcb") return;
    d->display = XOpenDisplay(nullptr); if (!d->display) return;
    bridgeDisplay=d->display; previousHandler=XSetErrorHandler(onXError);
    d->root=DefaultRootWindow(d->display);
    XSelectInput(d->display,d->root,PropertyChangeMask | SubstructureNotifyMask);
    d->changed.setSingleShot(true); d->changed.setInterval(35);
    connect(&d->changed,&QTimer::timeout,this,&WindowBridge::windowsChanged);
    int event, error, major=2, minor=0;
    if (XQueryExtension(d->display,"XInputExtension",&d->xi,&event,&error) && XIQueryVersion(d->display,&major,&minor) == Success) {
        unsigned char bits[XIMaskLen(XI_RawButtonPress)]{};
        XISetMask(bits,XI_RawKeyPress); XISetMask(bits,XI_RawKeyRelease); XISetMask(bits,XI_RawButtonPress);
        XIEventMask mask{XIAllMasterDevices,sizeof(bits),bits};
        XISelectEvents(d->display,d->root,&mask,1);
        d->superL=XKeysymToKeycode(d->display,XK_Super_L); d->superR=XKeysymToKeycode(d->display,XK_Super_R);
    }
    XFlush(d->display);
    auto *notifier = new QSocketNotifier(ConnectionNumber(d->display),QSocketNotifier::Read,this);
    connect(notifier,&QSocketNotifier::activated,this,[this] {
        while (XPending(d->display)) {
            XEvent e; XNextEvent(d->display,&e);
            if (e.type == GenericEvent && e.xcookie.extension == d->xi && XGetEventData(d->display,&e.xcookie)) {
                auto *raw = static_cast<XIRawEvent *>(e.xcookie.data);
                const bool super = raw->detail == d->superL || raw->detail == d->superR;
                if (e.xcookie.evtype == XI_RawKeyPress) {
                    if (super) { if (!d->held) { d->held=true; d->chord=false; } }
                    else if (d->held) d->chord=true;
                } else if (e.xcookie.evtype == XI_RawButtonPress) {
                    if (d->held) d->chord=true;
                } else if (e.xcookie.evtype == XI_RawKeyRelease && super) {
                    const bool toggle = d->held && !d->chord; d->held=false;
                    if (toggle) emit superPressed();
                }
                XFreeEventData(d->display,&e.xcookie);
            } else if (e.type == PropertyNotify) {
                if (e.xproperty.window == d->root && (e.xproperty.atom == d->atom("_NET_CURRENT_DESKTOP") || e.xproperty.atom == d->atom("_NET_NUMBER_OF_DESKTOPS"))) emit desktopChanged();
                if (e.xproperty.atom == d->atom("_NET_WM_ICON")) d->icons.remove(e.xproperty.window);
                const QSet<Atom> observed{d->atom("_NET_CLIENT_LIST"),d->atom("_NET_ACTIVE_WINDOW"),d->atom("_NET_WM_STATE"),d->atom("_NET_WM_NAME"),XA_WM_NAME,d->atom("_NET_WM_DESKTOP"),d->atom("_NET_WM_ICON")};
                if (observed.contains(e.xproperty.atom)) d->changed.start();
            } else if (e.type == DestroyNotify) {
                d->watched.remove(e.xdestroywindow.window); d->icons.remove(e.xdestroywindow.window); d->changed.start();
            }
        }
    });
}
WindowBridge::~WindowBridge() {
    if (d->display) { XCloseDisplay(d->display); XSetErrorHandler(previousHandler); bridgeDisplay=nullptr; }
}
bool WindowBridge::available() const { return d->display; }
QList<WindowInfo> WindowBridge::windows() const {
    QList<WindowInfo> result; if (!available()) return result;
    const auto clients=d->numbers(d->root,"_NET_CLIENT_LIST");
    for (auto win : clients) {
        const auto types=d->numbers(win,"_NET_WM_WINDOW_TYPE");
        if (types.contains(d->atom("_NET_WM_WINDOW_TYPE_DESKTOP")) || types.contains(d->atom("_NET_WM_WINDOW_TYPE_DOCK"))) continue;
        const auto states=d->numbers(win,"_NET_WM_STATE");
        if (states.contains(d->atom("_NET_WM_STATE_SKIP_TASKBAR"))) continue;
        if (!d->watched.contains(win)) { XSelectInput(d->display,win,PropertyChangeMask | StructureNotifyMask); d->watched.insert(win); }
        const auto desktop=d->numbers(win,"_NET_WM_DESKTOP");
        result.append({win,d->title(win),desktop.isEmpty() ? 0 : int(desktop.first()),states.contains(d->atom("_NET_WM_STATE_HIDDEN")),d->icon(win)});
    }
    const auto cached=d->icons.keys(); for (auto win : cached) if (!clients.contains(win)) { d->icons.remove(win); d->watched.remove(win); }
    XFlush(d->display); return result;
}
unsigned long WindowBridge::activeWindow() const { return available() ? d->numbers(d->root,"_NET_ACTIVE_WINDOW").value(0) : 0; }
int WindowBridge::currentDesktop() const { return available() ? int(d->numbers(d->root,"_NET_CURRENT_DESKTOP").value(0)) : 0; }
int WindowBridge::desktopCount() const { return available() ? qBound(1,int(d->numbers(d->root,"_NET_NUMBER_OF_DESKTOPS").value(0,1)),16) : 1; }
void WindowBridge::setDesktop(int index) { if (available()) d->send(d->root,"_NET_CURRENT_DESKTOP",index,CurrentTime); }
void WindowBridge::activate(unsigned long id) { if (available()) d->send(id,"_NET_ACTIVE_WINDOW",2,CurrentTime,activeWindow()); }
void WindowBridge::minimize(unsigned long id) { if (available()) { XIconifyWindow(d->display,id,DefaultScreen(d->display)); XFlush(d->display); } }
void WindowBridge::close(unsigned long id) { if (available()) d->send(id,"_NET_CLOSE_WINDOW",CurrentTime,2); }
void WindowBridge::moveToDesktop(unsigned long id,int desktop) { if (available()) d->send(id,"_NET_WM_DESKTOP",desktop,2); }
void WindowBridge::markDesktop(unsigned long id) {
    if (!available()) return;
    d->set(id,"_NET_WM_WINDOW_TYPE",XA_ATOM,{d->atom("_NET_WM_WINDOW_TYPE_DESKTOP")});
    d->set(id,"_NET_WM_DESKTOP",XA_CARDINAL,{0xffffffff});
}
void WindowBridge::markPanel(unsigned long id,const QRect &geo,int rootHeight) {
    if (!available()) return;
    d->set(id,"_NET_WM_WINDOW_TYPE",XA_ATOM,{d->atom("_NET_WM_WINDOW_TYPE_DOCK")});
    d->set(id,"_NET_WM_DESKTOP",XA_CARDINAL,{0xffffffff});
    d->set(id,"_NET_WM_STATE",XA_ATOM,{d->atom("_NET_WM_STATE_ABOVE"),d->atom("_NET_WM_STATE_SKIP_TASKBAR"),d->atom("_NET_WM_STATE_SKIP_PAGER")});
    unsigned long bottom=qMax(0,rootHeight-geo.top()+8);
    d->set(id,"_NET_WM_STRUT_PARTIAL",XA_CARDINAL,{0,0,0,bottom,0,0,0,0,0,0,static_cast<unsigned long>(qMax(0,geo.left())),static_cast<unsigned long>(qMax(0,geo.right()))});
    d->set(id,"_NET_WM_STRUT",XA_CARDINAL,{0,0,0,bottom});
}
void WindowBridge::activatePopup(unsigned long id) { if (available()) { XSetInputFocus(d->display,id,RevertToPointerRoot,CurrentTime); XFlush(d->display); } }
void WindowBridge::showDesktop(bool show) { if (available()) d->send(d->root,"_NET_SHOWING_DESKTOP",show ? 1 : 0); }
}
