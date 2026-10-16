/**
 * @file urcontrol_params.c
 * @brief Built-in metadata table for DSP parameters in functional units. @n
 * Таблица метаданных DSP-параметров в блоках.
 * @details Exact parameter spellings, encoded minima/maxima and defaults were
 * transcribed from askoriy/urcontrol, except the UR22C-tested correction
 * clean/Modulation: 116 instead of upstream 200. @n
 * Точные имена, числовые диапазоны и значения по умолчанию перенесены
 * из askoriy/urcontrol, кроме проверенного на UR22C исправления
 * clean/Modulation: 116 вместо 200. 
 */

#include "urcontrol.h"
#include <limits.h>
#include <string.h>
#include <strings.h>

/**
 * @brief Creates a parameter descriptor. @n
 * Создаёт дескриптор параметра.
 *
 * @param u Parameter unit name. @n 
 * Имя блока параметров.
 * @param n Parameter name. @n 
 * Имя параметра.
 * @param id Protocol parameter identifier. @n 
 * Идентификатор параметра в протоколе.
 * @param lo Minimum allowed value. @n 
 * Минимальное допустимое значение.
 * @param hi Maximum allowed value. @n 
 * Максимальное допустимое значение.
 * @param def Default value or URC_NO_DEFAULT. @n 
 * Значение по умолчанию или URC_NO_DEFAULT.
 * @param description Human-readable value descriptions. @n 
 * Текстовое описание значений.
 * @param notes Additional parameter information. @n 
 * Дополнительная информация о параметре.
 */
#define P(u,n,id,lo,hi,def,description,notes) \
    {u,n,id,lo,hi,def,description,notes}

/**
 * @brief Solo state values. @n
 * Значения состояния Solo. 
 * - 0 - Solo disabled. @n 
 *       Solo выключен. @n
 * - 1 - Solo enabled. @n
 *       Solo включён.
 */
#define S "0:unsolo; 1:solo"

/**
 * @brief Mute state values. @n
 * Значения состояния Mute. 
 * - 0 - Channel muted. @n
 *       Канал заглушён. @n
 * - 1 - Channel unmuted. @n
 *       Канал не заглушён.
 * @note The mute logic is inverted compared to a typical boolean flag. @n
 * Логика Mute инвертирована относительно обычного булевого флага.
 */
#define M "0:mute; 1:unmute"

/**
 * @brief Audio volume parameter values. @n
 * Значения параметра громкости. 
 * - 0:    Negative infinity (silence). @n
 *         Минус бесконечность (тишина). @n
 * - 1:   -74 dB. @n
 * - 103:   0 dB. @n
 * - 127: +06 dB. 
 * @note The parameter uses a nonlinear decibel scale. @n
 * Параметр использует нелинейную шкалу децибел.
 */
#define V "0:-inf; 1:-74dB; 103:0dB; 127:+6dB"

/**
 * @brief Stereo panning parameter values. @n
 * Значения параметра стереопанорамы.
 * - -16: Fully left. @n
 *        Полностью влево. @n
 * - 0:   Center. @n
 *        Центр. @n
 * - 16:  Fully right. @n
 *        Полностью вправо. 
 * @note The valid range is -16 to +16. @n
 * Допустимый диапазон: от -16 до +16.
 */
#define PAN "-16:L16; 0:C; 16:R16"

/**
 * @brief Boolean on/off parameter values. @n
 * Значения булевого параметра включения/выключения. 
 * - 0 - Disabled. @n
 *       Выключено. @n
 * - 1 - Enabled. @n
 *       Включено.
 */
#define B "0:off; 1:on"

/**
 * @brief Equalizer gain parameter values. @n
 * Значения усиления эквалайзера. 
 * - 0:   -18 dB. @n
 * - 180:   0 dB. @n
 * - 360: +18 dB. 
 * @note The gain range is -18 dB to +18 dB with a resolution of 0.1 dB per unit. @n
 * Диапазон усиления составляет от -18 до +18 дБ с шагом 0,1 дБ на единицу.
 */
