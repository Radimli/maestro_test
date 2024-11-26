/*
 * Copyright 2018-2022 NXP.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*!
 * @file    audio_sink_default.c
 * @brief   This file contains the implementation for the default device
 *          Audio Sink Interface
 *
 */

#include <string.h>

#include "pipeline.h"
#include "audio_cfg.h"
#include "audio_sink_default.h"
#include "audio_sink.h"
#include "streamer_element_properties.h"

AudioSinkStreamErrorType audiosink_default_init_params(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;
    uint32_t dev_sample_rate = 0;
    uint32_t dev_bit_width   = 0;
    uint8_t dev_num_channels = 0;
    uint8_t sign, endianness, interleaved;
    uint32_t word_length;
    int ret;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    dev_info    = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    sign        = AUDIO_SIGNED(audio_sink_ptr);
    endianness  = AUDIO_ENDIANNESS(audio_sink_ptr);
    word_length = AUDIO_WORD_LENGTH(audio_sink_ptr);
    interleaved = AUDIO_INTERLEAVED(audio_sink_ptr);

    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] sign = %d\n", sign);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] endianness = %d\n", endianness);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] interleaved = %d\n", interleaved);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] word_length = %d\n", word_length);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] sample rate = %d\n", audio_sink_ptr->sample_rate);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] num channels = %d\n", audio_sink_ptr->num_channels);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] chunk size = %d\n", audio_sink_ptr->chunk_size);
    STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] bits_per_sample= %d\n", audio_sink_ptr->bits_per_sample);

    /* Check if necessary functions exists */
    if ((audio_sink_ptr->appFunctions.set_param_func == NULL) ||
        (audio_sink_ptr->appFunctions.get_param_func == NULL) || (audio_sink_ptr->appFunctions.process_func == NULL))
    {
        return AUDIO_SINK_FAILED;
    }

    /* Set samplerate */
    /* Set audio/PCM channel. */
    /* Set PCM data format (bits per sample, tx fmt) */
    /* Setup pipeline in/out ports */
    ret =
        audio_sink_ptr->appFunctions.set_param_func(audio_sink_ptr->sample_rate, audio_sink_ptr->bits_per_sample,
                                                    audio_sink_ptr->num_channels, true, false, audio_sink_ptr->volume);

    if (ret != 0)
    {
        STREAMER_LOG_ERR(DBG_AUDIO_SINK, ERRCODE_NOT_SUPPORTED, "[DEFAULT Sink] failed to set device params\n");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_FAILED;
    }

    audio_sink_ptr->appFunctions.get_param_func(&dev_sample_rate, &dev_bit_width, &dev_num_channels);

    if ((dev_bit_width < audio_sink_ptr->bits_per_sample) || (dev_num_channels < audio_sink_ptr->num_channels))
    {
        STREAMER_LOG_ERR(DBG_AUDIO_SINK, ERRCODE_NOT_SUPPORTED,
                         "[DEFAULT Sink] unsupported PCM device parameters: bit width and/or channels number \n");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_FAILED;
    }

    audio_sink_ptr->codec_bits_per_sample = dev_bit_width;
    audio_sink_ptr->codec_num_channels    = dev_num_channels;

    dev_info->resample = false;

    /* START playback */
    if (audio_sink_ptr->appFunctions.start_func != NULL)
    {
        audio_sink_ptr->appFunctions.start_func();
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_init_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info = NULL;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    if (audio_sink_ptr == NULL)
    {
        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "Audio SINK element is NULL");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    dev_info = OSA_MemoryAllocate(sizeof(DefaultSinkDeviceInfo));
    if (!dev_info)
        return AUDIO_SINK_ERROR_OUT_OF_HEAP_MEMORY;

    memset(dev_info, 0, sizeof(DefaultSinkDeviceInfo));

    audio_sink_ptr->device_info = dev_info;
    dev_info->device_state      = AUDIO_SINK_DEVICE_STATE_OPENED;
    dev_info->resample          = false;

    if (audio_sink_ptr->refData_element)
    {
        if (STREAM_OK != element_set_property(audio_sink_ptr->refData_element, PROP_VOICESEEKER_REFDATA_NUM_BUFFERS,
                                              AUDIO_SINK_BUFFER_NUM + 1))
        {
            return AUDIO_SINK_FAILED;
        }
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_deinit_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    if (audio_sink_ptr == NULL)
    {
        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "Audio SINK element is NULL");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    if (!dev_info)
        return AUDIO_SINK_FAILED;

    dev_info->device_state = AUDIO_SINK_DEVICE_STATE_CLOSED;

    OSA_MemoryFree(audio_sink_ptr->device_info);
    audio_sink_ptr->device_info = NULL;

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

uint8_t audiosink_default_sink_pad_activation_handler(StreamPad *pad, uint8_t active)
{
    /* Do nothing. */

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);
    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return true;
}

FlowReturn audiosink_default_sink_pad_chain_handler(StreamPad *pad, StreamBuffer *buffer)
{
    ElementAudioSink *audio_sink_ptr = NULL;
    AudioPacketHeader *data_packet   = NULL;
    DefaultSinkDeviceInfo *dev_info      = NULL;

    int8_t *buffer_ptr   = NULL;
    uint32_t buffer_size = 0;
    int ret;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    audio_sink_ptr = (ElementAudioSink *)PAD_PARENT(pad);
    dev_info       = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    data_packet    = (AudioPacketHeader *)(BUFFER_PTR(buffer));

    if (!dev_info)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return FLOW_ERROR;
    }

    if (dev_info->device_state == AUDIO_SINK_DEVICE_STATE_CLOSED)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_ERROR_DEVICE_OPEN_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return FLOW_WRONG_STATE;
    }

    audio_sink_ptr->chunk_size      = AUDIO_CHUNK_SIZE(data_packet);
    audio_sink_ptr->bits_per_sample = AUDIO_BITS_PER_SAMPLE(data_packet);
    audio_sink_ptr->sample_rate     = AUDIO_SAMPLE_RATE(data_packet);
    audio_sink_ptr->num_channels    = AUDIO_NUM_CHANNELS(data_packet);
    audio_sink_ptr->format          = AUDIO_FORMAT(data_packet);

    buffer_ptr  = (BUFFER_PTR(buffer) + sizeof(AudioPacketHeader));
    buffer_size = (BUFFER_SIZE(buffer) - sizeof(AudioPacketHeader));

    if (audio_sink_ptr->last_time_sample_rate != AUDIO_SAMPLE_RATE(data_packet) ||
        audio_sink_ptr->last_num_channels != AUDIO_NUM_CHANNELS(data_packet) ||
        dev_info->alloc_size < AUDIO_CHUNK_SIZE(data_packet))
    {
        dev_info->init_params_done = false;
    }

    if (!dev_info->init_params_done)
    {
        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] Init params\n");

        /* Initialize the HW codec parameters */
        audio_sink_ptr->codec_bits_per_sample = audio_sink_ptr->bits_per_sample;
        audio_sink_ptr->codec_num_channels    = audio_sink_ptr->num_channels;

        if (audiosink_default_init_params(audio_sink_ptr) != AUDIO_SINK_SUCCESS)
        {
            audio_sink_ptr->error_element = FLOW_UNEXPECTED;
            return FLOW_UNEXPECTED;
        }

        /* Allocate audio data buffers. */
        for (int i = 0; i < AUDIO_SINK_BUFFER_NUM; i++)
        {
            if (dev_info->unaligned_buf[i])
            {
                OSA_MemoryFree(dev_info->unaligned_buf[i]);
                dev_info->unaligned_buf[i] = NULL;
                dev_info->alloc_size       = 0;
            }

            if ((audio_sink_ptr->num_channels != audio_sink_ptr->codec_num_channels) ||
                (audio_sink_ptr->bits_per_sample != audio_sink_ptr->codec_bits_per_sample))
            {
                uint32_t samples =
                    audio_sink_ptr->chunk_size / audio_sink_ptr->num_channels / (audio_sink_ptr->bits_per_sample >> 3);
                samples = samples * audio_sink_ptr->codec_num_channels * (audio_sink_ptr->codec_bits_per_sample >> 3);
                dev_info->alloc_size = (samples < MIN_AUDIO_BUFFER_SIZE) ? MIN_AUDIO_BUFFER_SIZE : samples;
            }
            else
            {
                dev_info->alloc_size = (audio_sink_ptr->chunk_size < MIN_AUDIO_BUFFER_SIZE) ?
                                           MIN_AUDIO_BUFFER_SIZE :
                                           audio_sink_ptr->chunk_size;
            }

            dev_info->unaligned_buf[i] = (char *)OSA_MemoryAllocate((uint32_t)(dev_info->alloc_size + SIZE_ALIGNMENT));
            dev_info->audbuf[i]        = (char *)MEM_ALIGN(dev_info->unaligned_buf[i], SIZE_ALIGNMENT);
            if (dev_info->audbuf[i] == NULL)
            {
                STREAMER_LOG_ERR(DBG_AUDIO_SINK, ERRCODE_OUT_OF_MEMORY, "[DEFAULT Sink] Audio buffer malloc failed \n");
                return FLOW_ERROR;
            }
        }

        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] Init params done\n");

        /* now device is ready to accept data */
        /* prepare the device */
        dev_info->init_params_done = true;

        dev_info->input_size  = 0;
        dev_info->input_index = 0;
    }

    if (buffer_size < audio_sink_ptr->chunk_size)
    {
        /* Add zeros to the end of the buffer to keep the signal the same length - only the last frame is affected */
        memset(buffer_ptr + buffer_size, 0, audio_sink_ptr->chunk_size - buffer_size);
        buffer_size = audio_sink_ptr->chunk_size;
    }

    if (buffer_size > 0U)
    {
        if ((audio_sink_ptr->num_channels != audio_sink_ptr->codec_num_channels) ||
            (audio_sink_ptr->bits_per_sample != audio_sink_ptr->codec_bits_per_sample))
        {
            int8_t *input_buffer      = buffer_ptr;
            int8_t *audio_buffer_head = (int8_t *)dev_info->audbuf[dev_info->input_index];

            uint8_t input_buffer_step = audio_sink_ptr->bits_per_sample >> 3;
            uint8_t audio_buffer_step = audio_sink_ptr->codec_bits_per_sample >> 3;
            uint32_t i;
            uint8_t k;
            int j, o;

            /* Interleave the actual data with zeros  */
            for (i = 0; i < (buffer_size / input_buffer_step);)
            {
                /* Copy bytes from input buffer to audio buffer */
                for (k = 0; k < audio_sink_ptr->num_channels; k++)
                {
                    for (j = (audio_buffer_step - 1), o = (input_buffer_step - 1); j >= 0; j--, o--)
                    {
                        audio_buffer_head[j] = o >= 0 ? input_buffer[o] : 0xFF;
                    }

                    /* Update pointers */
                    input_buffer      = input_buffer + input_buffer_step;
                    audio_buffer_head = audio_buffer_head + audio_buffer_step;
                    i++;
                }

                /* If the codec need more channels than the input buffer contains, add zeros to the audio buffer */
                for (k = 0; k < (audio_sink_ptr->codec_num_channels - audio_sink_ptr->num_channels); k++)
                {
                    for (j = 0; j < audio_buffer_step; j++)
                    {
                        audio_buffer_head[j] = 0x00;
                    }
                    /* Update audio buffer head pointer */
                    audio_buffer_head = audio_buffer_head + audio_buffer_step;
                }
            }

            /* Update the buffer size to the actual value */
            buffer_size =
                audio_sink_ptr->chunk_size / audio_sink_ptr->num_channels / (audio_sink_ptr->bits_per_sample >> 3);
            buffer_size =
                buffer_size * audio_sink_ptr->codec_num_channels * (audio_sink_ptr->codec_bits_per_sample >> 3);
        }
        else
        {
            /* Copy data directly into the audio buffer. */
            memcpy(dev_info->audbuf[dev_info->input_index], buffer_ptr, buffer_size);
        }

        dev_info->input_size = buffer_size;

        /* Write to PCM output driver. */
        ret = audio_sink_ptr->appFunctions.process_func((uint8_t *)dev_info->audbuf[dev_info->input_index],
                                                        dev_info->input_size);
        if ((ret != 0) && (ret != 1))
        {
            STREAMER_LOG_ERR(DBG_AUDIO_SINK, ERRCODE_BUSY, "[DEFAULT Sink] failed to write PCM data\n");
            STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
            return FLOW_ERROR;
        }
        if ((ret == 0) && (audio_sink_ptr->refData_element))
        {
            AudioRefData_t refData = {(uint8_t *)dev_info->audbuf[dev_info->input_index], dev_info->input_size};
            if (STREAM_OK != element_set_property(audio_sink_ptr->refData_element, PROP_VOICESEEKER_REFDATA_PUSH,
                                                  (uintptr_t)&refData))
            {
                return FLOW_ERROR;
            }
        }

        /* Move to the next PCM storage buffer for the next chunk. */
        dev_info->input_size  = 0;
        dev_info->input_index = (dev_info->input_index + 1) % AUDIO_SINK_BUFFER_NUM;
    }

    audio_sink_ptr->last_time_sample_rate = AUDIO_SAMPLE_RATE(data_packet);
    audio_sink_ptr->last_num_channels     = AUDIO_NUM_CHANNELS(data_packet);
    audio_sink_ptr->last_chunk_size       = AUDIO_CHUNK_SIZE(data_packet);

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return FLOW_OK;
}

