/**
 * @file urcontrol.c
 * @brief Yamaha SysEx codec, ALSA RawMIDI transport and synchronized request engine. @n
 * Кодирование Yamaha SysEx, транспорт ALSA RawMIDI и синхронные запросы.
 * @details The receiver polls an ALSA input, reconstructs SysEx packets, signals
 * a waiting synchronous operation and optionally invokes the user callback.
 * Поток приёма читает ALSA, собирает пакеты, уведомляет ожидающий
 * запрос и при необходимости вызывает пользовательский callback.
 */

#include "urcontrol.h"

#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>

#include <alsa/asoundlib.h>

/**
 * @struct urc_device
 * @brief Internal state of one ALSA rawmidi connection. @n
 * Внутреннее состояние одного соединения ALSA RawMIDI.
 * @details Contains the input/output handles, receive thread, request and
 * send locks, matching reply state and a streaming SysEx buffer. @n
 * Содержит ALSA-дескрипторы, поток приёма, мьютексы запросов/записи,
 * состояние ожидаемого ответа и буфер сборки SysEx.
 */
struct urc_device
{
    snd_rawmidi_t* input;           ///< ALSA input endpoint. 
                                    ///< Вход ALSA. 

    snd_rawmidi_t* output;          ///< ALSA output endpoint. 
                                    ///< Выход ALSA. 

    pthread_t receiver;             ///< Receiver thread handle. 
                                    ///< Поток приёма. 

    atomic_bool running;            ///< Receive/transport is active. 
                                    ///< Работа транспорта/приёма разрешена. 

    pthread_mutex_t request_mutex;  ///< Serializes synchronous transactions. 
                                    ///< Сериализует синхронные запросы. 

    pthread_mutex_t write_mutex;    ///< Serializes writes to ALSA. 
                                    ///< Сериализует запись в ALSA. 

    pthread_mutex_t response_mutex; ///< Protects reply state and callback pointer. 
                                    ///< Защищает ожидание ответа и callback. 

    pthread_cond_t response_cond;   ///< Signals matching reply/receiver exit. 
                                    ///< Сигнал о получении Reply/остановке приёма. 

    uint8_t waiting;                ///< A synchronous Query is pending. 
                                    ///< Ожидается синхронный ответ. 

    uint8_t received;               ///< Matching Reply has arrived. 
                                    ///< Совпадающий Reply получен. 

    uint16_t waiting_parameter;     ///< Expected parameter ID. 
                                    ///< Ожидаемый ID. 

    uint8_t waiting_channel;        ///< Expected channel (0-based). 
                                    ///< Ожидаемый канал, нумеруемый с нуля. 

    int32_t response_value;         ///< Captured Reply value. 
                                    ///<  Значение из ответа. 

    urc_message_callback callback;  ///< Optional incoming-message callback. 
                                    ///< Необязательный обработчик сообщений. 

    void* callback_user_data;       ///< Callback context pointer. 
                                    ///< Пользовательский контекст callback. 


    uint8_t sysex[URC_LENGTH_MAX_SYSEX]; ///< Reassembly buffer. 
                                         ///< Буфер сборки SysEx.

    size_t sysex_size;                   ///< Occupied bytes in buffer. 
                                         ///< Число принятых байтов. 
};

/**
 * @brief Compare a received byte sequence with an expected fixed header. @n
 * Проверить наличие ожидаемого заголовка в массиве байтов.
 * @param message - packet bytes. @n
 * байты пакета.
 * @param length - packet length. @n
 * длина пакета.
 * @param prefix - expected header bytes. @n
 * ожидаемый заголовок.
 * @param prefix_size - header length. @n
 * длина заголовка.
 * @return 1 if the prefix matches. @n
 * Возвращает 1 при совпадении начала пакета.
 */
static uint8_t has_prefix(const uint8_t *message, size_t length, const uint8_t *prefix, size_t prefix_size)
{
    return length >= prefix_size && memcmp(message, prefix, prefix_size) == 0;
}

/**
 * @brief Validate a protocol parameter ID and channel index. @n
 * Проверить ID параметра и индекс канала.
 * @param parameter - parameter ID. 
 * ID параметра.
 * @param channel - zero-based channel. @n
 * канал с нуля.
 * @return URC_OK or URC_ERR_RANGE. @n
 * Возвращает URC_OK или URC_ERR_RANGE.
 */
