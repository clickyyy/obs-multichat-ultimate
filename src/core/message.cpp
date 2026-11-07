#include "message.hpp"
#include <cstdio>

namespace mcu {

std::string jsonEscape(const std::string &s)
{
	std::string o;
	o.reserve(s.size() + 8);
	for (unsigned char c : s) {
		switch (c) {
		case '"': o += "\\\""; break;
		case '\\': o += "\\\\"; break;
		case '\n': o += "\\n"; break;
		case '\r': o += "\\r"; break;
		case '\t': o += "\\t"; break;
		default:
			if (c < 0x20) {
				char b[8];
				std::snprintf(b, sizeof b, "\\u%04x", c);
				o += b;
			} else
				o += (char)c;
		}
	}
	return o;
}

static void kv(std::string &o, const char *k, const std::string &v)
{
	o += "\"";
	o += k;
	o += "\":\"";
	o += jsonEscape(v);
	o += "\",";
}

std::string toJson(const ChatMessage &m)
{
	std::string o = "{";
	kv(o, "id", m.id);
	kv(o, "platform", m.platform);
	kv(o, "userId", m.userId);
	kv(o, "user", m.user);
	kv(o, "text", m.text);
	kv(o, "color", m.color);
	kv(o, "avatar", m.avatar);
	o += "\"badges\":[";
	for (size_t i = 0; i < m.badges.size(); ++i) {
		if (i) o += ",";
		o += "\"" + jsonEscape(m.badges[i]) + "\"";
	}
	o += "],\"emotes\":[";
	for (size_t i = 0; i < m.emotes.size(); ++i) {
		if (i) o += ",";
		o += "{\"start\":" + std::to_string(m.emotes[i].start) + ",\"end\":" +
		     std::to_string(m.emotes[i].end) + ",\"url\":\"" + jsonEscape(m.emotes[i].url) + "\"}";
	}
	o += "],";
	o += std::string("\"action\":") + (m.action ? "true" : "false") + ",";
	o += std::string("\"highlighted\":") + (m.highlighted ? "true" : "false") + ",";
	o += "\"ts\":" + std::to_string(m.timestampMs) + "}";
	return o;
}

} // namespace mcu
