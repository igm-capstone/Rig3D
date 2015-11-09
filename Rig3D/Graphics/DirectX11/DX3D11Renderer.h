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

		void	VUpdateConstantBuffer(void* buffer, void* data) override;

		void	VSetMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;
		void	VSetStaticMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;
		void	VSetDynamicMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) override;

		void	VSetMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;
		void	VSetStaticMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;
		void	VSetDynamicMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) override;

		void    VBindMesh(IMesh* mesh) override;

		void	VCreateShader(IShader** shader, LinearAllocator* allocator) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename, InputElement* inputElements, const uint32_t& count) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) override;
		void	VLoadVertexShader(IShader* vertexShader, const char* filename) override;
		void	VLoadPixelShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) override;
		void	VLoadPixelShader(IShader* pixelShader, const char* filename) override;

		void	VSetInputLayout(IShader* vertexShader) override;
		void	VSetVertexShaderInputLayout(IShader* vertexShader) override;
		void	VSetVertexShaderResources(IShader* vertexShader) override;
		void	VSetVertexShader(IShader* shader) override;

		void	VSetPixelShader(IShader* shader) override;

		void	VCreateShaderConstantBuffers(IShader* shader, void** data, size_t* sizes, const uint32_t& count) override;
		void	VUpdateShaderConstantBuffer(IShader* shader, void* data, uint32_t index) override;

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

