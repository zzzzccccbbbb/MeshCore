/**
 * Meshtastic API stub globals for MeshCore + W20 Deck Pro UI.
 */
#include "configuration.h"
#include "PowerFSM.h"
#include "RadioInterface.h"
#include "airtime.h"
#include "detect/LoRaRadioType.h"
#include "graphics/Screen.h"
#include "input/InputBroker.h"
#include "input/TouchScreenImpl1.h"
#include "mesh/Channels.h"
#include "mesh/MeshService.h"
#include "mesh/NodeDB.h"
#include "mesh/Router.h"
#include "GPS.h"
#include "GPSStatus.h"
#include "nimble/NimbleBluetooth.h"

meshtastic_Config config = {};

bool needReconnect = false;
unsigned long rebootAtMsec = 0;
unsigned long shutdownAtMsec = 0;

static NodeDB s_nodeDB;
NodeDB *nodeDB = &s_nodeDB;

static graphics::Screen s_screen;
graphics::Screen *screen = &s_screen;
TouchScreenImpl1 *touchScreenImpl1 = nullptr;
InputBroker *inputBroker = nullptr;

RadioInterface *rIf = nullptr;
Router *router = nullptr;
MeshService *service = nullptr;
AirTime *airTime = nullptr;
Channels channels;
PowerFSM powerFSM;
LoRaRadioType radioType = NO_RADIO;

GPS *gps = nullptr;
static meshtastic::GPSStatus s_gpsStatus;
meshtastic::GPSStatus *gpsStatus = &s_gpsStatus;
NimbleBluetooth *nimbleBluetooth = nullptr;

bool runASAP = false;
