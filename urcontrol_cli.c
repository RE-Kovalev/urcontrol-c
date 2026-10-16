/**
 * @file urcontrol_cli.c
 * @brief Command-line frontend for parameter catalog, SysEx codec and ALSA transport. @n
 * CLI для каталога параметров, SysEx-кодека и ALSA-транспорта.
 * @details Exactly one command is selected per invocation. Catalogue and
 * offline decoding commands do not need a port; MIDI operations require
 * --port. CLI channels are 1-based, unlike the C API. @n
 * За запуск выполняется ровно одна команда. Каталог и офлайн-разбор
 * не требуют MIDI-порта; остальные операции требуют --port.
 * В CLI каналы с единицы, в Си API - с нуля.
 */

#include "urcontrol.h"

#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

/**
 * @brief Print command-line help and usage examples. @n
 * Вывести справку CLI и примеры использования.
 * @param prog - program executable name. @n
 * имя исполняемого файла.
 */
void usage(const char *prog)
{
    fprintf(stderr,
        "Usage: %s [options] COMMAND\n"
        "\n"
        "Commands (one at a time):\n"
        "  --list-units               List supported parameter units\n"
        "  --list-parameters          List parameters in --unit\n"
        "  --get-parameter NAME       Read a named parameter\n"
        "  --set-parameter NAME VALUE Write named parameter (VALUE|min|max|def)\n"
        "  --get-id ID                Read a numeric parameter ID\n"
        "  --set-id ID VALUE          Write numeric parameter ID\n"
        "  --keepalive                Send Yamaha SysEx keepalive\n"
        "  --send-sysex FILE          Send SysEx bytes from an explicit .syx file\n"
        "  --decode-sysex HEX         Decode a packet offline (no device needed)\n"
        "  --get-midi-ports           Run amidi -l to discover ALSA ports\n"
        "\n"
        "Options:\n"
        "  --port PORT                ALSA rawmidi subdevice, e.g. hw:2,0,1\n"
        "  --unit UNIT, -u UNIT       mixer (default), chstrip, clean, ...\n"
        "  --input 1..6, -i N         Mixer input (one-based; default 1)\n"
        "                               UR22C Clean DSP uses protocol channel 0\n"
        "                               (-i 2 is routed to channel 0 for clean)\n"
        "  --timeout MS               Query/verification timeout (default 3000)\n"
        "  --no-confirm               Don't query after writing (unsafe)\n"
        "  --verbose, -v              Show IDs, ranges, descriptions\n"
        "  --help, -h                 This help message\n"
        "\n"
        "Examples:\n"
        "  %s --list-parameters --unit mixer\n"
        "  %s --port hw:2,0,1 -u mixer -i 1 -g InputMix3Volume\n"
        "  %s --port hw:2,0,1 -u mixer -i 1 -s InputMix3Volume 103\n",
        prog, prog, prog, prog);
}

/**
 * @brief Parse a signed 32-bit integer, honoring C base prefixes. @n
 * Разобрать знаковое 32-битное число с префиксами систем счисления C.
 * @param text - NULL-terminated numeric string. @n
 * текстовое представление числа.
 * @param value - output pointer on success. @n
 * указатель для результата.
 * @return 1 on success, 0 for overflow/trailing characters or invalid input. @n
 * Возвращает 1 при успехе, 0 при переполнении/лишних символах/ошибке ввода.
 */
uint8_t parse_int32(const char *text, int32_t *value)
{
    if (text == NULL 
    ||  text[0] == '\0')
        return 0;

    char *end = NULL;
    errno = 0;
    long long parsed = strtoll(text, &end, 0);

    if (errno != 0 
    ||  end == text 
    ||  *end != '\0' 
    ||  parsed < INT32_MIN 
    ||  parsed > INT32_MAX)
        return 0;

    *value = (int32_t)parsed;

    return 1;
}

/**
 * @brief Display one parameter entry, optionally with metadata. @n
 * Напечатать имя параметра и, опционально, его метаданные.
 * @param param  - catalog entry. @n
 * запись каталога.
 * @param verbose - print ID, range, default and remarks. @n
 * показать ID, диапазон, default и замечания.
 */
