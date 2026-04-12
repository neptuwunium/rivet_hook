// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include "asset.hpp"

#include <DirectXMath.h>

namespace rivet_hook::game::actor {
#pragma pack(push, 1)

	struct EngineHandle {
		union {
			struct {
				uint32_t id : 20;
				uint32_t type : 12;
			};
			uint32_t value;
		};
	};

	static_assert(sizeof(EngineHandle) == 4, "ActorHandle size is not 4");

	struct SceneObject;

	struct Actor {
		SceneObject *object;
		uint16_t type;
		uint16_t unknown1;
		uint32_t sceneIndex;
		uint32_t flags;
		EngineHandle parentHandle;
		uint16_t unknown2[2];
		int16_t parentIndex;
		uint16_t unknown4;
		intptr_t children;
		uint16_t childCount;
		uint16_t childCapacity;
		uint32_t unknown5;
		Asset *actorAsset;
		uint64_t unknown6[4];
		intptr_t firstDDL; // ddl *
		intptr_t ddl; // ddl entry *
		intptr_t unknown11; // ?? probably related to ddl
		uint16_t ddlCount;
		uint16_t unknown12;
		uint16_t ddlFirstFree;
		uint16_t ddlLastIndex;
		intptr_t ddlLastComponent;
		uint64_t unknown13[2];
		float unknownFloat[2];
		double unknownDouble[2];
		uint32_t unknown15[2];
		const char *name;
		uint64_t unknown17;

		__forceinline auto
		get_name() const -> const char * {
			if (name && *name) {
				return name;
			}

			if (actorAsset && actorAsset->name && *actorAsset->name) {
				if (actorAsset->nameOffset > 0 &&  actorAsset->nameOffset < 0x1ff) {
					return &actorAsset->name[actorAsset->nameOffset];
				}

				return actorAsset->name;
			}

			return nullptr;
		}

		__forceinline auto
		has_parent() const -> bool {
			return parentIndex > -1;
		}

		__forceinline auto
		has_child() const -> bool {
			return childCount > 0 && childCapacity > 0;
		}
	};
	static_assert(sizeof(Actor) == 0xC0, "SceneObject size is 0xC0");

	struct alignas(16) SceneObject {
		DirectX::XMMATRIX transform;
		DirectX::XMVECTORF32 unknownVector;
		DirectX::XMFLOAT3 extents;
		uint32_t objectFlags : 24;
		uint32_t objectType : 8;
		uint32_t unknown1;
		EngineHandle sceneHandle;
		EngineHandle actorHandle;
		uint32_t lastModifiedOnFrame;
		DirectX::XMFLOAT3 scale;
		uint32_t createdOnFrame;
	};
	static_assert(sizeof(SceneObject) == 0x80, "SceneObject size is 0x80");
	static_assert(offsetof(SceneObject, unknownVector) == 0x40, "SceneObject unknownVector offset is not 0x40");
	static_assert(offsetof(SceneObject, extents) == 0x50, "SceneObject radius offset is not 0x4C");
	static_assert(offsetof(SceneObject, scale) == 0x70, "SceneObject radius offset is not 0x70");

#pragma pack(pop)
} // namespace rivet_hook::game::actor
