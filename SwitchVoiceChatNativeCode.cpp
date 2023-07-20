#include "SwitchVoiceChatNativeCode.h"

namespace SwitchVoiceChatNativeCode {
	using namespace nn::audio;
	using namespace nn::codec;
	const int BUFFER_LENGTH_MILIS = 50;
	const int ENCODER_BIT_RATE = 24000;
	const int MIN_TOTAL_BUFFER_SIZE = 32 * 16384;
	const int ENCODER_FRAME_DURATION = 10000; // only 5000, 10000, and 20000 are valids values
	const int MAX_OPUS_ENCODER_OUTPUT_SIZE = OpusPacketSizeMaximum;

	AudioIn audioIn;
	AudioInBuffer audioInBuffer;
	nn::mem::StandardAllocator allocator;
	unsigned char* totalBuffer;
	void* audioBuffer;

	// remainToEncodeBuffer is a circular array
	int16_t* remainToEncodeBuffer;
	size_t remainToEncodeBufferSize;
	size_t remainToEncodeBufferStart;
	size_t remainToEncodeBufferEnd;
	int16_t* tempInputEncoderBuffer;

	size_t opusWorkBufferSize;
	unsigned char* opusWorkBuffer;
	OpusEncoder* encoder;
	int encodeSampleCountMaximum;

	int channelCount = 0;
	int sampleRate = 48000;

	bool AllocateBuffers()
	{
		channelCount = GetAudioInChannelCount(&audioIn);
		sampleRate = GetAudioInSampleRate(&audioIn);
		SampleFormat sampleFormat = GetAudioInSampleFormat(&audioIn);
		size_t sampleByteSize = GetSampleByteSize(sampleFormat);

		int frameRate = 1000 / BUFFER_LENGTH_MILIS;
		int frameSampleCount = sampleRate / frameRate;
		size_t dataSize = frameSampleCount * channelCount * sampleByteSize;
		size_t audioBufferSize = nn::util::align_up(dataSize, AudioOutBuffer::SizeGranularity);

		size_t totalBufferSize = audioBufferSize;
		if (totalBufferSize < MIN_TOTAL_BUFFER_SIZE) totalBufferSize = MIN_TOTAL_BUFFER_SIZE;
		totalBuffer = new unsigned char[totalBufferSize]();
		allocator.Initialize(totalBuffer, totalBufferSize);

		remainToEncodeBufferSize = totalBufferSize;
		remainToEncodeBuffer = new int16_t[remainToEncodeBufferSize];
		remainToEncodeBufferStart = 0;
		remainToEncodeBufferEnd = 0;

		audioBuffer = allocator.Allocate(audioBufferSize, AudioInBuffer::AddressAlignment);
		if (audioBuffer)
		{
			SetAudioInBufferInfo(&audioInBuffer, audioBuffer, audioBufferSize, dataSize);
			return true;
		}
		else
		{
			allocator.Finalize();
			delete totalBuffer;
			return false;
		}
	}

	bool InitializeEncoder()
	{
		encoder = new OpusEncoder();
		opusWorkBufferSize = encoder->GetWorkBufferSize(sampleRate, 1); // channelCount = 1, because we use mono
		opusWorkBuffer = new unsigned char[opusWorkBufferSize];
		OpusResult result = encoder->Initialize(sampleRate, 1, opusWorkBuffer, opusWorkBufferSize);
		if (result != OpusResult_Success) return false;

		encoder->SetBitRate(ENCODER_BIT_RATE);
		encoder->BindCodingMode(OpusCodingMode_Auto);

		encodeSampleCountMaximum = encoder->CalculateFrameSampleCount(ENCODER_FRAME_DURATION);
		tempInputEncoderBuffer = new int16_t[encodeSampleCountMaximum];
		return true;
	}

	void FinalizeEncoder()
	{
		encoder->Finalize();
		delete tempInputEncoderBuffer;
		delete opusWorkBuffer;
	}

