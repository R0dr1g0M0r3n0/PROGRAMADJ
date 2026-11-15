#include "audioforge/dj_library.h"

#include <cstdlib>

int main() {
    return audioforge::dj_library::build_marker() == 1 ? EXIT_SUCCESS : EXIT_FAILURE;
}
