#include <Windows.h>
#include <Rig3D\Engine.h>
#include <Rig3D\Graphics\Interface\IScene.h>
#include <Rig3D\Graphics\DirectX11\DX3D11Renderer.h>
#include <Rig3D\Graphics\Interface\IMesh.h>
#include <Rig3D\Common\Transform.h>
#include <Memory\Memory\Memory.h>
#include <Rig3D\SceneGraph.h>
#include "Rig3D\Graphics\MeshLibrary.h"
#include <Rig3D/TaskDispatch/TaskDispatcher.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <fstream>

#define PI 3.1415926535f

using namespace Rig3D;
using namespace cliqCity::multicore;

static const int VERTEX_COUNT = 8;
static const int INDEX_COUNT = 36;
static const float ANIMATION_DURATION = 20000.0f; // 20 Seconds
static const int KEY_FRAME_COUNT = 25;
static const int TRANSFORM_COUNT = 5;
static const int SCENE_GRAPH_BUFFER_SIZE = TRANSFORM_COUNT * sizeof(SceneGraphNode);
static const int THREAD_COUNT = 4;
static const int TASK_COUNT = 10;
static const int TASK_BUFFER_SIZE = TASK_COUNT * sizeof(TaskData);

static char gTaskBuffer[TASK_BUFFER_SIZE];
static char gSceneGraphBuffer[SCENE_GRAPH_BUFFER_SIZE];

class Rig3DSampleScene : public IScene, public virtual IRendererDelegate
{
public:

	typedef cliqCity::graphicsMath::Vector2			vec2f;

	enum InterpolationMode
	{
		INTERPOLATION_MODE_LINEAR,
		INTERPOLATION_MODE_CATMULL_ROM,
		INTERPOLATION_MODE_TCB
	};

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

	struct KeyFrame
	{
		vec3f mPositions[TRANSFORM_COUNT];
		quatf mRotations[TRANSFORM_COUNT];
		float mTime;
	};

	struct TCBProperties
	{
		float t, c, b;
	};

	SampleMatrixBuffer		mMatrixBuffer;
	KeyFrame				mKeyFrames[KEY_FRAME_COUNT];

	IMesh*					mCubeMesh;
	DX3D11Renderer*			mRenderer;
	ID3D11Device*			mDevice;
	ID3D11DeviceContext*	mDeviceContext;
	ID3D11Buffer*			mConstantBuffer;
	ID3D11InputLayout*		mInputLayout;
	ID3D11VertexShader*		mVertexShader;
	ID3D11PixelShader*		mPixelShader;

	Thread					mThreads[THREAD_COUNT];
	TaskID					mTaskIds[TASK_COUNT];
	TaskData				mTasksData[TASK_COUNT];
	TaskDispatcher			mTaskDispatcher;

	InterpolationMode		mInterpolationMode;
	TCBProperties			mTCBProperties;
	float					mAnimationTime;
	bool					mIsPlaying;

	Transform mTransforms[TRANSFORM_COUNT];

	SceneGraph				mSceneGraph;
	LinearAllocator			mAllocator;
	MeshLibrary<LinearAllocator> mMeshLibrary;

	Rig3DSampleScene() :
		mCubeMesh(nullptr),
		mRenderer(nullptr),
		mDevice(nullptr),
		mDeviceContext(nullptr),
		mConstantBuffer(nullptr),
		mInputLayout(nullptr),
		mVertexShader(nullptr),
		mPixelShader(nullptr),
		mSceneGraph(gSceneGraphBuffer, SCENE_GRAPH_BUFFER_SIZE),
		mTaskDispatcher(mThreads, THREAD_COUNT, gTaskBuffer, TASK_BUFFER_SIZE),
		mAllocator(1024)
	{
		mOptions.mWindowCaption = "Key Frame Sample";
		mOptions.mWindowWidth = 800;
		mOptions.mWindowHeight = 600;
		mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
		mOptions.mFullScreen = false;
		mAnimationTime = 0.0f;
		mIsPlaying = false;
		mMeshLibrary.SetAllocator(&mAllocator);
		mInterpolationMode = INTERPOLATION_MODE_LINEAR;
	}

