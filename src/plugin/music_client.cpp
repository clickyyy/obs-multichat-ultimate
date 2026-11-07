#include "music_client.hpp"
#include "hub.hpp"
#include "smtc_reader.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>

namespace mcu {

MusicClient::MusicClient(QObject *parent) : QObject(parent)
{
	// Los callbacks se ejecutan en el hilo de lectura. Se detiene en ~MusicClient antes de destruir nada,
	// así que capturar `this` es seguro. Todo lo que toca Qt pasa a la cola del hilo de la interfaz.
	reader_ = std::make_unique<SmtcReader>(
		[this](const MusicState &st) {
			const bool show = st.active && !st.title.empty();
			QByteArray json = "{}"; // objeto vacío = la fuente oculta el reproductor
			QString label;
			if (show) {
				QJsonObject o;
				o.insert("title", QString::fromStdString(st.title));
				o.insert("artist", QString::fromStdString(st.artist));
				o.insert("album", QString::fromStdString(st.album));
				o.insert("source", QString::fromStdString(st.source));
				o.insert("cover", QString::fromStdString(st.coverDataUri));
				o.insert("durationMs", (qint64)st.durationMs);
				o.insert("positionMs", (qint64)st.positionMs);
				o.insert("playing", st.playing);
				json = QJsonDocument(o).toJson(QJsonDocument::Compact);
				label = QString::fromStdString(st.title);
				if (!st.artist.empty())
					label += QString::fromUtf8(" — ") + QString::fromStdString(st.artist);
				if (!st.playing)
					label += QString::fromUtf8(" (en pausa)");
			}
			const QString detail = show ? QString::fromStdString(st.source) : tr("sin reproducción");
			QMetaObject::invokeMethod(
				this,
				[this, json, label, detail] {
					MessageHub::instance().publishNowPlaying(json);
					MessageHub::instance().setStatus("music", ConnState::Connected, detail);
					emit trackChanged(label);
				},
				Qt::QueuedConnection);
		},
		[this](const std::vector<MusicSession> &list) {
			QStringList ids, names;
			for (auto &s : list) {
				ids << QString::fromStdString(s.id);
				names << QString::fromStdString(s.name);
			}
			QMetaObject::invokeMethod(
				this, [this, ids, names] { emit sessionsChanged(ids, names); }, Qt::QueuedConnection);
		},
		[this](const std::string &err) {
			const QString msg = QString::fromStdString(err);
			QMetaObject::invokeMethod(
				this,
				[msg] { MessageHub::instance().setStatus("music", ConnState::Error, msg); },
				Qt::QueuedConnection);
		});
}

MusicClient::~MusicClient()
{
	if (reader_)
		reader_->stop();
}

void MusicClient::setEnabled(bool on)
{
	if (on == enabled_)
		return;
	enabled_ = on;
	auto &hub = MessageHub::instance();
	if (on) {
		hub.setStatus("music", ConnState::Connecting, tr("Buscando reproductores…"));
		reader_->start();
	} else {
		reader_->stop();
		hub.publishNowPlaying("{}");
		hub.setStatus("music", ConnState::Disconnected);
		emit trackChanged(QString());
	}
}

void MusicClient::setPreferred(const QString &sessionId)
{
	reader_->setPreferred(sessionId.toStdString());
}

} // namespace mcu