static int parameter_ok(uint16_t parameter, uint8_t channel)
{
    if (parameter > 0x3fff 
    ||  channel > 5)
        return URC_ERR_RANGE;

    return URC_OK;
}

/**
 * @brief Encode a two’s-complement int32 into five 7-bit MIDI data bytes. @n
 * Закодировать int32 в дополнительном коде пятью 7-битными байтами MIDI.
 * @param value - signed input value. @n
 * знаковое входное значение.
 * @param dest - writable array of at least five bytes. @n
 * буфер не меньше пяти байтов.
 * @note The highest encoded digit uses only its low four bits. @n
 * Старший разряд кодируется только четырьмя младшими битами.
 */
static void encode_value(int32_t value, uint8_t *dest)
{
    uint32_t n = (uint32_t)value;
    for (size_t i = 0; i < 5; ++i)
    {
        dest[4 - i] = (uint8_t)(n & 0x7f);
        n >>= 7;
    }
}

/**
 * @brief Decode five 7-bit bytes as signed int32. @n
 * Декодировать пять 7-битных байтов в знаковое int32.
 * @param encoded - input bytes (five elements). @n
 * входные пять байтов.
 * @param value - output pointer. @n
 * указатель на результат.
 * @return URC_OK or error when digits violate 7-bit/32-bit encoding. @n
 * URC_OK либо ошибка при нарушении формата 7/32 бит.
 */
static int decode_value(const uint8_t *encoded, int32_t *value)
{
    if (encoded[0] > 0x0f)
        return URC_ERR_RANGE; 
        
    uint32_t n = 0;
    for (size_t i = 0; i < 5; ++i)
    {
        if (encoded[i] & 0x80)
            return URC_ERR_ARGUMENT;
        n = (n << 7) | encoded[i];
    }

    // Convert to signed value
    // Преобразование к знаковому значению
    *value = (int32_t)n;

    return URC_OK;
}

/**
 * Header data for Change request
 * Данные заголовка для запроса Change
 */
static const uint8_t change_header[] = {0xf0, 0x43, 0x10, 0x3e, 0x14, 0x01, 0x01, 0x00};
/**
 * Header data for Query request
 * Данные заголовка для запроса Query
 */
static const uint8_t query_header[] = {0xf0, 0x43, 0x30, 0x3e, 0x14, 0x01, 0x04, 0x02, 0x00};

int urc_encode_change(uint16_t parameter, int32_t value, uint8_t channel, uint8_t output[URC_LENGTH_CHANGE])
{
    if (output == NULL)
        return URC_ERR_ARGUMENT;

    int result = parameter_ok(parameter, channel);
    if (result != URC_OK)
        return result;
    
    memcpy(output, change_header, sizeof(change_header));
    output[8] = (uint8_t)(parameter >> 7);
    output[9] = (uint8_t)(parameter & 0x7f);
    output[10] = 0;
    output[11] = 0;
    output[12] = channel;
    encode_value(value, &output[13]);
    output[18] = 0xf7;

    return URC_OK;
}

int urc_encode_query(uint16_t parameter, uint8_t channel, uint8_t output[URC_LENGTH_QUERY])
{
    if (output == NULL)
        return URC_ERR_ARGUMENT;

    int result = parameter_ok(parameter, channel);
    if (result != URC_OK)
        return result;

    memcpy(output, query_header, sizeof(query_header));
    output[9] = (uint8_t)(parameter >> 7);
    output[10] = (uint8_t)(parameter & 0x7f);
    output[11] = 0;
    output[12] = 0;
    output[13] = channel;
    output[14] = 0xf7;

    return URC_OK;
}

void urc_encode_keepalive(uint8_t output[URC_LENGTH_KEEPALIVE])
{
    if (output != NULL)
    {
        const uint8_t keepalive[URC_LENGTH_KEEPALIVE] = {0xf0, 0x43, 0x10, 0x3e, 0x14, 0x00, 0x04, 0x02, 0xf7};
        memcpy(output, keepalive, sizeof(keepalive));
    }
}

