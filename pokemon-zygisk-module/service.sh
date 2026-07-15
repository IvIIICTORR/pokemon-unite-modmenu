#!/system/bin/sh
# Pokemon Unite Mod Menu - Magisk Service Script
# This script runs as root at boot and monitors for the game process.
# When detected, it copies mod files and triggers the mod menu injection.

MODDIR=${0%/*}
PKG="jp.pokemon.pokemonunite"
TAG="PUMod"

log() {
  log -p i -t "$TAG" "$1"
}

log "Service script started from $MODDIR"

# Wait for boot to complete
while [ "$(getprop sys.boot_completed)" != "1" ]; do
  sleep 2
done
sleep 5
log "Boot completed, starting monitor"

# Auto-grant overlay permission (fixes mod menu icon not appearing)
cmd appops set "$PKG" SYSTEM_ALERT_WINDOW allow 2>/dev/null
log "Overlay permission granted for $PKG"

# Detect ABI
ABI=$(getprop ro.product.cpu.abi)
log "Device ABI: $ABI"

# Source paths
SRC_DEX="$MODDIR/mod/classes.dex"
SRC_SO_DIR="$MODDIR/mod/lib"

# Find the best .so for this device
find_so() {
  for abi in "$ABI" "arm64-v8a" "armeabi-v7a" "x86_64" "x86"; do
    if [ -f "$SRC_SO_DIR/$abi/libgiyuutmk.so" ]; then
      echo "$SRC_SO_DIR/$abi/libgiyuutmk.so"
      return
    fi
  done
}

SRC_SO=$(find_so)
log "Source DEX: $SRC_DEX (exists=$([ -f "$SRC_DEX" ] && echo yes || echo no))"
log "Source SO: $SRC_SO (exists=$([ -f "$SRC_SO" ] && echo yes || echo no))"

if [ ! -f "$SRC_DEX" ] || [ -z "$SRC_SO" ] || [ ! -f "$SRC_SO" ]; then
  log "ERROR: mod files not found, aborting"
  exit 1
fi

# Copy mod files to app's data directory
copy_mod_files() {
  APP_DIR="/data/data/$PKG"
  CACHE_DIR="$APP_DIR/mod_cache"
  LIB_DIR="$CACHE_DIR/lib"

  # Wait for app data dir to exist
  if [ ! -d "$APP_DIR" ]; then
    log "App data dir not found: $APP_DIR"
    return 1
  fi

  # Get app UID for proper ownership
  APP_UID=$(stat -c %u "$APP_DIR" 2>/dev/null)
  APP_GID=$(stat -c %g "$APP_DIR" 2>/dev/null)
  [ -z "$APP_UID" ] && APP_UID=10054
  [ -z "$APP_GID" ] && APP_GID=10054

  mkdir -p "$CACHE_DIR" "$LIB_DIR"
  cp -f "$SRC_DEX" "$CACHE_DIR/classes.dex"
  cp -f "$SRC_SO" "$LIB_DIR/libgiyuutmk.so"

  # Set permissions - app needs to read these
  chmod 777 "$CACHE_DIR"
  chmod 777 "$LIB_DIR"
  chmod 666 "$CACHE_DIR/classes.dex"
  chmod 777 "$LIB_DIR/libgiyuutmk.so"
  chown -R "$APP_UID:$APP_GID" "$CACHE_DIR"

  log "Files copied to $CACHE_DIR (uid=$APP_UID)"
  return 0
}

# Inject mod into running game process using app_process
inject_mod() {
  CACHE_DIR="/data/data/$PKG/mod_cache"

  # Use app_process to run a dex that loads our mod
  # Create a small injector script
  INJECT_DEX="$CACHE_DIR/classes.dex"
  INJECT_LIB="$CACHE_DIR/lib"

  if [ ! -f "$INJECT_DEX" ]; then
    log "Inject DEX not found"
    return 1
  fi

  # Use am to start the mod's service directly in the game's process
  # The Launcher service from our DEX will be started in the game's context
  am startservice -n "$PKG/com.android.support.Launcher" 2>&1 | while read line; do
    log "am startservice: $line"
  done

  return 0
}

# Monitor loop - watch for game process
INJECTED=0
LAST_PID=0

while true; do
  # Check if game is running
  GAME_PID=$(pidof "$PKG" 2>/dev/null)

  if [ -n "$GAME_PID" ] && [ "$GAME_PID" != "$LAST_PID" ]; then
    # New game instance detected
    log "Game detected! PID=$GAME_PID"
    LAST_PID=$GAME_PID
    INJECTED=0

    # Re-grant overlay permission every time game starts
    cmd appops set "$PKG" SYSTEM_ALERT_WINDOW allow 2>/dev/null
    log "Overlay permission re-granted"

    # Copy files first
    copy_mod_files

    # Wait for the game to fully initialize
    sleep 8

    # Check if still running
    if kill -0 "$GAME_PID" 2>/dev/null; then
      log "Injecting mod menu..."
      inject_mod
      INJECTED=1
      log "Injection attempted"
    else
      log "Game died before injection"
    fi
  elif [ -z "$GAME_PID" ]; then
    # Game not running, reset state
    LAST_PID=0
    INJECTED=0
  fi

  sleep 3
done
