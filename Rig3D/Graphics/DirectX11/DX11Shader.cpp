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

	ClearBuffers();
	ClearShaderResourceViews();
	ClearSamplerStates();
}

ID3D11Buffer**	DX11Shader::GetBuffers()
{
	return &mBuffers[0];
}

ID3D11ShaderResourceView** DX11Shader::GetShaderResourceViews()
{
	return &mShaderResourceViews[0];
}

ID3D11SamplerState** DX11Shader::GetSamplerStates()
{
	return &mSamplerStates[0];
}

uint32_t DX11Shader::GetBufferCount() const
{
	return mBuffers.size();
}

uint32_t DX11Shader::GetShaderResourceViewCount() const
{
	return mShaderResourceViews.size();
}

uint32_t DX11Shader::GetSamplerStateCount() const
{
	return mSamplerStates.size();
}

void DX11Shader::SetBuffers(std::vector<ID3D11Buffer*>& buffers)
{
	ClearBuffers();

	mBuffers = buffers;
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

void DX11Shader::ClearBuffers()
{
	for (uint32_t i = 0; i < mBuffers.size(); i++)
	{
		ReleaseMacro(mBuffers[i]);
	}

	mBuffers.clear();
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