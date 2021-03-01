#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <vector>

#include "Block.h"
#include "Macros.h"
#include "BlockDatabase.h"

namespace Blocks
{
	enum class WorldStructureType
	{
		Trees,
		Cacti,
		Undefined
	};

	struct StructureBlock
	{
		Block block;
		int x, y, z;
	};

	class WorldStructure
	{
	public :

		WorldStructure()
		{
			p_StructureType = WorldStructureType::Undefined;
		}

		~WorldStructure() 
		{
			p_Structure.clear();
		};

		WorldStructureType p_StructureType;
		std::vector<StructureBlock> p_Structure;

	protected : 

		void SetBlock(int x, int y, int z, BlockIDType type) noexcept
		{
			StructureBlock b;

			b.x = x;
			b.y = y;
			b.z = z;
			b.block = { type };
			p_Structure.push_back(b);
		}

		void SetBlocksX(const glm::vec3& position, int breadth, BlockIDType type) noexcept
		{
			for (int x = position.x; x < position.x + breadth; x++)
			{
				SetBlock(x, position.y, position.z, type);
			}
		}

		void SetBlocksZ(const glm::vec3& position, int depth, BlockIDType type) noexcept
		{
			for (int z = position.z; z < position.z + depth; z++)
			{
				SetBlock(position.x, position.y, z, type);
			}
		}

		void SetBlocksY(const glm::vec3& position, int height, BlockIDType type) noexcept
		{
			for (int y = position.y; y < position.y + height; y++)
			{
				SetBlock(position.x, y, position.z, type);
			}
		}

		void SetBlocksHorizontal(const glm::vec3& position, int breadth, int depth, BlockIDType type) noexcept
		{ 
			for (int i = position.x; i < position.x + breadth; i++)
			{
				for (int j = position.z; j < position.z + depth; j++)
				{
					SetBlock(i, position.y, j, type);
				}
			}
		}
	};


	class TreeStructure : public WorldStructure
	{
	public :

		TreeStructure()
		{
			// Define the tree structure

			// Leaves
			SetBlocksHorizontal(glm::vec3(0, 5, 0), 5, 5, BlockDatabase::GetBlockID("oak_leaves"));

			SetBlock(0, 6, 0, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(0, 6, 4, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(4, 6, 0, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(4, 6, 4, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlocksHorizontal(glm::vec3(1, 6, 1), 3, 3, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2, 7, 2, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2 + 1, 7, 2, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2 - 1, 7, 2, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2, 7, 2 + 1, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2, 7, 2 - 1, BlockDatabase::GetBlockID("oak_leaves"));
			SetBlock(2, 8, 2, BlockDatabase::GetBlockID("oak_leaves"));

			// Bark
			SetBlocksY(glm::vec3(2, 0, 2), 8, BlockDatabase::GetBlockID("oak_log"));
		}
	};

	class CactusStructure : public WorldStructure
	{
	public : 
		CactusStructure()
		{
			SetBlocksY(glm::vec3(0, 0, 0), 6, BlockDatabase::GetBlockID("cactus"));
		}
	};
}