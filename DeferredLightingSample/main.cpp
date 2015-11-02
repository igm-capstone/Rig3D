#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\LinearAllocator.h"
#include "Rig3D\MeshLibrary.h"
#include "Rig3D/TaskDispatch/TaskDispatcher.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include <random>
#include <ctime>

#define SATURATE_RANDOM				FLT_EPSILON + (float)(rand()) / ((float)(RAND_MAX / (1.0f - FLT_EPSILON)))
#define PI							3.1415926535f
#define MULTITHREAD					1
#define THREAD_COUNT				4
#define MAX_LIGHTS					120
#define MIN_LIGHTS					0
#define LIGHT_POSITION_RADIUS_I		3.0f
#define LIGHT_POSITION_RADIUS_O		6.0f
#define LIGHT_POSITION_RADIUS_OO	9.0f
#define LIGHT_POSITION_RADIUS_OOO	12.0f
#define LIGHT_MOVEMENT_SPEED		0.001f
#define MESH_COUNT					5
#define POINT_LIGHT_SCALE			0.2f
#define POINT_LIGHT_VOLUME_SCALE	5.0f

using namespace Rig3D;

typedef cliqCity::memory::LinearAllocator LinearAllocator;

uint8_t gMemory[10240];
uint8_t gTaskMemory[1024];
static const vec4f gAmbientColor = { 0.02f, 0.2f, 0.2f, 1.0f };
static const float gRadian = PI / 180.f;

struct Vertex2
{
	vec3f Position;
	vec2f UV;
};

struct Vertex3
{
	vec3f Position;
	vec3f Normal;
	vec2f UV;
};

std::mutex gMemoryMutex;

void PerformModelLoadTask(const cliqCity::multicore::TaskData& data)
{
	OBJBasicResource<Vertex3> resource(reinterpret_cast<const char*>(data.mKernelData));
	MeshLibrary<LinearAllocator>* meshLibrary = reinterpret_cast<MeshLibrary<LinearAllocator>*>(data.mStream.in[0]);
	IRenderer* drawContext = reinterpret_cast<IRenderer*>(data.mStream.in[1]);
	IMesh** mesh = reinterpret_cast<IMesh**>(data.mStream.in[2]);

	std::lock_guard<std::mutex> lock(gMemoryMutex);
	meshLibrary->LoadMesh(mesh, drawContext, resource);
}

class DeferredLightingScene : public IScene, public virtual IRendererDelegate
{
public:

	struct PointLight
	{
		vec4f Color;
		vec3f Position;
	};

	struct ModelViewProjection
	{
		mat4f Model;
		mat4f View;
		mat4f Projection;
	};

	struct SceneObject
	{
		Transform	mTransform;
		vec4f		mColor;
		IMesh*		mMesh;
	};

	ModelViewProjection				mMVP;
	SceneObject						mSceneObjects[5];
	mat4f							mPointLightWorldMatrices[MAX_LIGHTS];
	mat4f							mPointLightVolumeWorldMatrices[MAX_LIGHTS];
	PointLight						mPointLights[MAX_LIGHTS];
	int								mPointLightCount;

	MeshLibrary<LinearAllocator>	mMeshLibrary;
	LinearAllocator					mAllocator;
	
	DX3D11Renderer*					mRenderer;
	IMesh*							mTorusMesh;
	IMesh*							mCylinderMesh;
	IMesh*							mSphereMesh;
	IMesh*							mConeMesh;
	IMesh*							mHelixMesh;
	IMesh*							mPlaneMesh;
	IMesh*							mQuadMesh;

	ID3D11DeviceContext*			mDeviceContext;
	ID3D11Device*					mDevice;

	ID3D11RenderTargetView*			mPositionRTV;
	ID3D11RenderTargetView*			mDepthRTV;
	ID3D11RenderTargetView*			mColorRTV;
	ID3D11RenderTargetView*			mNormalRTV;
	ID3D11RenderTargetView*			mLightRTV;

	ID3D11Texture2D*				mPositionMap;
	ID3D11Texture2D*				mDepthMap;
	ID3D11Texture2D*				mColorMap;
	ID3D11Texture2D*				mNormalMap;
	ID3D11Texture2D*				mLightMap;

	ID3D11ShaderResourceView*		mPositionSRV;
	ID3D11ShaderResourceView*		mDepthSRV;
	ID3D11ShaderResourceView*		mColorSRV;
	ID3D11ShaderResourceView*		mNormalSRV;
	ID3D11ShaderResourceView*		mLightSRV;

	ID3D11SamplerState*				mWrapSamplerState;
	ID3D11BlendState*				mBlendState;

	ID3D11VertexShader*				mSceneVertexShader;
	ID3D11PixelShader*				mScenePixelShader;
	ID3D11VertexShader*				mPointLightVertexShader;
	ID3D11PixelShader*				mPointLightPixelShader;
	ID3D11VertexShader*				mLightVolumeVertexShader;
	ID3D11PixelShader*				mLightVolumePixelShader;
	ID3D11VertexShader*				mQuadVertexShader;
	ID3D11PixelShader*				mQuadPixelShader;
	ID3D11PixelShader*				mSingleBufferPixelShader;

