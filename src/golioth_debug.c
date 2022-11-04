#include "golioth_debug.h"
#include <stdio.h>

static golioth_debug_log_level_t _level = GOLIOTH_DEBUG_LOG_LEVEL_INFO;

void golioth_debug_set_log_level(golioth_debug_log_level_t level) {
    _level = level;
}

golioth_debug_log_level_t golioth_debug_get_log_level(void) {
    return _level;
}

// Adapted from https://stackoverflow.com/a/7776146
void golioth_debug_hexdump(const char* tag, const void* addr, int len) {
    const int per_line = 16;

    int i;
    unsigned char buff[per_line + 1];
    const unsigned char* pc = (const unsigned char*)addr;

    if (len <= 0) {
        return;
    }

    // Output description if given.
    if (tag != NULL) {
        printf("%s:\n", tag);
    }

    // Process every byte in the data.
    for (i = 0; i < len; i++) {
        // Multiple of per_line means new or first line (with line offset).

        if ((i % per_line) == 0) {
            // Only print previous-line ASCII buffer for lines beyond first.
            if (i != 0) {
                printf("  %s\n", buff);
            }

            // Output the offset of current line.
            printf("  %04x ", i);
        }

        // Now the hex code for the specific character.
        printf(" %02x", pc[i]);

        // And buffer a printable ASCII character for later.
        if ((pc[i] < 0x20) || (pc[i] > 0x7e)) {  // isprint() may be better.
            buff[i % per_line] = '.';
        } else {
            buff[i % per_line] = pc[i];
        }
        buff[(i % per_line) + 1] = '\0';
    }

    // Pad out last line if not exactly per_line characters.
    while ((i % per_line) != 0) {
        printf("   ");
        i++;
    }

    // And print the final ASCII buffer.
    printf("  %s\n", buff);
}
