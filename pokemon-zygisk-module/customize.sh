#!/system/bin/sh
# Pokemon Unite Mod Menu - Kitsune Mask Module Install Script

# Set permissions for mod libs
set_perm_recursive "$MODPATH/mod/lib" 0 0 0755 0755
set_perm_recursive "$MODPATH/zygisk" 0 0 0755 0755

ui_print "- Pokemon Unite Mod Menu installed!"
ui_print "- Make sure Zygisk is enabled in Kitsune Mask settings"
ui_print "- Target package: jp.pokemon.pokemonunite"

# Auto-grant overlay permission so mod menu icon appears
cmd appops set jp.pokemon.pokemonunite SYSTEM_ALERT_WINDOW allow 2>/dev/null
ui_print "- Overlay permission granted automatically"

ui_print "- Reboot to activate"
ui_print ""
