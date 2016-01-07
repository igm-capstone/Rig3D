#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include "Rig3D\Graphics\MeshLibrary.h"
#include <d3d11.h>
#include "Rig3D\Graphics\Interface\IShader.h"
#include "Rig3D\Graphics\Interface\IShaderResource.h"
#include "BVHResource.h"
#include <vector>
#include <utility>
#include <Rig3D\Graphics\DirectX11\DirectXTK\Inc\WICTextureLoader.h>

#define PI						3.1415926535f
#define CAMERA_SPEED			0.1f
#define CAMERA_ROTATION_SPEED	0.1f
#define RADIAN					3.1415926535f / 180.0f
#define INTERPOLATION			0
#define BVH_FILENAME			"BVH\\Tiptoe.bvh"

using namespace Rig3D;

typedef	std::vector<std::pair<uint32_t, uint32_t>> PairVector;

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

struct ModelViewProjection
{
	mat4f View;
	mat4f Projection;
};

class MotionCaptureSample : public IScene, public virtual IRendererDelegate
{
public:
	ModelViewProjection			mViewProjection;
	mat4f						mPlaneWorldMatrix;

	Transform					mCamera;
	BVHResource					mBVHResource;

	LinearAllocator				mAllocator;

	PairVector					mPairVector;
	mat4f*						mJointWorldMatrices;
	vec3f*						mLineVertices;
	Transform*					mTransforms;
	IMesh*						mCubeMesh;
	IMesh*						mPlaneMesh;

	TSingleton<IRenderer, DX3D11Renderer>*	mRenderer;
	IShader*					mVertexShader;
	IShader*					mPixelShader;
	IShader*					mLineVertexShader;
	IShader*					mLinePixelShader;
	IShader*					mPlaneVertexShader;
	IShader*					mPlanePixelShader;

	IShaderResource*			mVertexShaderResource;
	IShaderResource*			mPlaneVertexShaderResource;

	ID3D11Buffer*				mLineVertexBuffer;
	ID3D11ShaderResourceView*	mCheckerboardSRV;
	ID3D11SamplerState*			mSamplerState;

	uint32_t					mTransformCount;

	float						mMouseX;
	float						mMouseY;
	float						mAnimationDuration;
	float						mAnimationScale;

	MotionCaptureSample();
	~MotionCaptureSample();

	void VInitialize() override;
	void VUpdate(double milliseconds) override;
	void VRender() override;
	void VShutdown() override;
	void VOnResize() override;

	void InitializeGeometry();
	void InitializeBVHResources();
	void InitializeTransforms(Transform* transforms, PairVector* pairVector, const BVHJoint* joint);

	void InitializeShaders();

	void UpdateCamera();
	void UpdateTransforms(Transform* transforms, const BVHJoint* joint, const BVHMotion* motion, const uint32_t& transformCount, const uint32_t& frameIndex, const float& u);
	void UpdateJointWorldMatrices(mat4f* jointWorldMatrices, Transform* transforms, const uint32_t& count);
	void UpdateLineVertices(vec3f* lineVertices, PairVector* pairVector, mat4f* jointWorldMatrices);
	void HandleInput(Input& input);
};

MotionCaptureSample::MotionCaptureSample() : 
	mAllocator(10240),
	mJointWorldMatrices(nullptr),
	mLineVertices(nullptr),
	mTransforms(nullptr), 
	mCubeMesh(nullptr), 
	mPlaneMesh(nullptr),
	mRenderer(nullptr),
	mVertexShader(nullptr),
	mPixelShader(nullptr),
	mLineVertexShader(nullptr),
	mLinePixelShader(nullptr),
	mPlaneVertexShader(nullptr),
	mPlanePixelShader(nullptr),
	mVertexShaderResource(nullptr),
	mPlaneVertexShaderResource(nullptr),
	mLineVertexBuffer(nullptr),
	mCheckerboardSRV(nullptr),
	mSamplerState(nullptr),
	mTransformCount(0),
	mMouseX(0.0f),
	mMouseY(0.0f),
	mAnimationDuration(0.0f),
	mAnimationScale(3.0f)
{
	mOptions.mWindowCaption = "Motion Capture Sample";
	mOptions.mWindowWidth = 1200;
	mOptions.mWindowHeight = 900;
	mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
	mOptions.mFullScreen = false;
}

