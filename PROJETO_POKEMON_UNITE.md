# Pokemon Unite - Projeto Completo (Android + iOS)

**Package**: `jp.pokemon.pokemonunite`
**Engine**: Unity IL2CPP (ARM64)
**Tipo**: MOBA top-down com Fog of War

---

## Estrutura de Pastas

```
sniperstriker/
├── dump_pokemonunite/              # Dump Android (libil2cpp.so)
│   └── dump.cs
├── dump_pokemonunite_ios/          # Dump iOS (UnityFramework)
│   └── dump.cs
├── modmenu_pokemonunite/           # Mod Menu Android (baseado no Android-Mod-Menu-4.0)
│   ├── app/src/main/jni/
│   │   ├── Main.cpp                # Hooks principais (Sniper Strike base, adaptado para PU)
│   │   ├── Hacks.h                 # Map Hack hooks (3 vision functions) + ESP + Drone View
│   │   ├── Offsets.h               # RVAs Honor of Kings (base, não PU)
│   │   └── Draw.h                  # ImGui overlay
│   └── libs/arm64-v8a/libgiyuutmk.so
├── pokemon-zygisk-module/          # Zygisk loader para Pokemon Unite
│   ├── jni/
│   │   ├── loader.cpp              # PUMod - target jp.pokemon.pokemonunite
│   │   ├── zygisk.hpp              # API v4 (Kitsune 26.4)
│   │   ├── Android.mk
│   │   └── Application.mk
│   ├── build_module.py             # Script de build (ndk-build + zip)
│   ├── module.prop                 # id=pu_mod
│   └── output/PUMod.zip            # Módulo Magisk pronto
├── ipa_extracted/                   # IPA extraído do iOS
│   └── Payload/pokemonunite.app/Frameworks/UnityFramework.framework/
│       ├── UnityFramework           # Binário atual (patcheado)
│       └── UnityFramework.original  # Backup do original
├── patch_ios_maphack.py            # Script Python para patch estático do binário iOS
├── repack_ipa.py                   # Script Python para reempacotar IPA
├── pokemonunite_maphack.ipa        # IPA patcheado final (~916 MB)
├── GUIA_MAPHACK_POKEMON_UNITE.md   # Guia técnico detalhado do Map Hack
└── PROJETO_POKEMON_UNITE.md        # ESTE ARQUIVO
```

---

## 1. Map Hack - iOS (FUNCIONANDO ✅)

### Método: Patch estático do binário UnityFramework

Modificar bytes diretamente no binário ARM64 antes de empacotar o IPA.

### Patches ARM64
```
MOV W0, #1; RET = 20 00 80 52 C0 03 5F D6   (return true)
RET             = C0 03 5F D6                 (nop void function)
```

### Funções Patcheadas (confirmadas via libtool)

| # | Função | RVA iOS | Patch |
|---|--------|---------|-------|
| 1 | `MVisionSysUtil.IsVisible(Actor, Actor, bool)` | `0x619C080` | return true |
| 2 | `LVisionSysUtil.IsCampVisible(sysComp, Int32, Int32)` | `0x4FDF06C` | return true |
| 3 | `LVisionSys.CheckVisible(sysComp, Entity, Entity, out bool)` | `0x4FD95A4` | return true |
| 4 | `LVisionSys.IsCampVisible(sysComp, Int32, Entity)` | `0x4FD948C` | return true |
| 5 | `LBattleStatSys.UpdateToCheckVisionStat()` | `0x4F82A04` | RET (anti-cheat) |

### Anti-Cheat iOS

- **`LBattleStatSys.UpdateToCheckVisionStat()`** — coleta estatísticas de visão (mEnemyOBCount, mEnemyOBCheat na classe `AcntCheatInfo`). Patcheado com RET para impedir coleta.
- **`AcntCheatInfo`** (protobuf) — campos `mEnemyOBCount`, `mEnemyOBCheat`, `mHaveMofiyData`
- **`PGSInt`** — inteiros ofuscados/criptografados usados internamente

### Bug Corrigido: Auto-Attack Quebrado

