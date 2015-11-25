#pragma once
#include "Rig3D/Graphics/Interface/IShaderResource.h"
#include <d3d11.h>
#include <vector>

namespace Rig3D
{
	class DX11ShaderResource : public IShaderResource
	{
	public:
		DX11ShaderResource();
		~DX11ShaderResource();

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

		void AddShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews);
		void AddSamplerState(ID3D11SamplerState* samplerState);

		void VClearConstantBuffers() override;
		void VClearInstanceBuffers() override;
		void VClearShaderResourceViews() override;
		void VClearSamplerStates() override;

	private:
		std::vector<ID3D11ShaderResourceView*>	mShaderResourceViews;
		std::vector<ID3D11SamplerState*>		mSamplerStates;
		std::vector<ID3D11Buffer*>				mConstantBuffers;
		std::vector<ID3D11Buffer*>				mInstanceBuffers;
		std::vector<UINT>						mInstanceBufferStrides;
		std::vector<UINT>						mInstanceBufferOffsets;
	};
}