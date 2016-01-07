#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include "Rig3D\Graphics\MeshLibrary.h"
#include "Rig3D\Graphics\Interface\IShader.h"
#include "Rig3D\Graphics\Interface\IShaderResource.h"
#include "Rig3D/Intersection.h"
#include "Rig3D/Visibility.h"
#include "Rig3D/Graphics/Camera.h"
#include <d3d11.h>
#include <ctime>
#include <random>

#define RAND(range, offset)					(std::rand() % range) - offset

#define PI							3.1415926535f
#define CAMERA_SPEED				0.1f
#define CAMERA_ROTATION_SPEED		0.1f
#define RADIAN						3.1415926535f / 180.0f
#define INSTANCE_COUNT				20
#define SCENE_MEMORY				10240
#define BOID_NEIGHBOR_RADIUS		2.5f
#define BOID_SEPARATION_RADIUS		(BOID_NEIGHBOR_RADIUS * 0.6f)
#define OBSTACLE_AVOIDANCE_WEIGHT	0.2f
#define BOID_INVERSE_MASS			0.2f
#define PHYSICS_TIME_STEP			0.001f			// ms
#define MAX_BOID_SPEED				7.0f

using namespace Rig3D;

static vec4f gBoidColor  = { 0.0f, 0.6f, 0.4f, 1.0f};
static vec4f gPlaneColor = { 0.5f, 0.5f, 0.5f, 1.0f };


static float gSeparationWeight	= 0.4f;
static float gAlignmentWeight	= 0.1f;
static float gCohesionWeight	= 0.1f;

uint8_t gSceneMemory[SCENE_MEMORY];

struct Vertex3
{
	vec3f Position;
	vec3f Normal;
	vec2f UV;
};

struct RigidBody
{
	vec3f velocity;
	vec3f forces;
};

struct Boid
{
	vec3f			nPositions[3];
	vec3f			nDirections[3];
	float			nDistances[3];
	uint32_t		nIndices[3];
	
	Transform*		transform;
	RigidBody*		rigidBody;
	SphereCollider* collider;
};

struct ViewProjection
{
	mat4f view;
	mat4f projection;
};

class GroupMotionSample : public IScene, public IRendererDelegate
{
public:
	ViewProjection		mViewProjection;
	mat4f				mPlaneWorldMatrix;
	Camera				mCamera;
	Plane<vec3f>		mPlanes[6];
	float				mMouseX;
	float				mMouseY;

	LinearAllocator		mLinearAllocator;
	
	mat4f*				mBoidWorldMatrices;
	
	Transform*			mBoidTransforms;
	RigidBody*			mBoidRigidBodies;
	SphereCollider*		mBoidColliders;
	Boid*				mBoids;

	TSingleton<IRenderer, DX3D11Renderer>*	mRenderer;
	IShader*			mBoidVertexShader;
	IShader*			mBoidPixelShader;
	IShaderResource*	mBoidShaderResouce;
	IMesh*				mBoidMesh;
	IMesh*				mPlaneMesh;

	GroupMotionSample();
	~GroupMotionSample();

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

	void UpdateBoidBehaviors();
	void UpdateBoidForces();
	
	void IntegrateBoids(float deltaTime);
	void ResetBoids();
};

GroupMotionSample::GroupMotionSample() : 
	mMouseX(0.0f),
	mMouseY(0.0f),
	mLinearAllocator(gSceneMemory, gSceneMemory + SCENE_MEMORY),
	mBoidWorldMatrices(nullptr),
	mBoidTransforms(nullptr),
	mBoidRigidBodies(nullptr),
	mBoidColliders(nullptr),
	mBoids(nullptr),
	mRenderer(nullptr),
	mBoidVertexShader(nullptr),
	mBoidPixelShader(nullptr),
	mBoidShaderResouce(nullptr),
	mBoidMesh(nullptr),
	mPlaneMesh(nullptr)
{
	mOptions.mWindowCaption = "Group Motion Sample";
	mOptions.mWindowWidth = 1200;
	mOptions.mWindowHeight = 900;
	mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
	mOptions.mFullScreen = false;
}

GroupMotionSample::~GroupMotionSample()
{
	
}

void GroupMotionSample::VInitialize()
{
	mRenderer = mEngine->GetRenderer();
	mRenderer->SetDelegate(this);

	time_t now;
	time(&now);
	srand(static_cast<unsigned>(now));

	InitializeGeometry();
	InitializePhysics();
	InitializeShaders();
	InitializeShaderResources();
	VOnResize();
}

