# mod-summon-friend

An [AzerothCore](https://www.azerothcore.org) module that lets players summon
a group member with `.summonfriend`. The target gets the client's normal
*"X wants to summon you"* window and has two minutes to press Accept. It is a
meeting stone in your pocket: nobody is moved without saying yes.

```
.summonfriend Arthas     summon Arthas (must be in your group or raid)
.summonfriend            summon your current target
```

## Why

Getting a friend across continents normally needs more people than a small
server has online:

| Way | Needs |
|---|---|
| Meeting stone | two players at the stone (the portal object has `reqParticipants = 2`) |
| Ritual of Summoning | a warlock plus two helpers (`reqParticipants = 3`) |
| `.summon` / `.appear` | GM rights, and no dialog |

`.summon` and `.appear` teleport immediately without asking, work on anyone
outside the group, and `.summon` even moves an offline character's saved
position. Handing them to players lets anyone pull the others away mid-quest.

## How it works

The module does two things, both taken from `Spell::EffectSummonPlayer`:

```cpp
target->SetSummonPoint(map, x, y, z);          // expires after MAX_PLAYER_SUMMON_DELAY (120 s)
SMSG_SUMMON_REQUEST << guid << zone << 120000;  // the client shows the dialog
```

Everything after that is the unchanged core: a yes goes through
`WorldSession::HandleSummonResponseOpcode` (rejects dead players and players in
combat) → `Player::SummonIfPossible` (checks expiry) → `TeleportTo`, which
decides on its own whether the target may enter an instance
(`MapMgr::PlayerCannotEnter`). **The module grants no new access**, only a new
way to ask for it.

## Rules

They live in `src/summon_friend_rules.h` without AzerothCore types. A summon is
not sent when:

| Who | What | Why |
|---|---|---|
| target | not in the same group or raid | that is the whole difference from `.summon` |
| target | is a bot (a session without a client) | nobody answers `SMSG_SUMMON_REQUEST`; the dialog would expire silently. With mod-playerbots, whisper the bot `summon` instead |
| summoner | dead, or in combat | like meeting stones and warlocks (`AllowInCombat`) |
| summoner | in a battleground or arena | in: a hole in matchmaking. Out: deserter |
| summoner | on a taxi, a boat or in the air | the summon point would be one you fall from |
| summoner | in an instance | only when `AllowFromInstances = 0` |
| target | dead | the core silently rejects a yes from a dead player |
| target | in a battleground or arena | |
| target | being teleported, or already has a summon pending | |
| summoner | on cooldown | only when `CooldownSeconds > 0` |

A target **in combat** is not refused up front on purpose: the dialog stays
for two minutes, and the fight is usually over before that.

## Installation

1. Clone into `modules/` of your AzerothCore source tree and rebuild:
   ```bash
   cd azerothcore-wotlk/modules
   git clone https://github.com/MekBits/mod-summon-friend.git
   ```
2. Copy `conf/mod_summon_friend.conf.dist` to `mod_summon_friend.conf` in your
   config directory (the defaults also apply without it).
3. The help text for the command is in `data/sql/db-world/` and is applied by
   the database updater.

On startup `Server.log` shows
`SummonFriend: .summonfriend enabled (cooldown 0 s, in combat no, from instances yes)`.
Every summon is logged as `SummonFriend: <name> sent a summon to <name>`.

## Configuration

| Key | Default | |
|---|---|---|
| `SummonFriend.Enable` | 1 | enable the command |
| `SummonFriend.CooldownSeconds` | 0 | seconds between two summons from the same character; kept in memory |
| `SummonFriend.AllowInCombat` | 0 | may the summoner be in combat |
| `SummonFriend.AllowFromInstances` | 1 | may the summoner stand in a dungeon or raid; the core still checks the target's access on accept |

## Tests

```bash
./test/run.sh
```

Needs only g++ and tests the rules. The `core-build` workflow compiles the
module against AzerothCore.

## License

GNU Affero General Public License v3.0, see [LICENSE](LICENSE).
