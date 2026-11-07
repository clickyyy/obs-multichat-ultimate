#include "streamerbot_client.hpp"
#include "hub.hpp"
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QStringList>
#include <cstring>
#include <initializer_list>
#include <util/base.h>

namespace mcu {

namespace {

constexpr int kMaxBuffer = 8 << 20;
int g_rawLogged = 0; // solo se vuelcan los primeros eventos crudos, para ajustar el parser

void logLine(const QString &s)
{
	blog(LOG_INFO, "[OBS Multichat Ultimate] %s", s.toUtf8().constData());
}

QString scalarToString(const QJsonValue &v)
{
	if (v.isString())
		return v.toString();
	if (v.isDouble())
		return QString::number(v.toDouble(), 'f', 0);
	return QString();
}

// Devuelve el primer valor no vacio entre varias claves posibles.
QString pick(const QJsonObject &o, std::initializer_list<const char *> keys)
{
	for (const char *k : keys) {
		const QJsonValue v = o.value(QLatin1String(k));
		QString s;
		if (v.isObject()) {
			const QJsonObject c = v.toObject();
			s = scalarToString(c.value(QLatin1String("text")));
			if (s.isEmpty())
				s = scalarToString(c.value(QLatin1String("message")));
		} else {
			s = scalarToString(v);
		}
		if (!s.isEmpty())
			return s;
	}
	return QString();
}

void addBadge(ChatMessage &m, const QString &raw)
{
	const QString r = raw.toLower();
	const char *b = nullptr;
	if (r.contains(QLatin1String("broadcaster")) || r.contains(QLatin1String("owner")))
		b = "owner";
	else if (r.contains(QLatin1String("mod")))
		b = "moderator";
	else if (r.contains(QLatin1String("vip")))
		b = "vip";
	else if (r.contains(QLatin1String("sub")) || r.contains(QLatin1String("sponsor")) ||
		 r.contains(QLatin1String("member")))
		b = "subscriber";
	if (!b || m.hasBadge(b))
		return;
	m.badges.push_back(b);
}

} // namespace

StreamerbotClient::StreamerbotClient(QObject *parent) : QObject(parent)
{
	retry_.setSingleShot(true);
	connect(&retry_, &QTimer::timeout, this, [this] {
		if (wanted_)
			openSocket();
	});

	connect(&sock_, &QTcpSocket::connected, this, &StreamerbotClient::onConnected);
	connect(&sock_, &QTcpSocket::readyRead, this, &StreamerbotClient::onReadyRead);
	connect(&sock_, &QTcpSocket::disconnected, this, [this] {
		if (wanted_)
			scheduleRetry(tr("Conexion perdida"));
	});
	connect(&sock_, &QAbstractSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
		if (wanted_)
			scheduleRetry(sock_.errorString());
	});
}

void StreamerbotClient::start(const QString &host, quint16 port)
{
	wanted_ = false;
	retry_.stop();
	sock_.abort();
	host_ = host;
	port_ = port;
	wanted_ = true;
	backoffMs_ = 1000;
	openSocket();
}

void StreamerbotClient::stop()
{
	wanted_ = false;
	retry_.stop();
	sock_.abort();
	setAll(static_cast<int>(ConnState::Disconnected));
}

void StreamerbotClient::setAll(int state, const QString &detail)
{
	auto &hub = MessageHub::instance();
	hub.setStatus(QStringLiteral("kick"), static_cast<ConnState>(state), detail);
	hub.setStatus(QStringLiteral("youtube"), static_cast<ConnState>(state), detail);
}

void StreamerbotClient::openSocket()
{
	sock_.abort();
	buf_.clear();
	frag_.clear();
	handshakeDone_ = false;
	setAll(static_cast<int>(ConnState::Connecting), QStringLiteral("Streamer.bot"));
	sock_.connectToHost(host_, port_);
}

void StreamerbotClient::scheduleRetry(const QString &reason)
{
	if (retry_.isActive())
		return;
	setAll(static_cast<int>(ConnState::Error),
	       tr("Streamer.bot: %1. Reintentando en %2 s").arg(reason).arg(backoffMs_ / 1000));
	retry_.start(backoffMs_);
	backoffMs_ = qMin(backoffMs_ * 2, 30000);
}

void StreamerbotClient::fail(const QString &reason)
{
	if (wanted_)
		scheduleRetry(reason); // primero el reintento, para que abort() no lo pise
	sock_.abort();
}

// ---------------------------------------------------------------- WebSocket

void StreamerbotClient::onConnected()
{
	buf_.clear();
	frag_.clear();
	handshakeDone_ = false;

	QByteArray raw;
	for (int i = 0; i < 4; ++i) {
		const quint32 r = QRandomGenerator::global()->generate();
		raw.append(reinterpret_cast<const char *>(&r), 4);
	}
	const QByteArray key = raw.toBase64();

	QByteArray req = "GET / HTTP/1.1\r\nHost: " + host_.toUtf8() + ":" + QByteArray::number(port_) +
			 "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: " + key +
			 "\r\nSec-WebSocket-Version: 13\r\n\r\n";
	sock_.write(req);
}

