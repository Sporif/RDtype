#pragma once

#include "remotedesktop.h"

#include <QDBusObjectPath>

class RemoteDesktopSession : public QObject
{
    Q_OBJECT
public:
    RemoteDesktopSession();
    void createSession();
    bool isValid() const
    {
        return m_connecting || !m_xdpPath.path().isEmpty();
    }
    void keyPress(quint32 keyCode);
    void keyRelease(quint32 keyCode);
    void sendKey(quint32 keyCode);

    OrgFreedesktopPortalRemoteDesktopInterface *const iface;
    QDBusObjectPath m_xdpPath;
    bool m_connecting = false;

Q_SIGNALS:
    void sessionStarted();

private Q_SLOTS:
    void handleXdpSessionCreated(uint code, const QVariantMap &results);
    void handleXdpSessionConfigured(uint code, const QVariantMap &results);
    void handleXdpSessionFinished(uint code, const QVariantMap &results);
    void handleXdpSessionStarted(uint code, const QVariantMap &results);

private:
};
