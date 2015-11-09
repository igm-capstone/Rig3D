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

		ID3D11Buffer**				GetBuffers();
		ID3D11ShaderResourceView**	GetShaderResourceViews();
		ID3D11SamplerState**		GetSamplerStates();

		uint32_t GetBufferCount() const;
		uint32_t GetShaderResourceViewCount() const;
		uint32_t GetSamplerStateCount() const;

		void SetBuffers(std::vector<ID3D11Buffer*>& buffers);
		void SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews);
		void SetSamplerStates(std::vector<ID3D11SamplerState*>& samplerStates);

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
		std::vector<ID3D11Buffer*>				mBuffers;
		std::vector<ID3D11ShaderResourceView*>	mShaderResourceViews;
		std::vector<ID3D11SamplerState*>		mSamplerStates;

		void ClearBuffers();
		void ClearShaderResourceViews();
		void ClearSamplerStates();
	};
}