void StreamerbotClient::onReadyRead()
{
	buf_ += sock_.readAll();
	if (buf_.size() > kMaxBuffer) {
		fail(tr("Datos inesperados"));
		return;
	}

	if (!handshakeDone_) {
		const int end = buf_.indexOf("\r\n\r\n");
		if (end < 0)
			return;
		const QByteArray head = buf_.left(end);
		buf_.remove(0, end + 4);
		if (!head.startsWith("HTTP/1.1 101")) {
			fail(tr("el servidor rechazo la conexion WebSocket"));
			return;
		}
		handshakeDone_ = true;
		backoffMs_ = 1000;
		onHandshakeDone();
	}
	processFrames();
}

void StreamerbotClient::onHandshakeDone()
{
	setAll(static_cast<int>(ConnState::Connected), QStringLiteral("via Streamer.bot"));
	logLine(QStringLiteral("Conectado a Streamer.bot en %1:%2").arg(host_).arg(port_));

	// Dos suscripciones separadas: si un nombre de evento fuese invalido, la otra sigue funcionando.
	sendText(R"({"request":"Subscribe","id":"mcu-kick","events":{"Kick":["ChatMessage"]}})");
	sendText(R"({"request":"Subscribe","id":"mcu-youtube","events":{"YouTube":["Message"]}})");
	sendText(R"({"request":"GetEvents","id":"mcu-events"})");
}

void StreamerbotClient::processFrames()
{
	for (;;) {
		if (buf_.size() < 2)
			return;
		const quint8 b0 = static_cast<quint8>(buf_[0]);
		const quint8 b1 = static_cast<quint8>(buf_[1]);
		const bool fin = (b0 & 0x80) != 0;
		const quint8 op = b0 & 0x0F;
		const bool masked = (b1 & 0x80) != 0;
		quint64 len = b1 & 0x7F;
		int pos = 2;

		if (len == 126) {
			if (buf_.size() < 4)
				return;
			len = (static_cast<quint64>(static_cast<quint8>(buf_[2])) << 8) |
			      static_cast<quint8>(buf_[3]);
			pos = 4;
		} else if (len == 127) {
			if (buf_.size() < 10)
				return;
			len = 0;
			for (int i = 0; i < 8; ++i)
				len = (len << 8) | static_cast<quint8>(buf_[2 + i]);
			pos = 10;
		}

		QByteArray mask;
		if (masked) {
			if (buf_.size() < pos + 4)
				return;
			mask = buf_.mid(pos, 4);
			pos += 4;
		}

		if (len > static_cast<quint64>(kMaxBuffer)) {
			fail(tr("mensaje demasiado grande"));
			return;
		}
		if (static_cast<quint64>(buf_.size()) < static_cast<quint64>(pos) + len)
			return; // falta el resto del frame

		QByteArray payload = buf_.mid(pos, static_cast<int>(len));
		buf_.remove(0, pos + static_cast<int>(len));
		if (masked) {
			for (int i = 0; i < payload.size(); ++i)
				payload[i] = static_cast<char>(payload[i] ^ mask[i % 4]);
		}

		switch (op) {
		case 0x8: // close
			fail(tr("Streamer.bot cerro la conexion"));
			return;
		case 0x9: // ping
			sendFrame(0xA, payload);
			break;
		case 0xA: // pong
			break;
		case 0x0: // continuacion
		case 0x1: // texto
		case 0x2: // binario
			if (op != 0x0)
				frag_.clear();
			frag_ += payload;
			if (fin) {
				const QByteArray msg = frag_;
				frag_.clear();
				handleText(msg);
			}
			break;
		default:
			break;
		}
	}
}

void StreamerbotClient::sendText(const QByteArray &payload)
{
	sendFrame(0x1, payload);
}

void StreamerbotClient::sendFrame(quint8 opcode, const QByteArray &payload)
{
	if (sock_.state() != QAbstractSocket::ConnectedState)
		return;

	const int n = payload.size();
	QByteArray f;
	f.append(static_cast<char>(0x80 | opcode));
	if (n < 126) {
		f.append(static_cast<char>(0x80 | n));
	} else if (n <= 0xFFFF) {
		f.append(static_cast<char>(0x80 | 126));
		f.append(static_cast<char>((n >> 8) & 0xFF));
		f.append(static_cast<char>(n & 0xFF));
	} else {
		f.append(static_cast<char>(0x80 | 127));
		for (int i = 7; i >= 0; --i)
			f.append(static_cast<char>((static_cast<quint64>(n) >> (8 * i)) & 0xFF));
	}

	const quint32 r = QRandomGenerator::global()->generate();
	char m[4];
	std::memcpy(m, &r, 4);
	f.append(m, 4); // los frames del cliente deben ir enmascarados
	for (int i = 0; i < n; ++i)
		f.append(static_cast<char>(payload[i] ^ m[i % 4]));
	sock_.write(f);
}

