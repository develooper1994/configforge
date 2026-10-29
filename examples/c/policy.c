/*
 * policy.c - a Windows application, its user's settings, and the
 * administrator's policy over them.
 *
 * A desktop program keeps what its user chose under
 * HKCU\Software\<Vendor>\<App>, and an administrator pushes Group Policy
 * to HKCU\Software\Policies\<Vendor>\<App> (or HKLM's), which wins. Two
 * Registry sources at two ranks say exactly that, and one key goes
 * further: the update channel may only come from policy, so a user who
 * sets it in their own settings sees it refused, with the reason, rather
 * than silently ignored.
 *
 *     policy.exe
 *
 * It writes its own keys under HKCU\Software\ConfigForgeExample with
 * reg.exe and removes them afterwards. Elsewhere there is no Registry,
 * and it says so.
 */

#include <configforge/configforge.h>

#include <stdio.h>
#include <stdlib.h>

#define USER "HKCU\\Software\\ConfigForgeExample\\Editor"
#define POLICY "HKCU\\Software\\ConfigForgeExample\\Policies\\Editor"

static void reg(const char* args) {
    char cmd[512];
    cf_fmt(cmd, sizeof cmd, "reg %s >NUL 2>&1", args);
    if (system(cmd) != 0) {
        printf("(reg %s failed)\n", args);
    }
}

int main(void) {
    static const char* const policy_only[] = {"policy", NULL};
    cf_error                 err           = CF_ERROR_INIT;
    cf_builder*              b;
    cf_config*               cfg;
    cf_def                   channel;

    if (!cf_platform_has_registry()) {
        printf("no Registry on %s: on Windows this reads %s under %s\n",
               cf_platform_name(), USER, POLICY);
        return 0;
    }

    /* What the user chose, and what the administrator decided. */
    reg("add " USER " /v FontSize /t REG_DWORD /d 14 /f");
    reg("add " USER " /v Theme /t REG_SZ /d dark /f");
    reg("add " USER " /v UpdateChannel /t REG_SZ /d nightly /f");
    reg("add " POLICY " /v FontSize /t REG_DWORD /d 12 /f");
    reg("add " POLICY " /v UpdateChannel /t REG_SZ /d stable /f");

    b = cf_build(NULL);
    cf_builder_registry(b, "policy", CF_RANK_CLI, POLICY, CF_OPTIONAL);
    cf_builder_registry(b, "user", CF_RANK_USER_FILE, USER, CF_OPTIONAL);
    cf_builder_int(b, "fontsize", 11, 6, 72, 0);
    cf_builder_text(b, "theme", "light", 0);
    cf_def_init(&channel, CF_TYPE_TEXT, "updatechannel");
    channel.policy      = policy_only; /* D-33: only the administrator */
    channel.has_default = CF_TRUE;
    cf_value_text(&channel.default_value, "stable", 6);
    cf_builder_define(b, &channel);
    cfg = cf_builder_create(b, &err);
    if (cfg == NULL) {
        printf("policy: %s\n", err.message);
    } else {
        static char why[2048];
        printf("font %d, theme %s, channel %s\n\n", (int)cf_get_int(cfg, "fontsize", 0),
               cf_get_text(cfg, "theme", "?"), cf_get_text(cfg, "updatechannel", "?"));
        cf_explain(cfg, "fontsize", why, sizeof why);
        fputs(why, stdout);
        cf_explain(cfg, "updatechannel", why, sizeof why);
        fputs(why, stdout);
        cf_config_free(cfg);
    }
    reg("delete HKCU\\Software\\ConfigForgeExample /f");
    return 0;
}
