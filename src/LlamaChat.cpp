#include "LlamaChat.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>

static std::string decodeError(int32_t code) {
    switch (code) {
        case 1:  return "上下文不足：提示词 token 数超过 n_ctx，请调大“上下文”参数";
        case 2:  return "解码被中止";
        case -1: return "非法的输入 batch";
        default: return "llama_decode 失败（错误码 " + std::to_string(code) + "）";
    }
}

LlamaChat::~LlamaChat() {
    unload();
}

bool LlamaChat::load(const std::string & modelPath, const ChatOptions & opts, std::string & err) {
    unload();

    {
        std::ifstream f(modelPath, std::ios::binary);
        if (!f.good()) {
            err = "模型文件不存在或不可读：" + modelPath;
            return false;
        }
    }
    if (!opts.mmprojPath.empty()) {
        std::ifstream f(opts.mmprojPath, std::ios::binary);
        if (!f.good()) {
            err = "mmproj 文件不存在或不可读：" + opts.mmprojPath;
            return false;
        }
    }

    // 全局后端初始化，只需一次（llama 日志路由见 Logging.cpp）
    static std::once_flag backendInit;
    std::call_once(backendInit, [] { llama_backend_init(); });

    llama_model_params mparams = llama_model_default_params();
    mparams.n_gpu_layers = opts.n_gpu_layers; // -1 = 全部层卸载到 GPU（Metal），0 = 纯 CPU

    m_model = llama_model_load_from_file(modelPath.c_str(), mparams);
    if (!m_model) {
        err = "模型加载失败：" + modelPath + "\n（详细原因请查看终端输出的 llama.cpp 日志）";
        return false;
    }

    llama_context_params cparams = llama_context_default_params();
    cparams.n_ctx   = opts.n_ctx;
    cparams.n_batch = opts.n_batch;
    if (opts.n_threads > 0) {
        cparams.n_threads       = opts.n_threads;
        cparams.n_threads_batch = opts.n_threads;
    }

    m_ctx = llama_init_from_model(m_model, cparams);
    if (!m_ctx) {
        err = "上下文初始化失败（可能是 n_ctx 过大导致内存不足）";
        llama_model_free(m_model);
        m_model = nullptr;
        return false;
    }

    m_nBatch = llama_n_batch(m_ctx);

    // 多模态投影模型（可选）：加载后即可发送图片
    if (!opts.mmprojPath.empty()) {
        // 后端注册由 llama_backend_init() 完成，无需再调用 ggml_backend_load_all()

        mtmd_context_params mtmdParams = mtmd_context_params_default();
        mtmdParams.print_timings = false;
        // 非因果注意力的视觉模型需要整张图片在一个 ubatch 内解码
        const mtmd_memory_usage mem = mtmd_get_memory_usage(opts.mmprojPath.c_str(), mtmdParams);
        const int n_ubatch = (int) llama_n_ubatch(m_ctx);
        if (mem.use_non_causal && mem.image_max_tokens > n_ubatch) {
            mtmdParams.image_max_tokens = n_ubatch;
            mtmdParams.image_min_tokens = std::min(mtmdParams.image_min_tokens, n_ubatch);
        }

        m_mtmd.reset(mtmd_init_from_file(opts.mmprojPath.c_str(), m_model, mtmdParams));
        if (!m_mtmd) {
            err = "mmproj（多模态投影）加载失败：" + opts.mmprojPath
                + "\n（该投影模型可能与此 llama.cpp 版本或主模型不匹配）";
            unload();
            return false;
        }
    }

    // 聊天模板：从模型的 GGUF 元数据中读取（Gemma / Qwen / Llama 等均可）
    m_tmpls = common_chat_templates_init(m_model, "", "", "");
    if (!m_tmpls) {
        err = "模型缺少聊天模板（chat template），无法进行对话";
        unload();
        return false;
    }

    return true;
}

void LlamaChat::unload() {
    m_mtmd.reset();
    m_tmpls.reset();
    m_kvSize = 0;
    m_decodedText.clear();
    if (m_ctx) {
        llama_free(m_ctx);
        m_ctx = nullptr;
    }
    if (m_model) {
        llama_model_free(m_model);
        m_model = nullptr;
    }
}

