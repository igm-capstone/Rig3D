#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Graphics\DirectX11\DX11Mesh.h"
#include "Graphics/DirectX11/DX11Shader.h"
#include "Graphics/DirectX11/DX11ShaderResource.h"
#include "Rig3D\Engine.h"
#include "rig_defines.h"
#include "Rig3D\Graphics\DirectX11\DX11RenderContext.h"
#include "Rig3D\Graphics\DirectX11\DirectXTK\Inc\WICTextureLoader.h"
#include "Rig3D\Graphics\DirectX11\DirectXTK\Inc\DDSTextureLoader.h"
#include <WindowsX.h>
#include <sstream>
#include <d3dcompiler.h>
#include <d3d11shader.h>

static wchar_t* WINDOW_CLASS_NAME = L"Rig3D DirectX11 Window";

using namespace Rig3D;

DX3D11Renderer& DX3D11Renderer::SharedInstance()
{
	static DX3D11Renderer sharedInstance;
	return sharedInstance;
}

DX3D11Renderer::DX3D11Renderer()
{
	mGraphicsAPI = GRAPHICS_API_DIRECTX11;
}

DX3D11Renderer::~DX3D11Renderer()
{

}

int DX3D11Renderer::VInitialize(HINSTANCE hInstance, HWND hwnd, Options options)
{
	mHINSTANCE				= hInstance;
	mHWND					= hwnd;
	mWindowWidth			= options.mWindowWidth;
	mWindowHeight			= options.mWindowHeight;
	mWindowCaption			= options.mWindowCaption;
	mFullScreen				= options.mFullScreen;
	mDriverType				= D3D_DRIVER_TYPE_HARDWARE;
	mEnable4xMsaa			= false;
	mMSAA4xQuality			= 0;
	mDevice					= 0;
	mDeviceContext			= 0;
	mDepthStencilBuffer		= 0;
	mDepthStencilView		= 0;
	mRenderTargetView		= 0;
	mSwapChain				= 0;

	ZeroMemory(&mViewport, sizeof(D3D11_VIEWPORT));

	if (InitializeD3D11() == RIG_ERROR) 
	{
		return RIG_ERROR;
	}

	WMEventHandler::SharedInstance().RegisterObserver(WM_SIZE, this);
	WMEventHandler::SharedInstance().RegisterObserver(WM_ENTERSIZEMOVE, this);
	WMEventHandler::SharedInstance().RegisterObserver(WM_EXITSIZEMOVE, this);

	return RIG_SUCCESS;
}

int DX3D11Renderer::InitializeD3D11()
{
	UINT createDeviceFlags = 0;

	// Do we want a debug device?
#if defined(DEBUG) || defined(_DEBUG)
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	// Set up a swap chain description
	DXGI_SWAP_CHAIN_DESC swapChainDesc;
	swapChainDesc.BufferDesc.Width = mWindowWidth;
	swapChainDesc.BufferDesc.Height = mWindowHeight;
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 1;
	swapChainDesc.OutputWindow = mHWND;
	swapChainDesc.Windowed = true;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;	// Enables Alt + Enter Windowed / Full Screen Toggle.
	if (mEnable4xMsaa)
	{
		// Set up 4x MSAA
		swapChainDesc.SampleDesc.Count = 4;
		swapChainDesc.SampleDesc.Quality = mMSAA4xQuality - 1;
	}
	else
	{
		// No MSAA
		swapChainDesc.SampleDesc.Count = 1;
		swapChainDesc.SampleDesc.Quality = 0;
	}

	// Create the device and swap chain and determine the supported feature level
	mFeatureLevel = D3D_FEATURE_LEVEL_9_1; // Will be overwritten by next line
	HRESULT hr = D3D11CreateDeviceAndSwapChain(
		0,
		mDriverType,
		0,
		createDeviceFlags,
		0,
		0,
		D3D11_SDK_VERSION,
		&swapChainDesc,
		&mSwapChain,
		&mDevice,
		&mFeatureLevel,
		&mDeviceContext);

	// Handle any device creation or DirectX version errors
	if (FAILED(hr))
	{
		MessageBox(0, L"D3D11CreateDevice Failed", 0, 0);
		return 1;
	}

	// Check for 4X MSAA quality support
	HR(mDevice->CheckMultisampleQualityLevels(
		DXGI_FORMAT_R8G8B8A8_UNORM,
		4,
		&mMSAA4xQuality));
	assert(mMSAA4xQuality > 0); // Potential problem if quality is 0

	HR(mSwapChain->SetFullscreenState(mFullScreen, NULL));
	// The remaining steps also need to happen each time the window
	// is resized, so just run the OnResize method
	VOnResize();

	return 0;
}

void DX3D11Renderer::VOnResize()
{
	// Release the views, since we'll be destroying
	// the corresponding buffers.
	ReleaseMacro(mRenderTargetView);
	ReleaseMacro(mDepthStencilView);
	ReleaseMacro(mDepthStencilBuffer);

	// Resize the swap chain to match the window and
	// recreate the render target view
	HR(mSwapChain->ResizeBuffers(
		1,
		mWindowWidth,
		mWindowHeight,
		DXGI_FORMAT_R8G8B8A8_UNORM,
		0));
	ID3D11Texture2D* backBuffer;
	HR(mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)));
	HR(mDevice->CreateRenderTargetView(backBuffer, 0, &mRenderTargetView));
	ReleaseMacro(backBuffer);

	VCreateDepthStencilTexture2D(&mDepthStencilBuffer);
	HR(mDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));

	// Bind these views to the pipeline, so rendering actually
	// uses the underlying textures
	mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	// Update the viewport and set it on the device
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = static_cast<float>(mWindowWidth);
	mViewport.Height = static_cast<float>(mWindowHeight);
	mViewport.MinDepth = 0.0f;
	mViewport.MaxDepth = 1.0f;
	mDeviceContext->RSSetViewports(1, &mViewport);

	if (mDelegate) {
		mDelegate->VOnResize();
	}
}

void DX3D11Renderer::VUpdateScene(const double& milliseconds)
{

}

void DX3D11Renderer::VRenderScene()
{

}

void DX3D11Renderer::VShutdown()
{
	// Useful for debugging COM trash left behind
	//ID3D11Debug* DebugDevice;
	//HRESULT Result = mDevice->QueryInterface(__uuidof(ID3D11Debug), reinterpret_cast<void**>(&DebugDevice));
	//DebugDevice->ReportLiveDeviceObjects(D3D11_RLDO_DETAIL);
	//ReleaseMacro(DebugDevice);

	ReleaseMacro(mDevice);
	ReleaseMacro(mDeviceContext);
	ReleaseMacro(mDepthStencilBuffer);
	ReleaseMacro(mDepthStencilView);
	ReleaseMacro(mRenderTargetView);
	ReleaseMacro(mSwapChain);
}

