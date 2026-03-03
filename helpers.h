#pragma once
#include <cmath>

// Float-validaattori - käytössä kaikkialla
inline bool validf(float x) { return !isnan(x) && isfinite(x); }
