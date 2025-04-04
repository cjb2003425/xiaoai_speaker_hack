# xiaoai_speaker_hack

A project that patches the Xiaomi Linux system to connect to ChatGPT with WebRTC or WebSocket.

## Installation

X86:
To install the necessary dependencies for Linux, run the following command:

```sh
sudo apt install nlohmann-json3-dev libasound2-dev libcurl4 libcurl4-openssl-dev libopus-dev libwebsockets-dev
```
ARM:
Download toolchain from:
https://releases.linaro.org/components/toolchain/binaries/7.5-2019.12/arm-linux-gnueabihf/

## Usage

To build and run the project, follow these steps:

1. Clone the repository:
    ```sh
    git clone https://github.com/cjb2003425/xiaoai_speaker_hack.git
    cd xiaoai_speaker_hack
    git submodule update --init --recursive
    ```

2. Build the project:

    For arm:
    ```sh
    cmake -S . -B build --toolchain=toolchain.cmake
    cmake --build build
    ```
    For x86:
    ```sh
    cmake -S . -B build
    cmake --build build
    ``` 

3. Run the application:

    Download config from website(your own http server):
    ```sh
    ./chat --host <address> --key <key_string> --websocket
    ```

    or

    ```sh
    ./chat --host <address> --key <key_string> --webrtc
    ```

    Use the local config file (/data/env_config):
    ```sh
    ./chat --websocket
    ```

    or

    ```sh
    ./chat --webrtc
    ```

## Configuration

The application can be configured using environment variables. You can set these variables in a configuration file located at `/data/env_config`. The configuration file should have the following format:

```
# Example configuration
OPENAI_API_KEY=your_openai_api_key
OPENAI_REALTIMEAPI=https://api.openai-hk.com/v1/realtime?model=gpt-4o-realtime-preview-2024-10-01
```

## Features

- Connects to ChatGPT using WebRTC or WebSocket.
- Monitors file changes and processes instructions.
- Retrieves configuration from a server or local environment variables.

## Credits

This project is based on the following repositories:

- [duhow/xiaoai-patch](https://github.com/duhow/xiaoai-patch)
- [openai/openai-realtime-embedded-sdk](https://github.com/openai/openai-realtime-embedded-sdk)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
