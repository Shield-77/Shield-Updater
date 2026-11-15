#pragma once

#include <stdbool.h>

// Optional background music. Failure must not prevent the updater from working.
bool music_start(void);
void music_stop(void);