	ID3D11InputLayout*				mSceneInputLayout;
	ID3D11InputLayout*				mPointLightInputLayout;
	ID3D11InputLayout*				mQuadInputLayout;

	ID3D11Buffer*					mMVPBuffer;
	ID3D11Buffer*					mColorBuffer;
	ID3D11Buffer*					mLightBuffer;

	D3D11_VIEWPORT					mPositionViewport;
	D3D11_VIEWPORT					mDepthViewport;
	D3D11_VIEWPORT					mColorViewport;
	D3D11_VIEWPORT					mNormalViewport;

	DeferredLightingScene() : 
		mPointLightCount(MIN_LIGHTS),
		mAllocator(gMemory, gMemory + 10240),
		mRenderer(nullptr),
		mTorusMesh(nullptr),
		mCylinderMesh(nullptr),
		mSphereMesh(nullptr),
		mConeMesh(nullptr),
		mHelixMesh(nullptr),
		mPlaneMesh(nullptr),
		mQuadMesh(nullptr),
		mDeviceContext(nullptr),
		mDevice(nullptr),
		mPositionRTV(nullptr),
		mDepthRTV(nullptr),
		mColorRTV(nullptr),
		mNormalRTV(nullptr),
		mLightRTV(nullptr),
		mPositionMap(nullptr),
		mDepthMap(nullptr),
		mColorMap(nullptr),
		mNormalMap(nullptr),
		mLightMap(nullptr),
		mPositionSRV(nullptr),
		mDepthSRV(nullptr),
		mColorSRV(nullptr),
		mNormalSRV(nullptr),
		mLightSRV(nullptr),
		mWrapSamplerState(nullptr),
		mBlendState(nullptr),
		mSceneVertexShader(nullptr),
		mScenePixelShader(nullptr),
		mPointLightVertexShader(nullptr),
		mPointLightPixelShader(nullptr),
		mLightVolumeVertexShader(nullptr),
		mLightVolumePixelShader(nullptr),
		mQuadVertexShader(nullptr),
		mQuadPixelShader(nullptr),
		mSingleBufferPixelShader(nullptr),
		mSceneInputLayout(nullptr),
		mPointLightInputLayout(nullptr),
		mQuadInputLayout(nullptr),
		mMVPBuffer(nullptr),
		mColorBuffer(nullptr),
		mLightBuffer(nullptr)

	{
		mOptions.mWindowCaption = "Deferred Lighting Sample";
		mOptions.mWindowWidth = 1200;
		mOptions.mWindowHeight = 900;
		mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
		mOptions.mFullScreen = false;
		mMeshLibrary.SetAllocator(&mAllocator);
	};