	inline bool IsEmptyRemainToEncodeBuffer()
	{
		return remainToEncodeBufferStart == remainToEncodeBufferEnd;
	}

	inline size_t SizeRemainToEncodeBuffer()
	{
		if (remainToEncodeBufferStart <= remainToEncodeBufferEnd)
		{
			return remainToEncodeBufferEnd - remainToEncodeBufferStart;
		}
		else
		{
			return remainToEncodeBufferSize + remainToEncodeBufferEnd - remainToEncodeBufferStart;
		}
	}

	// Push elements to the remainToEncodeBuffer
	void PushRemainToEncodeBuffer(int16_t value)
	{
		remainToEncodeBuffer[remainToEncodeBufferEnd] = value;
		remainToEncodeBufferEnd = (remainToEncodeBufferEnd + 1) % remainToEncodeBufferSize;
		if (remainToEncodeBufferStart == remainToEncodeBufferEnd)
		{
			remainToEncodeBufferStart = (remainToEncodeBufferStart + 1) % remainToEncodeBufferSize;
		}
	}

	// Pop (adn discards) elements from the remainToEncodeBuffer
	void PopRemainToEncodeBuffer(size_t quantity)
	{
		if (quantity > SizeRemainToEncodeBuffer()) quantity = SizeRemainToEncodeBuffer();
		remainToEncodeBufferStart = (remainToEncodeBufferStart + quantity) % remainToEncodeBufferSize;
	}

	// Copy remainToEncodeBuffer to dest buffer (only call this function if count <= SizeRemainToEncodeBuffer)
	void CopyRemainToEncodeBuffer(int16_t* dest, size_t count)
	{
		if (remainToEncodeBufferStart <= remainToEncodeBufferEnd)
		{
			int16_t* source = &remainToEncodeBuffer[remainToEncodeBufferStart];
			memcpy(dest, source, count);
		}
		else
		{
			if (remainToEncodeBufferStart + count <= remainToEncodeBufferSize)
			{
				int16_t* source = &remainToEncodeBuffer[remainToEncodeBufferStart];
				memcpy(dest, source, count);
			}
			else
			{
				int16_t* source = &remainToEncodeBuffer[remainToEncodeBufferStart];
				size_t sourceCount = remainToEncodeBufferSize - remainToEncodeBufferStart;
				memcpy(dest, source, sourceCount);
				sourceCount = count - sourceCount;
				memcpy(dest, remainToEncodeBuffer, sourceCount);
			}
		}
	}

	void GetMicrophoneInput()
	{
		AudioInBuffer* releasedBuffer = GetReleasedAudioInBuffer(&audioIn);
		if (releasedBuffer)
		{
			size_t releasedBufferSize = GetAudioInBufferDataSize(releasedBuffer) / 2;
			int16_t* releasedBufferPointer = reinterpret_cast<int16_t*>(GetAudioInBufferDataPointer(releasedBuffer));

			// only get one channel
			size_t audioBufferMonoSize = releasedBufferSize / channelCount;
			int iteration;
			for (int i = 0; i < audioBufferMonoSize; i++)
			{
				PushRemainToEncodeBuffer(0);
				iteration++;
			}
			NN_LOG("Samples Captured by Mic: %d", iteration);
			AppendAudioInBuffer(&audioIn, &audioInBuffer);
		}
	}

	bool GetMicrophoneInputExternal(void* outBuffer, size_t& pOutBufferSize)
	{
		auto outBufferReinterpreted = static_cast<int16_t*>(outBuffer);
		AudioInBuffer* releasedBuffer = GetReleasedAudioInBuffer(&audioIn);
		if (releasedBuffer)
		{
			size_t releasedBufferSize = GetAudioInBufferDataSize(releasedBuffer);
			int16_t* releasedBufferPointer = reinterpret_cast<int16_t*>(GetAudioInBufferDataPointer(releasedBuffer));

			// only get one channel
			size_t audioBufferMonoSize = releasedBufferSize / channelCount;
			for (int i = 0; i < releasedBufferSize; i++)
			{
				//NN_LOG("%d\n",releasedBufferPointer[i]);
				outBufferReinterpreted[i] = 0;
			}
			AppendAudioInBuffer(&audioIn, &audioInBuffer);
			pOutBufferSize = releasedBufferSize;
			return true;
		}
		else
		{
			pOutBufferSize = 0;
			return false;
		}
	}

