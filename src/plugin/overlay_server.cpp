#include "overlay_server.hpp"
#include "hub.hpp"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QUrl>

namespace mcu {

OverlayServer::OverlayServer(QObject *parent) : QObject(parent)
{
	connect(&server_, &QTcpServer::newConnection, this, &OverlayServer::onNewConnection);
	connect(&keepAlive_, &QTimer::timeout, this, [this] { // comentario SSE: mantiene viva la conexión
		for (auto *s : sse_)
			s->write(": ping\n\n");
	});
	keepAlive_.start(15000);
}

bool OverlayServer::start(const QString &dataDir, quint16 preferred)
{
	dir_ = QDir(dataDir).absoluteFilePath("overlay");
	lastStyle_ = QJsonDocument(MessageHub::instance().style()).toJson(QJsonDocument::Compact);
	// Solo loopback: nunca expuesto a la red local.
	for (quint16 p = preferred; p < preferred + 50; ++p)
		if (server_.listen(QHostAddress::LocalHost, p))
			return true;
	return false;
}

void OverlayServer::onStyle(const QByteArray &json)
{
	lastStyle_ = json;
	broadcast("config", json);
}

void OverlayServer::broadcast(const QByteArray &event, const QByteArray &json)
{
	QByteArray frame = "event: " + event + "\ndata: " + json + "\n\n";
	for (auto *s : sse_)
		s->write(frame);
}

void OverlayServer::onNewConnection()
{
	while (auto *s = server_.nextPendingConnection()) {
		pending_[s] = QByteArray();
		connect(s, &QTcpSocket::readyRead, this, [this, s] {
			if (!pending_.contains(s))
				return;
			QByteArray &buf = pending_[s];
			buf += s->readAll();
			if (buf.size() > 16384) { // petición absurda
				s->disconnectFromHost();
				return;
			}
			int end = buf.indexOf("\r\n\r\n");
			if (end < 0)
				return;
			QByteArray head = buf.left(end);
			pending_.remove(s);
			handleRequest(s, head);
		});
		connect(s, &QTcpSocket::disconnected, this, [this, s] {
			pending_.remove(s);
			sse_.remove(s);
			s->deleteLater();
		});
	}
}

void OverlayServer::reply(QTcpSocket *s, int code, const QByteArray &type, const QByteArray &body)
{
	static const QHash<int, QByteArray> reason = {{200, "OK"}, {400, "Bad Request"}, {403, "Forbidden"}, {404, "Not Found"}, {405, "Method Not Allowed"}};
	QByteArray h = "HTTP/1.1 " + QByteArray::number(code) + " " + reason.value(code, "OK") + "\r\n";
	h += "Content-Type: " + type + "\r\nContent-Length: " + QByteArray::number(body.size()) +
	     "\r\nCache-Control: no-cache\r\nConnection: close\r\nX-Content-Type-Options: nosniff\r\n\r\n";
	s->write(h);
	s->write(body);
	s->disconnectFromHost();
}

void OverlayServer::serveFile(QTcpSocket *s, const QString &rel)
{
	if (rel.contains("..") || rel.contains('\\') || rel.startsWith('/')) {
		reply(s, 403, "text/plain", "forbidden");
		return;
	}
	QFileInfo fi(QDir(dir_).absoluteFilePath(rel));
	// Debe quedar dentro del directorio del overlay, también tras resolver enlaces.
	if (!fi.exists() || !fi.isFile() || !fi.canonicalFilePath().startsWith(QDir(dir_).canonicalPath())) {
		reply(s, 404, "text/plain", "not found");
		return;
	}
	QFile f(fi.absoluteFilePath());
	if (!f.open(QIODevice::ReadOnly)) {
		reply(s, 404, "text/plain", "not found");
		return;
	}
	QString ext = fi.suffix().toLower();
	QByteArray type = ext == "html" ? "text/html; charset=utf-8"
		: ext == "js" ? "application/javascript; charset=utf-8"
		: ext == "css" ? "text/css; charset=utf-8"
		: ext == "png" ? "image/png" : ext == "svg" ? "image/svg+xml" : ext == "woff2" ? "font/woff2"
		: "application/octet-stream";
	reply(s, 200, type, f.readAll());
}

void OverlayServer::handleRequest(QTcpSocket *s, const QByteArray &head)
{
	const QList<QByteArray> lines = head.split('\n');
	const QList<QByteArray> rl = lines.value(0).trimmed().split(' ');
	if (rl.size() < 2) { reply(s, 400, "text/plain", "bad request"); return; }

	// Protección contra DNS rebinding: solo aceptamos Host loopback.
	for (const auto &ln : lines) {
		if (ln.toLower().startsWith("host:")) {
			QByteArray host = ln.mid(5).trimmed().toLower();
			if (!(host.startsWith("127.0.0.1") || host.startsWith("localhost"))) {
				reply(s, 403, "text/plain", "forbidden");
				return;
			}
		}
	}
	if (rl[0] != "GET") { reply(s, 405, "text/plain", "method not allowed"); return; }

	const QString path = QUrl(QString::fromUtf8(rl[1])).path();
	if (path == "/events") {
		s->write("HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nCache-Control: no-cache\r\nConnection: keep-alive\r\n\r\n");
		s->write("retry: 2000\n\n");
		s->write("event: config\ndata: " + lastStyle_ + "\n\n");
		if (!lastNowPlaying_.isEmpty())
			s->write("event: nowplaying\ndata: " + lastNowPlaying_ + "\n\n");
		sse_.insert(s);
	} else if (path == "/api/config") {
		reply(s, 200, "application/json", lastStyle_);
	} else if (path.startsWith("/overlay/")) {
		serveFile(s, path.mid(9));
	} else if (path == "/" || path == "/overlay") {
		serveFile(s, "index.html");
	} else {
		reply(s, 404, "text/plain", "not found");
	}
}

} // namespace mcu
