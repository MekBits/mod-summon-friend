#pragma once

// The rules for .summonfriend, without a single AzerothCore type. The module
// translates the Player objects into the two structs below, so the whole
// decision can be tested with g++ alone (test/run.sh).
//
// The teleport itself is NOT here. It is the core's own: SetSummonPoint() +
// SMSG_SUMMON_REQUEST shows the client's normal summon dialog, and a yes goes
// through WorldSession::HandleSummonResponseOpcode -> SummonIfPossible ->
// TeleportTo. The rules the core enforces on accept (alive, not in combat,
// access to the instance) are not repeated here -- they are only checked up
// front where a later no would otherwise be unexplained to the player.

#include <cstdint>
#include <ctime>

namespace SummonFriend
{
    enum class Refusal
    {
        None,
        Disabled,
        Self,
        NotInGroup,
        TargetIsBot,
        SummonerDead,
        SummonerInCombat,
        SummonerInBattleground,
        SummonerInInstance,
        SummonerTravelling,
        TargetDead,
        TargetInBattleground,
        TargetBusy,
        Cooldown,
    };

    struct Settings
    {
        bool enabled = true;
        bool allowInCombat = false;
        bool allowFromInstances = true;
        uint32_t cooldownSeconds = 0;
    };

    struct Side
    {
        bool alive = true;
        bool inCombat = false;
        bool inBattleground = false;   // battleground OR arena
        bool inInstance = false;       // dungeon or raid
        bool travelling = false;       // taxi, boat/zeppelin, or flying in the air
        bool isBot = false;
        bool busy = false;             // being teleported, or already has a pending summon
    };

    struct Request
    {
        Side summoner;
        Side target;
        bool sameCharacter = false;
        bool sameGroup = false;
        std::time_t now = 0;
        std::time_t lastSummon = 0;    // 0 = never
    };

    // The order is the one that helps the player: wrong target before state,
    // and cooldown last, so the cooldown is never spent on a summon that could
    // not be sent anyway.
    inline Refusal Check(Settings const& s, Request const& r)
    {
        if (!s.enabled)
            return Refusal::Disabled;
        if (r.sameCharacter)
            return Refusal::Self;
        if (!r.sameGroup)
            return Refusal::NotInGroup;
        if (r.target.isBot)
            return Refusal::TargetIsBot;

        if (!r.summoner.alive)
            return Refusal::SummonerDead;
        if (r.summoner.inCombat && !s.allowInCombat)
            return Refusal::SummonerInCombat;
        // A summon OUT of a battleground would pull the player out of the
        // fight and give deserter; a summon INTO one is a hole in matchmaking.
        if (r.summoner.inBattleground)
            return Refusal::SummonerInBattleground;
        if (r.summoner.inInstance && !s.allowFromInstances)
            return Refusal::SummonerInInstance;
        // The summon point is the caster's position. On a taxi, a boat or in
        // the air on a flying mount, that is a point you fall from.
        if (r.summoner.travelling)
            return Refusal::SummonerTravelling;

        // HandleSummonResponseOpcode silently rejects a yes from a dead
        // player. Without this check the dialog would show and nothing happen.
        if (!r.target.alive)
            return Refusal::TargetDead;
        if (r.target.inBattleground)
            return Refusal::TargetInBattleground;
        if (r.target.busy)
            return Refusal::TargetBusy;

        if (s.cooldownSeconds && r.lastSummon && r.now < r.lastSummon + static_cast<std::time_t>(s.cooldownSeconds))
            return Refusal::Cooldown;

        return Refusal::None;
    }

    // "%s" is the target's name where it makes sense; the module fills it in.
    inline char const* Message(Refusal r)
    {
        switch (r)
        {
            case Refusal::None:                   return "";
            case Refusal::Disabled:               return "Summoning friends is switched off on this server.";
            case Refusal::Self:                   return "You can't summon yourself.";
            case Refusal::NotInGroup:             return "%s has to be in your group.";
            case Refusal::TargetIsBot:            return "%s is a bot and can't answer a summon. Whisper it \"summon\" instead.";
            case Refusal::SummonerDead:           return "You can't summon anyone while you're dead.";
            case Refusal::SummonerInCombat:       return "You can't summon anyone while you're in combat.";
            case Refusal::SummonerInBattleground: return "You can't summon anyone into a battleground or arena.";
            case Refusal::SummonerInInstance:     return "You can't summon anyone into a dungeon or raid.";
            case Refusal::SummonerTravelling:     return "You can't summon anyone while on a flight path, a boat or in the air.";
            case Refusal::TargetDead:             return "%s is dead and can't accept a summon.";
            case Refusal::TargetInBattleground:   return "%s is in a battleground or arena.";
            case Refusal::TargetBusy:             return "%s is already being summoned or teleported.";
            case Refusal::Cooldown:               return "You summoned someone too recently. Try again in a moment.";
        }
        return "";
    }
}
