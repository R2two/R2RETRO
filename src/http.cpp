#include "http.h"
#include <curl/curl.h>
#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <limits>
#include <memory>
#include <mutex>
#include <sys/stat.h>
#include <unistd.h>
#ifdef R2N64_PS4
#include <SDL2/SDL.h>
#include <orbis/Net.h>
#include <orbis/Sysmodule.h>
#endif

namespace r2n64 {
namespace {
std::mutex initializationMutex;
bool curlReady = false;
size_t activeRequests = 0;

bool caFailure(std::string& error, const char* stage, const char* description, int code) {
    error = std::string("Certificados CA: ") + description + " (" + stage + ", errno " +
        std::to_string(code) + ").";
    return false;
}
bool readableCa(const std::string& path, std::string& error) {
    if (path.empty() || path.find('\0') != std::string::npos)
        return caFailure(error, "ruta", "ruta vacia o invalida", EINVAL);
    struct File {
        int descriptor;
        explicit File(int value) : descriptor(value) {}
        ~File() { if (descriptor >= 0) ::close(descriptor); }
        File(const File&) = delete;
        File& operator=(const File&) = delete;
    } file(::open(path.c_str(), O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC));
    if (file.descriptor < 0)
        return caFailure(error, "open", "no se pudo abrir el archivo", errno);
    struct stat information{};
    if (::fstat(file.descriptor, &information) < 0)
        return caFailure(error, "fstat", "no se pudo consultar el archivo", errno);
    if (!S_ISREG(information.st_mode))
        return caFailure(error, "tipo", "se requiere un archivo regular", S_ISDIR(information.st_mode) ? EISDIR : EINVAL);
    if (information.st_size == 0)
        return caFailure(error, "vacio", "el archivo esta vacio", EINVAL);
    constexpr uint64_t maximumCaBytes = 2 * 1024 * 1024;
    if (information.st_size < 0 || static_cast<uint64_t>(information.st_size) > maximumCaBytes)
        return caFailure(error, "tamano", "el archivo excede el limite de 2 MiB", EFBIG);
    unsigned char byte = 0;
    ssize_t count;
    do { count = ::read(file.descriptor, &byte, 1); } while (count < 0 && errno == EINTR);
    if (count != 1)
        return caFailure(error, "read", "no se pudo leer el archivo", count < 0 ? errno : EIO);
    // This preflight diagnoses local file access; curl still opens the explicit
    // CAINFO path and validates PEM, certificate trust and the server hostname.
    return true;
}

bool initialize(std::string& error) {
#ifdef R2N64_PS4
    // Preserve the proven boot order. Downloading is optional and initializes
    // only NET, never USER_SERVICE, SDL, GoldHEN or controller/audio services.
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO)) {
        error = "La red requiere que la interfaz grafica este iniciada.";
        return false;
    }
    static bool netReady = false;
    if (!netReady) {
        // The module can already be loaded by the platform. Its load result
        // alone does not tell us whether networking is initialized/usable.
        const uint32_t module = sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET);
        const int32_t result = sceNetInit();
        if (result < 0) {
            OrbisNetEtherAddr unused{};
            if (sceNetGetMacAddress(&unused, 0) < 0) {
                char detail[96];
                std::snprintf(detail, sizeof(detail), "No se pudo iniciar la red PS4 (%08X, modulo %08X).",
                              static_cast<unsigned>(result), static_cast<unsigned>(module));
                error = detail;
                return false;
            }
        }
        netReady = true;
    }
#endif
    if (!curlReady) {
        if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
            error = "No se pudo iniciar el cliente HTTPS.";
            return false;
        }
        curlReady = true;
    }
    return true;
}

struct RequestLease {
    bool acquired = false;
    bool acquire(std::string& error) {
        std::lock_guard<std::mutex> lock(initializationMutex);
        if (!initialize(error)) return false;
        ++activeRequests;
        acquired = true;
        return true;
    }
    ~RequestLease() {
        if (acquired) {
            std::lock_guard<std::mutex> lock(initializationMutex);
            --activeRequests;
        }
    }
};

struct Transfer {
    const std::atomic<bool>& cancel;
    std::vector<uint8_t>& bytes;
    size_t limit;
    bool exceeded = false;
    bool allocationFailed = false;
    int fd = -1;
    std::atomic<uint64_t>* received = nullptr;
    size_t written = 0;
    bool writeFailed = false;
    const HttpOptions* options = nullptr;
    CURL* curl = nullptr;
};

