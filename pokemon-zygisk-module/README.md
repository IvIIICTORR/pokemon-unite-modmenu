# Sniper Strike Mod Menu - Zygisk Module

Módulo Zygisk para Kitsune Mask que injeta o mod menu no APK **original** do Sniper Strike (Play Store).

## Requisitos

- **Emulador/Device** com Kitsune Mask instalado
- **Zygisk** ativado nas configurações do Kitsune Mask
- **Sniper Strike** instalado da Play Store (`com.mgs.sniper1`)
- Para compilar: Android SDK com NDK + Java JDK

## Como compilar

```bash
cd zygisk-module
python build_module.py
```

O script vai:
1. Compilar o loader Zygisk (`zygisk_loader.so`)
2. Compilar a lib do mod menu (`libgiyuutmk.so`) do lglteam4.0
3. Compilar as classes Java em `classes.dex`
4. Empacotar tudo em `output/SniperStrikeMod.zip`

## Como instalar

1. Copie `output/SniperStrikeMod.zip` para o emulador/device
2. Abra o **Kitsune Mask**
3. Vá em **Módulos** → **Instalar do armazenamento**
4. Selecione o ZIP
5. **Reinicie** o emulador/device
6. Abra o Sniper Strike normalmente pela Play Store
7. O mod menu aparece automaticamente!

## Como funciona

```
Kitsune Mask (Zygisk ativado)
  └─ Detecta processo com.mgs.sniper1
      └─ Zygisk loader (zygisk/<abi>.so)
          ├─ Copia classes.dex + libgiyuutmk.so para /data/data/com.mgs.sniper1/mod_cache/
          ├─ Carrega DEX via DexClassLoader
          ├─ Chama Main.Start(context)
          │   ├─ System.loadLibrary("giyuutmk")  ← hooks nativos
          │   └─ Inicia overlay do mod menu
          └─ Mod menu funcionando no jogo original!
```

## Estrutura do módulo

```
SniperStrikeMod.zip
├── module.prop              # Metadata do módulo
├── customize.sh             # Script de instalação
├── zygisk/
│   ├── arm64-v8a.so         # Loader Zygisk (64-bit)
│   └── armeabi-v7a.so       # Loader Zygisk (32-bit)
└── mod/
    ├── classes.dex           # Classes Java do mod menu
    └── lib/
        ├── arm64-v8a/
        │   └── libgiyuutmk.so
        └── armeabi-v7a/
            └── libgiyuutmk.so
```

## Notas

- O APK do jogo **NÃO é modificado** - tudo é injetado em runtime via Zygisk
- Funciona com atualizações do jogo (desde que os offsets não mudem)
- Se o jogo atualizar e os hooks quebrarem, atualize os offsets no Main.cpp do lglteam4.0
