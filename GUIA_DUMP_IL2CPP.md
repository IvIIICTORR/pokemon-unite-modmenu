# Guia Completo: Dump IL2CPP em Runtime (Android/MuMu Player)

> **Última atualização**: Julho 2026
> **Testado com**: Kitsune Mask 26.4 + MuMu Player (x86_64) + NDK 27

---

## Pré-requisitos

- **MuMu Player** com root (Kitsune Mask / Magisk)
- **Zygisk habilitado** no Kitsune Mask
- **ADB**: `C:\Program Files\Netease\MuMuPlayer\nx_main\adb.exe`
- **Device**: `127.0.0.1:7555`
- **Android NDK**: `C:\Users\Victor\AppData\Local\Android\Sdk\ndk\27.0.12077973`
- **Projeto base**: `C:\Users\Victor\Desktop\MODs\sniperstriker\dump_honorofkings\il2cpp_zygisk\`

---

## AVISO CRÍTICO

### Usar APENAS o projeto `il2cpp_zygisk` (ndk-build)

**NUNCA** usar o projeto CMake `enenH_dumper`. Ele tem `zygisk.hpp` API v2 que é **incompatível** com Kitsune Mask 26.4 (que exige API v4). O módulo compila sem erros mas é **silenciosamente ignorado** pelo Zygisk — não carrega, não loga, não funciona. Nenhum erro aparece.

O projeto `il2cpp_zygisk` usa ndk-build e tem o `zygisk.hpp` correto (API v4, copiado do zygisk-module do Sniper Strike).

| Projeto | Build System | zygisk.hpp | Funciona? |
|---------|-------------|------------|-----------|
| `enenH_dumper/` (CMake) | CMake + Ninja | API v2 | **NÃO** |
| `il2cpp_zygisk/` (ndk-build) | ndk-build | API v4 | **SIM** |

---

## Passo a Passo

### 1. Identificar o jogo

Conectar ao emulador e ver qual jogo está rodando:

```powershell
$adb = "C:\Program Files\Netease\MuMuPlayer\nx_main\adb.exe"
& $adb connect 127.0.0.1:7555
& $adb -s 127.0.0.1:7555 root
& $adb -s 127.0.0.1:7555 shell "dumpsys activity top | grep ACTIVITY | head -5"
```

Anotar o **package name** (ex: `jp.pokemon.pokemonunite`, `com.levelinfinite.sgameGlobal`).

### 2. Confirmar que é Unity IL2CPP

```powershell
# Pegar PID do jogo
& $adb -s 127.0.0.1:7555 shell "pidof <PACKAGE_NAME>"

# Verificar se libil2cpp.so está carregada
& $adb -s 127.0.0.1:7555 shell "cat /proc/<PID>/maps | grep libil2cpp | head -5"
```

Se aparecer `libil2cpp.so` mapeada, é Unity IL2CPP. Se não, pode ser Mono ou outro engine.

### 3. Editar o `game.h`

Arquivo: `C:\Users\Victor\Desktop\MODs\sniperstriker\dump_honorofkings\il2cpp_zygisk\jni\game.h`

```c
#define GamePackageName "jp.pokemon.pokemonunite"  // <-- TROCAR AQUI
```

### 4. Compilar com ndk-build

```powershell
$ndk = "C:\Users\Victor\AppData\Local\Android\Sdk\ndk\27.0.12077973"
$proj = "C:\Users\Victor\Desktop\MODs\sniperstriker\dump_honorofkings\il2cpp_zygisk"

# Limpar builds antigos
Remove-Item -Recurse -Force "$proj\libs\*" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "$proj\obj\*" -ErrorAction SilentlyContinue