void GroupMotionSample::InitializeGeometry()
{
	OBJBasicResource<Vertex3> boidResource("Models\\boid.obj");

	MeshLibrary<LinearAllocator> meshLibrary;
	meshLibrary.SetAllocator(&mLinearAllocator);
	meshLibrary.LoadMesh(&mBoidMesh, mRenderer, boidResource);

	mCamera.mTransform.SetPosition({ 0.0f, 0.0f, -20.0f });

	mPlanes[0].normal = vec3f(0.0f, 0.0f, -1.0f);	// Front
	mPlanes[1].normal = vec3f(0.0f, 0.0f, 1.0f);	// Back
	mPlanes[2].normal = vec3f(-1.0f, 0.0f, 0.0f);	// Left
	mPlanes[3].normal = vec3f(1.0f, 0.0f, 0.0f);	// Right
	mPlanes[4].normal = vec3f(0.0f, 1.0f, 0.0f);	// Top
	mPlanes[5].normal = vec3f(0.0f, -1.0f, 0.0f);	// Bottom

	for (int i = 0; i < 6; i++)
	{
		mPlanes[i].distance = 10.0f;
	}

	// Plane

	float planeHalfWidth = 400.0f;
	float planeHalfDepth = 400.0f;
	float planeYOffset = 10.0f;

	Vertex3 vertices[4];
	vertices[0].Position = { -planeHalfWidth, -planeYOffset, -planeHalfDepth };
	vertices[1].Position = { -planeHalfWidth, -planeYOffset, +planeHalfDepth };
	vertices[2].Position = { +planeHalfWidth, -planeYOffset, +planeHalfDepth };
	vertices[3].Position = { +planeHalfWidth, -planeYOffset, -planeHalfDepth };

	vertices[0].Normal = { 0.0f, 1.0f, 0.0f };
	vertices[1].Normal = { 0.0f, 1.0f, 0.0f };
	vertices[2].Normal = { 0.0f, 1.0f, 0.0f };
	vertices[3].Normal = { 0.0f, 1.0f, 0.0f };

	vertices[0].UV = { 0.0f, 1.0f };
	vertices[1].UV = { 0.0f, 0.0f };
	vertices[2].UV = { 1.0f, 0.0f };
	vertices[3].UV = { 1.0f, 1.0f };

	uint16_t indices[6] =
	{
		0, 1, 2,
		2, 3, 0
	};

	meshLibrary.NewMesh(&mPlaneMesh, mRenderer);
	mRenderer->VSetMeshVertexBuffer(mPlaneMesh, vertices, sizeof(Vertex3) * 4, sizeof(Vertex3));
	mRenderer->VSetMeshIndexBuffer(mPlaneMesh, indices, 6);

	mPlaneWorldMatrix = mat4f::translate({ 0.0f, -15.0f, 0.0f }).transpose();
}

void GroupMotionSample::InitializePhysics()
{
	mBoidWorldMatrices	= reinterpret_cast<mat4f*>(mLinearAllocator.Allocate(sizeof(mat4f) * INSTANCE_COUNT, alignof(mat4f), 0));
	mBoidTransforms		= reinterpret_cast<Transform*>(mLinearAllocator.Allocate(sizeof(Transform) * INSTANCE_COUNT, alignof(Transform), 0));
	mBoidRigidBodies	= reinterpret_cast<RigidBody*>(mLinearAllocator.Allocate(sizeof(RigidBody) * INSTANCE_COUNT, alignof(RigidBody), 0));
	mBoidColliders		= reinterpret_cast<SphereCollider*>(mLinearAllocator.Allocate(sizeof(SphereCollider) * INSTANCE_COUNT, alignof(SphereCollider), 0));
	mBoids				= reinterpret_cast<Boid*>(mLinearAllocator.Allocate(sizeof(Boid) * INSTANCE_COUNT, alignof(Boid), 0));

	ResetBoids();
}

void GroupMotionSample::InitializeShaders()
{
	mRenderer->VCreateShader(&mBoidVertexShader, &mLinearAllocator);
	mRenderer->VCreateShader(&mBoidPixelShader, &mLinearAllocator);

	InputElement boidInputElements[] =
	{
		{ "POSITION",	0, 0, 0, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "NORMAL",		0, 0, 12, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 24, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "WORLD",		0, 1, 0, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		1, 1, 16, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		2, 1, 32, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		3, 1, 48, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE }
	};

	mRenderer->VLoadVertexShader(mBoidVertexShader, "BoidVertexShader.cso", boidInputElements, 7);
	mRenderer->VLoadPixelShader(mBoidPixelShader, "BoidPixelShader.cso");
}

