#include "Camera.h"

using namespace Rig3D;

Camera::Camera()
{
}

Camera::~Camera()
{
}

mat4f Camera::GetProjectionMatrix()
{
	return mProjection;
}

mat4f Camera::GetViewMatrix()
{
	if (mTransform.IsDirty())
	{
		mView = mTransform.GetWorldMatrix().inverse();
	}

	return mView;
}

void Camera::SetProjectionMatrix(const mat4f& projection)
{
	mProjection = projection;
}

void Camera::SetViewMatrix(const mat4f& view)
{
	mView = view;
}