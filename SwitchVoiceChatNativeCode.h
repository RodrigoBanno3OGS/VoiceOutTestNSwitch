#pragma once
#include <stdint.h>
#include <vector>
#include <cstdlib>
#include <nn/audio.h>
#include <nn/codec.h>
#include <nn/mem.h>
#include <nn/os.h>
#include <nn/nn_Log.h>



namespace SwitchVoiceChatNativeCode {
	bool AllocateBuffers();
	bool InitializeEncoder();
	void FinalizeEncoder();
	inline bool IsEmptyRemainToEncodeBuffer();
	inline size_t SizeRemainToEncodeBuffer();
	void PushRemainToEncodeBuffer(int16_t value);
	void PopRemainToEncodeBuffer(size_t quantity);
	void CopyRemainToEncodeBuffer(int16_t* dest, size_t count);
	void GetMicrophoneInput();
	bool Encode(intptr_t* handler, unsigned char** bufferOut, int* count);
	extern "C" void wntgd_StopRecordVoice();
	extern "C" bool wntgd_StartRecordVoice();
	extern "C" bool wntgd_GetVoiceBuffer(intptr_t * handler, unsigned char** bufferOut, int* count);
	extern "C" bool wntgd_ReleaseVoiceBuffer(intptr_t * handler);
}
