#include "twitch_client.hpp"
#include "core/irc_parser.hpp"
#include "hub.hpp"
#include <QDateTime>
#include <QRandomGenerator>

namespace mcu {

static const char *HOST = "irc.chat.twitch.tv";
static const quint16 PORT = 6697;

TwitchClient::TwitchClient(QObject *parent) : QObject(parent)
{
	retry_.setSingleShot(true);
	connect(&retry_, &QTimer::timeout, this, [this] { if (wanted_) openSocket(); });

	connect(&sock_, &QSslSocket::encrypted, this, [this] {
		backoffMs_ = 1000;
		lastRx_ = QDateTime::currentMSecsSinceEpoch();
		const QString nick = QString("justinfan%1").arg(QRandomGenerator::global()->bounded(10000, 99999));
		sock_.write("CAP REQ :twitch.tv/tags twitch.tv/commands\r\n");
		sock_.write("PASS SCHMOOZE\r\n"); // contraseña ficticia estándar para acceso anónimo
		sock_.write("NICK " + nick.toUtf8() + "\r\n");
		sock_.write("JOIN #" + channel_.toUtf8() + "\r\n");
	});
	connect(&sock_, &QSslSocket::readyRead, this, &TwitchClient::onReadyRead);
	connect(&sock_, &QSslSocket::disconnected, this, [this] {
		if (wanted_) scheduleRetry(tr("Conexión perdida"));
	});
	connect(&sock_, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
		if (wanted_) scheduleRetry(sock_.errorString());
	});

	// Twitch envía PING cada ~5 min; si no llega nada en 7 min, la conexión está muerta.
	watchdog_.setInterval(30000);
	connect(&watchdog_, &QTimer::timeout, this, [this] {
		if (wanted_ && sock_.state() == QAbstractSocket::ConnectedState &&
		    QDateTime::currentMSecsSinceEpoch() - lastRx_ > 7 * 60 * 1000) {
			sock_.abort();
			scheduleRetry(tr("Sin actividad"));
		}
	});
	watchdog_.start();
}

void TwitchClient::connectToChannel(const QString &channel)
{
	QString c = channel.trimmed().toLower();
	if (c.startsWith('#')) c.remove(0, 1);
	if (c.isEmpty()) {
		MessageHub::instance().setStatus("twitch", ConnState::Error, tr("Escribe el nombre del canal"));
		return;
	}
	channel_ = c;
	wanted_ = true;
	backoffMs_ = 1000;
	openSocket();
}

void TwitchClient::disconnectFromChannel()
{
	wanted_ = false;
	retry_.stop();
	sock_.abort();
	MessageHub::instance().setStatus("twitch", ConnState::Disconnected);
}

void TwitchClient::openSocket()
{
	sock_.abort();
	buf_.clear();
	MessageHub::instance().setStatus("twitch", ConnState::Connecting, "#" + channel_);
	sock_.connectToHostEncrypted(HOST, PORT);
}

void TwitchClient::scheduleRetry(const QString &reason)
{
	if (retry_.isActive()) return;
	MessageHub::instance().setStatus("twitch", ConnState::Error,
		tr("%1. Reintentando en %2 s").arg(reason).arg(backoffMs_ / 1000));
	retry_.start(backoffMs_);
	backoffMs_ = qMin(backoffMs_ * 2, 30000);
}

void TwitchClient::onReadyRead()
{
	lastRx_ = QDateTime::currentMSecsSinceEpoch();
	buf_ += sock_.readAll();
	if (buf_.size() > (1 << 20)) { buf_.clear(); return; } // protección ante basura
	int idx;
	while ((idx = buf_.indexOf("\r\n")) >= 0) {
		handleLine(buf_.left(idx));
		buf_.remove(0, idx + 2);
	}
}

void TwitchClient::handleLine(const QByteArray &raw)
{
	IrcLine l;
	if (!parseIrcLine(raw.toStdString(), l))
		return;
	if (l.command == "PING") {
		sock_.write("PONG :" + QByteArray::fromStdString(l.trailing) + "\r\n");
	} else if (l.command == "001") {
		MessageHub::instance().setStatus("twitch", ConnState::Connected, "#" + channel_);
	} else if (l.command == "RECONNECT") { // Twitch pide reconectar (mantenimiento)
		sock_.abort();
		scheduleRetry(tr("Twitch pidió reconectar"));
	} else if (l.command == "PRIVMSG") {
		ChatMessage m;
		if (twitchToMessage(l, m))
			MessageHub::instance().submit(m);
	}
}

} // namespace mcu