#define G "0:-18dB; 180:0dB; 360:+18dB"

/**
 * @brief Guitar amplifier speaker cabinet models. @n
 * Модели акустических кабинетов гитарного усилителя.
 * - 1: BS 4x12 @n
 * - 2: AC 2x12 @n
 * - 3: AC 1x12 @n
 * - 4: 4x10 @n
 * - 5: BC 2x12 @n
 * - 6: AM 4x12 @n
 * - 7: YC 4x12 @n
 * - 8: JC 2x12 
 * @note The numbers represent cabinet model identifiers, not speaker quantities. @n
 * Числа обозначают идентификаторы моделей кабинетов, а не количество динамиков.
 */
#define SPEAKER "1:BS 4x12; 2:AC 2x12; 3:AC 1x12; 4:4x10; 5:BC 2x12; 6:AM 4x12; 7:YC 4x12; 8:JC 2x12"

/**
 * @brief Guitar amplifier gain range description. @n
 * Описание диапазона усиления гитарного усилителя. 
 * @details Valid values: 0 to 10. @n
 * Допустимые значения: от 0 до 10. 
 * @note This is an amplifier control scale, not a gain value expressed in decibels. @n
 * Это шкала регулировки усилителя, а не значение усиления в децибелах.
 */
#define AMP_GAIN "0-10"

/**
 * @brief REV-X initial delay parameter values. @n
 * Значения начальной задержки REV-X. 
 * - 0:   0.1 ms. @n
 * - 2:   3.2 ms. @n
 * - 127: 200 ms. 
 * @note The parameter uses encoded values rather than directly storing milliseconds. @n
 * Параметр использует кодированные значения, а не непосредственно миллисекунды.
 */
#define REV_DELAY "0:0.1ms; 2:3.2ms; 127:200ms"

/**
 * @brief REV-X high-frequency decay ratio values. @n
 * Значения коэффициента затухания высоких частот REV-X. 
 * - 1:  0.1. @n
 * - 8:  0.8. @n
 * - 10: 1.0. @n
 * Controls the relative decay time of high frequencies in the reverberation tail. @n
 * Управляет относительным временем затухания высоких частот в хвосте реверберации.
 */
#define REV_HI "1:0.1; 8:0.8; 10:1.0"

/**
 * @brief REV-X low-frequency decay ratio values. @n
 * Значения коэффициента затухания низких частот REV-X.
 * - 1:  0.1. @n
 * - 12: 1.2. @n
 * - 14: 1.4. @n
 * Controls the relative decay time of low frequencies in the reverberation tail. @n
 * Управляет относительным временем затухания низких частот в хвосте реверберации.
 */
#define REV_LO "1:0.1; 12:1.2; 14:1.4"

/**
 * @brief REV-X low-frequency crossover parameter values. @n
 * Значения частоты разделения низкочастотного диапазона REV-X.
 * - 1: 22 Hz. @n
 * - 32: 800 Hz. @n
 * - 59: 18000 Hz. @n
 * Defines the frequency boundary used by the low-frequency decay ratio control. @n
 * Определяет частотную границу, используемую регулятором затухания низких частот.
 * @note The parameter uses encoded frequency values. @n
 * Параметр использует кодированные значения частоты.
 */
#define REV_LF "1:22Hz; 32:800Hz; 59:18kHz"

/**
 * @brief Immutable catalog of all known parameter definitions. @n
 * Неизменяемый массив всех описанных параметров.
 * @details Entries are ordered by unit and match original parameter names.
 * The sentinel URC_NO_DEFAULT means the upstream default is absent. Some numeric IDs
 * are shared between algorithms; use @c unit and @c name together. @n
 * Параметры упорядочены по блокам. URC_NO_DEFAULT означает отсутствие известного
 * значения по умолчанию. ID могут повторяться; используйте пару @c unit / @c name.
 */
