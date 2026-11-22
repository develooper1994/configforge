// registry.cpp - the adapters that are linked in, in the order they register.
#include "touch.h"

#include <vector>

std::vector<const Adapter*>& cfb_registry() {
    static std::vector<const Adapter*> r;
    return r;
}

Registrar::Registrar(const Adapter* a) {
    cfb_registry().push_back(a);
}
