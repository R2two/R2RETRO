#pragma once
#include <string>
#include <sys/types.h>

namespace r2n64::fileops {
// Names must be single components (no '.', '..', slash or backslash). Desktop
// uses native *at calls. PS4, and R2N64_PATH_FILEOPS tests, use absolute paths:
// all parent components must be real directories and match the supplied fd.
// Path opens additionally verify parent/child identity before returning a fd.
// These checks detect substitutions but cannot make a path syscall atomic with
// directory validation: this backend is not TOCTOU-equivalent to native *at.
// Do not move/replace application directories concurrently with these calls.
// There is no cwd change or shared state. Failures return -1 and retain errno.
int openAt(int parentFd, const std::string& parentPath, const char* name,
           int flags, mode_t mode = 0);
int mkdirAt(int parentFd, const std::string& parentPath, const char* name, mode_t mode);
// Path backend unlink/rename are for regular, singly-linked cache/settings files.
// Rename remains one atomic filesystem operation; no copy/delete fallback.
int unlinkAt(int parentFd, const std::string& parentPath, const char* name);
int renameAt(int sourceFd, const std::string& sourceParent, const char* sourceName,
             int destinationFd, const std::string& destinationParent, const char* destinationName);
}
