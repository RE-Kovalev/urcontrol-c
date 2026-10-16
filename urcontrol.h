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
 * @return Pointer to a const string. @n
 * Возвращает указатель на константную си-строку.
 */
const char* urc_strerror(int code);

/** 
 * @brief Return the number of entries in the built-in parameter table. @n
 * Возвращает количество записей встроенного каталога параметров.
 * @return Number of parameter metadata entries. @n
 * Возвращает число параметров с метаданными.
 */
size_t urc_parameter_count(void);

/** 
 * @brief Retrieve parameter metadata by its zero-based table index. @n
 * Получить метаданные по индексу в таблице, начиная с нуля.
 * @param index - entry index (0..urc_parameter_count()-1). @n
 * индекс параметра (0..urc_parameter_count()-1).
 * @return Pointer to const metadata or NULL if out of bounds. @n
 * Возвращает указатель на константные метаданные или NULL (при выходе за границы диапазона)
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
 * Возвращает указатель на константные метаданные или NULL (при отсутствие или ошибке аргументов)
 * @warning Do not free returned pointer. @n
 * Нельзя освобождать возвращенный указатель.
 */
const urc_parameter* urc_find_parameter(const char* unit, const char* name);

/** @} */

/** @defgroup urc_codec Yamaha SysEx codec
 *  Packet building and parsing, independent of any MIDI device. @n
 *  Формирование и разбор пакетов без подключения MIDI-устройства.
 *  @{ 
 */

/** @brief Encode a parameter change command (19 bytes). @n
 *  Закодировать команду записи параметра (19 байт).
 *  @param parameter - ID (0..16383).
 *  @param value - signed 32-bit value (no metadata range check). @n
 *  знаковое 32-битное значение (без проверки таблицы диапазонов).
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @param output - output buffer with capacity URC_CHANGE_LENGTH bytes. @n
 *  Выходной буфер минимум URC_CHANGE_LENGTH байт.
 *  @return URC_OK, URC_ERR_ARGUMENT or URC_ERR_RANGE. @n
 *  Возвращает URC_OK, URC_ERR_ARGUMENT или URC_ERR_RANGE.
 */
int urc_encode_change(uint16_t parameter, int32_t value, uint8_t channel, uint8_t output[URC_LENGTH_CHANGE]);

/** @brief Encode a parameter read request (15 bytes). @n
 *  Закодировать запрос параметра (15 байт).
 *  @param parameter - ID (0..16383). @n
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @param output - output buffer with capacity URC_QUERY_LENGTH bytes. @n
 *  выходной буфер минимум URC_QUERY_LENGTH байт.
 *  @return URC_OK, URC_ERR_ARGUMENT or URC_ERR_RANGE. @n
 *  Возвращает URC_OK, URC_ERR_ARGUMENT или URC_ERR_RANGE.
 */
int urc_encode_query(uint16_t parameter, uint8_t channel, uint8_t output[URC_LENGTH_QUERY]);

/** @brief Fill a 9-byte keepalive packet. @n
 *  Заполнить девятибайтовый пакет поддержания активности.
 *  @param output - writable buffer of URC_KEEPALIVE_LENGTH bytes; NULL is ignored. @n
 *  буфер для записи на URC_KEEPALIVE_LENGTH байт; NULL игнорируется.
 */
void urc_encode_keepalive(uint8_t output[URC_LENGTH_KEEPALIVE]);

/** @brief Identify and decode a complete SysEx packet. @n
 *  Определить тип и разобрать полный пакет SysEx.
 *  @param data - input byte sequence (does not have to be a recognized packet). @n
 *  массив байтов пакета (тип может быть неизвестен).
 *  @param length - number of bytes in @p data; at least 2. @n
 *  длина массива (минимум 2).
 *  @param result - output metadata; overwritten on every successful invocation. @n
 *  выходная структура; заполняется при успешном вызове.
 *  @return URC_ERR_ARGUMENT for NULL/short input; otherwise URC_OK even for
 *  unrecognized packets (result.type = URC_MSG_UNKNOWN). @n
 *  Возвращает URC_ERR_ARGUMENT при NULL/слишком коротком пакете; иначе URC_OK,
 *  даже если пакет неизвестен (result.type = URC_MSG_UNKNOWN).
 *  @note Meter payload fields are NOT parsed; use the raw-message callback. @n
 *  Поля индикаторов уровня не декодируются - используйте callback.
 */
int urc_decode(const uint8_t *data, size_t length, urc_message *result);

/** @} */

/** @defgroup urc_device_api Device and MIDI transport
 *  ALSA RawMIDI connection and incoming packet handler management. @n
 *  Управление подключением к ALSA RawMIDI и обработчиком входящих пакетов.
 *  @{ 
 */

