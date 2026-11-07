// Fuentes nativas de OBS. Cada fuente es un envoltorio sobre una fuente interna de navegador (obs-browser,
// incluida con OBS) que apunta a la capa visual local. El usuario solo ve "OBS Multichat ..." en Fuentes > +.
// Las conexiones son únicas (Hub); las fuentes solo consumen, así que no se duplican.
#include "plugin_api.hpp"
#include <obs-module.h>
#include <cstring>
#include <string>

namespace {

struct Kind {
	const char *id, *name, *module, *mode;
	int w, h;
};
const Kind KINDS[] = {
	{"mcu_chat_vertical", "OBS Multichat Vertical", "chat", "vertical", 500, 700},
	{"mcu_chat_horizontal", "OBS Multichat Horizontal", "chat", "horizontal", 1920, 120},
	{"mcu_chat_compact", "OBS Multichat Compacto", "chat", "compact", 500, 400},
	{"mcu_chat_bubbles", "OBS Multichat Burbujas", "chat", "bubbles", 500, 700},
	{"mcu_clock", "OBS Clock", "clock", "", 420, 160},
	{"mcu_nowplaying", "OBS Now Playing", "nowplaying", "", 480, 140},
};

const Kind *kindOf(const char *id)
{
	for (auto &k : KINDS)
		if (!strcmp(k.id, id))
			return &k;
	return nullptr;
}

struct Wrapper {
	obs_source_t *self = nullptr;
	obs_source_t *browser = nullptr;
	const Kind *kind = nullptr;
	int w = 0, h = 0;
	std::string url;
};

std::string pct(const std::string &s)
{
	static const char *hex = "0123456789ABCDEF";
	std::string o;
	for (unsigned char c : s) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
			o += (char)c;
		else {
			o += '%';
			o += hex[c >> 4];
			o += hex[c & 15];
		}
	}
	return o;
}

// Ajustes propios de cada fuente -> JSON que lee overlay.js (tienen prioridad sobre el estilo global).
std::string buildUrl(const Kind &k, obs_data_t *s)
{
	obs_data_t *cfg = obs_data_create();
	if (!strcmp(k.module, "chat")) {
		obs_data_set_int(cfg, "maxMessages", obs_data_get_int(s, "maxMessages"));
		obs_data_set_bool(cfg, "showAvatar", obs_data_get_bool(s, "showAvatar"));
		obs_data_set_int(cfg, "speed", obs_data_get_int(s, "speed"));
		obs_data_set_int(cfg, "spacing", obs_data_get_int(s, "spacing"));
		obs_data_set_string(cfg, "direction", obs_data_get_string(s, "direction"));
	} else if (!strcmp(k.module, "clock")) {
		obs_data_t *c = obs_data_create();
		obs_data_set_bool(c, "hour12", obs_data_get_bool(s, "hour12"));
		obs_data_set_bool(c, "seconds", obs_data_get_bool(s, "seconds"));
		obs_data_set_bool(c, "date", obs_data_get_bool(s, "date"));
		obs_data_set_string(c, "dateFormat", obs_data_get_string(s, "dateFormat"));
		obs_data_set_string(c, "design", obs_data_get_string(s, "design"));
		obs_data_set_string(c, "timeZone", obs_data_get_string(s, "timeZone"));
		obs_data_set_obj(cfg, "clock", c);
		obs_data_release(c);
	} else {
		obs_data_t *n = obs_data_create();
		obs_data_set_string(n, "layout", obs_data_get_string(s, "layout"));
		obs_data_set_bool(n, "showCover", obs_data_get_bool(s, "showCover"));
		obs_data_set_bool(n, "hideWhenIdle", obs_data_get_bool(s, "hideWhenIdle"));
		obs_data_set_obj(cfg, "np", n);
		obs_data_release(n);
	}
	std::string json = obs_data_get_json(cfg);
	obs_data_release(cfg);
	return "http://127.0.0.1:" + std::to_string(mcuOverlayPort()) + "/overlay/index.html?module=" + k.module +
	       "&mode=" + k.mode + "&cfg=" + pct(json);
}

