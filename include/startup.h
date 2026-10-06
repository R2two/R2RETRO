#pragma once

namespace r2n64 {
// Main-thread boot diagnostics, available before SDL and the normal logger.
void startupBegin();
void startupLog(const char* stage, const char* detail = nullptr);
void startupDataReady();
int startupFailure(const char* stage, const char* detail);
int finishApplication(int result);
}