MotionCaptureSample::~MotionCaptureSample()
{
	ReleaseMacro(mLineVertexBuffer);
	ReleaseMacro(mCheckerboardSRV);
	ReleaseMacro(mSamplerState);
}

void MotionCaptureSample::VInitialize()
{
	mRenderer = mEngine->GetRenderer();
	mRenderer->SetDelegate(this);

	mCamera.SetPosition(vec3f(0.0f, 100.0f, -400.0f));
	
	InitializeBVHResources();
	InitializeGeometry();
	InitializeShaders();
}

void MotionCaptureSample::VUpdate(double milliseconds)
{
	HandleInput(Input::SharedInstance());

	UpdateCamera();

	static uint32_t frame = 0;
	static float animationTime = 0.0f;

	float t = (animationTime / 1000.0f);

	if (t < mAnimationDuration)
	{
		// Find key frame index
		frame = static_cast<int>(floorf((t / mBVHResource.mMotion.FrameTime )* mAnimationScale));

		// Find fractional portion
		float u = (t - frame);
	
		UpdateTransforms(mTransforms, &mBVHResource.mHierarchy.Root, &mBVHResource.mMotion, mTransformCount, frame * mBVHResource.mMotion.ChannelCount, u);
		
		animationTime += static_cast<float>(milliseconds);
	}
	
	UpdateJointWorldMatrices(mJointWorldMatrices, mTransforms, mTransformCount);

	UpdateLineVertices(mLineVertices, &mPairVector, mJointWorldMatrices);

	char str[256];
	sprintf_s(str, "Frame: %u Animation: %f", frame, t);
	mRenderer->SetWindowCaption(str);

	frame++;

	if (frame == mBVHResource.mMotion.FrameCount)
	{
		frame = 0;
		animationTime = 0.0f;
	}
}

void MotionCaptureSample::VRender()
{
	ID3D11DeviceContext* deviceContext = mRenderer->GetDeviceContext();

	float color[4] = { 0.0f, 0.7294117647f, 1.0f, 1.0f };
	deviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
	deviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), color);
	deviceContext->ClearDepthStencilView(mRenderer->GetDepthStencilView(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	deviceContext->RSSetViewports(1, &mRenderer->GetViewport());

	mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);
	mRenderer->VSetInputLayout(mVertexShader);
	mRenderer->VSetVertexShaderInstanceBuffers(mVertexShaderResource);
	mRenderer->VSetVertexShader(mVertexShader);
	mRenderer->VSetPixelShader(mPixelShader);

	mRenderer->VUpdateShaderConstantBuffer(mVertexShaderResource, &mViewProjection, 0);
	mRenderer->VSetVertexShaderConstantBuffers(mVertexShaderResource);
	mRenderer->VBindMesh(mCubeMesh);
	deviceContext->DrawIndexedInstanced(mCubeMesh->GetIndexCount(), mTransformCount, 0, 0, 0);

	mRenderer->VSetInputLayout(mPlaneVertexShader);
	mRenderer->VSetVertexShader(mPlaneVertexShader);
	mRenderer->VSetPixelShader(mPlanePixelShader);

	mRenderer->VUpdateShaderConstantBuffer(mPlaneVertexShaderResource, &mViewProjection, 0);
	mRenderer->VUpdateShaderConstantBuffer(mPlaneVertexShaderResource, &mPlaneWorldMatrix, 1);
	mRenderer->VSetVertexShaderConstantBuffers(mPlaneVertexShaderResource);

	deviceContext->PSSetShaderResources(0, 1, &mCheckerboardSRV);
	deviceContext->PSSetSamplers(0, 1, &mSamplerState);

	mRenderer->VBindMesh(mPlaneMesh);
	mRenderer->VDrawIndexed(0, mPlaneMesh->GetIndexCount());

	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);
	mRenderer->VSetInputLayout(mLineVertexShader);
	mRenderer->VSetVertexShader(mLineVertexShader);
	mRenderer->VSetPixelShader(mLinePixelShader);
	mRenderer->VUpdateBuffer(mLineVertexBuffer, mLineVertices, sizeof(vec3f) * mPairVector.size() * 2);

	UINT stride = sizeof(vec3f);
	UINT offset = 0;

	deviceContext->IASetVertexBuffers(0, 1, &mLineVertexBuffer, &stride, &offset);
	deviceContext->Draw(mPairVector.size() * 2, 0);

	mRenderer->VSwapBuffers();
}

