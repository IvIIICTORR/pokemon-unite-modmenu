//
// PokemonUnite.h
// Offsets e assinaturas do Pokemon Unite (iOS / UnityFramework)
//
// Fonte dos RVAs: dump_pokemonunite_ios/dump.cs (versao 1.23.1)
// Binario alvo: Payload/pokemonunite.app/Frameworks/UnityFramework.framework/UnityFramework
// Bundle ID:    jp.pokemon.pokemonunite
//
// COMO ATUALIZAR EM NOVA VERSAO:
//   1. Gerar novo dump.cs do UnityFramework com il2cppdumper
//   2. Buscar as classes: MVisionSysUtil, LVisionSysUtil, LVisionSys,
//      LBattleStatSys, LSkillBaseSelectTarget, LSkillSelectNearestTarget, MSkillBtnSysComp
//   3. Copiar os novos "// RVA: 0x..." para as macros abaixo
//
#ifndef POKEMONUNITE_H
#define POKEMONUNITE_H

// ======================= MAP HACK (Sistema de Visao) =======================
// MobaBattleModule.Vision.M / .L

// static bool MVisionSysUtil.IsVisible(Actor actor, Actor checkedActor, bool checkAntiInvisible)
#define OFF_MVisionSysUtil_IsVisible        0x619C080

// static bool LVisionSysUtil.IsCampVisible(LVisionSysComp sysComp, int srcCamp, int dstId)
#define OFF_LVisionSysUtil_IsCampVisible    0x4FDF06C

// bool LVisionSys.CheckVisible(LVisionSysComp, Entity srcEntity, Entity dstEntity, out bool visible)  [instancia]
#define OFF_LVisionSys_CheckVisible         0x4FD95A4

// bool LVisionSys.IsCampVisible(LVisionSysComp, int camp, Entity dstEntity)  [instancia]
#define OFF_LVisionSys_IsCampVisible        0x4FD948C

// void LBattleStatSys.UpdateToCheckVisionStat()  [anti-cheat -> no-op]
#define OFF_LBattleStatSys_UpdateStat       0x4F82A04

// ======================= AIMBOT (Selecao de Alvo de Skill) =======================
// MobaBattleModule.SkillTargetSelect.L

// Entity LSkillBaseSelectTarget.SelectTargetByDir(Entity inActor, LSkillSlot inUseSlot, bool bIsAutoSelectTarget, VInt2 dir)
#define OFF_LSkill_SelectTargetByDir        0x4F5B750

// Entity LSkillBaseSelectTarget.SelectTarget(Entity inActor, LSkillSlot inUseSlot, bool bIsAutoSelectTarget, bool inUseMaxSearchDistance)
#define OFF_LSkill_SelectTarget             0x4F5B6C0

// Entity LSkillSelectNearestTarget.SelectTarget(...)  [impl. do alvo mais proximo]
#define OFF_LSkillNearest_SelectTarget      0x4F5DD00

// Entity LSkillSelectNearestTarget.GetNearestEnemyActor(Entity inActor, LSkillSlot inUseSlot, bool bIsAutoSelectTarget, bool inUseMaxSearchDistance)  [private]
#define OFF_LSkillNearest_GetNearestEnemy   0x4F5DDA0

// MobaBattleModule.SkillButton.M -> MSkillBtnSysComp
// Vector2 GetSkillIndicatorDir(eSkillSlotType slotType, sbyte idx)
#define OFF_MSkillBtn_GetIndicatorDir       0x614D4B8
// Vector2 GetSkillBtnDir(eSkillSlotType slotType, sbyte idx, bool btnDir, bool fixMirror)
#define OFF_MSkillBtn_GetSkillBtnDir        0x614D2A8

// Estrutura VInt2 (dois int32) usada pelo motor de logica do jogo
struct VInt2 { int x; int y; };

#endif // POKEMONUNITE_H
