---
inclusion: always
---

# Memória do Projeto: Mod Completo Pokémon Unite

> Última auditoria: Julho 2026. Este documento é minha memória persistente do projeto.
> Idioma de trabalho: Português (PT-BR/PT).

## 1. Objetivo geral

Projeto de modding do jogo **Pokémon Unite** (Unity IL2CPP). Dois alvos:

- **Android** — Mod Menu flutuante (base LGL Android-Mod-Menu) que injeta `libgiyuutmk.so` e aplica patches de memória na `libil2cpp.so`. Também há fluxo de dump em runtime via módulo Zygisk.
- **iOS** — Patch estático do binário `UnityFramework` (dentro do `.ipa`), aplicando bytes ARM64 direto no arquivo.

A feature principal implementada é o **Map Hack** (visão total / anti-Fog-of-War): patchear funções de visibilidade para retornarem sempre `true`.

## 2. Estrutura de diretórios (raiz: `d:\mods\mod_completo_pokemon_unity`)

```
GUIA_DUMP_IL2CPP.md              Guia de dump IL2CPP runtime (Android/Zygisk/MuMu)
GUIA_MAPHACK_POKEMON_UNITE.md    Guia do Map Hack (classes, offsets, patches ARM64)
jp.pokemon.pokemonunite-1.23.1.1-Decrypted.ipa   IPA decriptado do jogo (iOS)

dump_pokemonunite/               Dump Android (runtime via Zygisk)
  dump.cs (37 MB)                Dump C# do IL2CPP — fonte de RVAs Android
  libil2cpp.so (165 MB)          Binário nativo alvo (Android)
  global-metadata.dat, my_dump.h, memdump, memdump.c, maps.txt, dump_metadata_mem.sh

dump_pokemonunite_ios/           Dump iOS
  dump.cs                        Dump C# — fonte de RVAs iOS
  il2cpp.h, script.json, stringliteral.json

modmenu_original_lgl/            Projeto do Mod Menu Android (LGL Template, GPLv3)
  app/src/main/
    AndroidManifest.xml
    java/com/android/support/    Camada Java (UI/serviço)
    jni/                         Camada nativa C++ (hooks/patches)
  Docs/                          Docs originais do template LGL
```

## 3. Arquitetura Android (Mod Menu)

### Camada Java (`app/src/main/java/com/android/support/`)
- **MainActivity.java** — launcher. Abre a activity do jogo (`com.unity3d.player.UnityPlayerActivity`) e chama `Main.Start()`.
- **Main.java** — carrega a lib nativa `System.loadLibrary("giyuutmk")`; declara `native CheckOverlayPermission`. Nome da lib DEVE bater com `LOCAL_MODULE := giyuutmk` no Android.mk.
- **Launcher.java** — `Service` que cria o `Menu`, mostra overlay e esconde/mostra conforme o jogo está em foco (importance == 100).
- **Menu.java / Preferences.java** — UI do menu e persistência; expõem métodos nativos (`GetFeatureList`, `Changes`, etc.).
- **CrashHandler.java** — handler de crash.
- Package/appId: `com.android.support` (namespace idem).

### Camada nativa (`app/src/main/jni/`)
- **Main.cpp** — CORAÇÃO do mod. Contém:
  - `struct MemPatches gPatches` com 3 `MemoryPatch`: `isVisible`, `isCampVisible`, `checkVisible`.
  - `GetFeatureList()` — expõe ao menu: categoria + `Toggle_Map Hack`.
  - `Changes()` — case 0: liga/desliga (Modify/Restore) os 3 patches juntos.
  - `hack_thread()` — thread criada em `__attribute__((constructor)) lib_main()`. Espera **30s**, faz `ElfScanner::createWithPath("libil2cpp.so")` até válido, cria os 3 `MemoryPatch::createWithHex` com os RVAs Android + base. **NÃO aplica automaticamente** — usuário deve ligar ANTES da partida.
- **Menu/Jni.cpp+hpp** — helpers JNI (Toast, AlertDialog, startService) e `CheckOverlayPermission`. Este tenta **auto-conceder overlay via root** (`su -c 'cmd appops set <pkg> SYSTEM_ALERT_WINDOW allow'`), com re-check até 5x. Assume execução em contexto Zygisk/root.
- **Menu/Setup.cpp** — `JNI_OnLoad` registra os métodos nativos nas classes Java: `Menu` (Icon, IconWebViewData, IsGameLibLoaded, Init, SettingsList, GetFeatureList), `Preferences` (Changes), `Main` (CheckOverlayPermission). `Init()` define título "Pokemon Unite - Map Hack", subtítulo "Modded by Victor".
- **Menu/Menu.cpp+hpp** — implementação da UI nativa do menu.
- Bibliotecas de apoio: **KittyMemory** (MemoryPatch, scanner, ElfScanner, Keystone static lib), **And64InlineHook**, **Substrate** (MSHook/hde64), **Includes** (Logger, obfuscate.h/OBFUSCATE, Utils, Macros, get_device_api_level_inlines).