# Compilar
$env:ANDROID_NDK_ROOT = $ndk
& "$ndk\ndk-build.cmd" NDK_PROJECT_PATH="$proj" NDK_LIBS_OUT="$proj\libs" NDK_OUT="$proj\obj"
```

Deve mostrar `Install` para arm64-v8a, armeabi-v7a, x86, x86_64.

### 5. Gerar o ZIP do módulo Magisk

Editar `build_zip.py` se necessário (nome do output e module id), depois:

```powershell
cd $proj
python build_zip.py
```

Ou criar manualmente o ZIP com esta estrutura:

```
module.prop
META-INF/com/google/android/updater-script   → conteúdo: #MAGISK
META-INF/com/google/android/update-binary    → script de instalação padrão
zygisk/arm64-v8a.so
zygisk/armeabi-v7a.so
zygisk/x86.so
zygisk/x86_64.so
```

### 6. Instalar o módulo

```powershell
$adb = "C:\Program Files\Netease\MuMuPlayer\nx_main\adb.exe"
$zip = "$proj\Il2CppDumper-NomeDoJogo.zip"

# Push e instalar
& $adb -s 127.0.0.1:7555 push $zip /data/local/tmp/dumper.zip
& $adb -s 127.0.0.1:7555 shell "magisk --install-module /data/local/tmp/dumper.zip"
```

### 7. IMPORTANTE: Extrair zygisk/ manualmente

O `magisk --install-module` no Kitsune 26.4 do MuMu **NÃO extrai** o diretório `zygisk/` automaticamente. É preciso fazer manual:

```powershell
# Verificar o ID do módulo (olhar no module.prop)
& $adb -s 127.0.0.1:7555 shell "ls /data/adb/modules/"

# Verificar se zygisk/ está vazio
& $adb -s 127.0.0.1:7555 shell "ls /data/adb/modules/<MODULE_ID>/zygisk/ 2>/dev/null"

# Se estiver vazio ou não existir, extrair manualmente:
& $adb -s 127.0.0.1:7555 shell "mkdir -p /data/adb/modules/<MODULE_ID>/zygisk && cd /data/local/tmp && unzip -o dumper.zip 'zygisk/*' -d /data/adb/modules/<MODULE_ID>/ && chmod -R 755 /data/adb/modules/<MODULE_ID>/zygisk/"

# Confirmar que os .so estão lá
& $adb -s 127.0.0.1:7555 shell "ls -la /data/adb/modules/<MODULE_ID>/zygisk/"
```

Deve mostrar 4 ficheiros .so (arm64-v8a, armeabi-v7a, x86, x86_64).

### 8. Reboot e abrir o jogo

```powershell
& $adb -s 127.0.0.1:7555 shell reboot
# Esperar ~40 segundos
Start-Sleep 40
& $adb connect 127.0.0.1:7555
& $adb -s 127.0.0.1:7555 root

# Abrir o jogo
& $adb -s 127.0.0.1:7555 shell "monkey -p <PACKAGE_NAME> -c android.intent.category.LAUNCHER 1"
```

### 9. Monitorar o dump

```powershell
# Esperar 30-120 segundos dependendo do tamanho da libil2cpp.so
Start-Sleep 30

# Verificar logs
& $adb -s 127.0.0.1:7555 shell "logcat -d -s Il2CppDump | tail -20"
```

Sequência esperada de logs:
```
Il2CppDump: detect game: <package>
Il2CppDump: hack thread: <tid>
Il2CppDump: api level: 32
Il2CppDump: JNI_GetCreatedJavaVMs <addr>
Il2CppDump: lib dir <path>
Il2CppDump: nb <addr>                          ← NativeBridge (emulador x86)
Il2CppDump: arm handle <addr>
Il2CppDump: libil2cpp.so path <path>
Il2CppDump: il2cpp_handle: <addr>
Il2CppDump: il2cpp_base: <addr>
Il2CppDump: Waiting for il2cpp_init...
Il2CppDump: dumping...
Il2CppDump: Version greater than 2018.3
Il2CppDump: write dump file                    ← QUASE PRONTO
Il2CppDump: dump done!                         ← SUCESSO
```

### 10. Puxar os ficheiros de dump

```powershell
$outDir = "C:\Users\Victor\Desktop\MODs\sniperstriker\dump_<NOME_DO_JOGO>"
New-Item -ItemType Directory -Force -Path $outDir

