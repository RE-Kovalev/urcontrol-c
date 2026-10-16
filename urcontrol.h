/**
 * @file urcontrol.h
 * @brief Public interface for the Steinberg UR-C DSP mixer control library. @n
 * Публичный интерфейс для управления DSP-микшером Steinberg UR-C.
 * @details The library sends Yamaha SysEx commands to the internal mixer
 * MIDI port through ALSA RawMIDI. Protocol definitions were adapted from
 * askoriy/urcontrol (MIT). Parameters were transcribed from the UR44C map,
 * support for a specific UR-C model or firmware is not implied. @n
 * Библиотека отправляет команды Yamaha SysEx во внутренний MIDI-порт
 * микшера через ALSA RawMIDI. Протокол адаптирован от askoriy/urcontrol (MIT).
 * Таблица параметров перенесена из UR44C, совместимость с конкретной моделью
 * UR-C и прошивкой не гарантируется.
 */
#ifndef URCONTROL_H
#define URCONTROL_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * @defgroup urc_constants Protocol constants
 * Message lengths and maximum protocol buffer size. @n
 * Размеры сообщений и максимальный размер буфера протокола.
 * @{
 */

#define URC_LENGTH_CHANGE 19       ///< Length of a Change SysEx message (bytes) @n
                                   ///< Длина SysEx сообщения - Change в байтах

#define URC_LENGTH_QUERY 15        ///< Length of a Query SysEx message (bytes) @n
                                   ///< Длина сообщения SysEx - Query в байтах 

#define URC_LENGTH_REPLY 20        ///< Length of a Reply SysEx message (bytes) @n
                                   ///< Длина сообщения SysEx - Reply в байтах

#define URC_LENGTH_KEEPALIVE 9     ///< Length of a Keepalive SysEx message (bytes) @n
                                   ///< Длина сообщения SysEx - Keepalive в байтах   

#define URC_LENGTH_MAX_SYSEX 16384 ///< Maximum accepted SysEx size (bytes), including F0 and F7 @n
                                   ///< Максимальный размер SysEx в байтах с F0 и F7 включительно

/** @} */

/** 
 * @defgroup urc_types Message types and result codes
 * Result and error codes, message types, parameter metadata and device handle. @n
 * Коды выполнения, типы сообщений, метаданные параметров и дескритор устройства.
 * @{
 */

/** 
 * Result and error codes returned by most library operations. @n
 * Коды результата большинства операций библиотеки.
 */
typedef enum
{
    URC_OK = 0,                     ///< Success @n
                                    ///< Успех

    URC_ERR_ARGUMENT = -1,          ///< Invalid or missing argument @n
                                    ///< Некорректный аргумент

    URC_ERR_RANGE = -2,             ///< Value, ID or channel is out of range @n
                                    ///< Значение, ID или канал вне диапазона

    URC_ERR_ALSA = -3,              ///< MIDI transport / ALSA error @n
                                    ///< Ошибка уровня MIDI/ALSA

    URC_ERR_TIMEOUT = -4,           ///< No matching response before timeout @n
                                    ///< Истекло время ожидания ответа

    URC_ERR_MISMATCH = -5,          ///< Readback differs from requested value @n
                                    ///< Прочитанное значение не совпало с записанным

    URC_ERR_CLOSED = -6,            ///< Receive thread stopped / connection closed @n
                                    ///< Соединение закрыто или поток приема завершил работу

    URC_ERR_UNKNOWN_PARAMETER = -7, ///< Unknown unit or parameter name @n
                                    ///< Неизвестный блок или параметр

    URC_ERR_THREAD = -8,            ///< Thread or synchronization init failed @n
                                    ///< Ошибка инициализации потоков или средств синхронизации

    URC_ERR_INACTIVE = -9,          ///< Selected DSP unit is not active or cannot be checked @n
                                    ///< Выбранный DSP-блок не активен или его нельзя проверить

    URC_ERR_MEMORY = -10            ///< Memory allocation failed @n
                                    ///< Ошибка выделения памяти
} urc_result;

/**
 * Recognized SysEx message types, an unknown packet is not a decoder error @n
 * Распознаваемые типы SysEx, неизвестный пакет не считается ошибкой декодирования
 */
typedef enum
{
    URC_MSG_UNKNOWN,   ///< Unsupported or malformed packet shape @n
                       ///< Неизвесный или неподдерживаемый пакет

    URC_MSG_CHANGE,    ///< Parameter change request @n
                       ///< Команда изменения параметра

    URC_MSG_QUERY,     ///< Parameter read request @n
                       ///< Команда на запрос значения параметра

    URC_MSG_REPLY,     ///< Parameter value response @n
                       ///< Ответ со значением параметра

    URC_MSG_KEEPALIVE, ///< Keepalive request @n
                       ///< Сообщение для поддержания активности соединения

    URC_MSG_METER      ///< Meter data, payload not decoded @n
                       ///< Данные индикаторов уровня без разбора содержимого

} urc_message_type;

/**
 * Parsed metadata for a single SysEx packet. @n
 * Декодированные метаданные одного пакета SysEx.
 * @note Fields except @ref urc_message::type are meaningful only for
 * CHANGE/QUERY/REPLY, QUERY carries no value. @n
 * Кроме поля @ref urc_message::type, поля определены только для 
 * CHANGE/QUERY/REPLY, QUERY не содержит значения
 */
typedef struct 
{
    urc_message_type type; ///< Message type @n
                           ///< Тип сообщения

    uint16_t parameter;    ///< Protocol parameter ID (0..16383) @n
                           ///< ID параметра протокола (0..16383)

    uint8_t channel;       ///< Channel number (Zero-based - 0..5) @n
                           ///< Номер канала (с нуля 0..5)

    int32_t value;         ///< Signed value for CHANGE/REPLY @n
                           ///< Величина со знаком для CHANGE/REPLY
} urc_message;

#define URC_NO_DEFAULT INT32_MAX ///< Default value is unavailable
                                 ///< Значение по умолчанию не доступно

/** 
 * Read-only metadate of one named DSP parameter @n
 * Метаданные одного именованного DSP-параметра (только для чтения)
 * @note Parameter IDs can overlap between units and effects.
 * Numeric ranges are encoded protocol values not necessarily physical units (Hz, dB, etc.). @n
 * ID параметров могут совпадать для разных блоков.
 * Диапазоны - коды протокола, не обязательно физические величины (Гц, дБ, и т.д.)
 */
typedef struct
{
    const char* unit;        ///< Unit name (e.g. "mixer") @n
                             ///< Имя блока, например "mixer"

    const char* name;        ///< Parameter name @n
                             ///< Имя параметра

    uint16_t id;             ///< Parameter ID in SysEx @n
                             ///< ID параметра в SysEx

    int32_t minimum;         ///< Minimum encoded value @n
                             ///< Минимальный код значения

    int32_t maximum;         ///< Maximum encoded value @n
                             ///< Максимальный код значения

    int32_t default_value;   ///< Default value, if hasn't = URC_NO_DEFAULT @n
                             ///< Значение по умолчанию, если не задано = URC_NO_DEFAULT

    const char* description; ///< Original value labels / examples @n
                             ///< Метки и примеры значений из источника

    const char* notes;       ///< Original limitations and remarks @n
                             ///< Ограничения и замечания из источника
} urc_parameter;

/** 
 * Opaque handle owning ALSA input/output, receiver thread and locks @n
 * Прозрачный дескриптор ALSA-входа/выхода, потока приема и мьютексов
 */
typedef struct urc_device urc_device;

/**
 * @brief Incoming SysEx notification callback type @n
 * Тип обработчика входящих SysEx-сообщений
 * @param parsed - parsed packet metadata. May have type URC_MSG_UNKNOWN or URC_MSG_METER @n
 * результат декодирования, включая URC_MSG_UNKNOWN и URC_MSG_METER
 * @param raw - complete SysEx byte buffer, including F0/F7 @n
 * полный пакет SysEx, включая F0/F7
 * @param length - length of @p raw in bytes @n
 * длина @p raw в байтах
 * @param user_data - user pointer provided by urc_set_callback() @n
 * указатель, переданный в urc_set_callback()
 * @warning Runs on the receiver thread. @p parsed and @p raw are valid
 * only during the callback. Never call urc_close(), urc_get_parameter(),
 * urc_set_parameter() or their name-based variants here, avoid blocking. @n
 * Вызывается из потока приема. @p parsed и @p raw доступны только пока
 * выполняется callback. Нельзя вызывать urc_close(), urc_get_parameter(),
 * urc_set_parameter() и их аналоги. Избегайте блокировки потока приема.
 */
typedef void (*urc_message_callback)(const urc_message* parsed, const uint8_t* raw, size_t length, void* user_data);

/** @} */

/**
 * @defgroup urc_catalog Parameter catalog
 * Lookup and enumeration of parameter metadata. No hardware access. @n
 * Поиск и просмотр метаданных параметров. Функции не обращаются к оборудованию.
 * @{
 */

/**
 * @brief Return a human-readable English explanation of a status code. @n
 * Возвращает описание кода результата на английском.
 * @param code - any result code, including unknown values. @n
 * код результата, в том числе неизвестный
 * @result Pointer to a const string. @n
 * Указатель на константную си-строку.
 */
const char* urc_strerror(int code);

/** 
 * @brief Return the number of entries in the built-in parameter table. @n
 * Возвращает количество записей встроенного каталога параметров.
 * @return Number of parameter metadata entries. @n
 * Число параметров с метаданными.
 */
size_t urc_parameter_count(void);

/** 
 * @brief Retrieve parameter metadata by its zero-based table index. @n
 * Получить метаданные по индексу в таблице, начиная с нуля.
 * @param index - entry index (0..urc_parameter_count()-1). @n
 * индекс параметра (0..urc_parameter_count()-1).
 * @return Pointer to const metadata or NULL if out of bounds. @n
 * Указатель на константные метаданные или NULL (при выходе за границы диапазона)
 * @warning Do not free returned pointer. @n
 * Нельзя освобождать возвращенный указатель.
 */
const urc_parameter* urc_parameter_at(size_t index);

/** 
 * @brief Find metadata by exact unit and parameter names. @n
 * Найти параметр по имени блока и параметра
 * @param unit - unit identifier ("mixer", "chstrip", etc.) @n
 * идентификатор блока ("mixer", "chstrip" и пр.)
 * @param name - parameter identifier @n
 * идентификатор параметра
 * @return Pointer to const metadata or NULL if not found or given NULL args. @n
 * Указатель на константные метаданные или NULL (при отсутствие или ошибке аргументов)
 * @warning Do not free returned pointer. @n
 * Нельзя освобождать возвращенный указатель.
 */
const urc_parameter* urc_find_parameter(const char* unit, const char* name);

/** @} */



#ifdef __cplusplus
}
#endif // __cplusplus

#endif // URCONTROL_H
