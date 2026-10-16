**Language: ** [English](README.md) | [Русский](README.ru.md)

# urcontrol-c -- управление звуковой картой Steinberg UR-C на языке C

Порт основных возможностей проекта [askoriy/urcontrol](https://github.com/askoriy/urcontrol) с языка **Python** на **C11**. Совместимость параметров проверена только на **UR22C**, возможна, но не гарантируется, работа на UR24C и подобных. Для управления используется внутренний MIDI-порт DSP-микшера через **ALSA rawmidi**.

## Сборка

Требуется ОС семейства Linux, gcc или clang, POSIX threads, `libasound.so.2` (системная ALSA). 

```bash
make
```

Результат:

- `liburcontrol.a` -- библиотека C11;
- `urcontrol-cli` -- программа командной строки.

## Как найти MIDI-порт

```bash
amidi -l
```

или

```bash
./urcontrol-cli --get-midi-ports
```

У Steinberg может быть два порта: **физический MIDI** для MIDI-устройств и **внутренний порт для управления микшером**. Нужен второй. Выбирайте его явно, библиотека не будет угадывать и случайно отправлять команды в MIDI DIN. В примерах `hw:2,0,1` - **только образец**, ваш адрес может отличаться.

### urcontrol-cli

```bash
# Все 13 блоков
./urcontrol-cli --list-units

# Все параметры микшера с ID, диапазонами, значениями по умолчанию
# Используйте grep для выбора нужных
./urcontrol-cli -u mixer --list-parameters --verbose

# Прочитать уровень входа 1 на Mix 3 (канал вводится начиная с 1) 
# (уровень громкости входа 1 на мониторинге в наушники)
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --get-parameter InputMix3Volume

# Установить 0 dB (соотвествует 103) на входе 1 в Mix 3, затем прочитать и проверить
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --set-parameter InputMix3Volume 103

# Узнать, включена ли запись DSP-обработанного сигнала для входа 1
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --get-parameter InputFXRec

# Прочитать напрямую ID=213
./urcontrol-cli --port hw:2,0,1 -i 1 --get-id 213

# Явно послать keepalive 
./urcontrol-cli --port hw:2,0,1 --keepalive

# Разобрать полученный SysEx без устройства
./urcontrol-cli --decode-sysex F043103E140104020001550000000000000067F7

```

Ключ ``--set-parameter` понимает `min`, `max`, `def`, например `--set-parameter InputMix3Volume def`.
У параметров, для которых исходник не задает значение по умолчанию, `def` запрещен.  
Ключ `--no-confirm` отключает проверку после отправки.
Ключ `--timeout 3000` задает таймаут в миллисекундах

## Структура

- `urcontrol.h` -- публичный C API;
- `urcontrol.c` -- работа с SysEx, ALSA MIDI, потоки и ожидание ответов;
- `urcontrol_params.c` -- все именованные параметры с ID, диапазонами и примечаниями;
- `urcontrol_cli.c` -- аргументы и команды CLI;
- `Makefile` -- мейкфайл для сборки библиотеки и тестов;
- `LICENSE` -- лицензия.

## Лицензия и происхождение 

Основано на [askoriy/urcontrol](https://github.com/askoriy/urcontrol), Oleksandr Skoryi, лицензия MIT.

Оригинальное уведомление включено в `LICENSE`.

**Программное обеспечение предоставляется на условиях AS IS (КАК ЕСТЬ), без каких-либо гарантий. Авторы не несут ответственности за любые последствия, убытки или ущерб, возникшие в результате его использования.**