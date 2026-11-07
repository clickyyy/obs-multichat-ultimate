#pragma once
#include <QWidget>
#include <QtGlobal>

namespace mcu {
class TwitchClient;
class MusicClient;
}

quint16 mcuOverlayPort();            // puerto del servidor local (0 si no arrancó)
void mcuRegisterSources();           // registra las fuentes de OBS
QWidget *mcuCreateDock(mcu::TwitchClient *twitch, mcu::MusicClient *music); // construye el panel de administración
