#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include "Rig3D\Graphics\MeshLibrary.h"
#include "Rig3D\Graphics\Interface\IShader.h"
#include "Rig3D\Graphics\Interface\IShaderResource.h"
#include "Rig3D/Intersection.h"
#include "Rig3D/Visibility.h"
#include "Rig3D/Graphics/Camera.h"
#include "Rig3D/Geometry.h"
#include <d3d11.h>
#include "Rig3D/Graphics/DirectX11/DX11ShaderResource.h"


#define INSTANCE_COUNT				1
#define SCENE_MEMORY				2048
#define PI							3.1415926535f
#define CAMERA_SPEED				0.01f
#define CAMERA_ROTATION_SPEED		0.1f
#define RADIAN						3.1415926535f / 180.0f

#define TERRAIN_PATCH_WIDTH_COUNT	2
#define TERRAIN_PATCH_DEPTH_COUNT	2
#define TERRAIN_WIDTH				50.0f
#define TERRAIN_DEPTH				50.0f
#define TERRAIN_VERTEX_DENSITY		20

#define GRAVITY_CONSTANT				0.0000098196f
#define PHYSICS_TIME_STEP				0.1f			// ms

using namespace Rig3D;

static uint32_t gSceneMemory[SCENE_MEMORY];

struct Vertex3
{
	vec3f Position;
	vec3f Normal;
	vec2f UV;
};

struct ViewProjection
{
	mat4f view;
	mat4f projection;
};

struct Terrain
{
	float width;
	float depth;
	float patchWidthCount;
	float patchDepthCount;
};

struct RigidBody
{
	vec3f position;
	vec3f velocity;
	vec3f forces;
	float inverseMass;
};

class SurfaceConstrainedMotionSample : public IScene, public IRendererDelegate
{
public:
	ViewProjection		mViewProjection;
	Terrain				mTerrain;
	Camera				mCamera;
	Plane<vec3f>		mPlanes[6];
	float				mMouseX;
	float				mMouseY;

	LinearAllocator		mLinearAllocator;

	mat4f				mTerrainWorldMatrices[TERRAIN_PATCH_WIDTH_COUNT * TERRAIN_PATCH_DEPTH_COUNT];
	mat4f				mDynamicWorldMatrics[INSTANCE_COUNT];
	RigidBody			mRigidBodies[INSTANCE_COUNT];

	IRenderer*			mRenderer;
	IShader*			mTerrainVertexShader;
	IShader*			mTerrainPixelShader;
	IShader*			mCapsuleVertexShader;
	IShader*			mCapsulePixelShader;
	IShaderResource*	mShaderResouce;
	IMesh*				mTerrainMesh;
	IMesh*				mCapsuleMesh;

	ID3D11GeometryShader*	mGeometryShader;
	ID3D11ComputeShader*	mRigidBodyComputeShader;

	ID3D11Buffer*				mRigidBodyStagingBuffer;
	ID3D11Buffer*				mRigidBodyComputeBuffer;
	ID3D11ShaderResourceView*	mRigidBodySRV;
	ID3D11UnorderedAccessView*	mRigidBodyUAV;

	SurfaceConstrainedMotionSample();
	~SurfaceConstrainedMotionSample();

	void VInitialize() override;
	void VUpdate(double milliseconds) override;
	void VRender() override;
	void VShutdown() override;

	void VOnResize() override;

	void InitializeGeometry();
	void InitializePhysics();
	void InitializeShaders();
	void InitializeShaderResources();

	void UpdateShaderResources();
	void UpdateInput(Input& input);
	void UpdateCamera();

	void UpdateCollisions(RigidBody* rigidBodies, uint32_t count);
	void UpdateForces(RigidBody* rigidBodies, uint32_t count);
	void Integrate(RigidBody* rigidBodies, uint32_t count, float milliseconds);
	void RK4(RigidBody* rigidBodies, uint32_t count, float milliseconds);
	void Euler(RigidBody* rigidBodies, uint32_t count, float milliseconds);

};

