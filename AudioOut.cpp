/*--------------------------------------------------------------------------------*
  Copyright Nintendo.  All rights reserved.

  These coded instructions, statements, and computer programs contain proprietary
  information of Nintendo and/or its licensed developers and are protected by
  national and international copyright laws. They may not be disclosed to third
  parties or copied or duplicated in any form, in whole or in part, without the
  prior written consent of Nintendo.

  The content herein is highly confidential and should be handled accordingly.
 *--------------------------------------------------------------------------------*/

/**
* @examplesource{AudioOut/AudioOut.cpp,PageSampleAudioAudioOut}
*
* @brief
*  A sample program of audio playback.
*/

/**
* @page PageSampleAudioAudioOut  Audio Playback Sample
* @tableofcontents
*
* @brief
*  This sample program shows how to use audio playback.
*
* @section PageSampleAudioAudioOut_SectionBrief  Overview
*  This sample uses audio output and performs audio playback.
*
* @section PageSampleAudioAudioOut_SectionFileStructure  File Structure
*  This sample program is located in @link Samples/Sources/Applications/AudioOut Samples/Sources/Applications/AudioOut/ @endlink .
*
*
* @section PageSampleAudioAudioOut_SectionNecessaryEnvironment  Required Environment
*  You must be able to use audio output.
*
* @section PageSampleAudioAudioOut_SectionHowToOperate  How to Use
*  Run the sample program to play square waves.
*  You can operate the program using input from a keyboard or from <tt>DebugPad</tt>.
*
*  <p>
*  <table>
*  <tr><th> Input </th><th> Operation </th></tr>
*  <tr><td> START or SPACEBAR   </td><td> Exits the program. </td></tr>
*  <tr><td> L/R Buttons          </td><td> Changes the volume for audio output. </td></tr>
*  </table>
*  </p>
*
* @section PageSampleAudioAudioOut_SectionPrecaution  Important Information
*  Nothing specific.
*
* @section PageSampleAudioAudioOut_SectionHowToExecute  Execution Procedure
*  Build the sample program and then run it.
*
* @section PageSampleAudioAudioOut_SectionDetail  Description
*  This sample program dynamically generates a square wave and outputs it to audio output.
*
*  This sample program has the following flow.
*
*  - List the usable audio outputs.
*  - Open the default audio output.
*  - Get and verify audio output information.
*  - Prepare and register the playback buffer.
*  - Start playback.
*  - Play the audio.
*  - Changes the volume for audio output.
*  - End playback.
*  - Close audio output.
*  - Free the playback buffer.
*
*  First, list audio outputs.
*  If multiple audio outputs can be used on the system,
*  <tt>nn::audio::OpenAudioOut()</tt> can be called by specifying a name.
*  Different audio outputs can also be used at the same time.
*  This sample only performs the listing, opening, and closing processes for names,
*  and the actual audio playback uses the default audio output.
*
*  Two buffers for playback are prepared, each with a length of 50 milliseconds.
*  The developer can freely determine the number and length of buffers, but if the length is too short or if the number is too small, problems such as sound cutting out can occur.
*  The developer can freely determine the number and length of buffers, but if the length is too short or if the number is too small, problems such as sound cutting out can occur.
*  The prepared buffer is registered using the <tt>nn::audio::AppendAudioOutBuffer()</tt> function.
*  Registration can be performed before or after calling <tt>nn::audio::StartAudioOut()</tt>.
*  Actual playback is not performed until <tt>nn::audio::StartAudioOut()</tt> is called.
*
*  After playback starts, information in the buffer that completed playback is obtained with <tt>nn::audio::GetReleasedAudioOutBuffer()</tt>
*  and the dynamically generated square waveform data is copied to the playback buffer, and that data is registered again.
*  If downmixing or sample rate conversion is required, it is performed at this time.
*  Audio playback is realized by repeating these processes.
*
*  When the request to change the volume is made, the audio output volume is changed by the <tt>nn::audio::SetAudioOutVolume()</tt> function.
*
*  When the request to exit the program is made, the program cancels this repetitive processing,
*  stops playback, closes the audio output, and frees the memory.
*  Note that when <tt>nn::audio::StopAudioOut()</tt> is called, the playback stops and access to registered (and not played)
*  buffers is revoked, but access occurs when <tt>nn::audio::StartAudioOut()</tt> is run.
*  There is no access after <tt>nn::audio::CloseAudioOut()</tt>.
*/

