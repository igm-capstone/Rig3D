// References: http://www.gamedev.net/page/resources/_/technical/game-programming/bvh-file-loading-and-displaying-r3295

#include "BVHResource.h"
#include <string>
#include <fstream>
#include "StringTrim.h"
#include <sstream>

static const std::string BVHKeyHierarchy	("HIERARCHY");
static const std::string BVHKeyRoot			("ROOT");
static const std::string BVHKeyOffset		("OFFSET");
static const std::string BVHKeyChannels		("CHANNELS");
static const std::string BVHKeyJoint		("JOINT");
static const std::string BVHKeyMotion		("MOTION");
static const std::string BVHKeyPositionX	("Xposition");
static const std::string BVHKeyPositionY	("Yposition");
static const std::string BVHKeyPositionZ	("Zposition");
static const std::string BVHKeyRotationX	("Xrotation");
static const std::string BVHKeyRotationY	("Yrotation");
static const std::string BVHKeyRotationZ	("Zrotation");
static const std::string BVHKeyEndSite		("End");
static const std::string BVHKeyFrameCount	("Frames:");
static const std::string BVHKeyFrame        ("Frame");

static const uint16_t DOF_POSITION_X = 0x01;
static const uint16_t DOF_POSITION_Y = 0x02;
static const uint16_t DOF_POSITION_Z = 0x04;
static const uint16_t DOF_ROTATION_X = 0x10;
static const uint16_t DOF_ROTATION_Y = 0x20;
static const uint16_t DOF_ROTATION_Z = 0x40;

static const char BVHBeginElement	= '{';
static const char BVHEndElement		= '}';

BVHResource::BVHResource(const char* filename) : mFilename(filename)
{

}

BVHResource::BVHResource() : BVHResource(nullptr)
{
	
}

BVHResource::~BVHResource()
{
	DeleteJoint(&mHierarchy.Root);
	delete[] mMotion.Data;
}

int BVHResource::Load()
{
	std::fstream file(mFilename);

	if (!file.is_open())
	{
		return -1;
	}

	std::string line;

	file >> line;

	if (Trim(line) == BVHKeyHierarchy)
	{
		LoadHeirachy(file);
	}

	file.close();

	return 1;
}

void BVHResource::LoadJoint(std::fstream& file, BVHJoint* joint, BVHJoint* parent)
{
	if (parent) 
	{
		joint->Parent = parent;
	}

	// Joint name.
	file >> joint->Name;

	std::string line;

	static int	channelStart	= 0;
	uint32_t channelOrderIndex	= 0;

	while (file.good())
	{
		file >> line;

		line = Trim(line);
	
		char c = line.at(0);
		if (c == 'X' || c == 'Y' || c == 'Z')
		{
			if (line == BVHKeyPositionX)
			{
				joint->ChannelOrder.push_back(DOF_POSITION_X);
			}
			else if (line == BVHKeyPositionY)
			{
				joint->ChannelOrder.push_back(DOF_POSITION_Y);
			}
			else if (line == BVHKeyPositionZ)
			{
				joint->ChannelOrder.push_back(DOF_POSITION_Z);
			}
			else if (line == BVHKeyRotationX)
			{
				joint->ChannelOrder.push_back(DOF_ROTATION_X);
			}
			else if (line == BVHKeyRotationY)
			{
				joint->ChannelOrder.push_back(DOF_ROTATION_Y);
			}
			else if (line == BVHKeyRotationZ)
			{
				joint->ChannelOrder.push_back(DOF_ROTATION_Z);
			}
		}

		if (line == BVHKeyOffset)
		{
			file >> joint->Offset[0] >> joint->Offset[1] >> joint->Offset[2];
		}
		else if (line == BVHKeyChannels)
		{
			file >> joint->ChannelCount;
		
			mMotion.ChannelCount += joint->ChannelCount;

			joint->ChannelOffset = channelStart;
			channelStart += joint->ChannelCount;

			// set up channel order storage for next read	
			joint->ChannelOrder.reserve(joint->ChannelCount);
		}
		else if (line == BVHKeyJoint)
		{
			// Recursively define children
			joint->Children.push_back(BVHJoint());

			LoadJoint(file, &joint->Children.back(), joint);

			mHierarchy.JointCount++;
		}
		else if (line == BVHKeyEndSite)
		{
			file >> line >> line;

			joint->Children.push_back(BVHJoint());

			BVHJoint* leafJoint = &joint->Children.back();
			leafJoint->Parent = joint;
			leafJoint->ChannelCount = 0;
			leafJoint->Name = "End Site";
			
			file >> line;
			if (line == BVHKeyOffset)
			{
				file >> leafJoint->Offset[0] >> leafJoint->Offset[1] >> leafJoint->Offset[2];
			}

			file >> line;

			mHierarchy.JointCount++;
		}
		else if (line == "}")
		{
			break;
		}
	}
}

int	BVHResource::LoadHeirachy(std::fstream& file)
{
	std::string line;

	while (file.good())
	{
		file >> line;

		if (Trim(line) == BVHKeyRoot)
		{
			LoadJoint(file, &mHierarchy.Root);
		}
		else if (Trim(line) == BVHKeyMotion)
		{
			LoadMotion(file);
		}
	}

	return 1;
}

int	BVHResource::LoadMotion(std::fstream& file)
{
	std::string line;

	while(file.good())
	{
		file >> line;

		line = Trim(line);

		if (line == BVHKeyFrameCount)
		{
			file >> mMotion.FrameCount;
		}
		else if (line == BVHKeyFrame)
		{
			file >> line >> mMotion.FrameTime;	// Frame Time: 

			int frameCount		= mMotion.FrameCount;
			int channelCount	= mMotion.ChannelCount;
		
			mMotion.Data = new float[frameCount * channelCount];
			
			for (int frame = 0; frame < frameCount; frame++)
			{
				for (int channel = 0; channel < channelCount; channel++)
				{
					float x;
					std::stringstream ss;
					file >> line;
					ss << line;
					ss >> x;

					mMotion.Data[frame * channelCount + channel] = x;
				}
			}
		}
	}

	return 1;
}

void BVHResource::DeleteJoint(BVHJoint* joint)
{
	if (joint->Children.size() == 0)
	{
		return;
	}


	for (uint32_t i = 0; i < joint->Children.size(); i++)
	{
		DeleteJoint(&joint->Children[i]);
	}

	joint->Children.clear();
}
