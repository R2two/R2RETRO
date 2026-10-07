#include "library_metadata.h"
#include "http.h"
#include "rom.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

using namespace r2n64;
namespace fs = std::filesystem;
namespace {
using Bytes = std::vector<uint8_t>;
void require(bool condition, const std::string& description) {
    if (!condition) throw std::runtime_error(description);
}
Bytes readBytes(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    require(file.good(), "could not read test file: " + path.string());
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}
void writeBytes(const fs::path& path, const Bytes& bytes) {
    fs::create_directories(path.parent_path());
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    require(file.good(), "could not write test file: " + path.string());
}
Bytes hex(const char* value) {
    Bytes result;
    while (*value) {
        const auto nibble = [](char c) -> uint8_t {
            if (c >= '0' && c <= '9') return static_cast<uint8_t>(c - '0');
            if (c >= 'a' && c <= 'f') return static_cast<uint8_t>(c - 'a' + 10);
            throw std::runtime_error("bad hexadecimal fixture");
        };
        result.push_back(static_cast<uint8_t>((nibble(value[0]) << 4) | nibble(value[1])));
        value += 2;
    }
    return result;
}
void append(Bytes& target, const Bytes& value) { target.insert(target.end(), value.begin(), value.end()); }
void bigEndian(Bytes& bytes, uint64_t value, unsigned count) {
    for (unsigned i = count; i; --i) bytes.push_back(static_cast<uint8_t>(value >> ((i - 1) * 8)));
}
Bytes textValue(const std::string& value) {
    Bytes result;
    if (value.size() < 32) result.push_back(static_cast<uint8_t>(0xa0 | value.size()));
    else if (value.size() <= 255) { result.push_back(0xd9); result.push_back(static_cast<uint8_t>(value.size())); }
    else { result.push_back(0xda); bigEndian(result, value.size(), 2); }
    result.insert(result.end(), value.begin(), value.end());
    return result;
}
Bytes unsignedValue(uint64_t value) {
    if (value < 128) return {static_cast<uint8_t>(value)};
    Bytes result{0xcf};
    bigEndian(result, value, 8);
    return result;
}
Bytes binaryValue(const Bytes& value) {
    require(value.size() <= 255, "binary fixture too large");
    Bytes result{0xc4, static_cast<uint8_t>(value.size())};
    append(result, value);
    return result;
}
using Field = std::pair<std::string, Bytes>;
Bytes mapValue(const std::vector<Field>& fields) {
    require(fields.size() < 16, "map fixture too large");
    Bytes result{static_cast<uint8_t>(0x80 | fields.size())};
    for (const auto& field : fields) { append(result, textValue(field.first)); append(result, field.second); }
    return result;
}

// Original, non-executable test cartridge. SHA-1 and CRC32 below were computed
// independently with Python hashlib/zlib, not with the code under test.
Bytes cartridge() {
    Bytes data(32768);
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<uint8_t>(i * 17 + 3);
    std::fill(data.begin() + 0x100, data.begin() + 0x150, 0);
    const std::string title = "R2META TEST";
    std::copy(title.begin(), title.end(), data.begin() + 0x134);
    uint8_t checksum = 0;
    for (size_t i = 0x134; i <= 0x14c; ++i) checksum = static_cast<uint8_t>(checksum - data[i] - 1);
    data[0x14d] = checksum;
    return data;
}
const Bytes romSha1 = hex("6571d88cbd360cb379c67bb70f606fd61808ccb5");
const Bytes romCrc = hex("1d1395be");
const Bytes otherSha1(20, 0x42);
const char* pngSha1 = "c14c443dda1ee47820363cfa9432f107a1ae1357";
// An original, uniform white 8x8 RGB PNG (valid CRCs and zlib data).
const Bytes png{
    0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,0x49,0x48,0x44,0x52,
    0x00,0x00,0x00,0x08,0x00,0x00,0x00,0x08,0x08,0x02,0x00,0x00,0x00,0x4b,0x6d,0x29,0xdc,
    0x00,0x00,0x00,0x0f,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0xf8,0x8f,0x03,0x30,0x0c,0x2d,
    0x09,0x00,0xba,0x1e,0xbf,0x41,0x30,0x93,0x0a,0xfc,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,
    0x44,0xae,0x42,0x60,0x82
};
Bytes row(const std::string& title, const Bytes& sha = romSha1, const Bytes& crc = romCrc,
          uint64_t size = 32768) {
    std::vector<Field> fields{{"name", textValue(title)}, {"size", unsignedValue(size)}, {"region", textValue("USA")},
        {"releaseyear", unsignedValue(2026)}, {"developer", textValue("Original Test Studio")},
        {"publisher", textValue("R2 Test Publisher")}, {"genre", textValue("Test")},
        {"users", unsignedValue(1)}};
    if (!sha.empty()) fields.emplace_back("sha1", binaryValue(sha));
    if (!crc.empty()) fields.emplace_back("crc", binaryValue(crc));
    return mapValue(fields);
}
Bytes database(const std::vector<Bytes>& rows) {
    Bytes result{'R','A','R','C','H','D','B',0};
    result.resize(16);
    for (const auto& value : rows) append(result, value);
    result.push_back(0xc0);
    const uint64_t offset = result.size();
    for (unsigned i = 0; i < 8; ++i) result[8 + i] = static_cast<uint8_t>(offset >> ((7 - i) * 8));
    append(result, mapValue({{"count", unsignedValue(rows.size())}}));
    return result;
}

struct Transport {
    Bytes rdb = database({row("R2 Original (USA)")});
    Bytes image = png;
    std::map<std::string, Bytes> responses;
    std::vector<std::string> urls;
    size_t failAt = 0;
    size_t cancelAt = 0;
    std::atomic<bool>* cancellation = nullptr;
    bool offline = false;
} transport;
void network(const Bytes& rdb) { transport = Transport{}; transport.rdb = rdb; }
bool isDatabaseUrl(const std::string& url) { return url.size() >= 4 && url.compare(url.size() - 4, 4, ".rdb") == 0; }
size_t databaseRequests() {
    return static_cast<size_t>(std::count_if(transport.urls.begin(), transport.urls.end(), isDatabaseUrl));
}
fs::path metadataPath(const fs::path& root, const Game& game) {
    return root / "cache/library/gb" / (game.id + ".meta");
}
void noTemporaryFiles(const fs::path& root) {
    if (!fs::exists(root)) return;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        require(entry.path().filename().string().find(".tmp") == std::string::npos &&
                entry.path().filename().string().find(".catalog-") != 0,
                "temporary catalog file leaked: " + entry.path().string());
}
bool empty(const LibraryMetadata& value) {
    return value.title.empty() && value.region.empty() && value.developer.empty() && value.publisher.empty() &&
        value.genre.empty() && value.year.empty() && value.players.empty() && value.coverPath.empty() &&
        value.screenshotPath.empty() && value.titleScreenPath.empty() && value.match.empty();
}
bool equal(const LibraryMetadata& a, const LibraryMetadata& b) {
    return a.title == b.title && a.region == b.region && a.developer == b.developer &&
        a.publisher == b.publisher && a.genre == b.genre && a.year == b.year && a.players == b.players &&
        a.coverPath == b.coverPath && a.screenshotPath == b.screenshotPath &&
        a.titleScreenPath == b.titleScreenPath && a.match == b.match;
}
void replaceBytes(Bytes& bytes, const std::string& old, const std::string& value) {
    require(old.size() == value.size(), "replacement must preserve MessagePack string size");
    const auto found = std::search(bytes.begin(), bytes.end(), old.begin(), old.end());
    require(found != bytes.end(), "test replacement not found");
    std::copy(value.begin(), value.end(), found);
}
}

