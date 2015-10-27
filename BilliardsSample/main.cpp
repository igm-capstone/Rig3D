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
#include <Rig3D/Graphics/Camera.h>
#include "Rig3D/Intersection.h"
#include <vector>

#define DYNAMIC_COLLISION_TEST			0
#define PI								3.1415926535f
#define BALL_COUNT						16
#define BALL_RADIUS						0.1077032961f
#define CAMERA_SPEED					0.01f
#define CAMERA_ROTATION_SPEED			0.05f
#define RADIAN							3.1415926535f / 180.0f
#define SURFACE_Y						2.15f
#define PLANE_COUNT						4
#define INVERSE_BALL_MASS				5.88235294118f
#define BALL_MASS						0.17f
#define ELASTIC_CONSTANT				0.9f
#define PLANE_SPHERE_ELASTIC_CONSTANT	0.5f
#define TABLE_WIDTH						2.8f
#define TABLE_DEPTH						5.8f
#define CUE_SPEED						1.0f

using namespace Rig3D;

static const int	gMeshMemorySize = 2048;
static const vec3f  gTablePosition = { -1.0f, 0.0f, 1.0f };
static const mat3f	gSphereInverseTensor = mat4f((2.0f / 5.0f) * BALL_MASS * BALL_RADIUS * BALL_RADIUS).inverse();
static const mat3f	gPlaneInverseTensor;

char gMeshMemory[gMeshMemorySize];

class BilliardsSample : public IScene, public virtual IRendererDelegate
{
public:
	typedef cliqCity::memory::LinearAllocator LinearAllocator;
	typedef Plane<vec3f>	Plane;
	typedef Sphere<vec3f>	Sphere;

	struct Vertex3
	{
		vec3f Position;
		vec3f Normal;
		vec2f UV;
	};

	struct RigidBody
	{
		vec3f	velocity;
		vec3f	angularVelocity;
		vec3f	forces;
		float	inverseMass;

		RigidBody() : velocity(0.0f), angularVelocity(0.0f), forces(0.0f), inverseMass(INVERSE_BALL_MASS) {};
	};

	struct Collision
	{
		vec3f	poi;
		float	t;
		int		s0;
		int		s1;

		Collision(vec3f poi, float t, int s0, int s1) : poi(poi), t(t), s0(s0), s1(s1) {};
	};

	struct PoolTable
	{
		IMesh*	legs;
		IMesh*	feet;
		IMesh*	sides;
		IMesh*	guards;
		IMesh*	surface;
		IMesh*	bottom;
		IMesh*	holes;
	};

	struct ViewProjection
	{
		mat4f view;
		mat4f projection;
	};

	ViewProjection					mViewProjection;
	Transform						mBallTransforms[BALL_COUNT];
	mat4f							mBallWorldMatrices[BALL_COUNT];
	mat4f							mTableWorldMatrix;

	RigidBody						mRigidBodies[BALL_COUNT];
	Sphere							mSpheres[BALL_COUNT];
	Plane							mPlanes[PLANE_COUNT];
	std::vector<Collision>			mSphereCollisions;
	std::vector<Collision>			mPlaneCollisions;

	Camera							mCamera;

	LinearAllocator					mAllocator;
	MeshLibrary<LinearAllocator>	mMeshLibrary;

	float							mMouseX;
	float							mMouseY;

	DX3D11Renderer*					mRenderer;
	ID3D11DeviceContext*			mDeviceContext;
	ID3D11Device*					mDevice;

	PoolTable						mPoolTable;
	IMesh*							mBallMesh;

	ID3D11InputLayout*				mBallInputLayout;
	ID3D11VertexShader*				mBallVertexShader;
	ID3D11PixelShader*				mBallPixelShader;
	ID3D11ShaderResourceView*		mBallSRV;

	ID3D11InputLayout*				mPoolTableInputLayout;
	ID3D11VertexShader*				mPoolTableVertexShader;
	ID3D11PixelShader*				mPoolTablePixelShader;
	ID3D11ShaderResourceView*		mPoolTableWoodSRV;
	ID3D11ShaderResourceView*		mPoolTableDarkWoodSRV;
	ID3D11ShaderResourceView*		mPoolTableConcreteSRV;
	ID3D11ShaderResourceView*		mPoolTableFeltSRV;

