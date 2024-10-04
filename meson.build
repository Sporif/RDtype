project(
    'RDtype',
    'cpp',
    version: '0.1.0',
    license: 'MIT',
    default_options: [
        'buildtype=release',
        'cpp_std=c++20'
    ],
)
project_name = meson.project_name().to_lower()

qt6 = import('qt6')
qt6_deps = dependency('qt6', modules: ['Core', 'DBus'])
kconfig = dependency('KF6Config', method: 'cmake', modules: ['KF6::ConfigCore'])
kwaylandclient = dependency('KWaylandClient')
xkbcommon = dependency('xkbcommon')

qdbusxml2cpp = find_program('qdbusxml2cpp', dirs: ['/usr/lib/qt6/bin'])
remotedesktop_src = custom_target('remotedesktop',
                        input: ['dbus/org.freedesktop.portal.RemoteDesktop.xml'],
                        output: ['remotedesktop.h', 'remotedesktop.cpp'],
                        command: [qdbusxml2cpp, '@INPUT@', '-p', '@OUTPUT0@:@OUTPUT1@'])

qtprocessed = qt6.compile_moc(headers: [
    'src/main.h',
    'src/remote.h',
    remotedesktop_src[0],
])

src_files = files(
    'src/main.cpp',
    'src/remote.cpp',
)

executable(
    project_name,
    src_files,
    remotedesktop_src[1],
    qtprocessed,
    dependencies: [
        qt6_deps,
        kconfig,
        kwaylandclient,
        xkbcommon,
    ],
    install : true,
)
