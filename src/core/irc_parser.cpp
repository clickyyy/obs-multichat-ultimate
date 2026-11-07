#include "irc_parser.hpp"
#include <cstdlib>

namespace mcu {

static std::string unescapeTag(const std::string &v)
{
	std::string o;
	for (size_t i = 0; i < v.size(); ++i) {
		if (v[i] == '\\' && i + 1 < v.size()) {
			char n = v[++i];
			switch (n) {
			case ':': o += ';'; break;
			case 's': o += ' '; break;
			case 'r': o += '\r'; break;
			case 'n': o += '\n'; break;
			case '\\': o += '\\'; break;
			default: o += n;
			}
		} else
			o += v[i];
	}
	return o;
}

static std::vector<std::string> split(const std::string &s, char d)
{
	std::vector<std::string> r;
	std::string cur;
	for (char c : s) {
		if (c == d) {
			r.push_back(cur);
			cur.clear();
		} else
			cur += c;
	}
	r.push_back(cur);
	return r;
}

bool parseIrcLine(const std::string &line, IrcLine &o)
{
	o = IrcLine{};
	size_t i = 0, n = line.size();
	if (n == 0)
		return false;
	if (line[i] == '@') {
		size_t sp = line.find(' ', i);
		if (sp == std::string::npos)
			return false;
		for (auto &kvp : split(line.substr(1, sp - 1), ';')) {
			size_t eq = kvp.find('=');
			if (eq == std::string::npos)
				o.tags[kvp] = "";
			else
				o.tags[kvp.substr(0, eq)] = unescapeTag(kvp.substr(eq + 1));
		}
		i = sp + 1;
	}
	while (i < n && line[i] == ' ')
		++i;
	if (i < n && line[i] == ':') {
		size_t sp = line.find(' ', i);
		if (sp == std::string::npos)
			return false;
		o.prefix = line.substr(i + 1, sp - i - 1);
		i = sp + 1;
	}
	size_t sp = line.find(' ', i);
	if (sp == std::string::npos) {
		o.command = line.substr(i);
		return !o.command.empty();
	}
	o.command = line.substr(i, sp - i);
	i = sp + 1;
	std::string params;
	if (i < n && line[i] == ':') {
		o.trailing = line.substr(i + 1);
	} else {
		size_t colon = line.find(" :", i);
		if (colon != std::string::npos) {
			params = line.substr(i, colon - i);
			o.trailing = line.substr(colon + 2);
		} else
			params = line.substr(i);
	}
	auto ps = split(params, ' ');
	if (!ps.empty() && !ps[0].empty() && ps[0][0] == '#')
		o.channel = ps[0];
	return !o.command.empty();
}

bool twitchToMessage(const IrcLine &l, ChatMessage &m)
{
	if (l.command != "PRIVMSG")
		return false;
	m = ChatMessage{};
	m.platform = "twitch";
	auto get = [&](const char *k) {
		auto it = l.tags.find(k);
		return it == l.tags.end() ? std::string() : it->second;
	};
	m.id = get("id");
	m.userId = get("user-id");
	m.user = get("display-name");
	if (m.user.empty())
		m.user = l.prefix.substr(0, l.prefix.find('!'));
	m.color = get("color");
	m.text = l.trailing;
	const std::string pre = "\x01" "ACTION ";
	if (m.text.rfind(pre, 0) == 0) {
		m.action = true;
		m.text = m.text.substr(pre.size());
		if (!m.text.empty() && m.text.back() == '\x01')
			m.text.pop_back();
	}
	std::string ts = get("tmi-sent-ts");
	m.timestampMs = ts.empty() ? 0 : std::atoll(ts.c_str());

	for (auto &b : split(get("badges"), ',')) {
		std::string name = b.substr(0, b.find('/'));
		if (name == "broadcaster") m.badges.push_back("owner");
		else if (name == "moderator") m.badges.push_back("moderator");
		else if (name == "vip") m.badges.push_back("vip");
		else if (name == "subscriber" || name == "founder") m.badges.push_back("subscriber");
	}
	if (get("mod") == "1" && !m.hasBadge("moderator"))
		m.badges.push_back("moderator");
	if (get("msg-id") == "highlighted-message" || !get("custom-reward-id").empty())
		m.highlighted = true;

	std::string em = get("emotes"); // "25:0-4,12-16/1902:6-10"
	if (!em.empty()) {
		for (auto &grp : split(em, '/')) {
			size_t c = grp.find(':');
			if (c == std::string::npos) continue;
			std::string id = grp.substr(0, c);
			for (auto &rg : split(grp.substr(c + 1), ',')) {
				size_t d = rg.find('-');
				if (d == std::string::npos) continue;
				Emote e;
				e.start = std::atoi(rg.substr(0, d).c_str());
				e.end = std::atoi(rg.substr(d + 1).c_str());
				e.url = "https://static-cdn.jtvnw.net/emoticons/v2/" + id + "/default/dark/2.0";
				m.emotes.push_back(e);
			}
		}
	}
	return true;
}

} // namespace mcu
