//Based on: http://www.rastertek.com/dx11tut39.html

#include <Windows.h>
#include "Rig3D\Engine.h"
#include "Rig3D\Graphics\Interface\IScene.h"
#include "Rig3D\Graphics\DirectX11\DX3D11Renderer.h"
#include "Rig3D\Graphics\Interface\IMesh.h"
#include "Rig3D\Common\Transform.h"
#include "Memory\Memory\Memory.h"
#include <Rig3D\SceneGraph.h>
#include "Rig3D\Graphics\MeshLibrary.h"
#include "Rig3D\Graphics\Camera.h"
#include "Rig3D\Graphics\DirectX11\DX11IMGUI.h"
#include <d3d11.h>
#include <d3dcompiler.h>
#include "Rig3D\Graphics\DirectX11\DirectXTK\Inc\DDSTextureLoader.h"
#include <fstream>
#include <Rig3D\Graphics\DirectX11\imgui\imgui.h>

#define PI								3.1415926535f
#define RADIAN							3.1415926535f / 180.0f
#define CAMERA_SPEED					0.01f
#define CAMERA_ROTATION_SPEED			0.05f

using namespace Rig3D;

//#define MULTICORE

#ifdef MULTICORE
#include "Rig3D\TaskDispatch\TaskDispatcher.h"
uint8_t gTaskMemory[1024];
#define THREAD_COUNT 4
cliqCity::multicore::Thread threads[THREAD_COUNT];
cliqCity::multicore::TaskData modelData[4];
cliqCity::multicore::TaskID taskIDs[4];
cliqCity::multicore::TaskDispatcher dispatchQueue(threads, THREAD_COUNT, gTaskMemory, 1024);
void PerformModelLoadTask(const cliqCity::multicore::TaskData& data);
#endif

class ParticlesScene : public IScene, public virtual IRendererDelegate
{
public:

	typedef cliqCity::graphicsMath::Vector2 vec2f;

	struct Particle
	{
		vec4f position;
		vec4f color;
		float velocity;
		bool active;
	};

	struct ParticleVertex
	{
		vec4f position;
		vec4f color; 
		vec2f uv;
	};

	struct SampleMatrixBuffer
	{
		mat4f mWorld;
		mat4f mView;
		mat4f mProjection;
	};

	SampleMatrixBuffer			mMatrixBuffer;
	DX3D11Renderer*				mRenderer;
	ID3D11Device*				mDevice;
	ID3D11DeviceContext*		mDeviceContext;
	ID3D11Buffer*				mConstantBuffer;
	ID3D11InputLayout*			mInputLayout;
	ID3D11VertexShader*			mVertexShader;
	ID3D11PixelShader*			mPixelShader;
	ID3D11SamplerState*			mSampleState;
	ID3D11BlendState*			mBlendState;
	ID3D11ShaderResourceView*	mTextureSRV;

	Transform					mCubeTransform;
	Transform					mChildTransform;

	LinearAllocator				mAllocator;
	MeshLibrary<LinearAllocator> mMeshLibrary;

	Camera						mCamera;
	float						mMouseX;
	float						mMouseY;

	vec3f						mParticleDeviation;
	float						mParticleVelocity, mParticleVelocityVariation;
	float						mParticleSize, mParticlesPerSecond;
	int							mMaxParticles;

	int							mCurrentParticleCount;
	float						mAccumulatedTime;

	Particle*					mParticleList;
	ParticleVertex*				mVertices;
	ID3D11Buffer*				mVertexBuffer;
	ID3D11Buffer*				mIndexBuffer;
	int							mVertexCount;

	bool show_test_window = false;
	bool show_another_window = false;
	ImVec4 clear_col = ImColor(114, 144, 154);

