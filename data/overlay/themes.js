// Temas prediseñados. Cada tema es un conjunto de variables que overlay.js aplica al :root.
// El editor del plugin permite sobreescribir cualquier valor (ver overlay.js -> resolveStyle).
window.MCU_THEMES = {
  minimal:   {label:"Minimalista",      font:"Inter, Segoe UI, sans-serif", bg:"rgba(20,20,24,.55)", fg:"#f2f2f5", name:"#c4b5fd", border:"transparent", bw:0, radius:8,  blur:0,  shadow:"none"},
  clean:     {label:"Apple / Clean UI", font:"Segoe UI Variable, Segoe UI, sans-serif", bg:"rgba(250,250,252,.92)", fg:"#1d1d1f", name:"#0a64d8", border:"rgba(0,0,0,.06)", bw:1, radius:14, blur:0, shadow:"0 4px 18px rgba(0,0,0,.18)"},
  glass:     {label:"Glassmorphism",    font:"Segoe UI, sans-serif", bg:"rgba(255,255,255,.12)", fg:"#ffffff", name:"#e9d5ff", border:"rgba(255,255,255,.28)", bw:1, radius:16, blur:14, shadow:"0 8px 30px rgba(0,0,0,.35)"},
  neon:      {label:"Neon Gaming",      font:"Orbitron, Segoe UI, sans-serif", bg:"rgba(8,6,22,.82)", fg:"#e8f0ff", name:"#39ff9c", border:"#7c3aed", bw:2, radius:6, blur:0, shadow:"0 0 14px rgba(124,58,237,.8)"},
  cyberpunk: {label:"Cyberpunk",        font:"Rajdhani, Segoe UI, sans-serif", bg:"rgba(14,10,28,.88)", fg:"#fff7a8", name:"#ff2a6d", border:"#05d9e8", bw:2, radius:0, blur:0, shadow:"4px 4px 0 #ff2a6d"},
  elegant:   {label:"Dark Elegant",     font:"Georgia, 'Times New Roman', serif", bg:"rgba(12,12,14,.9)", fg:"#e8e4da", name:"#d4af7a", border:"rgba(212,175,122,.35)", bw:1, radius:10, blur:0, shadow:"0 6px 20px rgba(0,0,0,.5)"},
  twitch:    {label:"Modern Twitch",    font:"Roobert, Inter, Segoe UI, sans-serif", bg:"rgba(24,24,27,.9)", fg:"#efeff1", name:"#bf94ff", border:"#9146ff", bw:0, radius:6, blur:0, shadow:"none"},
  pastel:    {label:"Soft Pastel",      font:"Nunito, Segoe UI, sans-serif", bg:"rgba(255,236,245,.92)", fg:"#4b3a52", name:"#c05a9b", border:"rgba(192,90,155,.25)", bw:1, radius:18, blur:0, shadow:"0 4px 14px rgba(192,90,155,.25)"},
  transparent:{label:"Transparent",     font:"Segoe UI, sans-serif", bg:"transparent", fg:"#ffffff", name:"#a5b4fc", border:"transparent", bw:0, radius:0, blur:0, shadow:"none", textShadow:"0 1px 3px rgba(0,0,0,.9)"},
  premium:   {label:"Gradient Premium", font:"Segoe UI, sans-serif", bg:"linear-gradient(135deg,rgba(124,58,237,.9),rgba(37,99,235,.9))", fg:"#ffffff", name:"#fde68a", border:"rgba(255,255,255,.2)", bw:1, radius:14, blur:0, shadow:"0 8px 24px rgba(37,99,235,.35)"}
};
