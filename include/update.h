#pragma once
#include "http.h"
#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <thread>

namespace r2n64 {
inline constexpr const char* UpdateTitleId = "RNTD00064";
inline constexpr const char* UpdateContentId = "IV0001-RNTD00064_00-R2N64APP00000001";
inline constexpr uint64_t UpdateMaxBytes = 512ull * 1024 * 1024;
struct UpdateRelease {
    std::string version, sfo, channel, url, sha256, notes;
    uint64_t size = 0;
};
struct UpdatePreferences { bool automatic=true, experimental=true; };
UpdatePreferences loadUpdatePreferences(const std::string& root);
bool saveUpdatePreferences(const std::string& root, const UpdatePreferences&, std::string& error);
bool parseUpdateManifest(const std::string&, UpdateRelease&, std::string& error);
bool newerUpdate(const UpdateRelease&, const std::string& currentVersion, const std::string& currentSfo);
// Reads bounded chunks; verifies full SHA-256, PKG identity and embedded SFO.
// Descriptor remains owned by the caller; no filesystem mutation.
bool verifyUpdatePackage(int fd, const UpdateRelease&, const std::atomic<bool>& cancel, std::string& error);
std::string updateManifestUrl(bool experimental);
std::string updatePackageName(const UpdateRelease&);
bool downloadUpdate(const std::string& root, const std::string& ca, const UpdateRelease&,
                    const std::atomic<bool>& cancel, std::atomic<uint64_t>& received,
                    std::string& path, std::string& error, const HttpOptions& options = {});
// Platform adapter registers/starts a local system task. Success means queued,
// never installed. Must not uninstall on conflicts. Caller closes after success.
bool queueUpdateInstall(const std::string& path, const UpdateRelease&, std::string& error);

enum class UpdateJob { Check, Download, Install };
struct UpdateResult {
    UpdateJob job = UpdateJob::Check;
    UpdateRelease release;
    bool ok = false, cancelled = false;
    std::string path, error;
};
class UpdateService {
public:
    ~UpdateService();
    bool start(UpdateJob, const std::string& root, const std::string& ca,
               bool experimental, const UpdateRelease& release = {});
    bool poll(UpdateResult&);
    bool busy() const { return worker_.joinable(); }
    void cancel() { cancel_ = true; }
    void stop();
    uint64_t received() const { return received_; }
    HttpStage stage() const { return stage_; }
private:
    std::thread worker_;
    std::atomic<bool> cancel_{false}, ready_{false};
    std::atomic<uint64_t> received_{0};
    std::atomic<HttpStage> stage_{HttpStage::Idle};
    UpdateResult pending_;
};
}
