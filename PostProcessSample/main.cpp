#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\LinearAllocator.h"
#include "Rig3D\Graphics\DirectX11\DirectXTK\Inc\WICTextureLoader.h"
#include "Rig3D\MeshLibrary.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <random>
#include <ctime>

#define SATURATE_RANDOM			FLT_EPSILON + (float)(rand()) / ((float)(RAND_MAX / (1.0f - FLT_EPSILON)))
#define OFFSET_COUNT			12
#define PI						3.1415926535f
#define CAMERA_SPEED			0.1f
#define CAMERA_ROTATION_SPEED	0.1f
#define RADIAN					3.1415926535f / 180.0f
#define NODE_COUNT				8

using namespace Rig3D;

static const int VERTEX_COUNT			= 8;
static const int INDEX_COUNT			= 36;
static const float ANIMATION_DURATION	= 20000.0f; // 20 Seconds

class Rig3DSampleScene : public IScene, public virtual IRendererDelegate
{
public:

	typedef cliqCity::memory::LinearAllocator LinearAllocator;

	enum BlurType
	{
		BLUR_TYPE_NONE,
		BLUR_TYPE_GAUSSIAN,
		BLUR_TYPE_MOTION
	};

	struct SampleVertex
	{
		vec3f mPosition;
		vec2f mUV;
	};

	struct Vertex4 
	{
		vec4f Tangent;
		vec3f Position;
		vec3f Normal;
		vec2f UV;
	};

	struct SampleMatrixBuffer
	{
		mat4f mWorld;
		mat4f mView;
		mat4f mProjection;
	};

	struct BlurBuffer
	{
		vec2f uvOffsets[OFFSET_COUNT];
	};

	struct MBMatrixBuffer
	{
		mat4f mInverseClip;
		mat4f mPreviousClip;
	};
	
	struct SceneNode
	{
		Transform mTransform;
		vec4f	  mColor;
		IMesh*	  mMesh;
	};

	SampleMatrixBuffer		mMatrixBuffer;
	MBMatrixBuffer			mMBMatrixBuffer;
	BlurBuffer				mBlurH;
	BlurBuffer				mBlurV;
	SceneNode				mSceneNodes[NODE_COUNT];
	Transform				mCamera;

	vec2f					mPixelSize;
	vec4f					mClearColor;
	float					mMouseX;
	float					mMouseY;
	BlurType				mBlurType;

	LinearAllocator			mAllocator;

	MeshLibrary<LinearAllocator>	mMeshLibrary;
	IMesh*							mCubeMesh;
	IMesh*							mQuadMesh;

	DX3D11Renderer*			mRenderer;
	ID3D11Device*			mDevice;
	ID3D11DeviceContext*	mDeviceContext;

	ID3D11Buffer*			mConstantBuffer;
	ID3D11InputLayout*		mSceneInputLayout;
	ID3D11InputLayout*		mPostProcessInputLayout;
	ID3D11VertexShader*		mVertexShader;
	ID3D11PixelShader*		mPixelShader;

	ID3D11PixelShader*		mSCPixelShader;
	ID3D11Buffer*			mColorBuffer;

	// Use for blur
	ID3D11RenderTargetView*		mSceneRTV;
	ID3D11ShaderResourceView*	mSceneSRV;
	ID3D11Texture2D*			mSceneTexture2D;

	ID3D11RenderTargetView*		mBlurRTV;
	ID3D11ShaderResourceView*	mBlurSceneSRV;
	ID3D11Texture2D*			mBlurTexture2D;

	ID3D11ShaderResourceView*	mLavaSRV;
	ID3D11Texture2D*			mLavaTexture2D;

	ID3D11ShaderResourceView*	mWaterSRV;
	ID3D11Texture2D*			mWaterTexture2D;

	ID3D11SamplerState*			mSamplerState;

	ID3D11VertexShader*			mQuadVertexShader;
	ID3D11PixelShader*			mQuadBlurPixelShader;
	ID3D11Buffer*				mBlurBuffer;

	ID3D11PixelShader*			mMotionBlurPixelShader;
	ID3D11Buffer*				mMotionBlurBuffer;
	ID3D11Texture2D*			mDepthTexture2D;
	ID3D11DepthStencilView*     mDepthDSV;
	ID3D11ShaderResourceView*	mDepthSRV;

