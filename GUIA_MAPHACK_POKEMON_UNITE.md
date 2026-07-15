# Pokemon Unite - Map Hack Guide (iOS)

## Resumo

O Map Hack funciona patcheando funções de visibilidade para sempre retornarem `true`, fazendo com que todos os inimigos fiquem visíveis no minimapa e no jogo, mesmo quando estão na Fog of War.

---

## Arquitetura do Sistema de Visão

O jogo tem duas threads principais:
- **MainThread (M)** — renderização, UI, efeitos visuais
- **LogicThread (L)** — lógica de jogo, cálculo de visão, sincronização

### Classes Envolvidas

| Classe | Thread | Tipo | Função |
|--------|--------|------|--------|
| `MVisionSysUtil` | Main | static utility | Consulta de visibilidade para renderização |
| `LVisionSysUtil` | Logic | static utility | Consulta de visibilidade para lógica |
| `LVisionSys` | Logic | instância (sistema) | Update loop de visão, seta camp/personal visibility |
| `MVisionSys` | Main | instância (sistema) | Sync de visão para render |

---

## Funções para Patchear (Confirmadas Funcionando)

### 1. MVisionSysUtil.IsVisible (MainThread)

```
Namespace: MobaBattleModule.Vision.M
Class: MVisionSysUtil (static)
Method: public static bool IsVisible(Actor actor, Actor checkedActor, bool checkAntiInvisible = True)
```

**O que faz:** Verifica se um ator é visível para outro ator. Usada pela renderização para decidir se desenha o inimigo na tela.

**Como encontrar em atualizações:**
- Procurar por `MVisionSysUtil` no dump
- É a sobrecarga que recebe `(Actor, Actor, bool)` (não a de int)
- Tem `[IDTagAttribute]` decorador
- Fica no namespace `MobaBattleModule.Vision.M`

---

### 2. LVisionSysUtil.IsCampVisible (LogicThread utility)

```
Namespace: MobaBattleModule.Vision.L
Class: LVisionSysUtil
Method: public static bool IsCampVisible(LVisionSysComp sysComp, int srcCamp, int dstId)
```

**O que faz:** Verifica se uma entidade (`dstId`) é visível para um camp (`srcCamp`). Utility estática usada para consultas de visibilidade na lógica.

**Como encontrar em atualizações:**
- Procurar por `LVisionSysUtil` no dump (NÃO confundir com `LVisionSys`)
- É a `IsCampVisible` com parâmetros `(LVisionSysComp, int, int)`
- É **estática** (static)

---

### 3. LVisionSys.CheckVisible (LogicThread instância)

```
Namespace: MobaBattleModule.Vision.L
Class: LVisionSys
Method: private bool CheckVisible(LVisionSysComp inVisionSysComp, Entity srcEntity, Entity dstEntity, out bool visible)
```

**O que faz:** Verifica visibilidade entre duas entidades no loop de update. O `out bool visible` é o resultado real. Retorno `bool` indica se o caso foi processado.

**Como encontrar em atualizações:**
- Procurar por `class LVisionSys` (é a classe de instância, não utility)
- Método `CheckVisible` com parâmetros `(LVisionSysComp, Entity, Entity, out bool)`
- É **privado** e de **instância**

**NOTA:** No patch binário com `MOV W0, #1; RET`, o parâmetro `out bool` NÃO é setado. No libtool/hook dinâmico isso funciona porque o hook seta o out parameter. Em patch estático, pode quebrar auto-attack — mas nos testes com libtool funcionou.

---

### 4. LVisionSys.IsCampVisible (LogicThread instância)

```
Namespace: MobaBattleModule.Vision.L
Class: LVisionSys
Method: private bool IsCampVisible(LVisionSysComp inVisionSysComp, int camp, Entity dstEntity)
```

**O que faz:** Verifica se uma entidade é visível para um camp específico. Usada internamente no loop de update de visão.

**Como encontrar em atualizações:**
- Mesma classe `LVisionSys`
- Método `IsCampVisible` com parâmetros `(LVisionSysComp, int, Entity)`
- É **privado** e de **instância**
- NÃO confundir com `LVisionSysUtil.IsCampVisible` (que tem params diferentes)

---

## Anti-Cheat: UpdateToCheckVisionStat

### 5. LBattleStatSys.UpdateToCheckVisionStat

```
Namespace: MobaBattleModule.BattleStat.L
Class: LBattleStatSys
Method: private void UpdateToCheckVisionStat()
```

**O que faz:** Coleta estatísticas de visão a cada frame. Conta quantos inimigos "fora da visão" você está vendo (alimenta `mEnemyOBCount` e `mEnemyOBCheat` na classe `AcntCheatInfo`). Se o contador acumula muito, pode levar a ban.