uint8_t audiosink_default_sink_pad_event_handler(StreamPad *pad, StreamEvent *event)
{
    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    switch (EVENT_TYPE(event))
    {
        case EVENT_FLUSH_START:
            STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] EVENT_FLUSH_START\n");
            /* Clear tx buffer */
            break;
        case EVENT_EOS:
            STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] EVENT_EOS\n");
            break;
        default:
            break;
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return false;
}

uint8_t audiosink_default_sink_pad_query_handler(StreamPad *pad, StreamQuery *query)
{
    uint8_t ret = true;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    switch (QUERY_TYPE(query))
    {
        case INFO_BUFF_OCC:
            STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "[DEFAULT Sink] MSG_BUFF_OCC\n");
            /* Get buffer occupancy: store in data->value32u */
            break;
        default:
            ret = false;
            break;
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return ret;
}

AudioSinkStreamErrorType audiosink_default_start_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    if (audio_sink_ptr == NULL)
    {
        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "Device info is NULL");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;

    if (!dev_info)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    /* fopen() device */
    dev_info->alloc_size = 0;

    if (audio_sink_ptr->appFunctions.open_func != NULL)
    {
        if (audio_sink_ptr->appFunctions.open_func(AUDIO_SINK_BUFFER_NUM) != 0)
        {
            STREAMER_LOG_ERR(DBG_AUDIO_SINK, ERRCODE_BUSY, "[DEFAULT Sink] open file handler failed\n");
            STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
            return AUDIO_SINK_FAILED;
        }
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_stop_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    if (audio_sink_ptr == NULL)
    {
        STREAMER_LOG_DEBUG(DBG_AUDIO_SINK, "Device info is NULL");
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    if (!dev_info)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    /* Mute the PCM output to prevent any pops. */
    if (audio_sink_ptr->appFunctions.mute_func != NULL)
    {
        audio_sink_ptr->appFunctions.mute_func(true);
    }
    if (audio_sink_ptr->appFunctions.close_func != NULL)
    {
        audio_sink_ptr->appFunctions.close_func();
    }

    for (int i = 0; i < AUDIO_SINK_BUFFER_NUM; i++)
    {
        if (dev_info->unaligned_buf[i])
        {
            OSA_MemoryFree(dev_info->unaligned_buf[i]);
            dev_info->unaligned_buf[i] = NULL;
            dev_info->alloc_size       = 0;
        }
    }

    dev_info->init_params_done = false;

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_play_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    if (!dev_info)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    /* "Unmute" the PCM output. */
    if (audio_sink_ptr->appFunctions.volume_func != NULL)
    {
        audio_sink_ptr->appFunctions.volume_func(audio_sink_ptr->volume);
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_pause_device(ElementAudioSink *audio_sink_ptr)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;
    if (!dev_info)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    /* Mute the PCM output to prevent any pops. */
    if (audio_sink_ptr->appFunctions.mute_func != NULL)
    {
        audio_sink_ptr->appFunctions.mute_func(true);
    }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

AudioSinkStreamErrorType audiosink_default_set_volume(ElementAudioSink *audio_sink_ptr, int volume)
{
    DefaultSinkDeviceInfo *dev_info;

    STREAMER_FUNC_ENTER(DBG_AUDIO_SINK);

    if (volume < 0)
    {
        audio_sink_ptr->error_element = AUDIO_SINK_FAILED;
        STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);
        return AUDIO_SINK_ERROR_INVALID_ARGS;
    }

    audio_sink_ptr->volume = volume;

    dev_info = (DefaultSinkDeviceInfo *)audio_sink_ptr->device_info;

    /* Call function only if dev_info is valid - it is invalid before initialization. */
    if (dev_info)
        /* Set volume the PCM output. */
        if (audio_sink_ptr->appFunctions.volume_func != NULL)
        {
            audio_sink_ptr->appFunctions.volume_func(volume);
        }

    STREAMER_FUNC_EXIT(DBG_AUDIO_SINK);

    return AUDIO_SINK_SUCCESS;
}

/**
 * @brief Audio sink device function pointers
 *
 */
const AudioSinkPadDeviceIOFuncType audio_sink_device_default = {
    audiosink_default_init_device,
    audiosink_default_deinit_device,
    audiosink_default_sink_pad_activation_handler,
    audiosink_default_sink_pad_chain_handler,
    audiosink_default_sink_pad_event_handler,
    audiosink_default_sink_pad_query_handler,
    NULL,
    audiosink_default_start_device,
    audiosink_default_stop_device,
    audiosink_default_play_device,
    audiosink_default_pause_device,
    audiosink_default_set_volume,
};