void applyToBrowser(Wrapper *w, obs_data_t *s)
{
	w->w = (int)obs_data_get_int(s, "width");
	w->h = (int)obs_data_get_int(s, "height");
	std::string url = buildUrl(*w->kind, s);
	obs_data_t *bs = obs_data_create();
	obs_data_set_string(bs, "url", url.c_str());
	obs_data_set_int(bs, "width", w->w);
	obs_data_set_int(bs, "height", w->h);
	obs_data_set_int(bs, "fps", 60);
	obs_data_set_bool(bs, "shutdown", false);
	obs_data_set_bool(bs, "restart_when_active", false);
	obs_data_set_string(bs, "css", "body{background-color:rgba(0,0,0,0);margin:0;overflow:hidden}");
	if (!w->browser) {
		w->browser = obs_source_create_private("browser_source", "mcu-internal-browser", bs);
		if (!w->browser)
			blog(LOG_ERROR, "[OBS Multichat Ultimate] No se pudo crear la fuente de navegador interna "
					"(¿falta obs-browser?)");
	} else {
		obs_source_update(w->browser, bs);
	}
	// Diagnóstico: confirma en el log que el navegador interno se creó y con qué URL.
	blog(LOG_INFO, "[OBS Multichat Ultimate] fuente %s -> %s (browser=%p, %dx%d)", w->kind->id, url.c_str(),
	     (void *)w->browser, w->w, w->h);
	w->url = url;
	obs_data_release(bs);
}

const char *getName(void *type_data)
{
	return type_data ? ((const Kind *)type_data)->name : "OBS Multichat Ultimate";
}

void *create(obs_data_t *settings, obs_source_t *source)
{
	auto *w = new Wrapper;
	w->self = source;
	w->kind = kindOf(obs_source_get_id(source));
	if (w->kind)
		applyToBrowser(w, settings);
	return w;
}

void destroy(void *data)
{
	auto *w = (Wrapper *)data;
	if (w->browser)
		obs_source_release(w->browser);
	delete w;
}

void update(void *data, obs_data_t *settings)
{
	auto *w = (Wrapper *)data;
	if (w->kind)
		applyToBrowser(w, settings);
}

void videoRender(void *data, gs_effect_t *)
{
	auto *w = (Wrapper *)data;
	if (w->browser)
		obs_source_video_render(w->browser);
}

uint32_t getWidth(void *data) { return (uint32_t)((Wrapper *)data)->w; }
uint32_t getHeight(void *data) { return (uint32_t)((Wrapper *)data)->h; }

void show(void *d) { auto *w = (Wrapper *)d; if (w->browser) obs_source_inc_showing(w->browser); }
void hide(void *d) { auto *w = (Wrapper *)d; if (w->browser) obs_source_dec_showing(w->browser); }
void activate(void *d) { auto *w = (Wrapper *)d; if (w->browser) obs_source_inc_active(w->browser); }
void deactivate(void *d) { auto *w = (Wrapper *)d; if (w->browser) obs_source_dec_active(w->browser); }

void defaults(void *type_data, obs_data_t *s)
{
	const Kind *k = (const Kind *)type_data;
	obs_data_set_default_int(s, "width", k ? k->w : 500);
	obs_data_set_default_int(s, "height", k ? k->h : 700);
	obs_data_set_default_int(s, "maxMessages", 8);
	obs_data_set_default_bool(s, "showAvatar", false);
	obs_data_set_default_int(s, "speed", 70);
	obs_data_set_default_int(s, "spacing", 40);
	obs_data_set_default_string(s, "direction", "ltr");
	obs_data_set_default_bool(s, "hour12", false);
	obs_data_set_default_bool(s, "seconds", true);
	obs_data_set_default_bool(s, "date", true);
	obs_data_set_default_string(s, "dateFormat", "long");
	obs_data_set_default_string(s, "design", "digital");
	obs_data_set_default_string(s, "timeZone", "");
	obs_data_set_default_string(s, "layout", "rect");
	obs_data_set_default_bool(s, "showCover", true);
	obs_data_set_default_bool(s, "hideWhenIdle", true);
}

