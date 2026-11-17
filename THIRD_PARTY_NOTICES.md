# Third-party components

- **Qt** — application framework; dynamically linked from the selected Qt installation. Licensing depends on the Qt distribution and modules. See [Qt licensing](https://www.qt.io/licensing/).
- **whisper.cpp v1.9.5**, including its bundled ggml — fetched during configuration and linked into ASR-enabled builds. MIT license; see the [upstream license](https://github.com/ggml-org/whisper.cpp/blob/v1.9.5/LICENSE) and the fetched source tree. Source archive SHA-256: `ff1a9053feb509ff9d7729703355541ae9690073a6b1c40eb692c962e0dc1720`.
- **Whisper base.en model** — downloaded separately from the [whisper.cpp model repository](https://huggingface.co/ggerganov/whisper.cpp). OpenAI Whisper code and model weights are released under MIT; see [Whisper LICENSE](https://github.com/openai/whisper/blob/main/LICENSE). Model SHA-256: `a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002`.
- **FFmpeg** — separately installed executable, invoked locally through QProcess for decoding audio. No FFmpeg executable is copied into this repository. See [FFmpeg licensing](https://ffmpeg.org/legal.html) for the selected binary distribution.

The current `.app` is a developer build. A redistributable package must include the notices and license texts for its exact dependencies.

- **Ollama** — separately installed local inference server, accessed over loopback HTTP. See the [upstream license](https://github.com/ollama/ollama/blob/main/LICENSE). Its executable and models are not bundled by CMake.
- **Qwen2.5 3B Instruct (qwen2.5:3b)** — optional translation model downloaded through Ollama (approximately 1.9 GB). This 3B model uses the Qwen Research License, not the Apache license used by several other Qwen2.5 sizes. See the [model card and license](https://ollama.com/library/qwen2.5:3b).