void DX3D11Renderer::VSetPrimitiveType(GPUPrimitiveType type)
{
	mDeviceContext->IASetPrimitiveTopology(GD3D11_Primitive_Type_Map(type));
}

void DX3D11Renderer::VDrawIndexed(GPUPrimitiveType type, uint32_t startIndex, uint32_t count)
{
	mDeviceContext->IASetPrimitiveTopology(GD3D11_Primitive_Type_Map(type));
	mDeviceContext->DrawIndexed(
		count,
		startIndex,
		0);
}

void DX3D11Renderer::VDrawIndexed(uint32_t startIndex, uint32_t count)
{
	mDeviceContext->DrawIndexed(
		count,
		startIndex,
		0);
}

#pragma region ID3D11Buffer

void DX3D11Renderer::VCreateVertexBuffer(void* buffer, void* vertices, const size_t& size)
{
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DEFAULT;
	vbd.ByteWidth = size;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;	// Not used for Vertex Input

	D3D11_SUBRESOURCE_DATA* pVertexData = nullptr;
	if (vertices)
	{
		D3D11_SUBRESOURCE_DATA vertexData;
		vertexData.pSysMem = vertices;
		pVertexData = &vertexData;
	}

	mDevice->CreateBuffer(&vbd, pVertexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateStaticVertexBuffer(void* buffer, void* vertices, const size_t& size)
{
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	vbd.ByteWidth = size;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = 0;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;	// Not used for Vertex Input

	D3D11_SUBRESOURCE_DATA* pVertexData = nullptr;
	if (vertices)
	{
		D3D11_SUBRESOURCE_DATA vertexData;
		vertexData.pSysMem = vertices;
		pVertexData = &vertexData;
	}

	mDevice->CreateBuffer(&vbd, pVertexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateDynamicVertexBuffer(void* buffer, void* vertices, const size_t& size)
{
	D3D11_BUFFER_DESC vbd;
	vbd.Usage = D3D11_USAGE_DYNAMIC;
	vbd.ByteWidth = size;
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	vbd.MiscFlags = 0;
	vbd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pVertexData = nullptr;
	if (vertices)
	{
		D3D11_SUBRESOURCE_DATA vertexData;
		vertexData.pSysMem = vertices;
		pVertexData = &vertexData;
	}

	mDevice->CreateBuffer(&vbd, pVertexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = sizeof(uint16_t) * count;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;	// Not used for Vertex Input

	D3D11_SUBRESOURCE_DATA* pIndexData = nullptr;
	if (indices)
	{
		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = indices;
		pIndexData = &indexData;
	}

	mDevice->CreateBuffer(&ibd, pIndexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateStaticIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = sizeof(uint16_t) * count;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;	// Not used for Vertex Input

	D3D11_SUBRESOURCE_DATA* pIndexData = nullptr;
	if (indices)
	{
		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = indices;
		pIndexData = &indexData;
	}

	mDevice->CreateBuffer(&ibd, pIndexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateDynamicIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = sizeof(uint16_t) * count;
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;	// Not used for Vertex Input

	D3D11_SUBRESOURCE_DATA* pIndexData = nullptr;
	if (indices)
	{
		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = indices;
		pIndexData = &indexData;
	}

	mDevice->CreateBuffer(&ibd, pIndexData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateInstanceBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DEFAULT;
	ibd.ByteWidth = size;
	ibd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pInstanceData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA instanceData;
		instanceData.pSysMem = data;
		pInstanceData = &instanceData;
	}

	HRESULT hr = mDevice->CreateBuffer(&ibd, pInstanceData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateStaticInstanceBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	ibd.ByteWidth = size;
	ibd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ibd.CPUAccessFlags = 0;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pInstanceData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA instanceData;
		instanceData.pSysMem = data;
		pInstanceData = &instanceData;
	}

	mDevice->CreateBuffer(&ibd, pInstanceData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateDynamicInstanceBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC ibd;
	ibd.Usage = D3D11_USAGE_DYNAMIC;
	ibd.ByteWidth = size;
	ibd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	ibd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	ibd.MiscFlags = 0;
	ibd.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pInstanceData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA instanceData;
		instanceData.pSysMem = data;
		pInstanceData = &instanceData;
	}

	mDevice->CreateBuffer(&ibd, pInstanceData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateConstantBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = size;
	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pBufferData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA bufferData;
		bufferData.pSysMem = data;
		pBufferData = &bufferData;
	}
	mDevice->CreateBuffer(&bufferDesc, pBufferData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateStaticConstantBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = size;
	bufferDesc.Usage = D3D11_USAGE_IMMUTABLE;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = 0;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pBufferData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA bufferData;
		bufferData.pSysMem = data;
		pBufferData = &bufferData;
	}

	mDevice->CreateBuffer(&bufferDesc, pBufferData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VCreateDynamicConstantBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_BUFFER_DESC bufferDesc;
	bufferDesc.ByteWidth = size;
	bufferDesc.Usage = D3D11_USAGE_DYNAMIC;
	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bufferDesc.MiscFlags = 0;
	bufferDesc.StructureByteStride = 0;

	D3D11_SUBRESOURCE_DATA* pBufferData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA bufferData;
		bufferData.pSysMem = data;
		pBufferData = &bufferData;
	}

	mDevice->CreateBuffer(&bufferDesc, pBufferData, reinterpret_cast<ID3D11Buffer**>(buffer));
}

void DX3D11Renderer::VUpdateBuffer(void* buffer, void* data)
{
	mDeviceContext->UpdateSubresource(static_cast<ID3D11Buffer*>(buffer), 0, nullptr, data, 0, 0);
}

void DX3D11Renderer::VUpdateBuffer(void* buffer, void* data, const size_t& size)
{
	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	ZeroMemory(&mappedSubresource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ID3D11Buffer* mappedBuffer = static_cast<ID3D11Buffer*>(buffer);
	mDeviceContext->Map(mappedBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	memcpy(mappedSubresource.pData, data, size);
	mDeviceContext->Unmap(mappedBuffer, 0);
}

#pragma endregion 

#pragma region Texture

void DX3D11Renderer::VCreateNativeFormat(void* nativeFormat, InputFormat textureFormat)
{
	DXGI_FORMAT& format = *static_cast<DXGI_FORMAT*>(nativeFormat);
	switch (textureFormat)
	{
	case R_FLOAT32:
		format = DXGI_FORMAT_R32_FLOAT;
		break;
	case RG_FLOAT32:
		format = DXGI_FORMAT_R32G32_FLOAT;
		break;
	case RGB_FLOAT32:
		format = DXGI_FORMAT_R32G32B32_FLOAT;
		break;
	case RGBA_FLOAT32:
		format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		break;
	case R_SNORM8:
		format = DXGI_FORMAT_R8_SNORM;
		break;
	case RG_SNORM8:
		format = DXGI_FORMAT_R8G8_SNORM;
		break;
	case RGBA_SNORM8:
		format = DXGI_FORMAT_R8G8B8A8_SNORM;
		break;
	case R_TYPELESS8:
		format = DXGI_FORMAT_R8_TYPELESS;
		break;
	case RG_TYPELESS8:
		format = DXGI_FORMAT_R8G8_TYPELESS;
		break;
	case RGBA_TYPELESS8:
		format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
		break;
	}
}

void DX3D11Renderer::VCreateTexture2D(void* texture, void* data, uint32_t mipLevels, InputFormat textureFormat)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = GetWindowWidth();
	texture2DDesc.Height = GetWindowHeight();
	texture2DDesc.ArraySize = 1;
	texture2DDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.MipLevels = mipLevels;
	texture2DDesc.MiscFlags = 0;
	texture2DDesc.SampleDesc.Count = 1;
	texture2DDesc.SampleDesc.Quality = 0;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;

	VCreateNativeFormat(&texture2DDesc.Format, textureFormat);

	D3D11_SUBRESOURCE_DATA* pTextureData = nullptr;
	if (data)
	{
		D3D11_SUBRESOURCE_DATA textureData;
		textureData.pSysMem = data;
		pTextureData = &textureData;
	}

	mDevice->CreateTexture2D(&texture2DDesc, pTextureData, reinterpret_cast<ID3D11Texture2D**>(texture));
}

void DX3D11Renderer::VCreateDepthTexture2D(void* texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = GetWindowWidth();
	texture2DDesc.Height = GetWindowHeight();
	texture2DDesc.ArraySize = 1;
	texture2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.Format = DXGI_FORMAT_R8_UNORM;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.MiscFlags = 0;
	texture2DDesc.SampleDesc.Count = 1;
	texture2DDesc.SampleDesc.Quality = 0;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

void DX3D11Renderer::VCreateDepthResourceTexture2D(void * texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = GetWindowWidth();
	texture2DDesc.Height = GetWindowHeight();
	texture2DDesc.ArraySize = 1;
	texture2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.MiscFlags = 0;
	texture2DDesc.SampleDesc.Count = 1;
	texture2DDesc.SampleDesc.Quality = 0;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

void DX3D11Renderer::VCreateDepthStencilTexture2D(void* texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = mWindowWidth;
	texture2DDesc.Height = mWindowHeight;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.ArraySize = 1;
	texture2DDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;
	texture2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.MiscFlags = 0;
	if (mEnable4xMsaa)
	{
		// Turn on 4x MultiSample Anti Aliasing
		// This must match swap chain MSAA values
		texture2DDesc.SampleDesc.Count = 4;
		texture2DDesc.SampleDesc.Quality = mMSAA4xQuality - 1;
	}
	else
	{
		// No MSAA
		texture2DDesc.SampleDesc.Count = 1;
		texture2DDesc.SampleDesc.Quality = 0;
	}

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

void DX3D11Renderer::VCreateDepthStencilResourceTexture2D(void * texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = mWindowWidth;
	texture2DDesc.Height = mWindowHeight;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.ArraySize = 1;
	texture2DDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;
	texture2DDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.MiscFlags = 0;
	if (mEnable4xMsaa)
	{
		// Turn on 4x MultiSample Anti Aliasing
		// This must match swap chain MSAA values
		texture2DDesc.SampleDesc.Count = 4;
		texture2DDesc.SampleDesc.Quality = mMSAA4xQuality - 1;
	}
	else
	{
		// No MSAA
		texture2DDesc.SampleDesc.Count = 1;
		texture2DDesc.SampleDesc.Quality = 0;
	}

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

void DX3D11Renderer::VCreateRenderTexture2D(void* texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = GetWindowWidth();
	texture2DDesc.Height = GetWindowHeight();
	texture2DDesc.ArraySize = 1;
	texture2DDesc.BindFlags = D3D11_BIND_RENDER_TARGET;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.MiscFlags = 0;
	texture2DDesc.SampleDesc.Count = 1;
	texture2DDesc.SampleDesc.Quality = 0;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

void DX3D11Renderer::VCreateRenderResourceTexture2D(void * texture2D)
{
	D3D11_TEXTURE2D_DESC texture2DDesc;
	texture2DDesc.Width = GetWindowWidth();
	texture2DDesc.Height = GetWindowHeight();
	texture2DDesc.ArraySize = 1;
	texture2DDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	texture2DDesc.CPUAccessFlags = 0;
	texture2DDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	texture2DDesc.MipLevels = 1;
	texture2DDesc.MiscFlags = 0;
	texture2DDesc.SampleDesc.Count = 1;
	texture2DDesc.SampleDesc.Quality = 0;
	texture2DDesc.Usage = D3D11_USAGE_DEFAULT;

	mDevice->CreateTexture2D(&texture2DDesc, nullptr, reinterpret_cast<ID3D11Texture2D**>(texture2D));
}

#pragma endregion 

#pragma region SamplerState

void DX3D11Renderer::VCreateLinearClampSamplerState(void* samplerState)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	mDevice->CreateSamplerState(&samplerDesc, reinterpret_cast<ID3D11SamplerState**>(samplerState));
}

void DX3D11Renderer::VCreateLinearWrapSamplerState(void* samplerState)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	mDevice->CreateSamplerState(&samplerDesc, reinterpret_cast<ID3D11SamplerState**>(samplerState));
}

void DX3D11Renderer::VCreateLinearBorderSamplerState(void* samplerState, float* color)
{
	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.BorderColor[0] = color[0];
	samplerDesc.BorderColor[1] = color[1];
	samplerDesc.BorderColor[2] = color[2];
	samplerDesc.BorderColor[3] = color[3];
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	mDevice->CreateSamplerState(&samplerDesc, reinterpret_cast<ID3D11SamplerState**>(samplerState));
}

#pragma endregion 

#pragma region Mesh

void DX3D11Renderer::VSetMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mVertexStride = stride;
	VCreateVertexBuffer(&DXMesh->mVertexBuffer, vertices, size);
}

void DX3D11Renderer::VSetStaticMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mVertexStride = stride;
	VCreateStaticVertexBuffer(&DXMesh->mVertexBuffer, vertices, size);
}

void DX3D11Renderer::VSetDynamicMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mVertexStride = stride;
	VCreateDynamicVertexBuffer(&DXMesh->mVertexBuffer, vertices, size);
}

void DX3D11Renderer::VSetMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mIndexCount = count;
	VCreateIndexBuffer(&DXMesh->mIndexBuffer, indices, count);
}

void DX3D11Renderer::VSetStaticMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mIndexCount = count;
	VCreateStaticIndexBuffer(&DXMesh->mIndexBuffer, indices, count);
}

void DX3D11Renderer::VSetDynamicMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	DXMesh->mIndexCount = count;
	VCreateDynamicIndexBuffer(&DXMesh->mIndexBuffer, indices, count);
}

void DX3D11Renderer::VUpdateMeshVertexBuffer(IMesh* mesh, void* data, const size_t& size)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	VUpdateBuffer(DXMesh->mVertexBuffer, data, size);
}

void DX3D11Renderer::VUpdateMeshIndexBuffer(IMesh* mesh, void* data, const uint32_t& count)
{
	DX11Mesh* DXMesh = static_cast<DX11Mesh*>(mesh);
	VUpdateBuffer(DXMesh->mIndexBuffer, data, sizeof(uint16_t) * count);
}

void DX3D11Renderer::VBindMesh(IMesh* mesh)
{
	DX11Mesh* dxMesh = static_cast<DX11Mesh*>(mesh);
	uint32_t offset = 0;
	mDeviceContext->IASetVertexBuffers(0, 1, &dxMesh->mVertexBuffer, &dxMesh->mVertexStride, &offset);
	mDeviceContext->IASetIndexBuffer(dxMesh->mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
}

#pragma endregion

#pragma region Shader

void DX3D11Renderer::VCreateShader(IShader** shader, LinearAllocator* allocator)
{
	RIG_NEW(DX11Shader, allocator, *shader);
}

void DX3D11Renderer::VLoadVertexShader(IShader* vertexShader, const char* filename, InputElement* inputElements, const uint32_t& count)
{
	DX11Shader* dxShader = static_cast<DX11Shader*>(vertexShader);

	const wchar_t* wFilename;
	CSTR2WSTR(filename, wFilename);

	ID3DBlob* vsBlob;
	D3DReadFileToBlob(wFilename, &vsBlob);

	mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &dxShader->mVertexShader);
	
	// Input Layout
	D3D11_INPUT_ELEMENT_DESC* inputDescription = new D3D11_INPUT_ELEMENT_DESC[count];

	{
		for (uint32_t i = 0; i < count; i++)
		{
			inputDescription[i].SemanticName = inputElements[i].Name;
			inputDescription[i].SemanticIndex = inputElements[i].Index;
			inputDescription[i].InputSlot = inputElements[i].InputSlot;
			inputDescription[i].AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
			inputDescription[i].InstanceDataStepRate = inputElements[i].InstanceStepRate;

			switch (inputElements[i].Format)
			{
			case RG_FLOAT32:
				inputDescription[i].Format = DXGI_FORMAT_R32G32_FLOAT;
				break;
			case RGB_FLOAT32:
				inputDescription[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			case RGBA_FLOAT32:
				inputDescription[i].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
				break;
			default:
				inputDescription[i].Format = DXGI_FORMAT_R32_FLOAT;
				break;
			}

			switch (inputElements[i].InputSlotClass)
			{
			case INPUT_CLASS_PER_INSTANCE:
				inputDescription[i].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
				break;
			default:
				inputDescription[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				break;
			}
		}
	}

	mDevice->CreateInputLayout(inputDescription, count, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &dxShader->mInputLayout);

	ReleaseMacro(vsBlob);
	delete[] inputDescription;
}

void DX3D11Renderer::VLoadVertexShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator)
{
	//DX11Shader* dxShader = static_cast<DX11Shader*>(vertexShader);

	//const wchar_t* wFilename;
	//CSTR2WSTR(filename, wFilename);

	//ID3DBlob* vsBlob;
	//D3DReadFileToBlob(wFilename, &vsBlob);

	//ID3D11ShaderReflection* reflection = nullptr;
	//D3DReflect(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void**>(&reflection));

	//D3D11_SHADER_DESC shaderDesc;
	//reflection->GetDesc(&shaderDesc);

	//dxShader->CreateVertexShader(mDevice, vsBlob);
	//SetVertexShaderInputLayout(reflection, vsBlob, &shaderDesc, dxShader);
	//SetShaderConstantBuffers(reflection, &shaderDesc, dxShader, allocator);
	//SetShaderResources(reflection, &shaderDesc, dxShader);

	//ReleaseMacro(reflection);
	//ReleaseMacro(vsBlob);
}

void DX3D11Renderer::VLoadVertexShader(IShader* vertexShader, const char* filename)
{
	DX11Shader* dxShader = static_cast<DX11Shader*>(vertexShader);

	const wchar_t* wFilename;
	CSTR2WSTR(filename, wFilename);

	ID3DBlob* vsBlob;
	D3DReadFileToBlob(wFilename, &vsBlob);

	mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &dxShader->mVertexShader);

	ReleaseMacro(vsBlob);
}

void DX3D11Renderer::VLoadPixelShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator)
{

}

void DX3D11Renderer::VLoadPixelShader(IShader* pixelShader, const char* filename)
{
	DX11Shader* dxShader = reinterpret_cast<DX11Shader*>(pixelShader);

	const wchar_t* wFilename;
	CSTR2WSTR(filename, wFilename);

	ID3DBlob* psBlob;
	D3DReadFileToBlob(wFilename, &psBlob);

	mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &dxShader->mPixelShader);

	ReleaseMacro(psBlob);
}

void DX3D11Renderer::SetVertexShaderInputLayout(ID3D11ShaderReflection* reflection, ID3DBlob* vsBlob, D3D11_SHADER_DESC* shaderDesc, DX11Shader* vertexShader)
{
	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;
	for (UINT i = 0; i < shaderDesc->InputParameters; i++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC parameterDesc;
		reflection->GetInputParameterDesc(i, &parameterDesc);

		D3D11_INPUT_ELEMENT_DESC elementDesc;
		elementDesc.InputSlot				= 0;
		elementDesc.SemanticIndex			= parameterDesc.SemanticIndex;
		elementDesc.SemanticName			= parameterDesc.SemanticName;
		elementDesc.AlignedByteOffset		= D3D11_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass			= D3D11_INPUT_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate	= 0;

		if (parameterDesc.Mask == 1)
		{
			if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) { elementDesc.Format = DXGI_FORMAT_R32_UINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) { elementDesc.Format = DXGI_FORMAT_R32_SINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) { elementDesc.Format = DXGI_FORMAT_R32_FLOAT; }
		}
		else if (parameterDesc.Mask <= 3)
		{
			if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) { elementDesc.Format = DXGI_FORMAT_R32G32_UINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) { elementDesc.Format = DXGI_FORMAT_R32G32_SINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) { elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT; }
		}
		else if (parameterDesc.Mask <= 7)
		{
			if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT; }
		}
		else if (parameterDesc.Mask <= 15)
		{
			if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT; }
			else if (parameterDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) { elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT; }
		}

		inputLayoutDesc.push_back(elementDesc);
	}

	mDevice->CreateInputLayout(&inputLayoutDesc[0], inputLayoutDesc.size(), vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &vertexShader->mInputLayout);
}

