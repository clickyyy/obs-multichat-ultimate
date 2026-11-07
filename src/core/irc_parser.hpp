#pragma once
#include "message.hpp"
#include <map>
#include <string>

namespace mcu {

struct IrcLine {
	std::map<std::string, std::string> tags;
	std::string prefix;  // nick!user@host
	std::string command; // PRIVMSG, PING, ...
	std::string channel; // primer parámetro si empieza por '#'
	std::string trailing;
};

// Parsea una línea IRC con tags de Twitch (sin \r\n). false si no es válida.
bool parseIrcLine(const std::string &line, IrcLine &out);

// Convierte un PRIVMSG de Twitch en ChatMessage. false si no es PRIVMSG.
bool twitchToMessage(const IrcLine &l, ChatMessage &out);

} // namespace mcu