#include <algorithm>
#include <cstdlib>

#include <nn/nn_Abort.h>
#include <nn/nn_Assert.h>
#if defined(USE_NNS_LOG)
#include <nns/nns_Log.h>
#else // USE_NNS_LOG
#include <nn/nn_Log.h>
#define NNS_LOG(...) NN_LOG(__VA_ARGS__)
#endif // USE_NNS_LOG
#include <nn/audio.h>
#include <nn/os.h>
#include <nn/mem.h>
#include <nn/fs.h>
#include <nn/nn_TimeSpan.h>

#include <nns/audio/audio_HidUtilities.h>
#include <nn/hid.h>
#if defined(USE_KEYBOARD)
#include <nn/hid/hid_KeyboardKey.h>
#endif // USE_KEYBOARD
#if defined(USE_NPAD)
#include <nn/hid/hid_Npad.h>
#endif // USE_NPAD
#include <nn/settings/settings_DebugPad.h>

//Custom Includes
#include "SwitchVoiceChatDecodeNativeCode.h"
#include "SwitchVoiceChatNativeCode.h"

namespace
{

char g_HeapBuffer[256 * 1024];
const char Title[] = "AudioOut";

void InitializeHidDevices()
{
#if defined(USE_NPAD)
    nn::hid::InitializeDebugPad();
    nn::hid::InitializeNpad();
    const nn::hid::NpadIdType npadIds[2] = { nn::hid::NpadId::No1, nn::hid::NpadId::Handheld };
    nn::hid::SetSupportedNpadStyleSet(nn::hid::NpadStyleFullKey::Mask | nn::hid::NpadStyleHandheld::Mask);
    nn::hid::SetSupportedNpadIdType(npadIds, sizeof(npadIds) / sizeof(npadIds[0]));
#endif // USE_NPAD

    //Map keyboard keys to DebugPad buttons.
#if defined(USE_KEYBOARD)
    nn::settings::DebugPadKeyboardMap map;
    nn::settings::GetDebugPadKeyboardMap(&map);
    map.buttonA     = nn::hid::KeyboardKey::A::Index;
    map.buttonB     = nn::hid::KeyboardKey::B::Index;
    map.buttonX     = nn::hid::KeyboardKey::X::Index;
    map.buttonY     = nn::hid::KeyboardKey::Y::Index;
    map.buttonL     = nn::hid::KeyboardKey::L::Index;
    map.buttonR     = nn::hid::KeyboardKey::R::Index;
    map.buttonZL    = nn::hid::KeyboardKey::U::Index;
    map.buttonZR    = nn::hid::KeyboardKey::V::Index;
    map.buttonLeft  = nn::hid::KeyboardKey::LeftArrow::Index;
    map.buttonRight = nn::hid::KeyboardKey::RightArrow::Index;
    map.buttonUp    = nn::hid::KeyboardKey::UpArrow::Index;
    map.buttonDown  = nn::hid::KeyboardKey::DownArrow::Index;
    map.buttonStart = nn::hid::KeyboardKey::Space::Index;
    nn::settings::SetDebugPadKeyboardMap(map);
#endif // USE_KEYBOARD
}

void PrintUsage()
{
    NNS_LOG("--------------------------------------------------------\n");
    NNS_LOG("%s Sample\n", Title);
    NNS_LOG("--------------------------------------------------------\n");
    NNS_LOG("[Start/Space]   Shut down sample program\n");
    NNS_LOG("[L/R]           Change volume\n");
    NNS_LOG("-------------------------------------------------------\n");
}

//
// This function returns the state name.
//
const char* GetAudioOutStateName(nn::audio::AudioOutState state)
{
    switch (state)
    {
        case nn::audio::AudioOutState_Started:
            return "Started";
        case nn::audio::AudioOutState_Stopped:
            return "Stopped";
        default:
            NN_UNEXPECTED_DEFAULT;
    }
}

//
// This function returns the sample format name.
//
const char* GetSampleFormatName(nn::audio::SampleFormat format)
{
    switch (format)
    {
        case nn::audio::SampleFormat_Invalid:
            return "Invalid";
        case nn::audio::SampleFormat_PcmInt8:
            return "PcmInt8";
        case nn::audio::SampleFormat_PcmInt16:
            return "PcmInt16";
        case nn::audio::SampleFormat_PcmInt24:
            return "PcmInt24";
        case nn::audio::SampleFormat_PcmInt32:
            return "PcmInt32";
        case nn::audio::SampleFormat_PcmFloat:
            return "PcmFloat";
        default:
            NN_UNEXPECTED_DEFAULT;
    }
}

//
// The square waveform generating function that supports nn::audio::SampleFormat_PcmInt8. Not yet implemented.
//
void GenerateSquareWaveInt8(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    NN_UNUSED(buffer);
    NN_UNUSED(channelCount);
    NN_UNUSED(sampleRate);
    NN_UNUSED(sampleCount);
    NN_UNUSED(amplitude);
    NN_ABORT("Not implemented yet\n");
}

//
// The square waveform generating function that supports nn::audio::SampleFormat_PcmInt16.
//
void GenerateSquareWaveInt16(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    static int s_TotalSampleCount[6] = { 0 };
    const int frequencies[6] = { 415, 698, 554, 104, 349, 277 };

    int16_t* buf = reinterpret_cast<int16_t*>(buffer);
    for (int ch = 0; ch < channelCount; ch++)
    {
        int waveLength = sampleRate / frequencies[ch]; // Length of the waveform for one period (in sample count units).

        for (int sample = 0; sample < sampleCount; sample++)
        {
            int16_t value = static_cast<int16_t>(s_TotalSampleCount[ch] < (waveLength / 2) ? amplitude : -amplitude);
            buf[sample*channelCount + ch] = value;
            s_TotalSampleCount[ch]++;
            if (s_TotalSampleCount[ch] == waveLength)
            {
                s_TotalSampleCount[ch] = 0;
            }
        }
    }
}

//
// The square waveform generating function that supports nn::audio::SampleFormat_PcmInt24. Not yet implemented.
//
void GenerateSquareWaveInt24(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    NN_UNUSED(buffer);
    NN_UNUSED(channelCount);
    NN_UNUSED(sampleRate);
    NN_UNUSED(sampleCount);
    NN_UNUSED(amplitude);
    NN_ABORT("Not implemented yet\n");
}

//
// The square waveform generating function that supports nn::audio::SampleFormat_PcmInt32. Not yet implemented.
//
void GenerateSquareWaveInt32(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    NN_UNUSED(buffer);
    NN_UNUSED(channelCount);
    NN_UNUSED(sampleRate);
    NN_UNUSED(sampleCount);
    NN_UNUSED(amplitude);
    NN_ABORT("Not implemented yet\n");
}

//
// The square waveform generating function that supports nn::audio::SampleFormat_PcmFloat. Not yet implemented.
//
void GenerateSquareWaveFloat(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    NN_UNUSED(buffer);
    NN_UNUSED(channelCount);
    NN_UNUSED(sampleRate);
    NN_UNUSED(sampleCount);
    NN_UNUSED(amplitude);
    NN_ABORT("Not implemented yet\n");
}

//
// Returns the square waveform generating function supported by the sample format.
//
typedef void (*GenerateSquareWaveFunction)(void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude);
GenerateSquareWaveFunction GetGenerateSquareWaveFunction(nn::audio::SampleFormat format)
{
    switch (format)
    {
        case nn::audio::SampleFormat_PcmInt8:
            return GenerateSquareWaveInt8;
        case nn::audio::SampleFormat_PcmInt16:
            return GenerateSquareWaveInt16;
        case nn::audio::SampleFormat_PcmInt24:
            return GenerateSquareWaveInt24;
        case nn::audio::SampleFormat_PcmInt32:
            return GenerateSquareWaveInt32;
        case nn::audio::SampleFormat_PcmFloat:
            return GenerateSquareWaveFloat;
        default:
            NN_UNEXPECTED_DEFAULT;
    }
}

//
// Function to create a square waveform.
//
void GenerateSquareWave(nn::audio::SampleFormat format, void* buffer, int channelCount, int sampleRate, int sampleCount, int amplitude)
{
    NN_ASSERT_NOT_NULL(buffer);
    GenerateSquareWaveFunction func = GetGenerateSquareWaveFunction(format);
    if (func)
    {
        func(buffer, channelCount, sampleRate, sampleCount, amplitude);
    }
}

void* Allocate(size_t size)
{
    return std::malloc(size);
}

void Deallocate(void* p, size_t size)
{
    NN_UNUSED(size);
    std::free(p);
}

}  // Anonymous namespace.

