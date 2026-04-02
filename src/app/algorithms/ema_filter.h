#pragma once

namespace app::algorithms {

class EmaFilterFloat {
 public:
  void setAlpha(float value) {
    if (value <= 0.0f) {
      alpha = 0.01f;
      return;
    }
    if (value >= 1.0f) {
      alpha = 1.0f;
      return;
    }
    alpha = value;
  }

  void reset() {
    initialized = false;
    current = 0.0f;
  }

  float update(float input) {
    if (!initialized) {
      current = input;
      initialized = true;
      return current;
    }

    current = (alpha * input) + ((1.0f - alpha) * current);
    return current;
  }

  bool hasValue() const {
    return initialized;
  }

  float value() const {
    return current;
  }

 private:
  bool initialized = false;
  float alpha = 0.25f;
  float current = 0.0f;
};

}  // namespace app::algorithms