SurfaceConstrainedMotionSample::SurfaceConstrainedMotionSample() : 
	mMouseX(0.0f),
	mMouseY(0.0f),
	mLinearAllocator(gSceneMemory, gSceneMemory + SCENE_MEMORY),
	mRenderer(nullptr),
	mTerrainVertexShader(nullptr),
	mTerrainPixelShader(nullptr),
	mCapsuleVertexShader(nullptr),
	mCapsulePixelShader(nullptr),
	mShaderResouce(nullptr),
	mTerrainMesh(nullptr),
	mCapsuleMesh(nullptr),
	mGeometryShader(nullptr),
	mRigidBodyComputeShader(nullptr),
	mRigidBodyComputeBuffer(nullptr),
	mRigidBodyStagingBuffer(nullptr),
	mRigidBodySRV(nullptr),
	mRigidBodyUAV(nullptr)
{
	mOptions.mWindowCaption = "Surface Constrained Motion Sample";
	mOptions.mWindowWidth = 1200;
	mOptions.mWindowHeight = 900;
	mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
	mOptions.mFullScreen = false;
}

SurfaceConstrainedMotionSample::~SurfaceConstrainedMotionSample()
{
	ReleaseMacro(mGeometryShader);
	ReleaseMacro(mRigidBodyComputeShader);
	ReleaseMacro(mRigidBodyComputeBuffer);
	ReleaseMacro(mRigidBodyStagingBuffer);
	ReleaseMacro(mRigidBodySRV);
	ReleaseMacro(mRigidBodyUAV);
}

void SurfaceConstrainedMotionSample::VInitialize()
{
	mRenderer = &DX3D11Renderer::SharedInstance();
	mRenderer->SetDelegate(this);

	mCamera.mTransform.SetPosition({ 0.0f, 50.0f, -50.0f });
	mCamera.mTransform.RotatePitch(45.0f * RADIAN);

	InitializeGeometry();
	InitializePhysics();
	InitializeShaders();
	InitializeShaderResources();

	VOnResize();
}

void SurfaceConstrainedMotionSample::InitializeGeometry()
{
	mTerrain.width = TERRAIN_WIDTH;
	mTerrain.depth = TERRAIN_DEPTH;
	mTerrain.patchWidthCount = static_cast<float>(TERRAIN_PATCH_WIDTH_COUNT);
	mTerrain.patchDepthCount = static_cast<float>(TERRAIN_PATCH_DEPTH_COUNT);

	float patchWidth = mTerrain.width / mTerrain.patchWidthCount;
	float patchDepth = mTerrain.depth / mTerrain.patchDepthCount;

	float halfWidth = (mTerrain.width - patchWidth) * 0.5f;
	float halfDepth = (mTerrain.depth - patchDepth) * 0.5f;

	for (uint32_t z = 0; z < TERRAIN_PATCH_DEPTH_COUNT; z++)
	{
		for (uint32_t x = 0; x < TERRAIN_PATCH_WIDTH_COUNT; x++)
		{
			vec3f f = { x * patchWidth - halfWidth, 0.0f, halfDepth - z * patchDepth };
			mTerrainWorldMatrices[(z * TERRAIN_PATCH_WIDTH_COUNT) + x] = 
				mat4f::translate({ x * patchWidth - halfWidth, 0.0f, halfDepth - z * patchDepth }).transpose();
		}
	}

	std::vector<Vertex3> vertices;
	std::vector<uint16_t> indices;

	Geometry::Plane(vertices, indices, patchWidth, patchDepth, TERRAIN_VERTEX_DENSITY, TERRAIN_VERTEX_DENSITY);

	MeshLibrary<LinearAllocator> meshLibrary(&mLinearAllocator);
	meshLibrary.NewMesh(&mTerrainMesh, mRenderer);
	mRenderer->VSetMeshVertexBuffer(mTerrainMesh, &vertices[0], sizeof(Vertex3) * vertices.size(), sizeof(Vertex3));
	mRenderer->VSetMeshIndexBuffer(mTerrainMesh, &indices[0], indices.size());


	vertices.clear();
	indices.clear();

	Geometry::Sphere(vertices, indices, 10, 10, 0.5f);

	meshLibrary.NewMesh(&mCapsuleMesh, mRenderer);
	mRenderer->VSetMeshVertexBuffer(mCapsuleMesh, &vertices[0], sizeof(Vertex3) * vertices.size(), sizeof(Vertex3));
	mRenderer->VSetMeshIndexBuffer(mCapsuleMesh, &indices[0], indices.size());
}

