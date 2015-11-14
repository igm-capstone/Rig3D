#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Graphics\DirectX11\DX11Mesh.h"
#include "Graphics/DirectX11/DX11Shader.h"
#include "Rig3D/Common/Traits.h"
#include "Rig3D\Engine.h"
#include "rig_defines.h"
#include "Rig3D\Graphics\Interface\IScene.h"
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

	// Set up the description of the texture to use for the
	// depth stencil buffer
	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = mWindowWidth;
	depthStencilDesc.Height = mWindowHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;
	if (mEnable4xMsaa)
	{
		// Turn on 4x MultiSample Anti Aliasing
		// This must match swap chain MSAA values
		depthStencilDesc.SampleDesc.Count = 4;
		depthStencilDesc.SampleDesc.Quality = mMSAA4xQuality - 1;
	}
	else
	{
		// No MSAA
		depthStencilDesc.SampleDesc.Count = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
	}

	// Create the depth/stencil buffer and corresponding view
	HR(mDevice->CreateTexture2D(&depthStencilDesc, 0, &mDepthStencilBuffer));
	HR(mDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));

	// Bind these views to the pipeline, so rendering actually
	// uses the underlying textures
	mDeviceContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);

	// Update the viewport and set it on the device
	mViewport.TopLeftX = 0;
	mViewport.TopLeftY = 0;
	mViewport.Width = (float)mWindowWidth;
	mViewport.Height = (float)mWindowHeight;
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

	mDevice->CreateBuffer(&ibd, pInstanceData, reinterpret_cast<ID3D11Buffer**>(&buffer));
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
			case FLOAT2:
				inputDescription[i].Format = DXGI_FORMAT_R32G32_FLOAT;
				break;
			case FLOAT3:
				inputDescription[i].Format = DXGI_FORMAT_R32G32B32_FLOAT;
				break;
			case FLOAT4:
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

void DX3D11Renderer::VSetInstanceBuffers(IShader* vertexShader)
{
	DX11Shader* shader = static_cast<DX11Shader*>(vertexShader);

	uint8_t instanceBufferCount = shader->GetInstanceBufferCount();
	if (instanceBufferCount)
	{
		mDeviceContext->IASetVertexBuffers(1, instanceBufferCount, shader->GetInstanceBuffers(), shader->GetInstanceBufferStrides(), shader->GetInstanceBufferOffsets());
	}
}

void DX3D11Renderer::VSetVertexShaderInputLayout(IShader* vertexShader)
{
	DX11Shader* shader = static_cast<DX11Shader*>(vertexShader);
	mDeviceContext->IASetInputLayout(shader->mInputLayout);
	mDeviceContext->VSSetShader(shader->mVertexShader, nullptr, 0);
}

void DX3D11Renderer::VSetVertexShaderResources(IShader* vertexShader)
{
	DX11Shader* shader = static_cast<DX11Shader*>(vertexShader);

	uint8_t constBufferCount = shader->GetConstantBufferCount();
	if (constBufferCount)
	{
		mDeviceContext->VSSetConstantBuffers(0, constBufferCount, shader->GetConstantBuffers());
	}

	uint8_t srvCount = shader->GetShaderResourceViewCount();
	if (srvCount)
	{
		mDeviceContext->VSSetShaderResources(0, srvCount, shader->GetShaderResourceViews());
	}

	uint8_t samplerStateCount = shader->GetSamplerStateCount();
	if (samplerStateCount)
	{
		mDeviceContext->VSSetSamplers(0, samplerStateCount, shader->GetSamplerStates());
	}
}

void DX3D11Renderer::VSetVertexShader(IShader* shader)
{
	mDeviceContext->VSSetShader(static_cast<DX11Shader*>(shader)->mVertexShader, nullptr, 0);
}

void DX3D11Renderer::VSetPixelShader(IShader* shader)
{
	mDeviceContext->PSSetShader(static_cast<DX11Shader*>(shader)->mPixelShader, nullptr, 0);
}

void DX3D11Renderer::VCreateShaderConstantBuffers(IShader* shader, void** data, size_t* sizes, const uint32_t& count)
{
	std::vector<ID3D11Buffer*> constantBuffers(count);

	for (uint32_t i = 0; i < count; i++)
	{
		VCreateConstantBuffer(&constantBuffers[i], data[i], sizes[i]);
	}

	static_cast<DX11Shader*>(shader)->SetConstantBuffers(constantBuffers);
}

void DX3D11Renderer::VCreateShaderInstanceBuffers(IShader* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
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

	static_cast<DX11Shader*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VCreateStaticShaderInstanceBuffers(IShader* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
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

	static_cast<DX11Shader*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VCreateDynamicShaderInstanceBuffers(IShader* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count)
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

	static_cast<DX11Shader*>(shader)->SetInstanceBuffers(instanceBuffers, instanceBufferStrides, instanceBufferOffsets);
}

void DX3D11Renderer::VUpdateShaderConstantBuffer(IShader* shader, void* data, const uint32_t& index)
{
	ID3D11Buffer** constantBuffers = static_cast<DX11Shader*>(shader)->GetConstantBuffers();
	mDeviceContext->UpdateSubresource(constantBuffers[index], 0, nullptr, data, 0, 0);
}

void DX3D11Renderer::VUpdateShaderInstanceBuffer(IShader* shader, void* data, const size_t& size, const uint32_t& index)
{
	ID3D11Buffer** instanceBuffers = static_cast<DX11Shader*>(shader)->GetInstanceBuffers();

	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	ZeroMemory(&mappedSubresource, sizeof(D3D11_MAPPED_SUBRESOURCE));

	ID3D11Buffer* mappedBuffer = static_cast<ID3D11Buffer*>(instanceBuffers[index]);
	mDeviceContext->Map(mappedBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
	memcpy(mappedSubresource.pData, data, size);
	mDeviceContext->Unmap(mappedBuffer, 0);
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
