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
#define ROW_COUNT						5
#define BALL_RADIUS						0.105f
#define CAMERA_SPEED					0.01f
#define CAMERA_ROTATION_SPEED			0.05f
#define RADIAN							3.1415926535f / 180.0f
#define SURFACE_Y						2.15f
#define PLANE_COUNT						4
#define INVERSE_BALL_MASS				6.25f
#define BALL_MASS						0.16f			// kg
#define ELASTIC_CONSTANT				0.7f
#define PLANE_SPHERE_ELASTIC_CONSTANT	0.5f
#define KINETIC_FRICTION_CONSTANT		0.4f
#define STATIC_FRICTION_CONSTANT		0.015f
#define CUE_SPEED						0.03f			// m/ms^2
#define CUE_MASS						0.600f			// kg
#define CUE_INVERSE_MASS				2.22222222222f
#define PLANE_MASS						100.0f
#define PLANE_INVERSE_MASS				0.000000001f
#define LEFT_PLANE_DISTANCE				-1.68546724f
#define RIGHT_PLANE_DISTANCE			-1.47883308f
#define NEAR_PLANE_DISTANCE				-4.04303122f
#define FAR_PLANE_DISTANCE				-1.89119351f
#define TABLE_DEPTH						2.15183771f
#define TABLE_WIDTH						0.20663416f
#define PHYSICS_TIME_STEP				0.1f			// ms
#define GRAVITY_CONSTANT				0.0000098196f	// m/ms^2
#define LINEAR_VELOCITY_THRESHOLD		0.000995f
#define ANGULAR_VELOCITY_THRESHOLD		0.001f

#define ROTATIONAL_DYNAMICS				1

using namespace Rig3D;

static const int	gMeshMemorySize = 2048;

