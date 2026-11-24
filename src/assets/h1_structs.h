#pragma once

// H1 (MWR) struct layouts as used by the x64-zt H1 dumper (github.com/Joelrau/x64-zt,
// commit 3802db4, the same revision as the zonetool build in refs/tools/x64-zt).
// Only the fields the readers need are named; raw sizes are asserted so the tagged
// asset stream in .xmb/.xsb/.h1Image stays in sync.

#include <cstdint>

namespace wb::assets::h1 {

using scr_string_t = std::int32_t;

struct Bounds {
	float midPoint[3];
	float halfSize[3];
};

struct XModelAngle {
	std::int16_t x;
	std::int16_t y;
	std::int16_t z;
	std::int16_t base;
};

struct XModelTagPos {
	float x;
	float y;
	float z;
};

struct DObjAnimMat {
	float quat[4];
	float trans[3];
	float transWeight;
};

struct ReactiveMotionModelPart {
	float center[3];
	float stiffness;
};

struct ReactiveMotionModelTweaks {
	float scale[4];
};

struct XModelCollSurf {
	Bounds bounds;
	std::int32_t boneIdx;
	std::int32_t contents;
	std::int32_t surfFlags;
};

struct XBoneInfo {
	Bounds bounds;
	float radiusSquared;
};

struct BlendShapeWeightMap {
	std::uint16_t weightIndex;
	std::uint16_t targetIndex;
	float fullWeight;
};

struct MdaoVolume {
	float mins[3];
	float maxs[3];
	std::uint16_t cellCount[3];
	std::uint16_t parentBoneIndex;
	std::uint64_t volumeData;
};

struct XPhysBoneInfo {
	std::uint64_t physPreset;
	std::uint64_t physConstraint;
	std::uint64_t physCollmap;
	float unk1;
	std::int8_t boneIndex;
	std::int8_t unk2;
	char pad[2];
};

struct XModelLodInfo {
	float dist;
	std::uint16_t numsurfs;
	std::uint16_t surfIndex;
	std::uint64_t modelSurfs;
	std::int32_t partBits[8];
	std::uint64_t surfs;
	std::int32_t subdivLodValidMask;
	std::int8_t flags;
	char pad[3];
};

static_assert(sizeof(Bounds) == 24);
static_assert(sizeof(XModelAngle) == 8);
static_assert(sizeof(XModelTagPos) == 12);
static_assert(sizeof(DObjAnimMat) == 32);
static_assert(sizeof(ReactiveMotionModelPart) == 16);
static_assert(sizeof(ReactiveMotionModelTweaks) == 16);
static_assert(sizeof(XModelCollSurf) == 36);
static_assert(sizeof(XBoneInfo) == 28);
static_assert(sizeof(BlendShapeWeightMap) == 8);
static_assert(sizeof(MdaoVolume) == 40);
static_assert(sizeof(XPhysBoneInfo) == 32);
static_assert(sizeof(XModelLodInfo) == 64);

struct XModelRaw {
	std::uint64_t name;                                     // 0
	std::uint8_t numBones;                                  // 8
	std::uint8_t numRootBones;                              // 9
	std::uint8_t numsurfs;                                  // 10
	std::uint8_t numReactiveMotionParts;                     // 11
	std::uint8_t lodRampType;                               // 12
	std::uint8_t numBonePhysics;                            // 13
	char pad0[2];                                           // 14
	float scale;                                            // 16
	std::uint32_t noScalePartBits[8];                       // 20
	std::uint32_t pad1;                                     // 52
	std::uint64_t boneNames;                                // 56
	std::uint64_t parentList;                               // 64
	std::uint64_t tagAngles;                                // 72
	std::uint64_t tagPositions;                             // 80
	std::uint64_t partClassification;                       // 88
	std::uint64_t baseMat;                                  // 96
	std::uint64_t reactiveMotionParts;                      // 104
	std::uint64_t reactiveMotionTweaks;                     // 112
	std::uint64_t materialHandles;                          // 120
	XModelLodInfo lodInfo[6];                               // 128
	std::int8_t numLods;                                    // 512
	std::int8_t collLod;                                    // 513
	std::uint8_t numCompositeModels;                        // 514
	std::uint8_t reactiveMotionLOD;                         // 515
	std::uint16_t flags;                                    // 516
	std::uint16_t numCollSurfs;                             // 518
	std::uint64_t collSurfs;                                // 520
	std::int32_t contents;                                  // 528
	std::uint32_t pad2;                                     // 532
	std::uint64_t boneInfo;                                 // 536
	float radius;                                           // 544
	Bounds bounds;                                          // 548
	std::uint32_t pad3;                                     // 572
	std::uint64_t invHighMipRadius;                         // 576
	std::int32_t memUsage;                                  // 584
	std::uint8_t bad;                                       // 588
	char pad4;                                              // 589
	std::uint16_t blendShapeCount;                          // 590
	std::uint16_t numberOfBlendShapeWeights;                 // 592
	std::uint16_t numberOfBlendShapeWeightMaps;              // 594
	char pad5[4];                                           // 596
	std::uint64_t blendShapeWeightNames;                    // 600
	std::uint64_t blendShapeWeightMaps;                     // 608
	std::uint64_t physPreset;                               // 616
	std::uint64_t physCollmap;                              // 624
	std::uint16_t mdaoVolumeCount;                          // 632
	std::int16_t u3;                                        // 634
	float quantization;                                     // 636
	std::uint64_t mdaoVolumes;                              // 640
	float subdivRadius;                                     // 648
	std::int32_t u5;                                        // 652
	std::uint64_t skeletonScript;                           // 656
	std::uint64_t compositeModels;                          // 664
	std::uint64_t bonePhysics;                              // 672
};

static_assert(sizeof(XModelRaw) == 0x2A8);

struct GfxPackedVertex {
	float xyz[3];
	float binormalSign;
	std::uint8_t color[4];
	std::uint32_t texCoord;
	std::uint32_t normal;
	std::uint32_t tangent;
};

static_assert(sizeof(GfxPackedVertex) == 32);

struct Face {
	std::uint16_t v1;
	std::uint16_t v2;
	std::uint16_t v3;
};

static_assert(sizeof(Face) == 6);

struct XRigidVertList {
	std::uint16_t boneOffset;
	std::uint16_t vertCount;
	std::uint16_t triOffset;
	std::uint16_t triCount;
	std::uint64_t collisionTree;
};

static_assert(sizeof(XRigidVertList) == 16);

struct XSurfaceCollisionTree {
	float trans[3];
	float scale[3];
	std::uint32_t nodeCount;
	std::uint64_t nodes;
	std::uint32_t leafCount;
	std::uint32_t pad;
	std::uint64_t leafs;
};

static_assert(sizeof(XSurfaceCollisionTree) == 56);

struct XSurfaceCollisionNode {
	std::uint16_t mins[3];
	std::uint16_t maxs[3];
	std::uint16_t childBeginIndex;
	std::uint16_t childCount;
};

static_assert(sizeof(XSurfaceCollisionNode) == 16);

struct XSurfaceSubdivInfoRaw {
	std::uint64_t levels;
	std::int32_t flags;
	std::int32_t totalVertCount;
	std::int32_t totalRegularPatchCount;
	std::int32_t unk1;
	std::int32_t unk2;
	std::int32_t unk3;
	std::uint32_t cacheSize;
	std::uint32_t pad;
	std::uint64_t cacheBuffer;
	std::uint64_t cacheView;
};

static_assert(sizeof(XSurfaceSubdivInfoRaw) == 56);

struct XSurfaceSubdivLevelRaw {
	std::uint8_t bytes[232];
};

static_assert(sizeof(XSurfaceSubdivLevelRaw) == 232);

struct XSurfaceRaw {
	std::uint16_t flags;                                    // 0
	std::uint16_t vertCount;                                // 2
	std::uint16_t triCount;                                 // 4
	std::uint8_t rigidVertListCount;                        // 6
	std::uint8_t subdivLevelCount;                          // 7
	std::int16_t blendVertCounts[8];                        // 8
	std::uint64_t verts0;                                   // 24
	std::uint64_t triIndices;                               // 32
	std::uint64_t triIndices2;                              // 40
	std::uint64_t vb0;                                      // 48
	std::uint64_t vb0View;                                  // 56
	std::uint64_t indexBuffer;                              // 64
	std::uint64_t rigidVertLists;                           // 72
	std::uint64_t unknown0;                                 // 80
	std::uint64_t blendVerts;                               // 88
	std::uint64_t blendVertsTable;                          // 96
	std::uint64_t blendVertsBuffer;                         // 104
	std::uint64_t blendVertsView;                           // 112
	std::uint64_t lmapUnwrap;                               // 120
	std::uint64_t vblmapBuffer;                             // 128
	std::uint64_t vblmapView;                               // 136
	std::uint64_t subdiv;                                   // 144
	std::uint64_t tensionData;                              // 152
	std::uint64_t tensionAccumTable;                        // 160
	std::uint64_t tensionAccumTableBuffer;                  // 168
	std::uint64_t tensionAccumTableView;                    // 176
	std::uint64_t tensionDataBuffer;                        // 184
	std::uint64_t tensionDataView;                          // 192
	std::uint64_t indexBufferView;                          // 200
	std::uint64_t blendShapes;                              // 208
	std::uint32_t blendShapesCount;                         // 216
	char pad0[4];                                           // 220
	std::uint32_t vertexLightingIndex;                      // 224
	std::int32_t partBits[8];                               // 228
	char pad1[4];                                           // 260
};

static_assert(sizeof(XSurfaceRaw) == 0x108);

struct XModelSurfsRaw {
	std::uint64_t name;
	std::uint64_t surfs;
	std::uint16_t numsurfs;
	char pad[6];
	std::int32_t partBits[8];
};

static_assert(sizeof(XModelSurfsRaw) == 0x38);

struct BlendShape {
	std::uint32_t vertCount;
	std::uint32_t pad;
	std::uint64_t verts;
	std::uint64_t buffer;
	std::uint64_t view;
};

static_assert(sizeof(BlendShape) == 32);

struct BlendShapeVert {
	float positionOffset[3];
	std::int32_t vertIndex;
	std::uint32_t normalOffset;
	std::uint32_t tangentOffset;
	float tensionS;
	float tensionT;
};

static_assert(sizeof(BlendShapeVert) == 32);

struct GfxImageStreamData {
	std::uint16_t width;
	std::uint16_t height;
	std::int32_t levelCountAndSize;
};

static_assert(sizeof(GfxImageStreamData) == 8);

struct GfxImageRaw {
	std::uint64_t texture[3];                               // 0
	std::int32_t imageFormat;                               // 24
	std::uint8_t mapType;                                   // 28
	std::uint8_t semantic;                                  // 29
	std::uint8_t category;                                  // 30
	std::uint8_t flags;                                     // 31
	std::uint8_t picmip[2];                                 // 32
	std::uint8_t cardMemory[2];                             // 34
	std::uint32_t dataLen1;                                 // 36
	std::uint32_t dataLen2;                                 // 40
	std::uint16_t width;                                    // 44
	std::uint16_t height;                                   // 46
	std::uint16_t depth;                                    // 48
	std::uint16_t numElements;                              // 50
	std::uint8_t levelCount;                                // 52
	std::uint8_t streamed;                                  // 53
	char pad[2];                                            // 54
	std::uint64_t pixelData;                                // 56
	GfxImageStreamData streams[4];                          // 64
	std::uint64_t name;                                     // 96
};

static_assert(sizeof(GfxImageRaw) == 0x68);

} // namespace wb::assets::h1
