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
}