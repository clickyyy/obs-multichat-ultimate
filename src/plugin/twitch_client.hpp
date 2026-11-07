#pragma once
// Lectura del chat de Twitch por IRC sobre TLS (irc.chat.twitch.tv:6697), modo anónimo de solo lectura.
// No requiere OAuth ni guarda credenciales. Reconexión automática con espera creciente.
#include <QObject>
#include <QSslSocket>
#include <QTimer>

namespace mcu {

class TwitchClient : public QObject {
	Q_OBJECT
public:
	explicit TwitchClient(QObject *parent = nullptr);
	void connectToChannel(const QString &channel);
	void disconnectFromChannel();

private:
	void openSocket();
	void onReadyRead();
	void handleLine(const QByteArray &line);
	void scheduleRetry(const QString &reason);

	QSslSocket sock_;
	QByteArray buf_;
	QString channel_;
	bool wanted_ = false;
	int backoffMs_ = 1000;
	QTimer retry_, watchdog_;
	qint64 lastRx_ = 0;
};

} // namespace mcu