**Patch:** RET imediato (nop a função inteira). Impede qualquer coleta de dados suspícitos de visão.

**Como encontrar em atualizações:**
- Procurar por `LBattleStatSys` no dump
- Método `UpdateToCheckVisionStat` (void, sem parâmetros)
- Relacionado a `AcntCheatInfo.mEnemyOBCheat`

---

## Patches ARM64

```
MOV W0, #1; RET = 20 00 80 52 C0 03 5F D6   (return true - para funções bool)
RET             = C0 03 5F D6                 (return void - nop função inteira)
```

---

## Offsets da Versão Atual (iOS dump)

| Função | RVA |
|--------|-----|
| MVisionSysUtil.IsVisible(Actor, Actor, bool) | `0x619C080` |
| LVisionSysUtil.IsCampVisible(sysComp, int, int) | `0x4FDF06C` |
| LVisionSys.CheckVisible(sysComp, Entity, Entity, out bool) | `0x4FD95A4` |
| LVisionSys.IsCampVisible(sysComp, int, Entity) | `0x4FD948C` |
| LBattleStatSys.UpdateToCheckVisionStat() | `0x4F82A04` |

---

## Offsets da Versão Atual (Android - Zygisk mod)

| Função | RVA |
|--------|-----|
| MVisionSysUtil.IsVisible(Actor, Actor, bool) | `0x384b874` |
| LVisionSysUtil.IsCampVisible(sysComp, Int32, Int32) | `0x3705028` |
| LVisionSysUtil.CheckVisibleUnittypeMask(Int32, Int32) | `0x3705658` |

**NOTA Android:** Os métodos de instância do `LVisionSys` (IsCampVisible @ `0x351003c`, CheckVisible @ `0x3510154`) fazem personagens desaparecerem no Android. Usar apenas as utility estáticas.

---

## Como Encontrar os Offsets em Nova Versão

### Passo 1: Dump
- **iOS:** Extrair UnityFramework do IPA, usar il2cppdumper
- **Android:** Usar Zygisk Il2CppDumper (runtime dump)

### Passo 2: Buscar no dump.cs

```
# MVisionSysUtil.IsVisible
grep "class MVisionSysUtil" dump.cs
# Procurar: public static bool IsVisible(Actor actor, Actor checkedActor, bool checkAntiInvisible

# LVisionSysUtil.IsCampVisible
grep "class LVisionSysUtil" dump.cs
# Procurar: public static bool IsCampVisible(LVisionSysComp sysComp, int srcCamp, int dstId)

# LVisionSys.CheckVisible
grep "class LVisionSys " dump.cs
# Procurar: private bool CheckVisible(LVisionSysComp inVisionSysComp, Entity srcEntity, Entity dstEntity, out bool visible)

# LVisionSys.IsCampVisible
# Na mesma classe LVisionSys:
# Procurar: private bool IsCampVisible(LVisionSysComp inVisionSysComp, int camp, Entity dstEntity)

# Anti-cheat
grep "class LBattleStatSys" dump.cs
# Procurar: private void UpdateToCheckVisionStat()
```

### Passo 3: Extrair RVA
Cada método tem um comentário `// RVA: 0xXXXXXXX` acima da declaração. Esse é o offset no binário.

---

## Funções que NÃO usar / Cuidados

| Função | Problema |
|--------|----------|
| `LVisionSys.CheckVisible` (patch estático sem setar `out`) | Pode quebrar auto-attack se o `out bool` não for setado |
| `LVisionSysUtil.CheckVisibleUnittypeMask` (iOS) | Pode ser redundante se já tem as outras |
| Métodos de instância `LVisionSys` no Android | Fazem personagens desaparecerem |

---

## Binário Alvo

- **iOS:** `Payload/pokemonunite.app/Frameworks/UnityFramework.framework/UnityFramework`
- **Android:** `libil2cpp.so` (dentro do APK em `lib/arm64-v8a/`)

---

## Ferramentas

- **libtool** — para testar hooks em tempo real no iOS (jailbreak)
- **il2cppdumper** — para gerar o dump.cs com RVAs
- **Python script** (`patch_ios_maphack.py`) — para patch estático do binário iOS
- **Zygisk module** — para hooks inline no Android

---

## Resumo Rápido para Nova Versão

1. Fazer novo dump
2. Buscar `MVisionSysUtil`, `LVisionSysUtil`, `LVisionSys`, `LBattleStatSys`
3. Copiar os novos RVAs das 5 funções
4. Atualizar `patch_ios_maphack.py` com os novos offsets
5. Rodar o script → gerar IPA → assinar → instalar
