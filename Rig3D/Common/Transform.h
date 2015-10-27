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

		inline quatf GetRotation() const;
		inline vec3f GetRollPitchYaw() const;
		inline vec3f GetPosition() const;
		inline vec3f GetScale() const;

		inline void SetRotation(const quatf& rotation);
		inline void SetRotation(const vec3f& euler);
		inline void SetPosition(const vec3f& position);
		inline void SetScale(const vec3f& scale);

		inline void SetRotation(const float x, const float y, const float z);
		inline void SetPosition(const float x, const float y, const float z);
		inline void SetScale(const float x, const float y, const float z);

	private: 
		mat4f	mWorldMatrix;
		mat4f	mLocalMatrix;

		quatf	mRotation;
		vec3f	mPosition;
		vec3f	mScale;

		vec3f	mForward;
		vec3f	mUp;
		vec3f	mRight;

		Transform*	mParent;
		bool		mIsDirty;
	};
}