void MotionCaptureSample::VShutdown()
{
	mCubeMesh->~IMesh();
	mPlaneMesh->~IMesh();
	mVertexShader->~IShader();
	mPixelShader->~IShader();
	mLineVertexShader->~IShader();
	mLinePixelShader->~IShader();
	mPlaneVertexShader->~IShader();
	mPlanePixelShader->~IShader();
	mVertexShaderResource->~IShaderResource();
	mPlaneVertexShaderResource->~IShaderResource();
	mAllocator.Free();
}

void MotionCaptureSample::VOnResize()
{
	mViewProjection.Projection = mat4f::normalizedPerspectiveLH(PI * 0.25f, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
}

void MotionCaptureSample::InitializeGeometry()
{
	// Joint Model

	MeshLibrary<LinearAllocator> meshLibrary;
	meshLibrary.SetAllocator(&mAllocator);

	OBJBasicResource<Vertex3> cubeResource("Models\\cube.obj");
	meshLibrary.LoadMesh(&mCubeMesh, mRenderer, cubeResource);
	
	// Plane

	float planeHalfWidth = 400.0f;
	float planeHalfDepth = 400.0f;
	float planeYOffset = 10.0f;

	Vertex2 vertices[4];
	vertices[0].Position = { -planeHalfWidth, -planeYOffset, -planeHalfDepth };
	vertices[1].Position = { -planeHalfWidth, -planeYOffset, +planeHalfDepth };
	vertices[2].Position = { +planeHalfWidth, -planeYOffset, +planeHalfDepth };
	vertices[3].Position = { +planeHalfWidth, -planeYOffset, -planeHalfDepth };

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
	mRenderer->VSetMeshVertexBuffer(mPlaneMesh, vertices, sizeof(Vertex2) * 4, sizeof(Vertex2));
	mRenderer->VSetMeshIndexBuffer(mPlaneMesh, indices, 6);

	// Line buffer 

	mRenderer->VCreateDynamicVertexBuffer(&mLineVertexBuffer, nullptr, sizeof(vec3f) * mPairVector.size() * 2);
}

void MotionCaptureSample::InitializeBVHResources()
{
	// Load BVH file
	mBVHResource.SetFilename(BVH_FILENAME);
	mBVHResource.Load();

	// Set up animation traits
	mAnimationDuration = mBVHResource.mMotion.FrameTime * mBVHResource.mMotion.FrameCount * mAnimationScale;

	mTransformCount = mBVHResource.mHierarchy.JointCount;
	
	// Allocate Transforms
	size_t transformsByteSize = sizeof(Transform) * mTransformCount;
	mTransforms = reinterpret_cast<Transform*>(mAllocator.Allocate(transformsByteSize, alignof(Transform), 0));
	memset(mTransforms, 0, transformsByteSize);

	// Allocate world matrices
	size_t matricesByteSize = sizeof(mat4f) * mTransformCount;
	mJointWorldMatrices = reinterpret_cast<mat4f*>(mAllocator.Allocate(matricesByteSize, alignof(mat4f), 0));
	memset(mJointWorldMatrices, 0, matricesByteSize);

	// Initialize transforms from bvh resource
	BVHJoint* currentJoint = &mBVHResource.mHierarchy.Root;
	InitializeTransforms(mTransforms, &mPairVector, currentJoint);

	size_t lineVertexByteSize = sizeof(vec3f) * mPairVector.size() * 2;
	mLineVertices = reinterpret_cast<vec3f*>(mAllocator.Allocate(lineVertexByteSize, alignof(vec3f), 0));
	memset(mLineVertices, 0, lineVertexByteSize);

	mPlaneWorldMatrix = mat4f::scale(1.0f);
}

void MotionCaptureSample::InitializeTransforms(Transform* transforms, PairVector* pairVector, const BVHJoint* joint)
{	
	static uint32_t index = 0;

	std::pair<uint32_t, uint32_t> jointPair;
	jointPair.first = index;

	Transform* parent = &transforms[index++];
	parent->SetScale(vec3f(1.0f));
	

	for (uint32_t i = 0; i < joint->Children.size(); i++)
	{
		jointPair.second = index;
		pairVector->push_back(jointPair);

		transforms[index].SetParent(parent);
		InitializeTransforms(transforms, pairVector, &joint->Children[i]);
	}
}

void MotionCaptureSample::InitializeShaders()
{
	// ==== Joint Shaders ====

	InputElement inputElements[] =
	{
		{ "POSITION",	0, 0, 0,  0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "NORMAL",		0, 0, 12, 0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 24, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "WORLD",		0, 1, 0, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		1, 1, 16, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		2, 1, 32, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE },
		{ "WORLD",		3, 1, 48, 1, RGBA_FLOAT32, INPUT_CLASS_PER_INSTANCE }
	};

	mRenderer->VCreateShader(&mVertexShader, &mAllocator);
	mRenderer->VLoadVertexShader(mVertexShader, "MCVertexShader.cso", inputElements, 7);

	mRenderer->VCreateShader(&mPixelShader, &mAllocator);
	mRenderer->VLoadPixelShader(mPixelShader, "MCPixelShader.cso");

	mRenderer->VCreateShaderResource(&mVertexShaderResource, &mAllocator);

	void*	constantBufferData[]	= { &mViewProjection };
	size_t	constantBufferSizes[]	= { sizeof(ModelViewProjection) };
	mRenderer->VCreateShaderConstantBuffers(mVertexShaderResource, constantBufferData, constantBufferSizes, 1);

	void*	instanceBufferData[]	= { mJointWorldMatrices };
	size_t	instanceBufferSizes[]	= { sizeof(mat4f) * mTransformCount };
	size_t	instanceBufferStrides[] = { sizeof(mat4f) };
	size_t	instanceBufferOffsets[] = { 0 };
	mRenderer->VCreateDynamicShaderInstanceBuffers(mVertexShaderResource, instanceBufferData, instanceBufferSizes, instanceBufferStrides, instanceBufferOffsets, 1);

	// ==== Line Shaders ====

	InputElement lineInputElement = { "POSITION",	0, 0, 0,  0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX };
	mRenderer->VCreateShader(&mLineVertexShader, &mAllocator);
	mRenderer->VLoadVertexShader(mLineVertexShader, "MCLineVertexShader.cso", &lineInputElement, 1);

	mRenderer->VCreateShader(&mLinePixelShader, &mAllocator);
	mRenderer->VLoadPixelShader(mLinePixelShader, "MCLinePixelShader.cso");


	// ==== Plane Shaders ====

	InputElement planeInputElements[] =
	{
		{ "POSITION",	0, 0, 0,  0, RGB_FLOAT32, INPUT_CLASS_PER_VERTEX },
		{ "TEXCOORD",	0, 0, 12, 0, RG_FLOAT32, INPUT_CLASS_PER_VERTEX }
	};

	mRenderer->VCreateShader(&mPlaneVertexShader, &mAllocator);
	mRenderer->VLoadVertexShader(mPlaneVertexShader, "MCPlaneVertexShader.cso", planeInputElements, 2);

	mRenderer->VCreateShader(&mPlanePixelShader, &mAllocator);
	mRenderer->VLoadPixelShader(mPlanePixelShader, "MCPlanePixelShader.cso");

	mRenderer->VCreateShaderResource(&mPlaneVertexShaderResource, &mAllocator);

	void*	planeConstantBufferData[] = { &mViewProjection, &mPlaneWorldMatrix };
	size_t	planeConstantBufferSizes[] = { sizeof(ModelViewProjection), sizeof(mat4f) };
	mRenderer->VCreateShaderConstantBuffers(mPlaneVertexShaderResource, planeConstantBufferData, planeConstantBufferSizes, 2);

	ID3D11Device* device = mRenderer->GetDevice();
	DirectX::CreateWICTextureFromFile(device, L"Textures\\checkerboard.jpg", nullptr, &mCheckerboardSRV);

	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(D3D11_SAMPLER_DESC));
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	device->CreateSamplerState(&samplerDesc, &mSamplerState);
}