// ------------------------------------------------------------------ Mensajes

void StreamerbotClient::handleText(const QByteArray &text)
{
	QJsonParseError err;
	const QJsonDocument doc = QJsonDocument::fromJson(text, &err);
	if (err.error != QJsonParseError::NoError || !doc.isObject())
		return;
	const QJsonObject o = doc.object();
	if (o.contains(QLatin1String("event")))
		handleEvent(o);
	else
		handleResponse(o);
}

void StreamerbotClient::handleResponse(const QJsonObject &o)
{
	const QString id = o.value(QLatin1String("id")).toString();
	if (id == QLatin1String("mcu-events")) {
		// Solo interesa saber como llama Streamer.bot a los eventos de Kick y YouTube.
		const QJsonObject evs = o.value(QLatin1String("events")).toObject();
		for (auto it = evs.begin(); it != evs.end(); ++it) {
			const QString k = it.key().toLower();
			if (k != QLatin1String("kick") && k != QLatin1String("youtube"))
				continue;
			QStringList names;
			const QJsonArray arr = it.value().toArray();
			for (const QJsonValue &v : arr)
				names << v.toString();
			logLine(QStringLiteral("Streamer.bot eventos %1: %2").arg(it.key(), names.join(QStringLiteral(", "))));
		}
		return;
	}
	const QString s = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
	logLine(QStringLiteral("Streamer.bot respuesta: ") + s.left(300));
}

void StreamerbotClient::handleEvent(const QJsonObject &o)
{
	const QJsonObject ev = o.value(QLatin1String("event")).toObject();
	const QString src = ev.value(QLatin1String("source")).toString();
	const QString type = ev.value(QLatin1String("type")).toString();

	QString platform;
	if (src.compare(QLatin1String("Kick"), Qt::CaseInsensitive) == 0 && type == QLatin1String("ChatMessage"))
		platform = QStringLiteral("kick");
	else if (src.compare(QLatin1String("YouTube"), Qt::CaseInsensitive) == 0 && type == QLatin1String("Message"))
		platform = QStringLiteral("youtube");
	else
		return;

	// Los primeros eventos se vuelcan crudos al log para poder ajustar el parser.
	if (g_rawLogged < 6) {
		++g_rawLogged;
		const QString raw = QString::fromUtf8(QJsonDocument(o).toJson(QJsonDocument::Compact));
		logLine(QStringLiteral("Evento crudo %1.%2: %3").arg(src, type, raw.left(1500)));
	}

	const QJsonObject d = o.value(QLatin1String("data")).toObject();

	QJsonObject u;
	if (d.value(QLatin1String("user")).isObject())
		u = d.value(QLatin1String("user")).toObject();
	else if (d.value(QLatin1String("author")).isObject())
		u = d.value(QLatin1String("author")).toObject();

	ChatMessage m;
	m.platform = platform.toStdString();

	QString text = pick(d, {"text", "message", "messageText", "displayMessage"});
	if (text.trimmed().isEmpty())
		return;
	m.text = text.toStdString();

	QString name = pick(u, {"name", "displayName", "display_name", "login", "username", "userName"});
	if (name.isEmpty())
		name = pick(d, {"userName", "username", "displayName", "user"});
	if (name.isEmpty())
		name = QStringLiteral("?");
	m.user = name.toStdString();

	QString uid = pick(u, {"id", "userId", "login"});
	if (uid.isEmpty())
		uid = pick(d, {"userId"});
	m.userId = uid.toStdString();

	const QString color = pick(u, {"color", "nameColor"});
	if (color.startsWith(QLatin1Char('#')) && color.size() == 7)
		m.color = color.toStdString();

	m.avatar = pick(u, {"profileImageUrl", "profile_image_url", "profilePicture", "avatar", "imageUrl"}).toStdString();

	// Insignias: banderas booleanas (isModerator, isOwner...), un rol o una lista de insignias.
	for (auto it = u.begin(); it != u.end(); ++it) {
		if (it.value().isBool() && it.value().toBool())
			addBadge(m, it.key());
	}
	const QString role = pick(u, {"role"});
	if (!role.isEmpty())
		addBadge(m, role);
	const QJsonArray badges = u.value(QLatin1String("badges")).toArray();
	for (const QJsonValue &b : badges) {
		if (b.isString())
			addBadge(m, b.toString());
		else if (b.isObject())
			addBadge(m, pick(b.toObject(), {"name", "type", "text"}));
	}

	QString id = pick(d, {"messageId", "id", "eventId"});
	if (id.isEmpty())
		id = QStringLiteral("sb-%1").arg(++counter_);
	m.id = id.toStdString();

	m.timestampMs = QDateTime::currentMSecsSinceEpoch();

	MessageHub::instance().submit(m);
}

} // namespace mcu