	~Rig3DSampleScene()
	{
		ReleaseMacro(mVertexShader);
		ReleaseMacro(mPixelShader);
		ReleaseMacro(mConstantBuffer);
		ReleaseMacro(mInputLayout);
	}

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);

		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

		mSceneGraph.Add("Body",  &mTransforms[0]);
		mSceneGraph.Add("Chest", &mTransforms[1], &mTransforms[0]);
		mSceneGraph.Add("L_Arm",  &mTransforms[2],  &mTransforms[1]);
		mSceneGraph.Add("L_Hand", &mTransforms[3], &mTransforms[2]);
		mSceneGraph.Add("L_Hand", &mTransforms[4], &mTransforms[2]);

		VOnResize();

		InitializeDispatcher();
		InitializeAnimation();
		InitializeGeometry();
		InitializeShaders();
		InitializeCamera();
	}

	void InitializeDispatcher()
	{
		mTaskDispatcher.Start();
	}

	void InitializeAnimation()
	{
		std::ifstream file("Animation\\keyframe-input2.txt");

		if (!file.is_open()) {
			printf("ERROR OPENING FILE");
			return;
		}

		char line[100];
		int i = -1, j = 0;
		float radians = (PI / 180.f);

		while (file.good()) {
			file.getline(line, 100);
			if (line[0] == '\0') {
				continue;
			}

			if (line[0] == 'k')
			{
				i++;
				j = 0;

				if (i >= KEY_FRAME_COUNT)
				{
					break;
				}

				float time;
				sscanf_s(line, "k\t%f", &time);
				
				mKeyFrames[i].mTime = time;
				continue;
			}

			float angle, cangle;
			vec3f position, cposition, axis, caxis;
			sscanf_s(line, "%f\t%f\t%f\t%f\t%f\t%f\t%f",
				&position.x, &position.y, &position.z,
				&axis.x, &axis.y, &axis.z, &angle);
			mKeyFrames[i].mPositions[j] = position;
			mKeyFrames[i].mRotations[j] = normalize(quatf::angleAxis(angle * radians, axis));
			j++;
		}

		file.close();

		for (i = 0; i < TRANSFORM_COUNT; i++)
		{
			mTransforms[i].SetPosition(mKeyFrames[0].mPositions[i]);
			mTransforms[i].SetRotation(normalize(mKeyFrames[0].mRotations[i]));
		}

		mIsPlaying = true;

		mAnimInfo.mTCBProperties = &mTCBProperties;
		mAnimInfo.mKeyFrames = mKeyFrames;
	}

	void InitializeGeometry()
	{
		SampleVertex vertices[VERTEX_COUNT];
		vertices[0].mPosition = { -0.5f, +0.5f, +0.5f };	// Front Top Left
		vertices[0].mColor = { +1.0f, +1.0f, +0.0f };

		vertices[1].mPosition = { +0.5f, +0.5f, +0.5f };  // Front Top Right
		vertices[1].mColor = { +1.0f, +1.0f, +1.0f };

		vertices[2].mPosition = { +0.5f, -0.5f, +0.5f };  // Front Bottom Right
		vertices[2].mColor = { +1.0f, +0.0f, +1.0f };

		vertices[3].mPosition = { -0.5f, -0.5f, +0.5f };   // Front Bottom Left
		vertices[3].mColor = { +1.0f, +0.0f, +0.0f };

		vertices[4].mPosition = { -0.5f, +0.5f, -0.5f };;  // Back Top Left
		vertices[4].mColor = { +0.0f, +1.0f, +0.0f };

		vertices[5].mPosition = { +0.5f, +0.5f, -0.5f };  // Back Top Right
		vertices[5].mColor = { +0.0f, +1.0f, +1.0f };

		vertices[6].mPosition = { +0.5f, -0.5f, -0.5f };  // Back Bottom Right
		vertices[6].mColor = { +1.0f, +0.0f, +1.0f };

		vertices[7].mPosition = { -0.5f, -0.5f, -0.5f };  // Back Bottom Left
		vertices[7].mColor = { +0.0f, +0.0f, +0.0f };

		uint16_t indices[INDEX_COUNT];
		// Front Face
		indices[0] = 0;
		indices[1] = 2;
		indices[2] = 1;

		indices[3] = 2;
		indices[4] = 0;
		indices[5] = 3;

		// Right Face
		indices[6] = 1;
		indices[7] = 6;
		indices[8] = 5;

		indices[9] = 6;
		indices[10] = 1;
		indices[11] = 2;

		// Back Face
		indices[12] = 5;
		indices[13] = 7;
		indices[14] = 4;

		indices[15] = 7;
		indices[16] = 5;
		indices[17] = 6;

		// Left Face
		indices[18] = 4;
		indices[19] = 3;
		indices[20] = 0;

		indices[21] = 3;
		indices[22] = 4;
		indices[23] = 7;

		// Top Face
		indices[24] = 4;
		indices[25] = 1;
		indices[26] = 5;

		indices[27] = 1;
		indices[28] = 4;
		indices[29] = 0;

		// Bottom Face
		indices[30] = 3;
		indices[31] = 6;
		indices[32] = 2;

		indices[33] = 6;
		indices[34] = 3;
		indices[35] = 7;

		mMeshLibrary.NewMesh(&mCubeMesh, mRenderer);
		mRenderer->VSetStaticMeshVertexBuffer(mCubeMesh, vertices, sizeof(SampleVertex) * VERTEX_COUNT, sizeof(SampleVertex));
		mRenderer->VSetStaticMeshIndexBuffer(mCubeMesh, indices, INDEX_COUNT);
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
			if (mInputLayout != NULL) ReleaseMacro(mInputLayout);
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
		mMatrixBuffer.mProjection = mat4f::normalizedPerspectiveLH(0.25f * 3.1415926535f, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
		mMatrixBuffer.mView = mat4f::lookAtLH(vec3f(5.0, 0.0, 0.0), vec3f(5.0, 0.0, -35.0), vec3f(0.0, 1.0, 0.0)).transpose();
	}

	int zeroToFour[5] = { 0, 1, 2, 3, 4 };

	struct AnimInfo
	{
		int mFrameIndex;
		float mFrameFraction;
		InterpolationMode mInterpolationMode;
		TCBProperties* mTCBProperties;
		KeyFrame* mKeyFrames;
	}mAnimInfo;

	void VUpdate(double milliseconds) override
	{
		if (!mIsPlaying) {
			mIsPlaying = Input::SharedInstance().GetKey(KEYCODE_RIGHT);
		}
		else {
			float time = mAnimationTime / 1000.0f;
			if (time >= 6.0f) {
				mAnimationTime = time = 0;
			}

			// Find key frame index
			mAnimInfo.mFrameIndex = (int)floorf(time);

			// Find fractional portion
			mAnimInfo.mFrameFraction = (time - mAnimInfo.mFrameIndex);
			mAnimInfo.mInterpolationMode = mInterpolationMode;

			quatf rotation;
			vec3f position;

			for (int i = 0; i < TRANSFORM_COUNT; i++)
			{
				mTasksData[i].mStream.in[0] = &zeroToFour[i];
				mTasksData[i].mStream.in[1] = &mAnimInfo;
				mTasksData[i].mStream.out[0] = mTransforms;

				mTaskIds[i] = mTaskDispatcher.AddTask(mTasksData[i], InterpolateTransform);

				//switch (mInterpolationMode)
				//{
				//	case INTERPOLATION_MODE_LINEAR:
				//			LinearInterpolation(mKeyFrames, &position, &rotation, i, k, u);
				//		break;
				//	case INTERPOLATION_MODE_CATMULL_ROM:
				//			CatmullRomInterpolation(mKeyFrames, &position, &rotation, i, k, u);
				//		break;
				//	case INTERPOLATION_MODE_TCB:
				//			TCBInterpolation(mKeyFrames, mTCBProperties, &position, &rotation, i, k, u);
				//		break;
				//	default:
				//		break;
				//}

				//mTransforms[i].SetPosition(position);
				//mTransforms[i].SetRotation(normalize(rotation));
			}

			//mTaskDispatcher.Synchronize();

			char str[256];
			char animType = mInterpolationMode == INTERPOLATION_MODE_LINEAR ? 'L' : mInterpolationMode == INTERPOLATION_MODE_CATMULL_ROM ? 'C' : 'T';
			sprintf_s(str, "Milliseconds %c %f", animType, mAnimationTime);
			mRenderer->SetWindowCaption(str);
			

			mAnimationTime += (float)milliseconds;	
		}

		if (Input::SharedInstance().GetKeyDown(KEYCODE_LEFT)) {
			mAnimationTime = 0.0f;
			mIsPlaying = false;
		}

		if (Input::SharedInstance().GetKeyDown(KEYCODE_L)) {
			mInterpolationMode = INTERPOLATION_MODE_LINEAR;
		}
		else if (Input::SharedInstance().GetKeyDown(KEYCODE_C)) {
			mInterpolationMode = INTERPOLATION_MODE_CATMULL_ROM;
		}
		else if (Input::SharedInstance().GetKeyDown(KEYCODE_T)) {
			mInterpolationMode = INTERPOLATION_MODE_TCB;
		}

		if (Input::SharedInstance().GetGamepadButtonDown(GAMEPADBUTTON_A)) {
			mInterpolationMode = INTERPOLATION_MODE_LINEAR;
		}
		else if (Input::SharedInstance().GetGamepadButtonDown(GAMEPADBUTTON_B)) {
			mInterpolationMode = INTERPOLATION_MODE_CATMULL_ROM;
		}
		else if (Input::SharedInstance().GetGamepadButtonDown(GAMEPADBUTTON_X)) {
			mInterpolationMode = INTERPOLATION_MODE_TCB;
		}
	}

	static void InterpolateTransform(const TaskData& data)
	{
		auto i = *reinterpret_cast<int*>(data.mStream.in[0]);
		auto animInfo = *reinterpret_cast<AnimInfo*>(data.mStream.in[1]);
		auto transforms = reinterpret_cast<Transform*>(data.mStream.out[0]);

		quatf rotation;
		vec3f position;

		switch (animInfo.mInterpolationMode)
		{
		case INTERPOLATION_MODE_LINEAR:
			LinearInterpolation(animInfo.mKeyFrames, &position, &rotation, i, animInfo.mFrameIndex, animInfo.mFrameFraction);
			break;
		case INTERPOLATION_MODE_CATMULL_ROM:
			CatmullRomInterpolation(animInfo.mKeyFrames, &position, &rotation, i, animInfo.mFrameIndex, animInfo.mFrameFraction);
			break;
		case INTERPOLATION_MODE_TCB:
			TCBInterpolation(animInfo.mKeyFrames, *animInfo.mTCBProperties, &position, &rotation, i, animInfo.mFrameIndex, animInfo.mFrameFraction);
			break;
		default:
			break;
		}

		transforms[i].SetPosition(position);
		transforms[i].SetRotation(normalize(rotation));
	}

	static void LinearInterpolation(KeyFrame* keyFrames, vec3f* position, quatf* rotation, int i, int k, float u)
	{
		KeyFrame& current	= keyFrames[k];
		KeyFrame& after		= keyFrames[k + 1];
		
		*position			= (1 - u) * current.mPositions[i] + after.mPositions[i] * u;

		Slerp(rotation, current.mRotations[i], after.mRotations[i], u);
	}

	static void CatmullRomInterpolation(KeyFrame* keyFrames, vec3f* position, quatf* rotation, int i, int k, float u)
	{
		KeyFrame& before	= (k == 0) ? keyFrames[k] : keyFrames[k - 1];
		KeyFrame& current	= keyFrames[k];
		KeyFrame& after		= keyFrames[k + 1];
		KeyFrame& after2	= (k == KEY_FRAME_COUNT - 2) ? keyFrames[k + 1] : keyFrames[k + 2];

		mat4f CR = 0.5f * mat4f(
			0.0f, 2.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 1.0f, 0.0f,
			2.0f, -5.0f, 4.0f, -1.0f,
			-1.0f, 3.0f, -3.0f, 1.0f);

		mat4f P = { before.mPositions[i], current.mPositions[i], after.mPositions[i], after2.mPositions[i] };
		vec4f T = { 1, u, u * u, u * u * u };

		*position = T * CR * P;

		Slerp(rotation, current.mRotations[i], after.mRotations[i], u);
	}

	static void TCBInterpolation(KeyFrame* keyFrames, TCBProperties& tcb, vec3f* position, quatf* rotation, int i, int k, float u)
	{
		KeyFrame& before	= (k == 0) ? keyFrames[k] : keyFrames[k - 1];
		KeyFrame& current	= keyFrames[k];
		KeyFrame& after		= keyFrames[k + 1];

		tcb.t = -1.0f;
		tcb.c = -1.0f;
		tcb.b = -1.0f;

		mat4f H = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			-3.0f, -2.0f, -1.0f, 3.0f,
			2.0f, 1.0f, 1.0f, -2.0f };

		vec4f T = { 1, u, u * u, u * u * u };

		vec3f vIn = ((1.0f - tcb.t) * (1.0f + tcb.b) * (1.0f - tcb.c) * 0.5f) * (current.mPositions[i] - before.mPositions[i]) +
			((1.0f - tcb.t) * (1.0f - tcb.b) * (1.0f + tcb.c) * 0.5f) * (after.mPositions[i] - current.mPositions[i]);
		vec3f vOut = ((1.0f - tcb.t) * (1.0f + tcb.b) * (1.0f + tcb.c) * 0.5f) * (current.mPositions[i] - before.mPositions[i]) +
			((1.0f - tcb.t) * (1.0f - tcb.b) * (1.0f - tcb.c) * 0.5f) * (after.mPositions[i] - current.mPositions[i]);

		mat4f P = { current.mPositions[i], vOut, vIn, after.mPositions[i] };

		*position = T * H * P;

		Slerp(rotation, current.mRotations[i], after.mRotations[i], u);
	}

	static void Slerp(quatf* out, quatf q0, quatf q1, const float u)
	{
		float cosAngle = dot(q0, q1);
		if (cosAngle < 0.0f) {
			q1 = -q1;
			cosAngle = -cosAngle;
		}

		float k0, k1;				// Check for divide by zero
		if (cosAngle > 0.9999f) {
			k0 = 1.0f - u;
			k1 = u;
		}
		else {
			float angle = acosf(cosAngle);
			float oneOverSinAngle = 1.0f / sinf(angle);

			k0 = ((sinf(1.0f - u) * angle) * oneOverSinAngle);
			k1 = (sinf(u * angle) * oneOverSinAngle);
		}

		q0 = q0 * k0;
		q1 = q1 * k1;

		out->w = q0.w + q1.w;
		out->v.x = q0.v.x + q1.v.x;
		out->v.y = q0.v.y + q1.v.y;
		out->v.z = q0.v.z + q1.v.z;
	}

	void VRender() override
	{
		float color[4] = { 0.5f, 1.0f, 1.0f, 1.0f };

		// Set up the input assembler
		mDeviceContext->IASetInputLayout(mInputLayout);
		mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);

		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), color);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		mDeviceContext->PSSetShader(mPixelShader, NULL, 0);

		mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);

		mRenderer->VBindMesh(mCubeMesh);

		for (int i = 0; i < TRANSFORM_COUNT; i++)
		{
			auto pos = mTransforms[i].GetWorldMatrix().transpose();
			mMatrixBuffer.mWorld = pos;
			mDeviceContext->UpdateSubresource(mConstantBuffer, 0, nullptr, &mMatrixBuffer, 0, 0);

			mRenderer->VDrawIndexed(0, mCubeMesh->GetIndexCount());
		}

		mRenderer->VSwapBuffers();
	}

	void VOnResize() override
	{
		InitializeCamera();
	}

	void VShutdown() override
	{
		mCubeMesh->~IMesh();
		mAllocator.Free();
	}
};

DECLARE_MAIN(Rig3DSampleScene);