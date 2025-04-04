/*
The contents of this file are subject to the AROS Public License Version 1.1 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at
http://www.aros.org/license.html

Software distributed under the License is distributed on an "AS IS" basis, WITHOUT WARRANTY OF
ANY KIND, either express or implied. See the License for the specific language governing rights and
limitations under the License.

(C) Copyright 2010-2021 The AROS Dev Team
(C) Copyright 2009-2010 Stephen Jones.
(C) Copyright xxxx-2009 Davy Wentzler.

The Initial Developer of the Original Code is Davy Wentzler.

All Rights Reserved.
*/

#ifdef __AROS__
#include <aros/debug.h>
#endif

#include <config.h>

#include <devices/ahi.h>
#include <exec/memory.h>
#include <libraries/ahi_sub.h>
#include <math.h>

#include <proto/ahi_sub.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>

#include <string.h>

#include "library.h"
#include "regs.h"
#include "misc.h"
#include "pci_wrapper.h"



extern int z, timer;


/******************************************************************************
** Globals ********************************************************************
******************************************************************************/

static BOOL build_buffer_descriptor_list(struct HDAudioChip *card, ULONG nr_of_buffers, ULONG buffer_size, struct Stream *stream);
static void free_buffer_descriptor_list(struct HDAudioChip *card, ULONG nr_of_buffers, struct Stream *stream);
static BOOL stream_reset(struct Stream *stream, struct HDAudioChip *card);
void set_converter_format(struct HDAudioChip *card, UBYTE nid);


static const char *Inputs[INPUTS] =
{
  "Line in",
  "Mic 1",
  "Mic 2",
  "CD",
  "Mixer"
};


#define OUTPUTS 1


static const char *Outputs[OUTPUTS] =
{
  "Line",
};



#define uint32 unsigned int

static const ULONG nr_of_playback_buffers = 2;

/******************************************************************************
** AHIsub_AllocAudio **********************************************************
******************************************************************************/

ULONG _AHIsub_AllocAudio(struct TagItem* taglist,
                         struct AHIAudioCtrlDrv* AudioCtrl,
                         struct DriverBase* AHIsubBase)
{
    struct HDAudioBase* card_base = (struct HDAudioBase*) AHIsubBase;

    int   card_num;
    ULONG ret;
    ULONG i;

    card_num = (GetTagData(AHIDB_AudioID, 0, taglist) & 0x0000f000) >> 12;

    if (card_num >= card_base->cards_found ||
        card_base->driverdatas[card_num] == NULL)
    {
        D(bug("[HDAudio] no data for card = %ld\n", card_num));
        Req("No HDAudioChip for card %ld.", card_num);
        return AHISF_ERROR;
    }
    else
    {
        BOOL in_use;
        struct HDAudioChip *card;

        card  = card_base->driverdatas[card_num];
        AudioCtrl->ahiac_DriverData = card;

        ObtainSemaphore(&card_base->semaphore);
        in_use = (card->audioctrl != NULL);
        if (!in_use)
        {
            card->audioctrl = AudioCtrl;
        }
        ReleaseSemaphore(&card_base->semaphore);

        if (in_use)
        {
            return AHISF_ERROR;
        }

        //bug("AudioCtrl->ahiac_MixFreq = %lu\n", AudioCtrl->ahiac_MixFreq);
        if (AudioCtrl->ahiac_MixFreq < card->frequencies[0].frequency)
            AudioCtrl->ahiac_MixFreq = card->frequencies[0].frequency;

        card->selected_freq_index = 0;
        for (i = 1; i < card->nr_of_frequencies; i++)
        {
            if ((ULONG) card->frequencies[i].frequency >= AudioCtrl->ahiac_MixFreq)
            {
                if ((AudioCtrl->ahiac_MixFreq - (LONG) card->frequencies[i - 1].frequency)
                    < ((LONG) card->frequencies[i].frequency - AudioCtrl->ahiac_MixFreq))
                {
                    card->selected_freq_index = i - 1;
                    break;
                }
                else
                {
                    card->selected_freq_index = i;
                    break;
                }
            }
        }

        //bug("card->selected_freq_index = %lu\n", card->selected_freq_index);

        ret = AHISF_KNOWSTEREO | AHISF_MIXING | AHISF_TIMING;

        for (i = 0; i < card->nr_of_frequencies; i++)
        {
            if (AudioCtrl->ahiac_MixFreq == card->frequencies[i].frequency)
            {
                ret |= AHISF_CANRECORD;
                break;
            }
        }

        if (card->bitsizes[card->selected_bitsize_index] == 24)
            ret |= AHISF_KNOWHIFI;

        return ret;
    }
}



