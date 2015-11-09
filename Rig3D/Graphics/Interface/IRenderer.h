#pragma once
#include <Windows.h>
#include <stdint.h>
#include "Rig3D\Options.h"
#include "Memory/Memory/Memory.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif


namespace Rig3D 
{
	class IScene;
	class IMesh;
	class IShader;

	enum InputClass
	{
		INPUT_CLASS_PER_VERTEX,
		INPUT_CLASS_PER_INSTANCE
	};

	enum InputFormat
	{
		FLOAT2,
		FLOAT3,
		FLOAT4
	};

	struct InputElement
	{
		const char* Name;
		uint32_t	Index;
		uint32_t	InputSlot;
		uint32_t	ByteOffset;
		uint32_t	InstanceStepRate;
		InputFormat	Format;
		InputClass	InputSlotClass;
	};

	class RIG3D IRendererDelegate
	{
	public:
		virtual void VOnResize() = 0;

		IRendererDelegate() {};
		virtual ~IRendererDelegate() {};
	};

	class RIG3D IRenderer
	{
	public:
		IRenderer();
		virtual ~IRenderer();
		
		virtual int		VInitialize(HINSTANCE hInstance, HWND hwnd, Options options) = 0;
		virtual void	VOnResize() = 0;
		virtual void	VUpdateScene(const double& milliseconds) = 0;
		virtual void	VRenderScene() = 0;
		virtual void	VShutdown() = 0;

		virtual void	VSetPrimitiveType(GPUPrimitiveType type) = 0;
		virtual void	VDrawIndexed(GPUPrimitiveType type, uint32_t startIndex, uint32_t count) = 0;
		virtual void    VDrawIndexed(uint32_t startIndex, uint32_t count) = 0;

#pragma region Buffer

		virtual void	VCreateVertexBuffer(void* buffer, void* vertices, const size_t& size) = 0;
		virtual void	VCreateStaticVertexBuffer(void* buffer, void* vertices, const size_t& size) = 0;
		virtual void	VCreateDynamicVertexBuffer(void* buffer, void* vertices, const size_t& size) = 0;

		virtual void	VCreateIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) = 0;
		virtual void	VCreateStaticIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) = 0;
		virtual void	VCreateDynamicIndexBuffer(void* buffer, uint16_t* indices, const uint32_t& count) = 0;

		virtual void	VCreateInstanceBuffer(void* buffer, void* data, const size_t& size) = 0;
		virtual void	VCreateStaticInstanceBuffer(void* buffer, void* data, const size_t& size) = 0;
		virtual void	VCreateDynamicInstanceBuffer(void* buffer, void* data, const size_t& size) = 0;

		virtual void	VCreateConstantBuffer(void* buffer, void* data, const size_t& size) = 0;
		virtual void	VCreateStaticConstantBuffer(void* buffer, void* data, const size_t& size) = 0;
		virtual void	VCreateDynamicConstantBuffer(void* buffer, void* data, const size_t& size) = 0;

		virtual void	VUpdateConstantBuffer(void* buffer, void* data) = 0;
#pragma endregion

		virtual void	VSetMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) = 0;
		virtual void	VSetStaticMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) = 0;
		virtual void	VSetDynamicMeshVertexBuffer(IMesh* mesh, void* vertices, const size_t& size, const size_t& stride) = 0;

		virtual void	VSetMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) = 0;
		virtual void	VSetStaticMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) = 0;
		virtual void	VSetDynamicMeshIndexBuffer(IMesh* mesh, uint16_t* indices, const uint32_t& count) = 0;

		virtual void    VBindMesh(IMesh* mesh) = 0;

#pragma region Shaders

		virtual void	VCreateShader(IShader** shader, LinearAllocator* allocator) = 0;
		virtual void	VLoadVertexShader(IShader* vertexShader, const char* filename, InputElement* inputElements, const uint32_t& count) = 0;
		virtual void	VLoadVertexShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) = 0;
		virtual void	VLoadVertexShader(IShader* vertexShader, const char* filename) = 0;
		virtual void	VLoadPixelShader(IShader* vertexShader, const char* filename, LinearAllocator* allocator) = 0;
		virtual void	VLoadPixelShader(IShader* pixelShader, const char* filename) = 0;

		virtual void	VSetInputLayout(IShader* vertexShader) = 0;
		virtual void	VSetVertexShaderInputLayout(IShader* vertexShader) = 0;
		virtual void	VSetVertexShaderResources(IShader* vertexShader) = 0;
		virtual void	VSetVertexShader(IShader* shader) = 0;
		virtual void	VSetPixelShader(IShader* shader) = 0;

		// Constant buffers will be bound to GPU registers in the order they are arranged
		virtual void	VCreateShaderConstantBuffers(IShader* shader, void** data, size_t* sizes, const uint32_t& count) = 0;

		// Use the register number as an index
		virtual void	VUpdateShaderConstantBuffer(IShader* shader, void* data, uint32_t index) = 0;

#pragma endregion

		virtual void    VSwapBuffers() = 0;

		void SetWindowCaption(const char* caption);
		
		inline float		GetAspectRatio() const { return static_cast<float>(mWindowWidth) / mWindowHeight; };
		inline const int&	GetWindowWidth() const { return mWindowWidth; };
		inline const int&	GetWindowHeight() const { return mWindowHeight; };
		inline GraphicsAPI	GetGraphicsAPI() const { return mGraphicsAPI; };
	
		inline void SetDelegate(IRendererDelegate* renderDelegate) { mDelegate = renderDelegate; };

	protected:
		HINSTANCE			mHINSTANCE;
		HWND				mHWND;
		int					mWindowWidth;
		int					mWindowHeight;
		GraphicsAPI			mGraphicsAPI;
		IRendererDelegate*	mDelegate;
		const char*			mWindowCaption;		
		bool				mFullScreen;
	};
}