void DX3D11Renderer::SetShaderConstantBuffers(ID3D11ShaderReflection* reflection, D3D11_SHADER_DESC* shaderDesc, DX11Shader* shader, LinearAllocator* allocator)
{
	//void* buffers = allocator->Allocate(sizeof(DX11ShaderBuffer) * shaderDesc->ConstantBuffers, alignof(DX11ShaderBuffer), 0);
	//shader->SetConstantBuffers(reinterpret_cast<DX11ShaderBuffer*>(buffers), shaderDesc->ConstantBuffers);

	//for (UINT i = 0; i < shaderDesc->ConstantBuffers; i++)
	//{
	//	ID3D11ShaderReflectionConstantBuffer* cBufferReflection = reflection->GetConstantBufferByIndex(i);

	//	D3D11_SHADER_BUFFER_DESC shaderBufferDesc;
	//	cBufferReflection->GetDesc(&shaderBufferDesc);

	//	DX11ShaderBuffer* shaderBuffer = shader->GetShaderBufferAtIndex(i);
	//	shaderBuffer->Data = allocator->Allocate(shaderBufferDesc.Size, GetAlignment(shaderBufferDesc.Size), 0);
	//	ZeroMemory(shaderBuffer->Data, shaderBufferDesc.Size);

	//	D3D11_BUFFER_DESC bufferDesc;
	//	bufferDesc.Usage = D3D11_USAGE_DEFAULT;
	//	bufferDesc.ByteWidth = shaderBufferDesc.Size;
	//	bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	//	bufferDesc.CPUAccessFlags = 0;
	//	bufferDesc.MiscFlags = 0;
	//	bufferDesc.StructureByteStride = 0;
	//	mDevice->CreateBuffer(&bufferDesc, nullptr, &shaderBuffer->ConstantBuffer);

	//	for (UINT j = 0; j < shaderBufferDesc.Variables; j++)
	//	{
	//		ID3D11ShaderReflectionVariable* variableReflection = cBufferReflection->GetVariableByIndex(j);

	//		D3D11_SHADER_VARIABLE_DESC variableDesc;
	//		variableReflection->GetDesc(&variableDesc);

	//		DX11ShaderVariable shaderVariable;
	//		shaderVariable.BufferIndex	= i;
	//		shaderVariable.ByteOffset	= variableDesc.StartOffset;
	//		shaderVariable.Size			= variableDesc.Size;
	//	}
	//}
}

