#include "Managers/ActorContextManager.h"

#include "PCH.h"

#include <mutex>
#include <unordered_map>

static std::unordered_map<RE::FormID, ActorContext> g_actorContexts;
static std::mutex g_contextMutex;

ActorContext& GetActorContext(RE::Actor* actor) 
{
    std::scoped_lock lock(g_contextMutex);
    return g_actorContexts[actor->GetFormID()];
}


// Used to gate features the player remember about an enemy even if they are not visible anymore
bool HasActorBeenSeen(RE::FormID formID)
{
    std::scoped_lock lock(g_contextMutex);
    auto it = g_actorContexts.find(formID);
    return it != g_actorContexts.end() and it->second.everSeen;
}

void ClearActorContexts()
{ 
    std::scoped_lock lock(g_contextMutex);
    g_actorContexts.clear(); 
}

void ResetCombatState() 
{
    std::scoped_lock lock(g_contextMutex);
    for (auto& [id, context] : g_actorContexts)
    {
        context.lastHitActor = 0;
        context.inCombat = false;
    }
}
