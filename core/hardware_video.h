#pragma once
#include <cstdint>
#include <string>

namespace r2n64 {
// Platform-neutral contract. Owned by the frontend and borrowed for the whole
// core session; all calls execute on the thread that owns the GL context.
class HardwareVideoHost {
public:
    using Procedure = void (*)();
    virtual ~HardwareVideoHost() = default;
    virtual bool prepare(unsigned width, unsigned height, bool depth, bool stencil,
                         std::string& error) = 0;
    // Flush frontend work and preserve/restore the GL state around core calls.
    virtual bool begin(std::string& error) = 0;
    virtual bool end(std::string& error) = 0;
    virtual uintptr_t framebuffer() const = 0;
    virtual Procedure procedure(const char* name) = 0;
};
enum class N64Graphics { Software, Gles2 };
}
