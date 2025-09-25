# DRAMSys AXI 桥接快速上手指南

## 1. 项目概述
本项目提供一套可复用的 SystemC 组件，用于将 AXI4 总线事务转换为 DRAMSys 所使用的 TLM 事务，并打包成易于在其他工程中复用的静态/动态库。核心由 `AxiToTlmBridge` 桥接模块、`AxiDramsysSystem` 封装模块以及一系列 AXI 事务工具类组成，并在 CMake 构建脚本中默认导出 `axi_dramsys_bridge` 库目标，方便外部工程直接链接使用。【F:src/AxiToTlmBridge.h†L12-L85】【F:src/AxiToTlmBridge.cpp†L8-L195】【F:src/AxiDramsysSystem.h†L16-L40】【F:src/AxiDramsysSystem.cpp†L5-L39】【F:CMakeLists.txt†L301-L359】

## 2. 目录结构
- `build.sh`：提供快捷构建入口，可选择 Release/Debug/ASan 等模式，并支持打开原生集成开关或执行安装步骤。【F:build.sh†L4-L41】  
- `src/AxiToTlmBridge.*`：实现 128B AXI 目标 socket 与 TLM initiator 之间的桥接，支持配置默认/逐拍延迟、beat 拆分、日志打印和内部工作线程等功能。【F:src/AxiToTlmBridge.h†L12-L85】【F:src/AxiToTlmBridge.cpp†L8-L195】  
- `src/AxiDramsysSystem.*`：在桥接器外包了一层 SystemC 模块，负责在 elaboration 阶段加载 DRAMSys 配置并与 DRAMSys TLM socket 连接，外部只需操作 AXI target socket 和可选时钟端口即可。【F:src/AxiDramsysSystem.h†L16-L40】【F:src/AxiDramsysSystem.cpp†L5-L39】  
- `src/AXIHelper.*`：封装了常用的 AXI 请求/响应结构体、事务构建器及阻塞/非阻塞的发送辅助函数，便于在测试或上层组件中快速发起 AXI 访问。【F:src/AXIHelper.h†L36-L270】【F:src/AXIHelper.cpp†L7-L405】  
- `src/sc_main.cpp`：占位用的 `sc_main`，默认仅实例化桥接模块，实际仿真由外部工程主程序控制。【F:src/sc_main.cpp†L3-L10】  
- `src/DRAMSys`：默认包含 DRAMSys 上游源码，CMake 会将其作为子目录集成或按需切换为预编译库/外部脚本构建流程。【F:CMakeLists.txt†L127-L298】

## 3. 环境与依赖
- C++20 编译器与 CMake ≥ 3.22，项目通过 CMake 强制启用 C++20 并附加 `-DSC_INCLUDE_DYNAMIC_PROCESSES` 等 SystemC 所需编译宏。【F:CMakeLists.txt†L4-L5】【F:CMakeLists.txt†L103-L112】
- SystemC 2.3.4（或兼容版本）：通过 `SYSTEMC_PATH` 变量指定安装位置，若未启用 FetchContent，会直接查找并导入 `SystemC::systemc` 目标，否则可复用仓库内置源码。【F:CMakeLists.txt†L125-L177】
- DRAMSys：默认使用 `src/DRAMSys` 子目录源码，也可以通过 `DRAMSYS_LIB` 指向外部编译好的 `libdramsys`，或在 CMake 中找到已安装的 `DRAMSys::libdramsys` 包。【F:CMakeLists.txt†L127-L203】
- 可选依赖：脚本会尝试优先使用仓库内置的 Boost、fmt、spdlog、yaml-cpp 等依赖，若未找到需要自行安装或通过 `BOOST_ROOT` 等变量提供路径。【F:CMakeLists.txt†L49-L101】

## 4. 构建方式
### 4.1 使用 `build.sh`
执行 `./build.sh {rel|dbg|dbg_asan|clean} [native] [install]` 即可完成常见构建任务：
- `rel`/`dbg`/`dbg_asan` 分别对应 Release、Debug 和启用 AddressSanitizer 的 Debug 配置；
- 追加 `native` 会在 CMake 配置阶段打开 `-DDRAMSYS_NATIVE_INTEGRATION=ON`，直接在当前工程中构建 DRAMSys；
- 追加 `install` 会调用 `cmake --install` 安装生成的库和头文件。
脚本内部会自动创建 `build` 目录并并行编译，`clean` 选项用于清理构建缓存。【F:build.sh†L4-L41】

### 4.2 手动执行 CMake
若需自定义构建流程，可直接调用：
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
      -DSYSTEMC_PATH=/path/to/systemc-2.3.4 \
      -DDRAMSYS_PATH=/path/to/DRAMSys \
      -DDRAMSYS_NATIVE_INTEGRATION=ON
