#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\IScene.h"
#include "Rig3D\DX3D11Renderer.h"
#include "Rig3D\Transform.h"
#include "Rig3D\Input.h"
#include <d3d11.h>
#include <d3dcompiler.h>

using namespace Rig3D;

static const int VERTEX_COUNT	= 3;
static const int INDEX_COUNT	= 3;

class Rig3DSampleScene : public IScene
{
public:

	struct SampleVertex
	{
		vec3f mPosition;
		vec3f mColor;
	};

	struct SampleMatrixBuffer
	{
		mat4f mWorld;
		mat4f mView;
		mat4f mProjection;
	};
	
	SampleMatrixBuffer		mMatrixBuffer;
	SampleVertex			mVertices[VERTEX_COUNT];
	uint16_t				mIndices[INDEX_COUNT];

	DX3D11Renderer*			mRenderer;
	ID3D11Device*			mDevice;
	ID3D11DeviceContext*	mDeviceContext;
	ID3D11Buffer*			mVertexBuffer;
	ID3D11Buffer*			mIndexBuffer;
	ID3D11Buffer*			mConstantBuffer;
	ID3D11InputLayout*		mInputLayout;
	ID3D11VertexShader*		mVertexShader;
	ID3D11PixelShader*		mPixelShader;

	Rig3DSampleScene()
	{
		mWindowCaption	= "Rig3D Sample";
		mWindowWidth	= 800;
		mWindowHeight	= 600;
	}

	~Rig3DSampleScene()
	{

	}

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

		InitializeGeometry();
		InitializeShaders();
		InitializeCamera();
	}

	void InitializeGeometry()
	{
		mVertices[0].mPosition	= { +0.0f, +0.5f, +0.0f };
		mVertices[0].mColor		= { +1.0f, +0.0f, +0.0f };
		mVertices[1].mPosition	= { +0.45f, -0.5f, +0.0f };
		mVertices[1].mColor		= { +0.0f, +1.0f, +0.0f };
		mVertices[2].mPosition	= { -0.45f, -0.5f, +0.0f};
		mVertices[2].mColor		= { +0.0f, +0.0f, +1.0f };

		D3D11_BUFFER_DESC vbd;
		vbd.Usage = D3D11_USAGE_IMMUTABLE;
		vbd.ByteWidth = sizeof(SampleVertex) * VERTEX_COUNT;
		vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbd.CPUAccessFlags = 0;
		vbd.MiscFlags = 0;
		vbd.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA vertexData;
		vertexData.pSysMem = mVertices;
		mDevice->CreateBuffer(&vbd, &vertexData, &mVertexBuffer);

		mIndices[0] = 0;
		mIndices[1] = 1;
		mIndices[2] = 2;

		D3D11_BUFFER_DESC ibd;
		ibd.Usage = D3D11_USAGE_IMMUTABLE;
		ibd.ByteWidth = sizeof(uint16_t) * INDEX_COUNT;
		ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
		ibd.CPUAccessFlags = 0;
		ibd.MiscFlags = 0;
		ibd.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = mIndices;
		mDevice->CreateBuffer(&ibd, &indexData, &mIndexBuffer);
	}

	void InitializeShaders()
	{
		D3D11_INPUT_ELEMENT_DESC inputDescription[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		// Load Vertex Shader --------------------------------------
		ID3DBlob* vsBlob;
		D3DReadFileToBlob(L"SampleVertexShader.cso", &vsBlob);

		// Create the shader on the device
		mDevice->CreateVertexShader(
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			NULL,
			&mVertexShader);

		// Before cleaning up the data, create the input layout
		if (inputDescription) {
			mDevice->CreateInputLayout(
				inputDescription,					// Reference to Description
				2,									// Number of elments inside of Description
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				&mInputLayout);
		}

		// Clean up
		vsBlob->Release();

		// Load Pixel Shader ---------------------------------------
		ID3DBlob* psBlob;
		D3DReadFileToBlob(L"SamplePixelShader.cso", &psBlob);

		// Create the shader on the device
		mDevice->CreatePixelShader(
			psBlob->GetBufferPointer(),
			psBlob->GetBufferSize(),
			NULL,
			&mPixelShader);

		// Clean up
		psBlob->Release();
	
		// Constant buffers ----------------------------------------
		D3D11_BUFFER_DESC cBufferTransformDesc;
		cBufferTransformDesc.ByteWidth = sizeof(mMatrixBuffer);
		cBufferTransformDesc.Usage = D3D11_USAGE_DEFAULT;
		cBufferTransformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cBufferTransformDesc.CPUAccessFlags = 0;
		cBufferTransformDesc.MiscFlags = 0;
		cBufferTransformDesc.StructureByteStride = 0;

		mDevice->CreateBuffer(&cBufferTransformDesc, NULL, &mConstantBuffer);
	}

	void InitializeCamera()
	{
		mMatrixBuffer.mProjection = mat4f::perspective(0.25f * 3.1415926535f, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
		mMatrixBuffer.mView = mat4f::lookAt(vec3f(0.0, 0.0, 0.0), vec3f(0.0, 0.0, 5.0), vec3f(0.0, 1.0, 0.0)).transpose();
	}

	void VUpdate(double milliseconds) override
	{
		mMatrixBuffer.mWorld = mat4f(1.0);
	}

	void VRender() override
	{
		float color[4] = { 0.5f, 1.0f, 1.0f, 1.0f };

		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), color);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		// Set up the input assembler
		mDeviceContext->IASetInputLayout(mInputLayout);
		mDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		// Set the current vertex and pixel shaders
		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		mDeviceContext->PSSetShader(mPixelShader, NULL, 0);

		// Update the GPU-side constant buffer with our single CPU-side structure
		mDeviceContext->UpdateSubresource(
			mConstantBuffer,
			0,
			NULL,
			&mMatrixBuffer,
			0,
			0);

		// Set the constant buffer to be used by the Vertex Shader
		mDeviceContext->VSSetConstantBuffers(
			0,	// Corresponds to the constant buffer's register in the vertex shader
			1,
			&mConstantBuffer);

		// Set buffers in the input assembler
		UINT stride = sizeof(SampleVertex);
		UINT offset = 0;
		mDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		mDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);

		// Finally do the actual drawing
		mDeviceContext->DrawIndexed(
			INDEX_COUNT,
			0,
			0);

		mRenderer->GetSwapChain()->Present(0, 0);
	}

	void VHandleInput() override
	{
		Input* input = &Input::SharedInstance();
		
		if (input->GetKeyDown(KEYCODE_A))
		{
			// Do something
		}
	}
};


int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	Rig3DSampleScene sampleScene;
	Rig3D::Engine Rig3DEngine = Rig3D::Engine();
	Rig3DEngine.Initialize(hInstance, prevInstance, cmdLine, showCmd, sampleScene.mWindowWidth, sampleScene.mWindowHeight, sampleScene.mWindowCaption);
	Rig3DEngine.RunScene(&sampleScene);
}