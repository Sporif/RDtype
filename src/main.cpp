#include "main.h"
#include "remote.h"
#include "xkb.h"

#include <iostream>
#include <chrono>
#include <thread>

#include <QCoreApplication>
#include <QCommandLineParser>

#include <linux/input-event-codes.h>

RDtype::RDtype(QObject *parent)
    : QObject(parent)
    , m_connectionThread(new QThread(this))
    , m_connectionThreadObject(new ConnectionThread())
{
    init();
}

RDtype::~RDtype()
{
    quit();
}

void RDtype::quit() {
    m_connectionThread->quit();
    m_connectionThread->wait();
    m_connectionThreadObject->deleteLater();
    if (m_seat)
        m_seat->release();
    if (m_registry)
        m_registry->release();
}

void RDtype::init()
{
    connect(
        m_connectionThreadObject,
        &ConnectionThread::connected,
        this,
        [this] {
            m_eventQueue = new EventQueue(this);
            m_eventQueue->setup(m_connectionThreadObject);

            m_registry = new Registry(this);
            setupRegistry();
        },
        Qt::QueuedConnection
    );;

    m_connectionThreadObject->moveToThread(m_connectionThread);
    m_connectionThread->start();
    m_connectionThreadObject->initConnection();
}

void RDtype::setupRegistry()
{
    connect(m_registry, &Registry::seatAnnounced, this, [this](quint32 name, quint32 version) {
        m_seat = m_registry->createSeat(name, version, this);
        connect(m_seat, &Seat::hasKeyboardChanged, this, [this](bool has) {
            if (!has || m_keyboard) {
                return;
            }
            m_keyboard = m_seat->createKeyboard(this);
            connect(m_keyboard, &Keyboard::keymapChanged, this, [this](int fd, quint32 size) {
                Xkb::self()->keyboardKeymap(fd, size);
                Q_EMIT keymapChanged();
            });
        });
    });

    m_registry->setEventQueue(m_eventQueue);
    m_registry->create(m_connectionThreadObject);
    m_registry->setup();
}

int handleText(const QStringList& text, RemoteDesktopSession& rdsession)
{
    auto ret = 0;
    auto xkb = Xkb::self();

    if (!rdsession.isValid()) {
        std::cerr << "Unable to handle remote input. RemoteDesktop portal not authenticated\n";
        return 1;
    } else {
        // std::cout << "Handling text\n";
    }

    for (const auto& string : text) {
        QVector<uint> ucs4_string = string.toUcs4();
        for (const auto& ch : ucs4_string) {
            // std::cout << "Character: Ox" << std::format("{:X}", ch) << "\n";
            xkb_keysym_t keysym = xkb_utf32_to_keysym(ch);
            if (keysym == XKB_KEY_NoSymbol) {
                std::cerr << "Failed to convert character '0x" << std::format("{:X}", ch) << "' to keysym\n";
                ret = 2;
                continue;
            }

            auto keycode = xkb->keycodeFromKeysym(keysym);
            if (!keycode) {
                // Type using CTRL+SHIFT+U <UNICODE HEX>
                rdsession.keyPress(KEY_LEFTCTRL);
                rdsession.keyPress(KEY_LEFTSHIFT);
                rdsession.sendKey(KEY_U);
                rdsession.keyRelease(KEY_LEFTCTRL);
                rdsession.keyRelease(KEY_LEFTSHIFT);
                std::string ch_hex = std::format("{:x}", ch);
                for(char& ch : ch_hex) {
                    xkb_keysym_t keysym = xkb_utf32_to_keysym(ch);
                    auto keycode = xkb->keycodeFromKeysym(keysym);
                    rdsession.sendKey(keycode->code);
                }
                rdsession.sendKey(KEY_SPACE);
                continue;
            }

            switch (keycode->level) {
            case 0:
                rdsession.sendKey(keycode->code);
                break;
            case 1:
                rdsession.keyPress(KEY_LEFTSHIFT);
                rdsession.sendKey(keycode->code);
                rdsession.keyRelease(KEY_LEFTSHIFT);
                break;
            case 2:
                rdsession.keyPress(KEY_RIGHTALT);
                rdsession.sendKey(keycode->code);
                rdsession.keyRelease(KEY_RIGHTALT);
                break;
            case 3:
                rdsession.keyPress(KEY_LEFTSHIFT);
                rdsession.keyPress(KEY_RIGHTALT);
                rdsession.sendKey(keycode->code);
                rdsession.keyRelease(KEY_LEFTSHIFT);
                rdsession.keyRelease(KEY_RIGHTALT);
                break;
            default:
                std::cerr << "Unsupported key level: " << (keycode->level + 1) << ", key code: " << keycode->code << "\n";
                ret = 2;
                break;
            }
        }
    }

    return ret;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("RDtype");
    QCoreApplication::setApplicationVersion("0.1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("Virtual keyboard input through remote desktop portal");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("text", QCoreApplication::translate("main", "Text to type"));
    parser.process(app);
    const QStringList text = parser.positionalArguments();

    RDtype rdtype(&app);
    RemoteDesktopSession rdsession;

    QObject::connect(&rdtype, &RDtype::keymapChanged, [&text, &rdsession] {
        QObject::connect(&rdsession, &RemoteDesktopSession::sessionStarted, [&text, &rdsession] {
            auto ret = handleText(text, rdsession);
            QCoreApplication::exit(ret);
        });
        rdsession.createSession();
    });

    return app.exec();
}