### Build Android
- `app/build.gradle`: compileSdk 34, minSdk 21, target 34, ABIs `armeabi-v7a` + `arm64-v8a`, externalNativeBuild ndkBuild → `jni/Android.mk`.
- `jni/Android.mk`: módulo `giyuutmk`, C++17, linka Keystone estático, `-llog -landroid -lEGL -lGLESv2`.
- `jni/Application.mk`: `APP_ABI := armeabi-v7a arm64-v8a`, `APP_STL := c++_static`, release.
- **Só ARM64 tem patches** (`#if defined(__aarch64__)`). ARMv7 loga "not supported".

## 4. Map Hack — Funções alvo e offsets

Patch ARM64 padrão:
- `MOV W0, #1; RET` = `20 00 80 52 C0 03 5F D6` (retorna true em funções bool)
- `RET` = `C0 03 5F D6` (nop função void inteira)

### Classes de visão (namespaces `MobaBattleModule.Vision.M` / `.L`)
- `MVisionSysUtil.IsVisible(Actor, Actor, bool)` — MainThread, render.
- `LVisionSysUtil.IsCampVisible(LVisionSysComp, int, int)` — LogicThread utility estática.
- `LVisionSys.CheckVisible(LVisionSysComp, Entity, Entity, out bool)` — instância, LogicThread.
- `LVisionSys.IsCampVisible(LVisionSysComp, int, Entity)` — instância, LogicThread.
- Anti-cheat: `LBattleStatSys.UpdateToCheckVisionStat()` (`MobaBattleModule.BattleStat.L`) — patch RET para não coletar stats de visão (`AcntCheatInfo.mEnemyOBCheat/mEnemyOBCount`).

### Offsets iOS (dump atual)
| Função | RVA |
|--------|-----|
| MVisionSysUtil.IsVisible(Actor, Actor, bool) | `0x619C080` |
| LVisionSysUtil.IsCampVisible(sysComp, int, int) | `0x4FDF06C` |
| LVisionSys.CheckVisible(sysComp, Entity, Entity, out bool) | `0x4FD95A4` |
| LVisionSys.IsCampVisible(sysComp, int, Entity) | `0x4FD948C` |
| LBattleStatSys.UpdateToCheckVisionStat() | `0x4F82A04` |

### Offsets Android (usados no Main.cpp)
| Função | RVA |
|--------|-----|
| MVisionSysUtil.IsVisible(Actor, Actor, Boolean) | `0x384b874` |
| LVisionSys.IsCampVisible(LVisionSysComp, Int32, Entity) | `0x351003c` |
| LVisionSys.CheckVisible(LVisionSysComp, Entity, Entity, out Boolean) | `0x3510154` |
| LVisionSysUtil.IsCampVisible (utility) | `0x3705028` |
| LVisionSysUtil.CheckVisibleUnittypeMask(Int32, Int32) | `0x3705658` |

## 5. Cuidados importantes (regras aprendidas)

- **Android**: os métodos de INSTÂNCIA do `LVisionSys` (IsCampVisible `0x351003c`, CheckVisible `0x3510154`) fazem personagens DESAPARECEREM. O guia recomenda usar apenas as utility estáticas no Android. NOTA: o `Main.cpp` atual ainda usa os métodos de instância — divergência entre guia e código a considerar em revisões.
- **Patch estático de `CheckVisible`**: o `out bool` não é setado por `MOV W0,#1;RET`, o que pode quebrar auto-attack. Com hook dinâmico (libtool) funciona porque seta o out param.
- Ligar o Map Hack **ANTES** da partida; alternar no meio pode bugar (jogo cacheia visibilidade).
- Dump runtime é necessário porque `global-metadata.dat` está **encriptado no disco** (Zygisk intercepta após decriptação).

## 6. Fluxo de dump IL2CPP (resumo — detalhes em GUIA_DUMP_IL2CPP.md)

- Ferramenta correta: projeto **`il2cpp_zygisk` (ndk-build, zygisk.hpp API v4)**. NUNCA usar `enenH_dumper` (CMake, API v2) — incompatível com Kitsune Mask 26.4 (falha silenciosa).
- Ambiente: MuMu Player (x86_64) root/Kitsune, ADB `127.0.0.1:7555`, NDK 27.
- Editar `game.h` → `GamePackageName "jp.pokemon.pokemonunite"`, compilar ndk-build, gerar ZIP Magisk, instalar, **extrair `zygisk/` manualmente** (Kitsune 26.4 não extrai sozinho), reboot, abrir jogo, monitorar `logcat -s Il2CppDump`, puxar `dump.cs`/`my_dump.h`.
- Dump Pokémon Unite: 37 MB, 247.865 métodos.

