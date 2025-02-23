# xiaoai_speaker_hack

一个将小米 Linux 系统连接到 ChatGPT 的项目，支持 WebRTC 或 WebSocket。

## 安装

X86:
要在 Linux 上安装必要的依赖项，请运行以下命令：

```sh
sudo apt install nlohmann-json3-dev libasound2-dev libcurl4 libcurl4-openssl-dev libopus-dev libwebsockets-dev
```
ARM:
下载工具链：
https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/

## 使用

按照以下步骤构建和运行项目：

1. 克隆仓库：
    ```sh
    git clone https://github.com/yourusername/xiaoai_speaker_hack.git
    cd xiaoai_speaker_hack
    ```

2. 构建项目：
    对于 ARM：
    ```sh
    cmake -S . -B build --toolchain=toolchain.cmake
    ```
    对于 x86：
    ```sh
    cmake -S . -B build
    ``` 

3. 运行应用程序：
    从网站下载配置(自建http服务器)：
    ```sh
    ./xiaoai_speaker_hack --host <address> --key <key_string> --websocket
    ```

    或者

    ```sh
    ./xiaoai_speaker_hack --host <address> --key <key_string> --webrtc
    ```

    使用本地配置文件 (/data/env_config)：
    ```sh
    ./xiaoai_speaker_hack --websocket
    ```

    或者

    ```sh
    ./xiaoai_speaker_hack --webrtc
    ```

## 配置

可以使用环境变量配置应用程序。您可以在 `/data/env_config` 文件中设置这些变量。配置文件应具有以下格式：

```
# 示例配置
OPENAI_API_KEY=your_openai_api_key
OPENAI_REALTIMEAPI=https://api.openai-hk.com/v1/realtime?model=gpt-4o-realtime-preview-2024-10-01
```

## 功能

- 使用 WebRTC 或 WebSocket 连接到 ChatGPT。
- 监控文件更改并处理指令。
- 从服务器或本地环境变量中检索配置。

## 致谢

该项目基于以下仓库：

- [duhow/xiaoai-patch](https://github.com/duhow/xiaoai-patch)
- [openai/openai-realtime-embedded-sdk](https://github.com/openai/openai-realtime-embedded-sdk)

## 许可证

该项目根据 MIT 许可证授权。有关详细信息，请参阅 [LICENSE](LICENSE) 文件。