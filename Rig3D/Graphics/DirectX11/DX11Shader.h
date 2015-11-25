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
	};
}