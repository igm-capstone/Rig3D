#pragma once
#include "GraphicsMath\cgm.h"

#ifdef _WINDLL
#define RIG3D __declspec(dllexport)
#else
#define RIG3D __declspec(dllimport)
#endif

namespace Rig3D
{
	class RIG3D Transform
	{
	public:
		vec3f		mPosition;
		vec3f		mRotation;
		vec3f		mScale;
		Transform*	mParent;
		bool		mIsDirty;

		Transform();
		~Transform();

		void MoveForward();
		void MoveBackward();
		void MoveRight();
		void MoveLeft();
		void RotatePitch(float pitch);
		void RotateYaw(float yaw);
		void RotateRoll(float roll);

		mat4f GetWorldMatrix();
		mat3f GetRotationMatrix();
		vec3f GetForward();
		vec3f GetUp();
		vec3f GetRight();

		inline bool IsDirty() const;

		inline vec3f GetPosition() const;
		inline vec3f GetRotation() const;
		inline vec3f GetScale() const;
		inline quatf GetRowPitchYaw() const;

		inline void SetPosition(const vec3f& position);
		inline void SetRotation(const vec3f& rotation);
		inline void SetRotation(const quatf& rotation);
		inline void SetScale(const vec3f& scale);

		inline void SetPosition(const float x, const float y, const float z);
		inline void SetRotation(const float x, const float y, const float z);
		inline void SetScale(const float x, const float y, const float z);

	private: // Not used yet.
		mat4f	mWorldMatrix;
		mat4f	mLocalMatrix;
		vec3f	mForward;
		vec3f	mUp;
		vec3f	mRight;
	};
}

