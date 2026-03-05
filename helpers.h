#pragma once
#include <cmath>

// Float validator
inline bool validf(float x) { return !isnan(x) && isfinite(x); }
