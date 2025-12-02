#ifndef ECSTATIC
#define ECSTATIC

#include "../external/rapidhash/rapidhash.h"
#include <immintrin.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#define CHUNK_SIZE 1024

#define ENTITY_MAX UINT32_MAX - 1
#define COMPONENT_MAX UINT16_MAX - 1
#define ARCHETYPE_MAX UINT32_MAX - 1

#define ENTITY_INVALID UINT32_MAX
#define COMPONENT_INVALID UINT16_MAX
#define ARCHETYPE_INVALID UINT32_MAX

typedef void (*ErrorCallback)(const char* caller, const char* fmt);

typedef uint32_t EcstaticEntityId;
typedef uint16_t EcstaticComponentId;
typedef uint32_t EcstaticArchetypeId;

typedef struct EcstaticArchetype {
    void** components;
    uint64_t* componentMask;

    uint32_t* archetypeEntityIdToEntityId;

    uint32_t componentCount;

    uint32_t entityCount;
    uint32_t entityCapacity;

    uint16_t componentMaskCount;
} EcstaticArchetype;

typedef struct EcstaticWorld {
    EcstaticArchetype* archetypes;

    uint32_t* componentSizes;

    uint32_t* entityIdToArchetypeId;
    uint32_t* entityIdToArchetypeEntityId;

    uint32_t** componentMaskToArchetypeBucketArchetypeIds;
    uint16_t* componentMaskToArchetypeBucketArchetypeCounts;
    uint32_t componentMaskToArchetypeBucketCount;

    uint32_t entityCapacity;
    uint32_t archetypeCount;
    uint16_t componentCount;
} EcstaticWorld;

#ifdef __cplusplus
extern "C" {
#endif

void EcstaticDefaultErrorCallback(const char* caller, const char* fmt);
void EcstaticError(const char* caller, const char* fmt, ...);
void EcstaticSetErrorCallback(ErrorCallback errorCallback);

EcstaticWorld* EcstaticCreateWorld(uint32_t initialEntityCapacity, uint32_t componentMaskToArchetypeBucketCount);
void EcstaticDestroyWorld(EcstaticWorld* world);

EcstaticComponentId EcstaticCreateComponent(EcstaticWorld* world, uint64_t componentSize);

EcstaticEntityId EcstaticCreateEntity(EcstaticWorld* world);
void EcstaticUpdateEntityComponents(EcstaticWorld* world, EcstaticEntityId entityId, uint64_t* componentMask, uint16_t componentMaskCount);
void EcstaticAddComponentToEntity(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId);
void EcstaticRemoveComponentFromEntity(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId);
void* EcstaticGetEntityComponent(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId);
void EcstaticDestroyEntity(EcstaticWorld* world, EcstaticEntityId entityId);

uint32_t EcstaticCreateArchetype(EcstaticWorld* world, uint64_t* componentMask, uint16_t componentMaskCount, uint32_t initialEntityCapacity);
uint32_t EcstaticGetArchetypeIdHashFromComponentMask(const EcstaticWorld* world, const uint64_t* componentMask, uint16_t componentMaskCount);
uint32_t EcstaticGetArchetypeIdFromComponentMask(const EcstaticWorld* world, const uint64_t* componentMask, uint16_t componentMaskCount);
uint32_t EcstaticGetArchetypeIdFromEntityId(const EcstaticWorld* world, EcstaticEntityId entityId);
uint32_t EcstaticGetArchetypeEntityIdFromEntityId(const EcstaticWorld* world, EcstaticEntityId entityId);
uint16_t EcstaticGetComponentIdFromArchetypeComponentId(uint64_t* componentMask, uint16_t componentMaskCount, uint16_t archetypeComponentId);
uint16_t EcstaticGetArchetypeComponentIdFromComponentId(uint64_t* componentMask, uint16_t componentMaskCount, uint16_t componentId, bool silence);

uint8_t EcstaticGetNthSetBitIndex(uint64_t x, int n);

#ifdef __cplusplus
}
#endif

#endif