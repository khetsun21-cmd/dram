#ifndef AXI_HELPER_H
#define AXI_HELPER_H

#include <systemc>
#include <tlm>
#include <axi/axi_tlm.h>
#include <vector>
#include <memory>
#include <functional>

namespace axi_helper {

// 前向声明响应处理器
class AXIResponseHandler;

// 全局响应事件和处理器
extern sc_core::sc_event g_response_event;
extern AXIResponseHandler* g_response_handler;

/**
 * AXI响应处理器基类
 */
class AXIResponseHandler {
public:
    virtual ~AXIResponseHandler() = default;
    virtual tlm::tlm_sync_enum nb_transport_bw(axi::axi_protocol_types::tlm_payload_type& trans,
                                               tlm::tlm_phase& phase,
                                               sc_core::sc_time& delay) = 0;
    virtual void invalidate_direct_mem_ptr(sc_dt::uint64, sc_dt::uint64) = 0;

    sc_core::sc_event rsp_ev_;
    tlm::tlm_response_status status_;
    tlm::tlm_generic_payload* payload_;
};

/**
 * AXI请求参数结构体
 */
struct AXIRequest {
    sc_dt::uint64 address;           // 请求地址
    std::vector<unsigned char> data; // 数据缓冲区（读写共用）
    std::size_t data_length;         // 数据长度
    unsigned int id;                 // AXI ID
    axi::burst_e burst_type;         // burst类型
    unsigned int burst_length;       // burst长度（beat数量）
    unsigned int burst_size;         // burst大小（2^size字节）
    unsigned int cache;              // CACHE属性
    unsigned int prot;               // PROT属性
    unsigned int qos;                // QoS值
    unsigned int region;             // REGION值

    // 构造函数，提供默认值
    AXIRequest()
        : address(0), data_length(0), id(0),
          burst_type(axi::burst_e::INCR), burst_length(1), burst_size(0),
          cache(0), prot(0), qos(0), region(0) {}

    // 便捷构造函数
    AXIRequest(sc_dt::uint64 addr, std::size_t len, unsigned int req_id = 0)
        : address(addr), data_length(len), id(req_id),
          burst_type(axi::burst_e::INCR), burst_length(1), burst_size(0),
          cache(0), prot(0), qos(0), region(0) {
        data.resize(len);
    }
};

/**
 * AXI响应结构体
 */
struct AXIResponse {
    bool success;                    // 是否成功
    tlm::tlm_response_status status; // TLM响应状态
    axi::resp_e axi_resp;           // AXI响应状态
    sc_core::sc_time latency;       // 延迟时间

    AXIResponse() : success(false), status(tlm::TLM_INCOMPLETE_RESPONSE),
                   axi_resp(axi::resp_e::OKAY), latency(sc_core::SC_ZERO_TIME) {}
};

/**
 * AXI Helper类 - 提供便捷的AXI事务创建和发送功能
 */
class AXIHelper {
public:
    /**
     * 创建AXI写请求payload
     * @param req 请求参数
     * @return 配置好的payload指针（需要手动释放）
     */
    static tlm::tlm_generic_payload* createWritePayload(const AXIRequest& req);

    /**
     * 创建AXI读请求payload
     * @param req 请求参数
     * @return 配置好的payload指针（需要手动释放）
     */
    static tlm::tlm_generic_payload* createReadPayload(const AXIRequest& req);

    /**
     * 发送阻塞AXI写请求
     * @param socket AXI发起者socket
     * @param req 请求参数
     * @param delay 延迟时间（输入输出）
     * @return 响应结果
     */
    static AXIResponse sendBlockingWrite(axi::axi_initiator_socket<1024>& socket,
                                       const AXIRequest& req,
                                       sc_core::sc_time& delay);

    /**
     * 发送阻塞AXI读请求
     * @param socket AXI发起者socket
     * @param req 请求参数（数据将填充到req.data中）
     * @param delay 延迟时间（输入输出）
     * @return 响应结果
     */
    static AXIResponse sendBlockingRead(axi::axi_initiator_socket<1024>& socket,
                                      AXIRequest& req,
                                      sc_core::sc_time& delay);

    /**
     * 发送非阻塞AXI写请求
     * @param socket AXI发起者socket
     * @param req 请求参数
     * @param delay 延迟时间
     * @param callback 响应回调函数
     * @return TLM同步枚举
     */
    static tlm::tlm_sync_enum sendNonBlockingWrite(
        axi::axi_initiator_socket<1024>& socket,
        const AXIRequest& req,
        sc_core::sc_time& delay,
        std::function<void(const AXIResponse&)> callback = nullptr);

    /**
     * 发送非阻塞AXI读请求
     * @param socket AXI发起者socket
     * @param req 请求参数
     * @param delay 延迟时间
     * @param callback 响应回调函数
     * @return TLM同步枚举
     */
    static tlm::tlm_sync_enum sendNonBlockingRead(
        axi::axi_initiator_socket<1024>& socket,
        AXIRequest& req,
        sc_core::sc_time& delay,
        std::function<void(const AXIResponse&)> callback = nullptr);

