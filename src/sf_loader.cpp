void AddSummonFriendScripts();

// The name is not free: AzerothCore's module CMake generates a call to
// Add<directory name with _ instead of ->Scripts(). The directory is
// mod-summon-friend, so Addmod_summon_friendScripts().
void Addmod_summon_friendScripts()
{
    AddSummonFriendScripts();
}
