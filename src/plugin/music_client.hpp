#pragma once
// Conecta el lector de música de Windows (SMTC) con el hub y el panel.
// El hilo de lectura entrega los datos al hilo de la interfaz de Qt con conexiones en cola.
#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>

namespace mcu {

class SmtcReader;

class MusicClient : public QObject {
	Q_OBJECT
public:
	explicit MusicClient(QObject *parent = nullptr);
	~MusicClient() override;

	void setEnabled(bool on);
	bool enabled() const { return enabled_; }
	void setPreferred(const QString &sessionId); // "" = automático

signals:
	void sessionsChanged(const QStringList &ids, const QStringList &names);
	void trackChanged(const QString &label); // "Título — Artista", o vacío si no suena nada

private:
	std::unique_ptr<SmtcReader> reader_;
	bool enabled_ = false;
};

} // namespace mcu