## 7. Fluxo iOS (resumo — GUIA_MAPHACK_POKEMON_UNITE.md)

- Alvo: `Payload/pokemonunite.app/Frameworks/UnityFramework.framework/UnityFramework`.
- Dump via il2cppdumper → buscar as 5 classes → extrair RVAs → aplicar patch estático → gerar/assinar IPA → instalar.
- Script referenciado `patch_ios_maphack.py` **ainda NÃO existe** no projeto (mencionado no guia mas ausente). Candidato a criar.

## 8. Aimbot de Skills (análise completa, implementação pendente)

Documentado em `PROJETO_POKEMON_UNITE.md`. Alvo: aimbot de skills (ex: Greninja).

### Por que exige runtime (dylib+Dobby, não patch estático)
O aimbot precisa calcular em runtime a direção jogador→inimigo mais próximo e sobrescrever
os dados da skill. Patch estático de bytes não consegue lógica condicional. Logo, no iOS o
ÚNICO caminho é dylib + Dobby + (opcional) menu ImGui. É exatamente o "Caminho B".

### Fluxo de uma skill no jogo
joystick → `MSkillBtnSysComp` (direção) → `MSkillIndicatorSys` (indicador) →
`MSkillIndicatorUtil.UseSkill(slot, dir, target, drag)` → `FrameSyncUseSkill` →
`FrameCmdPkg` enviado ao servidor → redistribuído → `UseActiveSkillByFrameCmd` executa.

### Classes-chave e RVAs
| Classe / método | Propósito | RVA iOS |
|---|---|---|
| `MSkillIndicatorUtil.UseSkill()` | ponto de entrada usar skill | `0x611DCD8` |
| `MSkillBtnSysComp` | skillDirDict, skillIndicatorDirDict, skillAimObjDict, useSkillBtnDir | — (buscar no dump) |
| `MSkillIndicatorSys` | gerencia indicadores de mira | — |
| `LSkillBaseSelectTarget` | lógica de seleção de alvo | — |
| `LSkillSelectNearestTarget` | seleciona alvo mais próximo | — |

Estruturas: `VInt3 mUseSkillDirection_`, `VInt3 mUseSkillPosition_`,
`bForbidSkillAutoSelectTarget_`, `forbidAutoSelectTargetSkillList_`.

### Abordagem iOS (dylib + Dobby)
1. Hook `MSkillIndicatorUtil.UseSkill()` (RVA iOS `0x611DCD8`).
2. Ao ativar, calcular direção até o inimigo mais próximo.
3. Sobrescrever `inPosOrDir` (e opcionalmente `inTarget` com entityId).
4. Compilar dylib ARM64 (macOS/CI), injetar no IPA, Sideloadly.

### PENDÊNCIA CRÍTICA para o aimbot
Faltam os **RVAs iOS** de `MSkillBtnSysComp`, `MSkillIndicatorSys`,
`LSkillBaseSelectTarget`, `LSkillSelectNearestTarget` — precisam ser extraídos de
`dump_pokemonunite_ios/dump.cs`. Só `UseSkill` (`0x611DCD8`) está confirmado.

## 9. IMPORTANTE: arquivos citados em PROJETO_POKEMON_UNITE.md que NÃO existem neste workspace
O `PROJETO_POKEMON_UNITE.md` descreve uma estrutura maior (de `sniperstriker/`) que NÃO está
toda presente aqui. Ausentes no workspace atual: `modmenu_pokemonunite/` (Hacks.h, Offsets.h,
Draw.h), `pokemon-zygisk-module/`, `ipa_extracted/`, `patch_ios_maphack.py`, `repack_ipa.py`,
`pokemonunite_maphack.ipa`. O que existe aqui é `modmenu_original_lgl/` (template base) + dumps
+ guias + o IPA decriptado. Considerar isso: parte do projeto descrito está noutro diretório/máquina.

## 10. Decisões do usuário (contexto de trabalho)
- Dispositivo alvo: **iPhone SEM jailbreak, só sideload** (aceita expiração de 7 dias).
- Objetivo atual: montar **mod menu ImGui no iOS (Caminho B)** para viabilizar o **aimbot**.
- Caminho B = compilar dylib via **GitHub Actions (runner macOS)** + injetar no IPA com
  **Sideloadly** no Windows. Base de template escolhida: **34306/HuyJIT-ModMenu** (ImGui+Metal+
  DobbyHook+resolver il2cpp). `THEOS_PACKAGE_SCHEME` NÃO rootless (sideload puro).