	ParticlesScene() : 
		mRenderer(nullptr), 
		mDevice(nullptr), 
		mDeviceContext(nullptr),
		mConstantBuffer(nullptr),
		mInputLayout(nullptr),
		mVertexShader(nullptr),
		mPixelShader(nullptr),
		mAllocator(1024)
	{
		mOptions.mWindowCaption = "Particle System Sample";
		mOptions.mWindowWidth = 800;
		mOptions.mWindowHeight = 600;
		mOptions.mGraphicsAPI = GRAPHICS_API_DIRECTX11;
		mOptions.mFullScreen = false;
		mMeshLibrary.SetAllocator(&mAllocator);
	}

	~ParticlesScene()
	{
		DX11IMGUI::Shutdown();
		ReleaseMacro(mVertexShader);
		ReleaseMacro(mPixelShader);
		ReleaseMacro(mConstantBuffer);
		ReleaseMacro(mInputLayout);
		ReleaseMacro(mTextureSRV);
		ReleaseMacro(mSampleState);
		ReleaseMacro(mVertexBuffer);
		ReleaseMacro(mIndexBuffer);
		ReleaseMacro(mBlendState);
	}

	void VInitialize() override
	{
		mRenderer = &DX3D11Renderer::SharedInstance();
		mRenderer->SetDelegate(this);

		mDevice = mRenderer->GetDevice();
		mDeviceContext = mRenderer->GetDeviceContext();

#ifdef MULTICORE
		dispatchQueue.Start();
#endif

		VOnResize();

		InitializeParticleSystem();
		InitializeGeometry();
		InitializeShaders();
		InitializeCamera();

		DX11IMGUI::Init(mRenderer->GetHWnd(), mRenderer->GetDevice(), mRenderer->GetDeviceContext());
	}

	void InitializeParticleSystem()
	{
		// Set the random deviation of where the particles can be located when emitted.
		mParticleDeviation.x = 30.5f;
		mParticleDeviation.y = 0.1f;
		mParticleDeviation.z = 10.0f;

		// Set the speed and speed variation of particles.
		mParticleVelocity = 1.0f;
		mParticleVelocityVariation = 0.2f;

		// Set the physical size of the particles.
		mParticleSize = 1.0f;

		// Set the number of particles to emit per second.
		mParticlesPerSecond = 1000.0f;

		// Set the maximum number of particles allowed in the particle system.
		mMaxParticles = 10000;

		// Create the particle list.
		mParticleList = new Particle[mMaxParticles];
		if (!mParticleList)
		{
			exit(0);
		}

		// Initialize the particle list.
		for (int i = 0; i<mMaxParticles; i++)
		{
			mParticleList[i].active = false;
		}

		// Initialize the current particle count to zero since none are emitted yet.
		mCurrentParticleCount = 0;

		// Clear the initial accumulated time for the particle per second emission rate.
		mAccumulatedTime = 0.0f;
	}

	void InitializeGeometry()
	{
		mVertexCount = mMaxParticles * 6;

		// Create the vertex array for the particles that will be rendered.
		mVertices = new ParticleVertex[mVertexCount];
		if (!mVertices)
		{
			exit(0);
		}

		// Create the index array.
		uint16_t* indices;
		indices = new uint16_t[mVertexCount];
		if (!indices)
		{
			return exit(0);
		}

		// Initialize vertex array to zeros at first.
		memset(mVertices, 0, (sizeof(ParticleVertex) * mVertexCount));

		// Initialize the index array.
		for (int i = 0; i < mVertexCount; i++)
		{
			indices[i] = i;
		}

		D3D11_BUFFER_DESC vertexBufferDesc;
		vertexBufferDesc.Usage = D3D11_USAGE_DYNAMIC;
		vertexBufferDesc.ByteWidth = sizeof(ParticleVertex) * mVertexCount;
		vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vertexBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		vertexBufferDesc.MiscFlags = 0;
		vertexBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA vertexData;
		vertexData.pSysMem = mVertices;
		vertexData.SysMemPitch = 0;
		vertexData.SysMemSlicePitch = 0;

		mDevice->CreateBuffer(&vertexBufferDesc, &vertexData, &mVertexBuffer);

		D3D11_BUFFER_DESC indexBufferDesc;
		indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
		indexBufferDesc.ByteWidth = sizeof(uint16_t) * mVertexCount;
		indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		indexBufferDesc.CPUAccessFlags = 0;
		indexBufferDesc.MiscFlags = 0;
		indexBufferDesc.StructureByteStride = 0;

		D3D11_SUBRESOURCE_DATA indexData;
		indexData.pSysMem = indices;
		indexData.SysMemPitch = 0;
		indexData.SysMemSlicePitch = 0;

		// Create the index buffer.
		mDevice->CreateBuffer(&indexBufferDesc, &indexData, &mIndexBuffer);

		delete[] indices;
		indices = 0;
	}

