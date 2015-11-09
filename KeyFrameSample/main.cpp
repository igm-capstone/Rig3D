#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include "Rig3D\Graphics\MeshLibrary.h"
#include <d3d11.h>
#include <fstream>
#include "Rig3D\Graphics\Interface\IShader.h"

#define PI 3.1415926535f

using namespace Rig3D;

static const int VERTEX_COUNT = 8;
static const int INDEX_COUNT = 36;
static const float ANIMATION_DURATION = 20000.0f; // 20 Seconds
static const int KEY_FRAME_COUNT = 10;

class Rig3DSampleScene : public IScene, public virtual IRendererDelegate
{
public:

	typedef cliqCity::graphicsMath::Vector2 vec2f;

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
		quatf mRotation;
		vec3f mPosition;
		float mTime;
	};

	struct TCBProperties
	{
		float t, c, b;
	};

	SampleMatrixBuffer		mMatrixBuffer;
	IMesh*					mCubeMesh;
	LinearAllocator			mAllocator;
	KeyFrame				mKeyFrames[KEY_FRAME_COUNT];

	IShader*				mVertexShader;
	IShader*				mPixelShader;

	DX3D11Renderer*			mRenderer;
	ID3D11Device*			mDevice;
	ID3D11DeviceContext*	mDeviceContext;
	
	InterpolationMode		mInterpolationMode;
	TCBProperties			mTCBProperties;
	float					mAnimationTime;
	bool					mIsPlaying;

	MeshLibrary<LinearAllocator> mMeshLibrary;

	Rig3DSampleScene() : mAllocator(1024)
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

	}

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);

		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

		VOnResize();

		InitializeAnimation();
		InitializeGeometry();
		InitializeShaders();
		InitializeCamera();
	}

	void InitializeAnimation()
	{
		std::ifstream file("Animation\\keyframe-input.txt");

		if (!file.is_open()) {
			printf("ERROR OPENING FILE");
			return;
		}

		char line[100];
		int i = 0;
		float radians = (PI / 180.f);

		while (file.good()) {
			file.getline(line, 100);
			if (line[0] == '\0') {
				continue;
			}

			float time, angle;
			vec3f position, axis;
			sscanf_s(line, "%f %f %f %f %f %f %f %f\n", &time, &position.x, &position.y, &position.z, &axis.x, &axis.y, &axis.z, &angle);
			mKeyFrames[i].mTime = time;
			mKeyFrames[i].mPosition = position;
			mKeyFrames[i].mRotation = cliqCity::graphicsMath::normalize(quatf::angleAxis(angle * radians, axis));
			i++;
		}

		file.close();

		mMatrixBuffer.mWorld = mat4f::translate(mKeyFrames[0].mPosition).transpose();
		mAnimationTime = 0.0f;
		mIsPlaying = false;
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
		indices[1] = 1;
		indices[2] = 2;

		indices[3] = 2;
		indices[4] = 3;
		indices[5] = 0;

		// Right Face
		indices[6] = 1;
		indices[7] = 5;
		indices[8] = 6;

		indices[9] = 6;
		indices[10] = 2;
		indices[11] = 1;

		// Back Face
		indices[12] = 5;
		indices[13] = 4;
		indices[14] = 7;

		indices[15] = 7;
		indices[16] = 6;
		indices[17] = 5;

		// Left Face
		indices[18] = 4;
		indices[19] = 0;
		indices[20] = 3;

		indices[21] = 3;
		indices[22] = 7;
		indices[23] = 4;

		// Top Face
		indices[24] = 4;
		indices[25] = 5;
		indices[26] = 1;

		indices[27] = 1;
		indices[28] = 0;
		indices[29] = 4;

		// Bottom Face
		indices[30] = 3;
		indices[31] = 2;
		indices[32] = 6;

		indices[33] = 6;
		indices[34] = 7;
		indices[35] = 3;

		mMeshLibrary.NewMesh(&mCubeMesh, mRenderer);
		mRenderer->VSetMeshVertexBuffer(mCubeMesh, vertices, sizeof(SampleVertex) * VERTEX_COUNT, sizeof(SampleVertex));
		mRenderer->VSetMeshIndexBuffer(mCubeMesh, indices, INDEX_COUNT);
	}

	void InitializeShaders()
	{
		InputElement inputElements[] = 
		{
			{ "POSITION", 0, 0, 0, 0, FLOAT3, INPUT_CLASS_PER_VERTEX },
			{ "COLOR", 0, 0, 12, 0, FLOAT3, INPUT_CLASS_PER_VERTEX }
		};

		// Load Vertex Shader --------------------------------------
		mRenderer->VCreateShader(&mVertexShader, &mAllocator);
		mRenderer->VLoadVertexShader(mVertexShader, "SampleVertexShader.cso", inputElements, 2);

		// Load Pixel Shader ---------------------------------------

		mRenderer->VCreateShader(&mPixelShader, &mAllocator);
		mRenderer->VLoadPixelShader(mPixelShader, "SamplePixelShader.cso");

		// Constant buffers ----------------------------------------

		void* data[] = { &mMatrixBuffer };
		size_t sizes[] = { sizeof(SampleMatrixBuffer) };
		mRenderer->VCreateShaderConstantBuffers(mVertexShader, data, sizes, 1);
	}

	void InitializeCamera()
	{
		mMatrixBuffer.mProjection = mat4f::normalizedPerspectiveLH(0.25f * 3.1415926535f, mRenderer->GetAspectRatio(), 0.1f, 100.0f).transpose();
		mMatrixBuffer.mView = mat4f::lookAtLH(vec3f(0.0, 0.0, 0.0), vec3f(0.0, 0.0, -38.0), vec3f(0.0, 1.0, 0.0)).transpose();
	}

	void VUpdate(double milliseconds) override
	{
		if (!mIsPlaying) {
			mIsPlaying = Input::SharedInstance().GetKey(KEYCODE_RIGHT);
		}
		else {
			float t = mAnimationTime / 1000.0f;
			if (t < 9.0f) {

				// Find key frame index
				int i = (int)floorf(t);

				// Find fractional portion
				float u = (t - i);

				quatf rotation;
				vec3f position;

				switch (mInterpolationMode)
				{
					case INTERPOLATION_MODE_LINEAR:
						LinearInterpolation(&position, &rotation, i, u);
						break;
					case INTERPOLATION_MODE_CATMULL_ROM:
						CatmullRomInterpolation(&position, &rotation, i, u);
						break;
					case INTERPOLATION_MODE_TCB:
						TCBInterpolation(mTCBProperties, &position, &rotation, i, u);
						break;
					default:
						break;
				}

				mMatrixBuffer.mWorld = (cliqCity::graphicsMath::normalize(rotation)
					.toMatrix4() * mat4f::translate(position)).transpose();

				char str[256];
				sprintf_s(str, "Milliseconds %f", mAnimationTime);
				mRenderer->SetWindowCaption(str);
			}

			mAnimationTime += (float)milliseconds;	
		}

		if (Input::SharedInstance().GetKeyDown(KEYCODE_LEFT)) {
			InitializeAnimation();
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
	}

	void LinearInterpolation(vec3f* position, quatf* rotation, int i, float u)
	{
		KeyFrame& current	= mKeyFrames[i];
		KeyFrame& after		= mKeyFrames[i + 1];
		*position			= (1 - u) * current.mPosition + after.mPosition * u;
		Slerp(rotation, current.mRotation, after.mRotation, u);
	}

	void CatmullRomInterpolation(vec3f* position, quatf* rotation, int i, float u)
	{
		KeyFrame& before	= (i == 0) ? mKeyFrames[i] : mKeyFrames[i - 1];
		KeyFrame& current	= mKeyFrames[i];
		KeyFrame& after		= mKeyFrames[i + 1];
		KeyFrame& after2	= (i == KEY_FRAME_COUNT - 2) ? mKeyFrames[i + 1] : mKeyFrames[i + 2];

		mat4f CR = 0.5f * mat4f(
			0.0f, 2.0f, 0.0f, 0.0f,
			-1.0f, 0.0f, 1.0f, 0.0f,
			2.0f, -5.0f, 4.0f, -1.0f,
			-1.0f, 3.0f, -3.0f, 1.0f);

		mat4f P = { before.mPosition, current.mPosition, after.mPosition, after2.mPosition };
		vec4f T = { 1, u, u * u, u * u * u };

		*position = T * CR * P;
		Slerp(rotation, current.mRotation, after.mRotation, u);
	}

	void TCBInterpolation(TCBProperties& tcb, vec3f* position, quatf* rotation, int i, float u)
	{
		KeyFrame& before	= (i == 0) ? mKeyFrames[i] : mKeyFrames[i - 1];
		KeyFrame& current	= mKeyFrames[i];
		KeyFrame& after		= mKeyFrames[i + 1];

		tcb.t = -1.0f;
		tcb.c = -1.0f;
		tcb.b = -1.0f;

		vec3f vIn = ((1.0f - tcb.t) * (1.0f + tcb.b) * (1.0f - tcb.c) * 0.5f) * (current.mPosition - before.mPosition) +
			((1.0f - tcb.t) * (1.0f - tcb.b) * (1.0f + tcb.c) * 0.5f) * (after.mPosition - current.mPosition);
		vec3f vOut = ((1.0f - tcb.t) * (1.0f + tcb.b) * (1.0f + tcb.c) * 0.5f) * (current.mPosition - before.mPosition) +
			((1.0f - tcb.t) * (1.0f - tcb.b) * (1.0f - tcb.c) * 0.5f) * (after.mPosition - current.mPosition);

		mat4f H = {
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
		   -3.0f, -2.0f, -1.0f, 3.0f,
			2.0f, 1.0f, 1.0f, -2.0f};

		mat4f P = { current.mPosition, vOut, vIn, after.mPosition };
		vec4f T = { 1, u, u * u, u * u * u };

		*position = T * H * P;
		Slerp(rotation, current.mRotation, after.mRotation, u);
	}

	void Slerp(quatf* out, quatf q0, quatf q1, const float u)
	{
		float cosAngle = cliqCity::graphicsMath::dot(q0, q1);
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
		mRenderer->VSetInputLayout(mVertexShader);
		mRenderer->VSetPrimitiveType(GPU_PRIMITIVE_TYPE_TRIANGLE);

		mDeviceContext->RSSetViewports(1, &mRenderer->GetViewport());
		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), mRenderer->GetDepthStencilView());
		mDeviceContext->ClearRenderTargetView(*mRenderer->GetRenderTargetView(), color);
		mDeviceContext->ClearDepthStencilView(
			mRenderer->GetDepthStencilView(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			1.0f,
			0);

		mRenderer->VSetVertexShader(mVertexShader);
		mRenderer->VSetPixelShader(mPixelShader);

		mRenderer->VUpdateShaderConstantBuffer(mVertexShader, &mMatrixBuffer, 0);
		mRenderer->VSetVertexShaderResources(mVertexShader);

		mRenderer->VBindMesh(mCubeMesh);

		mRenderer->VDrawIndexed(0, mCubeMesh->GetIndexCount());
		mRenderer->VSwapBuffers();
	}

	void VOnResize() override
	{
		InitializeCamera();
	}

	void VShutdown() override
	{
		mVertexShader->~IShader();
		mPixelShader->~IShader();
		mCubeMesh->~IMesh();
		mAllocator.Free();
	}
};

DECLARE_MAIN(Rig3DSampleScene);