cmake --build build --parallel
```
可根据需要覆写 `DRAMSYS_LIB`、`DRAMSYS_USE_SUBPROJECT`、`DRAMSYS_USE_FETCH_CONTENT_*` 等变量来选择使用预编译库、内置源码或完全外部构建脚本。同时，项目会自动设置 `SystemC::systemc` 目标、包含目录和编译警告选项，确保桥接库与 DRAMSys/AXI 头文件正确链接。【F:CMakeLists.txt†L22-L177】【F:CMakeLists.txt†L183-L347】

## 5. 快速集成步骤
### 5.1 配置桥接模块
`AxiToTlmBridge` 提供 1024-bit AXI target socket 与下游 TLM initiator socket，并内置工作线程将 AXI burst 按 DRAM beat 拆分后转发；你可以通过 `set_base_latency`、`set_beat_latency`、`set_downstream_beat_bytes` 等接口调整延迟与拆分粒度，也可以关闭/打开日志输出。若未绑定外部时钟，模块会在 `before_end_of_elaboration` 中自动绑定内部 1ns 周期的时钟以保持兼容性。【F:src/AxiToTlmBridge.h†L16-L69】【F:src/AxiToTlmBridge.cpp†L8-L118】

### 5.2 将 DRAMSys 暴露为 AXI 从设备
`AxiDramsysSystem` 在 elaboration 阶段自动读取配置文件、实例化 DRAMSys，并把桥接器的 TLM initiator socket 与 DRAMSys 的 `tSocket` 绑定。上层只需在创建实例后调用 `set_config_path()` 指定 DRAMSys YAML 配置，随后将主设备的 AXI initiator socket 绑定到 `axi_target_socket`，并按需连接 `clk_i`。若配置文件不存在或未提前设置路径，模块会在 elaboration 阶段报错，确保仿真环境有效。【F:src/AxiDramsysSystem.h†L20-L39】【F:src/AxiDramsysSystem.cpp†L5-L39】

```cpp
AxiDramsysSystem mem("mem");
mem.set_config_path("configs/lpddr4.yaml");
initiator_socket.bind(mem.axi_target_socket);
mem.clk_i(clk_signal);
```
上述示例展示了在外部工程中创建 DRAMSys 模块并完成 socket 绑定的最小流程。【F:src/AxiDramsysSystem.h†L20-L39】

### 5.3 使用 AXIHelper 发起事务
`axi_helper::AXIRequest` / `AXIResponse` 结构体以及 `AXIHelper` 静态方法提供了创建 payload、发送阻塞/非阻塞事务、读写字符串或向量等常用操作，适合在测试平台或驱动模块中直接复用；`AXITransactionBuilder` 则支持链式设置地址、数据、burst 信息等参数后一次性发送事务。【F:src/AXIHelper.h†L36-L270】【F:src/AXIHelper.cpp†L30-L392】

```cpp
using namespace axi_helper;
sc_core::sc_time delay = sc_core::SC_ZERO_TIME;
AXIRequest req(0x1000, 64);
AXIResponse rsp = AXIHelper::sendBlockingRead(cpu_socket, req, delay);
```
该片段演示了如何构造 64 字节读请求并通过阻塞接口获取响应与延迟信息。【F:src/AXIHelper.h†L99-L119】【F:src/AXIHelper.cpp†L204-L236】

### 5.4 在自定义可执行程序中驱动仿真
仓库默认的 `sc_main` 仅用于占位，实际仿真应在外部可执行程序中完成模块实例化、连接和 `sc_start()` 调用。因此，当你将本项目作为子模块或静态库引用时，只需要在自己的顶层 SystemC 程序中包含相关头文件并启动仿真即可。【F:src/sc_main.cpp†L3-L10】

### 5.5 在纯 C++/ESL 顶层中按需驱动仿真
若你的上层并非 SystemC 模块，而是希望在普通 C++ 程序中手动调用 `sc_start()` 推进时间，可以使用新增的 `AxiDramsysModel` 封装。该类会在内部实例化 `AxiDramsysSystem`、桥接器及一个阻塞式 AXI master 线程，并提供：

- `write()` / `read()`：阻塞式 API，会在内部循环调用 `sc_start(step_time)` 直至事务完成；
- `post_write()` / `post_read()` + `is_request_done()` / `collect_response()`：异步提交接口，方便上层以自定义节奏驱动仿真；
- `advance_cycle()` / `advance_for()`：显式推进仿真时间，便于和其它 ESL 模型共享主循环。

典型用法如下：

```cpp
AxiDramsysModel model{"cxx_model"};
model.set_config_path("configs/lpddr4-example.json");

// 阻塞写/读
axi_helper::AXIRequest req(addr, size);
model.write(req);
model.read(req);