/******************************************************************************
** AHIsub_FreeAudio ***********************************************************
******************************************************************************/

void _AHIsub_FreeAudio(struct AHIAudioCtrlDrv* AudioCtrl,
                       struct DriverBase* AHIsubBase)
{
    struct HDAudioBase* card_base = (struct HDAudioBase*) AHIsubBase;
    struct HDAudioChip* card = (struct HDAudioChip*) AudioCtrl->ahiac_DriverData;

    if (card != NULL)
    {
        ObtainSemaphore(&card_base->semaphore);
        if (card->audioctrl == AudioCtrl)
        {
            // Release it if we own it.
            card->audioctrl = NULL;
        }

        ReleaseSemaphore(&card_base->semaphore);

        AudioCtrl->ahiac_DriverData = NULL;
    }
}


/******************************************************************************
** AHIsub_Disable *************************************************************
******************************************************************************/

void _AHIsub_Disable(struct AHIAudioCtrlDrv* AudioCtrl,
                     struct DriverBase* AHIsubBase)
{
    // V6 drivers do not have to preserve all registers

    Disable();
}


/******************************************************************************
** AHIsub_Enable **************************************************************
******************************************************************************/

void _AHIsub_Enable(struct AHIAudioCtrlDrv* AudioCtrl,
                    struct DriverBase* AHIsubBase)
{
    // V6 drivers do not have to preserve all registers

    Enable();
}


/******************************************************************************
** AHIsub_Start ***************************************************************
******************************************************************************/

ULONG _AHIsub_Start(ULONG flags,
                    struct AHIAudioCtrlDrv* AudioCtrl,
                    struct DriverBase* AHIsubBase)
{
    struct HDAudioChip* card = (struct HDAudioChip*) AudioCtrl->ahiac_DriverData;
    ULONG dma_buffer_size = 0;
    struct Stream *input_stream = &(card->streams[0]);
    struct Stream *output_stream = &(card->streams[card->nr_of_input_streams]);