/** @brief Open a named ALSA RawMIDI port and start the receiver. @n
 *  Открыть ALSA RawMIDI-порт и запустить поток приёма.
 *  @param device - output device handle; set to NULL before any allocation. @n
 *  указатель для результата; обнуляется перед выделением памяти.
 *  @param alsa_port - explicit subdevice name, e.g. "hw:2,0,1". @n
 *  явное имя MIDI-подпорта, например "hw:2,0,1".
 *  @return URC_OK, URC_ERR_ARGUMENT, URC_ERR_ALSA or URC_ERR_THREAD. @n
 *  Возвращает URC_OK, URC_ERR_ARGUMENT, URC_ERR_ALSA или URC_ERR_THREAD.
 *  @warning Select the *internal DSP mixer control* MIDI port, not MIDI DIN. 
 *  The library does not discover or choose a port automatically. @n
 *  Выбирайте *внутренний порт DSP-микшера*, а не MIDI DIN.
 *  Автоматического выбора порта нет.
 *  @note A separate input and output ALSA handle are opened non-blocking. @n
 *  ALSA-вход и выход открываются в неблокирующем режиме.
 */
int urc_open(urc_device **device, const char *alsa_port);

/** @brief Stop receiver, close ALSA handles and free the device. @n
 *  Остановить приём, закрыть ALSA и освободить устройство.
 *  @param device - handle returned by urc_open(); NULL is allowed. @n
 *  дескриптор из urc_open(); можно NULL.
 *  @warning Do not call from a callback or while another thread uses the device. @n
 *  Нельзя вызывать из callback и параллельно с операциями над устройством.
 */
void urc_close(urc_device *device);

/** @brief Register or clear the incoming SysEx callback. @n
 *  Назначить или отключить обработчик входящих SysEx.
 *  @param device - open device handle; NULL is ignored. @n
 *  открытое устройство; NULL игнорируется.
 *  @param callback - function to invoke or NULL to disable notifications. @n
 *  функция обработки или NULL для отключения.
 *  @param user_data - opaque pointer forwarded to the callback. @n
 *  пользовательский указатель, передаваемый обработчику.
 *  @note Replaces the callback atomically under response_mutex. @n
 *  Назначение обработчика защищено мьютексом.
 */
void urc_set_callback(urc_device *device, urc_message_callback callback, void *user_data);

/** @} */

/** @defgroup urc_io MIDI operations and synchronous transactions
 *  Low-level SysEx transmission and synchronous parameter read/write. @n
 *  Низкоуровневые операции SysEx и синхронное чтение/запись параметров.
 *  @{ 
 */

/** @brief Send a complete, prebuilt SysEx message via ALSA. @n
 *  Отправить готовый пакет SysEx через ALSA.
 *  @param device - open device handle. @n
 *  открытое устройство.
 *  @param message - byte buffer starting with F0 and ending with F7. @n
 *  массив байтов с F0 в начале и F7 в конце.
 *  @param length - buffer length in 2..URC_MAX_SYSEX. @n
 *  размер 2..URC_MAX_SYSEX байт.
 *  @return URC_OK or URC_ERR_ARGUMENT/URC_ERR_CLOSED/URC_ERR_ALSA. @n
 *  Возвращает URC_OK или URC_ERR_ARGUMENT/URC_ERR_CLOSED/URC_ERR_ALSA.
 *  @warning Does not verify that hardware accepted the command. Invalid
 *  or experimental bulk messages can alter device settings. @n
 *  Подтверждение принятия устройством не выполняется. Непроверенные
 *  bulk-пакеты могут изменить настройки оборудования.
 */
int urc_send_sysex(urc_device *device, const uint8_t *message, size_t length);

/** @brief Send the predefined Yamaha keepalive message. @n
 *  Отправить стандартное сообщение Yamaha keepalive.
 *  @param device - open device handle. @n
 *  открытое устройство.
 *  @return Result of urc_send_sysex(). @n
 *  Возвращает результат urc_send_sysex().
 */
int urc_send_keepalive(urc_device *device);

/** @brief Send a parameter change without readback confirmation. @n
 *  Отправить запись параметра без последующего чтения.
 *  @param device - open device handle. @n
 *  открытое устройство.
 *  @param parameter - ID (0..16383).
 *  @param value - signed 32-bit protocol value. @n
 *  знаковое 32-битное значение протокола.
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @return Codec/transport result. @n
 *  Возвращает код результата кодека или отправки.
 *  @note Does NOT enforce named parameter min/max metadata. @n
 *  Диапазон из таблицы именованных параметров не проверяется.
 */
int urc_change_parameter(urc_device *device, uint16_t parameter, int32_t value, uint8_t channel);

/** @brief Send a parameter Query without waiting for the Reply. @n
 *  Отправить Query, не ожидая ответа Reply.
 *  @param device - open device handle. @n
 *  дескриптор открытого устройства.
 *  @param parameter - ID (0..16383) 
 *  @param channel zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @return Codec/transport result. @n
 *  Возвращает код результата кодека или отправки.
 */