obs_properties_t *properties(void *data)
{
	auto *w = (Wrapper *)data;
	obs_properties_t *p = obs_properties_create();
	obs_properties_add_int(p, "width", "Ancho", 50, 7680, 1);
	obs_properties_add_int(p, "height", "Alto", 20, 4320, 1);
	if (!w || !w->kind)
		return p;
	std::string m = w->kind->module;
	if (m == "chat") {
		obs_properties_add_int(p, "maxMessages", "Mensajes visibles", 1, 50, 1);
		obs_properties_add_bool(p, "showAvatar", "Mostrar avatar");
		if (!strcmp(w->kind->mode, "horizontal")) {
			obs_properties_add_int(p, "speed", "Velocidad (px/s)", 10, 600, 5);
			obs_properties_add_int(p, "spacing", "Separación entre mensajes (px)", 0, 400, 5);
			obs_property_t *d = obs_properties_add_list(p, "direction", "Dirección", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
			obs_property_list_add_string(d, "De izquierda a derecha", "ltr");
			obs_property_list_add_string(d, "De derecha a izquierda", "rtl");
		}
	} else if (m == "clock") {
		obs_properties_add_bool(p, "hour12", "Formato de 12 horas");
		obs_properties_add_bool(p, "seconds", "Mostrar segundos");
		obs_properties_add_bool(p, "date", "Mostrar fecha");
		obs_property_t *f = obs_properties_add_list(p, "dateFormat", "Formato de fecha", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(f, "Largo (lunes, 3 de marzo de 2025)", "long");
		obs_property_list_add_string(f, "Corto (03 mar)", "short");
		obs_property_list_add_string(f, "Numérico (03/03/2025)", "numeric");
		obs_property_t *d = obs_properties_add_list(p, "design", "Diseño", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(d, "Digital", "digital");
		obs_property_list_add_string(d, "Minimalista (sin caja)", "minimal");
		obs_properties_add_text(p, "timeZone", "Zona horaria IANA (vacío = la del sistema, ej. America/Mexico_City)", OBS_TEXT_DEFAULT);
	} else {
		obs_property_t *l = obs_properties_add_list(p, "layout", "Diseño", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
		obs_property_list_add_string(l, "Rectangular", "rect");
		obs_property_list_add_string(l, "Cuadrado", "square");
		obs_property_list_add_string(l, "Minimalista", "minimal");
		obs_property_list_add_string(l, "Glassmorphism", "glass");
		obs_property_list_add_string(l, "Inspirado en Apple Music", "apple");
		obs_properties_add_bool(p, "showCover", "Mostrar portada");
		obs_properties_add_bool(p, "hideWhenIdle", "Ocultar cuando no hay música");
	}
	return p;
}

} // namespace

void mcuRegisterSources()
{
	for (auto &k : KINDS) {
		obs_source_info info = {};
		info.id = k.id;
		info.type_data = (void *)&k;
		info.type = OBS_SOURCE_TYPE_INPUT;
		// CUSTOM_DRAW: el envoltorio delega el dibujo en el navegador interno (igual que browser_source).
		info.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_SRGB;
		info.get_name = getName;
		info.create = create;
		info.destroy = destroy;
		info.update = update;
		info.get_defaults2 = defaults;
		info.get_properties = properties;
		info.video_render = videoRender;
		info.get_width = getWidth;
		info.get_height = getHeight;
		info.show = show;
		info.hide = hide;
		info.activate = activate;
		info.deactivate = deactivate;
		info.icon_type = OBS_ICON_TYPE_BROWSER;
		obs_register_source(&info);
	}
}
