#pragma once
#include <cmath>

// Float validator
inline bool validf(float x) { return !isnan(x) && isfinite(x); }

// Random float generator for testing
inline float randomFloat(float min, float max) {
  return min + (random(0, 10000) / 10000.0f) * (max - min);
}
