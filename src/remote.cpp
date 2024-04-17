#include "remote.h"

#include <iostream>
#include <chrono>
#include <thread>

// #include <QDebug>
#include <QSizeF>
#include <QDBusPendingCallWatcher>

#include <KConfigGroup>
#include <KSharedConfig>

#include <linux/input-event-codes.h>
#include <xkbcommon/xkbcommon.h>

// Q_GLOBAL_STATIC(RemoteDesktopSession, s_session);

RemoteDesktopSession::RemoteDesktopSession()
    : iface(new OrgFreedesktopPortalRemoteDesktopInterface(QStringLiteral("org.freedesktop.portal.Desktop"),
                                                           QStringLiteral("/org/freedesktop/portal/desktop"),
                                                           QDBusConnection::sessionBus(),
                                                           this))
{
}

void RemoteDesktopSession::createSession()
{
    if (isValid()) {
        std::cout << "pass, already created\n";
        return;
    }

    m_connecting = true;

    // create session
    const auto handleToken = QStringLiteral("rdtype%1").arg(QRandomGenerator::global()->generate());
    const auto sessionParameters = QVariantMap{{QStringLiteral("session_handle_token"), handleToken}, {QStringLiteral("handle_token"), handleToken}};
    auto sessionReply = iface->CreateSession(sessionParameters);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(sessionReply);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, sessionReply](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (sessionReply.isError()) {
            std::cerr << "Could not create the remote control session\n"; // << sessionReply.error() << "\n";
            m_connecting = false;
            return;
        }

        bool b = QDBusConnection::sessionBus().connect(QString(),
                                                       sessionReply.value().path(),
                                                       QStringLiteral("org.freedesktop.portal.Request"),
                                                       QStringLiteral("Response"),
                                                       this,
                                                       SLOT(handleXdpSessionCreated(uint, QVariantMap)));
        Q_ASSERT(b);
        // std::cout << "authenticating\n"; //<< sessionReply.value().path() << "\n";
    });
}

void RemoteDesktopSession::handleXdpSessionCreated(uint code, const QVariantMap &results)
{
    if (code != 0) {
        std::cerr << "Failed to create session with code\n"; //<< code << results << "\n";
        return;
    }
    m_connecting = false;
    m_xdpPath = QDBusObjectPath(results.value(QStringLiteral("session_handle")).toString());
    QVariantMap selectParameters = {
        {QStringLiteral("handle_token"), QStringLiteral("rdtype%1").arg(QRandomGenerator::global()->generate())},
        {QStringLiteral("types"), QVariant::fromValue<uint>(1)}, // request KeyBoard
        {QStringLiteral("persist_mode"), QVariant::fromValue<uint>(2)}, // persist permissions until explicitly revoked by user
    };

    KConfigGroup stateConfig = KSharedConfig::openStateConfig()->group(QStringLiteral("rdtype"));
    QString restoreToken = stateConfig.readEntry(QStringLiteral("RestoreToken"), QString());
    // std::cout << "restoreToken read: " << restoreToken.toStdString() << "\n";

    if (restoreToken.length() > 0) {
        selectParameters[QLatin1String("restore_token")] = restoreToken;
    }

    QDBusConnection::sessionBus().connect(QString(),
                                          m_xdpPath.path(),
                                          QStringLiteral("org.freedesktop.portal.Session"),
                                          QStringLiteral("Closed"),
                                          this,
                                          SLOT(handleXdpSessionFinished(uint, QVariantMap)));

    auto reply = iface->SelectDevices(m_xdpPath, selectParameters);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, reply](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (reply.isError()) {
            std::cerr << "Could not configure the remote control session\n"; //<< reply.error() << "\n";
            m_connecting = false;
            return;
        }

        bool b = QDBusConnection::sessionBus().connect(QString(),
                                                       reply.value().path(),
                                                       QStringLiteral("org.freedesktop.portal.Request"),
                                                       QStringLiteral("Response"),
                                                       this,
                                                       SLOT(handleXdpSessionConfigured(uint, QVariantMap)));
        Q_ASSERT(b);
        // std::cout << "configuring\n"; //<< reply.value().path() << "\n";
    });
}

void RemoteDesktopSession::handleXdpSessionConfigured(uint code, const QVariantMap &results)
{
    if (code != 0) {
        std::cerr << "Failed to configure session with code\n"; //<< code << results << "\n";
        m_connecting = false;
        return;
    }
    const QVariantMap startParameters = {
        {QStringLiteral("handle_token"), QStringLiteral("rdtype%1").arg(QRandomGenerator::global()->generate())},
    };
    auto reply = iface->Start(m_xdpPath, {}, startParameters);
    QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply);
    connect(watcher, &QDBusPendingCallWatcher::finished, this, [this, reply](QDBusPendingCallWatcher *self) {
        self->deleteLater();
        if (reply.isError()) {
            std::cerr << "Could not start the remote control session\n"; //<< reply.error() << "\n";
            m_connecting = false;
            return;
        }

        bool b = QDBusConnection::sessionBus().connect(QString(),
                                                       reply.value().path(),
                                                       QStringLiteral("org.freedesktop.portal.Request"),
                                                       QStringLiteral("Response"),
                                                       this,
                                                       SLOT(handleXdpSessionStarted(uint, QVariantMap)));
        Q_ASSERT(b);
        // std::cout << "starting\n"; //<< reply.value().path() << "\n";
    });
}

void RemoteDesktopSession::handleXdpSessionStarted(uint code, const QVariantMap &results)
{
    if (code != 0) {
        std::cerr << "Failed to start session with code\n"; //<< code << results << "\n";
        m_connecting = false;
        return;
    }

    auto devices = results.value(QStringLiteral("devices"));
    Q_ASSERT(devices == 1);

    KConfigGroup stateConfig = KSharedConfig::openStateConfig()->group(QStringLiteral("rdtype"));
    QString restoreToken = results[QStringLiteral("restore_token")].toString();
    // std::cout << "restoreToken written: " << restoreToken.toStdString() << "\n";
    stateConfig.writeEntry(QStringLiteral("RestoreToken"), restoreToken);

    Q_EMIT sessionStarted();
}

void RemoteDesktopSession::handleXdpSessionFinished(uint /*code*/, const QVariantMap & /*results*/)
{
    m_xdpPath = {};
}

void RemoteDesktopSession::keyPress(quint32 keyCode)
{
    iface->NotifyKeyboardKeycode(m_xdpPath, {}, keyCode, 1).waitForFinished();
}

void RemoteDesktopSession::keyRelease(quint32 keyCode)
{
    iface->NotifyKeyboardKeycode(m_xdpPath, {}, keyCode, 0).waitForFinished();
}

void RemoteDesktopSession::sendKey(quint32 keyCode)
{
    keyPress(keyCode);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    keyRelease(keyCode);
}
