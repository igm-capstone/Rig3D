#pragma once
#include "Rig3D/Graphics/Interface/IShader.h"
#include <vector>
#include <d3d11.h>

namespace Rig3D
{
	class DXD11Renderer;

	class DX11Shader : public IShader
	{
	public:
		DX11Shader();
		~DX11Shader();

		ID3D11ShaderResourceView**	GetShaderResourceViews();
		ID3D11SamplerState**		GetSamplerStates();
		ID3D11Buffer**				GetConstantBuffers();
		ID3D11Buffer**				GetInstanceBuffers();
		UINT*						GetInstanceBufferStrides();
		UINT*						GetInstanceBufferOffsets();

		uint32_t GetShaderResourceViewCount() const;
		uint32_t GetSamplerStateCount() const;
		uint32_t GetConstantBufferCount() const;
		uint32_t GetInstanceBufferCount() const;

		void SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews);
		void SetSamplerStates(std::vector<ID3D11SamplerState*>& samplerStates);
		void SetConstantBuffers(std::vector<ID3D11Buffer*>& buffers);
		void SetInstanceBuffers(std::vector<ID3D11Buffer*>& buffers, std::vector<UINT>& strides, std::vector<UINT>& offsets);

		union
		{
			struct
			{
				ID3D11VertexShader* mVertexShader;
				ID3D11InputLayout*	mInputLayout;
			};

			struct
			{
				ID3D11PixelShader*	mPixelShader;
				char padding[sizeof(void*)];
			};
		};

	private:
		std::vector<ID3D11ShaderResourceView*>	mShaderResourceViews;
		std::vector<ID3D11SamplerState*>		mSamplerStates;
		std::vector<ID3D11Buffer*>				mConstantBuffers;
		std::vector<ID3D11Buffer*>				mInstanceBuffers;
		std::vector<UINT>						mInstanceBufferStrides;
		std::vector<UINT>						mInstanceBufferOffsets;

		void ClearConstantBuffers();
		void ClearInstanceBuffers();
		void ClearShaderResourceViews();
		void ClearSamplerStates();
	};
}