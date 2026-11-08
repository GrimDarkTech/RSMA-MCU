#include <iostream>

#include "rsma_mcu/version.h"

int main()
{
    std::cout << "RSMA MCU " << RSMA_MCU_VERSION_STRING << '\n';
    std::cout << "Firmware format: RSFW v1 (planned)\n";
    std::cout << "Target VM: rsma-vm v1 (planned)\n";
    std::cout << "LLVM/Clang: bundled submodule\n";
    return 0;
}
