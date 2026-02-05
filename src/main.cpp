#include "app.hpp"

#include <iostream>

int main() {
  App app;

  try {
    app.run();
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << "\n";
    return 1;
  }

  return 0;
}