- **Problema**: `LVisionSys.CheckVisible` tem parâmetro `out bool visible`. Patch com `MOV W0, #1; RET` retorna true mas NÃO seta o `out`, deixando `visible=false` → auto-attack quebrado
- **Solução libtool**: Funciona porque o hook seta o `out` corretamente antes de retornar
- **Solução patch estático**: Usar funções utility estáticas (`LVisionSysUtil.*`) que não têm `out` params. MAS nos testes todas as 4 funcionaram via libtool, então mantemos as 4.

### Scripts

- **`patch_ios_maphack.py`** — aplica os 5 patches no binário, gera `.patched`
- **`repack_ipa.py`** — reempacota como `.ipa` (skip `.original` e `.patched`)

### Fluxo de Deploy iOS
```
1. python patch_ios_maphack.py          # Gera UnityFramework.patched
2. Copiar .patched → UnityFramework     # Substituir o binário
3. python repack_ipa.py                 # Gera pokemonunite_maphack.ipa
4. Assinar com ldid/codesign
5. Instalar via Sideloadly
```

---

## 2. Map Hack - Android (FUNCIONANDO ✅)

### Método: Zygisk Mod Menu com inline hooks

### Funções Hookadas (confirmadas funcionando)

| # | Função | Classe | RVA Android |
|---|--------|--------|-------------|
| 1 | `IsVisible(Actor, Actor, Boolean)` | **M**VisionSysUtil | `0x384b874` |
| 2 | `IsCampVisible(LVisionSysComp, Int32, Int32)` | **L**VisionSysUtil | `0x3705028` |
| 3 | `CheckVisibleUnittypeMask(Int32, Int32)` | **L**VisionSysUtil | `0x3705658` |

### IMPORTANTE - Funções que NÃO usar no Android

| Função | RVA | Problema |
|--------|-----|----------|
| `LVisionSys.IsCampVisible` (instância) | `0x351003c` | Faz personagens desaparecerem |
| `LVisionSys.CheckVisible` (instância) | `0x3510154` | Faz personagens desaparecerem |

### Hooks no Hacks.h (modmenu_pokemonunite)

```cpp
// Hook 1: MVisionSysUtil.IsVisible - return true quando ativado
static bool hk_IsVisible_Actor(void* actor, void* checkedActor, bool checkAntiInvisible) {
    if (Hacks::bHook_IsVisible) return true;
    return old_IsVisible_Actor(actor, checkedActor, checkAntiInvisible);
}

// Hook 2: LVisionSys.IsCampVisible (instance, pra Android usa utility estática)
static bool hk_IsCampVisible(void* self, void* sysComp, int camp, void* entity) {
    if (Hacks::bHook_IsCampVisible) return true;
    return old_IsCampVisible(self, sysComp, camp, entity);
}

// Hook 3: LVisionSys.CheckVisible - SETA out bool antes de retornar
static bool hk_CheckVisible(void* self, void* sysComp, void* srcEntity, void* dstEntity, bool* visible) {
    if (Hacks::bHook_CheckVisible) {
        if (visible) *visible = true;  // CRUCIAL: setar o out param
        return true;
    }
    return old_CheckVisible(self, sysComp, srcEntity, dstEntity, visible);
}
```

### Projeto Zygisk (pokemon-zygisk-module/)

- **Target**: `jp.pokemon.pokemonunite`
- **Module ID**: `pu_mod`
- **Module path no device**: `/data/adb/modules/pu_mod/`
- **loader.cpp**: PUMod class, copia DEX + SO para `/data/data/jp.pokemon.pokemonunite/mod_cache/`
- **Build**: `python build_module.py` → gera `output/PUMod.zip`

### Deploy Android
```
1. python build_module.py
2. adb push output/PUMod.zip /data/local/tmp/
3. adb shell "su -c 'magisk --install-module /data/local/tmp/PUMod.zip'"
4. adb shell reboot
```

---

## 3. Arquitetura do Sistema de Visão

O jogo usa duas threads:
- **MainThread (M)** — renderização, UI, efeitos visuais
- **LogicThread (L)** — lógica de jogo, cálculos, frame sync

### Classes do Sistema de Visão

