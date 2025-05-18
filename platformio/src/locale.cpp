#include "config.h"

#define STR(s)             #s
#define X_LOCALE_INC(code) STR(locales/locale_##code.inc)
#define LOCALE_INC(code)   X_LOCALE_INC(code)

#include LOCALE_INC(LOCALE)