namespace r2n64 {
bool httpGet(const std::string& url, size_t maxBytes, const std::string& caFile,
             const std::atomic<bool>& cancel, std::vector<uint8_t>& body, std::string& error, const HttpOptions&) {
    body.clear();
    error.clear();
    transport.urls.push_back(url);
    require(caFile == "fixture-ca.pem", "CA bundle argument was lost");
    require(url.rfind("https://", 0) == 0, "catalog requested a non-HTTPS URL");
    require(url.find(' ') == std::string::npos && url.find('\n') == std::string::npos,
            "catalog requested an unescaped URL");
    if (transport.cancelAt == transport.urls.size() && transport.cancellation)
        transport.cancellation->store(true);
    if (cancel.load() || transport.offline || transport.failAt == transport.urls.size()) {
        error = cancel.load() ? "Cancelled by fixture" : "Network unavailable in fixture";
        return false;
    }
    const auto response = transport.responses.find(url);
    const Bytes& value = response != transport.responses.end() ? response->second :
                         (isDatabaseUrl(url) ? transport.rdb : transport.image);
    if (value.size() > maxBytes) { error = "Fixture response exceeds maximum"; return false; }
    body = value;
    return true;
}
void httpShutdown() {}
}

int main() {
    char pattern[] = "/tmp/r2n64-library-XXXXXX";
    const char* directory = ::mkdtemp(pattern);
    if (!directory) return 1;
    const fs::path temporary(directory);
    try {
        const fs::path rom = temporary / "original.gb";
        writeBytes(rom, cartridge());
        Game game;
        std::string error;
        require(readRom(rom.string(), game, error), "original GB fixture failed validation: " + error);
        require(game.system == SystemType::GameBoy && game.size == 32768, "wrong fixture identity");
        std::atomic<bool> cancel{false};
        LibraryMetadata metadata;

        const fs::path normal = temporary / "normal";
        metadata.title = "stale output";
        error = "stale error";
        network({});
        require(!loadCachedMetadata(normal.string(), game, metadata, error) && empty(metadata) && error.empty(),
                "missing cache must clear output without reporting corruption");
        require(transport.urls.empty() && !fs::exists(normal), "offline cache lookup touched network or disk");

        const std::string originalId = game.id;
        const std::string originalTitle = game.title;
        const std::string originalPath = game.path;
        const Bytes originalRom = readBytes(rom);
        network(database({row("Weaker CRC match", {}, romCrc),
                          row("R2 Original (USA)", romSha1, hex("ffffffff"))}));
        require(fetchLibraryMetadata(normal.string(), "fixture-ca.pem", game, cancel, metadata, error),
                "SHA-1 download failed: " + error);
        require(error.empty() && metadata.title == "R2 Original (USA)" &&
                metadata.match == "SHA-1: contenido completo", "SHA-1 did not take precedence over CRC32");
        require(metadata.region == "USA" && metadata.developer == "Original Test Studio" &&
                metadata.publisher == "R2 Test Publisher" && metadata.genre == "Test" &&
                metadata.year == "2026" && metadata.players == "1", "RDB fields did not survive lookup");
        require(transport.urls.size() == 4 && databaseRequests() == 1, "unexpected download count");
        require(transport.urls.front() == "https://raw.githubusercontent.com/libretro/libretro-database/master/rdb/"
                "Nintendo%20-%20Game%20Boy.rdb", "database URL is not the official encoded path");
        const std::array<std::string, 3> folders{{"Named_Boxarts", "Named_Snaps", "Named_Titles"}};
        const std::array<std::string, 3> local{{"boxart", "snap", "title"}};
        const std::array<std::string, 3> paths{{metadata.coverPath, metadata.screenshotPath, metadata.titleScreenPath}};
        for (size_t i = 0; i < paths.size(); ++i) {
            require(transport.urls[i + 1] == "https://raw.githubusercontent.com/libretro-thumbnails/"
                    "Nintendo_-_Game_Boy/master/" + folders[i] + "/R2%20Original%20%28USA%29.png",
                    "wrong official thumbnail URL");
            const fs::path expected = normal / "covers/gb" / game.id /
                                      (local[i] + "-" + pngSha1 + ".png");
            require(fs::path(paths[i]) == expected && readBytes(expected) == png,
                    "content-addressed PNG path/hash is incorrect");
        }
        require(fs::exists(normal / "cache/libretro/gb.rdb") && fs::exists(metadataPath(normal, game)),
                "successful fetch did not commit database and metadata");
        require(game.id == originalId && game.title == originalTitle && game.path == originalPath &&
                readBytes(rom) == originalRom, "catalog modified the Game identity or cartridge");
        noTemporaryFiles(normal);
        const LibraryMetadata baseline = metadata;
        const Bytes manifest = readBytes(metadataPath(normal, game));
        const Bytes cachedDatabase = readBytes(normal / "cache/libretro/gb.rdb");

        network({});
        transport.offline = true;
        require(loadCachedMetadata(normal.string(), game, metadata, error) && equal(metadata, baseline) && error.empty(),
                "offline metadata round trip failed: " + error);
        require(transport.urls.empty(), "cache lookup unexpectedly used HTTP");
        require(fetchLibraryMetadata(normal.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                equal(metadata, baseline) && !error.empty(), "optional offline images erased valid cached images");
        require(databaseRequests() == 0 && transport.urls.size() == 3,
                "matching cached database was unnecessarily downloaded");
        require(readBytes(metadataPath(normal, game)) == manifest &&
                readBytes(normal / "cache/libretro/gb.rdb") == cachedDatabase,
                "offline refresh changed a valid cache");
        cancel = true;
        metadata = baseline;
        require(!loadCachedMetadata(normal.string(), game, metadata, error, &cancel) &&
                empty(metadata) && !error.empty(), "cancelled cache identification continued hashing");
        cancel = false;

        const auto success = [&](const std::string& name, const Bytes& rdb, const std::string& title,
                                 const std::string& match) {
            network(rdb);
            const auto root = temporary / name;
            require(fetchLibraryMetadata(root.string(), "fixture-ca.pem", game, cancel, metadata, error),
                    name + " fetch failed: " + error);
            require(metadata.title == title && metadata.match == match, name + " matched wrong record");
            noTemporaryFiles(root);
        };
        success("crc-only", database({row("CRC original", {}, romCrc)}), "CRC original",
                "CRC32 unico: contenido completo");
        success("duplicates", database({row("Same title"), row("Same title")}), "Same title",
                "SHA-1: contenido completo");
        success("sha-after-crc-ambiguity", database({row("CRC candidate 1", {}, romCrc),
                row("CRC candidate 2", {}, romCrc), row("Exact SHA title")}), "Exact SHA title",
                "SHA-1: contenido completo");
        success("without-size", database({mapValue({{"name", textValue("SHA without size")},
                {"sha1", binaryValue(romSha1)}})}), "SHA without size", "SHA-1: contenido completo");
        // Real Libretro RDBs repeat the ignored serial field as text and binary.
        // Identity and metadata keys remain unique; this exception is RDB-only.
        success("duplicate-serial", database({mapValue({{"name", textValue("Serial formats")},
                {"sha1", binaryValue(romSha1)}, {"serial", textValue("R2-TEST")},
                {"serial", binaryValue(Bytes{'R','2','-','T','E','S','T'})}})}), "Serial formats",
                "SHA-1: contenido completo");

        const auto rejected = [&](const std::string& name, const Bytes& rdb) {
            network(rdb);
            const auto root = temporary / name;
            metadata = baseline;
            error = "stale error";
            require(!fetchLibraryMetadata(root.string(), "fixture-ca.pem", game, cancel, metadata, error),
                    name + " unexpectedly matched");
            require(empty(metadata) && !error.empty(), name + " did not clear output or report failure");
            require(transport.urls.size() == 1 && databaseRequests() == 1,
                    name + " requested images without an unambiguous identity");
            require(!fs::exists(metadataPath(root, game)) && !fs::exists(root / "cache/libretro/gb.rdb"),
                    name + " committed rejected database or metadata");
            noTemporaryFiles(root);
        };
        rejected("no-approximate-title", database({row(game.title, otherSha1, hex("00000000"))}));
        rejected("sha-conflict-no-crc", database({row("Wrong SHA despite same CRC", otherSha1, romCrc)}));
        rejected("sha-malformed-no-crc", database({row("Short SHA", Bytes{1,2}, romCrc)}));
        rejected("crc-endian", database({row("Reversed CRC", {}, hex("be95131d"))}));
        rejected("sha-ambiguous", database({row("First title"), row("Second title")}));
        rejected("crc-ambiguous", database({row("First title", {}, romCrc), row("Second title", {}, romCrc)}));
        rejected("size-conflict", database({row("Wrong size", romSha1, romCrc, 32767)}));
        rejected("invalid-title-control", database({row("Invalid\nTitle")}));
        rejected("invalid-title-utf8", database({row(std::string("Bad ") + char(0xc0) + char(0xaf))}));
        rejected("invalid-magic", Bytes(64, 0));
        const Bytes validDatabase = database({row("R2 Original (USA)")});
        for (const auto length : {size_t(0), size_t(7), size_t(15), size_t(17),
                                  validDatabase.size() / 2, validDatabase.size() - 1}) {
            rejected("truncated-" + std::to_string(length), Bytes(validDatabase.begin(), validDatabase.begin() + length));
        }
        Bytes badCount = validDatabase;
        badCount.back() = 2;
        rejected("bad-count", badCount);
        Bytes badOffset = validDatabase;
        std::fill(badOffset.begin() + 8, badOffset.begin() + 16, 0xff);
        rejected("bad-offset", badOffset);
        rejected("duplicate-map-key", database({mapValue({{"name", textValue("One")},
            {"name", textValue("Two")}, {"sha1", binaryValue(romSha1)}})}));
        rejected("overflow-string-length", database({mapValue({{"name", Bytes{0xdb,0xff,0xff,0xff,0xff}},
            {"sha1", binaryValue(romSha1)}})}));
        Bytes nested(12, 0x91); // Twelve nested one-element arrays, then nil.
        nested.push_back(0xc0);
        rejected("excessive-nesting", database({mapValue({{"name", textValue("Deep")},
            {"sha1", binaryValue(romSha1)}, {"extra", nested}})}));

        for (unsigned ambiguity = 0; ambiguity < 2; ++ambiguity) {
            const auto root = temporary / (ambiguity ? "cached-ambiguous" : "cached-no-match");
            const Bytes rdb = ambiguity ? database({row("One"), row("Two")}) :
                                         database({row("Different content", otherSha1, hex("00000000"))});
            writeBytes(root / "cache/libretro/gb.rdb", rdb);
            network(validDatabase);
            require(!fetchLibraryMetadata(root.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                    empty(metadata) && !error.empty() && transport.urls.empty(),
                    "valid cached non-match/ambiguity unnecessarily downloaded database");
            require(readBytes(root / "cache/libretro/gb.rdb") == rdb && !fs::exists(metadataPath(root, game)),
                    "valid cached non-match was changed or assigned metadata");
        }
        const auto refresh = temporary / "corrupt-database-refresh";
        writeBytes(refresh / "cache/libretro/gb.rdb", Bytes{'b','a','d'});
        network(validDatabase);
        require(fetchLibraryMetadata(refresh.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                databaseRequests() == 1 && readBytes(refresh / "cache/libretro/gb.rdb") == validDatabase,
                "corrupt cached database was not refreshed");

        network(database({row("R2 & Test: A/B? (USA)")}));
        const auto escaped = temporary / "escaped";
        require(fetchLibraryMetadata(escaped.string(), "fixture-ca.pem", game, cancel, metadata, error),
                "escaped thumbnail fetch failed: " + error);
        for (size_t i = 1; i < transport.urls.size(); ++i)
            require(transport.urls[i].find("/R2%20_%20Test_%20A_B_%20%28USA%29.png") != std::string::npos,
                    "thumbnail name was not sanitized before URL encoding");

        network(validDatabase);
        cancel = true;
        metadata = baseline;
        const auto preCancelled = temporary / "pre-cancelled";
        require(!fetchLibraryMetadata(preCancelled.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && !error.empty() && transport.urls.empty() && !fs::exists(preCancelled),
                "pre-cancelled request performed work or retained output");
        cancel = false;
        network(validDatabase);
        transport.cancelAt = 3;
        transport.cancellation = &cancel;
        const auto midCancelled = temporary / "mid-cancelled";
        require(!fetchLibraryMetadata(midCancelled.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && cancel.load(), "mid-download cancellation did not stop the fetch");
        require(!fs::exists(metadataPath(midCancelled, game)) && !fs::exists(midCancelled / "cache/libretro/gb.rdb"),
                "cancelled download committed partial metadata/database");
        noTemporaryFiles(midCancelled);
        cancel = false;
        network(validDatabase);
        transport.cancelAt = 2; // Existing RDB: cancel while downloading the second image.
        transport.cancellation = &cancel;
        require(!fetchLibraryMetadata(normal.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && cancel.load(), "existing-cache refresh ignored cancellation");
        require(readBytes(metadataPath(normal, game)) == manifest &&
                readBytes(normal / "cache/libretro/gb.rdb") == cachedDatabase,
                "cancelled refresh replaced previously committed metadata/database");
        for (const auto& path : paths) require(readBytes(path) == png, "cancelled refresh changed an existing image");
        cancel = false;

        network(validDatabase);
        transport.failAt = 1;
        const auto offline = temporary / "network-failure";
        require(!fetchLibraryMetadata(offline.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && !error.empty() && !fs::exists(metadataPath(offline, game)),
                "failed database download committed metadata");
        network(validDatabase);
        transport.failAt = 3;
        const auto partial = temporary / "partial-images";
        require(fetchLibraryMetadata(partial.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                !error.empty() && !metadata.coverPath.empty() && metadata.screenshotPath.empty() &&
                !metadata.titleScreenPath.empty(), "optional image failure incorrectly rejected title or invented path");
        LibraryMetadata partialCache;
        require(loadCachedMetadata(partial.string(), game, partialCache, error) && equal(partialCache, metadata),
                "partial metadata did not survive offline round trip");
        network(validDatabase);
        transport.image = png;
        transport.image[48] ^= 1; // Damage IDAT without updating its CRC.
        const auto badImage = temporary / "bad-images";
        require(fetchLibraryMetadata(badImage.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                !error.empty() && metadata.coverPath.empty() && metadata.screenshotPath.empty() &&
                metadata.titleScreenPath.empty(), "corrupt PNG was accepted into metadata");
        writeBytes(baseline.coverPath, Bytes{'b','a','d'});
        network({});
        require(loadCachedMetadata(normal.string(), game, metadata, error) && metadata.title == baseline.title &&
                metadata.coverPath.empty() && metadata.screenshotPath == baseline.screenshotPath &&
                metadata.titleScreenPath == baseline.titleScreenPath && transport.urls.empty(),
                "corrupt optional cached PNG either invalidated title or remained exposed");
        writeBytes(baseline.coverPath, png);

        // ROM identity is rechecked even when a caller reuses an old Game record.
        Bytes alteredRom = originalRom;
        alteredRom.back() ^= 1;
        writeBytes(rom, alteredRom);
        metadata = baseline;
        network({});
        require(!loadCachedMetadata(normal.string(), game, metadata, error) && empty(metadata) && !error.empty() &&
                transport.urls.empty(), "stale metadata was accepted for changed ROM content");
        require(readBytes(metadataPath(normal, game)) == manifest, "stale-cache lookup modified original metadata");
        writeBytes(rom, originalRom);

        // Cache manifests are bounded, typed, and cannot reference arbitrary files.
        const auto corruptCache = temporary / "corrupt-cache";
        writeBytes(metadataPath(corruptCache, game), Bytes{'R','2'});
        metadata = baseline;
        require(!loadCachedMetadata(corruptCache.string(), game, metadata, error) && empty(metadata) && !error.empty(),
                "truncated manifest was accepted");
        Bytes pathEscape = manifest;
        const std::string imageName = std::string("boxart-") + pngSha1 + ".png";
        replaceBytes(pathEscape, imageName, "../" + std::string(imageName.size() - 3, 'x'));
        writeBytes(metadataPath(corruptCache, game), pathEscape);
        require(!loadCachedMetadata(corruptCache.string(), game, metadata, error) && empty(metadata),
                "manifest path traversal was accepted");
        writeBytes(metadataPath(corruptCache, game), Bytes(16385, 'x'));
        require(!loadCachedMetadata(corruptCache.string(), game, metadata, error) && empty(metadata),
                "oversized manifest was accepted");
        Bytes duplicatedSerial(manifest.begin(), manifest.begin() + 8);
        append(duplicatedSerial, Bytes{0xde, 0, 16}); // Original fourteen fields plus two serial fields.
        duplicatedSerial.insert(duplicatedSerial.end(), manifest.begin() + 9, manifest.end());
        for (unsigned i = 0; i < 2; ++i) {
            append(duplicatedSerial, textValue("serial"));
            append(duplicatedSerial, textValue("R2-TEST"));
        }
        writeBytes(metadataPath(corruptCache, game), duplicatedSerial);
        require(!loadCachedMetadata(corruptCache.string(), game, metadata, error) && empty(metadata),
                "RDB-only duplicate serial exception leaked into cache manifests");

        const auto outside = temporary / "outside";
        fs::create_directory(outside);
        const Bytes sentinel{'u','n','c','h','a','n','g','e','d'};
        writeBytes(outside / "sentinel", sentinel);
        const auto linkedRoot = temporary / "linked-root";
        fs::create_directory_symlink(outside, linkedRoot);
        network(validDatabase);
        require(!fetchLibraryMetadata(linkedRoot.string(), "fixture-ca.pem", game, cancel, metadata, error),
                "symlinked cache root accepted writes");
        require(!fs::exists(outside / "covers") && !fs::exists(outside / "cache") &&
                readBytes(outside / "sentinel") == sentinel, "symlinked root escaped confinement");

        const auto linkedManifest = temporary / "linked-manifest";
        fs::create_directories(metadataPath(linkedManifest, game).parent_path());
        fs::create_symlink(outside / "sentinel", metadataPath(linkedManifest, game));
        network(validDatabase);
        require(!loadCachedMetadata(linkedManifest.string(), game, metadata, error) && empty(metadata),
                "symlinked manifest was read");
        require(!fetchLibraryMetadata(linkedManifest.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && readBytes(outside / "sentinel") == sentinel &&
                fs::is_symlink(metadataPath(linkedManifest, game)), "symlinked manifest was replaced or followed");
        noTemporaryFiles(linkedManifest);

        const auto hardlink = temporary / "hardlink";
        fs::create_directories(metadataPath(hardlink, game).parent_path());
        fs::create_hard_link(outside / "sentinel", metadataPath(hardlink, game));
        network(validDatabase);
        require(!fetchLibraryMetadata(hardlink.string(), "fixture-ca.pem", game, cancel, metadata, error) &&
                empty(metadata) && readBytes(outside / "sentinel") == sentinel,
                "multiply linked manifest was overwritten");

        for (const std::string& id : {std::string("../escape"), std::string("bad/name"), std::string(81, 'x')}) {
            Game malicious = game;
            malicious.id = id;
            network(validDatabase);
            require(!fetchLibraryMetadata(normal.string(), "fixture-ca.pem", malicious, cancel, metadata, error) &&
                    empty(metadata) && transport.urls.empty(), "unsafe Game identifier reached network or cache");
        }
        Game linkedRom = game;
        linkedRom.path = (temporary / "linked.gb").string();
        fs::create_symlink(rom, linkedRom.path);
        network(validDatabase);
        require(!fetchLibraryMetadata(normal.string(), "fixture-ca.pem", linkedRom, cancel, metadata, error) &&
                empty(metadata) && transport.urls.empty(), "symbolic ROM was followed during identification");
        Game unsupported = game;
        unsupported.system = SystemType::Unknown;
        require(!fetchLibraryMetadata(normal.string(), "fixture-ca.pem", unsupported, cancel, metadata, error) &&
                empty(metadata) && transport.urls.empty(), "unsupported system reached the network");

        // These byte patterns are original identification fixtures, never run by
        // an emulator. Their SHA-1/CRC constants came from Python hashlib/zlib.
        // A manual Game isolates canonicalization from the scanner's heuristics.
        const auto normalized = [&](const std::string& name, SystemType system, const Bytes& contents,
                                    const Bytes& rdb, const std::string& title, const std::string& view) {
            const auto root = temporary / "normalized" / name;
            const auto cartridgePath = temporary / (name + ".rom");
            writeBytes(cartridgePath, contents);
            Game fixture;
            fixture.path = cartridgePath.string();
            fixture.id = "original-normalization-fixture";
            fixture.title = "Untrusted filename title";
            fixture.size = contents.size();
            fixture.system = system;
            network(rdb);
            const bool found = fetchLibraryMetadata(root.string(), "fixture-ca.pem", fixture, cancel, metadata, error);
            const auto cache = root / "cache/library" / systemId(system) / (fixture.id + ".meta");
            if (title.empty()) {
                require(!found && empty(metadata) && !error.empty() && !fs::exists(cache) &&
                        transport.urls.size() == 1, name + " should reject conflicting identities");
            } else {
                require(found && metadata.title == title && metadata.match == "SHA-1: " + view,
                        name + " normalized identity mismatch: " + error);
                LibraryMetadata restored;
                transport.urls.clear();
                transport.offline = true;
                require(loadCachedMetadata(root.string(), fixture, restored, error) && equal(restored, metadata) &&
                        transport.urls.empty(), name + " offline cache round trip failed");
            }
            require(readBytes(cartridgePath) == contents, name + " changed cartridge content");
            noTemporaryFiles(root);
        };

        Bytes n64(65536);
        for (size_t i = 0; i < n64.size(); ++i) n64[i] = static_cast<uint8_t>(i * 13 + 7);
        const Bytes n64Magic = hex("80371240");
        std::copy(n64Magic.begin(), n64Magic.end(), n64.begin());
        const Bytes n64Rdb = database({row("N64 canonical", hex("8a01daa85aabf784634c1dedb669d2e2f5ca523f"),
                                         hex("01e79536"), n64.size())});
        normalized("n64-z64", SystemType::Nintendo64, n64, n64Rdb, "N64 canonical", "contenido completo");
        Bytes v64 = n64;
        for (size_t i = 0; i < v64.size(); i += 2) std::swap(v64[i], v64[i + 1]);
        normalized("n64-v64", SystemType::Nintendo64, v64, n64Rdb, "N64 canonical", "N64 big-endian");
        Bytes littleN64 = n64;
        for (size_t i = 0; i < littleN64.size(); i += 4) std::reverse(littleN64.begin() + i, littleN64.begin() + i + 4);
        normalized("n64-little", SystemType::Nintendo64, littleN64, n64Rdb, "N64 canonical", "N64 big-endian");

        Bytes snes(65536);
        for (size_t i = 0; i < snes.size(); ++i) snes[i] = static_cast<uint8_t>(i * 29 + 11);
        const Bytes snesRdb = database({row("SNES canonical", hex("1e3e6759420a87139165053f169ba8de69840ce0"),
                                          hex("0f74111a"), snes.size())});
        normalized("snes-no-copier", SystemType::SuperNintendo, snes, snesRdb, "SNES canonical", "contenido completo");
        Bytes withCopier(512, 0x5a);
        append(withCopier, snes);
        normalized("snes-copier", SystemType::SuperNintendo, withCopier, snesRdb, "SNES canonical", "sin copier SNES");

        Bytes nesPayload(24576);
        for (size_t i = 0; i < nesPayload.size(); ++i) nesPayload[i] = static_cast<uint8_t>(i * 7 + 9);
        Bytes nes(16, 0);
        const Bytes nesHeader = hex("4e45531a010104");
        std::copy(nesHeader.begin(), nesHeader.end(), nes.begin());
        nes.resize(16 + 512, 0x33);
        append(nes, nesPayload);
        const Bytes nesFullRow = row("NES full", hex("4a62eec45e3ee76909a4db9419accca6e06849c7"),
                                    hex("f5ceb44a"), nes.size());
        const Bytes nesPayloadRow = row("NES payload", hex("e393dae81daef3ac91751c9be890ee9d2394df43"),
                                       hex("2d6ea521"), nesPayload.size());
        normalized("nes-full-trainer", SystemType::NintendoEntertainmentSystem, nes, database({nesFullRow}),
                   "NES full", "contenido completo");
        normalized("nes-payload-trainer", SystemType::NintendoEntertainmentSystem, nes, database({nesPayloadRow}),
                   "NES payload", "sin cabecera/trainer NES");
        normalized("nes-view-ambiguity", SystemType::NintendoEntertainmentSystem, nes,
                   database({nesFullRow, nesPayloadRow}), "", "");
        nes.erase(nes.begin() + 16, nes.begin() + 16 + 512);
        nes[6] = 0;
        normalized("nes-payload-no-trainer", SystemType::NintendoEntertainmentSystem, nes, database({nesPayloadRow}),
                   "NES payload", "sin cabecera/trainer NES");
        normalized("nes-full-no-trainer", SystemType::NintendoEntertainmentSystem, nes,
                   database({row("NES exact", hex("bc916ecf8b0757e50e0515ab2384f14f0974f0c4"),
                                 hex("a6f23b3b"), nes.size())}), "NES exact", "contenido completo");

        require(readBytes(rom) == originalRom && readBytes(metadataPath(normal, game)) == manifest,
                "negative tests damaged the ROM or previously valid cache");
        noTemporaryFiles(temporary);
        fs::remove_all(temporary);
        std::cout << "Library metadata tests PASS\n";
        return 0;
    } catch (const std::exception& failure) {
        std::cerr << "Library metadata tests FAIL: " << failure.what() << "\nArtifacts: " << temporary << '\n';
        return 1;
    }
}