void MotionCaptureSample::UpdateCamera()
{
	mViewProjection.View = mat4f::lookAtLH(mCamera.GetPosition() + mCamera.GetForward(), mCamera.GetPosition(), mCamera.GetUp()).transpose();
	mViewProjection.Projection = mat4f::normalizedPerspectiveLH(PI * 0.25f, mRenderer->GetAspectRatio(), 0.1f, 1000.0f).transpose();
}

void MotionCaptureSample::UpdateTransforms(Transform* transforms, const BVHJoint* joint, const BVHMotion* motion, const uint32_t& transformCount, const uint32_t& frameIndex, const float& u)
{
	static uint32_t index = 0;

	if (index == transformCount)
	{
		index = 0;
	}

#if INTERPOLATION == 1
	uint32_t frameIndices[4];
	frameIndices[0] = (frameIndex == 0) ? frameIndex : frameIndex - 1;
	frameIndices[1] = frameIndex;
	frameIndices[2] = frameIndex + 1;
	frameIndices[3] = (frameIndex == motion->FrameCount - 2) ? frameIndex + 1 : frameIndex + 2;

	vec3f jointPositions[4];
	vec3f jointRotations[4];

	for (int f = 0; f < 4; f ++)
	{
		uint32_t channelOffset = frameIndices[f] + joint->ChannelOffset;
		jointPositions[f] = { joint->Offset[0], joint->Offset[1], joint->Offset[2] };
		jointRotations[f] = { 0.0f, 0.0f, 0.0f };

		for (uint32_t i = 0; i < joint->ChannelCount; i++)
		{
			uint16_t channel = joint->ChannelOrder[i];
			float value = motion->Data[channelOffset + i];

			if (channel & DOF_POSITION_X)
			{
				jointPositions[f].x += value;
			}
			else if (channel & DOF_POSITION_Y)
			{
				jointPositions[f].y += value;
			}
			else if (channel & DOF_POSITION_Z)
			{
				jointPositions[f].z += value;
			}
			else if (channel & DOF_ROTATION_X)
			{
				jointRotations[f].x += value * RADIAN;
			}
			else if (channel & DOF_ROTATION_Y)
			{
				jointRotations[f].y += value * RADIAN;
			}
			else if (channel & DOF_ROTATION_Z)
			{
				jointRotations[f].z += value * RADIAN;
			}
		}
	}

	mat4f CR = 0.5f * mat4f(
		0.0f, 2.0f, 0.0f, 0.0f,
		-1.0f, 0.0f, 1.0f, 0.0f,
		2.0f, -5.0f, 4.0f, -1.0f,
		-1.0f, 3.0f, -3.0f, 1.0f);

	mat4f P = { jointPositions[0], jointPositions[1], jointPositions[2], jointPositions[3] };
	vec4f T = { 1, u, u * u, u * u * u };

	vec3f position = T * CR * P;
	
	quatf q0 = quatf::rollPitchYaw(jointRotations[1].z, jointRotations[1].x, jointRotations[1].y);
	quatf q1 = quatf::rollPitchYaw(jointRotations[2].z, jointRotations[2].x, jointRotations[2].y);
	quatf rotation = cliqCity::graphicsMath::slerp(q0, q1, u);
#else

	uint32_t channelOffset = frameIndex + joint->ChannelOffset;
	vec3f position = { joint->Offset[0], joint->Offset[1], joint->Offset[2] };
	vec3f rotation = { 0.0f, 0.0f, 0.0f };

	for (uint32_t i = 0; i < joint->ChannelCount; i++)
	{
		uint16_t channel = joint->ChannelOrder[i];
		float value = motion->Data[channelOffset + i];

		if (channel & DOF_POSITION_X)
		{
			position.x += value;
		}
		else if (channel & DOF_POSITION_Y)
		{
			position.y += value;
		}
		else if (channel & DOF_POSITION_Z)
		{
			position.z += value;
		}
		else if (channel & DOF_ROTATION_X)
		{
			rotation.x += value * RADIAN;
		}
		else if (channel & DOF_ROTATION_Y)
		{
			rotation.y += value * RADIAN;
		}
		else if (channel & DOF_ROTATION_Z)
		{
			rotation.z += value * RADIAN;
		}
	}
#endif

	Transform* transform = &transforms[index++];
	transform->SetPosition(position.x, position.y, position.z);
	transform->SetRotation(rotation.x, rotation.y, -rotation.z);
	
	for (uint32_t i = 0; i < joint->Children.size(); i++)
	{
		UpdateTransforms(transforms, &joint->Children[i], motion, transformCount, frameIndex, u);
	}
}