	ID3D11SamplerState*				mSamplerState;

	ID3D11Buffer*					mViewProjectionBuffer;
	ID3D11Buffer*					mBallTransformBuffer;
	ID3D11Buffer*					mTableTransformBuffer;

	BilliardsSample() :
		mAllocator(gMeshMemory, gMeshMemory + gMeshMemorySize),
		mMouseX(0.0f),
		mMouseY(0.0f),
		mRenderer(nullptr),
		mDeviceContext(nullptr),
		mDevice(nullptr),
		mBallMesh(nullptr),
		mBallInputLayout(nullptr),
		mBallVertexShader(nullptr),
		mBallPixelShader(nullptr),
		mBallSRV(nullptr),
		mPoolTableInputLayout(nullptr),
		mPoolTableVertexShader(nullptr),
		mPoolTablePixelShader(nullptr),
		mPoolTableWoodSRV(nullptr),
		mPoolTableDarkWoodSRV(nullptr),
		mPoolTableConcreteSRV(nullptr),
		mPoolTableFeltSRV(nullptr),
		mSamplerState(nullptr),
		mViewProjectionBuffer(nullptr),
		mBallTransformBuffer(nullptr),
		mTableTransformBuffer(nullptr)
	{
		mOptions.mWindowCaption = "Deferred Lighting Sample";
		mOptions.mWindowWidth = 800;
		mOptions.mWindowHeight = 600;
		mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
		mOptions.mFullScreen = false;
		mMeshLibrary.SetAllocator(&mAllocator);
		mSphereCollisions.reserve(BALL_COUNT);
		mPlaneCollisions.reserve(BALL_COUNT);
	}

	~BilliardsSample()
	{
		ReleaseMacro(mBallInputLayout);
		ReleaseMacro(mBallVertexShader);
		ReleaseMacro(mBallPixelShader);
		ReleaseMacro(mBallSRV);

		ReleaseMacro(mPoolTableInputLayout);
		ReleaseMacro(mPoolTableVertexShader);
		ReleaseMacro(mPoolTablePixelShader);
		ReleaseMacro(mPoolTableWoodSRV);
		ReleaseMacro(mPoolTableDarkWoodSRV);
		ReleaseMacro(mPoolTableConcreteSRV);
		ReleaseMacro(mPoolTableFeltSRV);
		ReleaseMacro(mSamplerState);

		ReleaseMacro(mViewProjectionBuffer);
		ReleaseMacro(mBallTransformBuffer);
		ReleaseMacro(mTableTransformBuffer);
	}

