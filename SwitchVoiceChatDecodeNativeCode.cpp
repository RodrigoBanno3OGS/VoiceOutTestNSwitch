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

	extern "C" bool wntgd_DecompressVoiceData(intptr_t * handle, unsigned char* inputBuffer, int count, void* audioOutBuffer, int* outSampleCount, unsigned int* sampleRateOut)
	{
		auto audioOutBufferReinterpreted = reinterpret_cast<int16_t*>(audioOutBuffer);
		size_t partialConsumed = 0;
		int partialOutSampleCount = 0;
		size_t totalConsumed = 0;
		size_t totalOutSampleCount = 0;
		std::vector<float>* outVector = new std::vector<float>(0);
		bool result = true;
		int iteration = 0;
		int aaaaaa = 0;
		int sampleCount = 0;
		while (count > 0)
		{
			OpusResult decoderResult = decoder->DecodeInterleaved(&partialConsumed, &partialOutSampleCount,
				decoderOutBuffer, decoderOutBufferSize, inputBuffer, count);
			NN_LOG("Iteration: %i, %d\n", iteration, partialOutSampleCount);
			if (decoderResult == OpusResult_Success)
			{
				inputBuffer += partialConsumed;
				count -= partialConsumed;
				totalConsumed += partialConsumed;
				totalOutSampleCount += partialOutSampleCount;
				outVector->resize(totalOutSampleCount);
				size_t index = totalOutSampleCount - partialOutSampleCount;
				//NN_LOG("INDEX: %d\n", index);
				for (int i = 0; i < partialOutSampleCount; i++)
				{
					//NN_LOG("SAMPLE COUNTER: %d\n", sampleCount);
					audioOutBufferReinterpreted[sampleCount] = decoderOutBuffer[i];
					sampleCount++;
					//NN_LOG("SAMPLE COUNTER: %d\n", sampleCount);
					audioOutBufferReinterpreted[sampleCount] = decoderOutBuffer[i];
					sampleCount++;
				}
			}
			else
			{
				result = false;
				break;
			}
			iteration++;
		}

		*handle = reinterpret_cast<intptr_t>(outVector);
		//*audioOutBuffer = outVector->data();
		*outSampleCount = sampleCount;
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