std::string LlamaChat::modelDesc() const {
    if (!m_model) {
        return {};
    }
    char buf[256];
    const int32_t n = llama_model_desc(m_model, buf, sizeof(buf));
    if (n <= 0) {
        return "未知模型";
    }
    return std::string(buf, (size_t) n);
}

bool LlamaChat::renderPrompt(const std::vector<ChatMessage> & history,
                             const ChatOptions & opts,
                             std::string & promptOut,
                             std::string & err,
                             bool skipEmpty,
                             bool withGenPrompt) const {
    std::vector<common_chat_msg> msgs;
    msgs.reserve(history.size());
    for (const auto & m : history) {
        if (skipEmpty && m.content.empty()) {
            continue; // 空消息（如模型直接以 EOS 结束）不参与模板渲染
        }
        common_chat_msg msg;
        msg.role    = m.role;
        msg.content = m.content;
        msgs.push_back(std::move(msg));
    }
    if (msgs.empty()) {
        err = "对话内容为空";
        return false;
    }

    common_chat_templates_inputs inputs;
    inputs.messages             = std::move(msgs);
    inputs.enable_thinking      = opts.enable_thinking;
    inputs.add_generation_prompt = withGenPrompt;

    const common_chat_params cparams = common_chat_templates_apply(m_tmpls.get(), inputs);
    promptOut = cparams.prompt.str();

    // 调试：QLLAMA_DEBUG_PROMPT=1 时打印两种方式渲染的提示词，便于排查模板问题
    if (getenv("QLLAMA_DEBUG_PROMPT")) {
        fprintf(stderr, "\n===== 提示词 (common_chat_templates) =====\n%s\n===== 结束 =====\n", promptOut.c_str());

        std::vector<llama_chat_message> cmsgs;
        for (const auto & m : history) {
            if (!m.content.empty()) {
                cmsgs.push_back({m.role.c_str(), m.content.c_str()});
            }
        }
        std::string buf(promptOut.size() * 2 + 256, '\0');
        int32_t n = llama_chat_apply_template(
            llama_model_chat_template(m_model, nullptr), cmsgs.data(), cmsgs.size(), true,
            buf.data(), (int32_t) buf.size());
        if (n < 0) {
            buf.resize((size_t) -n + 1);
            n = llama_chat_apply_template(
                llama_model_chat_template(m_model, nullptr), cmsgs.data(), cmsgs.size(), true,
                buf.data(), (int32_t) buf.size());
        }
        fprintf(stderr, "\n===== 提示词 (llama_chat_apply_template) =====\n%s\n===== 结束 =====\n",
                n >= 0 ? buf.c_str() : "(渲染失败)");
    }
    return true;
}

