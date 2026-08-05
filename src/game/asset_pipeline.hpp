// SPDX-FileCopyrightText: 2025-2026 Neptuwunium
//
// SPDX-License-Identifier: EUPL-1.2

#pragma once

#define WIN32_LEAN_AND_MEAN
#include <d3d11.h>
#include <dstorage.h>
#include <windows.h>

#include <cstdint>

#include "asset.hpp"

namespace rivet_hook::game {
	constexpr static auto decode_url_string_name = "?DecodeURLString@Library@cohtml@@SAXPEBDIPEADPEAI@Z";

#pragma pack(push, 1)
	enum class AssetFileStatus : uint32_t {
		Closed,
		Pending,
		OpenComplete,
		ReadComplete,
		OperationCanceling,
		OperationCanceled,
		OutOfBounds = 0x80000002,
		DoesNotExist = 0x80000003,
		StatFailed = 0x80000007,
		OpenFailed = 0x80000008,
		ReadFailed = 0x8000000a,
		NotInstalled = 0x8000000b,
		OperationAborted = 0x8000000c,
		BuildFailed = 0x80000014,
	};

	struct ArchiveAsset {
		uint32_t index;
		uint32_t offset;
	};

	static_assert(sizeof(ArchiveAsset) == 8, "ArchiveAsset size mismatch");

	struct FoundAsset {
		uint32_t size;
		ArchiveAsset asset;
		int32_t header;
	};

	static_assert(sizeof(FoundAsset) == 0x10, "FoundAsset size mismatch");

	struct LoadMetadata {
		uint8_t type;
		uint8_t language;
		uint8_t flags;
		uint8_t padding;
		uint32_t index;
	};

	static_assert(sizeof(LoadMetadata) == 8, "LoadMetadata size mismatch");

	struct LoadOperation {
		uint32_t index;
		ArchiveAsset asset;
		uint32_t size;
		uint32_t header;
		uint8_t language;
		uint8_t priority;
		uint16_t padding;
	};

	static_assert(sizeof(LoadOperation) == 0x18, "LoadOperation size mismatch");

	struct DataRange {
		uint8_t *buffer;
		int64_t size;
		int64_t unknown;
	};

	static_assert(sizeof(DataRange) == 0x18, "DataRange size mismatch");

	struct AssetHeader {
		int64_t committedVersion; // set this to zero?
		int64_t completedVersion; // set this to zero?
		int32_t status;			  // 0 is success
		int64_t assetId;		  // from args
		int32_t assetIndex;		  // from LoadMeta
		uint8_t assetType;		  // from LoadMeta
		uint8_t language;		  // important for asset reloading
		uint8_t flags;			  // from LoadMeta
		uint8_t padding;
		int32_t dataRangeCount;
		DataRange *dataRanges; // pointer to data ranges
		DataRange defaultDataRanges[0x4];
		void *customData[0x4]; // stuff from the asset manager, initialized to zero
	};

	static_assert(sizeof(AssetHeader) == 0xb0, "AssetHeader size mismatch");

	struct ArchiveFileSystem {
		void *vtable;
		uint8_t pad1[0x10];
		void *toc;
		uint8_t pad2[0x58];
		uint32_t *mountedTable;
	};

	static_assert(sizeof(ArchiveFileSystem) == 0x80, "ArchiveFileSystem size mismatch");

	struct SortFunc {
		intptr_t func;
		intptr_t target;
	};

	static_assert(sizeof(SortFunc) == 0x10, "SortFunc size mismatch");

	struct AssetFile {
		AssetFileStatus status;
		int32_t padding;
		uint64_t data;
		AssetId asset_id;
		uint64_t size;
	};

	static_assert(sizeof(AssetFile) == 0x20, "AssetFile size mismatch");

	struct MipDataRange {
		uint64_t start;
		uint64_t size;
	};

	static_assert(sizeof(MipDataRange) == 0x10, "MipDataRange size mismatch");