void print_parameter(const urc_parameter *param, bool verbose)
{
    if (!verbose)
    {
        printf("%s\n", param->name);
        return;
    }

    printf("%-23s ID=%-4u [%d, %d] default=", param->name, param->id, param->minimum, param->maximum);
    
    if (param->default_value != URC_NO_DEFAULT)
        printf("%d", param->default_value);
    else
        printf("N/A");

    if (param->description[0])
        printf(" | %s", param->description);

    if (param->notes[0])
        printf(" | %s", param->notes);

    printf("\n");
}

/**
 * @brief Decode CLI hexadecimal text into raw bytes. @n
 * Преобразовать шестнадцатеричную строку CLI в байты.
 * @param text - hexadecimal characters, allowing spaces, colon, hyphen and tab. @n
 * шестнадцатеричная строка; допустимы пробелы, двоеточия и дефисы.
 * @param dest - output buffer of URC_MAX_SYSEX bytes. @n
 * буфер ёмкостью URC_MAX_SYSEX байт.
 * @param length - output length in bytes. @n
 * длина результата в байтах.
 * @return URC_OK or URC_ERR_ARGUMENT/URC_ERR_RANGE @n
 * Возвращает URC_OK или URC_ERR_ARGUMENT/URC_ERR_RANGE.
 */
int parse_hex(const char *text, uint8_t *dest, size_t *length)
{
    int half = -1;
    size_t n = 0;
    for (const unsigned char *p = (const unsigned char *)text; *p; p++)
    {
        unsigned int nibble = 0;
        if (*p >= '0' 
        &&  *p <= '9') 
            nibble = *p - '0';
        else 
        if (*p >= 'a' 
        &&  *p <= 'f') 
            nibble = *p - 'a' + 10;
        else 
        if (*p >= 'A' 
        &&  *p <= 'F') 
            nibble = *p - 'A' + 10;
        else 
        if (*p == ' ' 
        ||  *p == ':' 
        ||  *p == '-' 
        ||  *p == '\t') 
            continue;
        else 
            return URC_ERR_ARGUMENT;

        if (half < 0)
            half = (int)nibble;
        else
        {
            if (n >= URC_LENGTH_MAX_SYSEX) 
                return URC_ERR_RANGE;

            dest[n++] = (uint8_t)((half << 4) | nibble);
            half = -1;
        }
    }
    if (half >= 0 
    ||  n < 2)
        return URC_ERR_ARGUMENT;
    
    *length = n;

    return URC_OK;
}

/**
 * @brief Load a binary .syx file, rejecting oversized files. @n
 * Загрузить двоичный .syx, отказав при превышении предела.
 * @param filename - file path. @n
 * путь к файлу.
 * @param dest - output buffer of URC_MAX_SYSEX bytes. @n
 * буфер URC_MAX_SYSEX байт.
 * @param length - output length. @n
 * длина результата.
 * @return URC_OK or URC_ERR_ARGUMENT. @n
 * Возвращает URC_OK или URC_ERR_ARGUMENT.
 */
int load_sysex(const char *filename, uint8_t *dest, size_t *length)
{
    FILE *file = fopen(filename, "rb");
    if (file == NULL)
    {
        perror(filename);
        return URC_ERR_ARGUMENT;
    }

    size_t n = fread(dest, 1, URC_LENGTH_MAX_SYSEX, file);
    int next = fgetc(file);
    bool bad = ferror(file) || next != EOF;
    fclose(file);

    if (bad 
    ||  n < 2)
    {
        fprintf(stderr, "Invalid or oversized SysEx file (limit %d bytes)\n", URC_LENGTH_MAX_SYSEX);
        return URC_ERR_ARGUMENT;
    }

    *length = n;
    
    return URC_OK;
}

/**
 * @brief Map a parsed message type to its English CLI label. @n
 * Получить английское имя типа сообщения для CLI.
 * @param type  - parsed SysEx type. @n
 * тип SysEx.
 * @return Pointer to static string. @n
 * Возвращает указатель на статическую строку.
 */
