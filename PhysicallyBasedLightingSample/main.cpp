#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include "Rig3D\Graphics\MeshLibrary.h"
#include "Rig3D\Graphics\Interface\IShader.h"
#include "Rig3D\Graphics\Interface\IShaderResource.h"
#include "Rig3D/Graphics/Camera.h"
#include <d3d11.h>

#define PI						3.1415926535f
#define CAMERA_SPEED			0.01f
#define CAMERA_ROTATION_SPEED	0.1f
#define RADIAN					3.1415926535f / 180.0f
#define INSTANCE_ROWS			10
#define INSTANCE_COLUMNS		10
#define RADIUS					0.725f
#include <Rig3D/Graphics/DirectX11/DX11ShaderResource.h>

using namespace Rig3D;

uint8_t				gMemory[1024];

struct SH
{
	vec4f coefficients[9];
};

struct Vertex3
{
	vec3f Position;
	vec3f Normal;
	vec2f UV;
};

struct SkyboxData
{
	mat4f	Model;
	mat4f	View;
	mat4f	Projection;
};

struct ModelData
{
	mat4f View;
	mat4f Projection;
};

struct LightData
{
	vec4f LightDirection;
	vec4f CameraPosition;
};

class PhysicallyBasedLightingSample : public IScene, public virtual IRendererDelegate
{
public:
	mat4f						mSphereWorldMatrices[INSTANCE_ROWS * INSTANCE_COLUMNS];
	SkyboxData					mSkyboxData;
	ModelData					mModelData;
	LightData					mLightData;
	SH							mSHData[6];	// Prefilter
	SH							mSH;		// Final
	Camera						mCamera;
	LinearAllocator				mAllocator;

	Transform*					mTransforms;
	IMesh*						mSkyboxMesh;
	IMesh*						mIcosphereMesh;

	TSingleton<IRenderer, DX3D11Renderer>*	mRenderer;
	IShader*					mPBLSkyboxVertexShader;
	IShader*					mPBLSkyboxPixelShader;
	IShader*					mPBLModelVertexShader;
	IShader*					mPBLModelPixelShader;
	IShaderResource*			mPBLSkyboxShaderResource;
	IShaderResource*			mPBLModelShaderResource;

	vec2f						mSphereMaterials[INSTANCE_ROWS * INSTANCE_COLUMNS];
	float						mMouseX;
	float						mMouseY;
	uint32_t					mInstanceCount;

	ID3D11DepthStencilState*	mDepthStencilState;

	ID3D11ComputeShader*		mSHComputeShader;
	ID3D11Buffer*				mSHUAVBuffer;
	ID3D11Buffer*				mSHStagingBuffer;
	ID3D11UnorderedAccessView*	mSHUAV;

	PhysicallyBasedLightingSample();
	~PhysicallyBasedLightingSample();

	void VInitialize() override;
	void VUpdate(double milliseconds) override;
	void VRender() override;
	void VShutdown() override;
	void VOnResize() override;

	void InitializeGeometry();
	void InitializeShaders();
	void InitializeShaderResources();
	void InitializeLighting();
	void PrefilterCubeMap();

	void HandleInput(Input& input);
};

PhysicallyBasedLightingSample::PhysicallyBasedLightingSample() :
	mAllocator(gMemory, gMemory + 1024),
	mTransforms(nullptr),
	mSkyboxMesh(nullptr),
	mIcosphereMesh(nullptr),
	mRenderer(nullptr),
	mPBLSkyboxVertexShader(nullptr),
	mPBLSkyboxPixelShader(nullptr),
	mPBLModelVertexShader(nullptr),
	mPBLModelPixelShader(nullptr),
	mPBLSkyboxShaderResource(nullptr),
	mPBLModelShaderResource(nullptr),
	mMouseX(0.0f),
	mMouseY(0.0f),
	mInstanceCount(INSTANCE_ROWS * INSTANCE_COLUMNS),
	mDepthStencilState(nullptr),
	mSHComputeShader(nullptr),
	mSHUAVBuffer(nullptr),
	mSHStagingBuffer(nullptr),
	mSHUAV(nullptr)
{
	mOptions.mWindowCaption = "Physically Based Lighting Sample";
	mOptions.mWindowWidth = 1200;
	mOptions.mWindowHeight = 900;
	mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
	mOptions.mFullScreen = false;
}

PhysicallyBasedLightingSample::~PhysicallyBasedLightingSample()
{
	ReleaseMacro(mDepthStencilState);
	ReleaseMacro(mSHComputeShader);
	ReleaseMacro(mSHUAVBuffer);
	ReleaseMacro(mSHStagingBuffer);
	ReleaseMacro(mSHUAV);
}

