/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Loch_Modan
SD%Complete: 100
SDComment: Quest support: 3181 (only to argue with pebblebitty to get to searing gorge, before quest rewarded), 309, 273
SDCategory: Loch Modan
EndScriptData */

/* ContentData
npc_miran
npc_huldar
EndContentData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/base/escort_ai.h"

/*######
## npc_miran
######*/

enum
{
    QUEST_PROTECTING_THE_SHIPMENT = 309,

    SAY_MIRAN_1           = 510,
    SAY_MIRAN_2           = 511,
    SAY_MIRAN_3           = 498,

    SAY_DARK_IRON_RAIDER  = 512,
    NPC_DARK_IRON_RAIDER  = 2149,

    MIRAN_ESCORT_PATH     = 1379
};

static const Position m_afAmbushSpawn[] =
{
    { -5705.012f, -3736.6575f, 318.56738f, 0.57595f},
    { -5696.1943f, -3736.78f, 318.58145f, 2.40855f}
};

struct npc_miranAI: public npc_escortAI
{
    npc_miranAI(Creature* creature): npc_escortAI(creature)
    {
        Reset();
    }

    uint8 m_uiDwarves;

    void Reset() override
    {
        if (!HasEscortState(STATE_ESCORT_ESCORTING))
            m_uiDwarves = 0;
    }

    void WaypointReached(uint32 uiPointId) override
    {
        switch (uiPointId)
        {
            case 16:
                DoBroadcastText(SAY_MIRAN_1, m_creature);
                break;
            case 17:
                SetEscortPaused(true);
                m_creature->SummonCreature(NPC_DARK_IRON_RAIDER, m_afAmbushSpawn[0].x, m_afAmbushSpawn[0].y, m_afAmbushSpawn[0].z, m_afAmbushSpawn[0].o, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 25000);
                m_creature->SummonCreature(NPC_DARK_IRON_RAIDER, m_afAmbushSpawn[1].x, m_afAmbushSpawn[1].y, m_afAmbushSpawn[1].z, m_afAmbushSpawn[1].o, TEMPSPAWN_CORPSE_TIMED_DESPAWN, 25000);
                break;
            case 21:
                DoBroadcastText(SAY_MIRAN_3, m_creature);
                if (Player* player = GetPlayerForEscort())
                    player->RewardPlayerAndGroupAtEventExplored(QUEST_PROTECTING_THE_SHIPMENT, m_creature);
                SetEscortPaused(true); 
                m_creature->ForcedDespawn(15000);
                break;
        }
    }

    void SummonedCreatureJustDied(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_DARK_IRON_RAIDER)
        {
            --m_uiDwarves;
            if (!m_uiDwarves)
            {
                DoBroadcastText(SAY_MIRAN_2, m_creature);
                SetEscortPaused(false);
            }
        }
    }

    void JustSummoned(Creature* summoned) override
    {
        if (summoned->GetEntry() == NPC_DARK_IRON_RAIDER)
        {
            ++m_uiDwarves;
            summoned->AI()->AttackStart(m_creature);
            DoBroadcastText(SAY_DARK_IRON_RAIDER, summoned);
        }
    }
};

bool QuestAccept_npc_miran(Player* player, Creature* creature, const Quest* quest)
{
    if (quest->GetQuestId() == QUEST_PROTECTING_THE_SHIPMENT)
    {
        if (npc_miranAI* pEscortAI = dynamic_cast<npc_miranAI*>(creature->AI()))
            pEscortAI->Start(false, player, quest, false, false, MIRAN_ESCORT_PATH);
    }
    return true;
}

UnitAI* GetAI_npc_miran(Creature* creature)
{
    return new npc_miranAI(creature);
}

/*######
## npc_huldar - Quest: Resupplying the Excavation (273)
##
## Blizzlike behavior:
##   Huldar and Saean wait near the supply wagon south of Thelsamar.
##   When a player who has Quest 273 incomplete approaches within 25 yards,
##   Saean reveals his betrayal and summons two Dark Iron Raiders to attack.
##   The player (and Huldar) fight them off. Once Saean is dead, the player
##   can speak to Huldar to turn in the quest.
##   Each player triggers their own independent copy of the event.
######*/

enum HuldarData
{
    QUEST_RESUPPLYING_EXCAVATION    = 273,
    NPC_SAEAN                       = 1380,

    SAY_SAEAN_AMBUSH                = -20001,   // Saean betrayal yell
    SAY_SAEAN_ATTACK                = -20002,   // Saean attack order to raiders
    SAY_HULDAR_VICTORY              = -20003,   // Huldar thanks player after win

    AMBUSH_TRIGGER_RANGE            = 25,
    AMBUSH_CORPSE_DESPAWN_MS        = 30000,
    AMBUSH_RESET_DELAY_MS           = 45000,
};

// Saean spawns slightly ahead of Huldar toward the wagon
static const Position aHuldarAmbushSpawn[] =
{
    // Saean
    { -5760.73f, -3437.71f, 305.54f, 2.44f },
    // Dark Iron Raider 1
    { -5757.80f, -3441.50f, 305.54f, 2.80f },
    // Dark Iron Raider 2
    { -5764.30f, -3440.20f, 305.54f, 2.10f }
};

