// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#include "asset.hpp"
#include "ddl.hpp"

#include <DirectXMath.h>

namespace rivet_hook::game {
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
	struct Component;

	struct ComponentPointer {
		ComponentInfo* componentType;
		Component* instance;
	};

	static_assert(sizeof(ComponentPointer) == 0x10, "ComponentPointer size is not 0x10");

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
		EngineHandle *children;
		uint16_t childCount;
		uint16_t childCapacity;
		uint32_t unknown5;
		Asset *actorAsset;
		uint64_t unknown6[4];
		ComponentPointer firstComponent;
		ComponentPointer* components;
		uint16_t componentCount;
		uint16_t unknown10;
		uint16_t componentRemain;
		uint16_t unknown11;
		intptr_t unknown12;
		ComponentPointer* componentLookup;
		uint16_t componentLookupCount;
		uint16_t componentLookupCapacity;
		uint32_t unknown13;
		float unknownFloat[2];
		double unknownDouble[2];
		uint64_t unknown14;
		const char *name;
		uint64_t unknown16;

		__forceinline auto
		GetName() const -> const char * {
			if (name && *name) {
				return name;
			}

			if (actorAsset && actorAsset->name && *actorAsset->name) {
				if (actorAsset->nameOffset > 0 && actorAsset->nameOffset < 0x1ff) {
					return &actorAsset->name[actorAsset->nameOffset];
				}

				return actorAsset->name;
			}

			return nullptr;
		}

		__forceinline auto
		HasParent() const -> bool {
			return parentIndex > -1;
		}

		__forceinline auto
		HasChildren() const -> bool {
			return childCount > 0 && childCapacity > 0;
		}

		__forceinline auto
		IsValid() const -> bool {
			return type != 0 && object != nullptr;
		}
	};

	static_assert(sizeof(Actor) == 0xC0, "Actor size is not 0xC0");
	static_assert(offsetof(Actor, actorAsset) == 0x30, "Actor actorAsset offset is not 0x30");
	static_assert(offsetof(Actor, components) == 0x68, "Actor components offset is not 0x68");
	static_assert(offsetof(Actor, componentLookup) == 0x80, "Actor componentLookup offset is not 0x80");
	static_assert(offsetof(Actor, name) == 0xb0, "Actor name offset is not 0xb0");

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

	constexpr int COMPONENT_VTABLE_GET_TYPE_INFO = 0xA;

	struct Component {
		intptr_t* vtable;
		void* ddlPriusData;
		Actor* actor;
		float unknown1;
		EngineHandle handle;
		uint32_t unknown2;
		uint32_t childCount : 8;
		uint32_t flags : 24;
		uint32_t unknown3[0x6];
		uint32_t flags2;
		EngineHandle parentComponent;
	};

	static_assert(sizeof(Component) == 0x48, "Component size is not 0x48");

	struct ActorGroup {
		EngineHandle *handles;
		const char *name;
		uint64_t unknown;
		uint16_t count;
		uint16_t capacity;
		uint16_t flags;
		uint16_t type;
	};

	static_assert(sizeof(ActorGroup) == 0x20, "ActorGroup size is not 0x20");

#pragma pack(pop)
} // namespace rivet_hook::game
