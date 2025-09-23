#include "AxiToTlmBridge.h"

int sc_main(int argc, char* argv[]) {
    // 占位示例：仅实例化桥接模块，具体系统连接由上层工程完成
    AxiToTlmBridge bridge("bridge");
    (void)argc; (void)argv; (void)bridge;
    // 如需运行仿真，请在外部可执行工程中实例化 DRAMSys 并进行 socket 连接
    // sc_core::sc_start();

    return 0;
}