//Custom Code Region

namespace encodingAndDecoiding 
{
    intptr_t* handler = new intptr_t(0);
    unsigned char* lptm = new unsigned char(0);
    unsigned char** bufferOut = &lptm;
    int bufferOutSize = 0;
    int* count = new int(0);
    float* ptqtp = new float(0);
    float** audioOut = &ptqtp;
    int* outSampleCount = new int(0);
    unsigned int* sampleRateOut = new unsigned int(0);
}

void EncoderAndDecoderInitialization() 
{
    NN_LOG("Let's try...\n");
    if (!SwitchVoiceChatNativeCode::wntgd_StartRecordVoice())
    {
        NNS_LOG("FAILED TO INITIALIZE MICROPHONE\n");
        return;
    }
    if (!SwitchVoiceChatDecodeNativeCode::wntgd_InitializeDecoder())
    {
        NNS_LOG("Decoder Initialization FAILED!\n");
        return;
    }
}

void EncodeAndDecode(nn::os::SystemEvent *systemEvent, nn::audio::AudioOut *audioOutOut, nn::audio::AudioOutBuffer* audioOutBuffer)
{
    using namespace encodingAndDecoiding;
    bool successOnce = false;
    while (successOnce)
    {
        if (SwitchVoiceChatNativeCode::wntgd_GetVoiceBuffer(handler, bufferOut, count))
        {
            NN_LOG("Habemus ENCODED AUDIO! %i\n", *count);
            if (SwitchVoiceChatDecodeNativeCode::wntgd_DecompressVoiceData(handler, bufferOut[0], *count, audioOut, outSampleCount, sampleRateOut)); 
            {
                successOnce = true;
                NN_LOG("Habemus DECODED AUDIO!\n");
            }
        }
    }
}