| Classe | Thread | Tipo | Namespace |
|--------|--------|------|-----------|
| `MVisionSysUtil` | Main | static utility | `MobaBattleModule.Vision.M` |
| `MVisionSys` | Main | instância | `MobaBattleModule.Vision.M` |
| `LVisionSysUtil` | Logic | static utility | `MobaBattleModule.Vision.L` |
| `LVisionSys` | Logic | instância | `MobaBattleModule.Vision.L` |

### Classes Auxiliares

| Classe | Propósito |
|--------|-----------|
| `LVisionSysComp` | Componente de visão (dados do sistema) |
| `EntityVisionComp` | Componente de visão por entidade |
| `CampVisionData` | Cache de visibilidade por camp |
| `LBattleStatSys` | Estatísticas de batalha (anti-cheat) |
| `LBattleStatComp` | Componente de estatísticas |
| `AcntCheatInfo` | Protobuf com contadores de cheat |
| `AcntDetailDataIndividual` | Estatísticas individuais do jogador |

---

## 4. Sistema de Skills e Aimbot (ANÁLISE COMPLETA ✅, IMPLEMENTAÇÃO PENDENTE ❌)

### Viabilidade

- **Android (Zygisk)**: ✅ Viável — hooks inline permitem lógica runtime
- **iOS (patch estático)**: ❌ Não viável — precisa de lógica runtime
- **iOS (dylib + Dobby)**: ⚠️ Possível — requer macOS/VM com Xcode para compilar

### Fluxo de Uma Skill

```
Player arrasta joystick → MSkillBtnSysComp captura direção
    → MSkillIndicatorSys atualiza indicador visual
    → MSkillIndicatorUtil.UseSkill(skillSlot, direction, target, drag)
    → FrameSyncUseSkill empacota em FrameCmdPkg
    → Servidor recebe e redistribui para todos os clientes
    → UseActiveSkillByFrameCmd executa a skill
```

### Classes Chave para Aimbot

| Classe | Propósito | RVA iOS |
|--------|-----------|---------|
| `MSkillIndicatorUtil.UseSkill()` | Ponto de entrada para usar skills | `0x611DCD8` |
| `MSkillBtnSysComp` | Direção do joystick, skillDirDict, skillAimObjDict | — |
| `MSkillIndicatorSys` | Gerencia indicadores de mira | — |
| `LSkillBaseSelectTarget` | Lógica de seleção de alvos | — |
| `LSkillSelectNearestTarget` | Seleciona alvo mais próximo | — |

### Estruturas de Dados

```csharp
// Direção e posição de skills
VInt3 mUseSkillDirection_;  // Direção da skill (frame command)
VInt3 mUseSkillPosition_;   // Posição da skill

// Componente de input de skill
MSkillBtnSysComp {
    Dictionary<int, Vector2> skillDirDict;           // Direção por slot
    Dictionary<int, Vector2> skillIndicatorDirDict;  // Direção do indicador
    Dictionary<int, int> skillAimObjDict;            // Objeto alvo
    bool useSkillBtnDir;                             // Usar direção do botão
}

// Auto-target existente no jogo
bool bForbidSkillAutoSelectTarget_;          // Proíbe auto-select
RepeatedField<int> forbidAutoSelectTargetSkillList_;  // Skills sem auto-select
```

### Abordagem para Aimbot (Android)

1. Hook `MSkillIndicatorUtil.UseSkill()` no RVA Android
2. Quando ativado, calcular direção do jogador → inimigo mais próximo
3. Substituir `inPosOrDir` com a direção calculada
4. Opcionalmente setar `inTarget` com entityId do inimigo

### Abordagem para Aimbot (iOS - Dylib + Dobby)

Requer:
1. macOS/VM com Xcode para compilar dylib ARM64
2. Dobby como hook engine (substitui MSHookFunction)
3. insert_dylib para injetar no IPA
4. Sideloadly para instalar

---

## 5. Features do Mod Menu Android (Hacks.h)

### Implementadas

