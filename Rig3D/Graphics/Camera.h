#pragma once
#include "Rig3D\Common\Transform.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	class RIG3D Camera
	{
	private:
		mat4f		mProjection;
		mat4f		mView;

	public:
		Transform	mTransform;

		Camera();
		~Camera();

		mat4f GetProjectionMatrix();
		mat4f GetViewMatrix();

		void SetProjectionMatrix(const mat4f& projection);
		void SetViewMatrix(const mat4f& view);
	};

}
