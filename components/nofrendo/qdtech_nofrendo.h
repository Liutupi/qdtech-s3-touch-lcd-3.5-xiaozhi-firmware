#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*qd_nofrendo_frame_cb_t)(const uint16_t* pixels, uint16_t width, uint16_t height,
                                      void* user);

void qd_nofrendo_set_frame_callback(qd_nofrendo_frame_cb_t callback, void* user);
void qd_nofrendo_set_controller(uint8_t controller);
void qd_nofrendo_request_stop(void);
int qd_nofrendo_run(const char* rom_path);

#ifdef __cplusplus
}
#endif
