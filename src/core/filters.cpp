#include "filters.hpp"
#include <algorithm>
#include <cctype>

namespace mcu {

std::string toLower(std::string s)
{
	for (auto &c : s)
		c = (char)std::tolower((unsigned char)c);
	return s;
}

bool containsLink(const std::string &text)
{
	std::string t = toLower(text);
	if (t.find("http://") != std::string::npos || t.find("https://") != std::string::npos ||
	    t.find("www.") != std::string::npos)
		return true;
	static const char *tlds[] = {".com", ".net", ".org", ".tv", ".gg", ".io", ".ly", ".me", ".co", ".mx", ".es"};
	for (auto *tld : tlds) {
		size_t p = t.find(tld);
		while (p != std::string::npos) {
			size_t e = p + std::string(tld).size();
			bool endOk = e >= t.size() || !std::isalnum((unsigned char)t[e]);
			bool startOk = p > 0 && std::isalnum((unsigned char)t[p - 1]);
			if (endOk && startOk)
				return true;
			p = t.find(tld, p + 1);
		}
	}
	return false;
}

static bool inList(const std::vector<std::string> &v, const std::string &lower)
{
	return std::any_of(v.begin(), v.end(), [&](const std::string &x) { return toLower(x) == lower; });
}

bool passes(const FilterConfig &c, const ChatMessage &m)
{
	if (!c.enabledPlatforms.count(m.platform))
		return false;
	std::string user = toLower(m.user);
	if (inList(c.blockedUsers, user))
		return false;
	if (c.hideBots && (inList(c.botNames, user) || m.hasBadge("bot")))
		return false;
	if (c.hideCommands && !m.text.empty() && m.text[0] == '!')
		return false;
	if (c.hideLinks && containsLink(m.text))
		return false;
	if (c.onlyHighlighted && !m.highlighted)
		return false;
	if (!c.showModerators && m.hasBadge("moderator"))
		return false;
	if (!c.showSubscribers && m.hasBadge("subscriber"))
		return false;
	std::string text = toLower(m.text);
	for (auto &w : c.blockedWords)
		if (!w.empty() && text.find(toLower(w)) != std::string::npos)
			return false;
	return true;
}

bool Deduper::markNew(const std::string &platform, const std::string &id)
{
	if (id.empty())
		return true;
	std::string k = platform + ":" + id;
	if (!seen_.insert(k).second)
		return false;
	order_.push_back(k);
	while (order_.size() > cap_) {
		seen_.erase(order_.front());
		order_.pop_front();
	}
	return true;
}

} // namespace mcu