static const mat3f	gSphereInverseTensor = mat4f((2.0f * BALL_MASS * BALL_RADIUS * BALL_RADIUS) / 5.0f).inverse();
static const mat3f	gPlaneInverseTensor = mat4f(12.0f * PLANE_INVERSE_MASS * ( 1.0f / (TABLE_DEPTH * TABLE_DEPTH + TABLE_WIDTH * TABLE_WIDTH)));
static const vec3f  gTablePosition = { -1.0f, 0.0f, 1.0f };
static const vec3f  gSphereInverseTensorVector = vec3f(1.0f / ((2.0f / 5.0f) * BALL_MASS * BALL_RADIUS * BALL_RADIUS));
static const float  gKineticFriction = KINETIC_FRICTION_CONSTANT * BALL_MASS * GRAVITY_CONSTANT;
static const float  gStaticFriction = STATIC_FRICTION_CONSTANT * BALL_MASS * GRAVITY_CONSTANT;

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
		vec3f	torques;
		float	inverseMass;

		RigidBody() : velocity(0.0f), angularVelocity(0.0f), forces(0.0f), torques(0.0f), inverseMass(INVERSE_BALL_MASS) {};
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
		mOptions.mWindowCaption = "Billiards Sample";
		mOptions.mWindowWidth = 1200;
		mOptions.mWindowHeight = 900;
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
		int i = 1;
		for (int z = 0; z < ROW_COUNT; z++)
		{
			for (int x = 0; x < xCount; x++)
			{

				position = { xOffset, yOffset, zOffset };
				mBallTransforms[i].SetPosition(position);
				mSpheres[i].origin = position;
				mSpheres[i].radius = BALL_RADIUS;
				xOffset += diameter + FLT_EPSILON;
				i++;
			}

			xOffset = -(BALL_RADIUS * xCount * 0.5f);
			zOffset += diameter + FLT_EPSILON;
			xCount++;
		}

		mPlanes[0].normal = { +1.0f, +0.0f, +0.0f };	// Left Plane Facing Right
		mPlanes[1].normal = { +0.0f, +0.0f, -1.0f };	// Back Plane Facing Camera
		mPlanes[2].normal = { -1.0f, +0.0f, +0.0f };	// Right Plane Facing Left
		mPlanes[3].normal = { +0.0f, +0.0f, +1.0f };	// Front Plane Facing DirectX Forward

		mPlanes[0].distance = LEFT_PLANE_DISTANCE;
		mPlanes[1].distance = FAR_PLANE_DISTANCE;
		mPlanes[2].distance = RIGHT_PLANE_DISTANCE;
		mPlanes[3].distance = NEAR_PLANE_DISTANCE;

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
		const wchar_t* filenames[16] = {
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
			for (auto j = 0; j < textureDesc.MipLevels; j++)
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
		static int frame = 0;
		HandleInput();

		
		if (frame % 2 == 0)
		{
			// Physics
			ApplyFriction(mSpheres, mRigidBodies, BALL_COUNT);
			IntegrateBalls(milliseconds);
		}
		else 
		{
			// Collisions
			DetectSphereSphereCollisions(&mSphereCollisions, mSpheres, mRigidBodies, BALL_COUNT);
			DetectPlaneSphereCollisions(&mPlaneCollisions, mPlanes, PLANE_COUNT, mSpheres, mRigidBodies, BALL_COUNT);

			// Impulses
			ResolveSphereSphereCollisions(&mSphereCollisions, mSpheres, mBallTransforms, mRigidBodies);
			ResolvePlaneSphereCollisions(&mPlaneCollisions, mPlanes, mSpheres, mRigidBodies);
		}
		// TO DO: Interpolate State 
		
		// Update Renderable Structures
		UpdateBallTransforms();
		UpdateCamera();

		frame++;
	}

	void UpdateCamera()
	{
		mViewProjection.view = mat4f::lookAtLH(mCamera.mTransform.GetPosition() + mCamera.mTransform.GetForward(), mCamera.mTransform.GetPosition(), vec3f(0.0f, 1.0f, 0.0f)).transpose();
		//mViewProjection.view = mat4f::lookAtLH(gTablePosition, mCamera.mTransform.GetPosition(), vec3f(0.0f, 1.0f, 0.0f)).transpose();

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
			ApplyImpulse(mSpheres[0], mRigidBodies[0], vec3f(0.0f, 0.0f, 1.0f));
		}


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

		// CUE Ball
		position = mBallTransforms[0].GetPosition();
		if (Input::SharedInstance().GetKey(KEYCODE_UP)) {
			position += mBallTransforms[0].GetForward() * CAMERA_SPEED * 0.01f;
			mBallTransforms[0].SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_LEFT)) {
			position += mBallTransforms[0].GetRight() * -CAMERA_SPEED * 0.01f;
			mBallTransforms[0].SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_RIGHT)) {
			position += mBallTransforms[0].GetRight() * CAMERA_SPEED * 0.01f;
			mBallTransforms[0].SetPosition(position);
		}

		if (Input::SharedInstance().GetKey(KEYCODE_DOWN)) {
			position += mBallTransforms[0].GetForward() * -CAMERA_SPEED * 0.01f;
			mBallTransforms[0].SetPosition(position);
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
		float frameTime = static_cast<float>(milliseconds);
		if (frameTime > 16.67f)
		{
			frameTime = 16.67f;
		}

		static float accumulator = 0.0f;
		accumulator += frameTime;

		int i = 0;
		while (accumulator >= PHYSICS_TIME_STEP)
		{
			//Euler(mBallTransforms, mSpheres, mRigidBodies, PHYSICS_TIME_STEP, BALL_COUNT);
			RK4(mBallTransforms, mSpheres, mRigidBodies, PHYSICS_TIME_STEP, BALL_COUNT);
			accumulator -= PHYSICS_TIME_STEP;
			i++;
		}

		quatf r = mBallTransforms[0].GetRotation();
		vec3f v = mRigidBodies[0].velocity;
		float FPS = 1.0f / (frameTime / 1000.0f);
		char str[256];
		sprintf_s(str, "Billiards FPS %f FT %f STEPS %d CUE Velocity %3f %3f %3f Angular Velocity %3f %3f %3f", FPS, frameTime, i, v.x, v.y, v.z, mRigidBodies[0].angularVelocity.x, mRigidBodies[0].angularVelocity.y, mRigidBodies[0].angularVelocity.z);
		mRenderer->SetWindowCaption(str);
	}

	void Euler(Transform* transforms, Sphere* spheres, RigidBody* rigidBodies, float dt, int count)
	{
		for (int i = 0; i < BALL_COUNT; i++)
		{
			// Initial State
			vec3f acceleration			= rigidBodies[i].forces * rigidBodies[i].inverseMass;
			vec3f angularAcceleration	= rigidBodies[i].torques * gSphereInverseTensorVector;
			vec3f velocity				= rigidBodies[i].velocity;
			vec3f angularVelocity		= rigidBodies[i].angularVelocity;
			vec3f position				= transforms[i].GetPosition();
			quatf rotation				= transforms[i].GetRotation();

			// Integrate
			angularVelocity	+= angularAcceleration * dt;
			velocity		+= acceleration * dt;
			position		+= velocity * dt;
			rotation		+= (rotation * quatf(0.0f, angularVelocity)) * 0.5f * dt;

			// Next State
			rigidBodies[i].forces = rigidBodies[i].torques = vec3f(0.0f);
			rigidBodies[i].angularVelocity	= angularVelocity;
			rigidBodies[i].velocity			= velocity;
			spheres[i].origin				= position;
			transforms[i].SetPosition(position);
			transforms[i].SetRotation(cliqCity::graphicsMath::normalize(rotation));
		}
	}

	void RK4(Transform* transforms, Sphere* spheres, RigidBody* rigidBodies, float dt, int count)
	{
		for (int i = 0; i < BALL_COUNT; i++)
		{
			// Initial State
			quatf rotation				= transforms[i].GetRotation();
			vec3f position				= transforms[i].GetPosition();
			vec3f velocity				= rigidBodies[i].velocity;
			vec3f angularVelocity		= rigidBodies[i].angularVelocity;
			vec3f acceleration			= rigidBodies[i].forces * rigidBodies[i].inverseMass;
			vec3f angularAcceleration	= rigidBodies[i].torques * gSphereInverseTensorVector;
			quatf spin					= rotation * quatf(0.0f, angularVelocity) * 0.5f;

			// Derivatives
			vec3f v0, v1, v2, v3; // Velocity
			vec3f w0, w1, w2, w3; // Angular Velocity
			vec3f a0, a1, a2, a3; // Acceleration
			vec3f l0, l1, l2, l3; // Angular Acceleration
			quatf s0, s1, s2, s3; // Spin

			RK4Evaluate(transforms[i], rigidBodies[i], 0.0f, acceleration, angularAcceleration, spin, v0, w0, a0, l0, s0);
			RK4Evaluate(transforms[i], rigidBodies[i], dt * 0.5f, a0, l0, s0, v1, w1, a1, l1, s1);
			RK4Evaluate(transforms[i], rigidBodies[i], dt * 0.5f, a1, l1, s1, v2, w2, a2, l2, s2);
			RK4Evaluate(transforms[i], rigidBodies[i], dt, a2, l2, s2, v3, w3, a3, l3, s3);

			vec3f vf = 1.0f / 6.0f * (v0 + 2.0f * (v1 + v2) + v3);		// Final Velocity
			vec3f af = 1.0f / 6.0f * (a0 + 2.0f * (a1 + a2) + a3);		// Final Acceleration
			vec3f wf = 1.0f / 6.0f * (w0 + 2.0f * (w1 + w2) + w3);		// Final Angular Velocity
			vec3f lf = 1.0f / 6.0f * (l0 + 2.0f * (l1 + l2) + l3);		// Final Angular Acceleration
			quatf sf = (s0 + ((s1 + s2) * 2.0f) + s3) * 1.0f / 6.0f;	

			// Integration
			position		+= vf * dt;
			velocity		+= af * dt;
			angularVelocity += lf * dt;
			//rotation		+= sf * dt;
			rotation		+= rotation * quatf(0.0f, wf) * 0.5f * dt;

			// Set Dynamics State
			rigidBodies[i].forces = rigidBodies[i].torques = vec3f(0.0f);
			rigidBodies[i].angularVelocity	= angularVelocity * vec3f(1.0f, 0.02f, 1.0f);
			rigidBodies[i].velocity			= velocity;
			spheres[i].origin				= position;
			transforms[i].SetPosition(position);
			transforms[i].SetRotation(cliqCity::graphicsMath::normalize(rotation));
		}
	}

	inline void RK4Evaluate(const Transform& transform, const RigidBody& rigidBody, float dt, vec3f aIn, vec3f lIn, quatf sIn, vec3f& vOut, vec3f& wOut, vec3f& aOut, vec3f& lOut, quatf& sOut)
	{
		vOut = rigidBody.velocity + aIn * dt;
		wOut = rigidBody.angularVelocity + lIn * dt;
		sOut = transform.GetRotation() + sIn * dt;
		aOut = aIn;
		lOut = lIn;
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

	void ResolvePlaneSphereCollisions(std::vector<Collision>* collisions, Plane* planes, Sphere* spheres, Transform* transforms, RigidBody* rigidBodies)
	{
		for (auto i = 0; i < collisions->size(); i++)
		{
			vec3f& poi = collisions->at(i).poi;
			int i0 = collisions->at(i).s0;
			int i1 = collisions->at(i).s1;
			vec3f contactNormal = planes[i0].normal;
			mat3f R1 = transforms[i].GetRotation().toMatrix3();
			mat3f J1 = R1.transpose() * gSphereInverseTensor * R1;
			float poiMag = cliqCity::graphicsMath::magnitudeSquared(poi);
			float s = sqrt(poiMag - (planes[i0].distance * planes[i0].distance));
			vec3f p0 =  cliqCity::graphicsMath::cross(vec3f(0.0f, 1.0f, 0.0f), contactNormal) * s;
			vec3f p1 = poi - spheres[i].origin;

			float k = CalculatePlaneSphereImpulse(spheres[i1], J1, rigidBodies[i1], contactNormal, p0, p1);
			vec3f g = k * contactNormal * rigidBodies[i1].inverseMass;
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
			rigidBodies[i1].angularVelocity += cliqCity::graphicsMath::cross(p1, k * contactNormal) * gSphereInverseTensor;
		}

		collisions->clear();
		collisions->reserve(BALL_COUNT);
	}

	void ResolvePlaneSphereCollisions(std::vector<Collision>* collisions, Plane* planes, Sphere* spheres, RigidBody* rigidBodies)
	{
		for (auto i = 0; i < collisions->size(); i++)
		{
			vec3f& poi = collisions->at(i).poi;
			int i0 = collisions->at(i).s0;
			int i1 = collisions->at(i).s1;
			vec3f contactNormal = planes[i0].normal;

			float k = CalculatePlaneSphereImpulse(spheres[i1], rigidBodies[i1], contactNormal);
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
		}

		collisions->clear();
		collisions->reserve(BALL_COUNT);
	}

	void ResolveSphereSphereCollisions(std::vector<Collision>* collisions, Sphere* spheres, Transform* transforms, RigidBody* rigidBodies)
	{
		for (auto i = 0; i < collisions->size(); i++)
		{
			vec3f& poi = collisions->at(i).poi;
			int i0 = collisions->at(i).s0;
			int i1 = collisions->at(i).s1;
			vec3f distance = spheres[i1].origin - spheres[i0].origin;
			float distanceMagnitude = cliqCity::graphicsMath::magnitude(distance);
			vec3f contactNormal = distance / distanceMagnitude; //cliqCity::graphicsMath::normalize(spheres[i1].origin - spheres[i0].origin);
			mat3f R0 = transforms[i0].GetRotationMatrix();
			mat3f R1 = transforms[i1].GetRotationMatrix();
			mat3f J0 = R0.transpose() * gSphereInverseTensor * R0;
			mat3f J1 = R1.transpose() * gSphereInverseTensor * R1;
			vec3f p0 = poi - spheres[i0].origin;
			vec3f p1 = poi - spheres[i1].origin;

#if ROTATIONAL_DYNAMICS == 1
			float k = CalculateSphereSphereImpulse(spheres[i0], spheres[i1], J0, J1, rigidBodies[i0], rigidBodies[i1], contactNormal, p0, p1);
			rigidBodies[i0].velocity -= k * contactNormal * rigidBodies[i0].inverseMass;
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
			//rigidBodies[i0].angularVelocity -= cliqCity::graphicsMath::cross(p0, k * contactNormal) * gSphereInverseTensor;
			//rigidBodies[i1].angularVelocity += cliqCity::graphicsMath::cross(p1, k * contactNormal) * gSphereInverseTensor;
#else
			float k = CalculateSphereSphereImpulse(spheres[i0], spheres[i1], rigidBodies[i0], rigidBodies[i1], contactNormal);
			rigidBodies[i0].velocity -= k * contactNormal * rigidBodies[i0].inverseMass;
			rigidBodies[i1].velocity += k * contactNormal * rigidBodies[i1].inverseMass;
#endif

			float m = ((BALL_RADIUS + BALL_RADIUS) - distanceMagnitude) * 0.5f;
			vec3f jPos = spheres[i0].origin - contactNormal * m;
			vec3f iPos = spheres[i1].origin + contactNormal * m;
			spheres[i0].origin = jPos;
			spheres[i1].origin = iPos;
			transforms[i0].SetPosition(jPos);
			transforms[i1].SetPosition(iPos);
		}

		collisions->clear();
		collisions->reserve(BALL_COUNT);
	}

	inline float CalculateSphereSphereImpulse(Sphere& s0, Sphere& s1, mat3f& J0, mat3f& J1, RigidBody& r0, RigidBody& r1, vec3f& normal, vec3f& poi0, vec3f& poi1)
	{
		//                              ((e + 1) * uRel * n)
		// -------------------------------------------------------------------------------
		// [((1/m1 + 1/m2) * n) + ((r1 x n) * J1^-1) x r1) + ((r2 x n) * J2^-1) x r2)] * n

		// uRel: Relative point velocity
		vec3f u0 = r0.velocity + cliqCity::graphicsMath::cross(r0.angularVelocity, poi0);
		vec3f u1 = r1.velocity + cliqCity::graphicsMath::cross(r1.angularVelocity, poi1);
		vec3f uRel = u0 - u1;

		float numerator = (ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(uRel, normal);
		vec3f sumInverseMassxN = (r0.inverseMass + r1.inverseMass) * normal;
		vec3f p0CrossN = cliqCity::graphicsMath::cross(poi0, normal);
		vec3f p1CrossN = cliqCity::graphicsMath::cross(poi1, normal);
		vec3f p0CrossNxJ0 = p0CrossN * J0;
		vec3f p1CrossNxJ1 = p1CrossN * J1;
		float denominator = cliqCity::graphicsMath::dot(sumInverseMassxN + cliqCity::graphicsMath::cross(p0CrossNxJ0, poi0) + cliqCity::graphicsMath::cross(p1CrossNxJ1, poi1), normal);
		return numerator / denominator;
	}

	inline float CalculateSphereSphereImpulse(Sphere& s0, Sphere& s1, RigidBody& r0, RigidBody& r1, vec3f& normal)
	{
		vec3f vRel = r0.velocity - r1.velocity;
		float numerator = (ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(vRel, normal);
		float denominator = cliqCity::graphicsMath::dot((r0.inverseMass + r1.inverseMass) * normal, normal);
		return numerator / denominator;
	}

	inline float CalculatePlaneSphereImpulse(Sphere& sphere, mat3f& J1, RigidBody& rigidBody, vec3f& normal, vec3f& poi0, vec3f& poi1)
	{
		vec3f uRel = rigidBody.velocity + cliqCity::graphicsMath::cross(rigidBody.angularVelocity, poi1);

		float numerator = (PLANE_SPHERE_ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(-uRel, normal);
		vec3f sumInverseMassxN = (PLANE_INVERSE_MASS + rigidBody.inverseMass) * normal;
		vec3f p0CrossN = cliqCity::graphicsMath::cross(poi0, normal);
		vec3f p1CrossN = cliqCity::graphicsMath::cross(poi1, normal);
		vec3f p0CrossNxJ0 = vec3f(0.0f);//p0CrossN * gPlaneInverseTensor;
		vec3f p1CrossNxJ1 = p1CrossN * J1;
		float denominator = cliqCity::graphicsMath::dot(sumInverseMassxN + cliqCity::graphicsMath::cross(p0CrossNxJ0, poi0) + cliqCity::graphicsMath::cross(p1CrossNxJ1, poi1), normal);
		return numerator / denominator;
	}

	inline float CalculatePlaneSphereImpulse(Sphere& sphere, RigidBody& rigidBody, vec3f& normal)
	{
		vec3f vRel = rigidBody.velocity;
		float numerator = (PLANE_SPHERE_ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(-vRel, normal);
		float denominator = cliqCity::graphicsMath::dot((rigidBody.inverseMass) * normal, normal);
		return numerator / denominator;
	}

	inline float CalculateImpulse(vec3f incoming, Sphere& sphere, RigidBody& rigidBody, vec3f& normal, vec3f& poi)
	{
		vec3f vRel = incoming - rigidBody.velocity;
		float numerator = (PLANE_SPHERE_ELASTIC_CONSTANT + 1.0f) * cliqCity::graphicsMath::dot(vRel, normal);
		float denominator = (CUE_INVERSE_MASS + rigidBody.inverseMass) * cliqCity::graphicsMath::dot(normal, normal);
		return numerator / denominator;
	}

	void ApplyFriction(Sphere* spheres, RigidBody* rigidBodies, int count)
	{
		vec3f r = { 0.0f, -BALL_RADIUS, 0.0f };
		for (int i = 0; i < count; i++)
		{

			// NOT MOVING
			if ((-LINEAR_VELOCITY_THRESHOLD <= rigidBodies[i].velocity.x && rigidBodies[i].velocity.x <= LINEAR_VELOCITY_THRESHOLD) &&
				(-LINEAR_VELOCITY_THRESHOLD <= rigidBodies[i].velocity.z && rigidBodies[i].velocity.z <= LINEAR_VELOCITY_THRESHOLD))
			{
				rigidBodies[i].velocity = rigidBodies[i].angularVelocity = vec3f(0.0f);
			}
			else
			{
				float velocity			= cliqCity::graphicsMath::magnitude(rigidBodies[i].velocity);
				float angularVelocity	= cliqCity::graphicsMath::magnitude(rigidBodies[i].angularVelocity * BALL_RADIUS);
				vec3f normal			= -(rigidBodies[i].velocity / velocity);
				float slidingPercentage = angularVelocity / velocity;

				vec3f f1 = normal * gKineticFriction * (1 - min(slidingPercentage, 1.0f));	// Applies torque in direction opposite linear velocity 
				vec3f f2 = normal * gStaticFriction * min(slidingPercentage, 1.0f);			// Applies torque in direction of linear velocity
				vec3f t1 = cliqCity::graphicsMath::cross(r, f1);
				vec3f t2 = cliqCity::graphicsMath::cross(r, f2);
				rigidBodies[i].forces	+= f1 + f2;
				rigidBodies[i].torques	+= t1 - t2;
			}
		}
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