| Feature | Toggle | Status |
|---------|--------|--------|
| Map Hack - IsVisible | `bHook_IsVisible` | ✅ Funciona |
| Map Hack - IsCampVisible | `bHook_IsCampVisible` | ✅ Funciona |
| Map Hack - CheckVisible | `bHook_CheckVisible` | ✅ Funciona |
| Drone View (Zoom) | `bDroneView` / `fDroneZoom` | ⚠️ Código do HoK, não testado em PU |
| ESP (Markers) | `bESP_Enabled` | ⚠️ Código do HoK, não testado em PU |

### Pendentes

| Feature | Prioridade | Dificuldade |
|---------|------------|-------------|
| Aimbot de Skills (Greninja etc) | Alta | Alta |
| Anti-cheat bypass Android | Alta | Média |
| ESP adaptado para PU | Média | Média |
| Drone View adaptado para PU | Baixa | Média |

---

## 6. Atualização para Nova Versão do Jogo

### Passo 1: Novo Dump

**Android:**
```
1. Editar il2cpp_zygisk/jni/game.h → GamePackageName = "jp.pokemon.pokemonunite"
2. ndk-build
3. python build_zip.py
4. adb push + magisk --install-module
5. Reboot → abrir jogo → pull dump.cs
```

**iOS:**
```
1. Extrair novo IPA
2. Usar il2cppdumper no UnityFramework
```

### Passo 2: Buscar Novas Funções

```bash
# No dump.cs:
grep "class MVisionSysUtil" dump.cs
grep "class LVisionSysUtil" dump.cs
grep "class LVisionSys " dump.cs
grep "class LBattleStatSys" dump.cs
grep "UpdateToCheckVisionStat" dump.cs
```

### Passo 3: Extrair RVAs

Cada método tem `// RVA: 0xXXXXXXX` no dump.cs. Copiar os novos valores.

### Passo 4: Atualizar

- **iOS**: Editar `patch_ios_maphack.py` com novos offsets
- **Android**: Editar `Hacks.h` / `Offsets.h` com novos RVAs

---

## 7. Ferramentas

| Ferramenta | Uso |
|-----------|-----|
| **libtool** | Testar hooks iOS em tempo real (jailbreak) |
| **il2cppdumper** | Gerar dump.cs com RVAs |
| **Zygisk-Il2CppDumper** | Dump runtime no Android (MuMu Player) |
| **ndk-build** | Compilar módulos Zygisk ARM64 |
| **Sideloadly** | Instalar IPA sem jailbreak |
| **patch_ios_maphack.py** | Patch estático iOS |
| **repack_ipa.py** | Reempacotar IPA |
| **build_module.py** | Build módulo Magisk/Zygisk |

### NDK Build

```powershell
$env:ANDROID_NDK_ROOT = "C:\Users\Victor\AppData\Local\Android\Sdk\ndk\27.0.12077973"
```

### Emulator (MuMu Player)

```
ADB: C:\Program Files\Netease\MuMuPlayer\nx_main\adb.exe
Device: 127.0.0.1:7555
Magisk: Kitsune 26.4 + Zygisk
```

---

## 8. Notas Técnicas

### Frame Sync (Lockstep)

O jogo usa sincronização de frames: ações são enviadas como `FrameCmdPkg` ao servidor e redistribuídas para todos os clientes. Skills são enviadas com `VInt3 direction` e `int targetEntityId`. Isso significa que o aimbot modifica dados que o servidor processa — não é puramente visual.

### Threads

- `M` prefix = MainThread (renderização)
- `L` prefix = LogicThread (lógica de jogo)
- As funções utility estáticas (`*Util`) são consultas seguras
- As funções de instância (`LVisionSys.*`) afetam o estado interno do sistema

### iOS vs Android

| Aspecto | iOS | Android |
|---------|-----|---------|
| Método | Patch estático + Sideload | Zygisk inline hooks |
| Map Hack | ✅ 4 funções + anti-cheat | ✅ 3 funções |
| Runtime hooks | ❌ (precisa dylib+Dobby) | ✅ (And64InlineHook) |
| Aimbot | ❌ Pendente (macOS needed) | ⚠️ Análise completa, implementação pendente |
| Mod Menu UI | ❌ Não tem | ✅ ImGui overlay |
| Anti-cheat | ✅ UpdateToCheckVisionStat → RET | ⚠️ Pendente |
| Deploy | Sideloadly | Magisk module + reboot |