void MotionCaptureSample::UpdateJointWorldMatrices(mat4f* jointWorldMatrices, Transform* transforms, const uint32_t& count)
{
	for (uint32_t i = 0; i < count; i++)
	{
		mJointWorldMatrices[i] = transforms[i].GetWorldMatrix().transpose();
	}

	mRenderer->VUpdateShaderInstanceBuffer(mVertexShaderResource, mJointWorldMatrices, sizeof(mat4f) * count, 0);
}

void MotionCaptureSample::UpdateLineVertices(vec3f* lineVertices, PairVector* pairVector, mat4f* jointWorldMatrices)
{
	for (uint32_t i = 0, j = 0; i < pairVector->size(); i++, j += 2)
	{
		lineVertices[j] = jointWorldMatrices[pairVector->at(i).first].transpose().t;
		lineVertices[j + 1] = jointWorldMatrices[pairVector->at(i).second].transpose().t;
	}
}

void MotionCaptureSample::HandleInput(Input& input)
{
	ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
	if (input.GetMouseButton(MOUSEBUTTON_LEFT)) {
		mCamera.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
		mCamera.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
	}

	mMouseX = mousePosition.x;
	mMouseY = mousePosition.y;

	vec3f position = mCamera.GetPosition();
	if (input.GetKey(KEYCODE_W)) {
		position += mCamera.GetForward() * CAMERA_SPEED;
		mCamera.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_A)) {
		position += mCamera.GetRight() * -CAMERA_SPEED;
		mCamera.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_D)) {
		position += mCamera.GetRight() * CAMERA_SPEED;
		mCamera.SetPosition(position);
	}

	if (input.GetKey(KEYCODE_S)) {
		position += mCamera.GetForward() * -CAMERA_SPEED;
		mCamera.SetPosition(position);
	}
}

DECLARE_MAIN(MotionCaptureSample);