#pragma once
#include "Rig3D/Graphics/Interface/IRenderContext.h"
#include <d3d11.h>
#include <vector>

namespace Rig3D
{
	class DX11RenderContext : public IRenderContext
	{
	public:
		DX11RenderContext();
		~DX11RenderContext();

		ID3D11DepthStencilView*		GetDepthStencilView();
		ID3D11ShaderResourceView*	GetDepthStencilResourceView();
		ID3D11RenderTargetView**	GetRenderTargetViews();
		ID3D11Texture2D**			GetRenderTextures();
		ID3D11ShaderResourceView**  GetShaderResourceViews();

		uint32_t GetRenderTargetViewCount() const;
		uint32_t GetRenderTextureCount() const;
		uint32_t GetShaderResourceViewCount() const;

		void SetDepthStencilView(ID3D11DepthStencilView* depthStencilView);
		void SetDepthStencilResourceView(ID3D11ShaderResourceView* depthStencilResourceView);
		void SetRenderTargetViews(std::vector<ID3D11RenderTargetView*>& renderTargetViews);
		void SetRenderTextures(std::vector<ID3D11Texture2D*>& renderTextures);
		void SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews);
	
		void VClearDepthStencilView() override;
		void VClearRenderTargetViews() override;
		void VClearRenderTextures() override;
		void VClearShaderResourceViews() override;

	private:
		ID3D11DepthStencilView*					mDepthStencilView;
		ID3D11ShaderResourceView*				mDepthStencilResourceView;

		std::vector<ID3D11RenderTargetView*>	mRenderTargetViews;
		std::vector<ID3D11Texture2D*>			mRenderTextures;
		std::vector<ID3D11ShaderResourceView*>	mShaderResourceViews;
	};
}