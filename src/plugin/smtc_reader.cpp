// Lectura de la música en reproducción mediante Windows Media Transport Controls (C++/WinRT).
// Corre en un hilo propio: las llamadas .get() bloquean y no deben hacerse en el hilo de la interfaz de OBS.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include "smtc_reader.hpp"

#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Media.Control.h>
#include <winrt/Windows.Storage.Streams.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>

namespace wmc = winrt::Windows::Media::Control;
namespace wss = winrt::Windows::Storage::Streams;

namespace mcu {
namespace {

std::string lower(std::string s)
{
	for (auto &c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}

std::string friendlyName(const std::string &id)
{
	const std::string l = lower(id);
	auto has = [&](const char *k) { return l.find(k) != std::string::npos; };
	if (has("spotify")) return "Spotify";
	if (has("amazon")) return "Amazon Music";
	if (has("applemusic") || has("itunes")) return "Apple Music";
	if (has("zunemusic") || has("mediaplayer")) return "Reproductor de Windows";
	if (has("msedge")) return "Microsoft Edge";
	if (has("chrome")) return "Google Chrome";
	if (has("opera")) return "Opera";
	if (has("brave")) return "Brave";
	if (has("firefox")) return "Firefox";
	if (has("vlc")) return "VLC";
	return id;
}

std::string base64(const std::vector<uint8_t> &d)
{
	static const char *T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	std::string o;
	o.reserve(((d.size() + 2) / 3) * 4);
	size_t i = 0;
	for (; i + 2 < d.size(); i += 3) {
		uint32_t n = ((uint32_t)d[i] << 16) | ((uint32_t)d[i + 1] << 8) | d[i + 2];
		o += T[(n >> 18) & 63];
		o += T[(n >> 12) & 63];
		o += T[(n >> 6) & 63];
		o += T[n & 63];
	}
	if (i + 1 == d.size()) {
		uint32_t n = (uint32_t)d[i] << 16;
		o += T[(n >> 18) & 63];
		o += T[(n >> 12) & 63];
		o += "==";
	} else if (i + 2 == d.size()) {
		uint32_t n = ((uint32_t)d[i] << 16) | ((uint32_t)d[i + 1] << 8);
		o += T[(n >> 18) & 63];
		o += T[(n >> 12) & 63];
		o += T[(n >> 6) & 63];
		o += '=';
	}
	return o;
}

// La portada se incrusta como data: URI para que la fuente de OBS no necesite pedir archivos.
std::string readCover(const wmc::GlobalSystemMediaTransportControlsSessionMediaProperties &p)
{
	try {
		auto ref = p.Thumbnail();
		if (!ref)
			return {};
		auto stream = ref.OpenReadAsync().get();
		const uint64_t size = stream.Size();
		if (size == 0 || size > 3ull * 1024 * 1024)
			return {};
		wss::DataReader reader(stream);
		reader.LoadAsync(static_cast<uint32_t>(size)).get();
		std::vector<uint8_t> buf(static_cast<size_t>(size));
		reader.ReadBytes(buf);
		std::string mime = winrt::to_string(stream.ContentType());
		if (mime.empty())
			mime = "image/jpeg";
		return "data:" + mime + ";base64," + base64(buf);
	} catch (...) {
		return {};
	}
}

} // namespace

SmtcReader::SmtcReader(StateFn onState, SessionsFn onSessions, ErrorFn onError)
	: onState_(std::move(onState)), onSessions_(std::move(onSessions)), onError_(std::move(onError))
{
}

SmtcReader::~SmtcReader() { stop(); }

void SmtcReader::start()
{
	std::lock_guard<std::mutex> lk(m_);
	if (th_.joinable())
		return;
	stop_ = false;
	th_ = std::thread([this] { run(); });
}

void SmtcReader::stop()
{
	{
		std::lock_guard<std::mutex> lk(m_);
		stop_ = true;
	}
	cv_.notify_all();
	if (th_.joinable())
		th_.join();
}

void SmtcReader::setPreferred(const std::string &sessionId)
{
	std::lock_guard<std::mutex> lk(m_);
	preferred_ = sessionId;
}

void SmtcReader::run()
{
	using namespace std::chrono;
	try {
		winrt::init_apartment(winrt::apartment_type::multi_threaded);
	} catch (...) {
	}

	wmc::GlobalSystemMediaTransportControlsSessionManager mgr{nullptr};
	std::vector<MusicSession> lastList;
	bool listSent = false;
	MusicState last;
	bool haveLast = false;
	auto lastTime = steady_clock::now();
	std::string coverKey, cover, lastErr;
	int coverTries = 0;

	while (true) {
		bool failed = false;
		try {
			if (!mgr)
				mgr = wmc::GlobalSystemMediaTransportControlsSessionManager::RequestAsync().get();

			// 1) Lista de reproductores con sesión abierta.
			auto sessions = mgr.GetSessions();
			std::vector<MusicSession> list;
			for (auto const &s : sessions) {
				std::string id = winrt::to_string(s.SourceAppUserModelId());
				list.push_back({id, friendlyName(id)});
			}
			bool listChanged = !listSent || list.size() != lastList.size() ||
					   !std::equal(list.begin(), list.end(), lastList.begin(),
						       [](const MusicSession &a, const MusicSession &b) { return a.id == b.id; });
			if (listChanged) {
				lastList = list;
				listSent = true;
				if (onSessions_)
					onSessions_(list);
			}

			// 2) Elegir sesión: la preferida; si no, la que esté sonando; si no, la actual de Windows.
			std::string pref;
			{
				std::lock_guard<std::mutex> lk(m_);
				pref = preferred_;
			}
			wmc::GlobalSystemMediaTransportControlsSession chosen{nullptr};
			if (!pref.empty()) {
				for (auto const &s : sessions)
					if (winrt::to_string(s.SourceAppUserModelId()) == pref) {
						chosen = s;
						break;
					}
			}
			if (!chosen) {
				for (auto const &s : sessions)
					if (s.GetPlaybackInfo().PlaybackStatus() ==
					    wmc::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing) {
						chosen = s;
						break;
					}
			}
			if (!chosen)
				chosen = mgr.GetCurrentSession();
			if (!chosen && sessions.Size() > 0)
				chosen = sessions.GetAt(0);

			// 3) Leer el estado de esa sesión.
			MusicState st;
			if (chosen) {
				st.active = true;
				st.sessionId = winrt::to_string(chosen.SourceAppUserModelId());
				st.source = friendlyName(st.sessionId);
				st.playing = chosen.GetPlaybackInfo().PlaybackStatus() ==
					     wmc::GlobalSystemMediaTransportControlsSessionPlaybackStatus::Playing;

				auto props = chosen.TryGetMediaPropertiesAsync().get();
				if (props) {
					st.title = winrt::to_string(props.Title());
					st.artist = winrt::to_string(props.Artist());
					st.album = winrt::to_string(props.AlbumTitle());
				}

				const std::string key = st.sessionId + "\x1f" + st.title + "\x1f" + st.artist + "\x1f" + st.album;
				if (key != coverKey) {
					coverKey = key;
					cover.clear();
					coverTries = 0;
				}
				// Algunos reproductores entregan la portada unos instantes después del título: se reintenta.
				if (props && cover.empty() && coverTries < 6 && !st.title.empty()) {
					cover = readCover(props);
					++coverTries;
				}
				st.coverDataUri = cover;

				auto tl = chosen.GetTimelineProperties();
				const long long start = duration_cast<milliseconds>(tl.StartTime()).count();
				const long long end = duration_cast<milliseconds>(tl.EndTime()).count();
				long long pos = duration_cast<milliseconds>(tl.Position()).count() - start;
				st.durationMs = std::max(0ll, end - start);
				if (st.playing) {
					// Windows entrega la posición en el instante LastUpdatedTime: hay que avanzarla.
					long long el = duration_cast<milliseconds>(winrt::clock::now() - tl.LastUpdatedTime()).count();
					if (el > 0 && el < 6ll * 3600 * 1000)
						pos += el;
				}
				if (st.durationMs <= 0)
					pos = 0;
				st.positionMs = std::clamp(pos, 0ll, st.durationMs);
			}

			// 4) Publicar solo si algo cambió (la capa visual redibuja todo al recibir un evento).
			const auto now = steady_clock::now();
			bool publish = !haveLast;
			if (haveLast) {
				const long long expected =
					last.positionMs + (last.playing ? duration_cast<milliseconds>(now - lastTime).count() : 0);
				publish = st.active != last.active || st.playing != last.playing || st.title != last.title ||
					  st.artist != last.artist || st.album != last.album || st.source != last.source ||
					  st.durationMs != last.durationMs || st.coverDataUri != last.coverDataUri ||
					  std::llabs(st.positionMs - expected) > 2500;
			}
			if (publish) {
				last = st;
				lastTime = now;
				haveLast = true;
				if (onState_)
					onState_(st);
			}
			lastErr.clear();
		} catch (winrt::hresult_error const &e) {
			failed = true;
			mgr = nullptr;
			std::string msg = winrt::to_string(e.message());
			if (msg != lastErr) {
				lastErr = msg;
				if (onError_)
					onError_(msg);
			}
		} catch (std::exception const &e) {
			failed = true;
			mgr = nullptr;
			if (lastErr != e.what()) {
				lastErr = e.what();
				if (onError_)
					onError_(lastErr);
			}
		}

		std::unique_lock<std::mutex> lk(m_);
		if (cv_.wait_for(lk, milliseconds(failed ? 3000 : 1000), [this] { return stop_; }))
			break;
	}
}

} // namespace mcu
