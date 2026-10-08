/*
* This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
*
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

#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"
#include "Spells/SpellMgr.h"

// 21082 - Seal of the Crusader
struct SealOfTheCrusader : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
        {
            // Seal of the Crusader damage reduction
            // SotC increases attack speed but reduces damage to maintain the same DPS
            float reduction = (-100.0f * aura->GetModifier()->m_amount) / (aura->GetModifier()->m_amount + 100.0f);
            aura->GetTarget()->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, reduction, apply);
            return;
        }

        if (aura->GetEffIndex() == EFFECT_INDEX_2)
        {
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
            return;
        }
    }

    void OnDamageCalculate(Aura* /*aura*/, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        totalMod *= 1.4f; // Patch 2.4.2 - Increases damage of Crusader Strike by 40%
    }
};

// 5373 - Judgement of Light Intermediate
struct JudgementOfLightIntermediate : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (spell->GetTriggeredByAuraSpellInfo() == nullptr)
            return;

        uint32 triggerSpell = 0;
        switch (spell->GetTriggeredByAuraSpellInfo()->Id)
        {
            case 20185: triggerSpell = 20267; break; // Rank 1
            case 20344: triggerSpell = 20341; break; // Rank 2
            case 20345: triggerSpell = 20342; break; // Rank 3
            case 20346: triggerSpell = 20343; break; // Rank 4
            case 27162: triggerSpell = 27163; break; // Rank 5
        }
        if (triggerSpell)
            spell->GetUnitTarget()->CastSpell(nullptr, triggerSpell, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
    }
};

// 1826 - Judgement of Wisdom Intermediate
struct JudgementOfWisdomIntermediate : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (spell->GetTriggeredByAuraSpellInfo() == nullptr)
            return;

        uint32 triggerSpell = 0;
        switch (spell->GetTriggeredByAuraSpellInfo()->Id)
        {
            case 20186: triggerSpell = 20268; break; // Rank 1
            case 20354: triggerSpell = 20352; break; // Rank 2
            case 20355: triggerSpell = 20353; break; // Rank 3
            case 27164: triggerSpell = 27165; break; // Rank 4
        }
        if (triggerSpell)
            spell->GetUnitTarget()->CastSpell(nullptr, triggerSpell, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
    }
};

// 20271 - Judgement
struct spell_judgement : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget || !unitTarget->IsAlive())
            return;

        Unit* caster = spell->GetCaster();

        uint32 spellId2 = 0;

        // all seals have aura dummy
        Unit::AuraList const& m_dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
        for (auto m_dummyAura : m_dummyAuras)
        {
            SpellEntry const* spellInfo = m_dummyAura->GetSpellProto();

            // search seal (all seals have judgement's aura dummy spell id in 2 effect
            if (!spellInfo || !IsSealSpell(m_dummyAura->GetSpellProto()) || m_dummyAura->GetEffIndex() != 2)
                continue;

            // must be calculated base at raw base points in spell proto, GetModifier()->m_value for S.Righteousness modified by SPELLMOD_DAMAGE
            spellId2 = m_dummyAura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2);

            if (spellId2 <= 1)
                continue;

            // found, remove seal
            caster->RemoveAurasDueToSpell(m_dummyAura->GetId());

            // Sanctified Judgement
            Unit::AuraList const& m_auras = caster->GetAurasByType(SPELL_AURA_DUMMY);
            for (Unit::AuraList::const_iterator i = m_auras.begin(); i != m_auras.end(); ++i)
            {
                if ((*i)->GetSpellProto()->SpellIconID == 205 && (*i)->GetSpellProto()->Attributes == uint64(0x01D0))
                {
                    int32 chance = (*i)->GetModifier()->m_amount;
                    if (roll_chance_i(chance))
                    {
                        int32 mana = spellInfo->manaCost;
                        if (Player* modOwner = caster->GetSpellModOwner())
                            modOwner->ApplySpellMod(spellInfo->Id, SPELLMOD_COST, mana);
                        mana = int32(mana * 0.8f);
                        caster->CastCustomSpell(nullptr, 31930, &mana, nullptr, nullptr, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
                    }
                    break;
                }
            }

            break;
        }
        caster->CastSpell(unitTarget, spellId2, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
        if (caster->HasAura(37188)) // improved judgement
            caster->CastSpell(nullptr, 43838, TRIGGERED_OLD_TRIGGERED);

        if (caster->HasAura(40470)) // PaladinTier6Trinket
            if (roll_chance_f(50.f))
                caster->CastSpell(unitTarget, 40472, TRIGGERED_OLD_TRIGGERED);
    }
};