void GroupMotionSample::InitializeShaderResources()
{
	mRenderer->VCreateShaderResource(&mBoidShaderResouce, &mLinearAllocator);

	void*	instanceData[]			= { &mBoidWorldMatrices };
	size_t	instanceDataSizes[]		= { sizeof(mat4f) * INSTANCE_COUNT };
	size_t	instanceDataStrides[]	= { sizeof(mat4f) };
	size_t	instanceDataOffsets[]	= { 0 };

	mRenderer->VCreateDynamicShaderInstanceBuffers(mBoidShaderResouce, instanceData, instanceDataSizes, instanceDataStrides, instanceDataOffsets, 1);

	void*	constantData[]		= { &mViewProjection, nullptr };
	size_t	constantDataSize[]	= { sizeof(ViewProjection), sizeof(vec4f) };

	mRenderer->VCreateShaderConstantBuffers(mBoidShaderResouce, constantData, constantDataSize, 2);
}

void GroupMotionSample::UpdateCamera()
{
	mCamera.SetViewMatrix(mat4f::lookAtLH(mCamera.mTransform.GetForward(), mCamera.mTransform.GetPosition(), vec3f(0.0f, 1.0f, 0.0f)));
}

void GroupMotionSample::VUpdate(double milliseconds)
{
	UpdateInput(Input::SharedInstance());
	UpdateCamera();

	UpdateBoidBehaviors();
	UpdateBoidForces();
	IntegrateBoids(static_cast<float>(milliseconds) * 0.001f);	// Convert to seconds

	UpdateShaderResources();	

	Boid* b = &mBoids[0];
	Boid* b1 = &mBoids[1];

	vec3f p = b->transform->GetPosition();
	char str[256];
	sprintf_s(str, "Group Motion Sample  S: %f  A: %f  C: %f  B0: %f %f %f B1: %f %f %f", gSeparationWeight, gAlignmentWeight, gCohesionWeight, b->rigidBody->velocity.x, b->rigidBody->velocity.y, b->rigidBody->velocity.z,
		b1->rigidBody->velocity.x, b1->rigidBody->velocity.y, b1->rigidBody->velocity.z);
	mRenderer->SetWindowCaption(str);
}

void GroupMotionSample::UpdateInput(Input& input)
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

	if (input.GetKey(KEYCODE_R))
	{
		ResetBoids();
	}

	float step = 0.05f;
	if (input.GetKey(KEYCODE_C) && input.GetKeyDown(KEYCODE_UP))
	{
		gCohesionWeight += step;
	}

	if (input.GetKey(KEYCODE_C) && input.GetKeyDown(KEYCODE_DOWN))
	{
		gCohesionWeight -= step;
	}

	if (input.GetKey(KEYCODE_V) && input.GetKeyDown(KEYCODE_UP))
	{
		gAlignmentWeight += step;
	}

	if (input.GetKey(KEYCODE_V) && input.GetKeyDown(KEYCODE_DOWN))
	{
		gAlignmentWeight -= step;
	}

	if (input.GetKey(KEYCODE_O) && input.GetKeyDown(KEYCODE_UP))
	{
		gSeparationWeight += step;
	}

	if (input.GetKey(KEYCODE_O) && input.GetKeyDown(KEYCODE_DOWN))
	{
		gSeparationWeight -= step;
	}
}

