**Язык: ** [English](README.md) | [Русский](README.ru.md)

# urcontrol-c -- Steinberg UR-C audio interface control library in C

A port of the core functionality of [askoriy/urcontrol](https://github.com/askoriy/urcontrol) from **Pytrhon** to **C11**. Parameter compatibility has only been verified with the **UR22C**. Other devices, such as the UR24C and similar models, may also work, but compatibility is not guaranteed. The library communicates with the audio interface's internal DSP mixer MIDI port via **ALSA rawmidi**.


## Building

Requires a Linux-based operating system, GCC or Clang, POSIX threads, and `libasound.so.2` (the system ALSA library).

```bash
make
```

Build outputs:

- `liburcontrol.a` -- C11 library;
- `urcontrol-cli` -- command-line tool.

## Finding the MIDI Port

```bash
amidi -l
```

or

```bash
./urcontrol-cli --get-midi-ports
```

Steinberg interfaces may expose two MIDI ports: a **physical MIDI port** for external MIDI devices and an **internal port for mixer control**. You need the latter. Select it explicitly: the library will not guess which port to use and risk sending commands to the physical MIDI DIN port. The address `hw:2,0,1` in the examples is **only an example**; your port address may differ.

### urcontrol-cli

```bash
# List all 13 parameter units
./urcontrol-cli --list-units

# List all mixer parameters with IDs, ranges, and defaults
# Use grep to find specific parameters
./urcontrol-cli -u mixer --list-parameters --verbose

# Read the level of input 1 in Mix 3 (CLI input numbers start at 1)
# (the level of input 1 in the headphone monitor mix)
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --get-parameter InputMix3Volume

# Set input 1 in Mix 3 to 0 dB (encoded value 103), then read back and verify
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --set-parameter InputMix3Volume 103

# Check whether recording of the DSP-processed signal is enabled for input 1
./urcontrol-cli --port hw:2,0,1 -u mixer -i 1 --get-parameter InputFXRec

# Read parameter ID 213 directly
./urcontrol-cli --port hw:2,0,1 -i 1 --get-id 213

# Explicitly send a keepalive message
./urcontrol-cli --port hw:2,0,1 --keepalive

# Decode a received SysEx message without connecting a device
./urcontrol-cli --decode-sysex F043103E140104020001550000000000000067F7
```

The `--set-parameter` option accepts `min`, `max`, and `def`, for example: `--set-parameter InputMix3Volume def`.
The `def` value is unavailable for parameters whose defaults are not specified by the original project.  
The `--no-confirm` option disables readback verification after writing.  
The `--timeout 3000` option sets the timeout in milliseconds.

## Project Structure

- `urcontrol.h` -- public C API;
- `urcontrol.c` -- SysEx handling, ALSA MIDI transport, threading, and response handling;
- `urcontrol_params.c` -- named parameters with IDs, ranges, and notes;
- `urcontrol_cli.c` -- command-line argument parsing and commands;
- `Makefile` -- build configuration for the library and tests;
- `LICENSE` -- software license.

## License and attribution

Based on [askoriy/urcontrol](https://github.com/askoriy/urcontrol) by Oleksandr Skoryi, licensed under the MIT License.

The original copyright notice is preserved in the `LICENSE` file.

**THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.**