const char *type_name(urc_message_type type)
{
    switch (type)
    {
        case URC_MSG_CHANGE: return "change";
        case URC_MSG_QUERY: return "query";
        case URC_MSG_REPLY: return "reply";
        case URC_MSG_KEEPALIVE: return "keepalive";
        case URC_MSG_METER: return "meter (raw payload)";
        default: return "unknown";
    }
}

/** 
 * @brief CLI command selector; only one command can be active. @n
 * Тип запрошенной команды CLI; одновременно допускается одна.
 */
typedef enum
{
    CMD_NONE, 
    CMD_UNITS, 
    CMD_PARAMETERS, 
    CMD_GET, 
    CMD_SET, 
    CMD_GET_ID,
    CMD_SET_ID, 
    CMD_KEEPALIVE, 
    CMD_SYSEX, 
    CMD_DECODE, 
    CMD_PORTS
} cli_command;

/**
 * @brief Select a single CLI command and reject multiple commands. @n
 * Выбрать одну команду CLI и запретить повторное назначение.
 * @param selected - selected command state. @n
 * текущая выбранная команда.
 * @param new_command - requested command. @n
 * новая команда.
 * @return 0 when accepted; -1 if a command was already selected. @n
 * Возвращает 0 при успехе, -1 если команда уже выбрана.
 */
int choose(cli_command *selected, cli_command new_command)
{
    if (*selected != CMD_NONE)
    {
        fprintf(stderr, "Specify exactly one command\n");
        return -1;
    }
    *selected = new_command;
    return 0;
}

/**
 * @brief Parse CLI arguments, run one command and exit with status.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on operational errors, 2 on command-line errors.
 * @brief Разобрать аргументы CLI, выполнить команду и завершить работу.
 * @param argc Число аргументов.
 * @param argv Массив строк аргументов.
 * @return 0 при успехе, 1 при сбое операции, 2 при ошибке аргументов.
 */