int urc_decode(const uint8_t *data, size_t length, urc_message *result)
{
    if (data == NULL 
    ||  result == NULL 
    ||  length < 2)
        return URC_ERR_ARGUMENT;

    memset(result, 0, sizeof(*result));
    result->type = URC_MSG_UNKNOWN;

    if (data[0] != 0xf0 
    ||  data[length - 1] != 0xf7)
        return URC_OK;

    for (size_t i = 1; i + 1 < length; ++i)
        if (data[i] & 0x80)
            return URC_OK;

    const uint8_t reply_header[] = {0xf0, 0x43, 0x10, 0x3e, 0x14, 0x01, 0x04, 0x02, 0x00};
    const uint8_t meter_header[] = {0xf0, 0x43, 0x10, 0x3e, 0x14, 0x02, 0x03};
    uint8_t channel;
    uint16_t parameter;
    int32_t value;

    if (length == URC_LENGTH_CHANGE 
    &&  has_prefix(data, length, change_header, sizeof(change_header)) 
    &&  data[10] == 0 
    &&  data[11] == 0)
    {
        parameter = ((uint16_t)data[8] << 7) | data[9];
        channel = data[12];

        if (parameter_ok(parameter, channel) != URC_OK 
        ||  decode_value(&data[13], &value) != URC_OK)
            return URC_OK;

        result->type = URC_MSG_CHANGE;
        result->parameter = parameter;
        result->channel = channel;
        result->value = value;
    }
    else 
    if (length == URC_LENGTH_QUERY 
    &&  has_prefix(data, length, query_header, sizeof(query_header)) 
    &&  data[11] == 0 
    &&  data[12] == 0)
    {
        parameter = ((uint16_t)data[9] << 7) | data[10];
        channel = data[13];

        if (parameter_ok(parameter, channel) != URC_OK)
            return URC_OK;

        result->type = URC_MSG_QUERY;
        result->parameter = parameter;
        result->channel = channel;
    }
    else 
    if (length == URC_LENGTH_REPLY 
    &&  has_prefix(data, length, reply_header, sizeof(reply_header)) 
    &&  data[11] == 0 
    &&  data[12] == 0)
    {
        parameter = ((uint16_t)data[9] << 7) | data[10];
        channel = data[13];

        if (parameter_ok(parameter, channel) != URC_OK 
        ||  decode_value(&data[14], &value) != URC_OK)
            return URC_OK;

        result->type = URC_MSG_REPLY;
        result->parameter = parameter;
        result->channel = channel;
        result->value = value;
    }
    else 
    if (length == URC_LENGTH_KEEPALIVE)
    {
        uint8_t keepalive[URC_LENGTH_KEEPALIVE];
        urc_encode_keepalive(keepalive);

        if (memcmp(data, keepalive, length) == 0)
            result->type = URC_MSG_KEEPALIVE;
    }
    else 
    if (has_prefix(data, length, meter_header, sizeof(meter_header)))
    {
        result->type = URC_MSG_METER;
        /* Original Python version recognizes this message but does not
         * decode its individual meter fields. Raw bytes go to callback. 
         * Оригинальная версия на Python обрабатывает это сообщение, но
         * не декодирует значения шкалы. Сырые байты возвращаются в callback. 
         */
    }
    return URC_OK;
}

const char *urc_strerror(int code)
{
    switch (code)
    {
        case URC_OK: 
            return "success";
        case URC_ERR_ARGUMENT: 
            return "invalid argument";
        case URC_ERR_RANGE: 
            return "value, parameter or channel out of range";
        case URC_ERR_ALSA: 
            return "ALSA raw MIDI error (check port and permissions)";
        case URC_ERR_TIMEOUT: 
            return "device did not return the requested parameter in time";
        case URC_ERR_MISMATCH: 
            return "device returned a different value after write";
        case URC_ERR_CLOSED: 
            return "device is closed";
        case URC_ERR_UNKNOWN_PARAMETER: 
            return "unknown unit or parameter";
        case URC_ERR_THREAD: 
            return "could not start receive thread";
        case URC_ERR_INACTIVE: 
            return "requested DSP effect is not active";
        case URC_ERR_MEMORY: 
            return "memory allocation failed";
        default: 
            return "unknown error";
    }
}

/**
 * @brief Deliver a complete packet to the waiting request and optional callback. @n
 * Передать полный пакет ожидающему запросу и пользовательскому callback.
 * @param device - active device. @n
 * активное устройство.
 * @param length - SysEx length in the device buffer. @n
 * длина SysEx в буфере устройства.
 * @note Matching is by Reply type, ID and channel. Callback executes without holding response_mutex. @n
 * Сопоставляются тип Reply, ID и канал. Callback вызывается без удержания response_mutex.
 */