void PhysicallyBasedLightingSample::VInitialize()
{
	mRenderer = mEngine->GetRenderer();
	mRenderer->SetDelegate(this);

	mCamera.mTransform.SetPosition(0.0f, 0.0f, -15.0f);

	InitializeGeometry();
	InitializeShaders();
	InitializeShaderResources();
	InitializeLighting();

	VOnResize();
}

void PhysicallyBasedLightingSample::VUpdate(double milliseconds)
{
	HandleInput(Input::SharedInstance());

	mSkyboxData.Model = mat4f::translate(mCamera.mTransform.GetPosition()).transpose();
	mSkyboxData.Projection = mModelData.Projection = mCamera.GetProjectionMatrix().transpose();
	mSkyboxData.View = mModelData.View = mCamera.GetViewMatrix().transpose();

	mLightData.CameraPosition = mCamera.mTransform.GetPosition();
}

void PhysicallyBasedLightingSample::VRender()
{
	ID3D11DeviceContext* deviceContext	= mRenderer->GetDeviceContext();

	PrefilterCubeMap();

	float color[4] = { 1.0f, 1.0, 1.0f, 1.0f };
	mRenderer->VSetContextTargetWithDepth();
	mRenderer->VClearContext(color, 1.0f, 0);
	deviceContext->RSSetViewports(1, &mRenderer->GetViewport());

	deviceContext->OMSetDepthStencilState(nullptr, 1);

	mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
	
	mRenderer->VSetInputLayout(mPBLModelVertexShader);
	mRenderer->VSetVertexShader(mPBLModelVertexShader);
	mRenderer->VSetPixelShader(mPBLModelPixelShader);

	mRenderer->VUpdateShaderConstantBuffer(mPBLModelShaderResource, &mModelData, 0);
	mRenderer->VUpdateShaderConstantBuffer(mPBLModelShaderResource, &mLightData, 1);
	mRenderer->VSetVertexShaderConstantBuffer(mPBLModelShaderResource, 0, 0);
	mRenderer->VSetPixelShaderConstantBuffer(mPBLModelShaderResource, 1, 0);
	mRenderer->VSetPixelShaderConstantBuffer(mPBLModelShaderResource, 2, 1);

	mRenderer->VBindMesh(mIcosphereMesh);
	mRenderer->VSetVertexShaderInstanceBuffers(mPBLModelShaderResource);

	deviceContext->DrawIndexedInstanced(mIcosphereMesh->GetIndexCount(), mInstanceCount, 0, 0, 0);

	deviceContext->OMSetDepthStencilState(mDepthStencilState, 1);

	mRenderer->VSetInputLayout(mPBLSkyboxVertexShader);

	mRenderer->VSetVertexShader(mPBLSkyboxVertexShader);
	mRenderer->VSetPixelShader(mPBLSkyboxPixelShader);

	mRenderer->VUpdateShaderConstantBuffer(mPBLSkyboxShaderResource, &mSkyboxData, 0);
	mRenderer->VSetVertexShaderConstantBuffer(mPBLSkyboxShaderResource, 0, 0);
	
	mRenderer->VSetPixelShaderResourceViews(mPBLSkyboxShaderResource);
	mRenderer->VSetPixelShaderSamplerStates(mPBLSkyboxShaderResource);

	mRenderer->VBindMesh(mSkyboxMesh);
	mRenderer->VDrawIndexed(0, mSkyboxMesh->GetIndexCount());

	mRenderer->VSwapBuffers();
}

void PhysicallyBasedLightingSample::VShutdown()
{
	mSkyboxMesh->~IMesh();
	mIcosphereMesh->~IMesh();
	mPBLModelVertexShader->~IShader();
	mPBLModelPixelShader->~IShader();
	mPBLSkyboxVertexShader->~IShader();
	mPBLSkyboxPixelShader->~IShader();
	mPBLSkyboxShaderResource->~IShaderResource();
	mPBLModelShaderResource->~IShaderResource();
	mAllocator.Free();
}

void PhysicallyBasedLightingSample::VOnResize()
{
	mCamera.SetProjectionMatrix(mat4f::normalizedPerspectiveLH(0.25f * PI, mRenderer->GetAspectRatio(), 0.1f, 100.0f));
}