void SurfaceConstrainedMotionSample::InitializePhysics()
{
	for (uint32_t i = 0; i < INSTANCE_COUNT; i++)
	{
		mRigidBodies[i].position = { 0.0f, 5.0f, 0.0f };
		mRigidBodies[i].velocity = { 0.0f, 0.0f, 0.0f };
		mRigidBodies[i].inverseMass = 50.0f;
	}
}

void SurfaceConstrainedMotionSample::InitializeShaders()
{
	mRenderer->VCreateShader(&mTerrainVertexShader, &mLinearAllocator);
	mRenderer->VCreateShader(&mTerrainPixelShader, &mLinearAllocator);

	InputElement terrainInputElements[] =
	{
		{ "POSITION",	0, 0, 0, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "NORMAL",		0, 0, 12, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 24, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "WORLD",		0, 1, 0, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		1, 1, 16, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		2, 1, 32, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		3, 1, 48, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE }
	};

	mRenderer->VLoadVertexShader(mTerrainVertexShader, "TerrainVertexShader.cso", terrainInputElements, 7);
	mRenderer->VLoadPixelShader(mTerrainPixelShader, "TerrainPixelShader.cso");

	mRenderer->VCreateShader(&mCapsuleVertexShader, &mLinearAllocator);
	mRenderer->VCreateShader(&mCapsulePixelShader, &mLinearAllocator);




	ID3DBlob* gsBlob;
	D3DReadFileToBlob(L"TerrainGeometryShader.cso", &gsBlob);

	ID3D11Device* device = reinterpret_cast<DX3D11Renderer*>(mRenderer)->GetDevice();
	device->CreateGeometryShader(gsBlob->GetBufferPointer(), gsBlob->GetBufferSize(), nullptr, &mGeometryShader);

	ReleaseMacro(gsBlob);

	mRenderer->VLoadVertexShader(mCapsuleVertexShader, "RigidBodyVertexShader.cso");
	mRenderer->VLoadPixelShader(mCapsulePixelShader, "RigidBodyPixelShader.cso");


	ID3DBlob* csBlob;
	D3DReadFileToBlob(L"RigidBodyComputeShader.cso", &csBlob);
	
	device->CreateComputeShader(csBlob->GetBufferPointer(), csBlob->GetBufferSize(), nullptr, &mRigidBodyComputeShader);

	ReleaseMacro(csBlob);
}

