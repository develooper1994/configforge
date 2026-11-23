#include "configforge/cf_version.h"
#include "configforge/cf_build_options.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
    static const struct {
        const char* name;
        int         enabled;
    } features[] = {{"ini", CF_WITH_INI},         {"json", CF_WITH_JSON},
                    {"toml", CF_WITH_TOML},       {"properties", CF_WITH_PROPERTIES},
                    {"cbor", CF_WITH_CBOR},       {"tlv", CF_WITH_TLV},
                    {"compose", CF_WITH_COMPOSE}, {"xml", CF_WITH_XML},
                    {"yaml", CF_WITH_YAML},       {"sqlite", CF_WITH_SQLITE},
                    {"real", CF_WITH_REAL},       {"dir", CF_WITH_DIR},
                    {"dotenv", CF_WITH_DOTENV},   {"preferences", CF_WITH_PREFERENCES},
                    {"emit", CF_WITH_EMIT},       {"image", CF_WITH_IMAGE},
                    {"report", CF_WITH_REPORT}};
    static const struct {
        const char* name;
        size_t      limit;
    } limits[] = {{"key", CF_KEY_MAX},
                  {"message", CF_MESSAGE_MAX},
                  {"path", CF_PATH_MAX},
                  {"depth", CF_MAX_DEPTH}};
    const char* profile;
    size_t      i;

    if (argc != 2) {
        fprintf(stderr, "expected a build profile name\n");
        return 1;
    }
    profile = cf_build_profile();
    if (profile == NULL || strcmp(profile, argv[1]) != 0) {
        fprintf(stderr, "expected profile %s, got %s\n", argv[1],
                profile != NULL ? profile : "(null)");
        return 1;
    }
    for (i = 0; i < sizeof features / sizeof features[0]; ++i) {
        if ((cf_has_feature(features[i].name) != CF_FALSE) !=
            (features[i].enabled == 1)) {
            fprintf(stderr, "feature %s differs from the build option\n",
                    features[i].name);
            return 1;
        }
    }
    for (i = 0; i < sizeof limits / sizeof limits[0]; ++i) {
        if (cf_build_limit(limits[i].name) != limits[i].limit) {
            fprintf(stderr, "limit %s differs from the build option\n", limits[i].name);
            return 1;
        }
    }
#ifdef CF_SINGLE_THREADED
    if (!cf_has_feature("single-threaded")) {
        fprintf(stderr, "single-threaded is missing from the feature list\n");
        return 1;
    }
#endif
    printf("profile %s: %lu features, key %lu\n", profile,
           (unsigned long)cf_feature_count(), (unsigned long)cf_build_limit("key"));
    return 0;
}
