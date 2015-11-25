#pragma once

namespace Rig3D
{
	class IRenderContext
	{
	public:
		IRenderContext()
		{
			
		}

		virtual ~IRenderContext()
		{
			
		}

		virtual void VClearDepthStencilView() = 0;
		virtual void VClearRenderTargetViews() = 0;
		virtual void VClearRenderTextures() = 0;
		virtual void VClearShaderResourceViews() = 0;
	};
}