#include "DX11Shader.h"
#include "DX3D11Renderer.h"

using namespace Rig3D;

DX11Shader::DX11Shader() : 
	mVertexShader(nullptr),
	mInputLayout(nullptr)
{
}


DX11Shader::~DX11Shader()
{
	ReleaseMacro(mVertexShader);
	ReleaseMacro(mInputLayout);
	ReleaseMacro(mPixelShader);

	ClearConstantBuffers();
	ClearInstanceBuffers();
	ClearShaderResourceViews();
	ClearSamplerStates();
}

ID3D11ShaderResourceView** DX11Shader::GetShaderResourceViews()
{
	return &mShaderResourceViews[0];
}

ID3D11SamplerState** DX11Shader::GetSamplerStates()
{
	return &mSamplerStates[0];
}

ID3D11Buffer**	DX11Shader::GetConstantBuffers()
{
	return &mConstantBuffers[0];
}

ID3D11Buffer**	DX11Shader::GetInstanceBuffers()
{
	return &mInstanceBuffers[0];
}

UINT* DX11Shader::GetInstanceBufferStrides()
{
	return &mInstanceBufferStrides[0];
}

UINT* DX11Shader::GetInstanceBufferOffsets()
{
	return &mInstanceBufferOffsets[0];
}

uint32_t DX11Shader::GetShaderResourceViewCount() const
{
	return mShaderResourceViews.size();
}

uint32_t DX11Shader::GetSamplerStateCount() const
{
	return mSamplerStates.size();
}

uint32_t DX11Shader::GetConstantBufferCount() const
{
	return mConstantBuffers.size();
}

uint32_t DX11Shader::GetInstanceBufferCount() const
{
	return mInstanceBuffers.size();
}

void DX11Shader::SetShaderResourceViews(std::vector<ID3D11ShaderResourceView*>& shaderResourceViews)
{
	ClearShaderResourceViews();

	mShaderResourceViews = shaderResourceViews;
}

void DX11Shader::SetSamplerStates(std::vector<ID3D11SamplerState*>& samplerStates)
{
	ClearSamplerStates();

	mSamplerStates = samplerStates;
}

void DX11Shader::SetConstantBuffers(std::vector<ID3D11Buffer*>& buffers)
{
	ClearConstantBuffers();

	mConstantBuffers = buffers;
}

void DX11Shader::SetInstanceBuffers(std::vector<ID3D11Buffer*>& buffers, std::vector<UINT>& strides, std::vector<UINT>& offsets)
{
	ClearInstanceBuffers();

	mInstanceBuffers = buffers;
	mInstanceBufferStrides = strides;
	mInstanceBufferOffsets = offsets;
}

void DX11Shader::ClearShaderResourceViews()
{
	for (uint32_t i = 0; i < mShaderResourceViews.size(); i++)
	{
		ReleaseMacro(mShaderResourceViews[i]);
	}

	mShaderResourceViews.clear();
}

void DX11Shader::ClearSamplerStates()
{
	for (uint32_t i = 0; i < mSamplerStates.size(); i++)
	{
		ReleaseMacro(mSamplerStates[i]);
	}

	mSamplerStates.clear();
}

void DX11Shader::ClearConstantBuffers()
{
	for (uint32_t i = 0; i < mConstantBuffers.size(); i++)
	{
		ReleaseMacro(mConstantBuffers[i]);
	}

	mConstantBuffers.clear();
}

void DX11Shader::ClearInstanceBuffers()
{
	for (uint32_t i = 0; i < mInstanceBuffers.size(); i++)
	{
		ReleaseMacro(mInstanceBuffers[i]);
	}

	mInstanceBuffers.clear();
	mInstanceBufferStrides.clear();
	mInstanceBufferOffsets.clear();
}