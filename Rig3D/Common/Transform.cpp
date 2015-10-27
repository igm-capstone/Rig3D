#include "Transform.h"
#include "GraphicsMath\Quaternion.hpp"
#include <cmath>

using namespace Rig3D;

Transform::Transform() : 
	mWorldMatrix(1.0f),
	mLocalMatrix(1.0f),
	mRotation(1.0f, 0.0f, 0.0f, 0.0f),
	mPosition(0.0f, 0.0f, 0.0f), 
	mScale(1.0f, 1.0f, 1.0f),
	mForward(0.0f, 0.0f, 1.0f),
	mUp(0.0f, 1.0f, 0.0f),
	mRight(1.0f, 0.0f, 0.0f),
	mParent(nullptr),
	mIsDirty(false)
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
	mRotation *= quatf::rollPitchYaw(0.0f, pitch, 0.0f);
	mIsDirty = true;
}

void Transform::RotateYaw(float yaw)
{
	mRotation *= quatf::rollPitchYaw(0.0f, 0.0f, yaw);
	mIsDirty = true;
}

void Transform::RotateRoll(float roll)
{
	mRotation *= quatf::rollPitchYaw(roll, 0.0f, 0.0f);
	mIsDirty = true;
}

mat4f Transform::GetWorldMatrix()
{
	if (!IsDirty())
	{
		return mWorldMatrix;
	}

	mat4f translation	= mat4f::translate(mPosition);
	mat4f rotation		= mRotation.toMatrix4();
	mat4f scale			= mat4f::scale(mScale);

	mLocalMatrix = scale * rotation * translation;
	mWorldMatrix = mParent == nullptr ? mLocalMatrix : mLocalMatrix * mParent->GetWorldMatrix();

	return mWorldMatrix;
}

mat3f Transform::GetRotationMatrix()
{
	return mRotation.toMatrix3();
}

vec3f Transform::GetForward()
{
	return mRotation * vec3f(0.0, 0.0, 1.0);
}

vec3f Transform::GetUp()
{
	return mRotation * vec3f(0.0, 1.0, 0.0);
}

vec3f Transform::GetRight()
{
	return mRotation * vec3f(1.0, 0.0, 0.0);
}

bool Transform::IsDirty() const
{
	return mIsDirty || (mParent != nullptr && mParent->IsDirty());
}

quatf Transform::GetRotation() const
{
	return mRotation;
}

vec3f Transform::GetRollPitchYaw() const
{
	return mRotation.toEuler();
}

vec3f Transform::GetPosition() const
{
	return mPosition;
}

vec3f Transform::GetScale() const
{
	return mScale;
}

void Transform::SetRotation(const quatf& rotation)
{
	mRotation.w = rotation.w;
	mRotation.v.x = rotation.v.x;
	mRotation.v.y = rotation.v.y;
	mRotation.v.z = rotation.v.z;

	mIsDirty = true;
}

void Transform::SetRotation(const vec3f& euler)
{
	mRotation = quatf::rollPitchYaw(euler.z, euler.x, euler.y);

	mIsDirty = true;
}

void Transform::SetPosition(const vec3f& position)
{
	mPosition.x = position.x;
	mPosition.y = position.y;
	mPosition.z = position.z;

	mIsDirty = true;
}

void Transform::SetScale(const vec3f& scale)
{
	mScale.x = scale.x;
	mScale.y = scale.y;
	mScale.z = scale.z;

	mIsDirty = true;
}

void Transform::SetRotation(const float x, const float y, const float z)
{
	mRotation = quatf::rollPitchYaw(z, x, y);

	mIsDirty = true;
}

void Transform::SetPosition(const float x, const float y, const float z)
{
	mPosition.x = x;
	mPosition.y = y;
	mPosition.z = z;

	mIsDirty = true;
}

void Transform::SetScale(const float x, const float y, const float z)
{
	mScale.x = x;
	mScale.y = y;
	mScale.z = z;

	mIsDirty = true;
}