	Rig3DSampleScene() : mAllocator(1024), mMouseX(0.0f), mMouseY(0.0f), mCubeMesh(nullptr), mQuadMesh(nullptr)
	{
		mOptions.mWindowCaption	= "Rig3D Sample";
		mOptions.mWindowWidth	= 800;
		mOptions.mWindowHeight	= 600;
		mOptions.mGraphicsAPI   = GRAPHICS_API_DIRECTX11;
		mOptions.mFullScreen	= false;
		mBlurType				= BLUR_TYPE_NONE;
		mClearColor				= { 0.2f, 0.2f, 0.2f, 1.0f };
		mMeshLibrary.SetAllocator(&mAllocator);
	}

	~Rig3DSampleScene()
	{
		ReleaseMacro(mConstantBuffer);
		ReleaseMacro(mSceneInputLayout);
		ReleaseMacro(mPostProcessInputLayout);
		ReleaseMacro(mVertexShader);
		ReleaseMacro(mPixelShader);

		ReleaseMacro(mSCPixelShader);
		ReleaseMacro(mColorBuffer);

		ReleaseMacro(mSceneRTV);
		ReleaseMacro(mSceneSRV);
		ReleaseMacro(mSceneTexture2D);

		ReleaseMacro(mBlurRTV);
		ReleaseMacro(mBlurSceneSRV);
		ReleaseMacro(mBlurTexture2D);

		ReleaseMacro(mLavaTexture2D);
		ReleaseMacro(mLavaSRV);

		ReleaseMacro(mWaterTexture2D);
		ReleaseMacro(mWaterSRV);

		ReleaseMacro(mSamplerState);

		ReleaseMacro(mQuadVertexShader);
		ReleaseMacro(mQuadBlurPixelShader);
		ReleaseMacro(mBlurBuffer);

		ReleaseMacro(mQuadBlurPixelShader);
		ReleaseMacro(mMotionBlurBuffer);
		ReleaseMacro(mDepthTexture2D);
		ReleaseMacro(mDepthDSV);
		ReleaseMacro(mDepthSRV);
	}

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);

		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext(); 

		VOnResize();

		mMouseX = 0.0f;
		mMouseY = 0.0f;

		std::time_t now;
		std::srand(static_cast<unsigned int>(std::time(&now)));

		InitializeGeometry();
		InitializeShaders();
		InitializeCamera();
	}

	void InitializeGeometry()
	{
		OBJResource<Vertex4> resource ("Models\\Sphere.obj");
		mMeshLibrary.LoadMesh(&mCubeMesh, mRenderer, resource);

		SampleVertex qVertices[4];
		qVertices[0].mPosition	= { -1.0f, 1.0f, 0.0f };
		qVertices[0].mUV		= { 0.0f, 0.0f};

		qVertices[1].mPosition	= { 1.0f, 1.0f, 0.0f };
		qVertices[1].mUV		= { 1.0f, 0.0f};

		qVertices[2].mPosition	= { 1.0f, -1.0f, 0.0f };
		qVertices[2].mUV		= { 1.0f, 1.0f };

		qVertices[3].mPosition	= { -1.0f, -1.0f, 0.0f };
		qVertices[3].mUV		= { 0.0f, 1.0f};

		uint16_t qIndices[6];
		qIndices[0] = 0;
		qIndices[1] = 1;
		qIndices[2] = 2;

		qIndices[3] = 2;
		qIndices[4] = 3;
		qIndices[5] = 0;

		mMeshLibrary.NewMesh(&mQuadMesh, mRenderer);
		mRenderer->VSetMeshVertexBufferData(mQuadMesh, qVertices, sizeof(SampleVertex) * 4, sizeof(SampleVertex), GPU_MEMORY_USAGE_STATIC);
		mRenderer->VSetMeshIndexBufferData(mQuadMesh, qIndices, 6, GPU_MEMORY_USAGE_STATIC);

		for (int i = 0; i < NODE_COUNT; i++) {
			mSceneNodes[i].mMesh = mCubeMesh;
			mSceneNodes[i].mColor = { SATURATE_RANDOM, SATURATE_RANDOM, SATURATE_RANDOM, 1.0f };
			mSceneNodes[i].mTransform.SetPosition((float)(rand() % 10) - 5.0f, (float)(rand() % 10) - 5.0f, (float)(rand() % 5));
		}

		mCamera.SetPosition( 0.0f, 0.0, -10.0f );
}

	void InitializeShaders()
	{
		ID3DBlob* vsBlob;
		ID3DBlob* psBlob;

		// Base Scene Shaders
		{
			D3D11_INPUT_ELEMENT_DESC inputDescription[] =
			{
				{ "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 28, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, 40, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};

			// Load Vertex Shader --------------------------------------
			D3DReadFileToBlob(L"SphereVertexShader.cso", &vsBlob);

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
					4,									// Number of elments inside of Description
					vsBlob->GetBufferPointer(),
					vsBlob->GetBufferSize(),
					&mSceneInputLayout);
			}

			// Clean up
			vsBlob->Release();

			// Load Pixel Shader ---------------------------------------
			D3DReadFileToBlob(L"SpherePixelShader.cso", &psBlob);

			// Create the shader on the device
			mDevice->CreatePixelShader(
				psBlob->GetBufferPointer(),
				psBlob->GetBufferSize(),
				NULL,
				&mPixelShader);

			psBlob->Release();

			D3DReadFileToBlob(L"SCPixelShader.cso", &psBlob);

			// Create the shader on the device
			mDevice->CreatePixelShader(
				psBlob->GetBufferPointer(),
				psBlob->GetBufferSize(),
				NULL,
				&mSCPixelShader);

			// Clean up
			psBlob->Release();

			// Constant buffers ----------------------------------------
			D3D11_BUFFER_DESC cBufferTransformDesc;
			cBufferTransformDesc.ByteWidth = sizeof(SampleMatrixBuffer);
			cBufferTransformDesc.Usage = D3D11_USAGE_DEFAULT;
			cBufferTransformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cBufferTransformDesc.CPUAccessFlags = 0;
			cBufferTransformDesc.MiscFlags = 0;
			cBufferTransformDesc.StructureByteStride = 0;

			mDevice->CreateBuffer(&cBufferTransformDesc, NULL, &mConstantBuffer);

			D3D11_BUFFER_DESC cBufferColorDesc;
			cBufferColorDesc.ByteWidth = sizeof(vec4f);
			cBufferColorDesc.Usage = D3D11_USAGE_DEFAULT;
			cBufferColorDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			cBufferColorDesc.CPUAccessFlags = 0;
			cBufferColorDesc.MiscFlags = 0;
			cBufferColorDesc.StructureByteStride = 0;

			mDevice->CreateBuffer(&cBufferColorDesc, NULL, &mColorBuffer);

			DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\lava.png", nullptr, &mLavaSRV);
			DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\water.png", nullptr, &mWaterSRV);

			D3D11_SAMPLER_DESC samplerDesc;
			ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
			samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

			mDevice->CreateSamplerState(&samplerDesc, &mSamplerState);
		}
		
		// Blur Shaders
		{
			D3D11_INPUT_ELEMENT_DESC inputDescription2[] =
			{
				{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
				{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
			};

			D3DReadFileToBlob(L"BlurVertexShader.cso", &vsBlob);

			mDevice->CreateVertexShader(
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				NULL,
				&mQuadVertexShader);

			if (inputDescription2) {
				mDevice->CreateInputLayout(
					inputDescription2,					// Reference to Description
					2,									// Number of elments inside of Description
					vsBlob->GetBufferPointer(),
					vsBlob->GetBufferSize(),
					&mPostProcessInputLayout);
			}

			vsBlob->Release();

			D3DReadFileToBlob(L"BlurPixelShader.cso", &psBlob);

			mDevice->CreatePixelShader(
				psBlob->GetBufferPointer(),
				psBlob->GetBufferSize(),
				NULL,
				&mQuadBlurPixelShader);

			psBlob->Release();

			D3D11_BUFFER_DESC blurBufferDesc;
			blurBufferDesc.ByteWidth = sizeof(BlurBuffer);
			blurBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			blurBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			blurBufferDesc.CPUAccessFlags = 0;
			blurBufferDesc.MiscFlags = 0;
			blurBufferDesc.StructureByteStride = 0;

			mDevice->CreateBuffer(&blurBufferDesc, NULL, &mBlurBuffer);
		}

		// Motion Blur Shaders 
		{
			D3DReadFileToBlob(L"MBQPixelShader.cso", &psBlob);

			mDevice->CreatePixelShader(
				psBlob->GetBufferPointer(),
				psBlob->GetBufferSize(),
				NULL,
				&mMotionBlurPixelShader);

			psBlob->Release();

			D3D11_BUFFER_DESC bufferDesc;
			bufferDesc.ByteWidth = sizeof(MBMatrixBuffer);
			bufferDesc.Usage = D3D11_USAGE_DEFAULT;
			bufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
			bufferDesc.CPUAccessFlags = 0;
			bufferDesc.MiscFlags = 0;
			bufferDesc.StructureByteStride = 0;

			mDevice->CreateBuffer(&bufferDesc, NULL, &mMotionBlurBuffer);
		}
	}

	void InitializeCamera()
	{
		mMBMatrixBuffer.mPreviousClip = mMatrixBuffer.mProjection * mMatrixBuffer.mView * mat4f(1.0f);
		mMatrixBuffer.mProjection = mat4f::normalizedPerspectiveLH(0.25f * PI, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
		mMatrixBuffer.mView = mCamera.GetWorldMatrix().inverse().transpose();
		mMBMatrixBuffer.mInverseClip = (mMatrixBuffer.mProjection * mMatrixBuffer.mView * mat4f(1.0f)).inverse();
		mat4f i = mMBMatrixBuffer.mPreviousClip * mMBMatrixBuffer.mInverseClip;
		//mMatrixBuffer.mView = mat4f::lookToLH(mCamera.GetForward(), mCamera.mPosition, vec3f(0.0f, 1.0f, 0.0f)).transpose();
	}

	void VUpdate(double milliseconds) override
	{
		InitializeCamera();

		mPixelSize = { 1.0f / mRenderer->GetWindowWidth(), 1.0f / mRenderer->GetWindowWidth() };
		
		int offset = OFFSET_COUNT / 2;
		for (int i = 0; i < OFFSET_COUNT; i++) {
			mBlurH.uvOffsets[i] = { mPixelSize.x * (i - offset) , 0.0f };
			mBlurV.uvOffsets[i] = { 0.0f, mPixelSize.y * (i - offset) };
		}

		ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
		if (Input::SharedInstance().GetMouseButton(MOUSEBUTTON_LEFT)) {
			mCamera.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
			mCamera.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
		}

		mMouseX = mousePosition.x;
		mMouseY = mousePosition.y;

		vec3f position = mCamera.GetPosition();
		if (Input::SharedInstance().GetKey(KEYCODE_W)) {
			position += mCamera.GetForward() * CAMERA_SPEED;
			mCamera.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_A)) {
			position += mCamera.GetRight() * -CAMERA_SPEED;
			mCamera.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_D)) {
			position += mCamera.GetRight() * CAMERA_SPEED;
			mCamera.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_S)) {
			position += mCamera.GetForward() * -CAMERA_SPEED;
			mCamera.SetPosition(position);
		}


		if (Input::SharedInstance().GetKey(KEYCODE_1)) {
			mBlurType = BLUR_TYPE_NONE;
		}

		if (Input::SharedInstance().GetKey(KEYCODE_2)) {
			mBlurType = BLUR_TYPE_GAUSSIAN;
		}

		if (Input::SharedInstance().GetKey(KEYCODE_3)) {
			mBlurType = BLUR_TYPE_MOTION;
		}
	}

	void VRender() override
	{
		// Set up the input assembler
		mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());

		switch (mBlurType)
		{
		case Rig3DSampleScene::BLUR_TYPE_NONE:
			RenderNoBlur();
			break;
		case Rig3DSampleScene::BLUR_TYPE_GAUSSIAN:
			RenderGuassianBlur();
			break;
		case Rig3DSampleScene::BLUR_TYPE_MOTION:
			RenderMotionBlur();
			break;
		default:
			break;
		}

		mRenderer->VSwapBuffers();
	}

	void RenderNoBlur()
	{
		mDeviceContext->IASetInputLayout(mSceneInputLayout);

		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		//mDeviceContext->PSSetShader(mPixelShader, NULL, 0);
		mDeviceContext->PSSetShader(mSCPixelShader, NULL, 0);

		DrawScene();
	}

	void RenderGuassianBlur()
	{
		mDeviceContext->IASetInputLayout(mSceneInputLayout);

		mDeviceContext->OMSetRenderTargets(1, &mSceneRTV, mDepthDSV);
		mDeviceContext->ClearRenderTargetView(mSceneRTV, (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mDepthDSV,
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		//mDeviceContext->PSSetShader(mPixelShader, NULL, 0);
		mDeviceContext->PSSetShader(mSCPixelShader, NULL, 0);

		DrawScene();

		// Horizontal pass gets rendered to the vertical
		mDeviceContext->IASetInputLayout(mPostProcessInputLayout);

		mDeviceContext->OMSetRenderTargets(1, &mBlurRTV, mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(mBlurRTV, (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mQuadVertexShader, NULL, 0);
		mDeviceContext->PSSetShader(mQuadBlurPixelShader, NULL, 0);

		// Bind mSceneTexture2D
		mDeviceContext->UpdateSubresource(
			mBlurBuffer,
			0,
			NULL,
			&mBlurH,
			0,
			0);

		mDeviceContext->PSSetConstantBuffers(
			0,
			1,
			&mBlurBuffer);

		mDeviceContext->PSSetShaderResources(0, 1, &mSceneSRV);
		mDeviceContext->PSSetShaderResources(1, 1, &mDepthSRV);
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

		mRenderer->VBindMesh(mQuadMesh);

		mRenderer->VDrawIndexed(0, mQuadMesh->GetIndexCount());

		// Final Pass
		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->UpdateSubresource(
			mBlurBuffer,
			0,
			NULL,
			&mBlurV,
			0,
			0);

		mDeviceContext->PSSetConstantBuffers(
			0,
			1,
			&mBlurBuffer);

		// Bind Blur Texture
		mDeviceContext->PSSetShaderResources(0, 1, &mBlurSceneSRV);
		mDeviceContext->PSSetShaderResources(1, 1, &mDepthSRV);
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

		mRenderer->VBindMesh(mQuadMesh);
		mRenderer->VDrawIndexed(0, mQuadMesh->GetIndexCount());

		ID3D11ShaderResourceView* nullSRV[2] = { 0, 0 };
		mDeviceContext->PSSetShaderResources(0, 2, nullSRV);
	}

	void RenderMotionBlur()
	{
		mDeviceContext->IASetInputLayout(mSceneInputLayout);

		mDeviceContext->OMSetRenderTargets(1, &mSceneRTV, mDepthDSV);
		mDeviceContext->ClearRenderTargetView(mSceneRTV, (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mDepthDSV,
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		//mDeviceContext->PSSetShader(mPixelShader, NULL, 0);
		mDeviceContext->PSSetShader(mSCPixelShader, NULL, 0);

		DrawScene();

		// One pass for motion blur
		mDeviceContext->IASetInputLayout(mPostProcessInputLayout);

		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), (const float*)&mClearColor);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mQuadVertexShader, NULL, 0);
		mDeviceContext->PSSetShader(mMotionBlurPixelShader, NULL, 0);

		mDeviceContext->UpdateSubresource(
			mMotionBlurBuffer,
			0,
			NULL,
			&mMBMatrixBuffer,
			0,
			0);

		mDeviceContext->PSSetConstantBuffers(
			0,
			1,
			&mMotionBlurBuffer);

		mDeviceContext->PSSetShaderResources(0, 1, &mSceneSRV);
		mDeviceContext->PSSetShaderResources(1, 1, &mDepthSRV);
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

		mRenderer->VBindMesh(mQuadMesh);
		mRenderer->VDrawIndexed(0, mQuadMesh->GetIndexCount());

		ID3D11ShaderResourceView* nullSRV[2] = { 0, 0 };
		mDeviceContext->PSSetShaderResources(0, 2, nullSRV);
	}

	void DrawScene()
	{
		for (int i = 0; i < NODE_COUNT; i++) {
			mMatrixBuffer.mWorld = mSceneNodes[i].mTransform.GetWorldMatrix().transpose();

			mDeviceContext->UpdateSubresource(
				mConstantBuffer,
				0,
				NULL,
				&mMatrixBuffer,
				0,
				0);

			mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);

			mDeviceContext->UpdateSubresource(
				mColorBuffer,
				0,
				NULL,
				&mSceneNodes[i].mColor,
				0,
				0);

			mDeviceContext->PSSetConstantBuffers(0, 1, &mColorBuffer);

			mDeviceContext->PSSetShaderResources(0, 1, &mLavaSRV);
			mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

			mRenderer->VBindMesh(mSceneNodes[i].mMesh);
			mRenderer->VDrawIndexed(0, mSceneNodes[i].mMesh->GetIndexCount());
		}
	}

	void VOnResize() override
	{
		ReleaseMacro(mBlurTexture2D);
		ReleaseMacro(mSceneTexture2D);

		ReleaseMacro(mBlurRTV);
		ReleaseMacro(mSceneRTV);

		ReleaseMacro(mBlurSceneSRV);
		ReleaseMacro(mSceneSRV);

		ReleaseMacro(mDepthTexture2D);
		ReleaseMacro(mDepthDSV);
		ReleaseMacro(mDepthSRV);
		
		D3D11_TEXTURE2D_DESC sceneTextureDesc;
		sceneTextureDesc.Width = mRenderer->GetWindowWidth();
		sceneTextureDesc.Height = mRenderer->GetWindowHeight();
		sceneTextureDesc.ArraySize = 1;
		sceneTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		sceneTextureDesc.CPUAccessFlags = 0;
		sceneTextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		sceneTextureDesc.MipLevels = 1;
		sceneTextureDesc.MiscFlags = 0;
		sceneTextureDesc.SampleDesc.Count = 1;
		sceneTextureDesc.SampleDesc.Quality = 0;
		sceneTextureDesc.Usage = D3D11_USAGE_DEFAULT;

		mDevice->CreateTexture2D(&sceneTextureDesc, 0, &mBlurTexture2D);
		mDevice->CreateTexture2D(&sceneTextureDesc, 0, &mSceneTexture2D);

		D3D11_RENDER_TARGET_VIEW_DESC rtvDesc;
		rtvDesc.Format = sceneTextureDesc.Format;
		rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvDesc.Texture2D.MipSlice = 0;

		mRenderer->GetDevice()->CreateRenderTargetView(mBlurTexture2D, 0, &mBlurRTV);
		mRenderer->GetDevice()->CreateRenderTargetView(mSceneTexture2D, 0, &mSceneRTV);

		D3D11_SHADER_RESOURCE_VIEW_DESC sceneSRVDesc;
		sceneSRVDesc.Format = sceneTextureDesc.Format;
		sceneSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sceneSRVDesc.Texture2D.MipLevels = 1;
		sceneSRVDesc.Texture2D.MostDetailedMip = 0;

		mDevice->CreateShaderResourceView(mBlurTexture2D, &sceneSRVDesc, &mBlurSceneSRV);
		mDevice->CreateShaderResourceView(mSceneTexture2D, &sceneSRVDesc, &mSceneSRV);

		D3D11_TEXTURE2D_DESC depthTextureDesc;
		depthTextureDesc.Width = mRenderer->GetWindowWidth();
		depthTextureDesc.Height = mRenderer->GetWindowHeight();
		depthTextureDesc.MipLevels = 1;
		depthTextureDesc.ArraySize = 1;
		depthTextureDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		depthTextureDesc.CPUAccessFlags = 0;
		depthTextureDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;
		depthTextureDesc.MiscFlags = 0;
		depthTextureDesc.SampleDesc.Count = 1;
		depthTextureDesc.SampleDesc.Quality = 0;
		depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;
		mDevice->CreateTexture2D(&depthTextureDesc, 0, &mDepthTexture2D);

		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc;
		dsvDesc.Flags = 0;
		dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;
		mDevice->CreateDepthStencilView(mDepthTexture2D, &dsvDesc, &mDepthDSV);

		D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
		depthSRVDesc.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
		depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		depthSRVDesc.Texture2D.MipLevels = depthTextureDesc.MipLevels;
		depthSRVDesc.Texture2D.MostDetailedMip = 0;
		mDevice->CreateShaderResourceView(mDepthTexture2D, &depthSRVDesc, &mDepthSRV);
	}

	void VShutdown() override
	{
		mQuadMesh->~IMesh();
		mCubeMesh->~IMesh();
		mAllocator.Free();
	}
};

DECLARE_MAIN(Rig3DSampleScene);