	struct GPUDesc11 {
		ID3D11Resource *resource;
		uint8_t unknown[0x30];
		ID3D12Resource **resource12;
	};

	static_assert(sizeof(GPUDesc11) == 0x40, "GPUDesc12 size mismatch");
	static_assert(offsetof(GPUDesc11, resource12) == 0x38, "GPUDesc12 d3d12 offset mismatch");

	struct NxChunk {
		ID3D12Resource *resource;
		uint64_t mipId;
		uint64_t unk2;
		uint32_t width;
		uint32_t height;
		uint64_t isCompressed;
		uint64_t handle;
		uint64_t offset;
		uint64_t size;
		uint64_t compressionType;
	};

	static_assert(sizeof(NxChunk) == 0x48, "NxChunk size mismatch");

	struct HighMipData {
		uint64_t *destPtr;
		uint64_t queue;
		uint32_t oldMinLod;
		uint32_t fileSize;
		uint32_t numRanges;
		uint32_t unknown;
		MipDataRange memRanges[0x100];
		MipDataRange fileRanges[0x100];
		GPUDesc11 *desc;
		DXGI_FORMAT dxgi_format;
		uint32_t alignment;
		uint32_t width;
		uint32_t height;
		uint32_t arraySize;
		uint32_t mipLevels;
		NxChunk chunk[64];
	};

	static_assert(sizeof(HighMipData) == 0x3240, "HighMipData size mismatch");
	static_assert(offsetof(HighMipData, memRanges) == 0x20, "HighMipData memRanges offset mismatch");
	static_assert(offsetof(HighMipData, fileRanges) == 0x1020, "HighMipData fileRanges offset mismatch");
	static_assert(offsetof(HighMipData, desc) == 0x2020, "HighMipData desc offset mismatch");

	struct NxDStorageWorkerEntry {
		void *buffer;
		void *cursor;
		void *decompressedBuffer;
		uint32_t decompressedSize;
		uint32_t targetSize;
		uint64_t offsetInBuffer;
		NxDStorageWorkerEntry *next;
		HANDLE flushSignal;
		uint64_t field_38;
		uint64_t field_40;
		uint64_t field_48;
		ID3D12Resource *resource;
		int32_t mipIndex;
		D3D12_BOX region;
		bool hasRegion;
		uint8_t padding[3];
	};

	static_assert(sizeof(NxDStorageWorkerEntry) == 0x78, "NxDStorageWorkerEntry size mismatch");

	struct NxDStorageWorkerContext {
		NxDStorageWorkerEntry *buffers;
		int32_t bufferSize;
		int32_t bufferIndex;
		NxDStorageWorkerEntry *last;
		NxDStorageWorkerEntry *first;
		HANDLE updateSignal;
		HANDLE resetSignal;
		CRITICAL_SECTION lock;
		HANDLE flushSignal;
		HANDLE thread;
		intptr_t build_fence;
		const char *name;
		IDStorageQueue *queue;
		HANDLE queueSignal;
	};

	static_assert(sizeof(NxDStorageWorkerContext) == 0x88, "NxDStorageWorkerContext size mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, last) == 0x10, "NxDStorageWorkerContext last offset mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, first) == 0x18, "NxDStorageWorkerContext first offset mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, updateSignal) == 0x20, "NxDStorageWorkerContext updateSignal offset mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, lock) == 0x30, "NxDStorageWorkerContext lock offset mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, flushSignal) == 0x58, "NxDStorageWorkerContext flushSignal offset mismatch");
	static_assert(offsetof(NxDStorageWorkerContext, queue) == 0x78, "NxDStorageWorkerContext queue offset mismatch");

	struct NxDStorage {
		void *dstorageFiles;
		uint64_t unknown1;
		int64_t *handles;
		uint64_t unknown2;
		uint64_t unknown3;
		CRITICAL_SECTION fileLock;
		int64_t *unknown4;
		uint64_t unknown5;
		IDStorageFactory *factory;
		IDStorageQueue *queue;
		uint64_t unknown6;
		IDStorageCustomDecompressionQueue1 *customDecompressionQueue;
		HANDLE dstorageDecompressSignal;
		PTP_WAIT *dstorageDecompressWait;
		PTP_POOL dstoragePool;
		TP_CALLBACK_ENVIRON dstorageDecompressEnv;
		HMODULE DStorageModule;
		intptr_t DStorageGetFactory;
		intptr_t DStorageSetConfiguration1;
		uint32_t unknown7;
		int32_t DStorageThreads;
		ID3D12Device *D3D12Device;
		intptr_t uploadGPUFence;
		LPCRITICAL_SECTION lock;
		int32_t queueCapacity;
	};

	static_assert(sizeof(NxDStorage) == 0x11c, "NxDStorage size mismatch");
	static_assert(offsetof(NxDStorage, factory) == 0x60, "NxDStorage factory offset mismatch");

	struct NxDStorageConfig {
		int32_t bufferSize;
		bool forceBuffering;
		bool disableGPU;
	};

	static_assert(sizeof(NxDStorageConfig) == 6, "DStorageConfig queue offset mismatch");

