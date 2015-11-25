#pragma once
#include "Rig3D\Graphics\Interface\IRenderer.h"
#include "Rig3D\Common\WMEventHandler.h"
#include "Rig3D\Graphics\rig_graphics_api_conversions.h"
#include "Rig3D\Graphics\DirectX11\dxerr.h"
#include <assert.h>
#include <d3dcompiler.h>

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

// Convenience macro for releasing a COM object
#define ReleaseMacro(x) { if(x){ x->Release(); x = 0; } }

// Macro for popping up a text box based
// on a failed HRESULT and then quitting (only in debug builds)
#if defined(DEBUG) | defined(_DEBUG)
#ifndef HR
#define HR(x)																\
				{															\
		HRESULT hr = (x);													\
		if(FAILED(hr))														\
						{													\
			DXTrace(__FILEW__, (DWORD)__LINE__, hr, L#x, true);				\
			PostQuitMessage(0);												\
						}													\
				}														
#endif
#else
#ifndef HR
#define HR(x) (x)
#endif
#endif

namespace Rig3D
{
	class DX11Shader;

	class RIG3D DX3D11Renderer : public IRenderer, public IObserver
	{
	public:
		static DX3D11Renderer& SharedInstance();

		int		VInitialize(HINSTANCE hInstance, HWND hwnd, Options options) override;
		void	VOnResize() override;
		void	VUpdateScene(const double& milliseconds) override;
		void	VRenderScene() override;
		void	VShutdown() override;

		inline void	VSetPrimitiveType(GPUPrimitiveType type) override;
		inline void	VDrawIndexed(GPUPrimitiveType type, uint32_t startIndex, uint32_t count) override;
		inline void	VDrawIndexed(uint32_t startIndex, uint32_t count) override;

#pragma region  Buffer

		void	VCreateVertexBuffer(void* buffer, void* vertices, const size_t& size) override;
		void	VCreateStaticVertexBuffer(void* buffer, void* vertices, const size_t& size) override;
		void	VCreateDynamicVertexBuffer(void* buffer, void* vertices, const size_t& size) override;

		void	VCreateIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) override;
		void	VCreateStaticIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) override;
		void	VCreateDynamicIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) override;

		void	VCreateInstanceBuffer(void* buffer, void* data, const size_t& size) override;
		void	VCreateStaticInstanceBuffer(void* buffer, void* data, const size_t& size) override;
		void	VCreateDynamicInstanceBuffer(void* buffer, void* data, const size_t& size) override;

		void	VCreateConstantBuffer(void* buffer, void* data, const size_t& size) override;
		void	VCreateStaticConstantBuffer(void* buffer, void* data, const size_t& size) override;
		void	VCreateDynamicConstantBuffer(void* buffer, void* data, const size_t& size) override;

		void	VUpdateBuffer(void* buffer, void* data) override;
		void	VUpdateBuffer(void* buffer, void* data, const size_t& size) override;

#pragma endregion 

#pragma region Texture

		void VCreateNativeFormat(void* nativeFormat, InputFormat textureFormat) override;
		void VCreateTexture2D(void* texture, void* data, uint32_t mipLevels, InputFormat textureFormat) override;

		// 1 Channel (32 Bit) Texture.
		void VCreateDepthTexture2D(void* texture2D) override;
		void VCreateDepthResourceTexture2D(void * texture2D) override;

		void VCreateDepthStencilTexture2D(void* texture2D) override;
		void VCreateDepthStencilResourceTexture2D(void * texture2D) override;

		// 4 Channel (32 Bit) Texture.
		void VCreateRenderTexture2D(void* texture2D) override;
		void VCreateRenderResourceTexture2D(void * texture2D) override;

#pragma endregion 

#pragma region SamplerState

		void VCreateLinearClampSamplerState(void* samplerState) override;
		void VCreateLinearWrapSamplerState(void* samplerState) override;
		void VCreateLinearBorderSamplerState(void* samplerState, float* color) override;

#pragma endregion 

#pragma region Mesh

		void	VSetMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;
		void	VSetStaticMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;
		void	VSetDynamicMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;

		void	VSetMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;
		void	VSetStaticMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;
		void	VSetDynamicMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;

		void	VUpdateMeshVertexBuffer(IMesh* mesh, void* data, const size_t& size) override;
		void	VUpdateMeshIndexBuffer(IMesh* mesh, void* data, const uint32_t& count) override;

		void    VBindMesh(IMesh* mesh) override;

#pragma endregion 

#pragma region Shader

		void	VCreateShader(IShader** shader, LinearAllocator* allocator) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename, InputElement* inputElements, const uint32_t& count) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename) override;
		void	VLoadPixelShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) override;
		void	VLoadPixelShader(IShader* pixelShader, const char* filename) override;

		void	VSetInputLayout(IShader* vertexShader) override;
		void	VSetVertexShaderInputLayout(IShader* vertexShader) override;
		void	VSetVertexShader(IShader* shader) override;
		void	VSetPixelShader(IShader* shader) override;

#pragma endregion 

