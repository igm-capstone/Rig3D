#pragma once
#include <d3dcompiler.h>
#include <map>
#include "DX11Shader.h"

namespace Rig3D
{

	struct DX11ShaderVariable
	{
		uint32_t ByteOffset;
		uint32_t Size;
		uint32_t BufferIndex;
	};

	struct DX11ShaderBuffer
	{
		ID3D11Buffer*	ConstantBuffer;
		void*			Data;
		uint8_t			BindIndex;
	};

	class DX11ShaderReflection
	{
	public:
		ID3D11ShaderReflection* mReflection;

		DX11ShaderReflection();
		~DX11ShaderReflection();
	};
}