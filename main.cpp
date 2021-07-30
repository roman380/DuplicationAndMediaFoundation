#include "main.h"
#include "WinDesktopDup.h"
#include <iostream>

WinDesktopDup dup;

void SetupDpiAwareness()
{
	if (!SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE))
		printf("SetProcessDpiAwarenessContext failed\n");
}

const UINT32 VIDEO_WIDTH = 3840;
const UINT32 VIDEO_HEIGHT = 2160;
const UINT32 VIDEO_FPS = 30;
const UINT64 VIDEO_FRAME_DURATION = 10 * 1000 * 1000 / VIDEO_FPS;
const UINT32 VIDEO_BIT_RATE = 800000;
const GUID   VIDEO_ENCODING_FORMAT = MFVideoFormat_H264;
const GUID   VIDEO_INPUT_FORMAT = MFVideoFormat_RGB32;
const UINT32 VIDEO_PELS = VIDEO_WIDTH * VIDEO_HEIGHT;
const UINT32 VIDEO_FRAME_COUNT = 20 * VIDEO_FPS;

template <class T>
void SafeRelease(T** ppT) {
	if (*ppT) {
		(*ppT)->Release();
		*ppT = NULL;
	}
}

HRESULT InitializeSinkWriter(IMFSinkWriter** ppWriter, DWORD* pStreamIndex)
{
	IMFDXGIDeviceManager* pDeviceManager = NULL;
	UINT                  resetToken;
	IMFAttributes* attributes;

	*ppWriter = NULL;
	*pStreamIndex = NULL;

	IMFSinkWriter* pSinkWriter = NULL;
	IMFMediaType* pMediaTypeOut = NULL;
	IMFMediaType* pMediaTypeIn = NULL;
	DWORD          streamIndex;

	HRESULT hr = MFCreateDXGIDeviceManager(&resetToken, &pDeviceManager);
	if (!SUCCEEDED(hr)) { printf("MFCreateDXGIDeviceManager failed\n"); }
	hr = pDeviceManager->ResetDevice(dup.D3DDevice, resetToken);
	if (!SUCCEEDED(hr)) { printf("ResetDevice failed\n"); }

	hr = MFCreateAttributes(&attributes, 3);
	if (!SUCCEEDED(hr)) { printf("MFCreateAttributes failed\n"); }
	hr = attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, 1);
	if (!SUCCEEDED(hr)) { printf("SetUINT32 failed\n"); }
	hr = attributes->SetUINT32(MF_LOW_LATENCY, 1);
	if (!SUCCEEDED(hr)) { printf("SetUINT32 (2) failed\n"); }
	hr = attributes->SetUnknown(MF_SINK_WRITER_D3D_MANAGER, pDeviceManager);
	if (!SUCCEEDED(hr)) { printf("SetUnknown failed\n"); }
	hr = MFCreateSinkWriterFromURL(L"output.mp4", NULL, attributes, &pSinkWriter);
	if (!SUCCEEDED(hr)) { printf("MFCreateSinkWriterFromURL failed\n"); }

	// Set the output media type.
	hr = MFCreateMediaType(&pMediaTypeOut);
	if (!SUCCEEDED(hr)) { printf("MFCreateMediaType failed\n"); }
	hr = pMediaTypeOut->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (!SUCCEEDED(hr)) { printf("SetGUID failed\n"); }
	hr = pMediaTypeOut->SetGUID(MF_MT_SUBTYPE, VIDEO_ENCODING_FORMAT);
	if (!SUCCEEDED(hr)) { printf("SetGUID (2) failed\n"); }
	hr = pMediaTypeOut->SetUINT32(MF_MT_AVG_BITRATE, VIDEO_BIT_RATE);
	if (!SUCCEEDED(hr)) { printf("SetUINT32 (3) failed\n"); }
	hr = pMediaTypeOut->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (!SUCCEEDED(hr)) { printf("SetUINT32 (4) failed\n"); }
	hr = MFSetAttributeSize(pMediaTypeOut, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeSize failed\n"); }
	hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeRatio failed\n"); }
	hr = MFSetAttributeRatio(pMediaTypeOut, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeRatio (2) failed\n"); }
	hr = pSinkWriter->AddStream(pMediaTypeOut, &streamIndex);
	if (!SUCCEEDED(hr)) { printf("AddStream failed\n"); }

	// Set the input media type.
	hr = MFCreateMediaType(&pMediaTypeIn);
	if (!SUCCEEDED(hr)) { printf("MFCreateMediaType failed\n"); }
	hr = pMediaTypeIn->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	if (!SUCCEEDED(hr)) { printf("SetGUID (3) failed\n"); }
	hr = pMediaTypeIn->SetGUID(MF_MT_SUBTYPE, VIDEO_INPUT_FORMAT);
	if (!SUCCEEDED(hr)) { printf("SetGUID (4) failed\n"); }
	hr = pMediaTypeIn->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	if (!SUCCEEDED(hr)) { printf("SetUINT32 (5) failed\n"); }
	hr = MFSetAttributeSize(pMediaTypeIn, MF_MT_FRAME_SIZE, VIDEO_WIDTH, VIDEO_HEIGHT);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeSize (2) failed\n"); }
	hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_FRAME_RATE, VIDEO_FPS, 1);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeRatio (3) failed\n"); }
	hr = MFSetAttributeRatio(pMediaTypeIn, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	if (!SUCCEEDED(hr)) { printf("MFSetAttributeRatio (4) failed\n"); }
	hr = pSinkWriter->SetInputMediaType(streamIndex, pMediaTypeIn, NULL);
	if (!SUCCEEDED(hr)) { printf("SetInputMediaType failed\n"); }

	// Tell the sink writer to start accepting data.
	hr = pSinkWriter->BeginWriting();
	if (!SUCCEEDED(hr)) { printf("BeginWriting failed\n"); }

	// Return the pointer to the caller.
	*ppWriter = pSinkWriter;
	(*ppWriter)->AddRef();
	*pStreamIndex = streamIndex;


	SafeRelease(&pSinkWriter);
	SafeRelease(&pMediaTypeOut);
	SafeRelease(&pMediaTypeIn);
	return hr;
}

ID3D11Texture2D* texture;

HRESULT WriteFrame(IMFSinkWriter* pWriter, DWORD streamIndex, const LONGLONG& rtStart)
{
	IMFSample* pSample = NULL;
	IMFMediaBuffer* pBuffer = NULL;

	HRESULT hr = MFCreateVideoSampleFromSurface(texture, &pSample);
	if (!SUCCEEDED(hr)) { printf("MFCreateVideoSampleFromSurface failed\n"); }
	hr = MFCreateDXGISurfaceBuffer(__uuidof(ID3D11Texture2D), texture, 0, false, &pBuffer);
	if (!SUCCEEDED(hr)) { printf("MFCreateDXGISurfaceBuffer failed\n"); }

	DWORD len;
	hr = ((IMF2DBuffer*)pBuffer)->GetContiguousLength(&len);
	if (!SUCCEEDED(hr)) { printf("GetContiguousLength failed\n"); }

	hr = pBuffer->SetCurrentLength(len);
	if (!SUCCEEDED(hr)) { printf("SetCurrentLength failed\n"); }

	// Create a media sample and add the buffer to the sample.
	hr = MFCreateSample(&pSample);
	if (!SUCCEEDED(hr)) { printf("MFCreateSample failed\n"); }

	hr = pSample->AddBuffer(pBuffer);
	if (!SUCCEEDED(hr)) { printf("AddBuffer failed\n"); }

	// Set the time stamp and the duration.
	hr = pSample->SetSampleTime(rtStart);
	if (!SUCCEEDED(hr)) { printf("SetSampleTime failed\n"); }

	hr = pSample->SetSampleDuration(VIDEO_FRAME_DURATION);
	if (!SUCCEEDED(hr)) { printf("SetSampleDuration failed\n"); }

	// Send the sample to the Sink Writer.

	hr = pWriter->WriteSample(streamIndex, pSample);

	if (!SUCCEEDED(hr)) { printf("WriteSample failed\n"); }

	SafeRelease(&pSample);
	SafeRelease(&pBuffer);
	return hr;
}

int APIENTRY main(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
{
	dup.copytocpu = true; // Self explanatory - when set to false, CaptureNext() will return gpuTex, instead of doing CopyResource
						  // to cpuTex and returning that. 
	SetupDpiAwareness();
	auto err = dup.Initialize();

	// Create MediaSinkWriter
	CoInitializeEx(0, COINIT_APARTMENTTHREADED); // Need to call this once when a thread is using COM or it wont work
	MFStartup(MF_VERSION);                       // Need to call this too for Media Foundation related memes
	IMFSinkWriter* pSinkWriter = NULL;
	DWORD          stream;
	HRESULT        hr = InitializeSinkWriter(&pSinkWriter, &stream);
	LONGLONG       rtStart = 0;


	const int CAPTURE_LENGTH = 5;

	int total_frames = VIDEO_FPS * CAPTURE_LENGTH;

	for (int i = 0; i < total_frames; i++)
	{
		texture = dup.CaptureNext();
		if (texture != nullptr)
		{
			hr = WriteFrame(pSinkWriter, stream, rtStart);
			if (!SUCCEEDED(hr))
				printf("Write Frame failed\n");
			rtStart += VIDEO_FRAME_DURATION;
			texture->Release();
		}
		else
		{
			i--;
		}
	}

	if (FAILED(hr))
	{
		std::cout << "Failure" << std::endl;
	}

	if (SUCCEEDED(hr)) {
		hr = pSinkWriter->Finalize();
	}

	SafeRelease(&pSinkWriter);
	MFShutdown();
	CoUninitialize();
}