#pragma once
#include <functional>
// Authored loopback peer shared by headless and original Lua login contracts.
// 0=realms, 1=bad SRP server proof, 2=server version rejection, 3=malformed realms,
// 4=unsupported token challenge, 5=actual realm UI/world proof and rejection.
// Never contacts an external endpoint.
void CheckNativeTestServerExchange(void (*pump)(),
    const std::function<void(const char*)>& begin,
    const std::function<bool()>& finished, int scenario);
