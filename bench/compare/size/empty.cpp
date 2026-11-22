// The adapter that does nothing: the program every library's size is measured
// against.
#include "touch.h"

namespace {
bool nothing(const char*, size_t, bool, Touch&) {
    return true;
}
const Adapter a = {"empty", "empty", "json", "-", nothing};
Registrar     r(&a);
} // namespace
