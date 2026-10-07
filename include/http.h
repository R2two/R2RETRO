#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace r2n64 {
enum class HttpStage { Idle, Certificates, Network, DnsConnect, Tls, Response, Receiving, Cleanup, Verify, Install, Done };
inline const char* httpStageName(HttpStage stage) {
    switch(stage) {
    case HttpStage::Certificates: return "Leyendo certificados CA";
    case HttpStage::Network: return "Iniciando cliente / red";
    case HttpStage::DnsConnect: return "Resolviendo DNS / conectando";
    case HttpStage::Tls: return "Negociando TLS";
    case HttpStage::Response: return "Esperando respuesta HTTPS";
    case HttpStage::Receiving: return "Recibiendo datos";
    case HttpStage::Cleanup: return "Cerrando conexion HTTPS";
    case HttpStage::Verify: return "Verificando archivo";
    case HttpStage::Install: return "Solicitando instalacion PS4";
    case HttpStage::Done: return "Consulta terminada";
    default: return "Preparando consulta";
    }
}
struct HttpOptions {
    std::atomic<HttpStage>* stage = nullptr;
#ifdef R2N64_HTTP_TESTING
    // Offline fixtures only. The app has no DNS/provider override.
    bool useSystemDns = true;
    std::string dohEndpoint = "https://cloudflare-dns.com/dns-query";
    std::string dohBootstrap = "cloudflare-dns.com:443:1.1.1.1,1.0.0.1";
#endif
    void report(HttpStage value) const { if(stage) stage->store(value); }
};
// Blocking transport: call from the catalog worker, after SDL video on PS4.
// Fixed Cloudflare DoH for updates AND catalog, with request-local bootstrap.
// Uses an explicit PEM CA bundle, verified HTTPS and at most three HTTPS redirects.
// The CA preflight requires a readable regular file of 1 byte to 2 MiB and
// rejects a leaf symlink. Local I/O errors name the stage/errno, never the path.
// On any failure/cancellation body is empty and error is safe to show in the UI.
bool httpGet(const std::string& url, size_t maxBytes, const std::string& caFile,
             const std::atomic<bool>& cancel, std::vector<uint8_t>& body,
             std::string& error, const HttpOptions& options = {});

// Writes to a caller-owned fresh regular file, never retains a PKG in RAM.
// On failure the file may contain partial data and must not be installed.
bool httpDownload(const std::string& url, uint64_t exactBytes, const std::string& caFile,
                  const std::atomic<bool>& cancel, int fd, std::atomic<uint64_t>& received,
                  std::string& error, const HttpOptions& options = {});

// Call only after joining all download workers. Shared PS4 networking stays alive.
void httpShutdown();
}