static void process_sysex(urc_device *device, size_t length)
{
    urc_message message;

    if (urc_decode(device->sysex, length, &message) != URC_OK)
        return;

    pthread_mutex_lock(&device->response_mutex);
    if (device->waiting 
    &&  !device->received 
    &&  message.type == URC_MSG_REPLY 
    &&  message.parameter == device->waiting_parameter 
    &&  message.channel == device->waiting_channel)
    {
        device->received = 1;
        device->response_value = message.value;
        pthread_cond_signal(&device->response_cond);
    }

    urc_message_callback callback = device->callback;
    void *user_data = device->callback_user_data;
    pthread_mutex_unlock(&device->response_mutex);

    if (callback != NULL)
        callback(&message, device->sysex, length, user_data);
}

/**
 * @brief Consume a MIDI byte and incrementally reassemble a SysEx packet. @n
 * Принять MIDI-байт и продолжить сборку пакета SysEx. 
 * @param device - receiver context. @n
 * контекст приёма.
 * @param byte - next MIDI byte. @n
 * очередной MIDI-байт.
 * @note Ignores MIDI realtime bytes. Drops oversized/invalid frames. @n
 * Игнорирует байты MIDI-сообщений реального времени. Отбрасывает слишком длинные и некорректные пакеты
 */
static void ingest_byte(urc_device *device, uint8_t byte)
{
    /* MIDI realtime events may be interleaved with SysEx. */
    if (byte >= 0xf8)
    {
        return;
    }
    if (byte == 0xf0)
    {
        device->sysex_size = 0;
    }
    if (device->sysex_size == 0 && byte != 0xf0)
    {
        return;
    }
    if (device->sysex_size >= sizeof(device->sysex))
    {
        device->sysex_size = 0;
        return;
    }
    device->sysex[device->sysex_size++] = byte;
    if (byte == 0xf7)
    {
        process_sysex(device, device->sysex_size);
        device->sysex_size = 0;
    }
    else if ((byte & 0x80) && byte != 0xf0)
    {
        device->sysex_size = 0;
    }
}

/**
 * @brief Poll ALSA input, feed the SysEx decoder and wake pending readers on exit. @n
 * Опросить ALSA-вход, собирать SysEx и разбудить ожидающие запросы при остановке.
 * @param context - pointer to urc_device. @n
 * указатель на urc_device.
 * @return Always NULL (pthread entry point - pthread_create). @n
 * Всегда возвращает NULL (точка входа pthread - pthread_create).
 * @warning User callbacks execute on this thread. @n
 * Пользовательские обработчики выполняются в этом потоке.
 */
static void *receive_thread(void *context)
{
    urc_device *device = context;

    int count = snd_rawmidi_poll_descriptors_count(device->input);
    if (count <= 0 
    ||  count > 64)
    {
        atomic_store(&device->running, 0);
        return NULL;
    }

    struct pollfd *fds = calloc((size_t)count, sizeof(*fds));
    if (fds == NULL)
    {
        atomic_store(&device->running, 0);
        return NULL;
    }

    if (snd_rawmidi_poll_descriptors(device->input, fds, (unsigned int)count) < 0)
    {
        free(fds);
        atomic_store(&device->running, 0);
        return NULL;
    }

    while (atomic_load(&device->running))
    {
        int ready = poll(fds, (nfds_t)count, 50);
        if (ready < 0)
        {
            if (errno == EINTR)
                continue;

            break;
        }
        if (ready == 0)
            continue;

        uint8_t error = 0;
        for (int i = 0; i < count; ++i)
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                error = 1;

        if (error)
            break;

        uint8_t bytes[512];
        ssize_t read_count = -EAGAIN;
        while ((atomic_load(&device->running)) 
        &&     (read_count = snd_rawmidi_read(device->input, bytes, sizeof(bytes))) > 0)
            for (ssize_t i = 0; i < read_count; ++i)
                ingest_byte(device, bytes[i]);

        if (read_count < 0 && read_count != -EAGAIN && read_count != -EINTR)
            break;
    }

    free(fds);
    atomic_store(&device->running, 0);
    pthread_mutex_lock(&device->response_mutex);
    pthread_cond_broadcast(&device->response_cond);
    pthread_mutex_unlock(&device->response_mutex);

    return NULL;
}

