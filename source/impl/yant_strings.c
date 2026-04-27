#include "../include/yant_strings.h"
#include <string.h>

bool ss_eq(StringSlice ssA, StringSlice ssB) {
    return ssA.length != ssB.length ? false : (memcmp(ssA.data, ssB.data, ssA.length) == 0);
}

bool ss_eq_cstr(StringSlice ss, const char* cstr) {
    return ss_eq(ss, ss_view(cstr));
}

i64  ss_cmp(StringSlice ssA, StringSlice ssB) {
    usize min_len = ssA.length < ssB.length ? ssA.length : ssB.length;
    int cmp = memcmp(ssA.data, ssB.data, min_len);
    if (cmp != 0) return cmp;
    if (ssA.length < ssB.length) return -1;
    if (ssA.length > ssB.length) return  1;
    return 0;
}

StringSlice ss_view(const char* cstr) {
    return (StringSlice) {.data=cstr, .length=strlen(cstr)};
}