void GroupMotionSample::UpdateBoidBehaviors()
{
	for (int i = 0; i < INSTANCE_COUNT; i++)
	{
		Boid* boid = &mBoids[i];

		vec3f separation = vec3f(0.0f);
		vec3f alignment = vec3f(0.0f);
		vec3f cohesion = vec3f(0.0f);

		float separationCount = 0.0f;
		float alignmentCount = 0.0f;
		float cohesionCount = 0.0f;

		float separationMagnitude = 0.0f;
		float alignmentMagnitude = 0.0f;
		float cohesionMagnitude = 0.0f;

		for (int j = 0; j < INSTANCE_COUNT; j++)
		{
			if (i == j)
			{
				continue;
			}

			vec3f nPosition = mBoids[j].transform->GetPosition();
			vec3f toNeighbor = nPosition - boid->transform->GetPosition();
			float distance = cliqCity::graphicsMath::magnitude(toNeighbor);

			if (distance > FLT_EPSILON && distance < BOID_NEIGHBOR_RADIUS)
			{
				if (distance < BOID_SEPARATION_RADIUS)
				{
					separation += (toNeighbor * (BOID_SEPARATION_RADIUS / -distance));
					separationCount++;
				}

				alignment += mBoids[j].rigidBody->velocity;
				cohesion += nPosition;

				alignmentCount++;
				cohesionCount++;
			}
		}

		if (separationCount > 0.0f)
		{
			separation /= separationCount;
		}

		if (alignmentCount > 0.0f)
		{
			alignment /= alignmentCount;

			alignmentMagnitude = cliqCity::graphicsMath::magnitude(alignment);
			if (alignmentMagnitude > 0.0f)
			{
				alignment /= alignmentMagnitude;
			}
		}

		if (cohesionCount > 0.0f)
		{
			cohesion /= cohesionCount;
			cohesion -= boid->transform->GetPosition();

			cohesionMagnitude = cliqCity::graphicsMath::magnitude(cohesion);

			if (cohesionMagnitude > 0)
			{
				cohesion /= cohesionMagnitude;
			}

			//if (cohesionMagnitude < 7.0f)
			//{
			//	cohesion *= 5.0f * (cohesion / 7.0f);
			//}
			//else
			//{
			//	cohesion *= 5.0f;
			//}

			cohesion -= boid->rigidBody->velocity;
		}

		separation	*= gSeparationWeight;
		cohesion	*= gCohesionWeight;
		alignment	*= gAlignmentWeight;

		boid->rigidBody->velocity += separation + alignment + cohesion;

		float speed = cliqCity::graphicsMath::magnitude(boid->rigidBody->velocity);
		if (speed > MAX_BOID_SPEED)
		{
			boid->rigidBody->velocity *= (MAX_BOID_SPEED / speed);
		}
	}
}

void GroupMotionSample::UpdateBoidForces()
{
	for (int i = 0; i < INSTANCE_COUNT; i++)
	{
		Boid* boid = &mBoids[i];

		vec3f obstacleAvoidance = 0.0f;
		for (int j = 0; j < 6; j++)
		{
			float d = cliqCity::graphicsMath::dot(mPlanes[j].normal, boid->collider->origin) - mPlanes[j].distance;
			if (d < 0.0f)
			{
				d = (5.0f / d);
			}
			else
			{
				d = -d;
			}

			obstacleAvoidance += (mPlanes[j].normal * d);
		}

	//	obstacleAvoidance /= 6.0f;

		boid->rigidBody->forces += obstacleAvoidance;
	}
}

void GroupMotionSample::IntegrateBoids(float deltaTime)
{
	if (deltaTime > 0.01667f)
	{
		deltaTime = 0.01667f;
	}

	static float accumulator = 0.0f;
	accumulator += deltaTime;

	while (accumulator >= PHYSICS_TIME_STEP)
	{
		static const vec3f zero = vec3f(0.0f);

		for (int i = 0; i < INSTANCE_COUNT; i++)
		{
			Boid* boid = &mBoids[i];
			vec3f acceleration = boid->rigidBody->forces * BOID_INVERSE_MASS;
			vec3f velocity = boid->rigidBody->velocity;
			vec3f position = boid->transform->GetPosition();


			vec3f rotation = boid->transform->GetRollPitchYaw();

			velocity += acceleration * deltaTime;
			position += velocity * deltaTime;

			float cosAngle = cliqCity::graphicsMath::dot(boid->transform->GetForward(), cliqCity::graphicsMath::normalize(velocity)) * 0.1f;

			rotation = { PI * 0.5f, acos(cosAngle), 0.0f };

			boid->rigidBody->velocity = velocity;
			boid->transform->SetPosition(position);
			boid->transform->SetRotation(rotation);
			boid->collider->origin = position;
			boid->rigidBody->forces = zero;
		}

		accumulator -= PHYSICS_TIME_STEP;
	}
}

