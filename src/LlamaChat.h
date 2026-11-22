#pragma once

// llama.cpp 0.9.0：C API + common 聊天模板 API + mtmd 多模态 API
#include "llama.h"
#include "chat.h"
#include "mtmd.h"
#include "mtmd-helper.h"

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// 一条对话消息（role: system / user / assistant）
// imagePaths 仅 user 消息有效：图片按顺序对应 content 中的 <__media__> 标记
struct ChatMessage {
    std::string role;
    std::string content;
    std::vector<std::string> imagePaths;
};

// 生成参数
struct ChatOptions {
    // —— 加载模型时生效 ——
    uint32_t n_ctx        = 4096;              // 上下文长度（KV 缓存大小）
    uint32_t n_batch      = 512;               // 单次 llama_decode 的批大小
    int32_t  n_gpu_layers = -1;                // 卸载到 GPU 的层数：-1 = 全部，0 = 纯 CPU
    int32_t  n_threads    = 0;                 // CPU 线程数：0 = 自动
    std::string mmprojPath;                    // 可选：多模态投影模型（mmproj*.gguf），支持发送图片
    // —— 生成时生效 ——
    int32_t  n_predict    = 2048;              // 单次回复的最大 token 数
    float    temperature  = 0.70f;
    int32_t  top_k        = 40;                // <= 0 表示禁用
    float    top_p        = 0.95f;
    float    min_p        = 0.05f;
    uint32_t seed         = LLAMA_DEFAULT_SEED; // LLAMA_DEFAULT_SEED = 随机种子
    bool     enable_thinking = false;          // 是否允许模型的思考模式（Qwen3 等支持）
    bool     auto_trim      = true;            // 上下文满时自动省略较早消息
};

// 一次生成的结果统计
struct ChatResult {
    int32_t prompt_tokens    = 0;
    int32_t generated_tokens = 0;
    double  tokens_per_sec   = 0.0;
    int32_t context_tokens   = 0; // 生成结束时的 KV 长度
    int32_t context_capacity = 0; // n_ctx 容量
    int32_t omitted_messages = 0; // 为适配上下文而省略的历史消息数
    bool    context_limit_reached = false; // 回复因上下文上限被截断
};

// llama.cpp 的薄封装：模型加载 + 聊天模板渲染 + 采样生成 + 多模态（图片）。
// 所有方法都是阻塞的，应在工作线程中调用。
class LlamaChat {
public:
    ~LlamaChat();

    bool load(const std::string & modelPath, const ChatOptions & opts, std::string & err);
    void unload();
    bool isLoaded() const { return m_ctx != nullptr; }

    // 是否加载了 mmproj（多模态投影模型），决定能否发送图片
    bool hasVision() const { return m_mtmd != nullptr; }

    // 模型类型描述，如 "gemma3 4B Q4_K - Medium"
    std::string modelDesc() const;

    // 上下文使用情况（KV token 数 / n_ctx 容量）
    size_t contextTokens() const { return m_kvSize; }
    size_t contextCapacity() const { return m_nCtx; }

    // 根据历史消息生成回复；每产生一段文本就回调一次 on_piece；
    // stopFlag 置位时在当前 token 边界尽快中止。
    // 最后一条消息可携带图片（imagePaths），图片按顺序对应 content 中的 <__media__> 标记。
    // onContext(used, capacity) 在提示词解码后、生成过程中（每 8 个 token）与结束时回调。
    // 返回 false 时 err 为错误描述。
    bool chat(const std::vector<ChatMessage> & history,
              const ChatOptions & opts,
              const std::function<void(const std::string &)> & onPiece,
              const std::atomic_bool & stopFlag,
              ChatResult & result,
              std::string & err,
              const std::function<void(size_t, size_t)> & onContext = {});

private:
    // skipEmpty=false 时渲染空消息（用于状态文本的对齐渲染）
    // withGenPrompt=false 时不追加末尾的助手生成引导词
    bool renderPrompt(const std::vector<ChatMessage> & history,
                      const ChatOptions & opts,
                      std::string & promptOut,
                      std::string & err,
                      bool skipEmpty = true,
                      bool withGenPrompt = true) const;

    llama_model *              m_model = nullptr;
    llama_context *            m_ctx   = nullptr;
    common_chat_templates_ptr  m_tmpls;
    mtmd::context_ptr          m_mtmd;
    uint32_t                   m_nBatch = 512; // llama_n_batch() 的实际值
    uint32_t                   m_nCtx   = 0;   // llama_n_ctx()（上下文容量）

    // 长对话增量复用：状态文本 = 完整历史（含最后一轮回复，assistant 以历史消息
    // 形式渲染）的渲染结果——下一轮的完整渲染必然以它为文本前缀；
    // m_kvSize 记录 KV 实际长度（含文本不可见的特殊 token 与图片嵌入），用于定位续解码位置
    std::string m_decodedText;
    size_t      m_kvSize = 0;
};