// 异步请求 + 手动驱动时钟
auto handle = model.post_read(req);
while (!model.is_request_done(handle)) {
    model.advance_cycle();
}
auto resp = model.collect_response(handle, &req);
```

`tests/cxx_model_test.cpp` 展示了完整的异步写读流程，并通过 `advance_cycle()`/`advance_cycles()` 演示如何让外部循环掌控仿真节奏。【F:src/AxiDramsysModel.h†L15-L146】【F:src/AxiDramsysModel.cpp†L5-L193】【F:tests/cxx_model_test.cpp†L1-L103】

## 6. 常用 CMake 选项速查
| 变量 | 默认值 | 作用 |
| --- | --- | --- |
| `SC_FORCE_SHARED` | `ON` | 构建 SystemC/DRAMSys 依赖为共享库，必要时可改为 `OFF` 获得静态链接。【F:CMakeLists.txt†L6-L9】 |
| `SYSTEMC_PATH` | `../systemc-2.3.4` | 指定 SystemC 根目录；未启用 FetchContent 时用于查找并导入 `SystemC::systemc`。【F:CMakeLists.txt†L125-L177】 |
| `DRAMSYS_PATH` | `src/DRAMSys` | 指向 DRAMSys 源码；在原生集成模式下会 `add_subdirectory` 进入构建。【F:CMakeLists.txt†L127-L298】 |
| `DRAMSYS_NATIVE_INTEGRATION` | `OFF` | 为 `ON` 时完全由当前 CMake 工程管理 DRAMSys 依赖；否则会尝试调用外部脚本构建。【F:CMakeLists.txt†L22-L35】 |
| `DRAMSYS_USE_SUBPROJECT` | `ON` | 控制是否把 `DRAMSys` 作为子项目直接构建，可在使用预编译库时关闭。【F:CMakeLists.txt†L151-L298】 |
| `DRAMSYS_LIB` | 空 | 指向外部 `libdramsys` 静态/动态库，优先于子项目方式。【F:CMakeLists.txt†L155-L203】 |
| `DRAMSYS_USE_FETCH_CONTENT_*` | `OFF`（视本地资源自动开启） | 控制是否复用仓库内置的 DRAMUtils/DRAMPower/SystemC/SQLite/nlohmann_json 等源码，避免网络下载。【F:CMakeLists.txt†L139-L269】 |
| `ARTERIS_PATH` | `DRAMSys/lib/tlm2-interfaces` | 指向 tlm2-interfaces 头文件位置，便于桥接模块包含 AXI 协议定义。【F:CMakeLists.txt†L157-L160】 |

## 7. 示例与测试
构建完成后会生成 `axi_dramsys_bridge` 库，以及在 `tests/` 目录存在源码时自动启用的 `bridge_test` 和 `dramsys_lpddr4_test` 可执行程序。它们分别用于验证桥接逻辑及与 DRAMSys 的端到端读写流程，默认链接 SystemC、DRAMSys 与桥接库，方便你在本仓库或外部工程中参考测试结构。【F:CMakeLists.txt†L247-L303】

### 7.1 确认测试已编译
默认情况下，`CMakeLists.txt` 只会在开启 `DRAMSYS_NATIVE_INTEGRATION` 时同时生成 `bridge_test` 和 `dramsys_lpddr4_test` 目标；如果检测到上一级目录存在 `build_dramsys.sh` 脚本且你没有显式传入 `-DDRAMSYS_NATIVE_INTEGRATION=ON`，CMake 会保留脚本式流程并跳过所有桥接/测试目标，从而导致 `ctest` 报告 “No tests were found”。【F:CMakeLists.txt†L25-L38】【F:CMakeLists.txt†L264-L320】
使用 `./build.sh rel native` 或其它构建模式附加 `native` 参数，脚本会在配置阶段注入 `-DDRAMSYS_NATIVE_INTEGRATION=ON` 并进入原生集成路径，确保测试目标被注册到构建目录中。【F:build.sh†L4-L65】

### 7.2 执行测试并查看日志
进入 `build` 目录后运行 `ctest -R "bridge_test|dramsys_lpddr4_test" --output-on-failure`（或使用 `ctest --test-dir build ...`），即可在终端看到 SystemC 打印的检查信息，完整输出会同时写入 `build/Testing/Temporary/LastTest.log` 便于后续排查。测试二进制位于构建目录根部，也可以直接执行 `./bridge_test` 或 `./dramsys_lpddr4_test` 单独调试。

## 8. 常见问题与排错
- **找不到 SystemC 库**：若 `SYSTEMC_PATH` 指向的目录未包含已编译的 SystemC 库，配置阶段会报错 `SystemC library not found`。请确认已按该路径编译安装 SystemC，或启用 FetchContent 使用仓库内自带源码。【F:CMakeLists.txt†L162-L177】
- **无法定位 DRAMSys**：当既未提供预编译库、也未启用子项目构建时，CMake 会直接报错提示三种可选方案。请检查 `DRAMSYS_LIB`、`DRAMSYS_USE_SUBPROJECT` 与 `DRAMSYS_PATH` 的配置是否正确。【F:CMakeLists.txt†L183-L327】
- **需要原生或脚本式构建**：若你已有独立的 `build_dramsys.sh`，保持 `DRAMSYS_NATIVE_INTEGRATION=OFF` 即可让 CMake 自动调用外部脚本；否则脚本缺失时会自动切换为原生集成模式，无需手动修改。【F:CMakeLists.txt†L22-L35】
