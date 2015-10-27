#include "Transform.h"
#include "GraphicsMath\Quaternion.hpp"
#include <cmath>

using namespace Rig3D;

Transform::Transform() : 
mPosition(0.0, 0.0, 0.0), 
mRotation(0.0, 0.0, 0.0), 
mScale(1.0, 1.0, 1.0),
mParent(nullptr),
mForward(0.0, 0.0, 1.0), 
mUp(0.0, 1.0, 0.0), 
mRight(1.0, 0.0, 0.0)
{
}

Transform::~Transform()
{
}

void Transform::MoveForward()
{
	mPosition.x += mForward.x;
	mPosition.y += mForward.y;
	mPosition.z += mForward.z;
}

void Transform::MoveBackward()
{
	mPosition.x -= mForward.x;
	mPosition.y -= mForward.y;
	mPosition.z -= mForward.z;
}

void Transform::MoveLeft()
{
	mPosition.x -= mRight.x;
	mPosition.y -= mRight.y;
	mPosition.z -= mRight.z;
}

void Transform::MoveRight()
{
	mPosition.x += mRight.x;
	mPosition.y += mRight.y;
	mPosition.z += mRight.z;
}

void Transform::RotatePitch(float pitch)
{
	mRotation.x += pitch;
}

void Transform::RotateYaw(float yaw)
{
	mRotation.y += yaw;
}

void Transform::RotateRoll(float roll)
{
	mRotation.z += roll;
}

mat4f Transform::GetWorldMatrix()
{
	if (!IsDirty())
	{
		return mWorldMatrix;
	}

	mat4f translation	= mat4f::translate(mPosition);
	mat4f rotation		= quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y).toMatrix4();
	mat4f scale			= mat4f::scale(mScale);

	mLocalMatrix = scale * rotation * translation;
	mWorldMatrix = mParent == nullptr ? mLocalMatrix : mLocalMatrix * mParent->GetWorldMatrix();

	return mWorldMatrix;
}

mat3f Transform::GetRotationMatrix()
{
	return quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y).toMatrix3();
}

vec3f Transform::GetForward()
{
	return quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y) * vec3f(0.0, 0.0, 1.0);
}

vec3f Transform::GetUp()
{
	return quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y) * vec3f(0.0, 1.0, 0.0);
}

vec3f Transform::GetRight()
{
	return quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y) * vec3f(1.0, 0.0, 0.0);
}

bool Transform::IsDirty() const
{
	return mIsDirty || (mParent != nullptr && mParent->IsDirty());
}

vec3f Transform::GetPosition() const
{
	return mPosition;
}

vec3f Transform::GetRotation() const
{
	return mRotation;
}

quatf Transform::GetRowPitchYaw() const
{
	return quatf::rollPitchYaw(mRotation.z, mRotation.x, mRotation.y);
}

vec3f Transform::GetScale() const
{
	return mScale;
}

void Transform::SetPosition(const vec3f& position)
{
	mPosition.x = position.x;
	mPosition.y = position.y;
	mPosition.z = position.z;

	mIsDirty = true;
}

void Transform::SetRotation(const vec3f& rotation)
{
	mRotation.x = rotation.x;
	mRotation.y = rotation.y;
	mRotation.z = rotation.z;

	mIsDirty = true;
}

void Transform:: SetRotation(const quatf& rotation)
{
	//vec4f q = { rotation.w, rotation.v.x, rotation.v.y, rotation.v.z };

	//mRotation.x = atan2(2 * (q[0] * q[1] + q[2] * q[3]), 1 - 2 * (q[1] * q[1] + q[2] * q[2]));
	//mRotation.y = asin(2 * (q[0] * q[2] - q[1] * q[3]));
	//mRotation.z = atan2(2 * (q[0] * q[3] + q[1] * q[2]), 1 - 2 * (q[2] * q[2] + q[3] * q[3]));

	auto w = rotation.w, x = rotation.v.x, y = rotation.v.y, z = rotation.v.z;

	auto sp = -2.0f * (y*z - w*x);

	if (fabs(sp) > 0.9999f)
	{
		mRotation.x = 1.570796f * sp;
		mRotation.y = atan2(-x*z + w*y, 0.5f - y*y - z*z);
		mRotation.z = 0.0f;
	}
	else
	{
		mRotation.x = asin(sp);
		mRotation.y = atan2(x*z + w*y, 0.5f - x*x - y*y);
		mRotation.z = atan2(x*y + w*z, 0.5f - x*x - z*z);
	}

	mIsDirty = true;
}

void Transform::SetScale(const vec3f& scale)
{
	mScale.x = scale.x;
	mScale.y = scale.y;
	mScale.z = scale.z;

	mIsDirty = true;
}

void Transform::SetPosition(const float x, const float y, const float z)
{
	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;

	mIsDirty = true;
}

void Transform::SetRotation(const float x, const float y, const float z)
{
	mRotation.x = x;
	mRotation.y = y;
	mRotation.z = z;

	mIsDirty = true;
}

void Transform::SetScale(const float x, const float y, const float z)
{
	mScale.x = x;
	mScale.y = y;
	mScale.z = z;

	mIsDirty = true;
}