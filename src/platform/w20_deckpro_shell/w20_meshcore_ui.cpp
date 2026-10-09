#include "w20_meshcore_ui.h"
#include "W20ShellEpdDisplay.h"
#include "UITask.h"
#include <helpers/ui/UIScreen.h>
#include "NodePrefs.h"
#include <helpers/SensorManager.h>

extern UITask ui_task;

static DisplayDriver *s_disp = nullptr;
static SensorManager *s_sensors = nullptr;
static NodePrefs *s_prefs = nullptr;
static bool s_begun = false;
static bool s_on_mesh_page = false;

void w20_meshcore_ui_setup(DisplayDriver *disp, SensorManager *sensors, NodePrefs *prefs) {
  s_disp = disp;
  s_sensors = sensors;
  s_prefs = prefs;
}

bool w20_meshcore_ui_ready(void) {
  return s_begun;
}

void w20_meshcore_ui_on_enter(void) {
  if (!s_disp || !s_sensors || !s_prefs) return;
  auto *epd = static_cast<class W20ShellEpdDisplay *>(s_disp);
  if (!s_begun) {
    if (!epd->begin()) return;
    ui_task.begin(s_disp, s_sensors, s_prefs);
    s_begun = true;
  } else {
    /* Re-apply MeshCore 180° each enter (Deck may have reset rotation). */
    epd->begin(); /* idempotent: sets rotation on shared GxEPD2 */
  }
  s_on_mesh_page = true;
  if (!s_disp->isOn()) s_disp->turnOn();
  ui_task.gotoHomeScreen();
  s_disp->forceFullRefresh();
}

void w20_meshcore_ui_on_exit(void) {
  s_on_mesh_page = false;
  if (s_disp)
    static_cast<class W20ShellEpdDisplay *>(s_disp)->restoreDeckRotation();
}

void w20_meshcore_ui_loop(void) {
  if (!s_begun || !s_on_mesh_page) return;
  ui_task.loop();
}

void w20_meshcore_ui_nav_next(void) {
  if (!s_begun || !s_on_mesh_page) return;
  ui_task.injectKey(KEY_NEXT);
}

void w20_meshcore_ui_nav_select(void) {
  if (!s_begun || !s_on_mesh_page) return;
  ui_task.injectKey(KEY_ENTER);
}

void w20_meshcore_ui_nav_home(void) {
  if (!s_begun || !s_on_mesh_page) return;
  ui_task.injectKey(KEY_HOME);
}