void DX3D11Renderer::SetShaderResources(ID3D11ShaderReflection* reflection, D3D11_SHADER_DESC* shaderDesc, DX11Shader* shader)
{
	for (unsigned int r = 0; r < shaderDesc->BoundResources; r++)
	{
		// Get this resource's description
		D3D11_SHADER_INPUT_BIND_DESC resourceDesc;
		reflection->GetResourceBindingDesc(r, &resourceDesc);

		// Check the type
		switch (resourceDesc.Type)
		{
		case D3D_SIT_TEXTURE: // A texture resource
		//	textureTable.insert(std::pair<std::string, unsigned int>(resourceDesc.Name, resourceDesc.BindPoint));
			break;

		case D3D_SIT_SAMPLER: // A sampler resource
		//	samplerTable.insert(std::pair<std::string, unsigned int>(resourceDesc.Name, resourceDesc.BindPoint));
			break;
		}
	}
}

void DX3D11Renderer::VSetInputLayout(IShader* vertexShader)
{
	mDeviceContext->IASetInputLayout(static_cast<DX11Shader*>(vertexShader)->mInputLayout);
}

void DX3D11Renderer::VSetVertexShaderInputLayout(IShader* vertexShader)
{
	DX11Shader* shader = static_cast<DX11Shader*>(vertexShader);
	mDeviceContext->IASetInputLayout(shader->mInputLayout);
	mDeviceContext->VSSetShader(shader->mVertexShader, nullptr, 0);
}

