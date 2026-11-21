// 冒烟测试（无 GUI）：验证 llama.cpp 链接、聊天模板渲染与生成流程。
// 用法: ./qllama_smoke <模型路径> [提示词1] [提示词2] ... [--mmproj <路径>] [--image <图片> ...]
//   多个提示词 = 多轮对话；只有一个提示词时默认问“你好，请用一句话介绍你自己”。
//   --mmproj 加载多模态投影模型；--image 图片随第一轮提示词发送（可多次指定）。

#include "LlamaChat.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

int main(int argc, char * argv[]) {
    if (argc < 2) {
        printf("用法: %s <模型路径> [提示词...] [--mmproj <路径>] [--image <图片>...]\n", argv[0]);
        return 1;
    }

    ChatOptions opts;
    opts.n_predict = 256;

    std::vector<std::string> prompts;
    std::vector<std::string> images;
    for (int i = 2; i < argc; i++) {
        if (std::strcmp(argv[i], "--mmproj") == 0 && i + 1 < argc) {
            opts.mmprojPath = argv[++i];
        } else if (std::strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            images.push_back(argv[++i]);
        } else {
            prompts.push_back(argv[i]);
        }
    }
    if (prompts.empty()) {
        prompts.push_back("你好，请用一句话介绍你自己。");
    }

    LlamaChat chat;
    std::string err;
    if (!chat.load(argv[1], opts, err)) {
        printf("加载失败: %s\n", err.c_str());
        return 1;
    }
    printf("模型: %s%s\n", chat.modelDesc().c_str(), chat.hasVision() ? "（多模态）" : "");

    std::vector<ChatMessage> history;
    for (size_t turn = 0; turn < prompts.size(); turn++) {
        ChatMessage msg{"user", prompts[turn]};
        if (turn == 0 && !images.empty()) {
            // 图片消息：content = 每张图片一个 <__media__> 标记 + 文本
            for (size_t i = 0; i < images.size(); i++) {
                msg.content = "<__media__>" + msg.content;
            }
            msg.imagePaths = images;
        }

        printf("\n[第 %zu 轮] 你: %s\n", turn + 1, prompts[turn].c_str());
        history.push_back(msg);

        std::atomic_bool stop{false};
        ChatResult result;
        std::string reply;
        const bool ok = chat.chat(history, opts,
            [&reply](const std::string & s) {
                reply += s;
                fwrite(s.data(), 1, s.size(), stdout);
                fflush(stdout);
            },
            stop, result, err);

        printf("\n--- 第 %zu 轮: 解码 %d token · 生成 %d token · %.1f tok/s ---\n",
               turn + 1, result.prompt_tokens, result.generated_tokens, result.tokens_per_sec);
        if (!ok) {
            printf("生成失败: %s\n", err.c_str());
            return 1;
        }
        history.push_back({"assistant", reply});
    }
    return 0;
}