int urc_open(urc_device **target, const char *alsa_port)
{
    if (target == NULL 
    ||  alsa_port == NULL 
    ||  alsa_port[0] == 0)
        return URC_ERR_ARGUMENT;

    *target = NULL;
    urc_device *device = calloc(1, sizeof(*device));

    if (device == NULL)
        return URC_ERR_ALSA;

    if (pthread_mutex_init(&device->request_mutex, NULL) != 0)
    {
        free(device);

        return URC_ERR_THREAD;
    }

    if (pthread_mutex_init(&device->write_mutex, NULL) != 0)
    {
        pthread_mutex_destroy(&device->request_mutex);
        free(device);

        return URC_ERR_THREAD;
    }

    if (pthread_mutex_init(&device->response_mutex, NULL) != 0)
    {
        pthread_mutex_destroy(&device->write_mutex);
        pthread_mutex_destroy(&device->request_mutex);
        free(device);

        return URC_ERR_THREAD;
    }

    if (pthread_cond_init(&device->response_cond, NULL) != 0)
    {
        pthread_mutex_destroy(&device->response_mutex);
        pthread_mutex_destroy(&device->write_mutex);
        pthread_mutex_destroy(&device->request_mutex);
        free(device);

        return URC_ERR_THREAD;
    }

    if (snd_rawmidi_open(&device->input, &device->output, alsa_port, SND_RAWMIDI_NONBLOCK) < 0)
    {
        if (device->input != NULL) 
            snd_rawmidi_close(device->input);

        if (device->output != NULL) 
            snd_rawmidi_close(device->output);

        pthread_cond_destroy(&device->response_cond);
        pthread_mutex_destroy(&device->response_mutex);
        pthread_mutex_destroy(&device->write_mutex);
        pthread_mutex_destroy(&device->request_mutex);
        free(device);

        return URC_ERR_ALSA;
    }

    atomic_init(&device->running, 1);
    if (pthread_create(&device->receiver, NULL, receive_thread, device) != 0)
    {
        snd_rawmidi_close(device->input);
        snd_rawmidi_close(device->output);
        pthread_cond_destroy(&device->response_cond);
        pthread_mutex_destroy(&device->response_mutex);
        pthread_mutex_destroy(&device->write_mutex);
        pthread_mutex_destroy(&device->request_mutex);
        free(device);

        return URC_ERR_THREAD;
    }

    *target = device;

    return URC_OK;
}

void urc_close(urc_device *device)
{
    if (device == NULL)
        return;

    atomic_store(&device->running, 0);
    pthread_join(device->receiver, NULL);
    snd_rawmidi_close(device->input);
    snd_rawmidi_close(device->output);
    pthread_cond_destroy(&device->response_cond);
    pthread_mutex_destroy(&device->response_mutex);
    pthread_mutex_destroy(&device->write_mutex);
    pthread_mutex_destroy(&device->request_mutex);
    free(device);
}

void urc_set_callback(urc_device *device, urc_message_callback callback, void *user_data)
{
    if (device == NULL)
        return;

    pthread_mutex_lock(&device->response_mutex);
    device->callback = callback;
    device->callback_user_data = user_data;
    pthread_mutex_unlock(&device->response_mutex);
}

int urc_send_sysex(urc_device *device, const uint8_t *message, size_t length)
{
    if (device == NULL 
    ||  message == NULL 
    ||  length < 2 
    ||  length > URC_LENGTH_MAX_SYSEX 
    ||  message[0] != 0xf0 
    ||  message[length - 1] != 0xf7)
        return URC_ERR_ARGUMENT;

    for (size_t i = 1; i + 1 < length; ++i)
        if (message[i] & 0x80)
            return URC_ERR_ARGUMENT;
    
    if (!atomic_load(&device->running))
        return URC_ERR_CLOSED;
    
    pthread_mutex_lock(&device->write_mutex);
    size_t written = 0;
    unsigned int retries = 0;
    while (written < length && atomic_load(&device->running))
    {
        ssize_t n = snd_rawmidi_write(device->output, message + written,
                                      length - written);
        if (n > 0)
        {
            written += (size_t)n;
            retries = 0;
            continue;
        }

        if (n != -EAGAIN && n != -EINTR && n != 0)
            break;

        if (++retries > 200)
            break;
        
        struct timespec pause = { .tv_sec = 0, .tv_nsec = 10000000L };
        nanosleep(&pause, NULL);
    }

    pthread_mutex_unlock(&device->write_mutex);

    return written == length ? URC_OK : URC_ERR_ALSA;
}

