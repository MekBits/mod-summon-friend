// Logic test for the .summonfriend rules. No AzerothCore -- only the rules.
#include "summon_friend_rules.h"

#include <cstdio>
#include <cstring>

using namespace SummonFriend;

static int g_fail = 0;

static char const* Name(Refusal r)
{
    switch (r)
    {
        case Refusal::None: return "None";
        case Refusal::Disabled: return "Disabled";
        case Refusal::Self: return "Self";
        case Refusal::NotInGroup: return "NotInGroup";
        case Refusal::TargetIsBot: return "TargetIsBot";
        case Refusal::SummonerDead: return "SummonerDead";
        case Refusal::SummonerInCombat: return "SummonerInCombat";
        case Refusal::SummonerInBattleground: return "SummonerInBattleground";
        case Refusal::SummonerInInstance: return "SummonerInInstance";
        case Refusal::SummonerTravelling: return "SummonerTravelling";
        case Refusal::TargetDead: return "TargetDead";
        case Refusal::TargetInBattleground: return "TargetInBattleground";
        case Refusal::TargetBusy: return "TargetBusy";
        case Refusal::Cooldown: return "Cooldown";
    }
    return "?";
}

static void Expect(char const* what, Refusal got, Refusal want)
{
    bool const ok = got == want;
    if (!ok)
        ++g_fail;
    std::printf("%-58s %-24s %s\n", what, Name(got), ok ? "ok" : "FAIL");
}

// The normal situation: two living players in the same group, out in the world.
static Request Ok()
{
    Request r;
    r.sameGroup = true;
    r.now = 1000;
    return r;
}

int main()
{
    Settings const def;

    Expect("group member out in the world", Check(def, Ok()), Refusal::None);

    { Settings s; s.enabled = false; Expect("module disabled", Check(s, Ok()), Refusal::Disabled); }
    { Request r = Ok(); r.sameCharacter = true; Expect("yourself", Check(def, r), Refusal::Self); }
    { Request r = Ok(); r.sameGroup = false; Expect("not in the group", Check(def, r), Refusal::NotInGroup); }
    { Request r = Ok(); r.target.isBot = true; Expect("target is a bot", Check(def, r), Refusal::TargetIsBot); }

    { Request r = Ok(); r.summoner.alive = false; Expect("summoner is dead", Check(def, r), Refusal::SummonerDead); }
    { Request r = Ok(); r.summoner.inCombat = true; Expect("summoner in combat", Check(def, r), Refusal::SummonerInCombat); }
    { Settings s; s.allowInCombat = true; Request r = Ok(); r.summoner.inCombat = true;
      Expect("summoner in combat, AllowInCombat = 1", Check(s, r), Refusal::None); }
    { Request r = Ok(); r.summoner.inBattleground = true; Expect("summoner in a battleground", Check(def, r), Refusal::SummonerInBattleground); }
    { Request r = Ok(); r.summoner.inInstance = true; Expect("summoner in a dungeon (allowed by default)", Check(def, r), Refusal::None); }
    { Settings s; s.allowFromInstances = false; Request r = Ok(); r.summoner.inInstance = true;
      Expect("summoner in a dungeon, AllowFromInstances = 0", Check(s, r), Refusal::SummonerInInstance); }
    { Request r = Ok(); r.summoner.travelling = true; Expect("summoner on taxi/boat/in the air", Check(def, r), Refusal::SummonerTravelling); }

    { Request r = Ok(); r.target.alive = false; Expect("target is dead", Check(def, r), Refusal::TargetDead); }
    { Request r = Ok(); r.target.inBattleground = true; Expect("target in a battleground", Check(def, r), Refusal::TargetInBattleground); }
    { Request r = Ok(); r.target.busy = true; Expect("target already has a summon", Check(def, r), Refusal::TargetBusy); }
    { Request r = Ok(); r.target.inCombat = true; r.target.inInstance = true;
      Expect("target in combat in a dungeon (core decides on accept)", Check(def, r), Refusal::None); }

    // Cooldown: 0 = none, and an expired cooldown does not block.
    { Request r = Ok(); r.lastSummon = 999; Expect("cooldown 0, summon 1 s ago", Check(def, r), Refusal::None); }
    { Settings s; s.cooldownSeconds = 60; Request r = Ok(); r.lastSummon = 990;
      Expect("cooldown 60, summon 10 s ago", Check(s, r), Refusal::Cooldown); }
    { Settings s; s.cooldownSeconds = 60; Request r = Ok(); r.lastSummon = 940;
      Expect("cooldown 60, summon 60 s ago", Check(s, r), Refusal::None); }
    { Settings s; s.cooldownSeconds = 60; Request r = Ok();
      Expect("cooldown 60, never summoned", Check(s, r), Refusal::None); }

    // Order: an impossible request must not report the cooldown.
    { Settings s; s.cooldownSeconds = 60; Request r = Ok(); r.lastSummon = 990; r.sameGroup = false;
      Expect("cooldown AND not in group -> group wins", Check(s, r), Refusal::NotInGroup); }

    // Every refusal needs a message; "%s" may appear at most once, because
    // the module fills in the name with a single replacement.
    for (int i = static_cast<int>(Refusal::Disabled); i <= static_cast<int>(Refusal::Cooldown); ++i)
    {
        char const* m = Message(static_cast<Refusal>(i));
        char const* first = std::strstr(m, "%s");
        bool const ok = std::strlen(m) > 0 && (!first || !std::strstr(first + 2, "%s"));
        if (!ok)
            ++g_fail;
        std::printf("%-58s %-24s %s\n", "message exists and has at most one %s", Name(static_cast<Refusal>(i)), ok ? "ok" : "FAIL");
    }

    std::printf("\n%s\n", g_fail ? "FAILED" : "all ok");
    return g_fail ? 1 : 0;
}