size_t receive(char* source, size_t width, size_t count, void* opaque) noexcept {
    auto& transfer = *static_cast<Transfer*>(opaque);
    if (transfer.cancel.load(std::memory_order_relaxed)) return 0;
    if (transfer.options) transfer.options->report(HttpStage::Receiving);
    if ((width && count > std::numeric_limits<size_t>::max() / width) ||
        width * count > transfer.limit - transfer.written) {
        transfer.exceeded = true;
        return 0;
    }
    const size_t length = width * count;
    if (!length) return 0;
    if (transfer.fd >= 0) {
        size_t offset = 0;
        while (offset < length) {
            if (transfer.cancel) return 0;
            const auto n = ::write(transfer.fd, source + offset, length - offset);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { transfer.writeFailed = true; return 0; }
            offset += size_t(n);
        }
        transfer.written += length;
        if (transfer.received) *transfer.received = transfer.written;
        return length;
    }
    try {
        const size_t wanted = transfer.bytes.size() + length;
        if (wanted > transfer.bytes.capacity()) {
            const size_t capacity = transfer.bytes.capacity();
            const size_t growth = std::min(transfer.limit - capacity, std::max(capacity / 2, size_t(16384)));
            transfer.bytes.reserve(std::max(wanted, capacity + growth));
        }
        transfer.bytes.insert(transfer.bytes.end(), source, source + length);
        transfer.written += length;
    } catch (...) {
        // Exceptions must not cross libcurl's C callback boundary.
        transfer.allocationFailed = true;
        return 0;
    }
    return length;
}

int progress(void* opaque, curl_off_t, curl_off_t, curl_off_t, curl_off_t) noexcept {
    auto& transfer = *static_cast<Transfer*>(opaque);
    if(transfer.cancel.load(std::memory_order_relaxed)) return 1;
    if(transfer.options && transfer.curl && !transfer.written) {
        double connected=0, tls=0;
        curl_easy_getinfo(transfer.curl,CURLINFO_CONNECT_TIME,&connected);
        curl_easy_getinfo(transfer.curl,CURLINFO_APPCONNECT_TIME,&tls);
        transfer.options->report(tls>0?HttpStage::Response:connected>0?HttpStage::Tls:HttpStage::DnsConnect);
    }
    return 0;
}

bool validUrl(const std::string& url) {
    if (url.size() < 9 || url.size() > 8192) return false;
    const char prefix[] = "https://";
    for (size_t i = 0; i < 8; ++i) {
        const char c = url[i] >= 'A' && url[i] <= 'Z' ? char(url[i] + ('a' - 'A')) : url[i];
        if (c != prefix[i]) return false;
    }
    for (unsigned char c : url) if (c <= 0x20 || c == 0x7f) return false;
    const auto end = url.find_first_of("/?#", 8);
    const auto authority = url.substr(8, end == std::string::npos ? end : end - 8);
    return !authority.empty() && authority.find('@') == std::string::npos;
}

std::string curlError(CURLcode code) {
    switch (code) {
    case CURLE_PEER_FAILED_VERIFICATION:
        return "No se pudo verificar el certificado TLS. Revisa la fecha de la consola y los certificados CA.";
    case CURLE_SSL_CACERT_BADFILE:
        return "El cliente TLS no pudo cargar el archivo de certificados CA (curl " +
            std::to_string(static_cast<int>(code)) + ").";
    case CURLE_SSL_CONNECT_ERROR:
        return "No se pudo establecer una conexion TLS segura.";
    case CURLE_OPERATION_TIMEDOUT:
        return "La descarga supero el tiempo de espera o la conexion es demasiado lenta.";
    case CURLE_COULDNT_RESOLVE_HOST:
    case CURLE_COULDNT_RESOLVE_PROXY:
        return "No se pudo resolver el servidor mediante Cloudflare HTTPS. Revisa la conexion, fecha y certificados CA.";
    case CURLE_COULDNT_CONNECT:
        return "No se pudo conectar al servidor HTTPS.";
    case CURLE_TOO_MANY_REDIRECTS:
        return "El servidor excedio el limite de redirecciones HTTPS.";
    case CURLE_UNSUPPORTED_PROTOCOL:
    case CURLE_URL_MALFORMAT:
    case CURLE_LOGIN_DENIED:
        return "La direccion o redireccion no es una URL HTTPS permitida.";
    case CURLE_PARTIAL_FILE:
        return "La descarga termino antes de recibir el archivo completo.";
    default:
        // Never reflect server bodies, URL credentials, local paths or a curl
        // error buffer into logs/UI. The numeric code is sufficient for support.
        return "Fallo de descarga HTTPS (curl " + std::to_string(static_cast<int>(code)) + ").";
    }
}

