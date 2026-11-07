/* OBS Multichat Ultimate – capa visual. Recibe datos del plugin por SSE (/events). */
(function () {
  "use strict";
  const qs = new URLSearchParams(location.search);
  const module_ = qs.get("module") || "chat";
  const mode = qs.get("mode") || "vertical";
  let local = {};
  try { local = JSON.parse(qs.get("cfg") || "{}"); } catch (e) {}
  const root = document.getElementById("root");

  const DEFAULTS = {
    theme: "twitch", fontSize: 22, gap: 8, maxMessages: 8, lifetime: 0, showAvatar: false, showBadges: true,
    showPlatform: true, bgMode: "theme", bgColor: "#18181b", bgOpacity: 0.9, radius: null, borderWidth: null,
    borderColor: null, shadow: null, nameColor: null, textColor: null, fontFamily: null, nameWeight: 700,
    msgWeight: 500, letterSpacing: 0, animation: "slideUp", animMs: 350, ease: "ease-out", distance: 24,
    speed: 70, spacing: 40, direction: "ltr",
    clock: {hour12: false, seconds: true, date: true, dateFormat: "long", timeZone: "", design: "digital", pulse: false},
    np: {layout: "rect", showCover: true, hideWhenIdle: true}
  };
  const PLATFORM_COLORS = {twitch: "#9146ff", youtube: "#ff0033", kick: "#53fc18", tiktok: "#25f4ee", starvios: "#38bdf8"};
  let global = {};

  function merged() {
    // Prioridad: valores por defecto < estilo global del panel < ajustes propios de la fuente.
    const out = Object.assign({}, DEFAULTS, global, local);
    out.clock = Object.assign({}, DEFAULTS.clock, global.clock, local.clock);
    out.np = Object.assign({}, DEFAULTS.np, global.np, local.np);
    return out;
  }

  function hexToRgba(hex, a) {
    const m = /^#?([0-9a-f]{6})$/i.exec(hex || "");
    if (!m) return hex;
    const n = parseInt(m[1], 16);
    return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${a})`;
  }

  function applyStyle() {
    const c = merged(), t = window.MCU_THEMES[c.theme] || window.MCU_THEMES.twitch, s = document.documentElement.style;
    s.setProperty("--font", c.fontFamily ? `"${c.fontFamily}", ${t.font}` : t.font);
    s.setProperty("--fs", c.fontSize + "px");
    s.setProperty("--fg", c.textColor || t.fg);
    s.setProperty("--name", c.nameColor || t.name);
    s.setProperty("--border", c.borderColor || t.border);
    s.setProperty("--bw", (c.borderWidth ?? t.bw) + "px");
    s.setProperty("--radius", (c.radius ?? t.radius) + "px");
    s.setProperty("--blur", t.blur + "px");
    s.setProperty("--shadow", c.shadow || t.shadow);
    s.setProperty("--textshadow", t.textShadow || "none");
    s.setProperty("--gap", c.gap + "px");
    s.setProperty("--anim", c.animMs + "ms");
    s.setProperty("--ease", c.ease);
    s.setProperty("--dist", c.distance + "px");
    s.setProperty("--ls", c.letterSpacing + "px");
    s.setProperty("--nameweight", c.nameWeight);
    s.setProperty("--msgweight", c.msgWeight);
    const A = {fade: ["fadeIn", "fadeOut"], slideUp: ["slideUpIn", "slideUpOut"], slideDown: ["slideDownIn", "slideUpOut"],
      slideLeft: ["slideLeftIn", "slideLeftOut"], slideRight: ["slideRightIn", "slideLeftOut"],
      scale: ["scaleIn", "scaleOut"], pop: ["popIn", "scaleOut"]};
    const a = A[c.animation] || A.slideUp;
    s.setProperty("--in", a[0]);
    s.setProperty("--out", a[1]);
    if (typeof refresh === "function") refresh();
  }

  function hashColor(str) {
    let h = 0;
    for (let i = 0; i < str.length; i++) h = (h * 31 + str.charCodeAt(i)) >>> 0;
    return `hsl(${h % 360} 65% 38% / var(--bgalpha,.9))`;
  }

  function messageBg(m, c) {
    const a = c.bgOpacity;
    switch (c.bgMode) {
      case "none": return "transparent";
      case "solid": return hexToRgba(c.bgColor, a);
      case "random": return `hsl(${Math.floor(Math.random() * 360)} 60% 35% / ${a})`;
      case "platform": return hexToRgba(PLATFORM_COLORS[m.platform] || "#555555", a);
      case "user": return hashColor(m.platform + m.user).replace("var(--bgalpha,.9)", a);
      case "type": return m.highlighted ? hexToRgba("#7c3aed", a) : m.badges.includes("moderator") ? hexToRgba("#16a34a", a)
        : m.badges.includes("subscriber") ? hexToRgba("#2563eb", a) : hexToRgba(c.bgColor, a);
      default: return null; // el del tema
    }
  }

  // ---- Render del texto con emotes (posiciones en code points) ----
  function esc(s) { return s.replace(/[&<>"']/g, ch => ({"&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;"}[ch])); }
  function renderText(m) {
    const cps = Array.from(m.text);
    const em = (m.emotes || []).slice().sort((a, b) => a.start - b.start);
    let out = "", i = 0;
    for (const e of em) {
      if (e.start < i || e.end >= cps.length) continue;
      out += esc(cps.slice(i, e.start).join(""));
      out += `<img class="emote" alt="${esc(cps.slice(e.start, e.end + 1).join(""))}" src="${esc(e.url)}">`;
      i = e.end + 1;
    }
    return out + esc(cps.slice(i).join(""));
  }

  function buildMessage(m, c) {
    const el = document.createElement("div");
    el.className = "msg" + (m.highlighted ? " hl" : "");
    const bg = messageBg(m, c);
    if (bg) el.style.setProperty("--msg-bg", bg);
    let h = "";
    if (c.showPlatform && mode !== "compact") h += `<span class="plat" style="--pc:${PLATFORM_COLORS[m.platform] || "#888"}" title="${esc(m.platform)}"></span>`;
    if (c.showAvatar && m.avatar) h += `<img class="avatar" src="${esc(m.avatar)}" alt="">`;
    h += `<div class="body"><span class="name" style="${m.color ? "--name-c:" + esc(m.color) : ""}">`;
    if (c.showBadges) for (const b of m.badges) h += `<span class="badge ${esc(b)}">${esc(b)}</span>`;
    h += `${esc(m.user)}</span><span class="text${m.action ? " action" : ""}">${renderText(m)}</span></div>`;
    el.innerHTML = h;
    return el;
  }

  // ---- Módulo chat ----
  let chat, refresh, active = [];
  function initChat() {
    chat = document.createElement("div");
    root.appendChild(chat);
    refresh = () => { chat.className = "chat " + mode; };
    refresh();
    if (mode === "horizontal") requestAnimationFrame(tickRibbon);
  }

  function removeMsg(el) {
    if (!el.parentNode) return;
    el.classList.add("out");
    setTimeout(() => el.remove(), merged().animMs);
  }

  function addMessage(m) {
    if (!chat) return;
    const c = merged();
    const el = buildMessage(m, c);
    if (mode === "horizontal") { queue.push(el); return; }
    chat.appendChild(el);
    active.push(el);
    while (active.length > c.maxMessages) removeMsg(active.shift());
    if (c.lifetime > 0) setTimeout(() => { const i = active.indexOf(el); if (i >= 0) active.splice(i, 1); removeMsg(el); }, c.lifetime * 1000);
  }

  // ---- Cinta horizontal ----
  const queue = [], lane = [];
  let lastT = 0;
  function tickRibbon(t) {
    const c = merged(), dt = lastT ? Math.min((t - lastT) / 1000, 0.1) : 0;
    lastT = t;
    const W = chat.clientWidth, ltr = c.direction === "ltr";
    for (const it of lane) {
      it.x += (ltr ? 1 : -1) * c.speed * dt;
      it.el.style.transform = `translate3d(${ltr ? it.x : it.x}px,0,0)`;
    }
    for (let i = lane.length - 1; i >= 0; i--) {
      const it = lane[i];
      if (ltr ? it.x > W : it.x + it.w < 0) { it.el.remove(); lane.splice(i, 1); }
    }
    // Entra el siguiente cuando hay hueco suficiente
    if (queue.length && lane.length < Math.max(1, c.maxMessages)) {
      const last = lane[lane.length - 1];
      const room = !last || (ltr ? last.x >= c.spacing : last.x + last.w <= W - c.spacing);
      if (room) {
        const el = queue.shift();
        chat.appendChild(el);
        const w = el.offsetWidth;
        const it = {el, w, x: ltr ? -w : W};
        lane.push(it);
        el.style.transform = `translate3d(${it.x}px,0,0)`;
      }
    }
    if (queue.length > 50) queue.splice(0, queue.length - 50); // no acumular indefinidamente
    requestAnimationFrame(tickRibbon);
  }

  // ---- Reloj ----
  function initClock() {
    const el = document.createElement("div");
    el.innerHTML = '<div class="box"><div class="time"></div><div class="date"></div></div>';
    root.appendChild(el);
    const tEl = el.querySelector(".time"), dEl = el.querySelector(".date");
    refresh = () => { const k = merged().clock; el.className = "clock " + k.design + (k.pulse ? " pulse" : ""); tick(); };
    function tick() {
      const k = merged().clock, now = new Date(), tz = k.timeZone || undefined;
      try {
        const o = {hour: "2-digit", minute: "2-digit", hour12: !!k.hour12, timeZone: tz};
        if (k.seconds) o.second = "2-digit";
        tEl.textContent = new Intl.DateTimeFormat(undefined, o).format(now);
        dEl.style.display = k.date ? "" : "none";
        const d = {long: {weekday: "long", day: "numeric", month: "long", year: "numeric"},
          short: {day: "2-digit", month: "short"}, numeric: {day: "2-digit", month: "2-digit", year: "numeric"}}[k.dateFormat] || {};
        dEl.textContent = new Intl.DateTimeFormat(undefined, Object.assign({timeZone: tz}, d)).format(now);
      } catch (e) { tEl.textContent = "Zona horaria no válida"; }
    }
    setInterval(tick, 250);
    refresh();
  }

  // ---- Now Playing ----
  let np = null, npEl;
  function fmt(ms) { const s = Math.max(0, Math.floor(ms / 1000)); return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0"); }
  function initNowPlaying() {
    npEl = document.createElement("div");
    root.appendChild(npEl);
    refresh = renderNp;
    renderNp();
    (function loop() { updateProgress(); requestAnimationFrame(loop); })();
  }
  function renderNp() {
    const k = merged().np;
    npEl.className = "np " + k.layout + (k.showCover ? "" : " nocover") + (np && np.playing ? "" : " paused") +
      ((!np || !np.title) && k.hideWhenIdle ? " hidden" : "");
    if (!np) { npEl.innerHTML = ""; return; }
    npEl.innerHTML = `<div class="card"><img class="cover" src="${esc(np.cover || "")}" alt="">
      <div class="info"><div class="src"><span class="eq"><b></b><b></b><b></b></span>${esc(np.source || "")}</div>
      <div class="title">${esc(np.title || "")}</div><div class="artist">${esc(np.artist || "")}</div>
      <div class="album">${esc(np.album || "")}</div>
      <div class="bar"><i></i></div><div class="times"><span class="cur">0:00</span><span>${fmt(np.durationMs || 0)}</span></div></div></div>`;
  }
  function updateProgress() {
    if (!np || !npEl) return;
    const pos = Math.min(np.durationMs || 0, np.positionMs + (np.playing ? Date.now() - np.receivedAt : 0));
    const bar = npEl.querySelector(".bar i"), cur = npEl.querySelector(".cur");
    if (bar) bar.style.width = (np.durationMs ? (pos / np.durationMs) * 100 : 0) + "%";
    if (cur) cur.textContent = fmt(pos);
  }

  // ---- Conexión con el plugin ----
  function connect() {
    const es = new EventSource("/events");
    es.addEventListener("config", e => { try { global = JSON.parse(e.data); applyStyle(); } catch (x) {} });
    es.addEventListener("message", e => { if (module_ === "chat") { try { addMessage(JSON.parse(e.data)); } catch (x) {} } });
    es.addEventListener("clear", () => { active.splice(0).forEach(removeMsg); queue.length = 0; });
    es.addEventListener("nowplaying", e => {
      try { const d = JSON.parse(e.data); np = d.title ? Object.assign(d, {receivedAt: Date.now()}) : null; if (module_ === "nowplaying") renderNp(); } catch (x) {}
    });
    // EventSource reconecta solo; solo nos aseguramos de refrescar la config al volver
    es.onopen = () => fetch("/api/config").then(r => r.json()).then(j => { global = j; applyStyle(); }).catch(() => {});
  }

  if (module_ === "chat") initChat(); else if (module_ === "clock") initClock(); else initNowPlaying();
  applyStyle();
  connect();

  // Solo para previsualizar la capa en un navegador sin el plugin: ?demo=1
  if (qs.get("demo") === "1") {
    const users = ["Ana", "Beto", "Cami", "Dani"], pl = Object.keys(PLATFORM_COLORS);
    let n = 0;
    setInterval(() => addMessage({id: "d" + n, platform: pl[n % 5], user: users[n % 4], text: "Mensaje de prueba número " + n++,
      color: "", badges: n % 3 ? [] : ["moderator"], emotes: [], action: false, highlighted: n % 7 === 0, ts: Date.now()}), 1200);
    np = {title: "Canción de ejemplo", artist: "Artista", album: "Álbum", source: "Demo", durationMs: 200000, positionMs: 30000, playing: true, receivedAt: Date.now()};
    if (module_ === "nowplaying") renderNp();
  }
})();