bool LlamaChat::chat(const std::vector<ChatMessage> & history,
                     const ChatOptions & opts,
                     const std::function<void(const std::string &)> & onPiece,
                     const std::atomic_bool & stopFlag,
                     ChatResult & result,
                     std::string & err) {
    if (!isLoaded()) {
        err = "模型尚未加载";
        return false;
    }

    // 图片只支持出现在最后一条消息（本轮新发送的消息）中
    const bool lastMsgHasImages = !history.empty() && !history.back().imagePaths.empty();

    // 1. 渲染聊天模板（末尾带助手回复引导词；图片消息的 <__media__> 标记原样保留）
    std::string prompt;
    if (!renderPrompt(history, opts, prompt, err)) {
        return false;
    }

    const llama_vocab * vocab = llama_model_get_vocab(m_model);

    // 2. 长对话增量复用（文本级前缀比对，与 llama.cpp common 的做法一致）：
    //    新提示词 = 旧提示词 + 上一轮回复 + 本轮追加的模板片段（文本层面必然前缀匹配），
    //    因此只需分词并解码新增的文本后缀，续接在现有 KV 之后；
    //    任何不匹配（如中途修改了系统提示词、清空了对话）都回退为全量重算。
    const bool reuse = !m_decodedText.empty()
                       && prompt.size() >= m_decodedText.size()
                       && prompt.compare(0, m_decodedText.size(), m_decodedText) == 0;
    const std::string suffix = reuse ? prompt.substr(m_decodedText.size()) : prompt;

    if (getenv("QLLAMA_DEBUG_PROMPT") && !reuse && !m_decodedText.empty()) {
        size_t i = 0;
        const size_t n = std::min(m_decodedText.size(), prompt.size());
        while (i < n && m_decodedText[i] == prompt[i]) {
            i++;
        }
        const size_t from = i > 24 ? i - 24 : 0;
        fprintf(stderr, "[KV 复用失配 @ %zu/%zu] 已解码: ...%s...\n", i, m_decodedText.size(),
                m_decodedText.substr(from, 48).c_str());
        fprintf(stderr, "[KV 复用失配 @ %zu/%zu] 新提示: ...%s...\n", i, prompt.size(),
                prompt.substr(from, 48).c_str());
    }

    // 3. 多模态判定：以「待解码文本中有媒体标记」为准——
    //    增量复用时只有新消息的标记（取新消息图片）；全量重算时历史里的标记
    //    都要重新编码（按历史顺序收集所有消息的图片）。
    const std::string marker = mtmd_default_marker();
    size_t nMarkers = 0;
    for (size_t p = 0; (p = suffix.find(marker, p)) != std::string::npos; p += marker.size()) {
        nMarkers++;
    }
    const bool hasMedia = nMarkers > 0;
    if (hasMedia && !m_mtmd) {
        err = "对话包含图片，但当前模型未加载 mmproj（多模态投影），无法处理";
        return false;
    }
    std::vector<std::string> mediaPaths;
    if (hasMedia) {
        if (reuse) {
            mediaPaths = history.back().imagePaths;
        } else {
            for (const auto & m : history) {
                mediaPaths.insert(mediaPaths.end(), m.imagePaths.begin(), m.imagePaths.end());
            }
        }
        if (mediaPaths.size() != nMarkers) {
            err = "图片数量与消息中的媒体标记数量不匹配";
            return false;
        }
    }
    (void) lastMsgHasImages;

    llama_batch batch = llama_batch_init(m_nBatch, 0, 1);
    int32_t nDecoded = 0; // 本轮实际新解码的 token 数

    if (hasMedia) {
        // —— 多模态路径：mtmd_tokenize_from_parts 分词 + mtmd_helper_eval_chunks 解码 ——

        // 加载图片位图（tokenize + encode 期间必须保持存活）
        std::vector<mtmd_bitmap *> bitmaps;
        bitmaps.reserve(mediaPaths.size());
        for (const auto & path : mediaPaths) {
            const mtmd_helper_bitmap_wrapper wrapper =
                mtmd_helper_bitmap_init_from_file(m_mtmd.get(), path.c_str(), false, mtmd_helper_init_opt_default());
            if (!wrapper.bitmap) {
                for (auto * b : bitmaps) {
                    mtmd_bitmap_free(b);
                }
                llama_batch_free(batch);
                err = "图片加载失败：" + path;
                return false;
            }
            bitmaps.push_back(wrapper.bitmap);
        }

        // 按标记拆分文本段，与位图交织为 parts
        std::vector<std::string> segments;
        segments.reserve(nMarkers + 1);
        {
            size_t start = 0;
            size_t pos = 0;
            while ((pos = suffix.find(marker, start)) != std::string::npos) {
                segments.push_back(suffix.substr(start, pos - start));
                start = pos + marker.size();
            }
            segments.push_back(suffix.substr(start));
        }

        std::vector<mtmd_input_text> texts(segments.size());
        std::vector<mtmd_input_part> parts;
        parts.reserve(segments.size() + bitmaps.size());
        for (size_t i = 0; i < segments.size(); i++) {
            texts[i] = {segments[i].data(), segments[i].size(), /* add_special */ false, /* parse_special */ true};
            parts.push_back({&texts[i], nullptr});
            if (i < bitmaps.size()) {
                parts.push_back({nullptr, bitmaps[i]});
            }
        }
        std::vector<const mtmd_input_part *> partsPtr;
        partsPtr.reserve(parts.size());
        for (const auto & p : parts) {
            partsPtr.push_back(&p);
        }

        mtmd_input_chunks * chunks = mtmd_input_chunks_init();
        // 与纯文本路径一致：KV 为空（全量重算）时补 BOS
        const int32_t resTok = mtmd_tokenize_from_parts(m_mtmd.get(), chunks,
                                                        partsPtr.data(), partsPtr.size(), !reuse);
        if (resTok != 0) {
            mtmd_input_chunks_free(chunks);
            for (auto * b : bitmaps) {
                mtmd_bitmap_free(b);
            }
            llama_batch_free(batch);
            err = "多模态分词失败（错误码 " + std::to_string(resTok) + "）";
            return false;
        }

        if (!reuse) {
            llama_memory_clear(llama_get_memory(m_ctx), true);
            m_kvSize = 0;
            m_decodedText.clear();
        }

        // 解码全部 chunk（文本解码 + 图片编码嵌入一步完成）
        llama_pos newPos = (llama_pos) m_kvSize;
        const int32_t resEval = mtmd_helper_eval_chunks(m_mtmd.get(), m_ctx, chunks,
                                                        newPos, 0, (int32_t) m_nBatch,
                                                        /* logits_last */ true, &newPos);
        if (resEval != 0) {
            mtmd_input_chunks_free(chunks);
            for (auto * b : bitmaps) {
                mtmd_bitmap_free(b);
            }
            llama_batch_free(batch);
            m_kvSize = 0;
            m_decodedText.clear();
            llama_memory_clear(llama_get_memory(m_ctx), true); // 失败后 KV 状态不可靠，重置
            err = "多模态解码失败（错误码 " + std::to_string(resEval) + "）";
            return false;
        }
        nDecoded = (int32_t) ((llama_pos) newPos - (llama_pos) m_kvSize);
        m_kvSize = (size_t) newPos;
        m_decodedText = prompt;

        mtmd_input_chunks_free(chunks);
        for (auto * b : bitmaps) {
            mtmd_bitmap_free(b);
        }
    } else {
        // —— 纯文本路径 ——
        // 3. 分词（parse_special=true：模板中的 <|im_start|> 等特殊 token 必须被解析）
        std::vector<llama_token> tokens(suffix.size() + 16);
        int32_t nTok = llama_tokenize(vocab, suffix.c_str(), (int32_t) suffix.size(),
                                      tokens.data(), (int32_t) tokens.size(), false, true);
        if (nTok < 0) {
            tokens.resize((size_t) -nTok); // 容量不够，按提示的大小重试
            nTok = llama_tokenize(vocab, suffix.c_str(), (int32_t) suffix.size(),
                                  tokens.data(), (int32_t) tokens.size(), false, true);
        }
        if (nTok <= 0) {
            llama_batch_free(batch);
            err = "提示词分词结果为空";
            return false;
        }
        tokens.resize((size_t) nTok);

        // 与 llama-simple-chat 一致：KV 为空时需要 BOS token 开头，否则 Gemma / Qwen
        // 会退化为回声循环。手动补 BOS 而非 add_special=true，避免某些词汇表误加 EOS。
        // 增量复用时 KV 开头已有 BOS，模板渲染结果不含 BOS，无需补。
        if (!reuse && llama_vocab_get_add_bos(vocab)) {
            const llama_token tokBos = llama_vocab_bos(vocab);
            if (tokens.empty() || tokens.front() != tokBos) {
                tokens.insert(tokens.begin(), tokBos);
            }
        }

        // 4. 解码提示词：复用则从 KV 末尾续解码，否则清空 KV 从头解码
        const int32_t startPos = (int32_t) m_kvSize;
        if (!reuse) {
            llama_memory_clear(llama_get_memory(m_ctx), true);
            m_kvSize = 0;
            m_decodedText.clear();
        }

        const int32_t nPrompt = (int32_t) tokens.size();
        for (int32_t pos = 0; pos < nPrompt; pos += (int32_t) m_nBatch) {
            const int32_t chunk = std::min<int32_t>((int32_t) m_nBatch, nPrompt - pos);
            batch.n_tokens = chunk;
            for (int32_t i = 0; i < chunk; i++) {
                batch.token   [i]    = tokens[pos + i];
                batch.pos     [i]    = startPos + pos + i;
                batch.n_seq_id[i]    = 1;
                batch.seq_id  [i][0] = 0;
                batch.logits  [i]    = false;
            }
            batch.logits[chunk - 1] = true; // 只需最后一个提示 token 的 logits 供采样
            const int32_t code = llama_decode(m_ctx, batch);
            if (code != 0) {
                llama_batch_free(batch);
                m_kvSize = 0;
                m_decodedText.clear();
                llama_memory_clear(llama_get_memory(m_ctx), true); // 失败后 KV 状态不可靠，重置
                err = decodeError(code);
                return false;
            }
            m_kvSize += (size_t) chunk;
        }
        m_decodedText = prompt;
        nDecoded = nPrompt;
    }

    result.prompt_tokens = nDecoded;

    // 采样链：top-k → top-p → min-p → 温度 → 分布采样
    llama_sampler * smpl = llama_sampler_chain_init(llama_sampler_chain_default_params());
    llama_sampler_chain_add(smpl, llama_sampler_init_top_k(opts.top_k));
    llama_sampler_chain_add(smpl, llama_sampler_init_top_p(opts.top_p, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_min_p(opts.min_p, 1));
    llama_sampler_chain_add(smpl, llama_sampler_init_temp(opts.temperature));
    llama_sampler_chain_add(smpl, llama_sampler_init_dist(opts.seed));

    // 逐 token 生成
    char pieceBuf[512];

    const auto t0 = std::chrono::steady_clock::now();
    int32_t nGen = 0;
    std::string replyRaw;
    while (nGen < opts.n_predict) {
        if (stopFlag.load(std::memory_order_relaxed)) {
            break; // 用户点击了“停止”
        }

        const llama_token tok = llama_sampler_sample(smpl, m_ctx, -1);
        llama_sampler_accept(smpl, tok);
        if (llama_vocab_is_eog(vocab, tok)) {
            break; // 模型说完了（EOS / EOT 等结束标记）
        }

        // 特殊 token（如 <think>）渲染为空，直接跳过
        const int32_t pieceLen = llama_token_to_piece(vocab, tok, pieceBuf, sizeof(pieceBuf) - 1, 0, false);
        if (pieceLen > 0) {
            pieceBuf[pieceLen] = '\0';
            replyRaw.append(pieceBuf, (size_t) pieceLen);
            if (onPiece) {
                onPiece(std::string(pieceBuf, (size_t) pieceLen));
            }
        }

        // 解码刚生成的 token，为下一次采样准备 logits
        batch.n_tokens = 1;
        batch.token   [0]    = tok;
        batch.pos     [0]    = (int32_t) m_kvSize; // KV 位置接在已解码内容之后
        batch.n_seq_id[0]    = 1;
        batch.seq_id  [0][0] = 0;
        batch.logits  [0]    = true;
        const int32_t code = llama_decode(m_ctx, batch);
        if (code != 0) {
            llama_batch_free(batch);
            llama_sampler_free(smpl);
            m_kvSize = 0;
            m_decodedText.clear();
            llama_memory_clear(llama_get_memory(m_ctx), true);
            err = decodeError(code);
            return false;
        }
        m_kvSize++;
        nGen++;
    }

    llama_sampler_free(smpl);
    llama_batch_free(batch);

    // 渲染对齐（保证下一轮渲染文本与已解码文本前缀一致）：
    // 用渲染器自身重建状态文本——完整历史（含本轮回复）末尾追加一个空 user 占位，
    // 使最后一条 assistant 以“历史消息”形式渲染（不含思考块、不含生成引导词），
    // 再减去空 user 占位部分。下一轮的完整渲染必然以该文本为前缀。
    // 若个别模板规则特殊导致失配，下一轮会自动回退全量重算（结果仍正确，仅性能略降）。
    std::string stateRender;
    std::string tmpErr;
    {
        std::vector<ChatMessage> stateHistory = history;
        stateHistory.push_back(ChatMessage{"assistant", replyRaw});
        stateHistory.push_back(ChatMessage{"user", ""}); // 空 user 占位
        if (renderPrompt(stateHistory, opts, stateRender, tmpErr, /* skipEmpty */ false, /* withGenPrompt */ false)) {
            // 空 user 占位部分：单独渲染一条空 user 消息得到
            std::string emptyUserPart;
            if (renderPrompt({{"user", ""}}, opts, emptyUserPart, tmpErr, /* skipEmpty */ false, /* withGenPrompt */ false)
                && stateRender.size() >= emptyUserPart.size()
                && stateRender.compare(stateRender.size() - emptyUserPart.size(),
                                       emptyUserPart.size(), emptyUserPart) == 0) {
                m_decodedText = stateRender.substr(0, stateRender.size() - emptyUserPart.size());
            } else {
                m_decodedText = stateRender; // 兜底：占位识别失败时整体作为状态文本
            }
        } else {
            m_decodedText = prompt; // 兜底：渲染失败时退回旧对齐方式
        }
    }

    const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    result.generated_tokens = nGen;
    result.tokens_per_sec   = secs > 0.0 ? (double) nGen / secs : 0.0;
    return true;
}
