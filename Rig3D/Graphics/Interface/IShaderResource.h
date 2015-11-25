#pragma once

namespace Rig3D
{
	class IShaderResource
	{
	public:
		IShaderResource()
		{
			
		}

		virtual ~IShaderResource()
		{
			
		}

		virtual void VClearConstantBuffers() = 0;
		virtual void VClearInstanceBuffers() = 0;
		virtual void VClearShaderResourceViews() = 0;
		virtual void VClearSamplerStates() = 0;
	};
}