    /**
     * 便捷函数：单次写操作
     * @param socket AXI发起者socket
     * @param address 地址
     * @param data 数据指针
     * @param length 数据长度
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool write(axi::axi_initiator_socket<1024>& socket,
                     sc_dt::uint64 address,
                     const unsigned char* data,
                     std::size_t length,
                     unsigned int id = 0,
                     sc_core::sc_time* delay = nullptr);

    /**
     * 便捷函数：单次读操作
     * @param socket AXI发起者socket
     * @param address 地址
     * @param data 数据缓冲区
     * @param length 数据长度
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool read(axi::axi_initiator_socket<1024>& socket,
                    sc_dt::uint64 address,
                    unsigned char* data,
                    std::size_t length,
                    unsigned int id = 0,
                    sc_core::sc_time* delay = nullptr);

    /**
     * 便捷函数：写向量数据
     * @param socket AXI发起者socket
     * @param address 地址
     * @param data 数据向量
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool writeVector(axi::axi_initiator_socket<1024>& socket,
                           sc_dt::uint64 address,
                           const std::vector<unsigned char>& data,
                           unsigned int id = 0,
                           sc_core::sc_time* delay = nullptr);

    /**
     * 便捷函数：读向量数据
     * @param socket AXI发起者socket
     * @param address 地址
     * @param data 数据向量（将被填充）
     * @param length 读取长度
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool readVector(axi::axi_initiator_socket<1024>& socket,
                          sc_dt::uint64 address,
                          std::vector<unsigned char>& data,
                          std::size_t length,
                          unsigned int id = 0,
                          sc_core::sc_time* delay = nullptr);

    /**
     * 便捷函数：写字符串
     * @param socket AXI发起者socket
     * @param address 地址
     * @param str 字符串
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool writeString(axi::axi_initiator_socket<1024>& socket,
                           sc_dt::uint64 address,
                           const std::string& str,
                           unsigned int id = 0,
                           sc_core::sc_time* delay = nullptr);

    /**
     * 便捷函数：读字符串
     * @param socket AXI发起者socket
     * @param address 地址
     * @param str 字符串缓冲区
     * @param max_length 最大读取长度
     * @param id AXI ID（默认0）
     * @param delay 延迟时间（输入输出）
     * @return 是否成功
     */
    static bool readString(axi::axi_initiator_socket<1024>& socket,
                          sc_dt::uint64 address,
                          std::string& str,
                          std::size_t max_length,
                          unsigned int id = 0,
                          sc_core::sc_time* delay = nullptr);

    /**
     * 释放payload资源
     * @param payload 要释放的payload指针
     */
    static void releasePayload(tlm::tlm_generic_payload* payload);

private:
    /**
     * 配置AXI扩展
     * @param payload TLM payload
     * @param req 请求参数
     */
    static void setupAXIExtension(tlm::tlm_generic_payload* payload, const AXIRequest& req);

    /**
     * 从payload提取响应信息
     * @param payload TLM payload
     * @return AXI响应结构体
     */
    static AXIResponse extractResponse(const tlm::tlm_generic_payload* payload);

    // 内存管理器
    static tlm::tlm_mm_interface* getMemoryManager();
};

/**
 * AXI事务构建器类 - 用于构建复杂的AXI事务
 */
class AXITransactionBuilder {
public:
    AXITransactionBuilder();

    // 设置基本参数
    AXITransactionBuilder& setAddress(sc_dt::uint64 addr);
    AXITransactionBuilder& setDataLength(std::size_t len);
    AXITransactionBuilder& setID(unsigned int id);
    AXITransactionBuilder& setData(const std::vector<unsigned char>& data);
    AXITransactionBuilder& setData(const unsigned char* data, std::size_t len);

    // 设置AXI特定参数
    AXITransactionBuilder& setBurstType(axi::burst_e type);
    AXITransactionBuilder& setBurstLength(unsigned int len);
    AXITransactionBuilder& setBurstSize(unsigned int size);
    AXITransactionBuilder& setCache(unsigned int cache);
    AXITransactionBuilder& setProt(unsigned int prot);
    AXITransactionBuilder& setQoS(unsigned int qos);
    AXITransactionBuilder& setRegion(unsigned int region);

    // 构建和发送
    AXIRequest build() const;
    AXIResponse sendBlockingWrite(axi::axi_initiator_socket<1024>& socket, sc_core::sc_time& delay);
    AXIResponse sendBlockingRead(axi::axi_initiator_socket<1024>& socket, sc_core::sc_time& delay);

private:
    AXIRequest request_;
};

} // namespace axi_helper

#endif // AXI_HELPER_H