CURLcode perform(CURL* easy, Transfer& transfer, const HttpOptions& options) {
    // easy_perform does not call the parent's progress callback regularly while
    // an internal DoH handle waits. Drive multi explicitly so cancellation is
    // observed even during DNS/TLS stalls, without abandoning a live worker.
    std::unique_ptr<CURLM, decltype(&curl_multi_cleanup)> multi(curl_multi_init(),curl_multi_cleanup);
    if(!multi) return CURLE_OUT_OF_MEMORY;
    if(curl_multi_add_handle(multi.get(),easy)!=CURLM_OK) return CURLE_FAILED_INIT;
    CURLcode result=CURLE_FAILED_INIT;
    while(true) {
        if(transfer.cancel.load(std::memory_order_relaxed)) { result=CURLE_ABORTED_BY_CALLBACK; break; }
        int running=0;
        if(curl_multi_perform(multi.get(),&running)!=CURLM_OK) break;
        int remaining=0; bool complete=false;
        while(auto* message=curl_multi_info_read(multi.get(),&remaining)) {
            if(message->msg==CURLMSG_DONE && message->easy_handle==easy) {
                result=message->data.result; complete=true;
            }
        }
        if(complete || !running) break;
        if(curl_multi_poll(multi.get(),nullptr,0,100,nullptr)!=CURLM_OK) break;
    }
    options.report(HttpStage::Cleanup);
    curl_multi_remove_handle(multi.get(),easy);
    return result;
}
}

static bool request(const std::string& url, size_t maxBytes, const std::string& caFile,
             const std::atomic<bool>& cancel, std::vector<uint8_t>& body, std::string& error,
             int fd, std::atomic<uint64_t>* downloaded, const HttpOptions& options) {
    body.clear();
    error.clear();
    if (cancel.load(std::memory_order_relaxed)) { error = "Descarga cancelada."; return false; }
    if (!validUrl(url)) { error = "Se requiere una URL HTTPS sin credenciales."; return false; }
    if (!maxBytes || maxBytes > static_cast<uint64_t>(std::numeric_limits<curl_off_t>::max())) {
        error = "El limite de descarga no es valido."; return false;
    }
    options.report(HttpStage::Certificates);
    if (!readableCa(caFile, error)) return false;
    options.report(HttpStage::Network);
    RequestLease lease;
    if (!lease.acquire(error)) return false;
    // A fresh buffer caps retained capacity independently of an earlier request.
    std::vector<uint8_t> received;
    Transfer transfer{cancel, received, maxBytes};
    transfer.fd = fd;
    transfer.received = downloaded;
    transfer.options = &options;
    // RESOLVE populates curl's request-local DNS cache, also used by DoH's
    // internal handles. Keep the list alive through easy cleanup.
    std::unique_ptr<curl_slist, decltype(&curl_slist_free_all)> bootstrap(nullptr, curl_slist_free_all);
    // Keep callback state alive until after curl_easy_cleanup on every exit.
    std::unique_ptr<CURL, decltype(&curl_easy_cleanup)> curl(curl_easy_init(), curl_easy_cleanup);
    if (!curl) { error = "No se pudo crear la descarga HTTPS."; return false; }
    transfer.curl = curl.get();
    auto set = [&](CURLoption key, auto value) { return curl_easy_setopt(curl.get(), key, value) == CURLE_OK; };
    bool configured = set(CURLOPT_URL, url.c_str()) &&
        set(CURLOPT_HTTPGET, 1L) && set(CURLOPT_FOLLOWLOCATION, 1L) && set(CURLOPT_MAXREDIRS, 3L) &&
        set(CURLOPT_DISALLOW_USERNAME_IN_URL, 1L) && set(CURLOPT_NETRC, long(CURL_NETRC_IGNORED)) &&
        set(CURLOPT_SSL_VERIFYPEER, 1L) && set(CURLOPT_SSL_VERIFYHOST, 2L) &&
        set(CURLOPT_SSLVERSION, long(CURL_SSLVERSION_TLSv1_2)) && set(CURLOPT_CAINFO, caFile.c_str()) &&
        set(CURLOPT_NOSIGNAL, 1L) && set(CURLOPT_CONNECTTIMEOUT, 12L) && set(CURLOPT_TIMEOUT, fd >= 0 ? 1800L : 40L) &&
        set(CURLOPT_LOW_SPEED_LIMIT, 1024L) && set(CURLOPT_LOW_SPEED_TIME, 10L) &&
        set(CURLOPT_MAXFILESIZE_LARGE, static_cast<curl_off_t>(maxBytes)) && set(CURLOPT_FAILONERROR, 1L) &&
        set(CURLOPT_ACCEPT_ENCODING, "identity") && set(CURLOPT_USERAGENT, "R2RETRO/HTTPS") &&
        set(CURLOPT_WRITEFUNCTION, receive) && set(CURLOPT_WRITEDATA, &transfer) &&
        set(CURLOPT_XFERINFOFUNCTION, progress) && set(CURLOPT_XFERINFODATA, &transfer) &&
        set(CURLOPT_NOPROGRESS, 0L);
    const char* dohEndpoint="https://cloudflare-dns.com/dns-query";
    const char* dohBootstrap="cloudflare-dns.com:443:1.1.1.1,1.0.0.1";
#ifdef R2N64_HTTP_TESTING
    dohEndpoint=options.dohEndpoint.c_str();
    dohBootstrap=options.dohBootstrap.c_str();
    if(!options.useSystemDns)
#endif
    {
        if(*dohBootstrap) bootstrap.reset(curl_slist_append(nullptr,dohBootstrap));
        // curl 7.80 DoH inherits CAINFO. Peer and host checks for its internal
        // HTTPS handles are separate options, explicitly required here too.
        configured = configured && validUrl(dohEndpoint) &&
            (!*dohBootstrap || (bootstrap && set(CURLOPT_RESOLVE,bootstrap.get()))) &&
            set(CURLOPT_DOH_URL, dohEndpoint) &&
            set(CURLOPT_DOH_SSL_VERIFYPEER, 1L) && set(CURLOPT_DOH_SSL_VERIFYHOST, 2L);
    }
#if LIBCURL_VERSION_NUM >= 0x075500
    configured = configured && set(CURLOPT_PROTOCOLS_STR, "https") && set(CURLOPT_REDIR_PROTOCOLS_STR, "https");
#else
    // PacBrew provides curl 7.80.0, before the *_STR options were introduced.
    configured = configured && set(CURLOPT_PROTOCOLS, long(CURLPROTO_HTTPS)) &&
        set(CURLOPT_REDIR_PROTOCOLS, long(CURLPROTO_HTTPS));
#endif
    if (!configured) { error = "El cliente HTTPS no admite la configuracion segura requerida."; return false; }
    options.report(HttpStage::DnsConnect);
    const CURLcode result = perform(curl.get(),transfer,options);
    long status = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &status);
    options.report(HttpStage::Cleanup);
    curl.reset(); // Diagnostic stays visible if a platform resolver takes time to join.
    if (cancel.load(std::memory_order_relaxed)) error = "Descarga cancelada.";
    else if (transfer.exceeded || result == CURLE_FILESIZE_EXCEEDED) error = "El archivo supera el limite de descarga.";
    else if (transfer.allocationFailed) error = "No hay memoria suficiente para la descarga.";
    else if (transfer.writeFailed) error = "No se pudo escribir la descarga. Comprueba el espacio disponible.";
    else if (status >= 400) error = status == 404 ? "Archivo no encontrado (HTTP 404)." :
        "El servidor rechazo la descarga (HTTP " + std::to_string(status) + ").";
    else if (result != CURLE_OK) error = curlError(result);
    else if (fd >= 0 && (status != 200 || transfer.written != maxBytes)) error = "El PKG recibido no tiene el tamaño esperado.";
    else if (status < 200 || status >= 300) error = "Respuesta HTTPS inesperada (HTTP " + std::to_string(status) + ").";
    else { body.swap(received); return true; }
    return false;
}

