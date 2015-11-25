#include "DX11RenderContext.h"

using namespace Rig3D;

DX11RenderContext::DX11RenderContext() : mDepthStencilView(nullptr), mDepthStencilResourceView(nullptr)
{
	
}


DX11RenderContext::~DX11RenderContext()
{
	if (mDepthStencilView)
	{
		mDepthStencilView->Release();
		mDepthStencilView = nullptr;
	}

	if (mDepthStencilResourceView)
	{
		mDepthStencilResourceView->Release();
		mDepthStencilResourceView = nullptr;
	}

	for (ID3D11RenderTargetView* rtv : mRenderTargetViews)
	{
		if (rtv)
		{
			rtv->Release();
		}
	}

	mRenderTargetViews.clear();

	for (ID3D11Texture2D* texture2D : mRenderTextures)
	{
		if (texture2D)
		{
			texture2D->Release();
		}
	}

	mRenderTextures.clear();

	for (ID3D11ShaderResourceView* srv : mShaderResourceViews)
	{
		if (srv)
		{
			srv->Release();
		}
	}

	mShaderResourceViews.clear();
}

ID3D11DepthStencilView* DX11RenderContext::GetDepthStencilView()
{
	return mDepthStencilView;
}

ID3D11ShaderResourceView* DX11RenderContext::GetDepthStencilResourceView()
{
	return mDepthStencilResourceView;
}

ID3D11RenderTargetView** DX11RenderContext::GetRenderTargetViews()
{
	return &mRenderTargetViews[0];
}

ID3D11Texture2D** DX11RenderContext::GetRenderTextures()
{
	return &mRenderTextures[0];
}

ID3D11ShaderResourceView** DX11RenderContext::GetShaderResourceViews()
{
	return &mShaderResourceViews[0];
}

uint32_t  DX11RenderContext::GetRenderTargetViewCount() const
{
	return mRenderTargetViews.size();
}

uint32_t DX11RenderContext::GetRenderTextureCount() const
{
	return mRenderTextures.size();
}

uint32_t DX11RenderContext::GetShaderResourceViewCount() const
{
	return mShaderResourceViews.size();
}

void DX11RenderContext::SetDepthStencilView(ID3D11DepthStencilView* depthStencilView)
{
	if (mDepthStencilView)
	{
		mDepthStencilView->Release();
		mDepthStencilView = nullptr;
	}

	mDepthStencilView = depthStencilView;
}

void DX11RenderContext::SetDepthStencilResourceView(ID3D11ShaderResourceView* depthStencilResourceView)
{
	if (mDepthStencilResourceView)
	{
		mDepthStencilResourceView->Release();
		mDepthStencilResourceView = nullptr;
	}

	mDepthStencilResourceView = depthStencilResourceView;
}

void DX11RenderContext::SetRenderTargetViews(std::vector<ID3D11RenderTargetView*>& renderTargetViews)
{
	VClearRenderTargetViews();

	mRenderTargetViews = renderTargetViews;
}

void DX11RenderContext::SetRenderTextures(std::vector<ID3D11Texture2D*>& renderTextures)
{
	VClearRenderTextures();

	mRenderTextures = renderTextures;
}

void DX11RenderContext::SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews)
{
	VClearShaderResourceViews();

	mShaderResourceViews = shaderResourceViews;
}

void DX11RenderContext::VClearDepthStencilView()
{
	if (mDepthStencilView)
	{
		mDepthStencilView->Release();
	}

	if (mDepthStencilResourceView)
	{
		mDepthStencilResourceView->Release();
	}
}

void DX11RenderContext::VClearRenderTargetViews()
{
	for (ID3D11RenderTargetView* rtv : mRenderTargetViews)
	{
		if (rtv)
		{
			rtv->Release();
		}
	}

	mRenderTargetViews.clear();
}

void DX11RenderContext::VClearRenderTextures()
{
	for (ID3D11Texture2D* texture2D : mRenderTextures)
	{
		if (texture2D)
		{
			texture2D->Release();
		}
	}

	mRenderTextures.clear();
}

void DX11RenderContext::VClearShaderResourceViews()
{
	for (ID3D11ShaderResourceView* srv : mShaderResourceViews)
	{
		if (srv)
		{
			srv->Release();
		}
	}

	mShaderResourceViews.clear();
}