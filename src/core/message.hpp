#pragma once
// Modelo de mensaje normalizado + serialización JSON. Sin dependencias (solo STL).
#include <cstdint>
#include <string>
#include <vector>

namespace mcu {

struct Emote {
	int start = 0; // posición (code points) en el texto, como la entrega Twitch
	int end = 0;   // inclusivo
	std::string url;
};

struct ChatMessage {
	std::string id;       // id único del mensaje en su plataforma
	std::string platform; // twitch | youtube | kick | tiktok | starvios
	std::string userId;
	std::string user;     // nombre visible
	std::string text;
	std::string color;    // color del nombre (#rrggbb) o vacío
	std::string avatar;   // URL o vacío
	std::vector<std::string> badges; // owner, moderator, vip, subscriber, bot...
	std::vector<Emote> emotes;
	bool action = false;      // /me
	bool highlighted = false; // mensaje destacado (canjeo de puntos, etc.)
	int64_t timestampMs = 0;

	bool hasBadge(const std::string &b) const
	{
		for (auto &x : badges)
			if (x == b)
				return true;
		return false;
	}
};

std::string jsonEscape(const std::string &s);
std::string toJson(const ChatMessage &m);

} // namespace mcu