static const urc_parameter parameters[] =
{
    P("mixer",    "InputMix1Solo",       7,   0,     1,     0, S, ""),
    P("mixer",    "InputMix2Solo",       8,   0,     1,     0, S, ""),
    P("mixer",    "InputMix3Solo",       209, 0,     1,     0, S, ""),
    P("mixer",    "InputMix1Mute",       9,   0,     1,     1, M, ""),
    P("mixer",    "InputMix2Mute",       10,  0,     1,     1, M, ""),
    P("mixer",    "InputMix3Mute",       211, 0,     1,     1, M, ""),
    P("mixer",    "InputMix1Volume",     12,  0,     127,   103, V, ""),
    P("mixer",    "InputMix2Volume",     13,  0,     127,   103, V, ""),
    P("mixer",    "InputMix3Volume",     213, 0,     127,   103, V, ""),
    P("mixer",    "InputMix1Pan",        15,  -16,   16,    0, PAN, ""),
    P("mixer",    "InputMix2Pan",        16,  -16,   16,    0, PAN, ""),
    P("mixer",    "InputMix3Pan",        215, -16,   16,    0, PAN, ""),
    P("mixer",    "InputStereo",         0,   0,     1,     0, "0:off; 1:input1+input2-stereo", "ch0,ch2,ch4 only"),
    P("mixer",    "InputHPF",            3,   0,     1,     0, B, "ch0-ch3 only"),
    P("mixer",    "InputInvertPhase",    2,   0,     1,     0, "0:normal; 1:inverted", ""),
    P("mixer",    "InputFXRec",          177, 0,     1,     0, B, ""),
    P("mixer",    "InputFX1Enabled",     178, 0,     1,     URC_NO_DEFAULT, B, "Inactive when No Effect"),
    P("mixer",    "InputFX1Type",        182, 0,     6,     0, "0:No Effect; 1:Ch.Strip; 2:Clean; 3:Crunch; 4:Lead; 5:Drive; 6:Pitch Fix", ""),
    P("mixer",    "InputFX2Enabled",     179, 0,     1,     URC_NO_DEFAULT, B, "Inactive when No Effect"),
    P("mixer",    "InputFX2Type",        183, 0,     6,     0, "0:No Effect; 1:Ch.Strip; 2:Clean; 3:Crunch; 4:Lead; 5:Drive; 6:Pitch Fix", ""),
    P("mixer",    "InputFX3Enabled",     262, 0,     1,     URC_NO_DEFAULT, B, "Inactive when No Effect"),
    P("mixer",    "InputFX3Type",        264, 0,     2,     0, "0:No Effect; 1:Gate; 2:Comp", ""),
    P("mixer",    "InputReverbSend",     14,  0,     127,   0, V, ""),
    P("mixer",    "DAWMix1Solo",         17,  0,     1,     0, S, ""),
    P("mixer",    "DAWMix2Solo",         18,  0,     1,     0, S, ""),
    P("mixer",    "DAWMix3Solo",         219, 0,     1,     0, S, ""),
    P("mixer",    "DAWMix1Mute",         19,  0,     1,     1, M, ""),
    P("mixer",    "DAWMix2Mute",         20,  0,     1,     1, M, ""),
    P("mixer",    "DAWMix3Mute",         221, 0,     1,     1, M, ""),
    P("mixer",    "DAWMix1Volume",       21,  0,     127,   103, V, ""),
    P("mixer",    "DAWMix2Volume",       22,  0,     127,   103, V, ""),
    P("mixer",    "DAWMix3Volume",       223, 0,     127,   103, V, ""),
    P("mixer",    "DAWMix1Pan",          23,  -16,   16,    0, PAN, ""),
    P("mixer",    "DAWMix2Pan",          24,  -16,   16,    0, PAN, ""),
    P("mixer",    "DAWMix3Pan",          225, -16,   16,    0, PAN, ""),
    P("mixer",    "MusicMix1Solo",       242, 0,     1,     0, S, "Music uses channel 1; Voice channel 2 per upstream comment"),
    P("mixer",    "MusicMix2Solo",       243, 0,     1,     0, S, ""),
    P("mixer",    "MusicMix3Solo",       245, 0,     1,     0, S, ""),
    P("mixer",    "MusicMix1Mute",       246, 0,     1,     1, M, ""),
    P("mixer",    "MusicMix2Mute",       247, 0,     1,     1, M, ""),
    P("mixer",    "MusicMix3Mute",       249, 0,     1,     1, M, ""),
    P("mixer",    "MusicMix1Volume",     250, 0,     127,   103, V, ""),
    P("mixer",    "MusicMix2Volume",     251, 0,     127,   103, V, ""),
    P("mixer",    "MusicMix3Volume",     253, 0,     127,   103, V, ""),
    P("mixer",    "MusicMix1Pan",        254, -16,   16,    0, PAN, ""),
    P("mixer",    "MusicMix2Pan",        255, -16,   16,    0, PAN, ""),
    P("mixer",    "MusicMix3Pan",        257, -16,   16,    0, PAN, ""),
    P("mixer",    "ReverbMix1Mute",      77,  0,     1,     1, M, ""),
    P("mixer",    "ReverbMix2Mute",      78,  0,     1,     1, M, ""),
    P("mixer",    "ReverbMix3Mute",      235, 0,     1,     1, M, ""),
    P("mixer",    "ReverbMix1Volume",    79,  0,     127,   103, V, ""),
    P("mixer",    "ReverbMix2Volume",    80,  0,     127,   103, V, ""),
    P("mixer",    "ReverbMix3Volume",    237, 0,     127,   103, V, ""),
    P("mixer",    "MainMix1Mute",        28,  0,     1,     1, M, ""),
    P("mixer",    "MainMix2Mute",        29,  0,     1,     1, M, ""),
    P("mixer",    "MainMix3Mute",        229, 0,     1,     1, M, ""),
    P("mixer",    "MainMix1Volume",      30,  0,     127,   103, V, ""),
    P("mixer",    "MainMix2Volume",      31,  0,     127,   103, V, ""),
    P("mixer",    "MainMix3Volume",      231, 0,     127,   103, V, ""),
    P("mixer",    "MainMix1Pan",         32,  -16,   16,    0, PAN, ""),
    P("mixer",    "MainMix2Pan",         33,  -16,   16,    0, PAN, ""),
    P("mixer",    "MainMix3Pan",         233, -16,   16,    0, PAN, ""),
    P("mixer",    "ReverbType",          65,  0,     3,     0, "0:Hall; 1:Room; 2:Plate; 3:Delay", ""),
    P("mixer",    "ReverbTime",          66,  0,     69,    23, "0:0.289; 23:2.51; 40:4.15; 69:29.0", "Type-dependent"),
    P("mixer",    "ReverbOutput",        37,  0,     4,     0, "0:mix1; 1:mix2; 4:mix3", ""),
    P("mixer",    "Headphones2Input",    34,  0,     4,     0, "0:mix1; 1:mix2; 4:mix3", "Doesn't reset on Initial Data"),
    P("mixer",    "MainMix3FXEnabled",   268, 0,     1,     URC_NO_DEFAULT, B, "Inactive when No Effect"),
    P("mixer",    "MainMix3FXType",      270, 0,     1,     0, "0:No Effect; 1:M.B.Comp", ""),
    P("mixer",    "HPFSetting",          4,   0,     4,     2, "0:40Hz; 1:60Hz; 2:80Hz; 3:100Hz; 4:120Hz", ""),
    P("mixer",    "LineInputLevel",      41,  0,     1,     1, "0:+4dB; 1:-10dB", "set on ch3"),
    P("mixer",    "PCFX3Enabled",        265, 0,     1,     URC_NO_DEFAULT, B, "Inactive when No Effect"),
    P("mixer",    "PCFX3Type",           267, 0,     1,     0, "0:No Effect; 1:Ducker", "ch1:DAW; ch2:Music; ch3:Voice"),

    P("chstrip",  "Enabled",             178, 0,     1,     1, B, ""),
    P("chstrip",  "CompEnabled",         43,  0,     1,     0, "0:on; 1:off", ""),
    P("chstrip",  "CompDrive",           44,  0,     200,   100, "0:0; 100:5.00; 200:10.00", ""),
    P("chstrip",  "CompAttack",          45,  57,    283,   184, "57:0.092ms; 184:4.122ms; 283:80ms", ""),
    P("chstrip",  "CompRelease",         46,  24,    300,   159, "24:9.3ms; 159:92ms; 300:999ms", ""),
    P("chstrip",  "CompRatio",           47,  0,     120,   30, "0:1.00; 30:2.50; 60:4.0; 90:14.0; 120:inf", ""),
    P("chstrip",  "CompKnee",            48,  0,     2,     1, "0:soft; 1:medium; 2:hard", ""),
    P("chstrip",  "CompSideChEnabled",   199, 0,     1,     1, B, ""),
    P("chstrip",  "CompSideChFreq",      51,  4,     124,   30, "4:20Hz; 30:90Hz; 124:20kHz", ""),
    P("chstrip",  "CompSideChGain",      52,  0,     360,   133, "0:-18dB; 133:-4.7dB; 360:+18dB", ""),
    P("chstrip",  "CompSideChQ",         53,  0,     60,    12, "0:0.50; 12:1; 60:16", ""),
    P("chstrip",  "Morphing",            54,  0,     200,   100, "0:0; 100:5; 200:10", ""),
    P("chstrip",  "EQEnabled",           55,  0,     1,     0, "0:on; 1:off", ""),
    P("chstrip",  "EQHighEnabled",       196, 0,     1,     1, B, ""),
    P("chstrip",  "EQHighFreq",          56,  60,    124,   112, "60:500Hz; 112:10kHz; 124:20kHz", ""),
    P("chstrip",  "EQHighGain",          57,  0,     360,   180, G, ""),
    P("chstrip",  "EQMidEnabled",        197, 0,     1,     1, B, ""),
    P("chstrip",  "EQMidFreq",           58,  4,     124,   71, "4:20Hz; 71:1kHz; 124:20kHz", ""),
    P("chstrip",  "EQMidGain",           59,  0,     360,   180, G, ""),
    P("chstrip",  "EQMidQ",              60,  0,     60,    12, "0:0.5; 12:1; 60:16", ""),
    P("chstrip",  "EQLowEnabled",        198, 0,     1,     1, B, ""),
    P("chstrip",  "EQLowFreq",           61,  4,     72,    32, "4:20Hz; 32:100Hz; 72:1kHz", ""),
    P("chstrip",  "EQLowGain",           62,  0,     360,   180, G, ""),
    P("chstrip",  "OutputLevel",         63,  0,     360,   180, G, ""),

    P("clean",    "Volume",              104, 0,     100,   19, AMP_GAIN, ""),
    P("clean",    "Distortion",          105, 0,     100,   0, "0:off; 100:10", ""),
    P("clean",    "Blend",               103, 0,     100,   50, AMP_GAIN, ""),
    P("clean",    "Modulation",          116, 0,     2,     1, "0:Chorus; 1:off; 2:Vibrato", ""),
    P("clean",    "Bass",                106, 0,     100,   61, AMP_GAIN, ""),
    P("clean",    "Middle",              107, 0,     100,   50, AMP_GAIN, ""),
    P("clean",    "Treble",              108, 0,     100,   40, AMP_GAIN, ""),
    P("clean",    "Presence",            109, 0,     100,   30, AMP_GAIN, ""),
    P("clean",    "VibrationSpeed",      117, 0,     100,   50, AMP_GAIN, ""),
    P("clean",    "VibrationDepth",      118, 0,     100,   50, AMP_GAIN, ""),
    P("clean",    "GateEnabled",         200, 0,     1,     0, B, ""),
    P("clean",    "GateLevel",           201, 0,     100,   20, AMP_GAIN, ""),
    P("clean",    "SpeakerType",         113, 1,     8,     8, SPEAKER, ""),
    P("clean",    "MicPosition",         115, 0,     1,     0, "0:Center; 1:Edge", ""),
    P("clean",    "Output",              111, 0,     127,   64, "-11.9dB (upstream)", ""),

    P("crunch",   "AmpType",             121, 0,     1,     1, "0:Normal; 1:Bright", ""),
    P("crunch",   "Gain",                122, 0,     100,   46, AMP_GAIN, ""),
    P("crunch",   "Bass",                124, 0,     100,   47, AMP_GAIN, ""),
    P("crunch",   "Middle",              125, 0,     100,   70, AMP_GAIN, ""),
    P("crunch",   "Trebble",             126, 0,     100,   53, AMP_GAIN, "Upstream spelling: Trebble"),
    P("crunch",   "Presence",            127, 0,     100,   28, AMP_GAIN, ""),
    P("crunch",   "GateEnabled",         200, 0,     1,     0, B, ""),
    P("crunch",   "GateLevel",           201, 0,     100,   20, AMP_GAIN, ""),
    P("crunch",   "SpeakerType",         113, 1,     8,     4, SPEAKER, ""),
    P("crunch",   "MicPosition",         115, 0,     1,     0, "0:Center; 1:Edge", ""),
    P("crunch",   "Output",              111, 0,     127,   64, "-11.9dB (upstream)", ""),

    P("lead",     "AmpType",             139, 0,     1,     0, "0:High; 1:Low", ""),
    P("lead",     "Gain",                140, 0,     100,   100, AMP_GAIN, ""),
    P("lead",     "Master",              146, 0,     100,   49, AMP_GAIN, ""),
    P("lead",     "Bass",                142, 0,     100,   66, AMP_GAIN, ""),
    P("lead",     "Middle",              143, 0,     100,   80, AMP_GAIN, ""),
    P("lead",     "Treble",              144, 0,     100,   30, AMP_GAIN, ""),
    P("lead",     "Presence",            145, 0,     100,   29, AMP_GAIN, ""),
    P("lead",     "GateEnabled",         200, 0,     1,     0, B, ""),
    P("lead",     "GateLevel",           201, 0,     100,   20, AMP_GAIN, ""),
    P("lead",     "SpeakerType",         113, 1,     8,     4, SPEAKER, ""),
    P("lead",     "MicPosition",         115, 0,     1,     0, "0:Center; 1:Edge", ""),
    P("lead",     "Output",              111, 0,     127,   42, "-11.9dB (upstream)", ""),

    P("drive",    "AmpType",             157, 0,     5,     4, "0:Raw1; 1:Raw2; 2:Vintage1; 3:Vintage2; 4:Modern1; 5:Modern2", ""),
    P("drive",    "Gain",                158, 0,     100,   75, AMP_GAIN, ""),
    P("drive",    "Master",              164, 0,     100,   40, AMP_GAIN, ""),
    P("drive",    "Bass",                160, 0,     100,   40, AMP_GAIN, ""),
    P("drive",    "Middle",              161, 0,     100,   50, AMP_GAIN, ""),
    P("drive",    "Treble",              162, 0,     100,   80, AMP_GAIN, ""),
    P("drive",    "Presence",            163, 0,     100,   90, AMP_GAIN, ""),
    P("drive",    "GateEnabled",         200, 0,     1,     0, B, ""),
    P("drive",    "GateLevel",           201, 0,     100,   20, AMP_GAIN, ""),
    P("drive",    "SpeakerType",         113, 1,     8,     6, SPEAKER, ""),
    P("drive",    "MicPosition",         115, 0,     1,     0, "0:Center; 1:Edge", ""),
    P("drive",    "Output",              111, 0,     127,   42, "-11.9dB (upstream)", ""),

    P("pitchfix", "Pitch",               272, -1200, 1200,  0, "", ""),
    P("pitchfix", "Formant",             273, 2,     126,   64, "2:-52; 64:0; 126:62", ""),
    P("pitchfix", "Mix",                 277, 0,     126,   126, "", ""),
    P("pitchfix", "Key",                 282, 0,     11,    0, "0:C; 11:B", ""),
    P("pitchfix", "Scale",               283, 0,     7,     0, "0:Custom; 1:Single; 2:Major; 3:Natural Minor; 4:Harmonic Minor; 5:Melodic Minor; 6:Pentatonic; 7:Chromatic", ""),
    P("pitchfix", "NoteC",               285, 0,     1,     1, "", ""),
    P("pitchfix", "NoteCsharp",          286, 0,     1,     1, "", ""),
    P("pitchfix", "NoteD",               287, 0,     1,     1, "", ""),
    P("pitchfix", "NoteDsharp",          288, 0,     1,     1, "", ""),
    P("pitchfix", "NoteE",               289, 0,     1,     1, "", ""),
    P("pitchfix", "NoteF",               290, 0,     1,     1, "", ""),
    P("pitchfix", "NoteFsharp",          291, 0,     1,     1, "", ""),
    P("pitchfix", "NoteG",               292, 0,     1,     1, "", ""),
    P("pitchfix", "NoteGsharp",          293, 0,     1,     1, "", ""),
    P("pitchfix", "NoteA",               294, 0,     1,     1, "", ""),
    P("pitchfix", "NoteAsharp",          295, 0,     1,     1, "", ""),
    P("pitchfix", "NoteB",               296, 0,     1,     1, "", ""),
    P("pitchfix", "NoteLowLimit",        280, 0,     127,   0, "0:C2; 127:G8", ""),
    P("pitchfix", "NoteHighLimit",       281, 0,     127,   127, "0:C2; 127:G8", ""),
    P("pitchfix", "MidiControl",         278, 0,     1,     0, "", ""),
    P("pitchfix", "MidiRealTimeControl", 279, 0,     1,     0, "", ""),

    P("hall",     "ReverbTime",          66,  0,     69,    23, "0:0.103ms; 23:2.51s; 69:31s", ""),
    P("hall",     "InitialDelay",        68,  0,     127,   2, REV_DELAY, ""),
    P("hall",     "Decay",               74,  0,     63,    27, "", ""),
    P("hall",     "RoomSize",            71,  0,     31,    29, "", ""),
    P("hall",     "Diffusion",           67,  0,     10,    10, "", ""),
    P("hall",     "HPF",                 69,  0,     52,    4, "0:20Hz; 4:32Hz; 52:8kHz", ""),
    P("hall",     "LPF",                 70,  34,    60,    50, "34:1kHz; 50:6.3kHz; 60:20kHz", ""),
    P("hall",     "HiRatio",             72,  1,     10,    8, REV_HI, ""),
    P("hall",     "LowRatio",            73,  1,     14,    12, REV_LO, ""),
    P("hall",     "LowFreq",             76,  1,     59,    32, REV_LF, ""),

    P("room",     "ReverbTime",          66,  0,     69,    6, "0:0.206s; 6:0.780s; 69:26s", ""),
    P("room",     "InitialDelay",        68,  0,     127,   2, REV_DELAY, ""),
    P("room",     "Decay",               74,  0,     63,    15, "", ""),
    P("room",     "RoomSize",            71,  0,     31,    15, "", ""),
    P("room",     "Diffusion",           67,  0,     10,    8, "", ""),
    P("room",     "HPF",                 69,  0,     52,    6, "0:20Hz; 6:40Hz; 52:8kHz", ""),
    P("room",     "LPF",                 70,  34,    60,    47, "34:1kHz; 47:4.5kHz; 60:20kHz", ""),
    P("room",     "HiRatio",             72,  1,     10,    8, REV_HI, ""),
    P("room",     "LowRatio",            73,  1,     14,    12, REV_LO, ""),
    P("room",     "LowFreq",             76,  1,     59,    32, REV_LF, ""),

    P("plate",    "ReverbTime",          66,  0,     69,    21, "0:0.333ms; 21:2.66s; 69:33.3s", ""),
    P("plate",    "InitialDelay",        68,  0,     127,   2, REV_DELAY, ""),
    P("plate",    "Decay",               74,  0,     63,    5, "", ""),
    P("plate",    "RoomSize",            71,  0,     31,    18, "", ""),
    P("plate",    "Diffusion",           67,  0,     10,    8, "", ""),
    P("plate",    "HPF",                 69,  0,     52,    12, "0:20Hz; 12:80Hz; 52:8kHz", ""),
    P("plate",    "LPF",                 70,  34,    60,    52, "34:1kHz; 52:8kHz; 60:20kHz", ""),
    P("plate",    "HiRatio",             72,  1,     10,    9, "1:0.1; 9:0.9; 10:1.0", ""),
    P("plate",    "LowRatio",            73,  1,     14,    10, "1:0.1; 10:1.0; 14:1.4", ""),
    P("plate",    "LowFreq",             76,  1,     59,    32, REV_LF, ""),

    P("delay",    "Stereo",              258, 0,     1,     0, "0:mono; 1:stereo", ""),
    P("delay",    "DelayTime",           259, 1,     13000, 2400, "1:0.1ms; 2400:240ms; 13000:1300ms", ""),
    P("delay",    "Feedback",            261, 64,    127,   80, "64:0; 80:15; 127:63", ""),
    P("delay",    "HighRatio",           260, 1,     10,    8, "1:0.1; 8:0.8; 10:1.0", ""),

    P("ducker",   "Input1Source",        316, 0,     1,     1, B, ""),
    P("ducker",   "Input2Source",        317, 0,     1,     1, B, ""),
    P("ducker",   "VoiceSource",         318, 0,     1,     0, B, ""),
    P("ducker",   "Threshold",           319, 13,    73,    33, "13:-60dB; 33:-40dB; 73:0dB", ""),
    P("ducker",   "Range",               320, 3,     73,    50, "3:-70dB; 50:-24dB; 72:0dB", ""),
    P("ducker",   "Attack",              321, 57,    283,   237, "57:0.092ms; 237:20.17ms; 283:80ms", ""),
    P("ducker",   "Decay",               323, 0,     121,   99, "0:1.3ms; 99:1s; 121:5s", ""),

    P("mbcomp",   "LowGain",             330, 0,     55,    39, "0:-inf; 1:-60dB; 39:2dB; 55:18dB", ""),
    P("mbcomp",   "MidGain",             335, 0,     55,    39, "0:-inf; 1:-60dB; 39:2dB; 55:18dB", ""),
    P("mbcomp",   "HighGain",            340, 0,     55,    39, "0:-inf; 1:-60dB; 39:2dB; 55:18dB", ""),
    P("mbcomp",   "LMxover",             342, 5,     96,    36, "5:21.2Hz; 36:125Hz; 96:4kHz", ""),
    P("mbcomp",   "MHxover",             343, 17,    108,   93, "17:42.5Hz; 93:3.35kHz; 108:8kHz" ,"")
};

size_t urc_parameter_count(void)
{
    return sizeof(parameters) / sizeof(parameters[0]);
}

const urc_parameter *urc_parameter_at(size_t index)
{
    return index < urc_parameter_count() ? &parameters[index] : NULL;
}

const urc_parameter *urc_find_parameter(const char *unit, const char *name)
{
    if (unit == NULL || name == NULL)
        return NULL;

    for (size_t i = 0; i < urc_parameter_count(); ++i)
        if (strcasecmp(parameters[i].unit, unit) == 0 
        &&  strcasecmp(parameters[i].name, name) == 0)
            return &parameters[i];

    return NULL;
}
