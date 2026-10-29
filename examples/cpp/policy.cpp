/*
 * policy.cpp - a Windows application, its user's settings, and the
 * administrator's policy over them, in C++03.
 *
 * policy.c through the wrapper: two Registry sources, the policy above
 * the user, and one key only the policy may set.
 *
 *     policy_cpp.exe
 */

#include <configforge/configforge.hpp>

#include <cstdio>
#include <cstdlib>

#define USER "HKCU\\Software\\ConfigForgeExample\\Editor"
#define POLICY "HKCU\\Software\\ConfigForgeExample\\Policies\\Editor"

static void reg(const char* args)
{
    char cmd[512];
    cf_fmt(cmd, sizeof cmd, "reg %s >NUL 2>&1", args);
    if (std::system(cmd) != 0) {
        std::printf("(reg %s failed)\n", args);
    }
}

int main()
{
    static const char* const policyOnly[] = {"policy", 0};

    if (!cf_platform_has_registry()) {
        std::printf("no Registry on %s: on Windows this reads %s under %s\n",
                    cf_platform_name(), USER, POLICY);
        return 0;
    }
    reg("add " USER " /v FontSize /t REG_DWORD /d 14 /f");
    reg("add " USER " /v Theme /t REG_SZ /d dark /f");
    reg("add " USER " /v UpdateChannel /t REG_SZ /d nightly /f");
    reg("add " POLICY " /v FontSize /t REG_DWORD /d 12 /f");
    reg("add " POLICY " /v UpdateChannel /t REG_SZ /d stable /f");

    cf_def channel;
    cf_def_init(&channel, CF_TYPE_TEXT, "updatechannel");
    channel.policy      = policyOnly; /* D-33: only the administrator */
    channel.has_default = CF_TRUE;
    cf_value_text(&channel.default_value, "stable", 6);

    cf::Config cfg = cf::Builder()
                         .registry("policy", CF_RANK_CLI, POLICY, CF_OPTIONAL)
                         .registry("user", CF_RANK_USER_FILE, USER, CF_OPTIONAL)
                         .defineInt("fontsize", 11, 6, 72)
                         .defineText("theme", "light")
                         .define(channel)
                         .create();
    if (!cfg) {
        std::printf("policy: %s\n", cfg.error().message());
    } else {
        std::printf("font %d, theme %s, channel %s\n\n", cfg.get<int>("fontsize", 0),
                    cfg.text("theme").c_str(), cfg.text("updatechannel").c_str());
        std::fputs(cfg.explain("fontsize").c_str(), stdout);
        std::fputs(cfg.explain("updatechannel").c_str(), stdout);
    }
    reg("delete HKCU\\Software\\ConfigForgeExample /f");
    return 0;
}