int main(int argc, char **argv)
{
    const struct option options[] =
    {
        {"list-units", no_argument, NULL, 1001},
        {"list-parameters", no_argument, NULL, 'l'},
        {"get-parameter", required_argument, NULL, 'g'},
        {"set-parameter", required_argument, NULL, 's'},
        {"get-id", required_argument, NULL, 1002},
        {"set-id", required_argument, NULL, 1003},
        {"keepalive", no_argument, NULL, 1004},
        {"send-sysex", required_argument, NULL, 1005},
        {"decode-sysex", required_argument, NULL, 1006},
        {"get-midi-ports", no_argument, NULL, 1007},
        {"port", required_argument, NULL, 'p'},
        {"unit", required_argument, NULL, 'u'},
        {"input", required_argument, NULL, 'i'},
        {"timeout", required_argument, NULL, 1008},
        {"no-confirm", no_argument, NULL, 1009},
        {"verbose", no_argument, NULL, 'v'},
        {"help", no_argument, NULL, 'h'},
        {NULL, 0, NULL, 0}
    };

    const char *port = NULL;
    const char *unit = "mixer";
    const char *name = NULL;
    const char *value_text = NULL;
    uint8_t verbose = 0;
    uint8_t confirm = 1;
    int timeout_ms = 3000;
    uint8_t channel = 0;
    cli_command command = CMD_NONE;
    int opt;

    while ((opt = getopt_long(argc, argv, "hvu:i:p:lg:s:", options, NULL)) != -1)
    {
        int32_t parsed = 0;
        switch (opt)
        {
            case 'h': 
                usage(argv[0]); 
                return 0;
            case 'v': 
                verbose = 1; 
                break;
            case 'u': 
                unit = optarg; 
                break;
            case 'p': 
                port = optarg; 
                break;
            case 'i':
                if (!parse_int32(optarg, &parsed) 
                ||  parsed < 1 
                ||  parsed > 6)
                {
                    fprintf(stderr, "--input must be from 1 to 6\n");
                    return 2;
                }
                channel = (uint8_t)(parsed - 1);
                break;
            case 1008:
                if (!parse_int32(optarg, &parsed) 
                ||  parsed < 1 
                ||  parsed > 600000)
                {
                    fprintf(stderr, "--timeout must be 1..600000 ms\n");
                    return 2;
                }
                timeout_ms = (int)parsed;
                break;
            case 1009: 
                confirm = 0; 
                break;
            case 1001: 
                if (choose(&command, CMD_UNITS)) 
                    return 2; 
                break;
            case 'l': 
                if (choose(&command, CMD_PARAMETERS)) 
                    return 2; 
                break;
            case 'g': 
                if (choose(&command, CMD_GET)) 
                    return 2; 
                name = optarg; 
                break;
            case 's':
                if (choose(&command, CMD_SET)) 
                    return 2;
                name = optarg;
                /* getopt treats '-1200' as an option unless we consume the
                 * signed value here, before getopt parses the next token. */
                if (optind < argc &&
                    (parse_int32(argv[optind], &parsed) ||
                     strcasecmp(argv[optind], "min") == 0 ||
                     strcasecmp(argv[optind], "max") == 0 ||
                     strcasecmp(argv[optind], "def") == 0))
                    value_text = argv[optind++];
                break;
            case 1002: 
                if (choose(&command, CMD_GET_ID)) 
                    return 2; 
                name = optarg; 
                break;
            case 1003:
                if (choose(&command, CMD_SET_ID)) 
                    return 2;
                name = optarg;
                if (optind < argc 
                &&  parse_int32(argv[optind], &parsed))
                    value_text = argv[optind++];
                break;
            case 1004: 
                if (choose(&command, CMD_KEEPALIVE)) 
                    return 2; 
                break;
            case 1005: 
                if (choose(&command, CMD_SYSEX)) 
                    return 2; 
                name = optarg;  
                break;
            case 1006: 
                if (choose(&command, CMD_DECODE)) 
                    return 2; 
                name = optarg; 
                break;
            case 1007: 
                if (choose(&command, CMD_PORTS)) 
                    return 2; 
                break;
            default: 
                usage(argv[0]); 
                return 2;
        }
    }
    if (command == CMD_NONE)
    {
        usage(argv[0]);
        return 2;
    }
    if (command == CMD_SET 
    ||  command == CMD_SET_ID)
    {
        if (value_text == NULL)
        {
            if (optind >= argc)
            {
                fprintf(stderr, "Missing parameter value\n");
                return 2;
            }
            value_text = argv[optind++];
        }
    }
    if (optind != argc)
    {
        fprintf(stderr, "Unexpected argument: %s\n", argv[optind]);
        return 2;
    }
    if (command == CMD_PORTS)
    {
        /* Discovery only. Never picks a port silently: the physical DIN
         * port and mixer/DSP control port may be present simultaneously. */
        execlp("amidi", "amidi", "-l", (char *)NULL);
        perror("amidi (install alsa-utils, or inspect /proc/asound)");
        return 1;
    }
    if (command == CMD_UNITS)
    {
        const char *previous = NULL;
        for (size_t i = 0; i < urc_parameter_count(); ++i)
        {
            const char *next = urc_parameter_at(i)->unit;
            if (previous == NULL 
            ||  strcasecmp(previous, next) != 0)
                puts(next);
            previous = next;
        }
        return 0;
    }
    if (command == CMD_PARAMETERS)
    {
        size_t count = 0;
        for (size_t i = 0; i < urc_parameter_count(); ++i)
        {
            const urc_parameter *param = urc_parameter_at(i);
            if (strcasecmp(param->unit, unit) == 0)
            {
                print_parameter(param, verbose);
                ++count;
            }
        }
        if (!count)
        {
            fprintf(stderr, "Unknown unit: %s\n", unit);
            return 2;
        }
        return 0;
    }
    if (command == CMD_DECODE)
    {
        uint8_t bytes[URC_LENGTH_MAX_SYSEX];
        size_t length;
        if (parse_hex(name, bytes, &length) != URC_OK)
        {
            fprintf(stderr, "Invalid hexadecimal string\n");
            return 2;
        }
        urc_message message;
        int result = urc_decode(bytes, length, &message);
        if (result != URC_OK)
        {
            fprintf(stderr, "Decode error: %s\n", urc_strerror(result));
            return 1;
        }
        printf("type=%s", type_name(message.type));
        if (message.type == URC_MSG_QUERY 
        ||  message.type == URC_MSG_REPLY 
        ||  message.type == URC_MSG_CHANGE)
        {
            printf(" parameter=%u channel=%u", message.parameter, (uint32_t)message.channel + 1);
            if (message.type == URC_MSG_REPLY 
            ||  message.type == URC_MSG_CHANGE)
            {
                printf(" value=%" PRId32, message.value);
            }
        }
        putchar('\n');
        return 0;
    }

    const urc_parameter *param = NULL;
    uint16_t parameter_id = 0;
    int32_t value = 0;
    if (command == CMD_GET 
    ||  command == CMD_SET)
    {
        param = urc_find_parameter(unit, name);
        if (param == NULL)
        {
            fprintf(stderr, "Unknown parameter: %s/%s\n", unit, name);
            return 2;
        }
        parameter_id = param->id;
    }
    else 
    if (command == CMD_GET_ID 
    ||  command == CMD_SET_ID)
    {
        int32_t id;
        if (!parse_int32(name, &id)
        ||  id < 0 
        ||  id > 16383)
        {
            fprintf(stderr, "Parameter ID must be 0..16383\n");
            return 2;
        }
        parameter_id = (uint16_t)id;
    }

    if (command == CMD_SET 
    ||  command == CMD_SET_ID)
        if (param != NULL)
        {
            if (strcasecmp(value_text, "min") == 0)
                value = param->minimum;
            else 
            if (strcasecmp(value_text, "max") == 0)
            {
                value = param->maximum;
            }
            else 
            if (strcasecmp(value_text, "def") == 0 
            &&  param->default_value != URC_NO_DEFAULT)
            {
                value = param->default_value;
            }
            else 
            if (!parse_int32(value_text, &value))
            {
                fprintf(stderr, "Invalid value or missing default: %s\n", value_text);
                return 2;
            }
            if (value < param->minimum 
            ||  value > param->maximum)
            {
                fprintf(stderr, "Out of range: %d (allowed %d..%d)\n", value, param->minimum, param->maximum);
                return 2;
            }
        }
    
    if (port == NULL)
    {
        fprintf(stderr, "MIDI commands require --port hw:CARD,DEVICE,SUBDEVICE\nUse --get-midi-ports to find it.\n");
        return 2;
    }

    urc_device *device = NULL;
    int result = urc_open(&device, port);
    if (result != URC_OK)
    {
        fprintf(stderr, "Open %s: %s\n", port, urc_strerror(result));
        return 1;
    }

    if (command == CMD_KEEPALIVE)
        result = urc_send_keepalive(device);
    else 
    if (command == CMD_SYSEX)
    {
        uint8_t bytes[URC_LENGTH_MAX_SYSEX];
        size_t length = 0;
        result = load_sysex(name, bytes, &length);
        if (result == URC_OK)
            result = urc_send_sysex(device, bytes, length);
    }
    else 
    if (command == CMD_GET 
    ||  command == CMD_GET_ID)
    {
        result = urc_get_parameter(device, parameter_id, channel, timeout_ms, &value);
        if (result == URC_OK)
        {
            if (verbose && param != NULL)
                print_parameter(param, 1);
            printf("%" PRId32 "\n", value);
        }
    }
    else 
    if (command == CMD_SET 
    ||  command == CMD_SET_ID)
    {
        result = urc_set_parameter(device, parameter_id, value, channel, confirm, timeout_ms);
        if (result == URC_OK && verbose)
        {
            printf("Written parameter=%u channel=%u value=%d %s\n", parameter_id, (uint32_t)channel + 1, value, confirm ? " (confirmed)" : " (unconfirmed)");
        }
    }
    urc_close(device);

    if (result != URC_OK)
    {
        fprintf(stderr, "UR control error: %s (%d)\n", urc_strerror(result), result);
        return 1;
    }

    return 0;
}