void DX3D11Renderer::VSetVertexShader(IShader* shader)
{
	mDeviceContext->VSSetShader(static_cast<DX11Shader*>(shader)->mVertexShader, nullptr, 0);
}

void DX3D11Renderer::VSetPixelShader(IShader* shader)
{
	mDeviceContext->PSSetShader(static_cast<DX11Shader*>(shader)->mPixelShader, nullptr, 0);
}

#pragma endregion 

#pragma region Shader Resource

void DX3D11Renderer::VCreateShaderResource(IShaderResource** shaderResouce, LinearAllocator* allocator)
{
	RIG_NEW(DX11ShaderResource, allocator, *shaderResouce);
}

void DX3D11Renderer::VCreateShaderTextures2D(IShaderResource* shader, const char** filenames, const uint32_t count)
{
	std::vector<ID3D11ShaderResourceView*> SRVs(count);
	for (uint32_t i = 0; i < count; i++)
	{
		const wchar_t* wFilename;
		CSTR2WSTR(filenames[i], wFilename);
		DirectX::CreateWICTextureFromFile(mDevice, wFilename, nullptr, &SRVs[i]);
	}

	reinterpret_cast<DX11ShaderResource*>(shader)->SetShaderResourceViews(SRVs);
}

void DX3D11Renderer::VCreateShaderContextTextures2D(IShaderResource* shader, IRenderContext* context)
{
	DX11RenderContext* dx11RenderContext = reinterpret_cast<DX11RenderContext*>(context);
	ID3D11Texture2D** textures = dx11RenderContext->GetRenderTextures();

	std::vector<ID3D11ShaderResourceView*> SRVs(dx11RenderContext->GetRenderTextureCount());

	for (uint32_t i = 0; i < dx11RenderContext->GetRenderTextureCount(); i++)
	{
		D3D11_TEXTURE2D_DESC texture2DDesc;
		textures[i]->GetDesc(&texture2DDesc);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = texture2DDesc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		SRVDesc.Texture2D.MostDetailedMip = 0;

		mDevice->CreateShaderResourceView(textures[i], &SRVDesc, &SRVs[i]);
	}

	reinterpret_cast<DX11ShaderResource*>(shader)->SetShaderResourceViews(SRVs);
}

