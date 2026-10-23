/* Windows (MinGW-w64) cross build only: the static FFmpeg of the Windows prefix was built against a newer MinGW runtime that names two calls
 * with a 64 suffix; the CI laptop's older runtime lacks them. Compiled straight into every executable (see the end of CMakeLists.txt), weak so
 * that a runtime that has them wins. */
#include <time.h>
__attribute__((weak)) int clock_gettime64(clockid_t c, struct timespec* t) { return clock_gettime(c, t); }
__attribute__((weak)) int nanosleep64(const struct timespec* a, struct timespec* b) { return nanosleep(a, b); }
