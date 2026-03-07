#pragma once
#include <cmath>

// Float validator
inline bool validf(float x) { return !isnan(x) && isfinite(x); }

// Random float generator for testing
inline float randomStep(float amplitude) {
  return ((esp_random() / (float)UINT32_MAX) * 2.0f - 1.0f) * amplitude;
}

inline float testData(float value, float min, float max, float step) {
  value += randomStep(step);
  if (value < min) value = min;
  if (value > max) value = max;
  return value;
}
