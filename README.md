**Язык: ** [English](README.md) | [Русский](README.ru.md)

# urcontrol-c -- Steinberg UR-C audio interface control library in C

A port of the core functionality of [askoriy/urcontrol](https://github.com/askoriy/urcontrol) from **Pytrhon** to **C11**. Parameter compatibility has only been verified with the **UR22C**. Other devices, such as the UR24C and similar models, may also work, but compatibility is not guaranteed. The library communicates with the audio interface's internal DSP mixer MIDI port via **ALSA rawmidi**.

## Project structure

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