void DX3D11Renderer::VAddShaderTextures2D(IShaderResource* shader, const char** filenames, const uint32_t count)
{
	std::vector<ID3D11ShaderResourceView*> SRVs(count);
	for (uint32_t i = 0; i < count; i++)
	{
		const wchar_t* wFilename;
		CSTR2WSTR(filenames[i], wFilename);
		DirectX::CreateWICTextureFromFile(mDevice, wFilename, nullptr, &SRVs[i]);
	}

	reinterpret_cast<DX11ShaderResource*>(shader)->AddShaderResourceViews(SRVs);
}

void DX3D11Renderer::VCreateShaderTextureCubes(IShaderResource* shader, const char** filenames, const uint32_t count)
{
	std::vector<ID3D11ShaderResourceView*> SRVs(count);
	for (uint32_t i = 0; i < count; i++)
	{
		const wchar_t* wFilename;
		CSTR2WSTR(filenames[i], wFilename);
		DirectX::CreateDDSTextureFromFile(mDevice, wFilename, nullptr, &SRVs[i]);
	}

	reinterpret_cast<DX11ShaderResource*>(shader)->SetShaderResourceViews(SRVs);
}
	
void DX3D11Renderer::VCreateShaderConstantBuffers(IShaderResource* shader, void** data, size_t* sizes, const uint32_t& count)
{
	std::vector<ID3D11Buffer*> constantBuffers(count);

	for (uint32_t i = 0; i < count; i++)
	{
		VCreateConstantBuffer(&constantBuffers[i], data[i], sizes[i]);
	}

	static_cast<DX11ShaderResource*>(shader)->SetConstantBuffers(constantBuffers);
}

