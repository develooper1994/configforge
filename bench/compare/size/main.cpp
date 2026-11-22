// The program the size rows measure: parse one fixed document of 64 bytes or
// fewer in the adapter's format, and print what was touched.
#include "touch.h"

#include <cstdio>
#include <string>
#include <vector>

std::vector<const Adapter*>& cfb_registry();

int main() {
    struct Doc {
        const char* format;
        std::string doc;
    };
    const Doc docs[] = {{"json", "{\"a\":1,\"b\":\"xy\",\"c\":[1,2.5,true,null]}"},
                        {"yaml", "a: 1\nb: xy\nc: [1, 2.5, true]\n"},
                        {"toml", "a = 1\nb = \"xy\"\nc = [1, 2.5]\n"},
                        {"cbor", std::string("\xa2\x61\x61\x01\x61\x62\x62xy", 9)},
                        {"ini", "[s]\na = 1\nb = xy\n"},
                        {"xml", "<r><a>1</a><b>xy</b><c>1</c></r>"}};
    Touch     t;
    for (const Adapter* a : cfb_registry())
        for (const Doc& d : docs)
            if (std::string(d.format) == a->format) {
                std::string s = d.doc;
                s.append(64, '\0');
                a->parse(s.data(), d.doc.size(), true, t);
            }
    std::printf("%llu %llu\n", (unsigned long long)t.strings,
                (unsigned long long)t.acc);
    return 0;
}