struct npc_huldarAI : public ScriptedAI
{
    npc_huldarAI(Creature* creature) : ScriptedAI(creature)
    {
        Reset();
    }

    bool            m_bEventInProgress;
    uint32          m_uiResetTimer;
    uint32          m_uiAliveAttackers;
    ObjectGuid      m_saeanGuid;

    // Track which players have already triggered the event this session
    // so the same player cannot trigger a second time while nearby.
    GuidSet         m_triggeredPlayerGuids;

    void Reset() override
    {
        m_bEventInProgress  = false;
        m_uiResetTimer      = 0;
        m_uiAliveAttackers  = 0;
        m_saeanGuid.Clear();
        // Note: intentionally do NOT clear m_triggeredPlayerGuids on Reset()
        // so a player who already did the event cannot re-trigger it.
    }

    void MoveInLineOfSight(Unit* who) override
    {
        ScriptedAI::MoveInLineOfSight(who);

        // Only trigger for players
        if (!who || who->GetTypeId() != TYPEID_PLAYER)
            return;

        // Only when not already running an event
        if (m_bEventInProgress)
            return;

        // Must be within trigger range
        if (!m_creature->IsWithinDistInMap(who, AMBUSH_TRIGGER_RANGE))
            return;

        Player* player = static_cast<Player*>(who);

        // Player must have Quest 273 in an incomplete state
        if (!player->IsCurrentQuest(QUEST_RESUPPLYING_EXCAVATION, 1))
            return;

        // Skip players who have already seen the event this session
        if (m_triggeredPlayerGuids.find(player->GetObjectGuid()) != m_triggeredPlayerGuids.end())
            return;

        // All checks passed - start the ambush
        StartAmbush(player);
    }

    void StartAmbush(Player* player)
    {
        m_bEventInProgress = true;
        m_uiAliveAttackers = 0;
        m_triggeredPlayerGuids.insert(player->GetObjectGuid());

        // Spawn Saean
        if (Creature* saean = m_creature->SummonCreature(
            NPC_SAEAN,
            aHuldarAmbushSpawn[0].x, aHuldarAmbushSpawn[0].y, aHuldarAmbushSpawn[0].z, aHuldarAmbushSpawn[0].o,
            TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, AMBUSH_CORPSE_DESPAWN_MS))
        {
            m_saeanGuid = saean->GetObjectGuid();
            DoScriptText(SAY_SAEAN_AMBUSH, saean);
            ++m_uiAliveAttackers;
        }

        // Spawn two Dark Iron Raiders
        for (int i = 1; i <= 2; ++i)
        {
            if (Creature* raider = m_creature->SummonCreature(
                NPC_DARK_IRON_RAIDER,
                aHuldarAmbushSpawn[i].x, aHuldarAmbushSpawn[i].y, aHuldarAmbushSpawn[i].z, aHuldarAmbushSpawn[i].o,
                TEMPSPAWN_TIMED_OOC_OR_DEAD_DESPAWN, AMBUSH_CORPSE_DESPAWN_MS))
            {
                ++m_uiAliveAttackers;
            }
        }
    }

    void JustSummoned(Creature* summoned) override
    {
        // All ambush mobs attack Huldar immediately; players will join naturally
        summoned->SetInCombatWithZone();
        summoned->AI()->AttackStart(m_creature);

        // Saean orders the raiders to attack
        if (summoned->GetEntry() == NPC_DARK_IRON_RAIDER)
        {
            if (Creature* saean = m_creature->GetMap()->GetCreature(m_saeanGuid))
                DoScriptText(SAY_SAEAN_ATTACK, saean);
        }
    }

    void SummonedCreatureJustDied(Creature* summoned) override
    {
        if (summoned->GetEntry() != NPC_SAEAN && summoned->GetEntry() != NPC_DARK_IRON_RAIDER)
            return;

        if (m_uiAliveAttackers > 0)
            --m_uiAliveAttackers;

        // Saean died - play victory line and start reset timer
        if (summoned->GetEntry() == NPC_SAEAN)
        {
            DoScriptText(SAY_HULDAR_VICTORY, m_creature);
            m_uiResetTimer = AMBUSH_RESET_DELAY_MS;
        }
    }

    void UpdateAI(const uint32 diff) override
    {
        if (m_uiResetTimer)
        {
            if (m_uiResetTimer <= diff)
            {
                m_uiResetTimer = 0;
                m_bEventInProgress = false;
                m_uiAliveAttackers = 0;
                m_saeanGuid.Clear();
                // m_triggeredPlayerGuids intentionally retained
            }
            else
                m_uiResetTimer -= diff;
        }
        
        ScriptedAI::UpdateAI(diff);
    }
};

UnitAI* GetAI_npc_huldar(Creature* creature)
{
    return new npc_huldarAI(creature);
}

void AddSC_loch_modan()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "npc_miran";
    pNewScript->GetAI = &GetAI_npc_miran;
    pNewScript->pQuestAcceptNPC = &QuestAccept_npc_miran;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "npc_huldar";
    pNewScript->GetAI = &GetAI_npc_huldar;
    pNewScript->RegisterSelf();
}
