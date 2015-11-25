#include "Rig3D/Graphics/DirectX11/DX11ShaderResource.h"
#include "DX3D11Renderer.h"

using namespace Rig3D;

DX11ShaderResource::DX11ShaderResource()
{
	
}

DX11ShaderResource::~DX11ShaderResource()
{
	for (uint32_t i = 0; i < mShaderResourceViews.size(); i++)
	{
		ReleaseMacro(mShaderResourceViews[i]);
	}

	mShaderResourceViews.clear();

	for (uint32_t i = 0; i < mSamplerStates.size(); i++)
	{
		ReleaseMacro(mSamplerStates[i]);
	}

	mSamplerStates.clear();

	for (uint32_t i = 0; i < mConstantBuffers.size(); i++)
	{
		ReleaseMacro(mConstantBuffers[i]);
	}

	mConstantBuffers.clear();

	for (uint32_t i = 0; i < mInstanceBuffers.size(); i++)
	{
		ReleaseMacro(mInstanceBuffers[i]);
	}

	mInstanceBuffers.clear();
	mInstanceBufferStrides.clear();
	mInstanceBufferOffsets.clear();
}

ID3D11ShaderResourceView** DX11ShaderResource::GetShaderResourceViews()
{
	return &mShaderResourceViews[0];
}

ID3D11SamplerState** DX11ShaderResource::GetSamplerStates()
{
	return &mSamplerStates[0];
}

ID3D11Buffer**	DX11ShaderResource::GetConstantBuffers()
{
	return &mConstantBuffers[0];
}

ID3D11Buffer**	DX11ShaderResource::GetInstanceBuffers()
{
	return &mInstanceBuffers[0];
}

UINT* DX11ShaderResource::GetInstanceBufferStrides()
{
	return &mInstanceBufferStrides[0];
}

UINT* DX11ShaderResource::GetInstanceBufferOffsets()
{
	return &mInstanceBufferOffsets[0];
}

uint32_t DX11ShaderResource::GetShaderResourceViewCount() const
{
	return mShaderResourceViews.size();
}

uint32_t DX11ShaderResource::GetSamplerStateCount() const
{
	return mSamplerStates.size();
}

uint32_t DX11ShaderResource::GetConstantBufferCount() const
{
	return mConstantBuffers.size();
}

uint32_t DX11ShaderResource::GetInstanceBufferCount() const
{
	return mInstanceBuffers.size();
}

void DX11ShaderResource::SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews)
{
	VClearShaderResourceViews();

	mShaderResourceViews = shaderResourceViews;
}

void DX11ShaderResource::SetSamplerStates(std::vector<ID3D11SamplerState*>& samplerStates)
{
	VClearSamplerStates();

	mSamplerStates = samplerStates;
}

void DX11ShaderResource::SetConstantBuffers(std::vector<ID3D11Buffer*>& buffers)
{
	VClearConstantBuffers();

	mConstantBuffers = buffers;
}

void DX11ShaderResource::SetInstanceBuffers(std::vector<ID3D11Buffer*>& buffers, std::vector<UINT>& strides, std::vector<UINT>& offsets)
{
	VClearInstanceBuffers();

	mInstanceBuffers = buffers;
	mInstanceBufferStrides = strides;
	mInstanceBufferOffsets = offsets;
}

void DX11ShaderResource::AddShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews)
{
	mShaderResourceViews.insert(std::end(mShaderResourceViews), std::begin(shaderResourceViews), std::end(shaderResourceViews));
}

void DX11ShaderResource::AddSamplerState(ID3D11SamplerState* samplerState)
{
	mSamplerStates.push_back(samplerState);
}

void DX11ShaderResource::VClearShaderResourceViews()
{
	for (uint32_t i = 0; i < mShaderResourceViews.size(); i++)
	{
		ReleaseMacro(mShaderResourceViews[i]);
	}

	mShaderResourceViews.clear();
}

void DX11ShaderResource::VClearSamplerStates()
{
	for (uint32_t i = 0; i < mSamplerStates.size(); i++)
	{
		ReleaseMacro(mSamplerStates[i]);
	}

	mSamplerStates.clear();
}

void DX11ShaderResource::VClearConstantBuffers()
{
	for (uint32_t i = 0; i < mConstantBuffers.size(); i++)
	{
		ReleaseMacro(mConstantBuffers[i]);
	}

	mConstantBuffers.clear();
}

void DX11ShaderResource::VClearInstanceBuffers()
{
	for (uint32_t i = 0; i < mInstanceBuffers.size(); i++)
	{
		ReleaseMacro(mInstanceBuffers[i]);
	}

	mInstanceBuffers.clear();
	mInstanceBufferStrides.clear();
	mInstanceBufferOffsets.clear();
}