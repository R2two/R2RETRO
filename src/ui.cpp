#include "ui.h"
#include <algorithm>
#include <cmath>
namespace r2n64 {
namespace {
constexpr SDL_Color white{245,249,252,255}, muted{183,199,214,240}, accent{124,221,239,255};
enum class Icon { Gamepad, Drive, Sliders, Info, Search, Folder, Screen, Power, Cartridge, Handheld, Advance };
void icon(Video& v, Icon type, int cx, int cy, int size, SDL_Color color, int stroke = 3) {
    auto p = [=](int x, int y) { return SDL_Point{cx + (x-32)*size/64, cy + (y-32)*size/64}; };
    auto l = [&](int x1, int y1, int x2, int y2) {
        auto a=p(x1,y1), b=p(x2,y2); v.line(a.x,a.y,b.x,b.y,color,stroke);
    };
    auto c = [&](int x, int y, int radius) { auto a=p(x,y); v.circle(a.x,a.y,radius*size/64,color,stroke); };
    switch (type) {
    case Icon::Handheld:
        v.path({p(15,3),p(49,3),p(49,53),p(41,61),p(15,61),p(15,3)},color,stroke);
        v.path({p(21,11),p(43,11),p(43,32),p(21,32),p(21,11)},color,stroke);
        l(20,44,30,44); l(25,39,25,49); c(38,46,2); c(44,41,2); l(30,55,35,55); break;
    case Icon::Advance:
        v.path({p(6,14),p(58,14),p(63,32),p(56,50),p(8,50),p(1,32),p(6,14)},color,stroke);
        v.path({p(19,21),p(45,21),p(45,43),p(19,43),p(19,21)},color,stroke);
        l(5,32,15,32); l(10,27,10,37); c(52,35,2); c(57,29,2); break;
    case Icon::Gamepad:
        v.path({p(17,18),p(47,18),p(54,23),p(61,46),p(57,52),p(50,52),p(42,42),p(22,42),p(14,52),p(7,52),p(3,46),p(10,23),p(17,18)},color,stroke);
        l(15,31,27,31); l(21,25,21,37); c(43,29,2); c(51,36,2); break;
    case Icon::Drive:
        v.path({p(12,12),p(52,12),p(58,40),p(58,52),p(6,52),p(6,40),p(12,12)},color,stroke);
        l(7,39,57,39); l(14,46,34,46); c(48,46,1); break;
    case Icon::Sliders:
        l(12,7,12,57); l(32,7,32,57); l(52,7,52,57);
        l(6,23,18,23); l(6,27,18,27); l(26,41,38,41); l(26,45,38,45); l(46,17,58,17); l(46,21,58,21); break;
    case Icon::Info:
        c(32,32,26); c(32,20,1); l(32,29,32,46); l(27,46,37,46); break;
    case Icon::Search:
        c(26,26,18); l(40,40,57,57); break;
    case Icon::Folder:
        v.path({p(5,18),p(25,18),p(31,25),p(59,25),p(59,52),p(5,52),p(5,18)},color,stroke);
        l(6,32,58,32); break;
    case Icon::Screen:
        v.path({p(5,10),p(59,10),p(59,45),p(5,45),p(5,10)},color,stroke);
        l(32,45,32,55); l(19,55,45,55); break;
    case Icon::Power:
        c(32,35,22); l(32,4,32,31); break;
    case Icon::Cartridge:
        v.path({p(12,13),p(52,13),p(58,23),p(58,53),p(6,53),p(6,23),p(12,13)},color,stroke);
        v.path({p(18,24),p(46,24),p(46,41),p(18,41),p(18,24)},color,stroke);
        for (int x=18; x<48; x+=7) l(x,49,x,53);
        break;
    }
}
struct Item { std::string title, subtitle, hint; Icon icon; };
size_t count(const View& s) {
    switch (s.menu.category) {
    case Category::Library: return s.menu.consoleDirectory() ? librarySystems.size() : s.games->empty() ? 2 : s.games->size();
    case Category::Storage: return std::max(size_t(1),s.roots.size());
    case Category::Settings: return 10;
    default: return 4;
    }
}
Item item(const View& s, size_t index) {
    switch (s.menu.category) {
    case Category::Library:
        if (s.menu.consoleDirectory()) {
            return {librarySystems[index] == SystemType::NintendoEntertainmentSystem ? "NES" : systemName(librarySystems[index]),
                s.scanning ? "Buscando juegos…" : std::to_string(s.consoleCounts[index]) + (s.consoleCounts[index] == 1 ? " juego" : " juegos"),
                "X  Abrir consola", librarySystems[index] == SystemType::GameBoyAdvance ? Icon::Advance :
                    (librarySystems[index] == SystemType::GameBoy || librarySystems[index] == SystemType::GameBoyColor) ?
                        Icon::Handheld : librarySystems[index] == SystemType::NintendoEntertainmentSystem ? Icon::Cartridge : Icon::Gamepad};
        }
        if (s.games->empty()) {
            if (index == 0) return {s.scanning ? "Buscando juegos…" : "No hay juegos en esta consola",
                std::string("Carpeta: R2N64/roms/") + systemId(s.menu.librarySystem()), "X  Buscar juegos", Icon::Search};
            return {"Dónde guardar tus juegos", "Memoria interna y almacenamiento USB", "X  Ver ubicaciones", Icon::Folder};
        } else {
            const auto& g = (*s.games)[index];
            const auto* metadata = index == s.menu.selected() ? s.metadata : nullptr;
            const auto& title = metadata && !metadata->title.empty() ? metadata->title : g.title;
            const auto& region = metadata && !metadata->region.empty() ? metadata->region : g.region;
            return {title, std::string(systemName(g.system)) + "  ·  " + region, s.menu.details ? "X  Iniciar juego" : "X  Ver detalles", Icon::Cartridge};
        }
    case Category::Storage:
        return {index == 0 ? "Memoria interna" : "USB " + std::to_string(index),
            index < s.roots.size() ? s.roots[index] : "No hay ubicaciones configuradas",
            s.menu.details ? "X  Buscar juegos" : "X  Ver ubicación", index == 0 ? Icon::Drive : Icon::Folder};
    case Category::Settings:
        if (index == 9) return {"Actualizaciones", s.updateAvailable ? "Hay una nueva versión disponible" : "Versiones y descarga desde GitHub", "X  Abrir actualizaciones", Icon::Info};
        if (index == 0) return {"Apariencia", "Ajusta el contraste del fondo y del menú.",
            s.menu.details ? "X  Cambiar contraste" : "X  Ajustar contraste", Icon::Screen};
        if (index == 1) return {"Controles", "Nintendo 64, NES, SNES y consolas portátiles.", "X  Ver controles", Icon::Gamepad};
        if (index == 3) return {"Rendimiento", s.menu.parallelRendering ? "Nintendo 64: renderizado con 4 hilos" : "Nintendo 64: renderizado con 1 hilo",
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Sliders};
        if (index == 4) return {"CPU de Nintendo 64", s.menu.automaticCpu ? "Automática: recompilador si está disponible" : "Intérprete con caché",
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Sliders};
        if (index == 5) return {"Procesamiento de audio", s.menu.audioHle ? "Nintendo 64: tareas reconocidas aceleradas" : "Nintendo 64: procesamiento original del RSP",
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Sliders};
        if (index == 6) return {"Medir rendimiento", s.menu.profileCore ? "Medición por componente activada" : "Medición por componente desactivada",
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Sliders};
        if (index == 8) return {"Perfil por juego N64", s.menu.automaticProfile ? "Automático · juegos reconocidos" : "Manual · tus ajustes",
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Sliders};
        if (index == 7) return {"Gráficos de Nintendo 64", s.menu.automaticProfile ?
            std::string("Base: ") + s.menu.graphicsLabel() + " · perfil por juego activo" : s.menu.graphicsLabel(),
            s.menu.details ? "X  Cambiar modo" : "X  Ver opciones", Icon::Screen};
        return {"Diagnóstico", "Mando, almacenamiento y estado de la aplicación.", "X  Ver estado", Icon::Sliders};
    case Category::About:
        if (index == 0) return {"R2RETRO", "Homebrew creado por Rtwo / R2", "X  Autoría, mejoras y Ko-fi", Icon::Info};
        if (index == 1) return {"Prueba Nintendo 64", "Comprueba imagen, sonido y controles.", "X  Iniciar prueba incluida", Icon::Cartridge};
        if (index == 2) return {"Prueba GPU", "Comprueba el renderizado de la consola.", "X  Iniciar prueba GPU", Icon::Screen};
        return {"Salir de R2RETRO", "Cerrar la aplicación", "X  Salir", Icon::Power};
    }
    return {};
}
void detail(Video& v, const View& s) {
    const auto selected = s.menu.selected();
    const int x=1260, w=518;
    v.rect(1220,414,610,448,{9,15,25,242});
    v.rect(x,447,45,3,accent);
    int y=482;
    auto title = [&](const std::string& t) { v.text(t,x,y,white,28,w); y+=62; };
    auto row = [&](const std::string& t, SDL_Color c=muted) { v.text(t,x,y,c,20,w); y+=39; };
    if (s.menu.category == Category::Library && !s.games->empty()) {
        const auto& game = (*s.games)[selected];
        if (s.metadata) {
            const auto& metadata = *s.metadata;
            v.text(metadata.title.empty() ? game.title : metadata.title, x, 472, white, 28, w);
            const auto& region = metadata.region.empty() ? game.region : metadata.region;
            v.text(std::string(systemName(game.system)) + " · " + region, x, 522, muted, 20, w);
            v.rect(x, 565, 176, 242, {3,9,17,175});
            if (!v.libraryArtwork(x,565,176,242)) {
                icon(v, Icon::Cartridge, x + 88, 664, 66, muted, 3);
                v.text("Sin imagen", x+26, 720, muted, 20, 148);
            }
            const int column = x + 202, columnWidth = w - 202;
            const auto field = [&](const char* label, const std::string& value, int top) {
                v.text(label, column, top, muted, 20, columnWidth);
                v.text(value.empty() ? "Sin datos" : value, column, top+25, white, 20, columnWidth);
            };
            field("Año", metadata.year, 562);
            field("Género", metadata.genre, 611);
            field("Desarrollo", metadata.developer, 660);
            field("Publicación", metadata.publisher, 709);
            field("Jugadores", metadata.players, 758);
            v.text("X  Iniciar juego", x, 820, accent, 20, w);
            v.text("Libretro", x+421, 820, muted, 20, 97);
            return;
        }
        title(game.title); row(systemName(game.system)); row(game.region); row("ID   " + game.id);
        row("Tamaño   " + std::to_string(game.size/1024) + " KiB");
        y+=12; row("X  Iniciar juego",accent);
        row(game.system == SystemType::Nintendo64 ? "OPTIONS abre la pausa." : "L3 + R3 abre la pausa.");
        row("Compatibilidad y rendimiento en pruebas.");
    } else if (s.menu.category == Category::Storage) {
        title(selected == 0 ? "Memoria interna" : "USB " + std::to_string(selected));
        const std::string root = selected < s.roots.size() ? s.roots[selected] : "";
        size_t found = 0;
        for (const auto& g : *(s.allGames ? s.allGames : s.games)) if (g.path.compare(0,root.size()+1,root+"/")==0) ++found;
        row("Carpeta de juegos"); row(root,white);
        row(".nes / .sfc / .smc / .gb / .gbc / .gba");
        row(".z64 / .n64 / .v64 · " + std::to_string(found) + " juegos");
        row("Carpetas nes, snes, gb, gbc, gba, n64.");
        y+=12; row(s.scanning ? "Buscando…" : "X  Buscar en todas las ubicaciones",accent);
    } else if (s.menu.category == Category::Settings) {
        if (selected == 0) {
            title("Apariencia"); row("Fondo: habitación Nintendo"); row("Ajuste a pantalla: 1920 × 1080");
            const char* presets[]={"Equilibrado", "Más oscuro", "Más luminoso"};
            row(std::string("Contraste: ") + presets[s.menu.contrast],white);
            y+=20; row("X  Cambiar contraste",accent); row("El ajuste dura esta sesión.");
        } else if (selected == 1) {
            title("Dentro del juego");
            row("NES / GB / GBA: X A · Cuadrado B");
            row("SNES: X B · Círculo A · Cuadrado Y");
            row("SNES: Triángulo X · L1/R1 L/R");
            row("N64: táctil Start · OPTIONS pausa");
            row("Otros: OPTIONS Start · táctil Select");
            row("Otros: L3+R3 pausa · R2 avance");
            row("PC: Enter Start · Tab Select · Esc pausa");
        } else if (selected == 3) {
            title("Rendimiento");
            row(s.menu.parallelRendering ? "Renderizado: 4 hilos" : "Renderizado: 1 hilo",white);
            row("Afecta solo a Angrylion (CPU).");
            row("1 hilo permite comparar resultados.");
            row("Se aplica al iniciar la próxima ROM.");
            row("El ajuste dura esta sesión.");
            y+=12; row("X  Cambiar modo",accent);
            row("100 % = velocidad de emulación normal.");
        } else if (selected == 4) {
            title("CPU de Nintendo 64");
            row(s.menu.automaticCpu ? "Modo: automático" : "Modo: intérprete",white);
            row("Automático utiliza el recompilador");
            row("si la consola permite ejecutarlo.");
            row("Si no, conserva el intérprete.");
            row("Se aplica al iniciar la próxima ROM.");
            y+=12; row("X  Cambiar modo",accent);
            row("El modo activo se muestra en pausa.");
        } else if (selected == 5) {
            title("Procesamiento de audio");
            row(s.menu.audioHle ? "Modo: acelerado (HLE)" : "Modo: original (LLE)",white);
            row("Acelera las tareas reconocidas.");
            row("Las demás usan el RSP original.");
            row("Si cambia el sonido, compara con");
            row("el modo original al reiniciar la ROM.");
            y+=12; row("X  Cambiar modo",accent);
            row("El ajuste dura esta sesión.");
        } else if (selected == 6) {
            title("Medir rendimiento");
            row(s.menu.profileCore ? "Medición: activada" : "Medición: desactivada",white);
            row("Tiempo por componente del núcleo.");
            row("Resultados en pausa y en el log.");
            row("Añade un pequeño coste de medición.");
            row("Se aplica a la próxima ROM.");
            row("El ajuste dura esta sesión.");
            y+=12; row("X  Cambiar modo",accent);
        } else if (selected == 8) {
            title("Perfil por juego N64");
            row(s.menu.automaticProfile ? "Modo: automático" : "Modo: manual",white);
            row("Reconoce el contenido y la revisión.");
            row("Mario USA / Zelda USA 1.2: GPU + HLE.");
            row("Perfiles experimentales; otros usan");
            row("tus ajustes. Se aplica al abrir juego.");
            row("Cambiar CPU, gráficos o audio activa");
            row("modo manual. No modifica guardados.");
            y+=12; row("X  Cambiar modo",accent);
        } else if (selected == 7) {
            title("Gráficos de Nintendo 64");
            row(s.menu.graphicsLabel(),white);
            row("GPU dibuja a 320 × 240; escala a 4:3.");
            row(s.menu.automaticProfile ? "El perfil por juego puede cambiarlo." : "Se aplica a la próxima ROM.");
            row("Si no inicia, vuelve al modo CPU.");
            row("HLE: tareas reconocidas; otras en LLE.");
            row("GPU + RSP LLE permite comparar.");
            row("El ajuste dura esta sesión.");
            y+=12; row("X  Cambiar modo",accent);
        } else {
            title("Diagnóstico"); row(s.connected ? "Mando conectado" : "Mando no detectado",white);
            row(s.storage ? "Datos y log disponibles" : "Almacenamiento no disponible",white);
            row(s.background ? "Fondo cargado" : "Fondo no disponible"); row("SDL2  ·  1920 × 1080");
            row(s.platform); row(s.dataError.empty() ? "N64 / NES / SNES / GB / GBC / GBA" : s.dataError,accent);
        }
    } else {
        title(selected == 0 ? "R2RETRO  v"+s.version : "En desarrollo");
        // Twelve short lines fit the detail panel without covering status hints.
        const auto aboutRow = [&](const std::string& text, SDL_Color color=muted) {
            v.text(text,x,y,color,20,w); y+=26;
        };
        aboutRow("Homebrew PS4 creado por Rtwo / R2",white);
        aboutRow("N64 · NES · SNES · GB · GBC · GBA");
        aboutRow("XMB, biblioteca por consola y juegos USB.");
        aboutRow("Partidas, estados, capturas y avance rápido.");
        aboutRow("Marcos, LCD/CRT y color GB opcional.");
        aboutRow("Carátulas y fichas de Libretro por internet.");
        aboutRow("N64: GPU y perfiles experimentales.");
        aboutRow("Actualizador desde el menú: experimental.");
        aboutRow("Núcleos: Mupen64Plus-Next, SameBoy, mGBA,");
        aboutRow("FCEUmm y bsnes-mercury. Gracias a sus autores.");
        aboutRow("Apoya el desarrollo: invítame un café",white);
        aboutRow("https://ko-fi.com/rtwo_",accent);
    }
}
void buttonHint(Video& v, int x, int y, char kind, const std::string& text) {
    const SDL_Color c{233,241,249,245};
    if (kind == 'x') { v.line(x-6,y-6,x+6,y+6,c,2); v.line(x+6,y-6,x-6,y+6,c,2); }
    else if (kind == 'o') v.circle(x,y,9,c,2);
    else if (kind == 't') v.path({{x,y-10},{x-10,y+8},{x+10,y+8},{x,y-10}},c,2);
    else v.path({{x-8,y-8},{x+8,y-8},{x+8,y+8},{x-8,y+8},{x-8,y-8}},c,2);
    v.text(text,x+23,y-15,c,20);
}
}
void renderUI(Video& v, const View& s) {
    if (s.updatesOpen) {
        v.clear(s.menu.contrast);
        v.rect(210,110,1500,810,{7,16,28,244});
        v.rect(210,110,5,810,accent);
        v.text("Actualizaciones de R2RETRO",275,155,white,42,1360);
        v.text("Instalada: v"+s.version+"  ·  GitHub / R2two / R2RETRO",278,220,muted,24,1300);
        const std::string labels[]={"Buscar ahora",std::string("Canal: ")+(s.updatePreferences.experimental?"Experimental":"Estable"),
            std::string("Buscar al iniciar: ")+(s.updatePreferences.automatic?"Sí":"No"),
            "Descargar actualización", "Instalar y cerrar R2RETRO", "Volver"};
        for(size_t i=0;i<6;++i) {
            const int y=295+int(i)*56;
            if(i==s.updateSelection) v.rect(264,y-10,690,51,{32,66,82,240});
            const bool enabled=(!s.updateBusy || (!s.updateInstalling && i==2)) &&
                (i!=3||s.updateAvailable) && (i!=4||s.updateDownloaded);
            v.text(labels[i],286,y,enabled?white:muted,28,650);
        }
        v.text(s.updateAvailable?"Disponible: v"+s.updateRelease.version:"Estado",1010,320,accent,28,620);
        auto lines=[&](const std::string& text,size_t width,size_t maximum,int x,int y,int pixels) {
            size_t offset=0;
            for(size_t line=0;line<maximum && offset<text.size();++line) {
                size_t end=std::min(text.size(),offset+width);
                if(end<text.size()) {
                    const auto space=text.rfind(' ',end);
                    if(space!=std::string::npos && space>offset) end=space;
                    else while(end>offset && (static_cast<unsigned char>(text[end])&0xc0)==0x80) --end;
                }
                v.text(text.substr(offset,end-offset),x,y+int(line)*30,muted,20,pixels);
                offset=end;
                while(offset<text.size() && text[offset]==' ') ++offset;
            }
        };
        if(s.updateAvailable) {
            v.text(std::to_string(s.updateRelease.size/(1024*1024))+" MiB",1010,368,muted,24,620);
            lines(s.updateRelease.notes,52,5,1010,420,620);
        }
        if(s.updateConfirm) {
            v.text("La app se cerrará para instalar.",1010,594,white,24,620);
            v.text("X confirma · Círculo cancela",1010,638,accent,24,620);
        }
        lines(s.updateStatus,100,3,278,765,1370);
        lines(s.updateDiagnostic.empty()?"DNS HTTPS (Cloudflare) · Activo para R2RETRO":s.updateDiagnostic,105,2,278,683,1370);
        v.text(s.updateBusy?"Círculo: solicitar cancelación y volver (excepto instalación)":"X: elegir   ·   Círculo: volver",278,870,accent,20,1330);
        return;
    }
    v.clear(s.menu.contrast);
    v.rect(90,65,5,48,accent);
    v.text("R2RETRO",114,56,white,48);
    if (s.menu.category == Category::Library) {
        v.text(s.menu.consoleDirectory() ? "Biblioteca / Consolas" :
            std::string("Biblioteca / ") + systemName(s.menu.librarySystem()) + "  ·  L1 / R1 Cambiar consola", 114, 126, muted, 24);
    }
    else v.text("Nintendo 64 · NES · SNES · GB · GBC · GBA",114,126,muted,20);
    v.circle(1574,91,4,s.connected ? accent : muted,3);
    v.text(s.connected ? "Mando conectado" : s.desktop ? "Teclado disponible" : "Conecta tu mando",1590,76,muted,20,248);
    v.text("v"+s.version,1574,112,muted,20,264);

    const char* categories[]={"Biblioteca","Almacenamiento","Ajustes","Acerca de"};
    const Icon icons[]={Icon::Gamepad,Icon::Drive,Icon::Sliders,Icon::Info};
    for (int i=0;i<4;++i) {
        const int x=430+int((i-s.categoryPosition)*275);
        if (x < 80 || x > 1800) continue;
        const bool active = i==int(s.menu.category);
        SDL_Color color = active ? white : SDL_Color{183,199,214,180};
        if (active) {
            v.rect(x-68,220,136,113,{20,36,47,155});
            v.line(x-32,379,x+32,379,accent,3);
        }
        icon(v,icons[i],x+2,284,active?72:54,{0,0,0,135},4);
        icon(v,icons[i],x,282,active?72:54,color,active?3:2);
        v.centered(categories[i],x,343,color,active?28:24);
    }
    v.line(430,392,430,445,{241,250,255,65},1);
    v.circle(430,449,2,accent,2);
    v.clip(330,404,880,462);
    const auto selected=s.menu.selected();
    const int n=int(count(s));
    for (int delta=-1; delta<=2; ++delta) {
        const int index=int(selected)+delta;
        if (index<0 || index>=n) continue;
        const bool active=delta==0;
        int y=538 + (delta<0 ? delta*112 : delta*152) + int(s.itemOffset);
        const auto entry=item(s,size_t(index));
        const uint8_t alpha=uint8_t((active?255:delta<0?125:175)*s.contentAlpha);
        SDL_Color color{245,250,255,alpha};
        if (active) {
            v.rect(372,y-50,826,163,{15,27,37,uint8_t(alpha*.82f)});
            v.rect(372,y-50,4,163,{124,221,239,alpha});
        }
        const bool artwork = active && s.menu.category == Category::Library && !s.menu.consoleDirectory() && !s.games->empty() &&
            s.metadata && v.libraryArtwork(386,y-52,88,104);
        const bool logo = s.menu.category == Category::Library && s.menu.consoleDirectory() &&
            v.consoleLogo(librarySystems[size_t(index)],386,y-(active?40:20),88,active?80:40,alpha);
        if (!artwork && !logo) {
            icon(v,entry.icon,432,y+2,active?58:35,{0,0,0,uint8_t(alpha*.6f)},4);
            icon(v,entry.icon,430,y,active?58:35,color,active?3:2);
        }
        v.text(entry.title,512,y-(active?31:19),color,active?42:28,675);
        if (active) {
            SDL_Color body{223,233,248,alpha};
            v.text(entry.subtitle,514,y+31,body,24,665);
            v.text(entry.hint,514,y+76,{124,221,239,alpha},20,665);
        }
    }
    v.unclip();
    if (s.menu.category == Category::Library) {
        if (s.menu.consoleDirectory() && selected < librarySystems.size())
            v.consoleLogo(librarySystems[selected],1260,480,520,280);
        else v.consoleLogo(s.menu.librarySystem(),1280,60,230,115);
    }
    if (s.menu.details) detail(v,s);
    if (s.menu.exitPrompt)
        v.text("Pulsa Círculo / Esc otra vez para salir.",512,892,white,24,1230);
    else if (!s.message.empty()) v.text(s.message,512,892,muted,20,1230);
    else if (s.menu.category == Category::Library && !s.libraryStatus.empty())
        v.text(s.libraryStatus,512,892,s.downloading ? accent : muted,20,1230);
    else if (s.scanning) v.text("Buscando en tus carpetas de juegos…",512,892,muted,20,1230);
    else if (s.updateAvailable) v.text("Nueva versión v"+s.updateRelease.version+" · Ajustes → Actualizaciones",512,892,accent,20,1230);

    v.rect(0,948,1920,132,{7,13,20,205});
    v.line(90,950,1830,950,{124,221,239,55},1);
    buttonHint(v,106,990,'x',"Abrir"); buttonHint(v,258,990,'o',"Volver");
    buttonHint(v,427,990,'t',"Buscar juegos"); buttonHint(v,665,990,'s',"Diagnóstico");
    if (s.menu.category == Category::Library && ((!s.menu.consoleDirectory() && !s.games->empty()) || s.downloading))
        v.text(std::string(s.desktop ? "OPTIONS / M  " : "OPTIONS  ") +
            (s.downloading ? "Cancelar" : "Descargar ficha"),905,975,accent,20,425);
    v.text("← →  Categorías     ↑ ↓  Opciones",1370,975,muted,20,460);
    if (s.menu.category==Category::Library)
        v.text(s.scanning ? "Buscando…" : s.menu.consoleDirectory() ?
            std::to_string(librarySystems.size()) + " consolas" : std::to_string(s.games->size())+
                (s.games->size() == 1 ? " juego en esta consola" : " juegos en esta consola"),90,895,muted,20,380);
}
}