#pragma region Shader Resource

		void	VCreateShaderResource(IShaderResource** shaderResouce, LinearAllocator* allocator) override;

		void	VCreateShaderTextures2D(IShaderResource* shader, const char** filenames, const uint32_t count) override;
		void	VCreateShaderContextTextures2D(IShaderResource* shader, IRenderContext* context) override;

		void	VAddShaderTextures2D(IShaderResource* shader, const char** filenames, const uint32_t count) override;

		void	VCreateShaderTextureCubes(IShaderResource* shader, const char** filenames, const uint32_t count) override;

		void	VCreateShaderConstantBuffers(IShaderResource* shader, void** data, size_t* sizes, const uint32_t& count) override;

		void	VCreateShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count) override;
		void	VCreateStaticShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count) override;
		void	VCreateDynamicShaderInstanceBuffers(IShaderResource* shader, void** data, size_t* sizes, size_t* strides, size_t* offsets, const uint32_t& count) override;

		void	VUpdateShaderConstantBuffer(IShaderResource* shader, void* data, const uint32_t& index) override;
		void	VUpdateShaderInstanceBuffer(IShaderResource* shader, void* data, const size_t& size, const uint32_t& index) override;

		void	VSetVertexShaderConstantBuffers(IShaderResource* shaderResource) override;
		void	VSetVertexShaderConstantBuffer(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex) override;

		void	VSetPixelShaderConstantBuffers(IShaderResource* shaderResource) override;
		void	VSetPixelShaderConstantBuffer(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex) override;

		void	VSetVertexShaderInstanceBuffers(IShaderResource* shaderResource) override;

		void	VSetPixelShaderResourceViews(IShaderResource* shaderResource) override;
		void	VSetPixelShaderResourceView(IShaderResource* shaderResource, const uint32_t& atIndex, const uint32_t& toBindingIndex) override;


		void	VAddShaderLinearSamplerState(IShaderResource* shaderResource, SamplerStateAddressType addressType, float* color = nullptr) override;
		
		void	VSetPixelShaderSamplerStates(IShaderResource* shaderResource) override;

#pragma endregion 

#pragma region Render Context

		void	VCreateRenderContext(IRenderContext** renderContext, LinearAllocator* allocator) override;

		void	VCreateContextDepthStencilTarget(IRenderContext* renderContext) override;
		void	VCreateContextTargets(IRenderContext* renderContext, const uint32_t& count) override;
		
		void	VCreateContextDepthResourceTarget(IRenderContext* renderContext) override;
		void	VCreateContextResourceTargets(IRenderContext* renderContext, const uint32_t& count) override;

		void	VSetContextTarget() override;
		void	VSetContextTargetWithDepth() override;
		void	VSetRenderContextTargets(IRenderContext* renderContext) override;
		void	VSetRenderContextTargetsWithDepth(IRenderContext* renderContext) override;

		void	VSetRenderContextTarget(IRenderContext* renderContext, const uint32_t& atIndex) override;
		void	VSetRenderContextTargetWithDepth(IRenderContext* renderContext, const uint32_t& atIndex) override;

		void	VClearContext(const float* color, float depth, uint8_t stencil) override;
		void	VClearContext(IRenderContext* renderContext, const float* color, float depth, uint8_t stencil) override;

		void	VClearContextTarget(const float* color) override;
		void	VClearContextTarget(IRenderContext* renderContext, const uint32_t& atIndex, const float* color) override;

		void	VClearDepthStencil(float depth, uint8_t stencil) override;
		void	VClearDepthStencil(IRenderContext* renderContext, float depth, uint8_t stencil) override;

		void	VSetVertexShaderDepthResourceView(IRenderContext* renderContext, const uint32_t& toBindingIndex) override;
		void	VSetPixelShaderDepthResourceView(IRenderContext* renderContext, const uint32_t& toBindingIndex) override;

		void	VSetVertexShaderResourceViews(IRenderContext* renderContext) override;
		void	VSetPixelShaderResourceViews(IRenderContext* renderContext) override;

		void	VSetVertexShaderResourceView(IRenderContext* renderContext, const uint32_t& atIndex, const uint32_t& toBindingIndex) override;
		void	VSetPixelShaderResourceView(IRenderContext* renderContext, const uint32_t& atIndex, const uint32_t& toBindingIndex) override;

#pragma endregion 

		void	VSwapBuffers() override;

		int						InitializeD3D11();
		ID3D11Device*			GetDevice()				const;
		ID3D11DeviceContext*	GetDeviceContext()		const;
		IDXGISwapChain*			GetSwapChain()			const;
		ID3D11Texture2D*		GetDepthStencilBuffer() const;
		ID3D11RenderTargetView* const* GetRenderTargetView()	const;
		ID3D11DepthStencilView* GetDepthStencilView()	const;
		D3D11_VIEWPORT const&	GetViewport()			const;

		void	HandleEvent(const IEvent& iEvent) override;

	private:
		UINT					mMSAA4xQuality;
		ID3D11Device*			mDevice;
		ID3D11DeviceContext*	mDeviceContext;
		IDXGISwapChain*			mSwapChain;
		ID3D11Texture2D*		mDepthStencilBuffer;
		ID3D11RenderTargetView* mRenderTargetView;
		ID3D11DepthStencilView* mDepthStencilView;
		D3D11_VIEWPORT			mViewport;
		D3D_DRIVER_TYPE			mDriverType;
		D3D_FEATURE_LEVEL		mFeatureLevel;

		bool					mEnable4xMsaa;
		bool					mIsPaused;
		bool					mIsMaximized;
		bool					mIsMinimized;
		bool					mIsResizing;

		DX3D11Renderer();
		~DX3D11Renderer();

		DX3D11Renderer(DX3D11Renderer const&) = delete;
		void operator=(DX3D11Renderer const&) = delete;

		void SetVertexShaderInputLayout(ID3D11ShaderReflection* reflection, ID3DBlob* vsBlob, D3D11_SHADER_DESC* shaderDesc, DX11Shader* vertexShader);
		void SetShaderConstantBuffers(ID3D11ShaderReflection* reflection, D3D11_SHADER_DESC* shaderDesc, DX11Shader* shader, LinearAllocator* allocator);
		void SetShaderResources(ID3D11ShaderReflection* reflection, D3D11_SHADER_DESC* shaderDesc, DX11Shader* shader);
	};
}