bool httpGet(const std::string& url, size_t maxBytes, const std::string& caFile,
             const std::atomic<bool>& cancel, std::vector<uint8_t>& body, std::string& error,
             const HttpOptions& options) {
    return request(url, maxBytes, caFile, cancel, body, error, -1, nullptr, options);
}
bool httpDownload(const std::string& url, uint64_t exactBytes, const std::string& caFile,
                  const std::atomic<bool>& cancel, int fd, std::atomic<uint64_t>& received,
                  std::string& error, const HttpOptions& options) {
    received = 0;
    struct stat st{};
    if (!exactBytes || exactBytes > 512ull * 1024 * 1024 || fd < 0 ||
        ::fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != 0 ||
        ::lseek(fd, 0, SEEK_CUR) != 0) {
        error = "Destino o tamaño de descarga no válido."; return false;
    }
    std::vector<uint8_t> unused;
    return request(url, size_t(exactBytes), caFile, cancel, unused, error, fd, &received, options);
}

void httpShutdown() {
    std::lock_guard<std::mutex> lock(initializationMutex);
    // Fail safe if a caller violated the join-before-shutdown contract.
    if (curlReady && activeRequests == 0) {
        curl_global_cleanup();
        curlReady = false;
    }
    // NET may now be used by another subsystem. Do not terminate shared PS4
    // networking or unload its module from this optional transport layer.
}
}
