#pragma once
// Cliente del servidor WebSocket local de Streamer.bot (ws://127.0.0.1:8080).
// Recibe el chat de Kick y YouTube y lo entrega al MessageHub con el mismo formato que Twitch.
// El servidor es local y sin cifrado, asi que el protocolo WebSocket se implementa a mano
// sobre QTcpSocket (no hace falta el modulo Qt WebSockets).
#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QTcpSocket>
#include <QTimer>

namespace mcu {

class StreamerbotClient : public QObject {
	Q_OBJECT
public:
	explicit StreamerbotClient(QObject *parent = nullptr);

	void start(const QString &host = QStringLiteral("127.0.0.1"), quint16 port = 8080);
	void stop();

private:
	void openSocket();
	void scheduleRetry(const QString &reason);
	void fail(const QString &reason);
	void setAll(int state, const QString &detail = QString());

	void onConnected();
	void onReadyRead();
	void onHandshakeDone();
	void processFrames();

	void sendText(const QByteArray &payload);
	void sendFrame(quint8 opcode, const QByteArray &payload);

	void handleText(const QByteArray &text);
	void handleEvent(const QJsonObject &root);
	void handleResponse(const QJsonObject &root);

	QTcpSocket sock_;
	QTimer retry_;
	QByteArray buf_;  // bytes recibidos sin procesar
	QByteArray frag_; // mensaje fragmentado en construccion
	QString host_ = QStringLiteral("127.0.0.1");
	quint16 port_ = 8080;
	bool wanted_ = false;
	bool handshakeDone_ = false;
	int backoffMs_ = 1000;
	quint64 counter_ = 0;
};

} // namespace mcu
