#include "hub.hpp"
#include "music_client.hpp"
#include "overlay_server.hpp"
#include "plugin_api.hpp"
#include "twitch_client.hpp"
#include "streamerbot_client.hpp"
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <QDir>

OBS_DECLARE_MODULE()

MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS Multichat Ultimate: chat multiplataforma, reloj y Now Playing";
}
MODULE_EXPORT const char *obs_module_name(void) { return "OBS Multichat Ultimate"; }

static mcu::OverlayServer *g_server = nullptr;
static mcu::TwitchClient *g_twitch = nullptr;
static mcu::StreamerbotClient *g_sb = nullptr;
static mcu::MusicClient *g_music = nullptr;
static bool g_dock = false;
static const char *DOCK_ID = "obs-multichat-ultimate-dock";

quint16 mcuOverlayPort() { return g_server ? g_server->port() : 0; }

static void onFrontendEvent(enum obs_frontend_event ev, void *)
{
	if (ev != OBS_FRONTEND_EVENT_FINISHED_LOADING || g_dock)
		return;
	QWidget *w = mcuCreateDock(g_twitch, g_music);
	g_dock = obs_frontend_add_dock_by_id(DOCK_ID, "OBS MULTICHAT ULTIMATE", w);
	auto &hub = mcu::MessageHub::instance();
	if (hub.twitchAutoConnect() && !hub.twitchChannel().isEmpty())
		g_twitch->connectToChannel(hub.twitchChannel());
        // Kick y YouTube llegan por el servidor WebSocket local de Streamer.bot.
        if (g_sb)  g_sb->start();
	// La música se activa después de crear el panel para que este reciba la lista de reproductores.
	g_music->setPreferred(hub.musicSource());
	g_music->setEnabled(hub.musicEnabled());
}

bool obs_module_load(void)
{
	auto &hub = mcu::MessageHub::instance();
	hub.load();

	g_server = new mcu::OverlayServer();
	char *data = obs_module_file("");
	QString dataDir = data ? QString::fromUtf8(data) : QString();
	bfree(data);
	if (!g_server->start(dataDir)) {
		blog(LOG_ERROR, "[OBS Multichat Ultimate] No se pudo abrir el servidor local");
		return false;
	}
	QObject::connect(&hub, &mcu::MessageHub::messageJson, g_server, &mcu::OverlayServer::onMessage);
	QObject::connect(&hub, &mcu::MessageHub::styleJson, g_server, &mcu::OverlayServer::onStyle);
	QObject::connect(&hub, &mcu::MessageHub::nowPlaying, g_server, &mcu::OverlayServer::onNowPlaying);

	g_twitch = new mcu::TwitchClient();
        g_sb = new mcu::StreamerbotClient();
	g_music = new mcu::MusicClient();
	mcuRegisterSources();
	obs_frontend_add_event_callback(onFrontendEvent, nullptr);
	blog(LOG_INFO, "[OBS Multichat Ultimate] Servidor local en 127.0.0.1:%u", g_server->port());
	return true;
}

void obs_module_unload(void)
{
	obs_frontend_remove_event_callback(onFrontendEvent, nullptr);
	if (g_dock)
		obs_frontend_remove_dock(DOCK_ID);
	delete g_music; // detiene el hilo de lectura antes de cerrar el resto
	if (g_sb) { g_sb->stop(); delete g_sb; g_sb = nullptr; }
        delete g_twitch;
	delete g_server;
	g_music = nullptr;
	g_twitch = nullptr;
	g_server = nullptr;
}