void GroupMotionSample::ResetBoids()
{
	float angle = 2.0f * PI / INSTANCE_COUNT;
	for (int i = 0; i < INSTANCE_COUNT; i++)
	{
		vec3f scale = { 1.0f, 1.0f, 1.0f };
		vec3f position = { 5.0f * cos(angle * i), 5.0f * sin(angle*  i), 0.0f };
		quatf rotation = { 1.0f, 0.0f, 0.0f, 0.0f };
		vec3f velocity = { static_cast<float>(RAND(5, 0.2f)), 0.0f, static_cast<float>(RAND(6, 0.1f)) };

		mBoidTransforms[i].SetPosition(position);
		mBoidTransforms[i].SetScale(scale);
		mBoidTransforms[i].SetRotation(rotation);

		mBoidRigidBodies[i].velocity = velocity;

		mBoidColliders[i].origin = position;
		mBoidColliders[i].radius = BOID_NEIGHBOR_RADIUS;

		mBoids[i].transform = &mBoidTransforms[i];
		mBoids[i].rigidBody = &mBoidRigidBodies[i];
		mBoids[i].collider = &mBoidColliders[i];

		mBoids[i].nDistances[0] = FLT_MAX;
		mBoids[i].nDistances[1] = FLT_MAX;
		mBoids[i].nDistances[2] = FLT_MAX;
	}

	mBoids[0].rigidBody->velocity = { -5.0f, 5.0f, 0.0f };
	mBoids[1].rigidBody->velocity = { 5.0f, -5.0f, 0.0f };
	mBoids[2].rigidBody->velocity = { 5.0f, 5.0f, 0.0f };

}

void GroupMotionSample::UpdateShaderResources()
{
	for (int i = 0; i < INSTANCE_COUNT; i++)
	{
		mBoidWorldMatrices[i] = mBoidTransforms[i].GetWorldMatrix().transpose();
	}

	mViewProjection.view = mCamera.GetViewMatrix().transpose();
	mViewProjection.projection = mCamera.GetProjectionMatrix().transpose();
}

void GroupMotionSample::VRender()
{
	float color[] = { 0.0f, 0.7294117647f, 1.0f, 1.0f };

	mRenderer->VSetContextTargetWithDepth();
	mRenderer->VClearContext(color, 1.0f, 0);

	mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
	mRenderer->VSetInputLayout(mBoidVertexShader);
	mRenderer->VSetVertexShader(mBoidVertexShader);
	mRenderer->VSetPixelShader(mBoidPixelShader);

	mRenderer->VBindMesh(mBoidMesh);

	mRenderer->VUpdateShaderInstanceBuffer(mBoidShaderResouce, mBoidWorldMatrices, sizeof(mat4f) * INSTANCE_COUNT, 0);
	mRenderer->VUpdateShaderConstantBuffer(mBoidShaderResouce, &mViewProjection, 0);
	mRenderer->VUpdateShaderConstantBuffer(mBoidShaderResouce, &gBoidColor, 1);
	mRenderer->VSetVertexShaderInstanceBuffers(mBoidShaderResouce);
	mRenderer->VSetVertexShaderConstantBuffer(mBoidShaderResouce, 0, 0);
	mRenderer->VSetPixelShaderConstantBuffer(mBoidShaderResouce, 1, 0);

	ID3D11DeviceContext* deviceContext = mRenderer->GetDeviceContext();
	deviceContext->DrawIndexedInstanced(mBoidMesh->GetIndexCount(), INSTANCE_COUNT, 0, 0, 0);

	mRenderer->VBindMesh(mPlaneMesh);

	mRenderer->VUpdateShaderInstanceBuffer(mBoidShaderResouce, &mPlaneWorldMatrix, sizeof(mat4f), 0);
	mRenderer->VUpdateShaderConstantBuffer(mBoidShaderResouce, &gPlaneColor, 1);
	mRenderer->VSetVertexShaderInstanceBuffers(mBoidShaderResouce);
	mRenderer->VSetVertexShaderConstantBuffer(mBoidShaderResouce, 0, 0);
	mRenderer->VSetPixelShaderConstantBuffer(mBoidShaderResouce, 1, 0);

	deviceContext->DrawIndexedInstanced(mPlaneMesh->GetIndexCount(), INSTANCE_COUNT, 0, 0, 0);

	mRenderer->VSwapBuffers();
}

void GroupMotionSample::VShutdown()
{
	mBoidVertexShader->~IShader();
	mBoidPixelShader->~IShader();
	mBoidShaderResouce->~IShaderResource();
	mBoidMesh->~IMesh();
	mLinearAllocator.Free();
}

void GroupMotionSample::VOnResize()
{
	mCamera.SetProjectionMatrix(mat4f::normalizedPerspectiveLH(PI * 0.25f, mRenderer->GetAspectRatio(), 0.1f, 100.0f));
}

DECLARE_MAIN(GroupMotionSample);