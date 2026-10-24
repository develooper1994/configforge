/*
 * plugins.c - settings the program cannot know in advance.
 *
 * A contract makes a typo an error instead of a setting that silently
 * does nothing. But a gateway with plugins cannot list every plugin's
 * settings, because the plugins are not its code: tomorrow someone adds
 * one. The answer is to keep the contract strict everywhere except one
 * prefix the program hands over, and to discover what is under it:
 *
 *     cf_builder_open(b, "plugins")       this prefix is not ours to check
 *     cf_iter_children(..., "plugins")    which plugins are configured
 *     cf_iter_keys(..., "plugins.auth")   everything one of them was given
 *     cf_array_count / cf_array_key       a list, element by element
 *
 *     ./plugins
 *
 * The suite checks its output.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <string.h>

#define FILE_NAME "plugins.toml"

static const char k_config[] = "[gateway]\n"
                               "listen = \"0.0.0.0:8443\"\n"
                               "\n"
                               "[plugins.auth]\n"
                               "enabled = true\n"
                               "issuer = \"https://id.example.com\"\n"
                               "audiences = [\"web\", \"mobile\"]\n"
                               "\n"
                               "[plugins.ratelimit]\n"
                               "enabled = true\n"
                               "per_minute = 600\n"
                               "\n"
                               "[plugins.cache]\n"
                               "enabled = false\n";

/* "plugins.<name>.<leaf>", or "" if it does not fit. */
static const char* key_of(char* buf, size_t cap, const char* name, size_t namelen,
                          const char* leaf) {
    size_t need = 8 + namelen + 1 + strlen(leaf) + 1;
    if (need > cap) {
        buf[0] = '\0';
        return buf;
    }
    memcpy(buf, "plugins.", 8);
    memcpy(buf + 8, name, namelen);
    buf[8 + namelen] = '.';
    strcpy(buf + 8 + namelen + 1, leaf);
    return buf;
}

/* One value, whatever its type: a plugin's settings have no contract,
   so the file's own types (TOML has them) are what there is. */
static void print_value(const cf_config* cfg, const char* key) {
    cf_type t = cf_type_of(cfg, key);
    if (t == CF_TYPE_BOOL) {
        printf("%s", cf_get_bool(cfg, key, CF_FALSE) ? "true" : "false");
    } else if (t == CF_TYPE_INT) {
        printf("%ld", (long)cf_get_int(cfg, key, 0));
    } else if (t == CF_TYPE_TEXT) {
        printf("\"%s\"", cf_get_text(cfg, key, ""));
    } else {
        printf("(%s)", cf_type_name(t));
    }
}

int main(void) {
    cf_error    err = CF_ERROR_INIT;
    cf_builder* b   = cf_build(NULL);
    cf_config*  cfg;
    cf_iter     plugin;
    char        key[128];
    FILE*       f = fopen(FILE_NAME, "wb");

    if (f == NULL) {
        return 1;
    }
    fputs(k_config, f);
    fclose(f);

    cf_builder_file(b, "file", CF_RANK_USER_FILE, FILE_NAME, NULL, 0);
    /* The program's own settings are a contract, strictly checked... */
    cf_builder_text(b, "gateway.listen", "0.0.0.0:8080", 0);
    cf_builder_unknown(b, CF_UNKNOWN_REJECT);
    /* ...and one prefix is handed over: anything under plugins is
       accepted, because the plugin, not the gateway, knows what is
       right there. "gateway.lisen" would still be refused. */
    cf_builder_open(b, "plugins");
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        fprintf(stderr, "plugins: %s\n", err.message);
        return 1;
    }

    printf("gateway listening on %s\n", cf_get_text(cfg, "gateway.listen", "?"));

    /* Which plugins are configured: each immediate child of "plugins",
       once, in order - discovered, not listed in the program. */
    cf_iter_children(&plugin, cfg, "plugins");
    while (cf_iter_next(&plugin)) {
        cf_iter setting;
        char    prefix[96];
        int     on;

        on = (int)cf_get_bool(
            cfg, key_of(key, sizeof key, plugin.name, plugin.namelen, "enabled"),
            CF_FALSE);
        printf("plugin %.*s: %s\n", (int)plugin.namelen, plugin.name,
               on ? "on" : "off");
        if (!on) {
            continue;
        }

        /* Everything this plugin was given, to hand to it. A list is a
           key with an index per element, and its recorded length - not
           the number of index keys, which a shorter list in a later
           source would leave behind. */
        key_of(prefix, sizeof prefix, plugin.name, plugin.namelen, "");
        prefix[strlen(prefix) - 1] = '\0';
        cf_iter_children(&setting, cfg, prefix);
        while (cf_iter_next(&setting)) {
            size_t count;
            key_of(key, sizeof key, plugin.name, plugin.namelen, "");
            if (strlen(key) + setting.namelen + 1 > sizeof key) {
                continue;
            }
            strncat(key, setting.name, setting.namelen);
            count = cf_array_count(cfg, key);
            printf("  %s = ", key);
            if (count > 0) {
                size_t i;
                printf("[");
                for (i = 0; i < count; ++i) {
                    char element[160];
                    cf_array_key(element, sizeof element, key, i);
                    printf("%s", i > 0 ? ", " : "");
                    print_value(cfg, element);
                }
                printf("]");
            } else {
                print_value(cfg, key);
            }
            printf("\n");
        }
    }

    cf_config_free(cfg);
    remove(FILE_NAME);
    return 0;
}