	void InitializeShaders()
	{
		D3D11_INPUT_ELEMENT_DESC inputDescription[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 16, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 }
		};

		// Load Vertex Shader --------------------------------------
		ID3DBlob* vsBlob;
		D3DReadFileToBlob(L"ParticlesVertexShader.cso", &vsBlob);

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
				3,									// Number of elments inside of Description
				vsBlob->GetBufferPointer(),
				vsBlob->GetBufferSize(),
				&mInputLayout);
		}

		// Clean up
		vsBlob->Release();

		// Load Pixel Shader ---------------------------------------
		ID3DBlob* psBlob;
		D3DReadFileToBlob(L"ParticlesPixelShader.cso", &psBlob);

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


		D3D11_SAMPLER_DESC samplerDesc;
		// Create a texture sampler state description.
		samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.MipLODBias = 0.0f;
		samplerDesc.MaxAnisotropy = 1;
		samplerDesc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		samplerDesc.BorderColor[0] = 0;
		samplerDesc.BorderColor[1] = 0;
		samplerDesc.BorderColor[2] = 0;
		samplerDesc.BorderColor[3] = 0;
		samplerDesc.MinLOD = 0;
		samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

		// Create the texture sampler state.
		mDevice->CreateSamplerState(&samplerDesc, &mSampleState);

		DirectX::CreateDDSTextureFromFileEx(mDevice, L"Textures\\star.dds", 0, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0, false, nullptr, &mTextureSRV);

		D3D11_BLEND_DESC blendStateDescription;
		ZeroMemory(&blendStateDescription, sizeof(D3D11_BLEND_DESC));
		blendStateDescription.RenderTarget[0].BlendEnable = TRUE;
		blendStateDescription.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		blendStateDescription.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
		blendStateDescription.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendStateDescription.RenderTarget[0].RenderTargetWriteMask = 0x0f;

		// Create the blend state using the description.
		mDevice->CreateBlendState(&blendStateDescription, &mBlendState);
	}

	void InitializeCamera()
	{
		mCamera.mTransform.SetPosition(0.0f, 0.0f, -30.0f);
		mCamera.mTransform.SetRotation(PI / 11.5f, 0.0f, 0.0f); 
	}

	void VUpdate(double milliseconds) override
	{
		Particles_Kill();
		Particles_Emit((float)milliseconds);
		Particles_Update((float)milliseconds);
		UpdateCamera();
	}

	void Particles_Emit(float frameTime)
	{
		bool emitParticle, found;
		vec3f position;
		float velocity;
		vec3f color;
		int index, i, j;


		// Increment the frame time.
		mAccumulatedTime += frameTime;

		// Set emit particle to false for now.
		emitParticle = false;

		// Check if it is time to emit a new particle or not.
		if (mAccumulatedTime > (1000.0f / mParticlesPerSecond))
		{
			mAccumulatedTime = 0.0f;
			emitParticle = true;
		}

		// If there are particles to emit then emit one per frame.
		if ((emitParticle == true) && (mCurrentParticleCount < (mMaxParticles - 1)))
		{
			mCurrentParticleCount++;

			// Now generate the randomized particle properties.
			position.x = (((float)rand() - (float)rand()) / RAND_MAX) * mParticleDeviation.x;
			position.y = (((float)rand() - (float)rand()) / RAND_MAX) * mParticleDeviation.y;
			position.z = (((float)rand() - (float)rand()) / RAND_MAX) * mParticleDeviation.z;

			velocity = mParticleVelocity + (((float)rand() - (float)rand()) / RAND_MAX) * mParticleVelocityVariation;

			color.x = (((float)rand() - (float)rand()) / RAND_MAX) + 0.5f;
			color.y = (((float)rand() - (float)rand()) / RAND_MAX) + 0.5f;
			color.z = (((float)rand() - (float)rand()) / RAND_MAX) + 0.5f;

			// Now since the particles need to be rendered from back to front for blending we have to sort the particle array.
			// We will sort using Z depth so we need to find where in the list the particle should be inserted.
			index = 0;
			found = false;
			while (!found)
			{
				if ((mParticleList[index].active == false) || (mParticleList[index].position.z < position.z))
				{
					found = true;
				}
				else
				{
					index++;
				}
			}

			// Now that we know the location to insert into we need to copy the array over by one position from the index to make room for the new particle.
			i = mCurrentParticleCount;
			j = i - 1;

			while (i != index)
			{
				mParticleList[i].position = mParticleList[j].position;
				mParticleList[i].color = mParticleList[j].color;
				mParticleList[i].velocity = mParticleList[j].velocity;
				mParticleList[i].active = mParticleList[j].active;
				i--;
				j--;
			}

			// Now insert it into the particle array in the correct depth order.
			mParticleList[index].position = position;
			mParticleList[index].color = color;
			mParticleList[index].velocity = velocity;
			mParticleList[index].active = true;
		}

		return;
	}

	void Particles_Kill()
	{
		int i, j;

		// Kill all the particles that have gone below a certain height range.
		for (i = 0; i < mMaxParticles; i++)
		{
			if ((mParticleList[i].active == true) && (mParticleList[i].position.y < -20.0f))
			{
				mParticleList[i].active = false;
				mCurrentParticleCount--;

				// Now shift all the live particles back up the array to erase the destroyed particle and keep the array sorted correctly.
				for (j = i; j < mMaxParticles - 1; j++)
				{
					mParticleList[j].position = mParticleList[j + 1].position;
					mParticleList[j].color = mParticleList[j + 1].color;
					mParticleList[j].velocity = mParticleList[j + 1].velocity;
					mParticleList[j].active = mParticleList[j + 1].active;
				}
			}
		}
		return;
	}

	void Particles_Update(float frameTime)
	{
		// Initialize vertex array to zeros at first.
		memset(mVertices, 0, (sizeof(ParticleVertex) * mVertexCount));

#ifdef MULTICORE
		int blockSize = mMaxParticles / THREAD_COUNT;
		bool done[4] = { false,false,false,false };
		for (int i = 0; i < THREAD_COUNT; i++)
		{
			modelData[i].mStream.out[0] = &mParticleList[i * blockSize];
			modelData[i].mStream.out[1] = &mVertices[i * 6 * blockSize];
			modelData[i].mStream.out[2] = &done[i];
			modelData[i].mStream.in[0] = &blockSize;
			modelData[i].mStream.in[1] = &frameTime;
			modelData[i].mStream.in[2] = &mParticleSize;
			if (i!=0)
			taskIDs[i] = dispatchQueue.AddTask(modelData[i], PerformModelLoadTask);
		}
		PerformModelLoadTask(modelData[0]);
		dispatchQueue.Synchronize();
#else
		// Now build the vertex array from the particle list array.  Each particle is a quad made out of two triangles.
		for (int i = 0; i<mCurrentParticleCount; i++)
		{
			mParticleList[i].position.y = mParticleList[i].position.y - (mParticleList[i].velocity * frameTime * 0.01f);

			vec4f color = vec4f(mParticleList[i].color, 1.0f);
			int index = i * 6;

			// Bottom left.
			mVertices[index].position = mParticleList[i].position + vec4f(-mParticleSize, -mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(0.0f, 1.0f);
			mVertices[index].color = color;
			index++;

			// Top left.
			mVertices[index].position = mParticleList[i].position + vec4f(-mParticleSize, mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(0.0f, 0.0f);
			mVertices[index].color = color;
			index++;

			// Bottom right.
			mVertices[index].position = mParticleList[i].position + vec4f(mParticleSize, -mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(1.0f, 1.0f);
			mVertices[index].color = color;
			index++;

			// Bottom right.
			mVertices[index].position = mParticleList[i].position + vec4f(mParticleSize, -mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(1.0f, 1.0f);
			mVertices[index].color = color;
			index++;

			// Top left.
			mVertices[index].position = mParticleList[i].position + vec4f(-mParticleSize, mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(0.0f, 0.0f);
			mVertices[index].color = color;
			index++;

			// Top right.
			mVertices[index].position = mParticleList[i].position + vec4f(mParticleSize, mParticleSize, 0, 1);
			mVertices[index].uv = vec2f(1.0f, 0.0f);
			mVertices[index].color = color;
			index++;
		}
#endif

		D3D11_MAPPED_SUBRESOURCE mappedResource;
		// Lock the vertex buffer.
		mDeviceContext->Map(mVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);

		ParticleVertex* verticesPtr = (ParticleVertex*)mappedResource.pData;
		// Copy the data into the vertex buffer.
		memcpy(verticesPtr, (void*)mVertices, (sizeof(ParticleVertex) * mVertexCount));

		// Unlock the vertex buffer.
		mDeviceContext->Unmap(mVertexBuffer, 0);
	}

	void UpdateCamera()
	{
		/*ScreenPoint mousePosition = Input::SharedInstance().mousePosition;
		if (Input::SharedInstance().GetMouseButton(MOUSEBUTTON_LEFT)) {
			mCamera.mTransform.RotatePitch(-(mousePosition.y - mMouseY) * RADIAN * CAMERA_ROTATION_SPEED);
			mCamera.mTransform.RotateYaw(-(mousePosition.x - mMouseX) * RADIAN * CAMERA_ROTATION_SPEED);
		}

		mMouseX = mousePosition.x;
		mMouseY = mousePosition.y;*/

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

		mMatrixBuffer.mView = mat4f::lookAtLH(position + mCamera.mTransform.GetForward(), position, vec3f(0.0f, 1.0f, 0.0f)).transpose();
	}

	void VRender() override
	{
		float color[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

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

		mDeviceContext->PSSetShaderResources(0, 1, &mTextureSRV);

		mDeviceContext->VSSetShader(mVertexShader, NULL, 0);
		mDeviceContext->PSSetShader(mPixelShader, NULL, 0);

		mDeviceContext->UpdateSubresource(mConstantBuffer, 0, NULL, &mMatrixBuffer, 0, 0);

		float blendFactor[4];


		// Setup the blend factor.
		blendFactor[0] = 0.0f;
		blendFactor[1] = 0.0f;
		blendFactor[2] = 0.0f;
		blendFactor[3] = 0.0f;


		mDeviceContext->VSSetConstantBuffers(0, 1, &mConstantBuffer);
		mDeviceContext->PSSetSamplers(0, 1, &mSampleState);
		mDeviceContext->OMSetBlendState(mBlendState, blendFactor, ~0);

		unsigned int stride = sizeof(ParticleVertex);
		unsigned int offset = 0;
		mDeviceContext->IASetVertexBuffers(0, 1, &mVertexBuffer, &stride, &offset);
		mDeviceContext->IASetIndexBuffer(mIndexBuffer, DXGI_FORMAT_R16_UINT, 0);
		
		mRenderer->VDrawIndexed(0, mVertexCount);
		
		ID3D11ShaderResourceView* nunll[1] = { nullptr };
		mDeviceContext->PSSetShaderResources(0, 1, nunll);

		mDeviceContext->OMSetRenderTargets(1, mRenderer->GetRenderTargetView(), nullptr);

		DX11IMGUI::NewFrame();
		// 1. Show a simple window
		// Tip: if we don't call ImGui::Begin()/ImGui::End() the widgets appears in a window automatically called "Debug"
		{
			static float f = 0.0f;
			ImGui::Text("Hello, world!");
			ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
			ImGui::ColorEdit3("clear color", (float*)&clear_col);
			if (ImGui::Button("Test Window")) show_test_window ^= 1;
			if (ImGui::Button("Another Window")) show_another_window ^= 1;
			ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate, ImGui::GetIO().Framerate);
		}

		// 2. Show another simple window, this time using an explicit Begin/End pair
		if (show_another_window)
		{
			ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiSetCond_FirstUseEver);
			ImGui::Begin("Another Window", &show_another_window);
			ImGui::Text("Hello");
			ImGui::End();
		}

		// 3. Show the ImGui test window. Most of the sample code is in ImGui::ShowTestWindow()
		if (show_test_window)
		{
			ImGui::SetNextWindowPos(ImVec2(650, 20), ImGuiSetCond_FirstUseEver);     // Normally user code doesn't need/want to call it because positions are saved in .ini file anyway. Here we just want to make the demo initial state a bit more friendly!
			ImGui::ShowTestWindow(&show_test_window);
		}

		ImGui::Render();

		mRenderer->VSwapBuffers();
	}

	void VOnResize() override
	{
		mMatrixBuffer.mProjection = mat4f::normalizedPerspectiveLH(PI * 0.2f, mRenderer->GetAspectRatio(), 0.1f, 1000.0f).transpose();
	}

	void VShutdown() override
	{
		if (mParticleList)
		{
			delete[] mParticleList;
		}

		mAllocator.Free();
	}
};

#ifdef MULTICORE
void PerformModelLoadTask(const cliqCity::multicore::TaskData& data)
{

	ParticlesScene::Particle* particle = reinterpret_cast<ParticlesScene::Particle*>(data.mStream.out[0]);
	ParticlesScene::ParticleVertex* vertices = reinterpret_cast<ParticlesScene::ParticleVertex*>(data.mStream.out[1]);
	bool* done = reinterpret_cast<bool*>(data.mStream.out[2]);
	int* blockSize = reinterpret_cast<int*>(data.mStream.in[0]);
	float frameTime = *(reinterpret_cast<float*>(data.mStream.in[1]));
	float mParticleSize = *(reinterpret_cast<float*>(data.mStream.in[2]));

	for (int i = 0; i < *blockSize; i++) {
		particle[i].position.y = particle[i].position.y - (particle[i].velocity * (frameTime) * 0.01f);

		vec4f color = vec4f(particle[i].color, 1.0f);
		int index = i * 6;

		// Bottom left.
		vertices[index].position = particle[i].position + vec4f(-mParticleSize, -mParticleSize, 0, 1);
		vertices[index].uv = vec2f(0.0f, 1.0f);
		vertices[index].color = color;
		index++;

		// Top left.
		vertices[index].position = particle[i].position + vec4f(-mParticleSize, mParticleSize, 0, 1);
		vertices[index].uv = vec2f(0.0f, 0.0f);
		vertices[index].color = color;
		index++;

		// Bottom right.
		vertices[index].position = particle[i].position + vec4f(mParticleSize, -mParticleSize, 0, 1);
		vertices[index].uv = vec2f(1.0f, 1.0f);
		vertices[index].color = color;
		index++;

		// Bottom right.
		vertices[index].position = particle[i].position + vec4f(mParticleSize, -mParticleSize, 0, 1);
		vertices[index].uv = vec2f(1.0f, 1.0f);
		vertices[index].color = color;
		index++;

		// Top left.
		vertices[index].position = particle[i].position + vec4f(-mParticleSize, mParticleSize, 0, 1);
		vertices[index].uv = vec2f(0.0f, 0.0f);
		vertices[index].color = color;
		index++;

		// Top right.
		vertices[index].position = particle[i].position + vec4f(mParticleSize, mParticleSize, 0, 1);
		vertices[index].uv = vec2f(1.0f, 0.0f);
		vertices[index].color = color;
		index++;
	}

	*done = true;
}
#endif

DECLARE_MAIN(ParticlesScene);