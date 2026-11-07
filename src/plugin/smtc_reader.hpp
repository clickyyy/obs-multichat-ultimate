#pragma once
// Lector de la música que Windows está reproduciendo (Global System Media Transport Controls, SMTC).
// Esta cabecera no incluye Qt ni WinRT a propósito: la parte de C++/WinRT vive solo en smtc_reader.cpp
// para evitar choques de macros con Qt. Los callbacks se llaman desde el hilo de lectura.
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mcu {

struct MusicSession {
	std::string id;   // AppUserModelId, p. ej. "Spotify.exe" o "Chrome"
	std::string name; // nombre legible
};

struct MusicState {
	bool active = false;  // hay un reproductor con sesión
	bool playing = false;
	std::string title, artist, album, source, sessionId;
	std::string coverDataUri; // "data:image/jpeg;base64,..." (vacío si no hay portada)
	long long durationMs = 0, positionMs = 0;
};

class SmtcReader {
public:
	using StateFn = std::function<void(const MusicState &)>;
	using SessionsFn = std::function<void(const std::vector<MusicSession> &)>;
	using ErrorFn = std::function<void(const std::string &)>;

	SmtcReader(StateFn onState, SessionsFn onSessions, ErrorFn onError);
	~SmtcReader();

	void start();
	void stop();
	void setPreferred(const std::string &sessionId); // "" = automático

private:
	void run();

	StateFn onState_;
	SessionsFn onSessions_;
	ErrorFn onError_;
	std::thread th_;
	std::mutex m_;
	std::condition_variable cv_;
	bool stop_ = false;
	std::string preferred_;
};

} // namespace mcu