void PhysicallyBasedLightingSample::InitializeGeometry()
{
	OBJBasicResource<Vertex3> skyboxResource("Models\\skybox.obj");
	OBJBasicResource<Vertex3> icosahedronResource("Models\\icosahedron.obj");

	MeshLibrary<LinearAllocator> meshLibrary(&mAllocator);
	meshLibrary.LoadMesh(&mSkyboxMesh, mRenderer, skyboxResource);
	meshLibrary.LoadMesh(&mIcosphereMesh, mRenderer, icosahedronResource);

	float width = (RADIUS + RADIUS) * INSTANCE_ROWS;
	float height = (RADIUS + RADIUS) * INSTANCE_COLUMNS;

	float halfWidth = (width - RADIUS) * 0.5f;
	float halfHeight = (height - RADIUS) * 0.5f;

	float rows = static_cast<float>(INSTANCE_ROWS);
	float cols = static_cast<float>(INSTANCE_COLUMNS);

	for (uint32_t y = 0; y < INSTANCE_COLUMNS; y++)
	{
		for (uint32_t x = 0; x < INSTANCE_ROWS; x++)
		{
			mSphereWorldMatrices[(y * INSTANCE_ROWS) + x] = mat4f::translate({ x * (RADIUS + RADIUS) - halfWidth, halfHeight - (y * (RADIUS + RADIUS)) , 0.0f }).transpose();
			mSphereMaterials[(y * INSTANCE_ROWS) + x] = { (x + 1) / rows, (y + 1) / cols };
		}
	}
}



void PhysicallyBasedLightingSample::InitializeShaders()
{
	InputElement skyboxInputElements[] =
	{
		{ "POSITION",	0, 0, 0, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "NORMAL",		0, 0, 12, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 24, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX }
	};

	mRenderer->VCreateShader(&mPBLSkyboxVertexShader, &mAllocator);
	mRenderer->VLoadVertexShader(mPBLSkyboxVertexShader, "PBLSkyboxVertexShader.cso", skyboxInputElements, 3);

	mRenderer->VCreateShader(&mPBLSkyboxPixelShader, &mAllocator);
	mRenderer->VLoadPixelShader(mPBLSkyboxPixelShader, "PBLSkyboxPixelShader.cso");

	InputElement sphereInputElements[] =
	{
		{ "POSITION",	0, 0, 0, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "NORMAL",		0, 0, 12, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 24, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "WORLD",		0, 1, 0, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		1, 1, 16, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		2, 1, 32, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		3, 1, 48, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "TEXCOORD",   1, 2, 0, 1, RG_FLOAT32, INPUT_CLASS_PER_INSTANCE }
	};

	mRenderer->VCreateShader(&mPBLModelVertexShader, &mAllocator);
	mRenderer->VLoadVertexShader(mPBLModelVertexShader, "PBLInstanceVertexShader.cso", sphereInputElements, 8);

	mRenderer->VCreateShader(&mPBLModelPixelShader, &mAllocator);
	mRenderer->VLoadPixelShader(mPBLModelPixelShader, "PBLInstancePixelShader.cso");
}

