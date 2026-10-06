#include "app.h"
#include "startup.h"
#include <cerrno>
#include <fcntl.h>
#include <sstream>
#include <unistd.h>

namespace r2n64 {
namespace {
bool saveReport(const std::string& path, const std::string& report) {
    const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW, 0600);
    if (fd < 0) return false;
    size_t offset = 0;
    while (offset < report.size()) {
        const auto count = ::write(fd, report.data() + offset, report.size() - offset);
        if (count < 0 && errno == EINTR) continue;
        if (count <= 0) { ::close(fd); return false; }
        offset += static_cast<size_t>(count);
    }
    return ::close(fd) == 0;
}
}
bool App::gpuDiagnostic(std::string& error, bool smoke, const std::string& screenshot) {
    constexpr SDL_Color white{245,249,255,255}, muted{186,203,222,255}, accent{169,234,217,255};
    video_.clear(1);
    video_.text("Comprobando GPU…", 140, 130, white, 42);
    video_.text("Shaders y dibujo de una imagen de prueba", 140, 205, muted, 28);
    if (!video_.present(error)) return false;
    log_.write("INFO", "Manual GPU diagnostic requested");
    const auto result = video_.probeGpu();
    std::ostringstream report;
    report << "R2RETRO " R2N64_VERSION " GPU diagnostic\n"
           << "status=" << result.status << "\npassed=" << result.passed
           << "\nsupported=" << result.supported << "\ncontext_created=" << result.contextCreated
           << "\nshader_compiler=" << result.shaderCompiler << "\nvertex_compiled=" << result.vertexCompiled
           << "\nfragment_compiled=" << result.fragmentCompiled << "\nprogram_linked=" << result.programLinked
           << "\nfbo_complete=" << result.framebufferComplete << "\npixels_match=" << result.pixelMatched
           << "\ncontext_restored=" << result.contextRestored << "\nvendor=" << result.vendor
           << "\nrenderer=" << result.renderer << "\nversion=" << result.version
           << "\nshading_language=" << result.shadingLanguage << "\n" << result.log << "\n";
    log_.write(result.passed ? "INFO" : "WARNING", report.str());
    const auto reportPath = platform_.dataPath() + "/logs/gpu-probe.txt";
    const bool saved = saveReport(reportPath, report.str());
    if (!saved) log_.write("ERROR", "Could not write GPU report: " + reportPath);
    if (!result.contextRestored) {
        error = "No se pudo restaurar el contexto gráfico. Consulta startup.log.";
        return false;
    }
    auto draw = [&]() {
        video_.clear(1);
        video_.rect(95, 90, 1730, 900, {3,9,17,214});
        video_.text("Prueba GPU", 140, 130, white, 42);
        video_.text(result.passed ? "Shaders y dibujo: correctos" : "Prueba no completada", 140, 210, accent, 34);
        video_.text(result.status, 140, 265, muted, 24, 1630);
        video_.text("GPU: " + result.renderer, 140, 325, white, 24, 1630);
        video_.text("API: " + result.version, 140, 369, muted, 24, 1630);
        const auto state = [](bool passed) { return passed ? "Correcto" : "No confirmado"; };
        video_.text(std::string("Compilador GLSL: ") + state(result.shaderCompiler), 140, 450, muted, 24);
        video_.text(std::string("Shaders: ") + state(result.vertexCompiled && result.fragmentCompiled) +
                    "   ·   Enlace: " + state(result.programLinked), 140, 500, muted, 24);
        video_.text(std::string("Destino de imagen: ") + state(result.framebufferComplete) +
                    "   ·   Píxeles: " + state(result.pixelMatched), 140, 550, muted, 24);
        video_.text("Esta prueba aún no activa gráficos N64 en GPU.", 140, 650, white, 24);
        video_.text(saved ? "Informe: " + reportPath : "No se pudo guardar el informe; consulta el log.",
                    140, 703, muted, 20, 1630);
        video_.text("X / Círculo  Volver al XMB", 140, 850, accent, 28);
    };
    draw();
    if (!screenshot.empty() && !video_.snapshot(screenshot)) {
        error = "No se pudo capturar la prueba GPU"; return false;
    }
    if (!video_.present(error)) return false;
    startupLog("GPU diagnostic returned to SDL");
    if (smoke) {
        if (!result.passed) error = result.status;
        return result.passed;
    }
    bool released = false;
    while (!quitting_) {
        const auto input = platform_.poll();
        if (input.quit) { quitting_ = true; break; }
        if (!(input.held & (Confirm | Back))) released = true;
        if (released && (input.pressed & (Confirm | Back))) break;
        SDL_Delay(16);
    }
    return true;
}
}
