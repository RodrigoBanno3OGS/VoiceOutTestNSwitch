#pragma once
#include <stdint.h>
#include <vector>
#include <cstdlib>
#include <nn/audio.h>
#include <nn/codec.h>
#include <nn/mem.h>
#include <nn/os.h>
#include <nn/nn_Log.h>



namespace SwitchVoiceChatDecodeNativeCode {
	extern "C" bool wntgd_InitializeDecoder();
	extern "C" void wntgd_FinalizeDecoder();
	extern "C" bool wntgd_DecompressVoiceData(intptr_t * handle, unsigned char* inputBuffer, int count, float** audioOut, int* outSampleCount, unsigned int* sampleRateOut);
	extern "C" bool wntgd_ReleaseDecompressBuffer(intptr_t * handler);
}