	void VInitialize()	override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);

		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

		InitializeGeometry();
		InitializeShaders();
		InitializeShaderResources();
		InitializeCamera();
		VOnResize();
	}

	void InitializeGeometry()
	{
		InitializeMeshes();
		InitializeBoundingVolumes();
	}

	void InitializeMeshes()
	{
		OBJBasicResource<Vertex3> poolTableLegs("Models\\Legs.obj");
		OBJBasicResource<Vertex3> poolTableFeet("Models\\LegsMetalPart.obj");
		OBJBasicResource<Vertex3> poolTableSides("Models\\Side.obj");
		OBJBasicResource<Vertex3> poolTableGuards("Models\\SideGuard.obj");
		OBJBasicResource<Vertex3> poolTableSurface("Models\\Surface.obj");
		OBJBasicResource<Vertex3> poolTableBottom("Models\\Bottom.obj");
		OBJBasicResource<Vertex3> poolTableHoles("Models\\Holes.obj");
		OBJBasicResource<Vertex3> ball("Models\\Ball.obj");

		mMeshLibrary.LoadMesh(&mPoolTable.legs, mRenderer, poolTableLegs);
		mMeshLibrary.LoadMesh(&mPoolTable.feet, mRenderer, poolTableFeet);
		mMeshLibrary.LoadMesh(&mPoolTable.sides, mRenderer, poolTableSides);
		mMeshLibrary.LoadMesh(&mPoolTable.guards, mRenderer, poolTableGuards);
		mMeshLibrary.LoadMesh(&mPoolTable.surface, mRenderer, poolTableSurface);
		mMeshLibrary.LoadMesh(&mPoolTable.bottom, mRenderer, poolTableBottom);
		mMeshLibrary.LoadMesh(&mPoolTable.holes, mRenderer, poolTableHoles);
		mMeshLibrary.LoadMesh(&mBallMesh, mRenderer, ball);
	}

	void InitializeBoundingVolumes()
	{
		float yOffset = SURFACE_Y + BALL_RADIUS;
		vec3f position = { 0.0f, yOffset, -3.0f };
		mBallTransforms[0].SetPosition(position);
		mSpheres[0].origin = position;
		mSpheres[0].radius = BALL_RADIUS;

		float diameter = BALL_RADIUS * 2.0f;
		float xOffset = 0;
		float zOffset = 0;
		int xCount = 1;
		int zCount = 5;
		int i = 1;
		for (int z = 0; z < zCount; z++)
		{
			for (int x = 0; x < xCount; x++)
			{

				vec3f position = { xOffset, yOffset, zOffset };
				mBallTransforms[i].SetPosition(position);
				mSpheres[i].origin = position;
				mSpheres[i].radius = BALL_RADIUS;
				xOffset += diameter;
				i++;
			}

			xOffset = -(BALL_RADIUS * xCount * 0.5f);
			zOffset += diameter;
			xCount++;
		}

		mPlanes[0].normal = { +1.0f, +0.0f, +0.0f };	// Left Plane Facing Right
		mPlanes[1].normal = { +0.0f, +0.0f, -1.0f };	// Back Plane Facing Camera
		mPlanes[2].normal = { -1.0f, +0.0f, +0.0f };	// Right Plane Facing Left
		mPlanes[3].normal = { +0.0f, +0.0f, +1.0f };	// Front Plane Facing DirectX Forward

		float halfWidth = TABLE_WIDTH * 0.5f;
		float halfDepth = TABLE_DEPTH * 0.5f;

		mPlanes[0].distance = -halfWidth + gTablePosition.x;
		mPlanes[1].distance = +halfDepth + gTablePosition.z;
		mPlanes[2].distance = +halfWidth + gTablePosition.x;
		mPlanes[3].distance = -halfDepth + gTablePosition.z;

		mTableWorldMatrix = (mat4f::rotateY(PI * 0.5f) * mat4f::translate(gTablePosition)).transpose();
	}

	void InitializeShaders()
	{
		InitializeDefaultShaders();
		InitializeInstanceShaders();
	}

	void InitializeInstanceShaders()
	{
		D3D11_INPUT_ELEMENT_DESC inputDescription[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "WORLD", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 0, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 16, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 32, D3D11_INPUT_PER_INSTANCE_DATA, 1 },
			{ "WORLD", 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 1, 48, D3D11_INPUT_PER_INSTANCE_DATA, 1 }
		};

		ID3DBlob* vsBlob;
		D3DReadFileToBlob(L"BallVertexShader.cso", &vsBlob);

		mDevice->CreateVertexShader(
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			nullptr,
			&mBallVertexShader);

		if (inputDescription) {
			mDevice->CreateInputLayout(
				inputDescription,
				7,
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				&mBallInputLayout);
		}

		vsBlob->Release();

		ID3DBlob* psBlob;
		D3DReadFileToBlob(L"BallPixelShader.cso", &psBlob);

		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mBallPixelShader);

		psBlob->Release();
	}

	void InitializeDefaultShaders()
	{
		D3D11_INPUT_ELEMENT_DESC inputDescription[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT,	0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24,D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		ID3DBlob* vsBlob;
		D3DReadFileToBlob(L"DefaultVertexShader.cso", &vsBlob);

		mDevice->CreateVertexShader(
			vsBlob->GetBufferPointer(),
			vsBlob->GetBufferSize(),
			nullptr,
			&mPoolTableVertexShader);

		if (inputDescription) {
			mDevice->CreateInputLayout(
				inputDescription,
				3,
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				&mPoolTableInputLayout);
		}

		vsBlob->Release();

		ID3DBlob* psBlob;
		D3DReadFileToBlob(L"DefaultPixelShader.cso", &psBlob);

		mDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &mPoolTablePixelShader);

		psBlob->Release();
	}

	void InitializeShaderResources()
	{
		LoadBallTextures();
		LoadPoolTableTextures();
		InitializeSamplerState();
		InitializeBuffers();
	}

	void LoadBallTextures()
	{
		const wchar_t* filenames[BALL_COUNT] = {
			L"Textures\\0.png",
			L"Textures\\1.png",
			L"Textures\\2.png",
			L"Textures\\3.png",
			L"Textures\\4.png",
			L"Textures\\5.png",
			L"Textures\\6.png",
			L"Textures\\7.png",
			L"Textures\\8.png",
			L"Textures\\10.png",
			L"Textures\\10.png",
			L"Textures\\11.png",
			L"Textures\\12.png",
			L"Textures\\13.png",
			L"Textures\\14.png",
			L"Textures\\15.png"
		};

		ID3D11Texture2D* ballTextures[BALL_COUNT];

		for (int i = 0; i < BALL_COUNT; i++)
		{
			DirectX::CreateWICTextureFromFileEx(mDevice, filenames[i], 0, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_READ, 0, false, reinterpret_cast<ID3D11Resource**>(&ballTextures[i]), nullptr);
		}

		D3D11_TEXTURE2D_DESC textureDesc;
		ballTextures[0]->GetDesc(&textureDesc);

		D3D11_TEXTURE2D_DESC textureArrayDesc;
		textureArrayDesc.Width = textureDesc.Width;
		textureArrayDesc.Height = textureDesc.Height;
		textureArrayDesc.MipLevels = textureDesc.MipLevels;
		textureArrayDesc.ArraySize = BALL_COUNT;
		textureArrayDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		textureArrayDesc.SampleDesc.Count = 1;
		textureArrayDesc.SampleDesc.Quality = 0;
		textureArrayDesc.Usage = D3D11_USAGE_DEFAULT;
		textureArrayDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		textureArrayDesc.CPUAccessFlags = 0;
		textureArrayDesc.MiscFlags = 0;

		ID3D11Texture2D* textureArray;
		mDevice->CreateTexture2D(&textureArrayDesc, nullptr, &textureArray);

		D3D11_MAPPED_SUBRESOURCE mappedSubresource;
		for (int i = 0; i < BALL_COUNT; i++)
		{
			for (int j = 0; j < textureDesc.MipLevels; j++)
			{
				mDeviceContext->Map(ballTextures[i], j, D3D11_MAP_READ, 0, &mappedSubresource);
				mDeviceContext->UpdateSubresource(textureArray, D3D11CalcSubresource(j, i, textureDesc.MipLevels), nullptr, mappedSubresource.pData, mappedSubresource.RowPitch, 0);
				mDeviceContext->Unmap(ballTextures[i], j);
			}
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc;
		SRVDesc.Format = textureArrayDesc.Format;
		SRVDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		SRVDesc.Texture2DArray.MostDetailedMip = 0;
		SRVDesc.Texture2DArray.MipLevels = textureArrayDesc.MipLevels;
		SRVDesc.Texture2DArray.FirstArraySlice = 0;
		SRVDesc.Texture2DArray.ArraySize = BALL_COUNT;

		mDevice->CreateShaderResourceView(textureArray, &SRVDesc, &mBallSRV);

		ReleaseMacro(textureArray);
		for (int i = 0; i < BALL_COUNT; i++)
		{
			ReleaseMacro(ballTextures[i]);
		}
	}

	void LoadPoolTableTextures()
	{
		DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\cherrywood.png", nullptr, &mPoolTableWoodSRV);
		DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\chocolatewood.png", nullptr, &mPoolTableDarkWoodSRV);
		DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\concrete.jpg", nullptr, &mPoolTableConcreteSRV);
		DirectX::CreateWICTextureFromFile(mDevice, L"Textures\\felt.jpg", nullptr, &mPoolTableFeltSRV);
	}

	void InitializeSamplerState()
	{
		D3D11_SAMPLER_DESC samplerDesc;
		ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		mDevice->CreateSamplerState(&samplerDesc, &mSamplerState);
	}

	void InitializeBuffers()
	{
		D3D11_BUFFER_DESC ballTransformBufferDesc;
		ballTransformBufferDesc.ByteWidth = sizeof(mat4f) * BALL_COUNT;
		ballTransformBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		ballTransformBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		ballTransformBufferDesc.CPUAccessFlags = D3D10_CPU_ACCESS_WRITE;
		ballTransformBufferDesc.MiscFlags = 0;
		ballTransformBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA transformData;
		transformData.pSysMem = mBallWorldMatrices;
		mDevice->CreateBuffer(&ballTransformBufferDesc, &transformData, &mBallTransformBuffer);

		D3D11_BUFFER_DESC viewProjectionBufferDesc;
		viewProjectionBufferDesc.ByteWidth = sizeof(ViewProjection);
		viewProjectionBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		viewProjectionBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		viewProjectionBufferDesc.CPUAccessFlags = 0;
		viewProjectionBufferDesc.MiscFlags = 0;
		viewProjectionBufferDesc.StructureByteStride = 0;

		mDevice->CreateBuffer(&viewProjectionBufferDesc, nullptr, &mViewProjectionBuffer);

		D3D11_BUFFER_DESC tableTransformDesc;
		tableTransformDesc.ByteWidth = sizeof(mat4f);
		tableTransformDesc.Usage = D3D11_USAGE_IMMUTABLE;
		tableTransformDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		tableTransformDesc.CPUAccessFlags = 0;
		tableTransformDesc.MiscFlags = 0;
		tableTransformDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA tableTransformData;
		tableTransformData.pSysMem = &mTableWorldMatrix;
		mDevice->CreateBuffer(&tableTransformDesc, &tableTransformData, &mTableTransformBuffer);
	}

	void InitializeCamera()
	{
		mCamera.mTransform.SetPosition(0.0f, SURFACE_Y + 2.5f, -10.0f);
		mCamera.mTransform.SetRotation(PI / 6.0f, 0.0f, 0.0f);
	}

	void VUpdate(double milliseconds) override
	{
		HandleInput();
		IntegrateBalls(milliseconds);
		DetectSphereSphereCollisions(&mSphereCollisions, mSpheres, mRigidBodies, BALL_COUNT);
		DetectPlaneSphereCollisions(&mPlaneCollisions, mPlanes, PLANE_COUNT, mSpheres, mRigidBodies, BALL_COUNT);
		ResolveSphereSphereCollisions(&mSphereCollisions, mSpheres, mBallTransforms, mRigidBodies);
		//ResolvePlaneSphereCollisions(&mPlaneCollisions, mPlanes, mSpheres, mRigidBodies);
		ApplyFriction(mSpheres, mRigidBodies, BALL_COUNT);
		// Interpolate State (Optional)
		
		// Update rendering structures.
		UpdateBallTransforms();
		UpdateCamera();
	}

	void UpdateCamera()
	{
		ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
		if (Input::SharedInstance().GetMouseButton(MOUSEBUTTON_LEFT)) {
			mCamera.mTransform.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
			mCamera.mTransform.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
		}

		mMouseX = mousePosition.x;
		mMouseY = mousePosition.y;

		vec3f position = mCamera.mTransform.GetPosition();
		if (Input::SharedInstance().GetKey(KEYCODE_W)) {
			position += mCamera.mTransform.GetForward() * CAMERA_SPEED;
			mCamera.mTransform.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_A)) {
			position += mCamera.mTransform.GetRight() * -CAMERA_SPEED;
			mCamera.mTransform.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_D)) {
			position += mCamera.mTransform.GetRight() * CAMERA_SPEED;
			mCamera.mTransform.SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_S)) {
			position += mCamera.mTransform.GetForward() * -CAMERA_SPEED;
			mCamera.mTransform.SetPosition(position);
		}

		mViewProjection.view = mat4f::lookAtLH(position + mCamera.mTransform.GetForward(), position, vec3f(0.0f, 1.0f, 0.0f)).transpose();
	}

	void UpdateBallTransforms()
	{
		for (int i = 0; i < BALL_COUNT; i++)
		{
			mBallWorldMatrices[i] = mBallTransforms[i].GetWorldMatrix().transpose();
		}

		D3D11_MAPPED_SUBRESOURCE mappedSubresource;
		ZeroMemory(&mappedSubresource, sizeof(D3D11_MAPPED_SUBRESOURCE));
		mDeviceContext->Map(mBallTransformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedSubresource);
		memcpy(mappedSubresource.pData, mBallWorldMatrices, sizeof(mat4f) * BALL_COUNT);
		mDeviceContext->Unmap(mBallTransformBuffer, 0);
	}

	void HandleInput()
	{
		if (Input::SharedInstance().GetKeyDown(KEYCODE_SPACE))
		{
			mat3f rotMat = mCamera.mTransform.GetRotationMatrix();
			vec3f f = vec3f(0.0f, 0.0f, 1.0f) * rotMat;
			vec3f cameraForward = mCamera.mTransform.GetForward();
			ApplyImpulse(mSpheres[0], mRigidBodies[0], vec3f(cameraForward.x, 0.0f, cameraForward.z));
		}
	}

	void VRender() override
	{
		float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), color);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
		mDeviceContext->UpdateSubresource(mViewProjectionBuffer, 0, nullptr, &mViewProjection, 0, 0);

		RenderBalls();
		RenderPoolTable();

		mRenderer->VSwapBuffers();
	}

	void RenderBalls()
	{
		mDeviceContext->IASetInputLayout(mBallInputLayout);
		mDeviceContext->VSSetShader(mBallVertexShader, nullptr, 0);
		mDeviceContext->PSSetShader(mBallPixelShader, nullptr, 0);

		ID3D11Buffer* constantBuffers[2] = { mViewProjectionBuffer, mTableTransformBuffer };
		mDeviceContext->VSSetConstantBuffers(0, 2, constantBuffers);
		mDeviceContext->PSSetShaderResources(0, 1, &mBallSRV);
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

		mRenderer->VBindMesh(mBallMesh);

		const UINT stride = sizeof(mat4f);
		const UINT offset = 0;
		mDeviceContext->IASetVertexBuffers(1, 1, &mBallTransformBuffer, &stride, &offset);

		mDeviceContext->DrawIndexedInstanced(mBallMesh->GetIndexCount(), BALL_COUNT, 0, 0, 0);
	}

	void RenderPoolTable()
	{
		mDeviceContext->IASetInputLayout(mPoolTableInputLayout);
		mDeviceContext->VSSetShader(mPoolTableVertexShader, nullptr, 0);
		mDeviceContext->PSSetShader(mPoolTablePixelShader, nullptr, 0);

		mDeviceContext->VSSetConstantBuffers(0, 1, &mViewProjectionBuffer);
		mDeviceContext->PSSetSamplers(0, 1, &mSamplerState);

		const UINT stride = sizeof(Vertex3);
		const UINT offset = 0;

		// Legs
		mDeviceContext->PSSetShaderResources(0, 1, &mPoolTableWoodSRV);
		mRenderer->VBindMesh(mPoolTable.legs);
		mRenderer->VDrawIndexed(0, mPoolTable.legs->GetIndexCount());

		// Feet
		mDeviceContext->PSSetShaderResources(0, 1, &mPoolTableConcreteSRV);
		mRenderer->VBindMesh(mPoolTable.feet);
		mRenderer->VDrawIndexed(0, mPoolTable.feet->GetIndexCount());

		// Guards
		mRenderer->VBindMesh(mPoolTable.guards);
		mRenderer->VDrawIndexed(0, mPoolTable.guards->GetIndexCount());

		// Surface
		mDeviceContext->PSSetShaderResources(0, 1, &mPoolTableFeltSRV);
		mRenderer->VBindMesh(mPoolTable.surface);
		mRenderer->VDrawIndexed(0, mPoolTable.surface->GetIndexCount());


		// Bottom 
		mDeviceContext->PSSetShaderResources(0, 1, &mPoolTableDarkWoodSRV);
		mRenderer->VBindMesh(mPoolTable.bottom);
		mRenderer->VDrawIndexed(0, mPoolTable.bottom->GetIndexCount());

		// Holes
		mRenderer->VBindMesh(mPoolTable.holes);
		mRenderer->VDrawIndexed(0, mPoolTable.holes->GetIndexCount());

		// Sides
		mRenderer->VBindMesh(mPoolTable.sides);
		mRenderer->VDrawIndexed(0, mPoolTable.sides->GetIndexCount());
	}

	void VShutdown() override
	{
		mBallMesh->~IMesh();
		mPoolTable.legs->~IMesh();
		mPoolTable.sides->~IMesh();
		mPoolTable.feet->~IMesh();
		mPoolTable.surface->~IMesh();
		mPoolTable.holes->~IMesh(); 
		mPoolTable.guards->~IMesh();
		mPoolTable.bottom->~IMesh();
		mAllocator.Free();
	}

	void VOnResize() override
	{
		mViewProjection.projection = mat4f::normalizedPerspectiveLH(PI * 0.25f, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
	}

	void IntegrateBalls(double milliseconds)
	{
		static float frameTime = static_cast<float>(milliseconds);
		if (frameTime > 16.67f)
		{
			frameTime = 16.67f;
		}

		static float dt = frameTime / 3.0f;
		float accumulator = 0.0f;

		while (accumulator < frameTime)
		{
			Euler(mBallTransforms, mSpheres, mRigidBodies, dt, BALL_COUNT);
			accumulator += dt;
		}
	}

	void Euler(Transform* transforms, Sphere* spheres, RigidBody* rigidBodies, float dt, int count)
	{
		for (int i = 0; i < BALL_COUNT; i++)
		{
			vec3f acceleration = rigidBodies[i].forces * rigidBodies[i].inverseMass;
			rigidBodies[i].velocity += acceleration * dt;

			vec3f position = transforms[i].GetPosition();
			position += rigidBodies[i].velocity * dt;
			transforms[i].SetPosition(position);
			spheres[i].origin = position;
		}
	}

	void DetectPlaneSphereCollisions(std::vector<Collision>* collisions, Plane* planes, int planeCount, Sphere* spheres, RigidBody* rigidBodies, int sphereCount)
	{
		vec3f poi;
		float t;

		for (int i = 0; i < planeCount; i++)
		{
			for (int j = 0; j < sphereCount; j++)
			{
#if DYNAMIC_COLLISION_TEST != 0
				if (IntersectDynamicSpherePlane<vec3f>(spheres[j], rigidBodies[j].velocity, planes[i], poi, t))
				{
					collisions->push_back(Collision(poi, t, i, j));
				}
#else
				if (IntersectSpherePlane<vec3f>(spheres[j], planes[i]))
				{
					poi = spheres[j].origin - (planes->normal * spheres[j].radius);
					t = 0.0f;
					collisions->push_back(Collision(poi, t, i, j));
				}
#endif
			}
		}
	}

	void DetectSphereSphereCollisions(std::vector<Collision>* collisions, Sphere* spheres, RigidBody* rigidBodies, int count)
	{
		vec3f poi;
		float t;

		for (int i = 0; i < count; i++)
		{
			for (int j = i + 1; j < count; j++)
			{
#if DYNAMIC_COLLISION_TEST != 0
				if (IntersectDynamicSphereSphere<vec3f>(spheres[i], rigidBodies[i].velocity, spheres[j], rigidBodies[j].velocity, poi, t))
				{
					collisions->push_back(Collision(poi, t, i, j));
				}
#else
				if (IntersectSphereSphere<vec3f>(spheres[i], spheres[j]))
				{
					poi = (spheres[i].origin + spheres[j].origin) * 0.5f;
					t = 0.0f;
					collisions->push_back(Collision(poi, t, i, j));
				}
#endif
			}
		}
	}

	void ResolvePlaneSphereCollisions(std::vector<Collision>* collisions, Plane* planes, Sphere* spheres, RigidBody* rigidBodies)
	{
		for (int i = 0; i < collisions->size(); i++)
		{
			vec3f& poi = collisions->at(i).poi;
			int i0 = collisions->at(i).s0;
			int i1 = collisions->at(i).s1;
			vec3f contactNormal = planes[i0].normal;

			float k = CalculateImpulse(spheres[i1], rigidBodies[i1], contactNormal, poi);
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
		}

		collisions->clear();
		collisions->reserve(BALL_COUNT);
	}

	void ResolveSphereSphereCollisions(std::vector<Collision>* collisions, Sphere* spheres, Transform* transforms, RigidBody* rigidBodies)
	{
		for (int i = 0; i < collisions->size(); i++)
		{
			vec3f& poi = collisions->at(i).poi;
			int i0 = collisions->at(i).s0;
			int i1 = collisions->at(i).s1;
			vec3f contactNormal = spheres[i1].origin - spheres[i0].origin;
			mat3f R0 = transforms[i0].GetRotationMatrix();
			mat3f R1 = transforms[i1].GetRotationMatrix();
			mat3f J0 = R0.transpose() * gSphereInverseTensor * R0;
			mat3f J1 = R1.transpose() * gSphereInverseTensor * R1;

			float k = CalculateImpulse(spheres[i0], spheres[i1], J0, J1, rigidBodies[i0], rigidBodies[i1], contactNormal, poi);
			rigidBodies[i0].velocity -= k * contactNormal * rigidBodies[i0].inverseMass;
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
			rigidBodies[i0].angularVelocity -= cliqCity::graphicsMath::cross(poi, k * contactNormal) * gSphereInverseTensor;
			rigidBodies[i1].angularVelocity += cliqCity::graphicsMath::cross(poi, k * contactNormal) * gSphereInverseTensor;
		}

		collisions->clear();
		collisions->reserve(BALL_COUNT);
	}

	inline float CalculateImpulse(Sphere& sphere, RigidBody& rigidBody, vec3f& normal, vec3f& poi)
	{
		vec3f vRel = -rigidBody.velocity;
		float numerator = (PLANE_SPHERE_ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(vRel, normal);
		float denominator = (rigidBody.inverseMass) * cliqCity::graphicsMath::dot(normal, normal);
		return numerator / denominator;
	}

	inline float CalculateImpulse(vec3f incoming, Sphere& sphere, RigidBody& rigidBody, vec3f& normal, vec3f& poi)
	{
		vec3f vRel = incoming - rigidBody.velocity;
		float numerator = (PLANE_SPHERE_ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(vRel, normal);
		float denominator = (rigidBody.inverseMass) * cliqCity::graphicsMath::dot(normal, normal);
		return numerator / denominator;
	}

	inline float CalculateImpulse(Sphere& s0, Sphere& s1, mat3f& J0, mat3f& J1, RigidBody& r0, RigidBody& r1, vec3f& normal, vec3f& poi)
	{
		vec3f u0 = r0.velocity + cliqCity::graphicsMath::cross(r0.angularVelocity, poi);
		vec3f u1 = r1.velocity + cliqCity::graphicsMath::cross(r1.angularVelocity, poi);
		vec3f uRel = u0 - u1;

		float numerator = (ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(uRel, normal);
		vec3f sumInverseMassxN = (r0.inverseMass + r1.inverseMass) * normal;
		vec3f rCrossN = cliqCity::graphicsMath::cross(poi, normal);
		vec3f r0CrossNxJ0 = rCrossN * J0;
		vec3f r1CrossNxJ1 = rCrossN * J1;
		float denominator = cliqCity::graphicsMath::dot(sumInverseMassxN + cliqCity::graphicsMath::cross(r0CrossNxJ0, poi) + cliqCity::graphicsMath::cross(r1CrossNxJ1, poi), normal);
		return numerator / denominator;
	}

	void ApplyFriction(Sphere* spheres, RigidBody* rigidBodies, int count)
	{
		
	}

	void ApplyImpulse(Sphere& sphere, RigidBody& rigidBody, vec3f normal)
	{
		vec3f poi = sphere.origin - sphere.radius * normal;
		vec3f velocity = normal * CUE_SPEED;
		float k = CalculateImpulse(velocity, sphere, rigidBody, normal, poi);
		rigidBody.velocity += k * normal * rigidBody.inverseMass;
	}
};

DECLARE_MAIN(BilliardsSample);