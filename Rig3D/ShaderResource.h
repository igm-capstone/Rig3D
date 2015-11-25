#pragma once
#include <vector>

namespace Rig3D
{
	template
		<
		typename BufferType, 
		typename TextureType, 
		typename SamplerType, 
		typename UnsignedIntegerType,
		typename ResourceReleasePolicy
		>
	class ShaderResource
	{
	public:
		ResourceReleasePolicy mReleasePolicy;

		ShaderResource() {}
		~ShaderResource() {}

		std::vector<TextureType>*			GetShaderResourceViews()		{ return &mShaderResourceViews[0]; }
		std::vector<SamplerType>*			GetSamplerStates()				{ return &mSamplerStates[0]; }
		std::vector<BufferType>*			GetConstantBuffers()			{ return &mConstantBuffers[0]; }
		std::vector<BufferType>*			GetInstanceBuffers()			{ return &mInstanceBuffers[0]; }
		std::vector<UnsignedIntegerType>*	GetInstanceBufferStrides()		{ return &mInstanceBufferStrides[0]; }
		std::vector<UnsignedIntegerType>*	GetInstanceBufferOffsets()		{ return &mInstanceBufferOffsets[0]; }

		uint32_t GetShaderResourceViewCount() const { return mShaderResourceViews.size(); }
		uint32_t GetSamplerStateCount() const		{ return mSamplerStates.size(); }
		uint32_t GetConstantBufferCount() const		{ return mConstantBuffers.size(); }
		uint32_t GetInstanceBufferCount() const		{ return mInstanceBuffers.size(); }

		void SetShaderResourceViews(std::vector<TextureType>& shaderResourceViews)
		{
			ClearShaderResourceViews();

			mShaderResourceViews = shaderResourceViews;
		}

		void SetSamplerStates(std::vector<TextureType>& samplerStates)
		{
			ClearSamplerStates();

			mSamplerStates = samplerStates;
		}
		
		void SetConstantBuffers(std::vector<TextureType>& buffers)
		{
			ClearConstantBuffers();

			mConstantBuffers = buffers;
		}
		
		void SetInstanceBuffers(std::vector<TextureType>& buffers, std::vector<TextureType>& strides, std::vector<UnsignedIntegerType>& offsets)
		{
			ClearInstanceBuffers();

			mInstanceBuffers = buffers;
			mInstanceBufferStrides = strides;
			mInstanceBufferOffsets = offsets;
		}

	private:
		std::vector<TextureType>			mShaderResourceViews;
		std::vector<SamplerType>			mSamplerStates;
		std::vector<BufferType>				mConstantBuffers;
		std::vector<BufferType>				mInstanceBuffers;
		std::vector<UnsignedIntegerType>	mInstanceBufferStrides;
		std::vector<UnsignedIntegerType>	mInstanceBufferOffsets;

		void ClearShaderResourceViews()
		{
			for (uint32_t i = 0; i < mShaderResourceViews.size(); i++)
			{
				mReleasePolicy.Release(mShaderResourceViews[i]);
			}

			mShaderResourceViews.clear();
		}

		void ClearConstantBuffers()
		{
			for (uint32_t i = 0; i < mConstantBuffers.size(); i++)
			{
				mReleasePolicy.Release(mConstantBuffers[i]);
			}

			mConstantBuffers.clear();
		}

		void ClearInstanceBuffers()
		{
			for (uint32_t i = 0; i < mInstanceBuffers.size(); i++)
			{
				mReleasePolicy.Release(mInstanceBuffers[i]);
			}

			mInstanceBuffers.clear();
			mInstanceBufferStrides.clear();
			mInstanceBufferOffsets.clear();
		}
		
		void ClearSamplerStates()
		{
			for (uint32_t i = 0; i < mSamplerStates.size(); i++)
			{
				mReleasePolicy.Release(mSamplerStates[i]);
			}

			mSamplerStates.clear();
		}
	};

	template<typename T>
	struct DefaultReleasePolicy
	{
		inline void Release(T t) {}
	};
}