int urc_send_keepalive(urc_device *device)
{
    uint8_t message[URC_LENGTH_KEEPALIVE];
    urc_encode_keepalive(message);

    return urc_send_sysex(device, message, sizeof(message));
}

int urc_meter_decode(const uint8_t *raw, size_t length, size_t index, urc_meter_level *level)
{
    if (raw == NULL
    ||  level == NULL
    ||  length != URC_LENGTH_METER)
        return URC_ERR_ARGUMENT;

    if (index >= URC_METER_SLOTS)
        return URC_ERR_RANGE;

    static const uint8_t meter_header[] = { 0xF0, 0x43, 0x10, 0x3E, 0x14, 0x02, 0x03 };

    if (memcmp(raw, meter_header, sizeof(meter_header)) != 0
    ||  raw[length - 1] != 0xF7)
        return URC_ERR_ARGUMENT;

    for (size_t i = 1; i < length - 1; ++i)
        if (raw[i] & 0x80)
            return URC_ERR_ARGUMENT;

    size_t offset = sizeof(meter_header) + index * 4;

    int current = (raw[offset] << 7) | raw[offset + 1];
    int peak = (raw[offset + 2] << 7) | raw[offset + 3];

    if (current & 0x2000)
        current -= 0x4000;

    if (peak & 0x2000)
        peak -= 0x4000;

    level->current = (int16_t)current;
    level->peak = (int16_t)peak;

    return URC_OK;
}

int urc_meters_start(urc_device *device)
{
    static const uint8_t meter_on[] = { 0xf0, 0x43, 0x30, 0x3e, 0x14, 0x02, 0x03, 0x32, 0x7f, 0xf7 };

    int result = urc_send_sysex(device, meter_on, sizeof(meter_on));
    if (result != URC_OK)
        return result;

    result = urc_send_keepalive(device);
    if (result != URC_OK)
        urc_meters_stop(device);
    return result;
}

int urc_meters_stop(urc_device *device)
{
    static const uint8_t meter_off[] = { 0xf0, 0x43, 0x30, 0x3e, 0x14, 0x02, 0x03, 0x32, 0x00, 0xf7 };

    return urc_send_sysex(device, meter_off, sizeof(meter_off));
}

int urc_change_parameter(urc_device *device, uint16_t parameter,
                         int32_t value, uint8_t channel)
{
    uint8_t message[URC_LENGTH_CHANGE];
    int result = urc_encode_change(parameter, value, channel, message);

    return result != URC_OK ? result : urc_send_sysex(device, message, sizeof(message));
}

int urc_query_parameter(urc_device *device, uint16_t parameter,
                        uint8_t channel)
{
    uint8_t message[URC_LENGTH_QUERY];
    int result = urc_encode_query(parameter, channel, message);

    return result != URC_OK ? result : urc_send_sysex(device, message, sizeof(message));
}

/**
 * @brief Convert a relative millisecond timeout into a CLOCK_REALTIME deadline. @n
 * Преобразовать относительный таймаут в абсолютное время CLOCK_REALTIME.
 * @param end - output deadline. @n
 * указатель на дедлайн.
 * @param ms - nonnegative timeout in milliseconds. @n
 * положительный таймаут в миллисекундах.
 */
static void deadline_from_now(struct timespec *end, int ms)
{
    clock_gettime(CLOCK_REALTIME, end);
    end->tv_sec += ms / 1000;
    end->tv_nsec += (long)(ms % 1000) * 1000000L;
    if (end->tv_nsec >= 1000000000L)
    {
        end->tv_sec++;
        end->tv_nsec -= 1000000000L;
    }
}