void SurfaceConstrainedMotionSample::InitializeShaderResources()
{
	mRenderer->VCreateShaderResource(&mShaderResouce, &mLinearAllocator);

	// Instance buffer
	void*	instanceData[] = { &mTerrainWorldMatrices, &mDynamicWorldMatrics };
	size_t	instanceDataSizes[] = { sizeof(mat4f) * TERRAIN_PATCH_WIDTH_COUNT * TERRAIN_PATCH_DEPTH_COUNT, sizeof(mat4f) * INSTANCE_COUNT };
	size_t	instanceDataStrides[] = { sizeof(mat4f), sizeof(mat4f) };
	size_t	instanceDataOffsets[] = { 0, 0 };

	mRenderer->VCreateDynamicShaderInstanceBuffers(mShaderResouce, instanceData, instanceDataSizes, instanceDataStrides, instanceDataOffsets, 2);
	mRenderer->VUpdateShaderInstanceBuffer(mShaderResouce, mTerrainWorldMatrices, instanceDataSizes[0], 0);

	// Constant buffer
	void*	constantData[] = { &mViewProjection, &mTerrain };
	size_t	constantDataSize[] = { sizeof(ViewProjection), sizeof(Terrain)};

	mRenderer->VCreateShaderConstantBuffers(mShaderResouce, constantData, constantDataSize, 2);
	mRenderer->VUpdateShaderConstantBuffer(mShaderResouce, &mTerrain, 1);

	// Height map
	const char* filename[] = 
	{
		//"Textures\\mt-tarawera-15m-dem.png",
		//"Textures\\mt-tarawera-15m-bump.png"
		"Textures\\morgulterrain.png",
		"Textures\\morgulterrainbump.png"
	};
	mRenderer->VCreateShaderTextures2D(mShaderResouce, filename, 2);

	// Sampler
	mRenderer->VAddShaderLinearSamplerState(mShaderResouce, SAMPLER_STATE_ADDRESS_CLAMP);

	D3D11_BUFFER_DESC bufferDesc;
	ZeroMemory(&bufferDesc, sizeof(D3D11_BUFFER_DESC));
	bufferDesc.BindFlags = D3D11_BIND_UNORDERED_ACCESS;
	bufferDesc.ByteWidth = sizeof(RigidBody) * INSTANCE_COUNT;
	bufferDesc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bufferDesc.StructureByteStride = sizeof(RigidBody);

	D3D11_SUBRESOURCE_DATA data;
	data.pSysMem = &mRigidBodies;

	ID3D11Device* device = reinterpret_cast<DX3D11Renderer*>(mRenderer)->GetDevice();
	device->CreateBuffer(&bufferDesc, &data, &mRigidBodyComputeBuffer);

	bufferDesc.Usage = D3D11_USAGE_STAGING;
	bufferDesc.BindFlags = 0;
	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	device->CreateBuffer(&bufferDesc, nullptr, &mRigidBodyStagingBuffer);

	D3D11_UNORDERED_ACCESS_VIEW_DESC UAVDesc;
	ZeroMemory(&UAVDesc, sizeof(D3D11_UNORDERED_ACCESS_VIEW_DESC));
	UAVDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	UAVDesc.Buffer.FirstElement = 0;
	UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
	UAVDesc.Buffer.NumElements = bufferDesc.ByteWidth / bufferDesc.StructureByteStride;

	device->CreateUnorderedAccessView(mRigidBodyComputeBuffer, &UAVDesc, &mRigidBodyUAV);
}

void SurfaceConstrainedMotionSample::VUpdate(double milliseconds)
{
	UpdateInput(Input::SharedInstance());
	UpdateCamera();
	UpdateForces(mRigidBodies, INSTANCE_COUNT);
	Integrate(mRigidBodies, INSTANCE_COUNT, static_cast<float>(milliseconds));

	UpdateShaderResources();
}

void SurfaceConstrainedMotionSample::UpdateShaderResources()
{
	mViewProjection.projection = mCamera.GetProjectionMatrix().transpose();
	mViewProjection.view = mCamera.GetViewMatrix().transpose();

	for (uint32_t i = 0; i < INSTANCE_COUNT; i++)
	{
		mDynamicWorldMatrics[i] = mat4f::translate(mRigidBodies[i].position).transpose();
	}


}

