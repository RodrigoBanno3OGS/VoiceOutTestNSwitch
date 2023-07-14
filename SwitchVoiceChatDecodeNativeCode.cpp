#include "SwitchVoiceChatDEcodeNativeCode.h";
#include <nns/nns_Log.h>

namespace SwitchVoiceChatDecodeNativeCode {
	using namespace nn::audio;
	using namespace nn::codec;

	const int TOTAL_BUFFER_SIZE = 1024 * 1024;

	nn::mem::StandardAllocator decoderAllocator;
	unsigned char* totalBufferDecoder;
	int16_t* decoderOutBuffer;
	int decoderOutBufferSize;

	size_t opusDecoderWorkBufferSize;
	unsigned char* opusDecoderWorkBuffer;
	OpusDecoder* decoder;

	const int SAMPLE_RATE = 48000;

	extern "C" bool wntgd_InitializeDecoder()
	{
		totalBufferDecoder = new unsigned char[TOTAL_BUFFER_SIZE]();
		decoderAllocator.Initialize(totalBufferDecoder, TOTAL_BUFFER_SIZE);

		decoderOutBufferSize = TOTAL_BUFFER_SIZE * 9 / 10;
		NNS_LOG("OPUS DECODER out BUFFER SIZE: %i\n", decoderOutBufferSize);
		decoderOutBuffer = reinterpret_cast<int16_t*>(decoderAllocator.Allocate(decoderOutBufferSize, AudioInBuffer::AddressAlignment));
		decoderOutBufferSize = decoderOutBufferSize / sizeof(int16_t);
		NNS_LOG("OPUS DECODER out BUFFER SIZE: %i\n", decoderOutBufferSize);
		if (!decoderOutBuffer)
		{
			decoderAllocator.Finalize();
			delete totalBufferDecoder;
			return false;
		}

		decoder = new OpusDecoder();
		opusDecoderWorkBufferSize = decoder->GetWorkBufferSize(SAMPLE_RATE, 1); // channelCount = 1, because we use mono
		NNS_LOG("OPUS DECODER WORK BUFFER SIZE: %i\n", opusDecoderWorkBufferSize);
		opusDecoderWorkBuffer = new unsigned char[opusDecoderWorkBufferSize];
		OpusResult result = decoder->Initialize(SAMPLE_RATE, 1, opusDecoderWorkBuffer, opusDecoderWorkBufferSize);
		NNS_LOG("OPUS RESULT: %i\n", result);
		if (result != OpusResult_Success)
		{
			return false;
		}
		else
		{
			return true;
		}
	}

	extern "C" void wntgd_FinalizeDecoder()
	{
		decoder->Finalize();
		delete opusDecoderWorkBuffer;
		decoderAllocator.Free(decoderOutBuffer);
		decoderAllocator.Finalize();
		delete totalBufferDecoder;
	}

	extern "C" bool wntgd_DecompressVoiceData(intptr_t * handle, unsigned char* inputBuffer, int count, float** audioOut, int* outSampleCount, unsigned int* sampleRateOut)
	{
		size_t partialConsumed = 0;
		int partialOutSampleCount = 0;
		size_t totalConsumed = 0;
		size_t totalOutSampleCount = 0;
		std::vector<float>* outVector = new std::vector<float>(0);
		bool result = true;

		while (count > 0)
		{
			OpusResult decoderResult = decoder->DecodeInterleaved(&partialConsumed, &partialOutSampleCount,
				decoderOutBuffer, decoderOutBufferSize, inputBuffer, count);

			if (decoderResult == OpusResult_Success)
			{
				inputBuffer += partialConsumed;
				count -= partialConsumed;
				totalConsumed += partialConsumed;
				totalOutSampleCount += partialOutSampleCount;
				outVector->resize(totalOutSampleCount);
				for (int i = 0; i < partialOutSampleCount; i++)
				{
					outVector->at(totalOutSampleCount - partialOutSampleCount + i) = static_cast<float>(decoderOutBuffer[i]) / 32767;
				}
			}
			else
			{
				result = false;
				break;
			}
		}

		*handle = reinterpret_cast<intptr_t>(outVector);
		*audioOut = outVector->data();
		*outSampleCount = totalOutSampleCount;
		*sampleRateOut = SAMPLE_RATE;
		return result;
	}

	extern "C" bool wntgd_ReleaseDecompressBuffer(intptr_t * handler)
	{
		auto outVector = reinterpret_cast<std::vector<float>*>(handler);
		delete outVector;
		return true;
	}
}