// 40470 - Paladin Tier 6 Trinket
struct PaladinTier6Trinket : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* /*aura*/, ProcExecutionData& procData) const override
    {
        if (!procData.spellInfo)
            return SPELL_AURA_PROC_FAILED;

        float chance = 0.f;

        // Flash of light/Holy light
        if (procData.spellInfo->SpellFamilyFlags & uint64(0x00000000C0000000))
        {
            procData.triggeredSpellId = 40471;
            chance = 15.0f;
            procData.triggerTarget = procData.victim;
        }

        if (!roll_chance_f(chance))
            return SPELL_AURA_PROC_FAILED;

        return SPELL_AURA_PROC_OK;
    }
};

// 31789 - Righteous Defense
struct RighteousDefense : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target)
            return SPELL_CAST_OK;

        Unit* caster = spell->GetCaster();
        if (spell->m_spellInfo->HasAttribute(SPELL_ATTR_EX5_IMPLIED_TARGETING))
        {
            if (!caster->CanAssistSpell(target, spell->m_spellInfo))
            {
                if (Unit* targetOfUnitTarget = target->GetTarget(caster))
                {
                    if (caster->CanAssistSpell(targetOfUnitTarget, spell->m_spellInfo))
                        target = targetOfUnitTarget;
                }
            }
        }

        if (target->getAttackers().empty())
            return SPELL_FAILED_BAD_TARGETS;

        return SPELL_CAST_OK;
    }

    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED) || spell->GetCaster()->CanAssistSpell(target, spell->m_spellInfo))
            return true;

        return false;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget)
            return;
        Unit* caster = spell->GetCaster();

        if (unitTarget->getAttackers().empty())
            return;

        // not empty (checked), copy
        Unit::AttackerSet attackers = unitTarget->getAttackers();

        // selected from list 3
        size_t size = std::min(size_t(3), attackers.size());
        for (uint32 i = 0; i < size; ++i)
        {
            Unit::AttackerSet::iterator aItr = attackers.begin();
            std::advance(aItr, urand() % attackers.size());
            caster->CastSpell((*aItr), 31790, TRIGGERED_NONE); // step 2
            attackers.erase(aItr);
        }
    }
};

enum
{
    SPELL_SEAL_OF_BLOOD_DAMAGE              = 31893,
    SPELL_SEAL_OF_BLOOD_SELF_DAMAGE         = 32221,

    SPELL_JUDGEMENT_OF_BLOOD                = 31898,
    SPELL_JUDGEMENT_OF_BLOOD_SELF_DAMAGE    = 32220
};

// 31893 - Seal of Blood
struct SealOfBloodSelfDamage : public SpellScript
{
    void OnAfterHit(Spell* spell) const override
    {
        int32 damagePoint = spell->GetTotalTargetDamage() * 10 / 100;
        spell->GetCaster()->CastCustomSpell(nullptr, SPELL_SEAL_OF_BLOOD_SELF_DAMAGE, &damagePoint, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
};

// 31898 - Judgement of Blood
struct JudgementOfBloodSelfDamage : public SpellScript
{
    void OnAfterHit(Spell* spell) const override
    {
        int32 damagePoint = spell->GetTotalTargetDamage() * 33 / 100;
        spell->GetCaster()->CastCustomSpell(nullptr, SPELL_JUDGEMENT_OF_BLOOD_SELF_DAMAGE, &damagePoint, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
};

// 19977 - Blessing of Light
struct BlessingOfLight : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_HEALING_TAKEN, apply);
    }

    void OnDamageCalculate(Aura* aura, Unit* attacker, Unit* /*victim*/, int32& advertisedBenefit, float& /*totalMod*/) const override
    {
        advertisedBenefit += (aura->GetModifier()->m_amount);  // BoL is penalized since 2.3.0
        // Note: This forces the caster to keep libram equipped, but works regardless if the BOL is his or not
        if (Aura* improved = attacker->GetAura(38320, EFFECT_INDEX_0)) // improved Blessing of light
        {
            if (aura->GetEffIndex() == EFFECT_INDEX_0)
                advertisedBenefit += improved->GetModifier()->m_amount; // holy light gets full amount
            else
                advertisedBenefit += (improved->GetModifier()->m_amount / 2); // flash of light gets half
        }
    }
};

// 19752 - Divine Intervention
struct DivineIntervention : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target)
            return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
        if (target->HasAura(23333) || target->HasAura(23335) || target->HasAura(34976)) // possibly SPELL_ATTR_EX_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS
            return SPELL_FAILED_TARGET_AURASTATE;
        return SPELL_CAST_OK;
    }
};

