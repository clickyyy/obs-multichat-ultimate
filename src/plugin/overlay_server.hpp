#pragma once
// Servidor HTTP local (solo 127.0.0.1) que sirve la capa visual y difunde eventos por SSE.
#include <QByteArray>
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTcpServer>
#include <QTimer>

class QTcpSocket;

namespace mcu {

class OverlayServer : public QObject {
	Q_OBJECT
public:
	explicit OverlayServer(QObject *parent = nullptr);
	bool start(const QString &dataDir, quint16 preferredPort = 17420);
	quint16 port() const { return server_.serverPort(); }
	int clientCount() const { return sse_.size(); }

public slots:
	void broadcast(const QByteArray &event, const QByteArray &json);
	void onMessage(const QByteArray &json) { broadcast("message", json); }
	void onStyle(const QByteArray &json);
	void onNowPlaying(const QByteArray &json) { lastNowPlaying_ = json; broadcast("nowplaying", json); }

private:
	void onNewConnection();
	void handleRequest(QTcpSocket *s, const QByteArray &head);
	void reply(QTcpSocket *s, int code, const QByteArray &type, const QByteArray &body);
	void serveFile(QTcpSocket *s, const QString &rel);

	QTcpServer server_;
	QString dir_;
	QHash<QTcpSocket *, QByteArray> pending_;
	QSet<QTcpSocket *> sse_;
	QByteArray lastStyle_ = "{}", lastNowPlaying_;
	QTimer keepAlive_;
};

} // namespace mcu