    if (flags & AHISF_PLAY)
    {
        ULONG dma_sample_frame_size = 2; /* 16-bit */;
        APTR    bufftmp;

        detect_headphone_change(card);

        bufftmp = AllocVec(AudioCtrl->ahiac_BuffSize, MEMF_PUBLIC | MEMF_CLEAR);
        if (bufftmp == NULL)
        {
            D(bug("[HDAudio] Unable to allocate %ld bytes for mixing buffer.", AudioCtrl->ahiac_BuffSize));
            return AHIE_NOMEM;
        }
#if defined(__AROS__) && (__WORDSIZE==64)
        card->lower_mix_buffer = (IPTR)bufftmp & 0xFFFFFFFF;
        card->upper_mix_buffer = ((IPTR)bufftmp >> 32) & 0xFFFFFFFF;
#else
        card->lower_mix_buffer = (ULONG)bufftmp;
        card->upper_mix_buffer = 0;
#endif

        /* Allocate a buffer large enough for 16-bit or 32-bit double-buffered playback (mono or stereo) */
        if (card->bitsizes[card->selected_bitsize_index] == 24)
            dma_sample_frame_size = 4; /* 32-bit */

        if (AudioCtrl->ahiac_Flags & AHIACF_STEREO)
            dma_sample_frame_size *= 2; /* STEREO */

        dma_buffer_size = AudioCtrl->ahiac_MaxBuffSamples * dma_sample_frame_size;


        D(bug("dma_sample_frame_size = %ld, dma_buffer_size = %ld, freq = %d\n",
            dma_sample_frame_size, dma_buffer_size, AudioCtrl->ahiac_MixFreq));
        build_buffer_descriptor_list(card, nr_of_playback_buffers, dma_buffer_size, output_stream);

        card->playback_buffer1 = output_stream->bdl[0].address;
        card->playback_buffer2 = output_stream->bdl[1].address;

        //bug("BDLE[0] = %lx, BDLE[1] = %lx\n", output_stream->bdl[0].lower_address, output_stream->bdl[1].lower_address);
        if (stream_reset(output_stream, card) == FALSE)
        {
            return AHISF_ERROR;
        }

        // 4.5.3 Starting Streams
        outb_setbits((output_stream->tag << 4), output_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL + 2, card); // set stream number
        pci_outl(dma_buffer_size * nr_of_playback_buffers, output_stream->sd_reg_offset + HD_SD_OFFSET_CYCLIC_BUFFER_LEN, card);
        pci_outw(1, output_stream->sd_reg_offset + HD_SD_OFFSET_LAST_VALID_INDEX, card); // 2 buffers, last valid index = 1

        pci_outw(get_hda_format(card), output_stream->sd_reg_offset + HD_SD_OFFSET_FORMAT, card);

        send_command_4(card->codecnr, card->dac_nid, VERB_SET_CONVERTER_FORMAT, get_hda_format(card), card);

        // set BDL for scatter/gather
#if defined(__AROS__) && (__WORDSIZE==64)
        pci_outl((ULONG)((IPTR)output_stream->bdl & 0xFFFFFFFF), output_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_LOW, card);
        pci_outl((ULONG)(((IPTR)output_stream->bdl >> 32) & 0xFFFFFFFF), output_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_HIGH, card);
#else
        pci_outl((ULONG) output_stream->bdl, output_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_LOW, card);
        pci_outl(0, output_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_HIGH, card);
#endif

        // set stream ID and channel for DAC
        send_command_12(card->codecnr, card->dac_nid, VERB_SET_CONVERTER_STREAM_CHANNEL, (output_stream->tag << 4), card); // stream 1, channel 0

        card->current_bytesize = dma_buffer_size;
        card->current_frames = AudioCtrl->ahiac_MaxBuffSamples;
        card->current_buffer   = card->playback_buffer1;
        card->flip = 1;
        card->is_playing = TRUE;
    }

    if (flags & AHISF_RECORD)
    {
        dma_buffer_size = RECORD_BUFFER_SAMPLES * 4;

        build_buffer_descriptor_list(card, 2, dma_buffer_size, input_stream);

        card->record_buffer1 = input_stream->bdl[0].address;
        card->record_buffer2 = input_stream->bdl[1].address;

        if (stream_reset(input_stream, card) == FALSE)
        {
            return AHISF_ERROR;
        }

        // 4.5.3 Starting Streams
        outb_setbits((input_stream->tag << 4), input_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL + 2, card); // set stream number
        pci_outl(dma_buffer_size * 2, input_stream->sd_reg_offset + HD_SD_OFFSET_CYCLIC_BUFFER_LEN, card);
        pci_outw(1, input_stream->sd_reg_offset + HD_SD_OFFSET_LAST_VALID_INDEX, card); // 2 buffers, last valid index = 1

        // set sample rate and format
        pci_outw(((card->frequencies[card->selected_freq_index].base44100 > 0) ? BASE44 : 0) | // base freq: 48 or 44.1 kHz
            (card->frequencies[card->selected_freq_index].mult << 11) | // multiplier
            (card->frequencies[card->selected_freq_index].div << 8) | // divisor
            FORMAT_16BITS | // set to 16-bit for now
            FORMAT_STEREO,
        input_stream->sd_reg_offset + HD_SD_OFFSET_FORMAT, card);

        send_command_4(card->codecnr, card->adc_nid, VERB_SET_CONVERTER_FORMAT,
            ((card->frequencies[card->selected_freq_index].base44100 > 0) ? BASE44 : 0) | // base freq: 48 or 44.1 kHz
            (card->frequencies[card->selected_freq_index].mult << 11) | // multiplier
            (card->frequencies[card->selected_freq_index].div << 8) | // divisor
            FORMAT_16BITS | // set to 16-bit for now
            FORMAT_STEREO , card); // stereo

        // set BDL for scatter/gather
#if defined(__AROS__) && (__WORDSIZE==64)
        pci_outl((ULONG)((IPTR)input_stream->bdl & 0xFFFFFFFF), input_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_LOW, card);
        pci_outl((ULONG)(((IPTR)input_stream->bdl >> 32) & 0xFFFFFFFF), input_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_HIGH, card);
#else
        pci_outl((ULONG) input_stream->bdl, input_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_LOW, card);
        pci_outl(0, input_stream->sd_reg_offset + HD_SD_OFFSET_BDL_ADDR_HIGH, card);
#endif

        // set stream ID and channel for ADC
        send_command_12(card->codecnr, card->adc_nid, VERB_SET_CONVERTER_STREAM_CHANNEL, (input_stream->tag << 4), card);

        D(bug("[HDAudio] RECORD\n"));

        card->current_record_bytesize = dma_buffer_size;
        card->current_record_buffer = card->record_buffer1;
        card->recflip = 1;
        card->is_recording = TRUE;
    }