// 20467, 20963, 20964, 20965, 20966, 27171 - Judgement of Command
struct JudgementOfCommand : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (!spell->GetUnitTarget()->IsStunned())
            spell->SetDamage(uint32(spell->GetDamage() / 2));
    }
};


// 20473, 20929, 20930, 27174, 33072 - Holy Shock
struct HolyShock : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget)
            return;

        uint32 hurt = 0;
        uint32 heal = 0;
        switch (spell->m_spellInfo->Id)
        {
            case 20473: hurt = 25912; heal = 25914; break;
            case 20929: hurt = 25911; heal = 25913; break;
            case 20930: hurt = 25902; heal = 25903; break;
            case 27174: hurt = 27176; heal = 27175; break;
            case 33072: hurt = 33073; heal = 33074; break;
            default: return;
        }

        if (spell->GetCaster()->CanAssistSpell(unitTarget, spell->m_spellInfo))
            spell->GetCaster()->CastSpell(unitTarget, heal, TRIGGERED_OLD_TRIGGERED);
        else
            spell->GetCaster()->CastSpell(unitTarget, hurt, TRIGGERED_OLD_TRIGGERED);
    }
};

// 20154 etc. - Seal of Righteousness Proc
struct SealOfRighteousnessProc : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0)
            return SPELL_AURA_PROC_FAILED;

        if (aura->GetCaster()->GetTypeId() != TYPEID_PLAYER)
            return SPELL_AURA_PROC_FAILED;

        uint32 spellId;
        switch (aura->GetId())
        {
            case 20154: spellId = 25742; break;     // Rank 1
            case 21084: spellId = 25741; break;     // Rank 1.5
            case 20287: spellId = 25740; break;     // Rank 2
            case 20288: spellId = 25739; break;     // Rank 3
            case 20289: spellId = 25738; break;     // Rank 4
            case 20290: spellId = 25737; break;     // Rank 5
            case 20291: spellId = 25736; break;     // Rank 6
            case 20292: spellId = 25735; break;     // Rank 7
            case 20293: spellId = 25713; break;     // Rank 8
            case 27155: spellId = 27156; break;     // Rank 9
            default: return SPELL_AURA_PROC_FAILED;
        }

        Player* player = (Player*)aura->GetCaster();
        Item* item = player->GetItemByPos(INVENTORY_SLOT_BAG_0, EQUIPMENT_SLOT_MAINHAND);
        float speed = (item ? item->GetProto()->Delay : BASE_ATTACK_TIME) / 1000.0f;

        float damageBasePoints;
        float coeff;
        if (item && item->GetProto()->InventoryType == INVTYPE_2HWEAPON)
        {
            damageBasePoints = 1.20f * aura->GetModifier()->m_amount * 1.2f * 1.03f * speed / 100.0f + 1;
            coeff = .108f * speed;
        }
        else
        {
            damageBasePoints = 0.85f * ceil(aura->GetModifier()->m_amount * 1.2f * 1.03f * speed / 100.0f) - 1;
            coeff = .092f * speed;
        }

        int32 damagePoint = int32(damageBasePoints + 0.03f * (player->GetBaseWeaponDamage(BASE_ATTACK, MINDAMAGE) + player->GetBaseWeaponDamage(BASE_ATTACK, MAXDAMAGE)) / 2.0f) + 1;

        if (damagePoint >= 0)
        {
            int32 bonusDamage = player->SpellBaseDamageBonusDone(GetSpellSchoolMask(aura->GetSpellProto())) + procData.victim->SpellBaseDamageBonusTaken(GetSpellSchoolMask(aura->GetSpellProto()));
            if (Aura* impAura = player->GetAura(43743, EFFECT_INDEX_0)) // Improved Seal of Righteousness
                bonusDamage += impAura->GetAmount();
            damagePoint += bonusDamage * coeff * player->CalculateLevelPenalty(aura->GetSpellProto());
        }

        player->CastCustomSpell(procData.victim, spellId, &damagePoint, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED, nullptr, aura);
        return SPELL_AURA_PROC_OK;
    }
};