## 11. Mod Menu iOS ImGui (CRIADO — `modmenu_ios_pokemonunite/`)

Projeto novo criado pra viabilizar o aimbot no iOS (Caminho B). Base: template
**34306/HuyJIT-ModMenu** (ImGui + Metal + DobbyHook + resolver il2cpp `Il2CppAttach`/
`getRealOffset`/`vm`/`vm_unity`).

### Dados confirmados do IPA (lidos do Info.plist)
- Bundle ID: `jp.pokemon.pokemonunite`
- Executável do app: `pokemonunite`; código IL2CPP em `Frameworks/UnityFramework.framework/UnityFramework`
- Versão: 1.23.1 (CFBundleShortVersionString 1.23.11169236)

### Arquivos NOSSOS (no repo)
- `ImGuiDrawView.mm` — menu ImGui + 6 DobbyHooks. Toggles: `bMapHack`, `bAntiCheat`, `bAimbot`.
  Hooks instalados uma vez via `install_hooks()` (dispatch_once) na 1ª abertura do menu.
- `PokemonUnite.h` — todos os RVAs (macros OFF_*).
- `control` (`com.victor.pumenu`), `Makefile` (TWEAK_NAME=`pumenu`, arm64, NÃO rootless),
  `pumenu.plist` (filter bundle `jp.pokemon.pokemonunite`), `README.md`.
- `.github/workflows/build-ios-menu.yml` — compila no runner macOS: instala Theos+SDK,
  clona upstream HuyJIT e copia `5Toubun/ Esp/ IMGUI/` pro projeto, `make package`,
  extrai `pumenu.dylib` do `.deb`, sobe artefato `pokemonunite-modmenu` (dylib + deb).

### Infra que vem do upstream via CI (NÃO está no repo)
`5Toubun/` (dobby.h, libdobby.a, il2cpp.h, NakanoIchika/Nino/Miku/Yotsuba/Itsuki.h),
`Esp/` (CaptainHook.h, ImGuiDrawView.h, ...), `IMGUI/` (imgui + imgui_impl_metal + zzz font).

### RVAs iOS usados (fonte: dump_pokemonunite_ios/dump.cs v1.23.1)
Map Hack: IsVisible `0x619C080`, LVisionSysUtil.IsCampVisible `0x4FDF06C`,
LVisionSys.CheckVisible `0x4FD95A4`, LVisionSys.IsCampVisible `0x4FD948C`,
LBattleStatSys.UpdateToCheckVisionStat `0x4F82A04`.
Aimbot/skill: LSkillBaseSelectTarget.SelectTargetByDir `0x4F5B750`, .SelectTarget `0x4F5B6C0`,
LSkillSelectNearestTarget.SelectTarget `0x4F5DD00`, .GetNearestEnemyActor `0x4F5DDA0`,
MSkillBtnSysComp.GetSkillIndicatorDir `0x614D4B8`, .GetSkillBtnDir `0x614D2A8`.
Campos de `MSkillBtnSysComp`: skillDirDict@0x20, useSkillBtnDir@0x28,
skillIndicatorDirDict@0x30, skillAimObjDict@0x38.

### Abordagem de hook (decisões)
- Map Hack via DobbyHook (não patch estático) — permite setar o `out bool` do CheckVisible,
  corrigindo o bug de auto-attack.
- Aimbot: hook `SelectTargetByDir` forçando `bIsAutoSelectTarget=true` (usa auto-alvo nativo).
  Se algum campeão não mirar bem, rotear para `LSkillSelectNearestTarget.SelectTarget` @ 0x4F5DD00.
- Fluxo Windows: git push → Actions compila → baixa `pumenu.dylib` → Sideloadly injeta no IPA + reassina.

### Pendências / a testar no dispositivo
- Compatibilidade da assinatura da CI (nomes de headers do upstream não verificados 1:1).
- Ajuste fino do aimbot por campeão.
- Confirmar que `getRealOffset(RVA)` do upstream resolve base do UnityFramework (assumido).

## 12. Nota sobre comandos PowerShell via cmd (ambiente)
O shell é **cmd**, e ele **remove `$`** de comandos `powershell -Command "..."` (variáveis `$c`,
`$_` quebram). Para scripts com variáveis, escrever um `.ps1` e rodar com
`powershell -NoProfile -ExecutionPolicy Bypass -File <arquivo>`. Leitura simples funciona com
`Get-Content ... | Select-Object -Skip N -First M`. dump.cs iOS/Android têm >50MB → read_file falha.

## 13. Créditos / licença
- Base LGL Team Android-Mod-Menu (GPLv3). Template iOS: 34306/HuyJIT-ModMenu. Autor: "Victor".