#pragma pack(pop)

	using create_asset_id_t = AssetId *(*) (AssetId * result, const char *path);
	using is_valid_asset_t = bool (*)(ArchiveFileSystem *self, AssetId asset);
	using open_file_t = void (*)(intptr_t self, AssetFile *file, AssetId asset_id, AssetType type, int32_t platform, uint8_t manager_id);
	using read_file_t = bool (*)(intptr_t self, AssetFile *file, char *buffer, size_t offset, size_t size, int32_t priority, int32_t unknown2);
	using close_file_t = void (*)(intptr_t self, AssetFile *file);
	using resolve_handle_t = int64_t (*)(intptr_t self, AssetId asset_id, AssetType type, int32_t platform, uint8_t manager_id);
	using set_file_status_t = void (*)(AssetFile *file, AssetFileStatus status);
	using decode_url_t = void (*)(const char *, unsigned int, char *, unsigned int *);
	using mgr_load_asset_t = intptr_t (*)(intptr_t, AssetId, AssetId, const char *, intptr_t, intptr_t, int32_t);
	using sort_t = void (*)(intptr_t elems, int32_t count, int32_t element_size, SortFunc dispatcher);
	using mount_archive_t = void (*)(ArchiveFileSystem *self, uint32_t index);
	using commit_assets_t = void (*)(int32_t count);
	using alloc_asset_t = AssetHeader *(*) (uint32_t flags, int32_t result, AssetId asset_id, LoadMetadata *metadata, uint8_t language);
	using resolve_asset_t = FoundAsset *(*) (void *self, AssetId asset_id, AssetLanguage language, AssetType type);
	using set_language_t = void (*)(AssetLanguage language);
	using create_asset_t = bool (*)(AssetHeader *header, const uint8_t *dataHeader, void *globalData);
	using create_mip_t = void (*)(intptr_t self, intptr_t asset, uint32_t lod);
	using create_mip_ng_t = void (*)(intptr_t self);
	using window_init_t = bool (*)(intptr_t self);
	using is_asset_valid_t = bool (*)(uint32_t magic, uint8_t manager_id, AssetId asset_id);
	using nextgen_load_data_t = bool (*)(void *asset, int32_t minLod);
	using init_mips_t = HighMipData *(*) (TextureAsset * asset, HighMipData *data, int32_t numMips);
	using get_storage_link_t = void *(*) (void *arg1, int64_t size, int32_t alignment);
	using create_texture_resource_t = void *(*) (TextureAsset * asset, HighMipData *data);
	using dstorage_flush_queue_t = void* (*)(NxDStorageWorkerContext* context);
	using dstorage_create_context_t = NxDStorageWorkerContext* (*)(NxDStorageWorkerContext* self, void* callback, int32_t bufferSize, const char* name);
	using dstorage_init_t = HRESULT (WINAPI *)(REFIID riid, _COM_Outptr_ void** ppv);
} // namespace rivet_hook::game
