#include "json_guard.h"

// Scanning semantics are load-bearing (they were fuzzed against cJSON in the
// portal's original in-file version): string-literal aware, backslash-escape
// aware, '[' and '{' increment, ']' and '}' decrement, cap JSON_MAX_DEPTH.
// The only change made when this moved out of portal.c is that the terminator
// is the length, not a NUL byte.
bool json_depth_ok_n(const char *s, size_t len)
{
    int depth = 0;
    bool in_str = false;

    for (size_t i = 0; i < len; i++) {
        const char c = s[i];
        if (in_str) {
            if (c == '\\' && i + 1 < len) {
                i++;  // skip the escaped byte (notably \" and \\)
            } else if (c == '"') {
                in_str = false;
            }
            continue;
        }
        switch (c) {
        case '"':
            in_str = true;
            break;
        case '[':
        case '{':
            if (++depth > JSON_MAX_DEPTH) {
                return false;
            }
            break;
        case ']':
        case '}':
            depth--;  // may go negative on malformed input; cJSON rejects that
            break;
        default:
            break;
        }
    }
    return true;
}