	~DeferredLightingScene()
	{
		ReleaseMacro(mPositionRTV);
		ReleaseMacro(mDepthRTV);
		ReleaseMacro(mColorRTV);
		ReleaseMacro(mNormalRTV);
		ReleaseMacro(mLightRTV);
		ReleaseMacro(mPositionMap);
		ReleaseMacro(mDepthMap);
		ReleaseMacro(mColorMap);
		ReleaseMacro(mNormalMap);
		ReleaseMacro(mLightMap);
		ReleaseMacro(mPositionSRV);
		ReleaseMacro(mDepthSRV);
		ReleaseMacro(mColorSRV);
		ReleaseMacro(mNormalSRV);
		ReleaseMacro(mLightSRV);

		ReleaseMacro(mWrapSamplerState);
		ReleaseMacro(mBlendState);

		ReleaseMacro(mSceneVertexShader);
		ReleaseMacro(mScenePixelShader);
		ReleaseMacro(mPointLightVertexShader);
		ReleaseMacro(mPointLightPixelShader);
		ReleaseMacro(mLightVolumeVertexShader);
		ReleaseMacro(mLightVolumePixelShader);
		ReleaseMacro(mQuadVertexShader);
		ReleaseMacro(mQuadPixelShader);
		ReleaseMacro(mSingleBufferPixelShader);

		ReleaseMacro(mSceneInputLayout);
		ReleaseMacro(mPointLightInputLayout);
		ReleaseMacro(mQuadInputLayout);
	
		ReleaseMacro(mMVPBuffer);
		ReleaseMacro(mColorBuffer);
		ReleaseMacro(mLightBuffer);
	};

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);
		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

		InitializeGeometry();
		InitializeLighting();
		InitializeSceneObjects();
		InitializeShaders();
		InitializeShaderResources();
	}

	void VUpdate(double milliseconds) override
	{
		HandleInput();

		vec3f axis = { 0.0f, 1.0f, 0.0 };
		mat4f physicalScale = mat4f::scale(vec3f(POINT_LIGHT_SCALE));
		mat4f volumeScale = mat4f::scale(vec3f(POINT_LIGHT_VOLUME_SCALE));
		static float t = 0.0f;

		for (int i = 0; i < MAX_LIGHTS; i++)
		{
			
			vec3f linear	= cliqCity::graphicsMath::cross(mPointLights[i].Position, axis);
			vec3f velocity	= cliqCity::graphicsMath::normalize(linear) * LIGHT_MOVEMENT_SPEED;
			float wave = (i % 2 == 0) ? sin(t + atan2(mPointLights[i].Position.z, mPointLights[i].Position.x)) : cos(t + atan2(mPointLights[i].Position.z, mPointLights[i].Position.x));
			mPointLights[i].Position += velocity * static_cast<float>(milliseconds);
			mPointLights[i].Position.y = 0.5f * wave;

			mat4f translation = mat4f::translate(mPointLights[i].Position);
			mPointLightWorldMatrices[i] = (physicalScale * translation).transpose();
			mPointLightVolumeWorldMatrices[i] = (volumeScale * translation).transpose();
		}

		t += static_cast<float>(milliseconds / 1000.0f);
	}

	void HandleInput()
	{
		if (Input::SharedInstance().GetKeyDown(KEYCODE_UP))
		{
			mPointLightCount = min(mPointLightCount + 1, MAX_LIGHTS);
		}
		else if (Input::SharedInstance().GetKeyDown(KEYCODE_DOWN))
		{
			mPointLightCount = max(mPointLightCount - 1, MIN_LIGHTS);
		}
	}

	void VRender() override
	{
		const float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
		
		// G-Buffer Pass
		{
			mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());

			ID3D11RenderTargetView* RTVs[4] = { mPositionRTV, mDepthRTV, mColorRTV, mNormalRTV };
			mDeviceContext->OMSetRenderTargets(4, RTVs, mRenderer->GetDepthStencilView());
			mDeviceContext->ClearRenderTargetView(mPositionRTV, color);
			mDeviceContext->ClearRenderTargetView(mDepthRTV, color);
			mDeviceContext->ClearRenderTargetView(mColorRTV, color);
			mDeviceContext->ClearRenderTargetView(mNormalRTV, color);
			mDeviceContext->ClearDepthStencilView(
				mRenderer->GetDepthStencilView(),
				D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
				1.0f,
				0);

			mDeviceContext->IASetInputLayout(mSceneInputLayout);
			mDeviceContext->VSSetShader(mSceneVertexShader, nullptr, 0);
			mDeviceContext->PSSetShader(mScenePixelShader, nullptr, 0);

			for (int i = 0; i < 5; i++)
			{
				mMVP.Model = mSceneObjects[i].mTransform.GetWorldMatrix().transpose();
				mDeviceContext->UpdateSubresource(mMVPBuffer, 0, nullptr, &mMVP, 0, 0);
				mDeviceContext->UpdateSubresource(mColorBuffer, 0, nullptr, &mSceneObjects[i].mColor, 0, 0);
				mDeviceContext->VSSetConstantBuffers(0, 1, &mMVPBuffer);
				mDeviceContext->PSSetConstantBuffers(0, 1, &mColorBuffer);
				mRenderer->VBindMesh(mSceneObjects[i].mMesh);
				mRenderer->VDrawIndexed(0, mSceneObjects[i].mMesh->GetIndexCount());
			}
		}

		// Light Map Pass
		{
			ID3D11ShaderResourceView* SRVs[2] = { mPositionSRV, mNormalSRV };

			const float black[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
			mDeviceContext->OMSetRenderTargets(1, &mLightRTV, nullptr);
			mDeviceContext->ClearRenderTargetView(mLightRTV,  black);

			mDeviceContext->VSSetShader(mLightVolumeVertexShader, nullptr, 0);
			mDeviceContext->PSSetShader(mLightVolumePixelShader, nullptr, 0);

			mDeviceContext->PSSetShaderResources(0, 2, SRVs);
			mDeviceContext->PSSetSamplers(0, 1, &mWrapSamplerState);

			float c[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
			mDeviceContext->OMSetBlendState(mBlendState, c, 0xffffffff);

			mRenderer->VBindMesh(mSphereMesh);
			for (int i = 0; i < mPointLightCount; i++)
			{
				mMVP.Model = mPointLightVolumeWorldMatrices[i];
				mDeviceContext->UpdateSubresource(mMVPBuffer, 0, nullptr, &mMVP, 0, 0);
				mDeviceContext->UpdateSubresource(mLightBuffer, 0, nullptr, &mPointLights[i], 0, 0);
				mDeviceContext->VSSetConstantBuffers(0, 1, &mMVPBuffer);
				mDeviceContext->VSSetConstantBuffers(1, 1, &mLightBuffer);
				mRenderer->VDrawIndexed(0, mSphereMesh->GetIndexCount());
			}

			mDeviceContext->OMSetBlendState(nullptr, c, 0xffffffff);
		}

		// Final Pass
		{
			ID3D11ShaderResourceView* SRVs[3] = { mColorSRV, mLightSRV, mDepthSRV };

			mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), nullptr);

			mDeviceContext->IASetInputLayout(mQuadInputLayout);
			mDeviceContext->VSSetShader(mQuadVertexShader, nullptr, 0);
			mDeviceContext->PSSetShader(mQuadPixelShader, nullptr, 0);

			mDeviceContext->PSSetShaderResources(0, 3, SRVs);
			mDeviceContext->PSSetSamplers(0, 1, &mWrapSamplerState);

			mDeviceContext->UpdateSubresource(mColorBuffer, 0, nullptr, &gAmbientColor, 0, 0);
			mDeviceContext->PSSetConstantBuffers(0, 1, &mColorBuffer);

			mRenderer->VBindMesh(mQuadMesh);
			mRenderer->VDrawIndexed(0, mQuadMesh->GetIndexCount());
		}

		// Lighting Geometry
		{
			mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
			mDeviceContext->IASetInputLayout(mPointLightInputLayout);
			mDeviceContext->VSSetShader(mPointLightVertexShader, nullptr, 0);
			mDeviceContext->PSSetShader(mPointLightPixelShader, nullptr, 0);

			mRenderer->VBindMesh(mSphereMesh);
			for (int i = 0; i < mPointLightCount; i++)
			{
				mMVP.Model = mPointLightWorldMatrices[i];
				mDeviceContext->UpdateSubresource(mMVPBuffer, 0, nullptr, &mMVP, 0, 0);
				mDeviceContext->UpdateSubresource(mColorBuffer, 0, nullptr, &mPointLights[i].Color, 0, 0);
				mDeviceContext->VSSetConstantBuffers(0, 1, &mMVPBuffer);
				mDeviceContext->VSSetConstantBuffers(1, 1, &mColorBuffer);
				mRenderer->VDrawIndexed(0, mSphereMesh->GetIndexCount());
			}
		}

		// G-Buffer Array Pass
		{
			ID3D11ShaderResourceView* SRVs[4] = { mPositionSRV, mLightSRV, mColorSRV, mNormalSRV };

			D3D11_VIEWPORT viewports[] = { mPositionViewport, mDepthViewport, mColorViewport, mNormalViewport };
			mDeviceContext->IASetInputLayout(mQuadInputLayout);
			mDeviceContext->VSSetShader(mQuadVertexShader, nullptr, 0);
			mDeviceContext->PSSetShader(mSingleBufferPixelShader, nullptr, 0);
			mDeviceContext->PSSetSamplers(0, 1, &mWrapSamplerState);

			mRenderer->VBindMesh(mQuadMesh);
			for (int i = 0; i < 4; i++) {
				mDeviceContext->RSSetViewports(1, &viewports[i]);
				mDeviceContext->ClearDepthStencilView(
					mRenderer->GetDepthStencilView(),
					D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
					1.0f,
					0);

				mDeviceContext->PSSetShaderResources(0, 1, &SRVs[i]);

				mRenderer->VDrawIndexed(0, mQuadMesh->GetIndexCount());
			}

			ID3D11ShaderResourceView* nullSRV[4] = { 0, 0, 0, 0 };
			mDeviceContext->PSSetShaderResources(0, 4, nullSRV);
		}

		mRenderer->VSwapBuffers();
	}

	void VShutdown() override
	{
		mTorusMesh->~IMesh();
		mSphereMesh->~IMesh();
		mConeMesh->~IMesh();
		mCylinderMesh->~IMesh();
		mHelixMesh->~IMesh();
		mPlaneMesh->~IMesh();
		mQuadMesh->~IMesh();
		mAllocator.Free();
	}

	void VOnResize() override
	{
		ReleaseMacro(mPositionRTV);
		ReleaseMacro(mDepthRTV);
		ReleaseMacro(mColorRTV);
		ReleaseMacro(mNormalRTV);
		ReleaseMacro(mLightRTV);
		ReleaseMacro(mPositionMap);
		ReleaseMacro(mDepthMap);
		ReleaseMacro(mColorMap);
		ReleaseMacro(mNormalMap);
		ReleaseMacro(mLightMap);
		ReleaseMacro(mPositionSRV);
		ReleaseMacro(mDepthSRV);
		ReleaseMacro(mColorSRV);
		ReleaseMacro(mNormalSRV);
		ReleaseMacro(mLightSRV);

		// Position
		{
			D3D11_TEXTURE2D_DESC positionTextureDesc;
			positionTextureDesc.Width = mRenderer->GetWindowWidth();
			positionTextureDesc.Height = mRenderer->GetWindowHeight();
			positionTextureDesc.ArraySize = 1;
			positionTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			positionTextureDesc.CPUAccessFlags = 0;
			positionTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			positionTextureDesc.MipLevels = 1;
			positionTextureDesc.MiscFlags = 0;
			positionTextureDesc.SampleDesc.Count = 1;
			positionTextureDesc.SampleDesc.Quality = 0;
			positionTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			mDevice->CreateTexture2D(&positionTextureDesc, nullptr, &mPositionMap);

			D3D11_RENDER_TARGET_VIEW_DESC positionRTVDesc;
			positionRTVDesc.Format = positionTextureDesc.Format;
			positionRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			positionRTVDesc.Texture2D.MipSlice = 0;

			mRenderer->GetDevice()->CreateRenderTargetView(mPositionMap, &positionRTVDesc, &mPositionRTV);

			D3D11_SHADER_RESOURCE_VIEW_DESC positionSRVDesc;
			positionSRVDesc.Format = positionTextureDesc.Format;
			positionSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			positionSRVDesc.Texture2D.MipLevels = 1;
			positionSRVDesc.Texture2D.MostDetailedMip = 0;

			mDevice->CreateShaderResourceView(mPositionMap, &positionSRVDesc, &mPositionSRV);
		}

		// Depth
		{	
			D3D11_TEXTURE2D_DESC depthTextureDesc;
			depthTextureDesc.Width = mRenderer->GetWindowWidth();
			depthTextureDesc.Height = mRenderer->GetWindowHeight();
			depthTextureDesc.ArraySize = 1;
			depthTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			depthTextureDesc.CPUAccessFlags = 0;
			depthTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			depthTextureDesc.MipLevels = 1;
			depthTextureDesc.MiscFlags = 0;
			depthTextureDesc.SampleDesc.Count = 1;
			depthTextureDesc.SampleDesc.Quality = 0;
			depthTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			mDevice->CreateTexture2D(&depthTextureDesc, nullptr, &mDepthMap);

			D3D11_RENDER_TARGET_VIEW_DESC depthRTVDesc;
			depthRTVDesc.Format = depthTextureDesc.Format;
			depthRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			depthRTVDesc.Texture2D.MipSlice = 0;

			mRenderer->GetDevice()->CreateRenderTargetView(mDepthMap, &depthRTVDesc, &mDepthRTV);

			D3D11_SHADER_RESOURCE_VIEW_DESC depthSRVDesc;
			depthSRVDesc.Format = depthTextureDesc.Format;
			depthSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			depthSRVDesc.Texture2D.MipLevels = 1;
			depthSRVDesc.Texture2D.MostDetailedMip = 0;

			mDevice->CreateShaderResourceView(mDepthMap, &depthSRVDesc, &mDepthSRV);
		}

		// Color
		{	
			D3D11_TEXTURE2D_DESC colorTextureDesc;
			colorTextureDesc.Width = mRenderer->GetWindowWidth();
			colorTextureDesc.Height = mRenderer->GetWindowHeight();
			colorTextureDesc.ArraySize = 1;
			colorTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			colorTextureDesc.CPUAccessFlags = 0;
			colorTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			colorTextureDesc.MipLevels = 1;
			colorTextureDesc.MiscFlags = 0;
			colorTextureDesc.SampleDesc.Count = 1;
			colorTextureDesc.SampleDesc.Quality = 0;
			colorTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			mDevice->CreateTexture2D(&colorTextureDesc, nullptr, &mColorMap);

			D3D11_RENDER_TARGET_VIEW_DESC colorRTVDesc;
			colorRTVDesc.Format = colorTextureDesc.Format;
			colorRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			colorRTVDesc.Texture2D.MipSlice = 0;

			mRenderer->GetDevice()->CreateRenderTargetView(mColorMap, &colorRTVDesc, &mColorRTV);

			D3D11_SHADER_RESOURCE_VIEW_DESC colorSRVDesc;
			colorSRVDesc.Format = colorTextureDesc.Format;
			colorSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			colorSRVDesc.Texture2D.MipLevels = 1;
			colorSRVDesc.Texture2D.MostDetailedMip = 0;

			mDevice->CreateShaderResourceView(mColorMap, &colorSRVDesc, &mColorSRV);
		}

		// Normal
		{
			D3D11_TEXTURE2D_DESC normalTextureDesc;
			normalTextureDesc.Width = mRenderer->GetWindowWidth();
			normalTextureDesc.Height = mRenderer->GetWindowHeight();
			normalTextureDesc.ArraySize = 1;
			normalTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			normalTextureDesc.CPUAccessFlags = 0;
			normalTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			normalTextureDesc.MipLevels = 1;
			normalTextureDesc.MiscFlags = 0;
			normalTextureDesc.SampleDesc.Count = 1;
			normalTextureDesc.SampleDesc.Quality = 0;
			normalTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			mDevice->CreateTexture2D(&normalTextureDesc, nullptr, &mNormalMap);

			D3D11_RENDER_TARGET_VIEW_DESC normalRTVDesc;
			normalRTVDesc.Format = normalTextureDesc.Format;
			normalRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			normalRTVDesc.Texture2D.MipSlice = 0;

			mRenderer->GetDevice()->CreateRenderTargetView(mNormalMap, &normalRTVDesc, &mNormalRTV);

			D3D11_SHADER_RESOURCE_VIEW_DESC normalSRVDesc;
			normalSRVDesc.Format = normalTextureDesc.Format;
			normalSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			normalSRVDesc.Texture2D.MipLevels = 1;
			normalSRVDesc.Texture2D.MostDetailedMip = 0;

			mDevice->CreateShaderResourceView(mNormalMap, &normalSRVDesc, &mNormalSRV);
		}

		// Light
		{
			D3D11_TEXTURE2D_DESC lightTextureDesc;
			lightTextureDesc.Width = mRenderer->GetWindowWidth();
			lightTextureDesc.Height = mRenderer->GetWindowHeight();
			lightTextureDesc.ArraySize = 1;
			lightTextureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
			lightTextureDesc.CPUAccessFlags = 0;
			lightTextureDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
			lightTextureDesc.MipLevels = 1;
			lightTextureDesc.MiscFlags = 0;
			lightTextureDesc.SampleDesc.Count = 1;
			lightTextureDesc.SampleDesc.Quality = 0;
			lightTextureDesc.Usage = D3D11_USAGE_DEFAULT;

			mDevice->CreateTexture2D(&lightTextureDesc, nullptr, &mLightMap);

			D3D11_RENDER_TARGET_VIEW_DESC lightRTVDesc;
			lightRTVDesc.Format = lightTextureDesc.Format;
			lightRTVDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			lightRTVDesc.Texture2D.MipSlice = 0;

			mRenderer->GetDevice()->CreateRenderTargetView(mLightMap, &lightRTVDesc, &mLightRTV);

			D3D11_SHADER_RESOURCE_VIEW_DESC lightSRVDesc;
			lightSRVDesc.Format = lightTextureDesc.Format;
			lightSRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			lightSRVDesc.Texture2D.MipLevels = 1;
			lightSRVDesc.Texture2D.MostDetailedMip = 0;

			mDevice->CreateShaderResourceView(mLightMap, &lightSRVDesc, &mLightSRV);
		}

		// Viewports
		{
			float width = static_cast<float>(mRenderer->GetWindowWidth()) * 0.25f;
			float windowHeight = static_cast<float>(mRenderer->GetWindowHeight());
			float height = windowHeight / 6.0f;
			float topY = windowHeight - height;

			mPositionViewport.TopLeftX = 0.0f;
			mPositionViewport.TopLeftY = topY;
			mPositionViewport.Width = width;
			mPositionViewport.Height = height;
			mPositionViewport.MinDepth = 0;
			mPositionViewport.MaxDepth = 1;

			mDepthViewport.TopLeftX = width;
			mDepthViewport.TopLeftY = topY;
			mDepthViewport.Width = width;
			mDepthViewport.Height = height;
			mDepthViewport.MinDepth = 0;
			mDepthViewport.MaxDepth = 1;

			mColorViewport.TopLeftX = width + width;
			mColorViewport.TopLeftY = topY;
			mColorViewport.Width = width;
			mColorViewport.Height = height;
			mColorViewport.MinDepth = 0;
			mColorViewport.MaxDepth = 1;

			mNormalViewport.TopLeftX = width + width + width;
			mNormalViewport.TopLeftY = topY;
			mNormalViewport.Width = width;
			mNormalViewport.Height = height;
			mNormalViewport.MinDepth = 0;
			mNormalViewport.MaxDepth = 1;
		}

		// Camera
		{
			mMVP.View = mat4f::lookAtLH(mSceneObjects[4].mTransform.GetPosition() - vec3f(0.0f, 0.0f, 5.0f), vec3f(0.0f, 20.0f, -20.0f), vec3f(0.0f, 1.0f, 0.0f)).transpose();
			mMVP.Projection = mat4f::normalizedPerspectiveLH(0.25f * PI, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
		}
	}

	void InitializeGeometry()
	{
#ifdef MULTITHREAD
		cliqCity::multicore::Thread threads[THREAD_COUNT];
		cliqCity::multicore::TaskData modelData[MESH_COUNT];
		char* fileNames[MESH_COUNT] = {
			"Models\\torus.obj",
			"Models\\cylinder.obj",
			"Models\\cone.obj",
			"Models\\helix.obj",
			"Models\\sphere.obj"
		};

		IMesh** meshes[MESH_COUNT] = {
			&mTorusMesh,
			&mCylinderMesh,
			&mConeMesh,
			&mHelixMesh,
			&mSphereMesh
		};

		cliqCity::multicore::TaskID taskIDs[MESH_COUNT];

		cliqCity::multicore::TaskDispatcher dispatchQueue(threads, THREAD_COUNT, gTaskMemory, 1024);
		dispatchQueue.Start();
		for (int i = 0; i < MESH_COUNT; i++)
		{
			modelData[i].mKernelData = fileNames[i];
			modelData[i].mStream.in[0] = &mMeshLibrary;
			modelData[i].mStream.in[1] = mRenderer;
			modelData[i].mStream.in[2] = meshes[i];
			taskIDs[i] = dispatchQueue.AddTask(modelData[i], PerformModelLoadTask);
		}
#else
		OBJBasicResource<Vertex3> torusResource("Models\\torus.obj");
		OBJBasicResource<Vertex3> cylinderResource("Models\\cylinder.obj");
		OBJBasicResource<Vertex3> coneResource("Models\\cone.obj");
		OBJBasicResource<Vertex3> cubeResource("Models\\cube.obj");
		OBJBasicResource<Vertex3> sphereResource("Models\\sphere.obj");

		mMeshLibrary.LoadMesh(&mTorusMesh, mRenderer, torusResource);
		mMeshLibrary.LoadMesh(&mCylinderMesh, mRenderer, cylinderResource);
		mMeshLibrary.LoadMesh(&mConeMesh, mRenderer, coneResource);
		mMeshLibrary.LoadMesh(&mHelixMesh, mRenderer, cubeResource);
		mMeshLibrary.LoadMesh(&mSphereMesh, mRenderer, sphereResource);
#endif
	
		Vertex3 planeVertices[9];
		for (int z = 0; z < 3; z++)
		{
			for (int x = 0; x < 3; x++)
			{
				Vertex3& vertex = planeVertices[z * 3 + x];
				vertex.Position = { -1.0f + x, 0.0f, 1.0f - z };
				vertex.Normal = { 0.0f, 1.0f, 0.0f };
				vertex.UV = { x / 3.0f, (1.0f + z / 3.0f) - 1.0f };
			}
		}

		uint16_t planeIndices[24] = { 0, 1, 4, 4, 3, 0, 1, 2, 5, 5, 4, 1, 3, 4, 7, 7, 6, 3, 4, 5, 8, 8, 7, 4 };

		mMeshLibrary.NewMesh(&mPlaneMesh, mRenderer);
		mRenderer->VSetMeshVertexBufferData(mPlaneMesh, planeVertices, sizeof(Vertex3) * 9, sizeof(Vertex3), GPU_MEMORY_USAGE_STATIC);
		mRenderer->VSetMeshIndexBufferData(mPlaneMesh, planeIndices, 24, GPU_MEMORY_USAGE_STATIC);

		Vertex2 quadVertices[4];
		quadVertices[0].Position = { -1.0f, 1.0f, 0.0f };
		quadVertices[0].UV = { 0.0f, 0.0f };
		quadVertices[1].Position = { 1.0f, 1.0f, 0.0f };
		quadVertices[1].UV = { 1.0f, 0.0f };
		quadVertices[2].Position = { 1.0f, -1.0f, 0.0f };
		quadVertices[2].UV = { 1.0f, 1.0f };
		quadVertices[3].Position = { -1.0f, -1.0f, 0.0f };
		quadVertices[3].UV = { 0.0f, 1.0f };

		uint16_t quadIndices[6] = { 0, 1, 2, 2, 3, 0 };

		mMeshLibrary.NewMesh(&mQuadMesh, mRenderer);
		mRenderer->VSetMeshVertexBufferData(mQuadMesh, quadVertices, sizeof(Vertex2) * 4, sizeof(Vertex2), GPU_MEMORY_USAGE_STATIC);
		mRenderer->VSetMeshIndexBufferData(mQuadMesh, quadIndices, 6, GPU_MEMORY_USAGE_STATIC);
	}

	void InitializeLighting()
	{
		std::time_t now;
		std::srand(static_cast<unsigned int>(std::time(&now)));
		int eighthCount		= MAX_LIGHTS / 8;
		int quarterCount	= MAX_LIGHTS / 4;
		int halfCount		= MAX_LIGHTS / 2;

		float angle0 = 2.0f * PI / eighthCount;
		float angle1 = 2.0f * PI / quarterCount;
		float angle2 = 2.0f * PI / halfCount;

		for (int i = 0; i < eighthCount; i++)
		{
			mPointLights[i].Color = { SATURATE_RANDOM, SATURATE_RANDOM, SATURATE_RANDOM, 1.0f };
			mPointLights[i].Position = { LIGHT_POSITION_RADIUS_I * cos(angle0 * i), 1.0f, LIGHT_POSITION_RADIUS_I * sin(angle0 * i) };
		}

		for (int i = eighthCount; i < quarterCount; i++)
		{
			mPointLights[i].Color = { SATURATE_RANDOM, SATURATE_RANDOM, SATURATE_RANDOM, 1.0f };
			mPointLights[i].Position = { LIGHT_POSITION_RADIUS_O * cos(angle0 * i), 1.0f, LIGHT_POSITION_RADIUS_O * sin(angle0 * i) };
		}

		for (int i = quarterCount; i < halfCount; i++)
		{
			mPointLights[i].Color = { SATURATE_RANDOM, SATURATE_RANDOM, SATURATE_RANDOM, 1.0f };
			mPointLights[i].Position = { LIGHT_POSITION_RADIUS_OO * cos(angle1 * i), 1.0f, LIGHT_POSITION_RADIUS_OO * sin(angle1 * i) };
		}

		for (int i = halfCount; i < MAX_LIGHTS; i++)
		{
			mPointLights[i].Color = { SATURATE_RANDOM, SATURATE_RANDOM, SATURATE_RANDOM, 1.0f };
			mPointLights[i].Position = { LIGHT_POSITION_RADIUS_OOO * cos(angle2 * i), 1.0f, LIGHT_POSITION_RADIUS_OOO * sin(angle2 * i) };
		}
	}

	void InitializeSceneObjects()
	{
		vec4f color = { 1.0f, 1.0f, 1.0f, 1.0f };

		mSceneObjects[0].mTransform.SetPosition(-2.2f, 0.0f, 3.0f);
		mSceneObjects[0].mColor = color;// { 1.0f, 1.0f, 0.0f, 1.0f };
		mSceneObjects[0].mMesh = mTorusMesh;

		mSceneObjects[1].mTransform.SetPosition(3.9f, -0.1f, 1.5f );
		mSceneObjects[1].mColor = color; // { 1.0f, 0.0f, 1.0f, 1.0f };
		mSceneObjects[1].mMesh = mCylinderMesh;

		mSceneObjects[2].mTransform.SetPosition(3.7f, 0.0f, -3.0f );
		mSceneObjects[2].mColor = color;// { 0.0f, 1.0f, 1.0f, 1.0f };
		mSceneObjects[2].mMesh = mHelixMesh;

		mSceneObjects[3].mTransform.SetPosition(-3.5f, 0.0f, -2.0f );
		mSceneObjects[3].mColor = color; //{ 1.0f, 0.0f, 0.0f, 1.0f };
		mSceneObjects[3].mMesh = mConeMesh;

		mSceneObjects[4].mTransform.SetPosition(0.0f, -0.6f, 0.0f );
		mSceneObjects[4].mTransform.SetScale(vec3f(30.0f, 1.0f, 20.0f));
		mSceneObjects[4].mColor = { 0.5f, 0.5f, 0.5f, 1.0f };
		mSceneObjects[4].mMesh = mPlaneMesh;
	}

	void InitializeShaders()
	{
		ID3DBlob* vsBlob;
		ID3DBlob* psBlob;

		D3D11_INPUT_ELEMENT_DESC vertex3InputDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,		0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		D3DReadFileToBlob(L"ModelVertexShader.cso", &vsBlob);
		mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mSceneVertexShader);
		mDevice->CreateInputLayout(vertex3InputDesc, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &mSceneInputLayout);
	
		D3DReadFileToBlob(L"ModelPixelShader.cso", &psBlob);
		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mScenePixelShader);

		D3DReadFileToBlob(L"PointLightVertexShader.cso", &vsBlob);
		mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mPointLightVertexShader);
		mDevice->CreateInputLayout(vertex3InputDesc, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &mPointLightInputLayout);

		D3DReadFileToBlob(L"PointLightPixelShader.cso", &psBlob);
		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mPointLightPixelShader);

		D3DReadFileToBlob(L"LightVolumeVertexShader.cso", &vsBlob);
		mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mLightVolumeVertexShader);

		D3DReadFileToBlob(L"LightVolumePixelShader.cso", &psBlob);
		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mLightVolumePixelShader);

		D3D11_INPUT_ELEMENT_DESC vertex2InputDesc[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,		0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		D3DReadFileToBlob(L"QuadVertexShader.cso", &vsBlob);
		mDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &mQuadVertexShader);
		mDevice->CreateInputLayout(vertex2InputDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), &mQuadInputLayout);

		D3DReadFileToBlob(L"QuadPixelShader.cso", &psBlob);
		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mQuadPixelShader);

		D3DReadFileToBlob(L"SingleBufferPixelShader.cso", &psBlob);
		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mSingleBufferPixelShader);

		vsBlob->Release();
		psBlob->Release();
	}

	void InitializeShaderResources()
	{
		D3D11_BUFFER_DESC MVPBufferDesc;
		MVPBufferDesc.ByteWidth = sizeof(ModelViewProjection);
		MVPBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		MVPBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		MVPBufferDesc.CPUAccessFlags = 0;
		MVPBufferDesc.MiscFlags = 0;
		MVPBufferDesc.StructureByteStride = 0;

		mDevice->CreateBuffer(&MVPBufferDesc, nullptr, &mMVPBuffer);

		D3D11_BUFFER_DESC colorBufferDesc;
		colorBufferDesc.ByteWidth = sizeof(vec4f);
		colorBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		colorBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		colorBufferDesc.CPUAccessFlags = 0;
		colorBufferDesc.MiscFlags = 0;
		colorBufferDesc.StructureByteStride = 0;

		mDevice->CreateBuffer(&colorBufferDesc, nullptr, &mColorBuffer);

		D3D11_BUFFER_DESC lightBufferDesc;
		lightBufferDesc.ByteWidth = sizeof(PointLight);
		lightBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		lightBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		lightBufferDesc.CPUAccessFlags = 0;
		lightBufferDesc.MiscFlags = 0;
		lightBufferDesc.StructureByteStride = 0;

		mDevice->CreateBuffer(&lightBufferDesc, nullptr, &mLightBuffer);

		D3D11_SAMPLER_DESC wrapSamplerDesc;
		ZeroMemory(&wrapSamplerDesc, sizeof(D3D11_SAMPLER_DESC));
		wrapSamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		wrapSamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		wrapSamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		wrapSamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		wrapSamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		mDevice->CreateSamplerState(&wrapSamplerDesc, &mWrapSamplerState);

		D3D11_BLEND_DESC blendDesc;
		ZeroMemory(&blendDesc, sizeof(D3D11_BLEND_DESC));
		blendDesc.AlphaToCoverageEnable = 0;
		blendDesc.IndependentBlendEnable = 0;
		blendDesc.RenderTarget[0].BlendEnable = 1;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		mDevice->CreateBlendState(&blendDesc, &mBlendState);

		VOnResize();
	}
};

DECLARE_MAIN(DeferredLightingScene);