void SurfaceConstrainedMotionSample::UpdateInput(Input& input)
{
	ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
	if (input.GetMouseButton(MOUSEBUTTON_LEFT))
	{
		mCamera.mTransform.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
		mCamera.mTransform.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
	}

	mMouseX = mousePosition.x;
	mMouseY = mousePosition.y;

	vec3f position = mCamera.mTransform.GetPosition();
	if (input.GetKey(KEYCODE_W))
	{
		position += mCamera.mTransform.GetForward() * CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_A))
	{
		position += mCamera.mTransform.GetRight() * -CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_D))
	{
		position += mCamera.mTransform.GetRight() * CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_S))
	{
		position += mCamera.mTransform.GetForward() * -CAMERA_SPEED;
		mCamera.mTransform.SetPosition(position);
	}


	if (input.GetKey(KEYCODE_UP))
	{
		mRigidBodies[0].position.z += CAMERA_SPEED;
	}

	if (input.GetKey(KEYCODE_DOWN))
	{
		mRigidBodies[0].position.z -= CAMERA_SPEED;
	}

	if (input.GetKey(KEYCODE_RIGHT))
	{
		mRigidBodies[0].position.x += CAMERA_SPEED;
	}

	if (input.GetKey(KEYCODE_LEFT))
	{
		mRigidBodies[0].position.x -= CAMERA_SPEED;
	}
}

void SurfaceConstrainedMotionSample::UpdateCamera()
{
	mCamera.SetViewMatrix(mat4f::lookAtLH(mCamera.mTransform.GetPosition() + mCamera.mTransform.GetForward(), mCamera.mTransform.GetPosition(), vec3f(0.0f, 1.0f, 0.0f)));
}

void SurfaceConstrainedMotionSample::UpdateCollisions(RigidBody* rigidBodies, uint32_t count)
{


}

void SurfaceConstrainedMotionSample::UpdateForces(RigidBody* rigidBodies, uint32_t count)
{
	for (uint32_t i = 0; i < count;i++)
	{
		rigidBodies[i].forces += { 0.0f, -GRAVITY_CONSTANT, 0.0f };
	}
}

void SurfaceConstrainedMotionSample::Integrate(RigidBody* rigidBodies, uint32_t count, float deltaTime)
{
	if (deltaTime > 16.67f)
	{
		deltaTime = 16.67f;
	}

	static float accumulator = 0.0f;
	accumulator += deltaTime;

	int i = 0;
	while (accumulator >= PHYSICS_TIME_STEP)
	{
		Euler(mRigidBodies, count, PHYSICS_TIME_STEP);
		accumulator -= PHYSICS_TIME_STEP;
		i++;
	}
}

void SurfaceConstrainedMotionSample::RK4(RigidBody* rigidBodies, uint32_t count, float deltaTime)
{
}

void SurfaceConstrainedMotionSample::Euler(RigidBody* rigidBodies, uint32_t count, float deltaTime)
{
	for (uint32_t i = 0; i < count; i++)
	{
		vec3f acceleration = rigidBodies[i].forces * rigidBodies[i].inverseMass;
		rigidBodies[i].velocity += acceleration * deltaTime;
		rigidBodies[i].position += rigidBodies[i].velocity * deltaTime;
		rigidBodies[i].forces = { 0.0f, 0.0f, 0.0f };
	}
}

