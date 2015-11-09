#pragma once
#include <stdint.h>
#include <vector>
#include <fstream>

extern const uint16_t DOF_POSITION_X;
extern const uint16_t DOF_POSITION_Y;
extern const uint16_t DOF_POSITION_Z;
extern const uint16_t DOF_ROTATION_X;
extern const uint16_t DOF_ROTATION_Y;
extern const uint16_t DOF_ROTATION_Z;

struct BVHJoint
{
	float					Offset[3];
	uint32_t				ChannelOffset;
	uint32_t				ChannelCount;
	BVHJoint*				Parent;
	std::string				Name;
	std::vector<BVHJoint>	Children;
	std::vector<uint16_t>	ChannelOrder;
};

struct BVHHierarchy
{
	BVHJoint	Root;
	uint32_t	ChannelCount;
	uint32_t	JointCount = 1;
};

struct BVHMotion
{
	uint32_t	FrameCount;
	uint32_t	ChannelCount;
	float*		Data;
};

class BVHResource
{
public:
	BVHHierarchy mHierarchy;
	BVHMotion	 mMotion;

	BVHResource(const char* filename);
	BVHResource();
	~BVHResource();

	int Load();

	inline void SetFilename(const char* filename)
	{
		mFilename = filename;
	}

private:
	const char* mFilename;

	void		LoadJoint(std::fstream& file, BVHJoint* joint, BVHJoint* parent = nullptr);
	int			LoadHeirachy(std::fstream& file);
	int			LoadMotion(std::fstream& file);
	void		DeleteJoint(BVHJoint* joint);
};

