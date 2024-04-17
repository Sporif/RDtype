# RDtype

A cli tool that provides virtual keyboard input on Wayland.

It uses the [Remote desktop portal](https://flatpak.github.io/xdg-desktop-portal/docs/doc-org.freedesktop.impl.portal.RemoteDesktop.html).

## Limitations

- A notification is shown every time the tool is used (at least with xdg-desktop-portal-kde)
  Workaround: Disable popups for KDE Portal Integration in Notification settings
- Persistence does not work across reboots on KDE (https://bugs.kde.org/show_bug.cgi?id=480235)

## Dependencies

- C++ compiler (GCC/Clang)
- Qt6
- KF6Config
- KWayland
- xkbcommon

## Build and Install

```
meson setup --buildtype=release build
meson install -C build
```

## Usage

Run the following command:

```
rdtype <text>
```
