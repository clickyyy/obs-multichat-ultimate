#pragma once
#include "message.hpp"
#include <deque>
#include <set>
#include <string>
#include <vector>

namespace mcu {

struct FilterConfig {
	bool hideBots = true;
	bool hideCommands = true;
	bool hideLinks = false;
	bool onlyHighlighted = false;
	bool showModerators = true;
	bool showSubscribers = true;
	std::vector<std::string> botNames = {"nightbot", "streamelements", "streamlabs", "moobot", "fossabot", "wizebot"};
	std::vector<std::string> blockedUsers;
	std::vector<std::string> blockedWords;
	std::set<std::string> enabledPlatforms = {"twitch", "youtube", "kick", "tiktok", "starvios"};
};

std::string toLower(std::string s);
bool containsLink(const std::string &text);

// true = el mensaje se muestra.
bool passes(const FilterConfig &cfg, const ChatMessage &m);

// Evita duplicados por (plataforma, id) con memoria acotada.
class Deduper {
public:
	explicit Deduper(size_t capacity = 1000) : cap_(capacity) {}
	// true si es nuevo (y lo registra); false si ya se vio.
	bool markNew(const std::string &platform, const std::string &id);

private:
	size_t cap_;
	std::set<std::string> seen_;
	std::deque<std::string> order_;
};

} // namespace mcu
