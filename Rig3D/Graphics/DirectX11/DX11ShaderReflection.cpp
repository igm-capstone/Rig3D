#include "DX11ShaderReflection.h"
#include "DX3D11Renderer.h"

using namespace Rig3D;

DX11ShaderReflection::DX11ShaderReflection() : mReflection(nullptr)
{
	
}

DX11ShaderReflection::~DX11ShaderReflection()
{
	ReleaseMacro(mReflection);
}