// 31804 - Judgement of Vengeance
struct JudgementOfVengeance : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        uint32 stacks = 0;
        Unit::AuraList const& auras = spell->GetUnitTarget()->GetAurasByType(SPELL_AURA_PERIODIC_DAMAGE);
        for (auto aura : auras)
        {
            if ((aura->GetId() == 31803) && aura->GetCasterGuid() == spell->GetCaster()->GetObjectGuid())
            {
                stacks = aura->GetStackAmount();
                break;
            }
        }
        if (!stacks)
            spell->SetDamage(-1);
        else
            spell->SetDamage(spell->GetDamage() * stacks);
    }
};

// 20216 etc - Illumination
struct Illumination : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        if (!procData.spellInfo)
            return SPELL_AURA_PROC_FAILED;

        uint32 originalSpellId = procData.spellInfo->Id;

        if (procData.spellInfo->SpellFamilyFlags & uint64(0x0001000000000000))
        {
            switch (procData.spellInfo->Id)
            {
                case 25914: originalSpellId = 20473; break;
                case 25913: originalSpellId = 20929; break;
                case 25903: originalSpellId = 20930; break;
                case 27175: originalSpellId = 27174; break;
                case 33074: originalSpellId = 33072; break;
                default: return SPELL_AURA_PROC_FAILED;
            }
        }

        SpellEntry const* originalSpell = sSpellTemplate.LookupEntry<SpellEntry>(originalSpellId);
        if (!originalSpell)
            return SPELL_AURA_PROC_FAILED;

        int32 cost = originalSpell->manaCost;
        procData.basepoints[0] = cost * aura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_1) / 100;
        procData.triggerTarget = aura->GetCaster();
        procData.triggeredSpellId = 20272;
        return SPELL_AURA_PROC_OK;
    }
};

void LoadPaladinScripts()
{
    RegisterSpellScript<JudgementOfLightIntermediate>("spell_judgement_of_light_intermediate");
    RegisterSpellScript<JudgementOfWisdomIntermediate>("spell_judgement_of_wisdom_intermediate");
    RegisterSpellScript<DivineIntervention>("spell_divine_intervention");
    RegisterSpellScript<spell_judgement>("spell_judgement");
    RegisterSpellScript<RighteousDefense>("spell_righteous_defense");
    RegisterSpellScript<SealOfTheCrusader>("spell_seal_of_the_crusader");
    RegisterSpellScript<SealOfBloodSelfDamage>("spell_seal_of_blood_self_damage");
    RegisterSpellScript<JudgementOfBloodSelfDamage>("spell_judgement_of_blood_self_damage");
    RegisterSpellScript<PaladinTier6Trinket>("spell_paladin_tier_6_trinket");
    RegisterSpellScript<BlessingOfLight>("spell_blessing_of_light");
    RegisterSpellScript<JudgementOfCommand>("spell_judgement_of_command");
    RegisterSpellScript<HolyShock>("spell_pal_holy_shock");
    RegisterSpellScript<SealOfRighteousnessProc>("spell_seal_of_righteousness_proc");
    RegisterSpellScript<JudgementOfVengeance>("spell_judgement_of_vengeance");
    RegisterSpellScript<Illumination>("spell_pal_illumination");
}