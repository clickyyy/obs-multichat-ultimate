#pragma once
// Servicio central: recibe mensajes de todas las plataformas, deduplica, filtra y los difunde.
// Vive en el hilo de la interfaz de Qt (no bloquea OBS: toda la E/S de red es asíncrona).
#include "core/filters.hpp"
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

namespace mcu {

enum class ConnState { Disconnected = 0, Connecting = 1, Connected = 2, Error = 3 };

class MessageHub : public QObject {
	Q_OBJECT
public:
	static MessageHub &instance();

	void load();
	void save() const;

	// Entrada de mensajes (cualquier plataforma).
	void submit(const ChatMessage &m);

	// Estilo global (se difunde a todas las fuentes al instante).
	QJsonObject style() const { return config_.value("style").toObject(); }
	void setStyleValue(const QString &key, const QJsonValue &v);
	void resetStyle();

	// Filtros
	FilterConfig &filters() { return filters_; }
	void setFilterBool(const QString &key, bool v);
	void setFilterList(const QString &key, const QStringList &v);
	void setPlatformEnabled(const QString &platform, bool on);
	QStringList filterList(const QString &key) const;

	// Ajustes de plataformas
	QString twitchChannel() const { return config_.value("twitch").toObject().value("channel").toString(); }
	bool twitchAutoConnect() const { return config_.value("twitch").toObject().value("autoconnect").toBool(true); }
	void setTwitch(const QString &channel, bool autoconnect);

	// Música (Now Playing por Windows SMTC)
	bool musicEnabled() const { return config_.value("music").toObject().value("enabled").toBool(true); }
	QString musicSource() const { return config_.value("music").toObject().value("source").toString(); }
	void setMusic(bool enabled, const QString &source);

	// Estado y estadísticas
	void setStatus(const QString &platform, ConnState s, const QString &detail = QString());
	ConnState status(const QString &platform) const;
	int received() const { return received_; }
	int hidden() const { return hidden_; }
	QString lastMessage() const { return last_; }

	// Now Playing (lo alimentará el módulo de música).
	void publishNowPlaying(const QByteArray &json) { emit nowPlaying(json); }

signals:
	void messageJson(const QByteArray &json);
	void styleJson(const QByteArray &json);
	void nowPlaying(const QByteArray &json);
	void statusChanged(const QString &platform, int state, const QString &detail);
	void statsChanged();

private:
	MessageHub() = default;
	QString configPath() const;
	void applyFilters();

	QJsonObject config_;
	FilterConfig filters_;
	Deduper dedupe_{2000};
	QHash<QString, ConnState> states_;
	int received_ = 0, hidden_ = 0;
	QString last_;
};

} // namespace mcu
