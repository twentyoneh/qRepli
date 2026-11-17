# Проверка минимальной реализации

Дата: 16 ноября 2026 года. Среда: macOS ARM64, Apple M1 Pro, 16 ГБ, Qt 6.11.2, whisper.cpp v1.9.5, модель base.en.

## Сборка и автоматические проверки

- `cmake --build build --parallel 6` — успешно, сборка RelWithDebInfo.
- `ctest --test-dir build --output-on-failure` — три набора: subtitles, player, transcription. Проверки с реальной моделью в обычном запуске пропускаются, если не заданы переменные ниже.
- Parser/model: UTF-8/BOM/CRLF, оформление, несколько строк, сортировка и перекрытие реплик, исключительная правая граница, невалидные блоки/кодировка, обратный seek и reset, экспорт с повторным чтением.
- Player: реальный локальный WAV, play/pause/seek, независимые времена оригинала/перевода, ошибочный SRT сохраняет прежнюю дорожку, смена источника очищает текст, режим только с переводом.
- Audio/ASR: FFmpeg читает заданный интервал независимо от плеера, путь с пробелами, некорректный исполняемый файл/диапазон, отмена и отбрасывание устаревшего результата, ошибка модели.

## Проверки настоящего распознавания

Один раз создано видео `build/speech-demo.mp4` с тестовой таблицей и английской речью Samantha. Исходный текст:

> Welcome to our video player. We can recognize English speech on this computer. The subtitles appear while you watch the video. You can pause the video and jump to another sentence.

Модель `models/ggml-base.en.bin` проверена по SHA-256 `a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002`.

1. `test_transcription realModelSmokeTest` с `QREPLI_TEST_MODEL` и `QREPLI_TEST_AUDIO`: успешно. Получен ожидаемый английский текст и допустимые временные интервалы. Первый прогон вместе с загрузкой/первой подготовкой Metal занял около 16,1 с. Лог: `build/asr-smoke.log`.
2. `test_transcription boundedLookaheadAndRestart` с `QREPLI_TEST_MODEL` и `QREPLI_TEST_LONG_AUDIO=.../build/long-speech.wav`: успешно. Для 65-секундного файла при playhead=0 подготовлены только интервалы 0–20 и 20–40 с; worker переходит в ожидание. После замены задачи на 55 с выдаётся только новый интервал до 65 с. Лог: `build/asr-long-test.log`.
3. `test_player realTranscriptionAndSourceChange`: успешно. Плеер ожидает ASR, ручная пауза во время подготовки сохраняется, готовая реплика доступна после seek, экспорт читается обратно; экспорт поверх видео отклоняется; смена источника во время повторного ASR очищает результаты. Лог: `build/asr-player-test.log`.

В длинном синтетическом тесте после прогрева измерено: загрузка модели 201/109 мс, инференс окон 20/20/10 с — 313/300/196 мс. Это время инференса на повторяемой синтезированной речи, без обещаний такой же скорости и качества на фильмах. Отдельного длительного измерения RSS/энергопотребления не проводилось.

Воспроизведение тестового материала на macOS:

```sh
say -v Samantha -o build/speech.aiff \
  'Welcome to our video player. We can recognize English speech on this computer. The subtitles appear while you watch the video. You can pause the video and jump to another sentence.'
ffmpeg -hide_banner -loglevel error -y \
  -f lavfi -i 'testsrc2=size=960x540:rate=24' -i build/speech.aiff \
  -c:v libx264 -preset ultrafast -crf 28 -c:a aac -shortest build/speech-demo.mp4
ffmpeg -hide_banner -loglevel error -y -stream_loop 7 -i build/speech.aiff \
  -t 65 -ar 16000 -ac 1 build/long-speech.wav

QREPLI_TEST_MODEL="$PWD/models/ggml-base.en.bin" \
QREPLI_TEST_LONG_AUDIO="$PWD/build/long-speech.wav" \
./build/test_transcription boundedLookaheadAndRestart
```

## Проверка реального окна приложения

Проверены запуск, воспроизведение тестового MP4, две импортированные дорожки SRT, пауза, переход по реплике, стрелка назад на 5 с, отключение оригинала, полный экран и выход, Cmd+O, открытие видео через нативный диалог и очистка предыдущих субтитров.

Затем приложение запущено с настоящим ASR (`--model models/ggml-base.en.bin --transcribe`). Полученные две реплики появились поверх видео и в боковой панели. Проверены пауза и переход к распознанной реплике. Кнопкой «Сохранить SRT» создан `build/recognized-demo.srt`; файл прочитан с диска, содержит интервалы 0–5,280 и 5,280–9,840 с и фактически распознанный английский текст.

## Границы проверки

Windows/Linux, шумные реальные видео, длительный просмотр, полноценный VAD, кэш, соответствие нескольких аудиодорожек и автоматический перевод EN→RU не проверены/ещё не реализованы. Первый запуск Qt Multimedia с `QT_QPA_PLATFORM=offscreen` зависал на загрузке AVFoundation; тесты macOS используют нативный QPA. Headless Linux настроен на offscreen.
