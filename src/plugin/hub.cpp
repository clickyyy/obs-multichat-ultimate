#include "hub.hpp"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStandardPaths>

namespace mcu {

MessageHub &MessageHub::instance()
{
	static MessageHub hub;
	return hub;
}

QString MessageHub::configPath() const
{
	QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/obs-multichat-ultimate";
	QDir().mkpath(dir);
	return dir + "/config.json";
}

void MessageHub::load()
{
	QFile f(configPath());
	if (f.open(QIODevice::ReadOnly)) {
		QJsonParseError err;
		auto doc = QJsonDocument::fromJson(f.readAll(), &err);
		if (err.error == QJsonParseError::NoError && doc.isObject())
			config_ = doc.object();
	}
	applyFilters();
}

void MessageHub::save() const
{
	// Escritura atómica: si OBS se cierra a mitad, no se corrompe el archivo.
	QSaveFile f(configPath());
	if (f.open(QIODevice::WriteOnly)) {
		f.write(QJsonDocument(config_).toJson(QJsonDocument::Indented));
		f.commit();
	}
}

static std::vector<std::string> toVec(const QJsonArray &a)
{
	std::vector<std::string> v;
	for (auto x : a)
		v.push_back(x.toString().toStdString());
	return v;
}

void MessageHub::applyFilters()
{
	QJsonObject o = config_.value("filters").toObject();
	FilterConfig c;
	c.hideBots = o.value("hideBots").toBool(c.hideBots);
	c.hideCommands = o.value("hideCommands").toBool(c.hideCommands);
	c.hideLinks = o.value("hideLinks").toBool(c.hideLinks);
	c.onlyHighlighted = o.value("onlyHighlighted").toBool(c.onlyHighlighted);
	c.showModerators = o.value("showModerators").toBool(c.showModerators);
	c.showSubscribers = o.value("showSubscribers").toBool(c.showSubscribers);
	if (o.contains("blockedUsers")) c.blockedUsers = toVec(o.value("blockedUsers").toArray());
	if (o.contains("blockedWords")) c.blockedWords = toVec(o.value("blockedWords").toArray());
	if (o.contains("botNames")) c.botNames = toVec(o.value("botNames").toArray());
	if (o.contains("platforms")) {
		c.enabledPlatforms.clear();
		for (auto x : o.value("platforms").toArray())
			c.enabledPlatforms.insert(x.toString().toStdString());
	}
	filters_ = c;
}

void MessageHub::submit(const ChatMessage &m)
{
	if (!dedupe_.markNew(m.platform, m.id))
		return;
	if (!passes(filters_, m)) {
		++hidden_;
		emit statsChanged();
		return;
	}
	++received_;
	last_ = QString::fromStdString(m.user + ": " + m.text);
	emit messageJson(QByteArray::fromStdString(toJson(m)));
	emit statsChanged();
}

void MessageHub::setStyleValue(const QString &key, const QJsonValue &v)
{
	QJsonObject s = style();
	s.insert(key, v);
	config_.insert("style", s);
	save();
	emit styleJson(QJsonDocument(s).toJson(QJsonDocument::Compact));
}

void MessageHub::resetStyle()
{
	config_.insert("style", QJsonObject());
	save();
	emit styleJson("{}");
}

static void setFilterJson(QJsonObject &config, const QString &key, const QJsonValue &v)
{
	QJsonObject f = config.value("filters").toObject();
	f.insert(key, v);
	config.insert("filters", f);
}

void MessageHub::setFilterBool(const QString &key, bool v)
{
	setFilterJson(config_, key, v);
	applyFilters();
	save();
}

void MessageHub::setFilterList(const QString &key, const QStringList &v)
{
	setFilterJson(config_, key, QJsonArray::fromStringList(v));
	applyFilters();
	save();
}

QStringList MessageHub::filterList(const QString &key) const
{
	QStringList out;
	QJsonObject f = config_.value("filters").toObject();
	if (f.contains(key)) {
		for (auto x : f.value(key).toArray())
			out << x.toString();
	} else if (key == "botNames") {
		for (auto &b : FilterConfig().botNames)
			out << QString::fromStdString(b);
	}
	return out;
}

void MessageHub::setPlatformEnabled(const QString &platform, bool on)
{
	if (on) filters_.enabledPlatforms.insert(platform.toStdString());
	else filters_.enabledPlatforms.erase(platform.toStdString());
	QJsonArray arr;
	for (auto &p : filters_.enabledPlatforms)
		arr.append(QString::fromStdString(p));
	setFilterJson(config_, "platforms", arr);
	save();
}

void MessageHub::setTwitch(const QString &channel, bool autoconnect)
{
	QJsonObject t;
	t.insert("channel", channel);
	t.insert("autoconnect", autoconnect);
	config_.insert("twitch", t);
	save();
}

void MessageHub::setMusic(bool enabled, const QString &source)
{
	QJsonObject m;
	m.insert("enabled", enabled);
	m.insert("source", source);
	config_.insert("music", m);
	save();
}

void MessageHub::setStatus(const QString &platform, ConnState s, const QString &detail)
{
	states_[platform] = s;
	emit statusChanged(platform, (int)s, detail);
}

ConnState MessageHub::status(const QString &platform) const
{
	return states_.value(platform, ConnState::Disconnected);
}

} // namespace mcu
