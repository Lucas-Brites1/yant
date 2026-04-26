#include "../include/yant_strings.h"
#include <string.h>

bool ss_eq(StringSlice ssA, StringSlice ssB) {
    return ssA.length != ssB.length ? false : (memcmp(ssA.data, ssB.data, ssA.length) == 0);
}

bool ss_eq_cstr(StringSlice ss, const char* cstr) {
    return ss_eq(ss, ss_view(cstr));
}

StringSlice ss_view(const char* cstr) {
    return (StringSlice) {.data=cstr, .length=strlen(cstr)};
}