	bool Encode(intptr_t* handler, unsigned char** bufferOut, int* count, size_t& outSampleCount)
	{
		size_t partialEncodedOutSize = 0;
		size_t totalEncodedOutSize = 0;
		auto outVector = new std::vector<unsigned char>(0);

		int iteration = 0;
		size_t remainToEncodeSize = SizeRemainToEncodeBuffer();

		outSampleCount = 0;
		while (remainToEncodeSize >= encodeSampleCountMaximum)
		{
			NN_LOG("Size Remain To Encode Buffer: %d\nIteration: %i\n", remainToEncodeSize, iteration);
			CopyRemainToEncodeBuffer(tempInputEncoderBuffer, encodeSampleCountMaximum);
			outVector->resize(totalEncodedOutSize + MAX_OPUS_ENCODER_OUTPUT_SIZE);
			OpusResult result = encoder->EncodeInterleaved(
				&partialEncodedOutSize, outVector->data() + totalEncodedOutSize, MAX_OPUS_ENCODER_OUTPUT_SIZE,
				tempInputEncoderBuffer, encodeSampleCountMaximum);
			NN_LOG("PartialEncodedOutSize: %d\n", partialEncodedOutSize);
			if (result != OpusResult_Success)
			{
				NN_LOG("Opus Encoding Error: %s", result);
				return false;
			}

			totalEncodedOutSize += partialEncodedOutSize;
			PopRemainToEncodeBuffer(encodeSampleCountMaximum);
			remainToEncodeSize = SizeRemainToEncodeBuffer();
			iteration++;
			outSampleCount += encodeSampleCountMaximum;
		}
		if (remainToEncodeSize < encodeSampleCountMaximum)
		{
			;
		}
		outVector->resize(totalEncodedOutSize);

		*handler = reinterpret_cast<intptr_t>(outVector);
		*bufferOut = outVector->data();
		*count = outVector->size();

		if (*count > 0)
		{
			return true;
		}
		else return false;
	}

	extern "C" void wntgd_StopRecordVoice()
	{
		// encoder cleanup
		FinalizeEncoder();
		delete remainToEncodeBuffer;

		// audioIn cleanup
		StopAudioIn(&audioIn);
		CloseAudioIn(&audioIn);
		allocator.Free(audioBuffer);
		allocator.Finalize();
		delete totalBuffer;
	}

	extern "C" bool wntgd_StartRecordVoice()
	{
		AudioInParameter param;
		InitializeAudioInParameter(&param);

		if (!OpenDefaultAudioIn(&audioIn, param).IsSuccess()) return false;

		if (!StartAudioIn(&audioIn).IsSuccess())
		{
			CloseAudioIn(&audioIn);
			return false;
		}

		if (!AllocateBuffers())
		{
			StopAudioIn(&audioIn);
			CloseAudioIn(&audioIn);
			return false;
		}

		if (!InitializeEncoder())
		{
			wntgd_StopRecordVoice();
			return false;
		}

		AppendAudioInBuffer(&audioIn, &audioInBuffer);
		return true;
	}

	extern "C" bool wntgd_GetVoiceBuffer(intptr_t * handler, unsigned char** bufferOut, int* count, size_t& outSampleCount)
	{
		GetMicrophoneInput();
		return Encode(handler, bufferOut, count, outSampleCount);
	}

	extern "C" bool wntgd_ReleaseVoiceBuffer(intptr_t * handler)
	{
		auto outVector = reinterpret_cast<std::vector<unsigned char>*>(handler);
		delete outVector;
		return true;
	}
}