int urc_query_parameter(urc_device *device, uint16_t parameter, uint8_t channel);

/** @brief Read a parameter and wait for a matching Reply. @n
 *  Прочитать параметр с ожиданием совпадающего Reply.
 *  @param device - open device handle. @n
 *  дескриптор открытого устройства.
 *  @param parameter - ID (0..16383).
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @param timeout_ms - positive timeout in milliseconds. @n
 *  положительный таймаут в миллисекундах.
 *  @param value - output value written only after a matching Reply. @n
 *  указатель на результат; меняется только при совпавшем ответе.
 *  @return URC_OK, URC_ERR_TIMEOUT, URC_ERR_CLOSED or validation/transport error. @n
 *  Возвращает URC_OK, URC_ERR_TIMEOUT, URC_ERR_CLOSED или ошибку аргументов/ALSA. 
 *  @note Requests on the same device are serialized by request_mutex.
 *  Only the matching ID and channel satisfy a request. @n
 *  Запросы к одному устройству сериализуются мьютексом request_mutex.
 *  Совпасть должны и ID, и канал.
 */
int urc_get_parameter(urc_device *device, uint16_t parameter, uint8_t channel, int timeout_ms, int32_t *value);

/** @brief Write a numeric parameter with optional readback verification. @n
 *  Записать параметр по ID, при необходимости проверить чтением.
 *  @param device - open device handle. @n
 *  дескриптор открытого устройства.
 *  @param parameter - ID (0..16383). 
 *  @param value - signed protocol value (metadata range not checked). @n
 *  знаковое кодированное значение (без проверки таблицы диапазонов).
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @param confirm - if 1, read back and compare; may return URC_ERR_MISMATCH. @n
 *  если 1, прочитать и сверить; может вернуть URC_ERR_MISMATCH.
 *  @param timeout_ms - positive readback timeout (used when confirm=1). @n
 *  положительный таймаут чтения, если confirm=1. 
 *  @return URC_OK or an error code; URC_OK with confirm=false only means sent. @n
 *  Возвращает URC_OK или ошибка; при confirm=false URC_OK означает лишь отправку.
 */
int urc_set_parameter(urc_device *device, uint16_t parameter, int32_t value, uint8_t channel, uint8_t confirm, int timeout_ms);

/** @brief Read a parameter identified by exact names. @n
 *  Прочитать параметр по точному имени блока и параметра.
 *  @param device - open device handle. @n
 *  дескриптор открытого устройства.
 *  @param unit - unit name, case-sensitive. @n
 *  имя блока с учётом регистра.
 *  @param name - parameter name, case-sensitive.
 *  имя параметра с учётом регистра.
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала канал 0..5.
 *  @param timeout_ms - positive timeout in milliseconds. @n
 *  положительный таймаут в миллисекундах.
 *  @param value - output pointer to signed protocol value. @n
 *  указатель на возвращаемое значение.
 *  @return URC_ERR_UNKNOWN_PARAMETER if name not found; otherwise urc_get_parameter() result.
 *  Возвращает URC_ERR_UNKNOWN_PARAMETER при неизвестном имени, иначе результат urc_get_parameter().
 */
int urc_get_by_name(urc_device *device, const char *unit, const char *name, uint8_t channel, int timeout_ms, int32_t *value);

/** @brief Write a named parameter with catalog-range validation. @n
 *  Записать параметр по имени с проверкой диапазона из каталога.
 *  @param device - open device handle. @n
 *  дескриптор открытого устройства.
 *  @param unit - unit name, case-sensitive. @n
 *  имя блока с учётом регистра.
 *  @param name - parameter name, case-sensitive. @n
 *  имя параметра с учётом регистра.
 *  @param value - encoded protocol value in [minimum, maximum]. @n
 *  значение протокола из [minimum, maximum].
 *  @param channel - zero-based channel in 0..5. @n
 *  номер канала 0..5.
 *  @param confirm - verify by readback if true. @n
 *  проверять чтением при true.
 *  @param timeout_ms - readback timeout in milliseconds (if used). @n
 *  таймаут чтения в миллисекундах, если включено.
 *  @return URC_ERR_UNKNOWN_PARAMETER, URC_ERR_RANGE or urc_set_parameter() result. @n
 *  Возвращает URC_ERR_UNKNOWN_PARAMETER, URC_ERR_RANGE или результат urc_set_parameter().
 *  @warning Catalog ranges do not guarantee hardware support or safety. @n
 *  Диапазоны таблицы не гарантируют поддержку и безопасность на устройстве.
 */
int urc_set_by_name(urc_device *device, const char *unit, const char *name, int32_t value, uint8_t channel, uint8_t confirm, int timeout_ms);

/** @} */


#ifdef __cplusplus
}
#endif // __cplusplus

#endif // URCONTROL_H