& $adb -s 127.0.0.1:7555 pull "/data/data/<PACKAGE_NAME>/files/dump.cs" "$outDir\dump.cs"
& $adb -s 127.0.0.1:7555 pull "/data/data/<PACKAGE_NAME>/files/my_dump.h" "$outDir\my_dump.h"
```

### 11. (Opcional) Puxar a libil2cpp.so do disco

Se a `libil2cpp.so` não estiver encriptada no disco (verificar se tem header ELF `7F 45 4C 46`):

```powershell
# Encontrar o path
& $adb -s 127.0.0.1:7555 shell "cat /proc/<PID>/maps | grep libil2cpp | head -1"

# Pull
& $adb -s 127.0.0.1:7555 pull "<PATH_COMPLETO>/libil2cpp.so" "$outDir\libil2cpp.so"
```

### 12. Restaurar o game.h

Depois de terminar, restaurar o `game.h` para o valor padrão:

```c
#define GamePackageName "com.levelinfinite.sgameGlobal"
```

---

## Troubleshooting

### Nenhum log aparece (`logcat -s Il2CppDump` vazio)

1. **Verificar se usou o projeto certo** → DEVE ser `il2cpp_zygisk` (ndk-build), NÃO `enenH_dumper` (CMake)
2. **Verificar se os .so estão no zygisk/** → `ls /data/adb/modules/<ID>/zygisk/` deve ter 4 .so
3. **Verificar se fez reboot** → Zygisk só carrega módulos no boot
4. **Verificar se o Zygisk está ativo** → No Magisk Manager, Zygisk deve estar ON
5. **Verificar conflito de módulos** → Se outro módulo Zygisk crashar, pode afetar

### "Metadata file not found or encrypted" (Il2CppDumper PC)

O `global-metadata.dat` está encriptado no disco. **Isso é normal** — por isso usamos dump em runtime (Zygisk intercepta DEPOIS da decriptação).

### Il2CppDumper PC funciona

Se o metadata NÃO estiver encriptado (magic `AF 1B B1 FA`), pode usar o Il2CppDumper PC diretamente:

```powershell
& "...\Il2CppDumper\Il2CppDumper.exe" libil2cpp.so global-metadata.dat output_dir
```

### Dump demora muito (>5 min)

Normal para jogos grandes. A `libil2cpp.so` do Pokemon Unite tem 165MB e demorou ~2.5 minutos. Verificar progresso com `logcat -d -s Il2CppDump | tail -5`.

### Jogo crasha ao abrir

O dump pode interferir se o jogo tiver anti-cheat agressivo. Tente:
- Desabilitar outros módulos Magisk
- Verificar se o anti-cheat detecta Magisk (usar MagiskHide/DenyList)

---

## Dumps Realizados

| Jogo | Package | dump.cs | Métodos | Data |
|------|---------|---------|---------|------|
| Honor of Kings | `com.levelinfinite.sgameGlobal` | 16.8 MB | 354,275 | Jul 2026 |
| Pokemon Unite | `jp.pokemon.pokemonunite` | 37.0 MB | 247,865 | Jul 2026 |

---

## Estrutura de Ficheiros

```
dump_honorofkings/
├── il2cpp_zygisk/           ← PROJETO CORRETO (ndk-build, zygisk.hpp API v4)
│   ├── jni/
│   │   ├── Android.mk
│   │   ├── Application.mk
│   │   ├── game.h           ← EDITAR PACKAGE NAME AQUI
│   │   ├── hack.cpp
│   │   ├── il2cpp_dump.cpp
│   │   ├── main.cpp
│   │   ├── zygisk.hpp       ← API v4 (CORRETO para Kitsune 26.4)
│   │   └── xdl/
│   ├── libs/                ← Output ndk-build
│   └── build_zip.py         ← Gera o ZIP do módulo
│
├── enenH_dumper/             ← PROJETO ERRADO (CMake, zygisk.hpp API v2) ⚠️ NÃO USAR
│   └── ...
│
└── Il2CppDumper/             ← Dumper PC (só funciona se metadata não for encriptado)
    └── Il2CppDumper.exe
```