void SurfaceConstrainedMotionSample::VRender()
{
	ID3D11DeviceContext* deviceContext = static_cast<DX3D11Renderer*>(mRenderer)->GetDeviceContext();

	float color[4] = { 0.0f, 0.7294117647f, 1.0f, 1.0f };

	mRenderer->VSetContextTargetWithDepth();
	mRenderer->VClearContext(color, 1.0f, 0);

	mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
	mRenderer->VSetInputLayout(mTerrainVertexShader);
	mRenderer->VSetVertexShader(mTerrainVertexShader);

//	deviceContext->GSSetShader(mGeometryShader, nullptr, 0);
	mRenderer->VSetPixelShader(mTerrainPixelShader);

	mRenderer->VBindMesh(mTerrainMesh);

	mRenderer->VUpdateShaderConstantBuffer(mShaderResouce, &mViewProjection, 0);
	mRenderer->VSetVertexShaderInstanceBuffer(mShaderResouce, 0, 1);
	mRenderer->VSetVertexShaderConstantBuffers(mShaderResouce);
	mRenderer->VSetVertexShaderResourceView(mShaderResouce, 0, 0);
	mRenderer->VSetPixelShaderResourceView(mShaderResouce, 1, 0);
	mRenderer->VSetVertexShaderSamplerStates(mShaderResouce);
	mRenderer->VSetPixelShaderSamplerStates(mShaderResouce);

	deviceContext->DrawIndexedInstanced(mTerrainMesh->GetIndexCount(), TERRAIN_PATCH_WIDTH_COUNT * TERRAIN_PATCH_DEPTH_COUNT, 0, 0, 0);

	ID3D11Buffer** constantBuffers = reinterpret_cast<DX11ShaderResource*>(mShaderResouce)->GetConstantBuffers();
	ID3D11ShaderResourceView** SRVs = reinterpret_cast<DX11ShaderResource*>(mShaderResouce)->GetShaderResourceViews();
	ID3D11SamplerState** samplerStates = reinterpret_cast<DX11ShaderResource*>(mShaderResouce)->GetSamplerStates();
	ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr };
	ID3D11SamplerState* nullSamplerState[] = { nullptr };

	deviceContext->CSSetShader(mRigidBodyComputeShader, nullptr, 0);
	deviceContext->CSSetConstantBuffers(0, 1, &constantBuffers[1]);
	deviceContext->CSSetShaderResources(0, 2, SRVs);
	deviceContext->CSSetSamplers(0, 1, samplerStates);
	deviceContext->UpdateSubresource(mRigidBodyComputeBuffer, 0, nullptr, &mRigidBodies, 0, 0);
	deviceContext->CSSetUnorderedAccessViews(0, 1, &mRigidBodyUAV, nullptr);
	deviceContext->Dispatch(1, 1, 1);
	deviceContext->CSSetShader(nullptr, nullptr, 0);
	deviceContext->CSSetShaderResources(0, 2, nullSRVs);
	deviceContext->CSSetSamplers(0, 1, nullSamplerState);
	deviceContext->CopyResource(mRigidBodyStagingBuffer, mRigidBodyComputeBuffer);

	D3D11_MAPPED_SUBRESOURCE mappedSubresource;
	deviceContext->Map(mRigidBodyStagingBuffer, 0, D3D11_MAP_READ, 0, &mappedSubresource);
	RigidBody* rigidbodies = reinterpret_cast<RigidBody*>(mappedSubresource.pData);
	memcpy(mRigidBodies, rigidbodies, sizeof(RigidBody) * INSTANCE_COUNT);
	deviceContext->Unmap(mRigidBodyStagingBuffer, 0);

	mRenderer->VSetVertexShader(mCapsuleVertexShader);
	mRenderer->VSetPixelShader(mCapsulePixelShader);

	mRenderer->VBindMesh(mCapsuleMesh);
	mRenderer->VUpdateShaderInstanceBuffer(mShaderResouce, mDynamicWorldMatrics, sizeof(mat4f) * INSTANCE_COUNT, 1);
	mRenderer->VSetVertexShaderInstanceBuffer(mShaderResouce, 1, 1);
	mRenderer->VSetVertexShaderConstantBuffer(mShaderResouce, 0, 0);

	deviceContext->DrawIndexedInstanced(mCapsuleMesh->GetIndexCount(), INSTANCE_COUNT, 0, 0, 0);

	mRenderer->VSwapBuffers();
}

void SurfaceConstrainedMotionSample::VShutdown()
{
	mTerrainMesh->~IMesh();
	mCapsuleMesh->~IMesh();
	mTerrainVertexShader->~IShader();
	mTerrainPixelShader->~IShader();
	mCapsuleVertexShader->~IShader();
	mCapsulePixelShader->~IShader();
	mShaderResouce->~IShaderResource();
	mLinearAllocator.Free();
}

void SurfaceConstrainedMotionSample::VOnResize()
{
	mCamera.SetProjectionMatrix(mat4f::normalizedPerspectiveLH(PI * 0.25f, mRenderer->GetAspectRatio(), 0.1f, 100.f));
}

DECLARE_MAIN(SurfaceConstrainedMotionSample);