//Custom Code Region End
//
// The main function.
//
extern "C" void nnMain()
{
    nn::mem::StandardAllocator allocator(g_HeapBuffer, sizeof(g_HeapBuffer));

    nn::audio::AudioOut audioOut;

    nn::fs::SetAllocator(Allocate, Deallocate);
    InitializeHidDevices();
    int timeout = 0; // Parameter, for which the default is 0.
    char** argvs = nn::os::GetHostArgv();
    for(int i = 0; i < nn::os::GetHostArgc(); ++i)
    {
        if(strcmp("timeout", argvs[i]) == 0)
        {
            if(i < nn::os::GetHostArgc())
                timeout = atoi(argvs[i + 1]);
        }
    }

    nn::TimeSpan endTime = nn::os::GetSystemTick().ToTimeSpan() + nn::TimeSpan::FromSeconds(timeout);

    EncoderAndDecoderInitialization();

    // Get a list of available audio outputs.
    {
        NNS_LOG("Available AudioOuts:\n");
        nn::audio::AudioOutInfo audioOutInfos[nn::audio::AudioOutCountMax];
        const int count = nn::audio::ListAudioOuts(audioOutInfos, sizeof(audioOutInfos) / sizeof(*audioOutInfos));
        for (int i = 0; i < count; ++i)
        {
            NNS_LOG("  %2d: %s\n", i, audioOutInfos[i].name);

            // Verify that open and close can be performed.
            nn::audio::AudioOutParameter parameter;
            nn::audio::InitializeAudioOutParameter(&parameter);
            NN_ABORT_UNLESS(
                nn::audio::OpenAudioOut(&audioOut, audioOutInfos[i].name, parameter).IsSuccess(),
                "Failed to open AudioOut."
            );
            nn::audio::CloseAudioOut(&audioOut);
        }
        NNS_LOG("\n");
    }

    // Open audio output with the specified sampling rate and number of channels.
    // If the specified sampling rate is not supported and the process fails, the default sampling rate is used.
    nn::os::SystemEvent systemEvent;
    nn::audio::AudioOutParameter parameter;
    nn::audio::InitializeAudioOutParameter(&parameter);
    parameter.sampleRate = 48000;
    parameter.channelCount = 1;
    // parameter.channelCount = 6;  // For 5.1ch output, specify 6 for the number of channels.
    if (nn::audio::OpenDefaultAudioOut(&audioOut, &systemEvent, parameter).IsFailure())
    {
        parameter.sampleRate = 0;
        parameter.channelCount = 0;
        NN_ABORT_UNLESS(
            nn::audio::OpenDefaultAudioOut(&audioOut, &systemEvent, parameter).IsSuccess(),
            "Failed to open AudioOut."
        );
    }

    NNS_LOG("AudioOut is opened\n  State: %s\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&audioOut)));

    // Get the audio output properties.
    int channelCount = nn::audio::GetAudioOutChannelCount(&audioOut);
    int sampleRate = nn::audio::GetAudioOutSampleRate(&audioOut);
    nn::audio::SampleFormat sampleFormat = nn::audio::GetAudioOutSampleFormat(&audioOut);
    NNS_LOG("  Name: %s\n", nn::audio::GetAudioOutName(&audioOut));
    NNS_LOG("  ChannelCount: %d\n", channelCount);
    NNS_LOG("  SampleRate: %d\n", sampleRate);
    NNS_LOG("  SampleFormat: %s\n", GetSampleFormatName(sampleFormat));
    // This sample assumes that the sample format is 16-bit.
    NN_ASSERT(sampleFormat == nn::audio::SampleFormat_PcmInt16);

    // Prepare parameters for the buffer.
    const int frameRate = 20;                             // 20 fps
    const int frameSampleCount = sampleRate / frameRate;  // 50 milliseconds (in samples)
    const size_t dataSize = frameSampleCount * channelCount * nn::audio::GetSampleByteSize(sampleFormat);
    const size_t bufferSize = nn::util::align_up(dataSize, nn::audio::AudioOutBuffer::SizeGranularity);
    const int bufferCount = 4;
    const int amplitude = std::numeric_limits<int16_t>::max() / 16;

    nn::audio::AudioOutBuffer audioOutBuffer[bufferCount];
    void* outBuffer[bufferCount];
    for (int i = 0; i < bufferCount; ++i)
    {
        outBuffer[i] = allocator.Allocate(bufferSize, nn::audio::AudioOutBuffer::AddressAlignment);
        NN_ASSERT(outBuffer[i]);
        EncodeAndDecode(&systemEvent, &audioOut, audioOutBuffer);
        //GenerateSquareWave(sampleFormat, outBuffer[i], channelCount, sampleRate, frameSampleCount, amplitude);
        nn::audio::SetAudioOutBufferInfo(&audioOutBuffer[i], outBuffer[i], bufferSize, dataSize);
        nn::audio::AppendAudioOutBuffer(&audioOut, &audioOutBuffer[i]);
    }

    // Start playback.
    NN_ABORT_UNLESS(
        nn::audio::StartAudioOut(&audioOut).IsSuccess(),
        "Failed to start playback."
    );
    NNS_LOG("AudioOut is started\n  State: %s\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&audioOut)));

    // Wait one frame.
    const nn::TimeSpan interval(nn::TimeSpan::FromNanoSeconds(1000 * 1000 * 1000 / frameRate));
    nn::os::SleepThread(interval);

    PrintUsage();

    // Play the audio.
    NNS_LOG("Start audio playback\n");

    for(;;)
    {
#if defined(USE_NPAD)
        nn::hid::NpadButtonSet npadButtonCurrent = {};
        nn::hid::NpadButtonSet npadButtonDown = {};

        // Get the Npad input.
        if (nn::hid::GetNpadStyleSet(nn::hid::NpadId::No1).Test<nn::hid::NpadStyleFullKey>())
        {
            static nn::hid::NpadFullKeyState npadFullKeyState = {};
            nn::hid::NpadFullKeyState state;
            nn::hid::GetNpadState(&state, nn::hid::NpadId::No1);
            npadButtonCurrent |= state.buttons;
            npadButtonDown |= state.buttons & ~npadFullKeyState.buttons;
            npadFullKeyState = state;
        }
        if (nn::hid::GetNpadStyleSet(nn::hid::NpadId::Handheld).Test<nn::hid::NpadStyleHandheld>())
        {
            static nn::hid::NpadHandheldState npadHandheldState = {};
            nn::hid::NpadHandheldState state;
            nn::hid::GetNpadState(&state, nn::hid::NpadId::Handheld);
            npadButtonCurrent |= state.buttons;
            npadButtonDown |= state.buttons & ~npadHandheldState.buttons;
            npadHandheldState = state;
        }

        // Get the input from the DebugPad.
        {
            static nn::hid::DebugPadState debugPadState = {};
            nn::hid::DebugPadButtonSet debugPadButtonCurrent = {};
            nn::hid::DebugPadButtonSet debugPadButtonDown = {};
            nn::hid::DebugPadState state;
            nn::hid::GetDebugPadState(&state);
            debugPadButtonCurrent |= state.buttons;
            debugPadButtonDown |= state.buttons & ~debugPadState.buttons;
            debugPadState = state;
            nns::audio::ConvertDebugPadButtonsToNpadButtons(&npadButtonCurrent, debugPadButtonCurrent);
            nns::audio::ConvertDebugPadButtonsToNpadButtons(&npadButtonDown, debugPadButtonDown);
        }

        if(npadButtonDown.Test< ::nn::hid::NpadButton::Plus >())
        {
            break;
        }

        // Change the volume.
        const auto volumeStep = 0.1f;
        const auto currentVolume = nn::audio::GetAudioOutVolume(&audioOut);
        auto updateVolume = currentVolume;

        if(npadButtonCurrent.Test< ::nn::hid::NpadButton::L >())
        {
            updateVolume = std::max(nn::audio::AudioOut::GetVolumeMin(), currentVolume - volumeStep);
        }

        if(npadButtonCurrent.Test< ::nn::hid::NpadButton::R >())
        {
            updateVolume = std::min(nn::audio::AudioOut::GetVolumeMax(), currentVolume + volumeStep);
        }

        if(updateVolume != currentVolume)
        {
            nn::audio::SetAudioOutVolume(&audioOut, updateVolume);
            NNS_LOG("Volume: %.2f\n", updateVolume);
        }
#endif // USE_NPAD

        // Timeout condition.
        if(timeout > 0 && (nn::os::GetSystemTick().ToTimeSpan() > endTime))
        {
            break;
        }
        systemEvent.Wait();

        // Get the buffer that completed playback.
        nn::audio::AudioOutBuffer* pAudioOutBuffer = nullptr;

        pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(&audioOut);
        while (pAudioOutBuffer)
        {
            // Create square waveform data and register it again.
            void* pOutBuffer = nn::audio::GetAudioOutBufferDataPointer(pAudioOutBuffer);
            NN_ASSERT(nn::audio::GetAudioOutBufferDataSize(pAudioOutBuffer) == frameSampleCount * channelCount * nn::audio::GetSampleByteSize(sampleFormat));
            GenerateSquareWave(sampleFormat, pOutBuffer, channelCount, sampleRate, frameSampleCount, amplitude);
            nn::audio::AppendAudioOutBuffer(&audioOut, pAudioOutBuffer);

            pAudioOutBuffer = nn::audio::GetReleasedAudioOutBuffer(&audioOut);
        }
    }

    NNS_LOG("Stop audio playback\n");

    // Stop playback.
    nn::audio::StopAudioOut(&audioOut);
    NNS_LOG("AudioOut is closed\n  State: %s\n", GetAudioOutStateName(nn::audio::GetAudioOutState(&audioOut)));

    // Close audio output.
    nn::audio::CloseAudioOut(&audioOut);
    nn::os::DestroySystemEvent(systemEvent.GetBase());

    // Free memory.
    for (int i = 0; i < bufferCount; ++i)
    {
        allocator.Free(outBuffer[i]);
    }

    return;
} // NOLINT(readability/fn_size)
