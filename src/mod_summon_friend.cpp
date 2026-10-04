/*
 * mod-summon-friend
 *
 * `.summonfriend <name>` -- or `.summonfriend` with a player targeted -- sends
 * a normal summon to a group member. The target gets the client's own
 * "X wants to summon you" dialog and has two minutes to press Accept.
 *
 * Why a module and not the GM commands: `.summon` and `.appear`
 * (cs_misc.cpp) teleport immediately without asking, to and from anyone --
 * also outside the group, and `.summon` even moves an offline character's
 * saved position. This is a meeting stone in your pocket instead: the same
 * dialog, and the same rules once the target says yes.
 *
 * The mechanism is the core's own, taken from Spell::EffectSummonPlayer:
 *
 *   target->SetSummonPoint(map, x, y, z)   expires after MAX_PLAYER_SUMMON_DELAY
 *   SMSG_SUMMON_REQUEST (guid, zone, ms)   the client shows the dialog
 *
 * A yes comes back as CMSG_SUMMON_RESPONSE and goes through
 * WorldSession::HandleSummonResponseOpcode (rejects dead players and players
 * in combat) -> Player::SummonIfPossible (checks expiry) -> TeleportTo, which
 * decides on its own whether the target may enter an instance
 * (MapMgr::PlayerCannotEnter). The module grants no new access, only a new way
 * to ask for it.
 *
 * The rules for WHEN a summon is sent at all live in summon_friend_rules.h
 * without any AzerothCore types, so they can be tested with g++ alone.
 */

#include "Chat.h"
#include "CommandScript.h"
#include "Config.h"
#include "GameTime.h"
#include "Log.h"
#include "Map.h"
#include "Opcodes.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldPacket.h"
#include "WorldSession.h"

#include "summon_friend_rules.h"

#include <string>
#include <unordered_map>

using namespace Acore::ChatCommands;

namespace
{
    SummonFriend::Settings g_settings;

    // Last summon per character. Kept in memory on purpose: the cooldown
    // defaults to 0, and a restart that resets it is harmless.
    std::unordered_map<ObjectGuid::LowType, std::time_t> g_lastSummon;

    SummonFriend::Side Describe(Player* player, std::time_t now)
    {
        SummonFriend::Side side;
        Map const* map = player->GetMap();

        side.alive = player->IsAlive();
        side.inCombat = player->IsInCombat();
        side.inBattleground = map && map->IsBattlegroundOrArena();
        side.inInstance = map && map->IsDungeon();
        side.travelling = player->IsInFlight() || player->GetTransport() || player->IsFlying();
        // A session without a client (playerbots, for example) has nobody to
        // answer SMSG_SUMMON_REQUEST, so the dialog would just expire.
        side.isBot = player->GetSession() && player->GetSession()->IsHeadless();
        side.busy = player->IsBeingTeleported() || player->GetSummonExpireTimer() > now;
        return side;
    }

    std::string WithName(char const* message, std::string const& name)
    {
        std::string text(message);
        std::string::size_type const at = text.find("%s");
        if (at != std::string::npos)
            text.replace(at, 2, name);
        return text;
    }
}

class SummonFriendWorld : public WorldScript
{
public:
    SummonFriendWorld() : WorldScript("SummonFriendWorld") { }

    // Read here and not on every call: GetOption() logs "Missing property"
    // each time a key is missing.
    void OnAfterConfigLoad(bool /*reload*/) override
    {
        g_settings.enabled            = sConfigMgr->GetOption<bool>("SummonFriend.Enable", true);
        g_settings.cooldownSeconds    = sConfigMgr->GetOption<uint32>("SummonFriend.CooldownSeconds", 0);
        g_settings.allowInCombat      = sConfigMgr->GetOption<bool>("SummonFriend.AllowInCombat", false);
        g_settings.allowFromInstances = sConfigMgr->GetOption<bool>("SummonFriend.AllowFromInstances", true);

        LOG_INFO("module", "SummonFriend: .summonfriend {} (cooldown {} s, in combat {}, from instances {})",
                 g_settings.enabled ? "enabled" : "disabled", g_settings.cooldownSeconds,
                 g_settings.allowInCombat ? "yes" : "no", g_settings.allowFromInstances ? "yes" : "no");
    }
};

class SummonFriendCommand : public CommandScript
{
public:
    SummonFriendCommand() : CommandScript("SummonFriendCommand") { }

    ChatCommandTable GetCommands() const override
    {
        // SEC_PLAYER = 0: ChatCommandNode::IsVisible treats 0-4 as security
        // levels and only >= 200 as RBAC permissions, so every player can use
        // it. The same number is in `command` (data/sql).
        static ChatCommandTable commandTable =
        {
            { "summonfriend", HandleSummonFriend, SEC_PLAYER, Console::No },
        };
        return commandTable;
    }

    static bool HandleSummonFriend(ChatHandler* handler, Optional<PlayerIdentifier> target)
    {
        Player* summoner = handler->GetSession()->GetPlayer();

        if (!target)
            target = PlayerIdentifier::FromTarget(handler);

        if (!target)
        {
            handler->SendErrorMessage("Target a player in your group, or type .summonfriend and their name.", false);
            return false;
        }

        Player* friendPlayer = target->GetConnectedPlayer();
        if (!friendPlayer)
        {
            handler->SendErrorMessage(WithName("%s is not online.", target->GetName()), false);
            return false;
        }

        std::time_t const now = GameTime::GetGameTime().count();
        auto const last = g_lastSummon.find(summoner->GetGUID().GetCounter());

        SummonFriend::Request request;
        request.summoner = Describe(summoner, now);
        request.target = Describe(friendPlayer, now);
        request.sameCharacter = friendPlayer == summoner;
        request.sameGroup = friendPlayer != summoner && summoner->IsInSameRaidWith(friendPlayer);
        request.now = now;
        request.lastSummon = last != g_lastSummon.end() ? last->second : 0;

        SummonFriend::Refusal const refusal = SummonFriend::Check(g_settings, request);
        if (refusal != SummonFriend::Refusal::None)
        {
            handler->SendErrorMessage(WithName(SummonFriend::Message(refusal), friendPlayer->GetName()), false);
            return false;
        }

        // Next to the summoner, not on top: the same point `.summon` uses.
        float x, y, z;
        summoner->GetClosePoint(x, y, z, friendPlayer->GetObjectSize());
        friendPlayer->SetSummonPoint(summoner->GetMapId(), x, y, z);

        WorldPacket data(SMSG_SUMMON_REQUEST, 8 + 4 + 4);
        data << summoner->GetGUID();
        data << uint32(summoner->GetZoneId());
        data << uint32(MAX_PLAYER_SUMMON_DELAY * IN_MILLISECONDS);
        friendPlayer->SendDirectMessage(&data);

        g_lastSummon[summoner->GetGUID().GetCounter()] = now;

        handler->SendSysMessage(WithName("Summoning %s. They have two minutes to accept.", friendPlayer->GetName()));
        LOG_INFO("module", "SummonFriend: {} sent a summon to {} (map {}, zone {})",
                 summoner->GetName(), friendPlayer->GetName(), summoner->GetMapId(), summoner->GetZoneId());
        return true;
    }
};

void AddSummonFriendScripts()
{
    new SummonFriendWorld();
    new SummonFriendCommand();
}
