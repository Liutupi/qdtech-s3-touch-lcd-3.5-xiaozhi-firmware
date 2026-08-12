#ifndef QDTECH_PROVISIONING_COMPAT_H
#define QDTECH_PROVISIONING_COMPAT_H

#include "sdkconfig.h"

#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_STATELESS_SOFTAP_RF)
#define QDTECH_PROVISIONING_STATELESS_RF 1
#endif

#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_SOFTAP_CHANNEL_6_AB)
#define QDTECH_PROVISIONING_CHANNEL_6 1
#endif

#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_SINGLE_DRIVER_APSTA_PROVISIONING)
#define QDTECH_PROVISIONING_APSTA 1
#endif

#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT) || \
    defined(CONFIG_QDTECH_EXPERIMENT_RAW_BEACON_FALLBACK)
#define QDTECH_PROVISIONING_STA_BEACON 1
#endif

// The QDTech board is commonly deployed behind consumer mesh systems where
// several radios advertise the same SSID.  Keep the workaround board-local:
// pin each scanned candidate long enough to test it and rotate to the next
// BSSID when authentication/association fails.
#if defined(CONFIG_QDTECH_PROVISIONING_COMPAT)
#define QDTECH_WIFI_MESH_FALLBACK 1
#endif

#endif  // QDTECH_PROVISIONING_COMPAT_H