void DX3D11Renderer::VCreateShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
{
	std::vector<ID3D11Buffer*> instanceBuffers(count);
	std::vector<UINT> instanceBufferStrides(count);
	std::vector<UINT> instanceBufferOffsets(count);

	for (uint32_t i = 0; i < count; i++)
	{
		VCreateInstanceBuffer(&instanceBuffers[i], data[i], sizes[i]);
		instanceBufferStrides[i] = strides[i];
		instanceBufferOffsets[i] = offsets[i];
	}

	static_cast<DX11ShaderResource*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VCreateStaticShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
{
	std::vector<ID3D11Buffer*> instanceBuffers(count);
	std::vector<UINT> instanceBufferStrides(count);
	std::vector<UINT> instanceBufferOffsets(count);

	for (uint32_t i = 0; i < count; i++)
	{
		VCreateStaticInstanceBuffer(&instanceBuffers[i], data[i], sizes[i]);
		instanceBufferStrides[i] = strides[i];
		instanceBufferOffsets[i] = offsets[i];
	}

	static_cast<DX11ShaderResource*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VCreateDynamicShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
{
	std::vector<ID3D11Buffer*> instanceBuffers(count);
	std::vector<UINT> instanceBufferStrides(count);
	std::vector<UINT> instanceBufferOffsets(count);

	for (uint32_t i = 0; i < count; i++)
	{
		VCreateDynamicInstanceBuffer(&instanceBuffers[i], data[i], sizes[i]);
		instanceBufferStrides[i] = strides[i];
		instanceBufferOffsets[i] = offsets[i];
	}

	static_cast<DX11ShaderResource*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VUpdateShaderConstantBuffer(IShaderResource* shader, void* data, const uint32_t& index)
{
	ID3D11Buffer** constantBuffers = static_cast<DX11ShaderResource*>(shader)->GetConstantBuffers();
	mDeviceContext->UpdateSubresource(constantBuffers[index], 0, nullptr, data, 0, 0);
}

void DX3D11Renderer::VUpdateShaderInstanceBuffer(IShaderResource* shader, void* data, const size_t& size, const uint32_t& index)
{
	ID3D11Buffer** instanceBuffers = static_cast<DX11ShaderResource*>(shader)->GetInstanceBuffers();

	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	ZeroMemory(&mappedSubresource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ID3D11Buffer* mappedBuffer = static_cast<ID3D11Buffer*>(instanceBuffers[index]);
	mDeviceContext->Map(mappedBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	memcpy(mappedSubresource.pData, data, size);
	mDeviceContext->Unmap(mappedBuffer, 0);
}

void DX3D11Renderer::VSetVertexShaderConstantBuffers(IShaderResource* shaderResource)
{
	DX11ShaderResource* resource = static_cast<DX11ShaderResource*>(shaderResource);

	uint8_t constBufferCount = resource->GetConstantBufferCount();
	if (constBufferCount)
	{
		mDeviceContext->VSSetConstantBuffers(0, constBufferCount, resource->GetConstantBuffers());
	}
}

void DX3D11Renderer::VSetVertexShaderConstantBuffer(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex)
{
	ID3D11Buffer** constantBuffers = static_cast<DX11ShaderResource*>(shaderResource)->GetConstantBuffers();
	mDeviceContext->VSSetConstantBuffers(toBindingIndex, 1, &constantBuffers[atIndex]);
}

void DX3D11Renderer::VSetPixelShaderConstantBuffers(IShaderResource* shaderResource)
{
	DX11ShaderResource* resource = static_cast<DX11ShaderResource*>(shaderResource);

	uint8_t constBufferCount = resource->GetConstantBufferCount();
	if (constBufferCount)
	{
		mDeviceContext->PSSetConstantBuffers(0, constBufferCount, resource->GetConstantBuffers());
	}
}

void DX3D11Renderer::VSetPixelShaderConstantBuffer(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex)
{
	ID3D11Buffer** constantBuffers = static_cast<DX11ShaderResource*>(shaderResource)->GetConstantBuffers();
	mDeviceContext->PSSetConstantBuffers(toBindingIndex, 1, &constantBuffers[atIndex]);
}

void DX3D11Renderer::VSetVertexShaderInstanceBuffers(IShaderResource* shaderResource)
{
	DX11ShaderResource* resource = static_cast<DX11ShaderResource*>(shaderResource);

	uint8_t instanceBufferCount = resource->GetInstanceBufferCount();
	if (instanceBufferCount)
	{
		mDeviceContext->IASetVertexBuffers(1, instanceBufferCount, resource->GetInstanceBuffers(), resource->GetInstanceBufferStrides(), resource->GetInstanceBufferOffsets());
	}
}

void DX3D11Renderer::VSetPixelShaderResourceViews(IShaderResource* shaderResource)
{
	DX11ShaderResource* resource = static_cast<DX11ShaderResource*>(shaderResource);

	uint8_t srvCount = resource->GetShaderResourceViewCount();
	if (srvCount)
	{
		mDeviceContext->PSSetShaderResources(0, srvCount, resource->GetShaderResourceViews());
	}
}

void DX3D11Renderer::VSetPixelShaderResourceView(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex)
{
	ID3D11ShaderResourceView** SRVs = static_cast<DX11ShaderResource*>(shaderResource)->GetShaderResourceViews();
	mDeviceContext->PSSetShaderResources(toBindingIndex, 1, &SRVs[atIndex]);
}

void DX3D11Renderer::VAddShaderLinearSamplerState(IShaderResource* shaderResource, SamplerStateAddressType addressType, float* color)
{
	ID3D11SamplerState* samplerState = nullptr;

	switch(addressType)
	{
	case SAMPLER_STATE_ADDRESS_CLAMP:
	{
		VCreateLinearClampSamplerState(&samplerState);
		break;
	}
	case SAMPLER_STATE_ADDRESS_WRAP:
	{
		VCreateLinearWrapSamplerState(&samplerState);
		break;
	}
	case SAMPLER_STATE_ADDRESS_BORDER:
	{
		VCreateLinearBorderSamplerState(&samplerState, color);
		break;
	}
	default:
		break;
	}

	static_cast<DX11ShaderResource*>(shaderResource)->AddSamplerState(samplerState);
}

void DX3D11Renderer::VSetPixelShaderSamplerStates(IShaderResource* shaderResource)
{
	DX11ShaderResource* resource = static_cast<DX11ShaderResource*>(shaderResource);

	uint8_t samplerStateCount = resource->GetSamplerStateCount();
	if (samplerStateCount)
	{
		mDeviceContext->PSSetSamplers(0, samplerStateCount, resource->GetSamplerStates());
	}
}

#pragma endregion 

#pragma region Render Context

void DX3D11Renderer::VCreateRenderContext(IRenderContext** renderContext, LinearAllocator* allocator)
{
	RIG_NEW(DX11RenderContext, allocator, *renderContext);
}

void DX3D11Renderer::VCreateContextDepthStencilTarget(IRenderContext* renderContext)
{
	ID3D11Texture2D* texture2D;
	VCreateDepthStencilTexture2D(&texture2D);

	D3D11_TEXTURE2D_DESC textureDesc;
	texture2D->GetDesc(&textureDesc);
		
	ID3D11DepthStencilView* DSV;

	mDevice->CreateDepthStencilView(texture2D, nullptr, &DSV);

	reinterpret_cast<DX11RenderContext*>(renderContext)->SetDepthStencilView(DSV);

	ReleaseMacro(texture2D);
}

void DX3D11Renderer::VCreateContextTargets(IRenderContext* renderContext, const uint32_t& count)
{
	std::vector<ID3D11RenderTargetView*> RTVs(count);

	for (uint32_t i = 0; i < count; i++)
	{
		ID3D11Texture2D* texture2D;
		VCreateRenderTexture2D(&texture2D);

		D3D11_TEXTURE2D_DESC textureDesc;
		texture2D->GetDesc(&textureDesc);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = textureDesc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		mDevice->CreateRenderTargetView(texture2D, &rtvDesc, &RTVs[i]);

		ReleaseMacro(texture2D);
	}

	reinterpret_cast<DX11RenderContext*>(renderContext)->SetRenderTargetViews(RTVs);
}

void DX3D11Renderer::VCreateContextDepthResourceTarget(IRenderContext* renderContext)
{
	ID3D11Texture2D* texture2D;
	VCreateDepthResourceTexture2D(&texture2D);

	D3D11_DEPTH_STENCIL_VIEW_DESC DSVDesc;
	DSVDesc.Flags = 0;
	DSVDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	DSVDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	DSVDesc.Texture2D.MipSlice = 0;

	ID3D11DepthStencilView* DSV;

	mDevice->CreateDepthStencilView(texture2D, &DSVDesc, &DSV);

	D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
	SRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
	SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	SRVDesc.Texture2D.MipLevels = 1;
	SRVDesc.Texture2D.MostDetailedMip = 0;

	ID3D11ShaderResourceView* SRV;
	mDevice->CreateShaderResourceView(texture2D, &SRVDesc, &SRV);

	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	context->SetDepthStencilView(DSV);
	context->SetDepthStencilResourceView(SRV);
	ReleaseMacro(texture2D);
}

void DX3D11Renderer::VCreateContextResourceTargets(IRenderContext* renderContext, const uint32_t& count)
{
	std::vector<ID3D11RenderTargetView*> RTVs(count);
	std::vector<ID3D11ShaderResourceView*> SRVs(count);

	for (uint32_t i = 0; i < count; i++)
	{
		ID3D11Texture2D* texture2D;
		VCreateRenderResourceTexture2D(&texture2D);

		D3D11_TEXTURE2D_DESC textureDesc;
		texture2D->GetDesc(&textureDesc);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = textureDesc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		mDevice->CreateRenderTargetView(texture2D, &rtvDesc, &RTVs[i]);

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = textureDesc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		SRVDesc.Texture2D.MipLevels = 1;
		SRVDesc.Texture2D.MostDetailedMip = 0;

		mDevice->CreateShaderResourceView(texture2D, &SRVDesc, &SRVs[i]);

		ReleaseMacro(texture2D);
	}

	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	context->SetRenderTargetViews(RTVs);
	context->SetShaderResourceViews(SRVs);
}

void DX3D11Renderer::VSetContextTarget()
{
	mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, nullptr);
}

void DX3D11Renderer::VSetContextTargetWithDepth()
{
	mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
}

void DX3D11Renderer::VSetRenderContextTargets(IRenderContext* renderContext)
{
	DX11RenderContext* dx11RenderContext = reinterpret_cast<DX11RenderContext*>(renderContext);
	mDeviceContext->OMSetRenderTargets(dx11RenderContext->GetRenderTargetViewCount(), dx11RenderContext->GetRenderTargetViews(), nullptr);
}

void DX3D11Renderer::VSetRenderContextTargetsWithDepth(IRenderContext* renderContext)
{
	DX11RenderContext* dx11RenderContext = reinterpret_cast<DX11RenderContext*>(renderContext);
	mDeviceContext->OMSetRenderTargets(dx11RenderContext->GetRenderTargetViewCount(), dx11RenderContext->GetRenderTargetViews(), dx11RenderContext->GetDepthStencilView());
}

void DX3D11Renderer::VSetRenderContextTarget(IRenderContext* renderContext, const uint32_t& atIndex)
{
	ID3D11RenderTargetView** RTVs = static_cast<DX11RenderContext*>(renderContext)->GetRenderTargetViews();
	mDeviceContext->OMSetRenderTargets(1, &RTVs[atIndex], nullptr);
}

void DX3D11Renderer::VSetRenderContextTargetWithDepth(IRenderContext* renderContext, const uint32_t& atIndex)
{
	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	ID3D11RenderTargetView** RTVs = context->GetRenderTargetViews();
	mDeviceContext->OMSetRenderTargets(1, &RTVs[atIndex], context->GetDepthStencilView());
}

void DX3D11Renderer::VClearContext(const float* color, float depth, uint8_t stencil)
{
	mDeviceContext->ClearRenderTargetView(mRenderTargetView, color);
	mDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void DX3D11Renderer::VClearContext(IRenderContext* renderContext, const float* color, float depth, uint8_t stencil)
{
	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	ID3D11RenderTargetView** RTVs = context->GetRenderTargetViews();
	uint32_t RTVCount = context->GetRenderTargetViewCount();
	
	for (uint32_t i = 0; i < RTVCount; i++)
	{
		mDeviceContext->ClearRenderTargetView(RTVs[i], color);
	}

	mDeviceContext->ClearDepthStencilView(context->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void DX3D11Renderer::VClearContextTarget(const float* color)
{
	mDeviceContext->ClearRenderTargetView(mRenderTargetView, color);
}

void DX3D11Renderer::VClearContextTarget(IRenderContext* renderContext, const uint32_t& atIndex, const float* color)
{
	ID3D11RenderTargetView** RTVs = static_cast<DX11RenderContext*>(renderContext)->GetRenderTargetViews();
	mDeviceContext->ClearRenderTargetView(RTVs[atIndex], color);
}

void DX3D11Renderer::VClearDepthStencil(float depth, uint8_t stencil)
{
	mDeviceContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void DX3D11Renderer::VClearDepthStencil(IRenderContext* renderContext, float depth, uint8_t stencil)
{
	mDeviceContext->ClearDepthStencilView(static_cast<DX11RenderContext*>(renderContext)->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
}

void DX3D11Renderer::VSetVertexShaderDepthResourceView(IRenderContext* renderContext, const uint32_t& toBindingIndex)
{
	ID3D11ShaderResourceView* SRVs[] = { static_cast<DX11RenderContext*>(renderContext)->GetDepthStencilResourceView() };
	mDeviceContext->VSSetShaderResources(toBindingIndex, 1, SRVs);
}

void DX3D11Renderer::VSetPixelShaderDepthResourceView(IRenderContext* renderContext, const uint32_t& toBindingIndex)
{
	ID3D11ShaderResourceView* SRVs[] = { static_cast<DX11RenderContext*>(renderContext)->GetDepthStencilResourceView() };
	mDeviceContext->PSSetShaderResources(toBindingIndex, 1, SRVs);
}

void DX3D11Renderer::VSetVertexShaderResourceViews(IRenderContext* renderContext)
{
	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	mDeviceContext->VSSetShaderResources(0, context->GetShaderResourceViewCount(), context->GetShaderResourceViews());
}

void DX3D11Renderer::VSetPixelShaderResourceViews(IRenderContext* renderContext)
{
	DX11RenderContext* context = static_cast<DX11RenderContext*>(renderContext);
	mDeviceContext->PSSetShaderResources(0, context->GetShaderResourceViewCount(), context->GetShaderResourceViews());
}

void DX3D11Renderer::VSetVertexShaderResourceView(IRenderContext* renderContext, const uint32_t& atIndex, const uint32_t& toBindingIndex)
{
	ID3D11ShaderResourceView** SRVs = static_cast<DX11RenderContext*>(renderContext)->GetShaderResourceViews();
	mDeviceContext->VSSetShaderResources(toBindingIndex, 1, &SRVs[atIndex]);
}

void DX3D11Renderer::VSetPixelShaderResourceView(IRenderContext* renderContext, const uint32_t& atIndex, const uint32_t& toBindingIndex)
{
	ID3D11ShaderResourceView** SRVs = static_cast<DX11RenderContext*>(renderContext)->GetShaderResourceViews();
	mDeviceContext->PSSetShaderResources(toBindingIndex, 1, &SRVs[atIndex]);
}

#pragma endregion 

void DX3D11Renderer::VSwapBuffers()
{
	mSwapChain->Present(0, 0);
}

#pragma region IObserver

void DX3D11Renderer::HandleEvent(const IEvent& iEvent)
{
	const WMEvent& wmEvent = static_cast<const WMEvent&>(iEvent);
	switch (wmEvent.msg)
	{
	case WM_SIZE:
		// Save the new client area dimensions.
		mWindowWidth = LOWORD(wmEvent.lparam);
		mWindowHeight = HIWORD(wmEvent.lparam);
		if (mDevice)
		{
			if (wmEvent.wparam == SIZE_MINIMIZED)
			{
				mIsPaused = true;
				mIsMinimized = true;
				mIsMaximized = false;
			}
			else if (wmEvent.wparam == SIZE_MAXIMIZED)
			{
				mIsPaused = false;
				mIsMinimized = false;
				mIsMaximized = true;
				VOnResize();
			}
			else if (wmEvent.wparam == SIZE_RESTORED)
			{
				// Restoring from minimized state?
				if (mIsMinimized)
				{
					mIsPaused = false;
					mIsMinimized = false;
					VOnResize();
				}

				// Restoring from maximized state?
				else if (mIsMaximized)
				{
					mIsPaused = false;
					mIsMaximized = false;
					VOnResize();
				}
				else if (mIsResizing)
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					VOnResize();
				}
			}
		}
		break;
		// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mIsPaused = true;
		mIsResizing = true;
		//timer.Stop();
		break;

		// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
		// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mIsPaused = false;
		mIsResizing = false;
		//timer.Start();
		VOnResize();
		break;
	default:
		break;
	}
}

#pragma endregion 

ID3D11Device* DX3D11Renderer::GetDevice() const
{
	return mDevice;
}

ID3D11DeviceContext* DX3D11Renderer::GetDeviceContext() const
{
	return mDeviceContext;
}

IDXGISwapChain*	DX3D11Renderer::GetSwapChain()	const
{
	return mSwapChain;
}

ID3D11Texture2D* DX3D11Renderer::GetDepthStencilBuffer() const
{
	return mDepthStencilBuffer;
}

ID3D11RenderTargetView* const* DX3D11Renderer::GetRenderTargetView() const
{
	return &mRenderTargetView;
}

ID3D11DepthStencilView* DX3D11Renderer::GetDepthStencilView()	const
{
	return mDepthStencilView;
}

D3D11_VIEWPORT const& DX3D11Renderer::GetViewport() const
{
	return mViewport;
}
