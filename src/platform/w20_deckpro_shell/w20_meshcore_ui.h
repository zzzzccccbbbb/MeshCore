#pragma once

/** MeshCore companion UI bridged behind Deck Pro Mesh menu entry. */
void w20_meshcore_ui_setup(class DisplayDriver *disp, class SensorManager *sensors, struct NodePrefs *prefs);
void w20_meshcore_ui_on_enter(void);
void w20_meshcore_ui_on_exit(void);
void w20_meshcore_ui_loop(void);
bool w20_meshcore_ui_ready(void);

/** CST3530 gesture → MeshCore UITask navigation (next / select / home). */
void w20_meshcore_ui_nav_next(void);
void w20_meshcore_ui_nav_select(void);
void w20_meshcore_ui_nav_home(void);
