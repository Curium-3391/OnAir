// Must NOT compile with ONAIR_WEB_SESSION (the ctest web_tx_guard expects the compiler to fail): web code cannot reach the transmit path.
#include "dect2/source/source.h"
#include "dect2/tx/tx.h"
int main() {
    std::unique_ptr<dect2::TxSink> t = dect2::makeTxSink(dect2::DeviceInfo{});
    return t ? 1 : 0;
}