/**
 * @brief Perform a Query/Reply transaction with request_mutex held by caller. @n
 * Выполнить Query/Reply при заранее захваченном request_mutex.
 * @param device - open device handle. @n
 * дескриптор открытого устройства.
 * @param parameter - requested parameter ID. @n
 * ID запрошенного параметра.
 * @param channel - zero-based channel. @n
 * канал с нуля.
 * @param timeout_ms positive timeout. @n
 * положительный таймаут.
 * @param value - output value if response matches. @n
 * результат при совпадении ответа.
 * @return URC_OK or validation, transport, timeout or closed error. @n
 * Возвращает URC_OK либо ошибка аргументов, транспорта, таймаута или закрытия.
 * @pre Caller holds device->request_mutex. Sets waiting state before sending Query to prevent reply races. @n
 * Вызывающий захватил device->request_mutex. Ожидание устанавливается до отправки Query, чтобы не потерять быстрый ответ.
 */
static int get_parameter_locked(urc_device *device, uint16_t parameter, uint8_t channel, int timeout_ms, int32_t *value)
{
    if (value == NULL 
    ||  timeout_ms <= 0)
        return URC_ERR_ARGUMENT;

    int result = parameter_ok(parameter, channel);
    if (result != URC_OK)
        return result;
    
    if (!atomic_load(&device->running))
        return URC_ERR_CLOSED;
    
    pthread_mutex_lock(&device->response_mutex);
    device->waiting = 1;
    device->received = 0;
    device->waiting_parameter = parameter;
    device->waiting_channel = channel;
    pthread_mutex_unlock(&device->response_mutex);

    result = urc_query_parameter(device, parameter, channel);
    if (result != URC_OK)
    {
        pthread_mutex_lock(&device->response_mutex);
        device->waiting = 0;
        pthread_mutex_unlock(&device->response_mutex);
        return result;
    }

    struct timespec end;
    deadline_from_now(&end, timeout_ms);
    pthread_mutex_lock(&device->response_mutex);
    while (!device->received && atomic_load(&device->running))
    {
        int wait_error = pthread_cond_timedwait(&device->response_cond, &device->response_mutex, &end);
        if (wait_error == ETIMEDOUT)
            break;

        if (wait_error != 0)
            break;
    }

    uint8_t received = device->received;
    if (received)
        *value = device->response_value;

    device->waiting = 0;
    pthread_mutex_unlock(&device->response_mutex);

    return received ? URC_OK : (atomic_load(&device->running) ? URC_ERR_TIMEOUT : URC_ERR_CLOSED);
}

int urc_get_parameter(urc_device *device, uint16_t parameter, uint8_t channel, int timeout_ms, int32_t *value)
{
    if (device == NULL)
        return URC_ERR_ARGUMENT;

    pthread_mutex_lock(&device->request_mutex);
    int result = get_parameter_locked(device, parameter, channel, timeout_ms, value);
    pthread_mutex_unlock(&device->request_mutex);
    
    return result;
}

int urc_set_parameter(urc_device *device, uint16_t parameter, int32_t value, uint8_t channel, uint8_t confirm, int timeout_ms)
{
    if (device == NULL)
        return URC_ERR_ARGUMENT;
        
    pthread_mutex_lock(&device->request_mutex);
    int result = urc_change_parameter(device, parameter, value, channel);
    if (result == URC_OK 
    &&  confirm)
    {
        int32_t readback = 0;
        result = get_parameter_locked(device, parameter, channel, timeout_ms, &readback);
        if (result == URC_OK && readback != value)
            result = URC_ERR_MISMATCH;
    }
    pthread_mutex_unlock(&device->request_mutex);

    return result;
}

int urc_get_by_name(urc_device *device, const char *unit, const char *name, uint8_t channel, int timeout_ms, int32_t *value)
{
    const urc_parameter *parameter = urc_find_parameter(unit, name);
    if (parameter == NULL)
        return URC_ERR_UNKNOWN_PARAMETER;
        
    return urc_get_parameter(device, parameter->id, channel, timeout_ms, value);
}

int urc_set_by_name(urc_device *device, const char *unit, const char *name, int32_t value, uint8_t channel, uint8_t confirm, int timeout_ms)
{
    const urc_parameter *parameter = urc_find_parameter(unit, name);
    if (parameter == NULL)
        return URC_ERR_UNKNOWN_PARAMETER;

    if (value < parameter->minimum 
    ||  value > parameter->maximum)
        return URC_ERR_RANGE;

    return urc_set_parameter(device, parameter->id, value, channel,
                             confirm, timeout_ms);
}