    if (flags & AHISF_PLAY)
    {
        // enable irq's
        z = 0;
        timer = 0; // for demo/test
        
        //bug("GOING TO PLAY\n");
        outl_setbits((1 << output_stream->index), HD_INTCTL, card);
        outl_setbits(HD_SD_CONTROL_STREAM_RUN | HD_SD_STATUS_MASK, output_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // Stream Run
    }


    if (flags & AHISF_RECORD)
    {
        // enable irq's
        outl_setbits((1 << input_stream->index), HD_INTCTL, card);
        outl_setbits(HD_SD_CONTROL_STREAM_RUN | HD_SD_STATUS_MASK, input_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // Stream Run

    }

    return AHIE_OK;
}


/******************************************************************************
** AHIsub_Update **************************************************************
******************************************************************************/

void _AHIsub_Update(ULONG flags,
                    struct AHIAudioCtrlDrv* AudioCtrl,
                    struct DriverBase* AHIsubBase)
{
    struct HDAudioChip* card = (struct HDAudioChip*) AudioCtrl->ahiac_DriverData;

    card->current_frames = AudioCtrl->ahiac_BuffSamples;

    if (AudioCtrl->ahiac_Flags & AHIACF_STEREO)
    {
        card->current_bytesize = card->current_frames * 4;
    }
    else
    {
        card->current_bytesize = card->current_frames * 2;
    }
}


/******************************************************************************
** AHIsub_Stop ****************************************************************
******************************************************************************/

void _AHIsub_Stop(ULONG flags,
                  struct AHIAudioCtrlDrv* AudioCtrl,
                  struct DriverBase* AHIsubBase)
{
    struct HDAudioChip* card = (struct HDAudioChip*) AudioCtrl->ahiac_DriverData;

    //bug("Stop\n");

    if ((flags & AHISF_PLAY) && card->is_playing)
    {
        struct Stream *output_stream = &(card->streams[card->nr_of_input_streams]);
        outl_clearbits(HD_SD_CONTROL_STREAM_RUN, output_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card);
        card->is_playing = FALSE;

        card->current_bytesize = 0;
        card->current_frames = 0;
        card->current_buffer = 0;

        if (card->mix_buffer)
        {
            FreeVec((APTR)(IPTR)card->mix_buffer);
        }
        card->mix_buffer = 0;

        free_buffer_descriptor_list(card, nr_of_playback_buffers, output_stream);

        D(bug("[HDAudio] IRQ's received was %d\n", z));
    }

    if ((flags & AHISF_RECORD) && card->is_recording)
    {
        struct Stream *input_stream = &(card->streams[0]);
        outl_clearbits(HD_SD_CONTROL_STREAM_RUN, input_stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card);

        card->record_buffer1 = 0;
        card->record_buffer2 = 0;
        card->current_record_bytesize = 0;

        card->is_recording = FALSE;

        free_buffer_descriptor_list(card, 2, input_stream);
    }

    card->current_bytesize = 0;
}


/******************************************************************************
** AHIsub_GetAttr *************************************************************
******************************************************************************/

IPTR _AHIsub_GetAttr(ULONG attribute,
		             LONG argument,
		             IPTR def,
		             struct TagItem* taglist,
		             struct AHIAudioCtrlDrv* AudioCtrl,
		             struct DriverBase* AHIsubBase)
{
    struct HDAudioBase* card_base = (struct HDAudioBase*) AHIsubBase;
    struct HDAudioChip* card = card_base->driverdatas[0];
    ULONG i;

    switch(attribute)
    {
        case AHIDB_Bits:
          return 16;

        case AHIDB_Frequencies:
        {
          return card->nr_of_frequencies;
        }

        case AHIDB_Frequency: // Index->Frequency
          return (LONG) card->frequencies[argument].frequency;

        case AHIDB_Index: // Frequency->Index
        {
          if (argument <= (LONG) card->frequencies[0].frequency)
          {
            return 0;
          }

          if (argument >= (LONG) card->frequencies[card->nr_of_frequencies - 1].frequency)
          {
            return card->nr_of_frequencies - 1;
          }

          for (i = 1; i < card->nr_of_frequencies; i++)
          {
            if ((LONG) card->frequencies[i].frequency > argument)
            {
              if ((argument - (LONG) card->frequencies[i - 1].frequency) < ((LONG) card->frequencies[i].frequency - argument))
              {
                return (IPTR)i-1;
              }
              else
              {
                return (IPTR)i;
              }
            }
          }

          return 0;  // Will not happen
        }

        case AHIDB_Author:
          return (IPTR) "Davy Wentzler";

        case AHIDB_Copyright:
          return (IPTR) "(C) 2010 Stephen Jones, (C) 2010-2017 The AROS Dev Team";

        case AHIDB_Version:
          return (IPTR) LibIDString;

        case AHIDB_Annotation:
          return (IPTR) "HD Audio driver";

        case AHIDB_Record:
          return TRUE;

        case AHIDB_FullDuplex:
          return TRUE;

        case AHIDB_Realtime:
          return TRUE;

        case AHIDB_MaxRecordSamples:
          return RECORD_BUFFER_SAMPLES;

        /* formula's:
        #include <math.h>

        unsigned long res = (unsigned long) (0x10000 * pow (10.0, dB / 20.0));
        double dB = 20.0 * log10(0xVALUE / 65536.0);

        printf("dB = %f, res = %lx\n", dB, res);*/

        case AHIDB_MinMonitorVolume:
          return (unsigned long) (0x10000 * pow (10.0, -34.5 / 20.0)); // -34.5 dB

        case AHIDB_MaxMonitorVolume:
          return (unsigned long) (0x10000 * pow (10.0, 12.0 / 20.0)); // 12 dB

        case AHIDB_MinInputGain:
          return (unsigned long) (0x10000 * pow (10.0, card->adc_min_gain / 20.0));

        case AHIDB_MaxInputGain:
          return (unsigned long) (0x10000 * pow (10.0, card->adc_max_gain / 20.0));

        case AHIDB_MinOutputVolume:
          return  (unsigned long) (0x10000 * pow (10.0, card->dac_min_gain / 20.0));

        case AHIDB_MaxOutputVolume:
          return  (unsigned long) (0x10000 * pow (10.0, card->dac_max_gain / 20.0));

        case AHIDB_Inputs:
          return INPUTS;

        case AHIDB_Input:
          return (IPTR) Inputs[argument];

        case AHIDB_Outputs:
          return OUTPUTS;

        case AHIDB_Output:
          return (IPTR) Outputs[argument];

        default:
          return def;
    }
}


/******************************************************************************
** AHIsub_HardwareControl *****************************************************
******************************************************************************/

ULONG _AHIsub_HardwareControl(ULONG attribute,
                              LONG argument,
                              struct AHIAudioCtrlDrv* AudioCtrl,
                              struct DriverBase* AHIsubBase)
{
    struct HDAudioBase* card_base = (struct HDAudioBase*) AHIsubBase;
    struct HDAudioChip* card = card_base->driverdatas[0];

    switch(attribute)
    {
        case AHIC_MonitorVolume:
        {
            double dB = 20.0 * log10(argument / 65536.0);

            card->monitor_volume = argument;
            set_monitor_volumes(card, dB);

            return TRUE;
        }

        case AHIC_MonitorVolume_Query:
        {
            return card->monitor_volume;
        }

        case AHIC_InputGain:
        {
            double dB = 20.0 * log10(argument / 65536.0);
            card->input_gain = argument;

            set_adc_gain(card, dB);
            return TRUE;
        }

        case AHIC_InputGain_Query:
            return card->input_gain;

        case AHIC_OutputVolume:
        {
            double dB = 20.0 * log10(argument / 65536.0);
            card->output_volume = argument;

            set_dac_gain(card, dB);

            return TRUE;
        }

        case AHIC_OutputVolume_Query:
            return card->output_volume;

        case AHIC_Input:
            card->input = argument;
            set_adc_input(card);

          return TRUE;

        case AHIC_Input_Query:
            return card->input;

        case AHIC_Output:
            card->output = argument;

            return TRUE;

        case AHIC_Output_Query:
            return card->output;

        default:
            return FALSE;
    }
}


static BOOL build_buffer_descriptor_list(struct HDAudioChip *card, ULONG nr_of_buffers, ULONG buffer_size, struct Stream *stream)
{
    unsigned int entry;

    stream->bdl = pci_alloc_consistent(nr_of_buffers * sizeof(struct BDLE), &(stream->bdl_nonaligned), 128);
    stream->bdl_nonaligned_addresses = (APTR) AllocVec(nr_of_buffers * 8, MEMF_PUBLIC | MEMF_CLEAR);

    for (entry = 0; entry < nr_of_buffers; entry++)
    {
        APTR non_aligned_address = 0;
        APTR buffer;

        buffer = pci_alloc_consistent(buffer_size, &non_aligned_address, 128);

#if defined(__AROS__) && (__WORDSIZE==64)
        stream->bdl[entry].lower_address = (ULONG)((IPTR)buffer & 0xFFFFFFFF);
        stream->bdl[entry].upper_address = (ULONG)(((IPTR)buffer >> 32) & 0xFFFFFFFF);
#else
        stream->bdl[entry].lower_address = (ULONG)buffer;
        stream->bdl[entry].upper_address = 0;
#endif

        stream->bdl[entry].length = buffer_size;
        stream->bdl[entry].reserved_ioc = 1;

        stream->bdl_nonaligned_addresses[entry] = non_aligned_address;

        //bug("BDL %d = %lx\n", entry, stream->bdl[entry].lower_address);
    }

    return TRUE;
}


static void free_buffer_descriptor_list(struct HDAudioChip *card, ULONG nr_of_buffers, struct Stream *stream)
{
    unsigned int entry;

    for (entry = 0; entry < nr_of_buffers; entry++)
    {
        pci_free_consistent(stream->bdl_nonaligned_addresses[entry]);
        stream->bdl[entry].lower_address = 0;
        stream->bdl_nonaligned_addresses[entry] = NULL;
    }

    pci_free_consistent(stream->bdl_nonaligned);
    stream->bdl_nonaligned = NULL;

    FreeVec(stream->bdl_nonaligned_addresses);
    stream->bdl_nonaligned_addresses = NULL;
}


static BOOL stream_reset(struct Stream *stream, struct HDAudioChip *card)
{
    int i;
    UBYTE control;

    outb_clearbits(HD_SD_CONTROL_STREAM_RUN, stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // stop stream run

    outb_setbits(0x1C, stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card);
    outb_setbits(0x1C, stream->sd_reg_offset + HD_SD_OFFSET_STATUS, card);
    outb_setbits(0x1, stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // stream reset

    for (i = 0; i < 1000; i++)
    {
        if ((pci_inb(stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card) & 0x1) == 0x1)
        {
            break;
        }

        udelay(100);
    }

    if (i == 1000)
    {
        D(bug("[HDAudio] Stream reset not ok\n"));
        return FALSE;
    }

    outb_clearbits(0x1, stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // stream reset
    udelay(10);
    for (i = 0; i < 1000; i++)
    {
        if ((pci_inb(stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card) & 0x1) == 0x0)
        {
            break;
        }

        udelay(100);
    }

    if (i == 1000)
    {
       D(bug("[HDAudio] Stream reset 2 not ok\n"));
       return FALSE;
    }

    outb_clearbits(HD_SD_CONTROL_STREAM_RUN, stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card); // stop stream run

    control = pci_inb(stream->sd_reg_offset + HD_SD_OFFSET_CONTROL, card);
    if ((control & 0x1) == 1)
    {
        return FALSE;
    }

    return TRUE;
}


void set_converter_format(struct HDAudioChip *card, UBYTE nid)
{
    send_command_4(card->codecnr, nid, VERB_SET_CONVERTER_FORMAT, (1 << 14) | (0x3 << 4) | 1, card); // 44.1kHz 24-bits stereo
}

