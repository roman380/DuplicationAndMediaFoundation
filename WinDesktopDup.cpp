#include "WinDesktopDup.h"
#include <iostream>

WinDesktopDup::~WinDesktopDup() 
{
	Close();
}

Error WinDesktopDup::Initialize() 
{
	// Get desktop
	HDESK hDesk = OpenInputDesktop(0, FALSE, GENERIC_ALL);
	if (!hDesk)
		return "Failed to open desktop";

	// Attach desktop to this thread (presumably for cases where this is not the main/UI thread)
	bool deskAttached = SetThreadDesktop(hDesk) != 0;
	CloseDesktop(hDesk);
	hDesk = nullptr;
	if (!deskAttached)
		printf("Failed to attach recording thread to desktop");

	// Initialize DirectX
	HRESULT hr = S_OK;

	// Driver types supported
	D3D_DRIVER_TYPE driverTypes[] = {
	    D3D_DRIVER_TYPE_HARDWARE,
	    D3D_DRIVER_TYPE_WARP,
	    D3D_DRIVER_TYPE_REFERENCE,
	};
	auto numDriverTypes = ARRAYSIZE(driverTypes);

	// Feature levels supported
	D3D_FEATURE_LEVEL featureLevels[] = {
	    D3D_FEATURE_LEVEL_11_0,
	    D3D_FEATURE_LEVEL_10_1,
	    D3D_FEATURE_LEVEL_10_0,
	    D3D_FEATURE_LEVEL_9_1};
	auto numFeatureLevels = ARRAYSIZE(featureLevels);

	D3D_FEATURE_LEVEL featureLevel;

	// Create device
	for (size_t i = 0; i < numDriverTypes; i++) {
		hr = D3D11CreateDevice(nullptr, driverTypes[i], nullptr, 0, featureLevels, (UINT) numFeatureLevels,
		                       D3D11_SDK_VERSION, &D3DDevice, &featureLevel, &D3DDeviceContext);
		if (SUCCEEDED(hr))
			break;
	}
	if (FAILED(hr))
		printf("D3D11CreateDevice failed: %v", hr);

	// Initialize the Desktop Duplication system
	//m_OutputNumber = Output;

	// Get DXGI device
	IDXGIDevice* dxgiDevice = nullptr;
	hr                      = D3DDevice->QueryInterface(__uuidof(IDXGIDevice), (void**) &dxgiDevice);
	if (FAILED(hr))
		printf("D3DDevice->QueryInterface failed: %v", hr);

	// Get DXGI adapter
	IDXGIAdapter* dxgiAdapter = nullptr;
	hr                        = dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**) &dxgiAdapter);
	dxgiDevice->Release();
	dxgiDevice = nullptr;
	if (FAILED(hr)) {
		printf("dxgiDevice->GetParent failed: %v", hr);
		//return ProcessFailure(m_Device, L"Failed to get parent DXGI Adapter", L"Error", hr, SystemTransitionsExpectedErrors);
	}

	// Get output
	IDXGIOutput* dxgiOutput = nullptr;
	hr                      = dxgiAdapter->EnumOutputs(OutputNumber, &dxgiOutput);
	dxgiAdapter->Release();
	dxgiAdapter = nullptr;
	if (FAILED(hr)) {
		printf("dxgiAdapter->EnumOutputs failed: %v", hr);
		//return ProcessFailure(m_Device, L"Failed to get specified output in DUPLICATIONMANAGER", L"Error", hr, EnumOutputsExpectedErrors);
	}

	dxgiOutput->GetDesc(&OutputDesc);

	// QI for Output 1
	IDXGIOutput1* dxgiOutput1 = nullptr;
	hr                        = dxgiOutput->QueryInterface(__uuidof(dxgiOutput1), (void**) &dxgiOutput1);
	dxgiOutput->Release();
	dxgiOutput = nullptr;
	if (FAILED(hr))
		printf("dxgiOutput->QueryInterface failed: %v", hr);

	// Create desktop duplication
	hr = dxgiOutput1->DuplicateOutput(D3DDevice, &DeskDupl);
	dxgiOutput1->Release();
	dxgiOutput1 = nullptr;
	if (FAILED(hr)) {
		if (hr == DXGI_ERROR_NOT_CURRENTLY_AVAILABLE) {
			//MessageBoxW(nullptr, L"There is already the maximum number of applications using the Desktop Duplication API running, please close one of those applications and then try again.", L"Error", MB_OK);
			return "Too many desktop recorders already active";
		}
		printf("DuplicateOutput failed: %v", hr);
		//return ProcessFailure(m_Device, L"Failed to get duplicate output in DUPLICATIONMANAGER", L"Error", hr, CreateDuplicationExpectedErrors);
	}

	return "";
}

void WinDesktopDup::Close() 
{
	if (DeskDupl)
		DeskDupl->Release();

	if (D3DDeviceContext)
		D3DDeviceContext->Release();

	if (D3DDevice)
		D3DDevice->Release();

	DeskDupl         = nullptr;
	D3DDeviceContext = nullptr;
	D3DDevice        = nullptr;
	HaveFrameLock    = false;
}

ID3D11Texture2D* WinDesktopDup::CaptureNext() 
{
	if (!DeskDupl)
		return nullptr;

	HRESULT hr;

	// according to the docs, it's best for performance if we hang onto the frame for as long as possible,
	// and only release the previous frame immediately before acquiring the next one. Something about
	// the OS coalescing updates, so that it doesn't have to store them as distinct things.
	if (HaveFrameLock) 
	{
		HaveFrameLock = false;
		hr            = DeskDupl->ReleaseFrame();
		// ignore response
	}

	IDXGIResource*          deskRes = nullptr;
	DXGI_OUTDUPL_FRAME_INFO frameInfo;
	hr = DeskDupl->AcquireNextFrame(0, &frameInfo, &deskRes);
	if (hr == DXGI_ERROR_WAIT_TIMEOUT) 
	{
		printf("Timeout: %x\n", hr);

		// nothing to see here
		return nullptr;
	}
	if (FAILED(hr)) 
	{
		// perhaps shutdown and reinitialize
		printf("Acquire failed: %x\n", hr);
		return nullptr;
	}

	HaveFrameLock = true;

	ID3D11Texture2D* gpuTex = nullptr;
	hr                      = deskRes->QueryInterface(__uuidof(ID3D11Texture2D), (void**) &gpuTex);
	deskRes->Release();
	deskRes = nullptr;
	if (FAILED(hr)) 
	{
		// not expected
		return nullptr;
	}

	//bool ok = true;

	//D3D11_TEXTURE2D_DESC desc;
	//gpuTex->GetDesc(&desc);
	//desc.CPUAccessFlags     = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
	//desc.Usage              = D3D11_USAGE_STAGING;
	//desc.BindFlags          = 0;
	//desc.MiscFlags          = 0; // D3D11_RESOURCE_MISC_GDI_COMPATIBLE ?
	//ID3D11Texture2D* cpuTex = nullptr;
	//hr                      = D3DDevice->CreateTexture2D(&desc, nullptr, &cpuTex);
	//if (SUCCEEDED(hr)) 
	//{
	//	D3DDeviceContext->CopyResource(cpuTex, gpuTex);
	//} 
	//else 
	//{
	//	// not expected
	//	ok = false;
	//	hr = 0;
	//	hr = D3DDevice->GetDeviceRemovedReason();
	//}

	//cpuTex->Release();
	//gpuTex->Release();

	return gpuTex;
}