void PhysicallyBasedLightingSample::InitializeShaderResources()
{
	mRenderer->VCreateShaderResource(&mPBLSkyboxShaderResource, &mAllocator);

	size_t	skyboxConstantBufferSizes[] = { sizeof(SkyboxData) };
	void*	skyboxData[]				= { &mSkyboxData };
	mRenderer->VCreateShaderConstantBuffers(mPBLSkyboxShaderResource, skyboxData, skyboxConstantBufferSizes, 1);

	const char* skyboxes[] =
	{
		"Textures\\Skybox.dds"
	};

	mRenderer->VCreateShaderTextureCubes(mPBLSkyboxShaderResource, skyboxes, 1);
	mRenderer->VAddShaderLinearSamplerState(mPBLSkyboxShaderResource, SAMPLER_STATE_ADDRESS_WRAP);

	mRenderer->VCreateShaderResource(&mPBLModelShaderResource, &mAllocator);

	size_t	modelConstantBufferSizes[] = { sizeof(ModelData), sizeof(LightData), sizeof(SH) };
	void*	modelData[] = { &mModelData, &mLightData, &mSH };
	mRenderer->VCreateShaderConstantBuffers(mPBLModelShaderResource, modelData, modelConstantBufferSizes, 3);

	size_t instanceBufferSizes[] = { sizeof(mat4f) * mInstanceCount, sizeof(vec2f) * mInstanceCount };
	UINT strides[] = { sizeof(mat4f), sizeof(vec2f) };
	UINT offsets[] = { 0, 0 };
	void* instanceData[] = { &mSphereWorldMatrices, &mSphereMaterials };
	mRenderer->VCreateDynamicShaderInstanceBuffers(mPBLModelShaderResource, instanceData, instanceBufferSizes, strides, offsets, 2);
	mRenderer->VUpdateShaderInstanceBuffer(mPBLModelShaderResource, mSphereWorldMatrices, instanceBufferSizes[0], 0);
	mRenderer->VUpdateShaderInstanceBuffer(mPBLModelShaderResource, mSphereMaterials, instanceBufferSizes[1], 1);

	D3D11_DEPTH_STENCIL_DESC depthStencilDesc;
	depthStencilDesc.DepthEnable = true;
	depthStencilDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
	depthStencilDesc.StencilEnable = true;
	depthStencilDesc.StencilReadMask = 0xFF;
	depthStencilDesc.StencilWriteMask = 0xFF;

	// Stencil operations if pixel is front-facing
	depthStencilDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_INCR;
	depthStencilDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	// Stencil operations if pixel is back-facing
	depthStencilDesc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_DECR;
	depthStencilDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	depthStencilDesc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;

	ID3D11Device* device = reinterpret_cast<DX3D11Renderer *>(mRenderer)->GetDevice();
	device->CreateDepthStencilState(&depthStencilDesc, &mDepthStencilState);

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.ByteWidth = sizeof(SH) * 6;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(SH);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &mSHData;

	device->CreateBuffer(&bufferDesc, &data, &mSHUAVBuffer);

	bufferDesc.Usage = D3D11_USAGE_STAGING;
	bufferDesc.BindFlags = 0;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	device->CreateBuffer(&bufferDesc, nullptr, &mSHStagingBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.Buffer.NumElements = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;

	device->CreateUnorderedAccessView(mSHUAVBuffer, &UAVDesc, &mSHUAV);
}

void PhysicallyBasedLightingSample::InitializeLighting()
{
	mLightData.LightDirection = cliqCity::graphicsMath::normalize(vec4f(1.0f, -1.0f, 1.0f, 0.0f));

}

void PhysicallyBasedLightingSample::PrefilterCubeMap()
{
	ID3D11ShaderResourceView** SRVs = reinterpret_cast<DX11ShaderResource*>(mPBLSkyboxShaderResource)->GetShaderResourceViews();
	ID3D11SamplerState** ss = reinterpret_cast<DX11ShaderResource*>(mPBLSkyboxShaderResource)->GetSamplerStates();

	ID3D11Resource* resource;
	SRVs[0]->GetResource(&resource);

	ID3D11Texture2D* textureCube = reinterpret_cast<ID3D11Texture2D*>(resource);
	D3D11_TEXTURE2D_DESC textureDesc;
	textureCube->GetDesc(&textureDesc);

	ID3D11ShaderResourceView* nullSRV[] = { nullptr };
	ID3D11DeviceContext* deviceContext = static_cast<DX3D11Renderer*>(mRenderer)->GetDeviceContext();
	deviceContext->CSSetShader(mSHComputeShader, nullptr, 0);
	deviceContext->CSSetShaderResources(0, 1, &SRVs[0]);
	deviceContext->CSSetSamplers(0, 1, &ss[0]);

	deviceContext->CSSetUnorderedAccessViews(0, 1, &mSHUAV, nullptr);
	deviceContext->Dispatch(6, 1, 1);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShaderResources(0, 1, nullSRV);

	deviceContext->CopyResource(mSHStagingBuffer, mSHUAVBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	deviceContext->Map(mSHStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedSubresource);
	SH* sh = reinterpret_cast<SH*>(mappedSubresource.pData);
	memcpy(mSHData, sh, sizeof(SH) * 6);
	deviceContext->Unmap(mSHStagingBuffer, 0);

	for (uint32_t i = 0; i < 6; i++)
	{
		mSH.coefficients[0] += mSHData[i].coefficients[0];
		mSH.coefficients[1] += mSHData[i].coefficients[1];
		mSH.coefficients[2] += mSHData[i].coefficients[2];
		mSH.coefficients[3] += mSHData[i].coefficients[3];
		mSH.coefficients[4] += mSHData[i].coefficients[4];
		mSH.coefficients[5] += mSHData[i].coefficients[5];
		mSH.coefficients[6] += mSHData[i].coefficients[6];
		mSH.coefficients[7] += mSHData[i].coefficients[7];
		mSH.coefficients[8] += mSHData[i].coefficients[8];
	}
}

void PhysicallyBasedLightingSample::HandleInput(Input& input)
{
	ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
	if (input.GetMouseButton(MOUSEBUTTON_LEFT)) {
		mCamera.mTransform.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
		mCamera.mTransform.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
	}

	mMouseX = mousePosition.x;
	mMouseY = mousePosition.y;

	vec3f position = mCamera.mTransform.GetPosition();
	if (input.GetKey(KEYCODE_W)) {
		position += mCamera.mTransform.GetForward() * CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_A)) {
		position += mCamera.mTransform.GetRight() * -CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_D)) {
		position += mCamera.mTransform.GetRight() * CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_S)) {
		position += mCamera.mTransform.GetForward() * -CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_UP)) {
		position += mCamera.mTransform.GetUp() * CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_DOWN)) {
		position += mCamera.mTransform.GetUp() * -CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}
}

DECLARE_MAIN(PhysicallyBasedLightingSample);