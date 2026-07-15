# Pokemon Unite - Mod Menu ImGui (iOS)

Mod menu flutuante para Pokemon Unite no iOS, com **Map Hack** e **Aimbot de skills**.
Baseado no template [34306/HuyJIT-ModMenu](https://github.com/34306/HuyJIT-ModMenu)
(ImGui + Metal + DobbyHook + resolver il2cpp). *Conteudo adaptado para o Pokemon Unite.*

- **Alvo:** `jp.pokemon.pokemonunite` / `UnityFramework` (versao 1.23.1)
- **Dispositivo:** iPhone **sem jailbreak** (sideload) — tambem funciona em jailbreak.

---

## O que faz

| Feature | Como funciona |
|---|---|
| **Map Hack** | Hooka 4 funcoes de visao (`MVisionSysUtil.IsVisible`, `LVisionSysUtil.IsCampVisible`, `LVisionSys.CheckVisible`, `LVisionSys.IsCampVisible`) para retornarem "visivel". O `CheckVisible` seta o `out bool` corretamente (evita quebrar auto-attack). |
| **Anti-Cheat** | Neutraliza `LBattleStatSys.UpdateToCheckVisionStat` (nop) para nao coletar estatisticas de visao. |
| **Aimbot** | Hooka `LSkillBaseSelectTarget.SelectTargetByDir` e forca `bIsAutoSelectTarget = true`, fazendo as skills usarem o auto-alvo do proprio jogo. |

Todos os offsets ficam em `PokemonUnite.h`. Toggles ficam no menu ImGui.

---

## Como compilar (sem precisar de Mac)

O build roda no **GitHub Actions** (runner macOS). Voce so precisa de uma conta GitHub.

1. Suba esta pasta (junto com `.github/workflows/build-ios-menu.yml`) para um repositorio GitHub.
2. Na aba **Actions** do repo, rode o workflow **"Build iOS Mod Menu (Pokemon Unite)"**
   (botao *Run workflow*), ou apenas de `git push`.
3. Ao terminar, baixe o artefato **`pokemonunite-modmenu`**. Ele contem:
   - `pumenu.dylib` -> para sideload (sem jailbreak)
   - `*.deb` -> para jailbreak

O workflow clona automaticamente o template upstream (pastas `5Toubun/`, `Esp/`, `IMGUI/`
com ImGui, backend Metal, Dobby e o resolver il2cpp) e sobrepoe os nossos arquivos.

---

## Como instalar no iPhone (sideload, Windows)

1. Instale o **Sideloadly** (https://sideloadly.io) + **iTunes da Apple** (nao o da Microsoft Store).
2. Abra o Sideloadly, arraste o IPA do Pokemon Unite
   (`jp.pokemon.pokemonunite-1.23.1.1-Decrypted.ipa`).
3. Em **Advanced options / Inject dylib**, adicione o `pumenu.dylib`.
4. Coloque seu **Apple ID**, conecte o iPhone e clique em **Start**.
5. Abra o jogo. Faca **3 dedos + 2 toques** na tela para abrir o menu.

> Conta Apple ID gratuita: o app expira em ~7 dias (basta reassinar). Com conta de dev paga: 1 ano.

### Instalar em jailbreak (alternativa)
Instale o `.deb` (Sileo/Zebra) ou `dpkg -i`.

---

## Gestos do menu
- **3 dedos, 2 toques** = abrir menu
- **2 dedos, 2 toques** = ocultar menu
- Ative os toggles **no lobby, ANTES da partida**.

---

## Atualizar para nova versao do jogo
1. Gere um novo `dump.cs` do `UnityFramework` (il2cppdumper).
2. Atualize os RVAs em `PokemonUnite.h` (as classes estao documentadas no topo do header).
3. Recompile (Actions) e reinstale.

---

## Estrutura
```
modmenu_ios_pokemonunite/
  ImGuiDrawView.mm     <- menu + hooks (Map Hack + Aimbot)  [NOSSO]
  PokemonUnite.h       <- todos os offsets/RVAs             [NOSSO]
  control              <- metadados do pacote               [NOSSO]
  Makefile             <- build (arm64, sideload)           [NOSSO]
  pumenu.plist         <- filtro de bundle (jp.pokemon...)  [NOSSO]
  5Toubun/ Esp/ IMGUI/ <- infra (baixado do upstream na CI) [UPSTREAM]
```

## Notas
- Map Hack e proven (mesmas funcoes usadas no projeto Android/iOS). O uso de **hook** (Dobby)
  em vez de patch estatico corrige o bug do `out bool` do `CheckVisible`.
- O **Aimbot** usa o auto-select nativo do jogo; se algum campeao especifico nao mirar bem,
  ajuste a estrategia em `hk_LSkill_SelectTargetByDir` (ex.: rotear para
  `LSkillSelectNearestTarget.SelectTarget` @ `0x4F5DD00`). Requer teste no dispositivo.
