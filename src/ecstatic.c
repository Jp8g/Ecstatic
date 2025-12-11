#include "../include/ecstatic.h"

uint32_t lastEntityId = 0;
ErrorCallback ecstaticErrorCallback = EcstaticDefaultErrorCallback;

void EcstaticDefaultErrorCallback(const char* caller, const char* err) {
    printf("%s CALLER=%s\n", err, caller);
}

void EcstaticError(const char* caller, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);

    va_list argsCopy;
    va_copy(argsCopy, args);

    int len = vsnprintf(NULL, 0, fmt, argsCopy);
    va_end(argsCopy);

    char* buffer = malloc(len + 1);
    if (!buffer) {
        ecstaticErrorCallback(caller, "Failed to allocate memory for char* buffer");
        va_end(args);
        return;
    }

    vsnprintf(buffer, len + 1, fmt, args);
    va_end(args);

    ecstaticErrorCallback(caller, buffer);

    free(buffer);
}

void EcstaticSetErrorCallback(ErrorCallback errorCallback) {
    ecstaticErrorCallback = errorCallback;
}

EcstaticWorld* EcstaticCreateWorld(uint32_t initialEntityCapacity, uint32_t componentMaskToArchetypeBucketCount) {
    EcstaticWorld* newWorld = malloc(sizeof(EcstaticWorld));
    if (!newWorld) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for EcstaticWorld* newWorld", sizeof(EcstaticWorld));
        return NULL;
    }
    
    newWorld->archetypes = NULL;

    newWorld->componentSizes = calloc(1, COMPONENT_MAX * sizeof(uint32_t));
    if (!newWorld->componentSizes) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint32_t* componentSizes", COMPONENT_MAX * sizeof(uint32_t));
        free(newWorld);
        return NULL;
    }

    newWorld->entityIdToArchetypeId = calloc(1, initialEntityCapacity * sizeof(uint32_t));
    if (!newWorld->entityIdToArchetypeId ) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint32_t* entityIdToArchetypeId", initialEntityCapacity * sizeof(uint32_t));
        free(newWorld->componentSizes);
        free(newWorld);
        return NULL;
    }

    newWorld->entityIdToArchetypeEntityId = calloc(1, initialEntityCapacity * sizeof(uint32_t));
    if (!newWorld->entityIdToArchetypeEntityId) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint32_t* entityIdToArchetypeEntityId", initialEntityCapacity * sizeof(uint32_t));
        free(newWorld->entityIdToArchetypeId);
        free(newWorld->componentSizes);
        free(newWorld);
        return NULL;
    }

    newWorld->componentMaskToArchetypeBucketArchetypeIds = calloc(1, componentMaskToArchetypeBucketCount * sizeof(uint32_t*));
    if (!newWorld->componentMaskToArchetypeBucketArchetypeIds) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint32_t** componentMaskToArchetypeBucketArchetypeIds", componentMaskToArchetypeBucketCount * sizeof(uint32_t*));
        free(newWorld->entityIdToArchetypeEntityId);
        free(newWorld->entityIdToArchetypeId);
        free(newWorld->componentSizes);
        free(newWorld);
        return NULL;
    }

    newWorld->componentMaskToArchetypeBucketArchetypeCounts = calloc(1, componentMaskToArchetypeBucketCount * sizeof(uint16_t));
    if (!newWorld->componentMaskToArchetypeBucketArchetypeCounts) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint16_t* componentMaskToArchetypeBucketArchetypeCounts", componentMaskToArchetypeBucketCount * sizeof(uint16_t));
        free(newWorld->componentMaskToArchetypeBucketArchetypeIds);
        free(newWorld->entityIdToArchetypeEntityId);
        free(newWorld->entityIdToArchetypeId);
        free(newWorld->componentSizes);
        free(newWorld);
        return NULL;
    }

    newWorld->componentMaskToArchetypeBucketCount = componentMaskToArchetypeBucketCount;
    newWorld->entityCapacity = initialEntityCapacity;
    newWorld->archetypeCount = 0;
    newWorld->componentCount = 0;

    memset(newWorld->entityIdToArchetypeId, 0xFF, initialEntityCapacity * 4);
    memset(newWorld->entityIdToArchetypeEntityId, 0XFF, initialEntityCapacity * 4);

    return newWorld;
}

void EcstaticDestroyWorld(EcstaticWorld *world) {
    if (!world) {
        EcstaticError(__func__, "World not initialised");
        return;
    }

    if (world->archetypes) {
        for (uint32_t i = 0; i < world->archetypeCount; i++) {
            free(world->archetypes[i].archetypeEntityIdToEntityId);
            if (world->archetypes[i].componentMask) free(world->archetypes[i].componentMask);

            for (uint32_t j = 0; j < world->archetypes[i].componentCount; j++) {
                free(world->archetypes[i].components[j]);
            }

            free(world->archetypes[i].components);
        }

        free(world->archetypes);
    }

    free(world->componentSizes);
    free(world->entityIdToArchetypeId);
    free(world->entityIdToArchetypeEntityId);

    if (world->componentMaskToArchetypeBucketArchetypeIds) {
        for (uint32_t i = 0; i < world->componentMaskToArchetypeBucketCount; i++) {
            free(world->componentMaskToArchetypeBucketArchetypeIds[i]);
        }

        free(world->componentMaskToArchetypeBucketArchetypeIds);
    }

    free(world->componentMaskToArchetypeBucketArchetypeCounts);

    free(world);
}

EcstaticComponentId EcstaticCreateComponent(EcstaticWorld* world, uint64_t componentSize) {
    if (componentSize < 1) {
        EcstaticError(__func__, "Failed to create component: componentSize cannot be less than one");
        return COMPONENT_INVALID;
    }

    world->componentSizes[world->componentCount] = componentSize;
    world->componentCount++;

    return world->componentCount - 1;
}

EcstaticEntityId EcstaticGetNextEntityId() {
    if (lastEntityId >= ENTITY_MAX) return ENTITY_INVALID;

    return lastEntityId++;
}

EcstaticEntityId EcstaticCreateEntity(EcstaticWorld* world) {
    EcstaticEntityId entityId = EcstaticGetNextEntityId();

    if (entityId == ENTITY_INVALID) {
        EcstaticError(__func__, "Out of entity indexes");
        return ENTITY_INVALID;
    }

    uint32_t archetypeId = EcstaticGetArchetypeIdFromComponentMask(world, NULL, 0);
    
    if (archetypeId == ARCHETYPE_INVALID) {
        archetypeId = EcstaticCreateArchetype(world, 0, 1, 1);
    }
    
    EcstaticArchetype* archetype = &world->archetypes[archetypeId];

    if (archetype->entityCount >= archetype->entityCapacity) {
        while (archetype->entityCount >= archetype->entityCapacity) {
            archetype->entityCapacity = archetype->entityCapacity == 0 ? 1 : archetype->entityCapacity * 2;
        }

        void* tmp = realloc(archetype->archetypeEntityIdToEntityId, archetype->entityCapacity * sizeof(uint32_t));
        if (!tmp) {
            EcstaticError(__func__, "Failed to reallocate %zu bytes for uint32_t* archetypeEntityIdToEntityId", archetype->entityCapacity * sizeof(uint32_t));
            return ENTITY_INVALID;
        }
        archetype->archetypeEntityIdToEntityId = tmp;

        for (uint16_t i = 0; i < archetype->componentCount; i++) {
            EcstaticComponentId globalComponentId = EcstaticGetComponentIdFromArchetypeComponentId(archetype->componentMask, archetype->componentMaskCount, i);

            uint32_t componentSize = world->componentSizes[globalComponentId];

            void* tmp = realloc(archetype->components[i], archetype->entityCapacity * componentSize);
            if (!tmp) {
                EcstaticError(__func__, "Failed to reallocate %zu bytes for void* components[i]", archetype->entityCapacity * componentSize);
                return ENTITY_INVALID;
            }
            archetype->components[i] = tmp;
        }
    }

    archetype->archetypeEntityIdToEntityId[archetype->entityCount] = entityId;

    if (entityId >= world->entityCapacity) {
        while (entityId >= world->entityCapacity) {
            world->entityCapacity = world->entityCapacity == 0 ? 1 : world->entityCapacity * 2;
        }

        void* tmp = realloc(world->entityIdToArchetypeId, world->entityCapacity * sizeof(uint32_t));
        if (!tmp) {
            EcstaticError(__func__, "Failed to reallocate %zu bytes for uint32_t* entityIdToArchetypeId", world->entityCapacity * sizeof(uint32_t));
            return ENTITY_INVALID;
        }
        world->entityIdToArchetypeId = tmp;

        void* tmp2 = realloc(world->entityIdToArchetypeEntityId, world->entityCapacity * sizeof(uint32_t));
        if (!tmp2) {
            EcstaticError(__func__, "Failed to reallocate %zu bytes for uint32_t* entityIdToArchetypeEntityId", world->entityCapacity * sizeof(uint32_t));
            return ENTITY_INVALID;
        }
        world->entityIdToArchetypeEntityId = tmp2;
    }

    world->entityIdToArchetypeEntityId[entityId] = archetype->entityCount;
    world->entityIdToArchetypeId[entityId] = archetypeId;
    archetype->entityCount++;

    return entityId;
}

void EcstaticUpdateEntityComponents(EcstaticWorld* world, EcstaticEntityId entityId, uint64_t* componentMask, uint16_t componentMaskCount) {
    uint32_t newArchetypeId = EcstaticGetArchetypeIdFromComponentMask(world, componentMask, componentMaskCount);

    if (newArchetypeId == ARCHETYPE_INVALID) {
        newArchetypeId = EcstaticCreateArchetype(world, componentMask, componentMaskCount, 1);
    }

    EcstaticArchetype* newArchetype = &world->archetypes[newArchetypeId];
    uint32_t newArchetypeEntityId = newArchetype->entityCount;

    if (newArchetypeEntityId >= newArchetype->entityCapacity) {
        while (newArchetypeEntityId >= newArchetype->entityCapacity) {
            newArchetype->entityCapacity = newArchetype->entityCapacity == 0 ? 1 : newArchetype->entityCapacity * 2;
        }

        void* tmp = realloc(newArchetype->archetypeEntityIdToEntityId, newArchetype->entityCapacity * sizeof(uint32_t));
        if (!tmp) {
            EcstaticError(__func__, "Failed to reallocate %zu bytes for uint32_t* archetypeEntityIdToEntityId", newArchetype->entityCapacity * sizeof(uint32_t));
            return;
        }
        newArchetype->archetypeEntityIdToEntityId = tmp;

        for (uint16_t i = 0; i < newArchetype->componentCount; i++) {
            uint16_t globalComponentId = EcstaticGetComponentIdFromArchetypeComponentId(newArchetype->componentMask, newArchetype->componentMaskCount, i);

            uint32_t componentSize = world->componentSizes[globalComponentId];

            void* tmp = realloc(newArchetype->components[i], newArchetype->entityCapacity * componentSize);
            if (!tmp) {
                EcstaticError(__func__, "Failed to reallocate %zu bytes for void* components[i]", newArchetype->entityCapacity * componentSize);
                return;
            }
            newArchetype->components[i] = tmp;
        }
    }

    uint32_t oldArchetypeId = EcstaticGetArchetypeIdFromEntityId(world, entityId);
    uint32_t oldArchetypeEntityId = EcstaticGetArchetypeEntityIdFromEntityId(world, entityId);
    EcstaticArchetype* oldArchetype = &world->archetypes[oldArchetypeId];

    for (uint32_t i = 0; i < oldArchetype->componentCount; i++) {
        EcstaticComponentId componentId = EcstaticGetComponentIdFromArchetypeComponentId(oldArchetype->componentMask, oldArchetype->componentMaskCount, i);
        uint16_t archetypeComponentId = EcstaticGetArchetypeComponentIdFromComponentId(newArchetype->componentMask, newArchetype->componentMaskCount, componentId, true);

        if (componentId != COMPONENT_INVALID && archetypeComponentId != COMPONENT_INVALID) {
            uint32_t componentSize = world->componentSizes[componentId];

            uint8_t* destinationComponent = (uint8_t*)newArchetype->components[archetypeComponentId];
            uint8_t* sourceComponent = (uint8_t*)oldArchetype->components[i];

            memmove(destinationComponent + (newArchetypeEntityId * componentSize), sourceComponent + (oldArchetypeEntityId * componentSize), componentSize);
        }
    }

    newArchetype->archetypeEntityIdToEntityId[newArchetypeEntityId] = entityId;

    world->entityIdToArchetypeId[entityId] = newArchetypeId;
    world->entityIdToArchetypeEntityId[entityId] = newArchetypeEntityId;

    uint32_t lastOldArchetypeEntityId = oldArchetype->entityCount - 1;

    if (oldArchetypeEntityId != lastOldArchetypeEntityId) {
        uint32_t movedEntity = oldArchetype->archetypeEntityIdToEntityId[lastOldArchetypeEntityId];

        oldArchetype->archetypeEntityIdToEntityId[oldArchetypeEntityId] = movedEntity;
        world->entityIdToArchetypeEntityId[movedEntity] = oldArchetypeEntityId;

        for (uint32_t i = 0; i < oldArchetype->componentCount; i++) {
            uint16_t componentId = EcstaticGetComponentIdFromArchetypeComponentId(oldArchetype->componentMask, oldArchetype->componentMaskCount, i);
            uint32_t componentSize = world->componentSizes[componentId];
            memcpy(oldArchetype->components[i] + oldArchetypeEntityId * componentSize, oldArchetype->components[i] + lastOldArchetypeEntityId * componentSize, componentSize);
        }
    }

    for (uint32_t i = 0; i < newArchetype->componentCount; i++) {
        uint16_t componentId = EcstaticGetComponentIdFromArchetypeComponentId(newArchetype->componentMask, newArchetype->componentMaskCount, i);

        if (EcstaticGetArchetypeComponentIdFromComponentId(oldArchetype->componentMask, oldArchetype->componentMaskCount, componentId, true) != COMPONENT_INVALID) continue;

        memset((uint8_t*)newArchetype->components[i] + (size_t)newArchetypeEntityId * world->componentSizes[componentId], 0, world->componentSizes[componentId]);
    }

    oldArchetype->entityCount--;
    newArchetype->entityCount++;

    oldArchetype->archetypeEntityIdToEntityId[lastOldArchetypeEntityId] = ENTITY_INVALID;
}

void EcstaticAddComponentToEntity(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId) {
    if (componentId >= world->componentCount) {
        EcstaticError(__func__, "Invalid component: %hu", componentId);
        return;
    }

    uint32_t oldArchetypeId = EcstaticGetArchetypeIdFromEntityId(world, entityId);
    EcstaticArchetype* oldArchetype = &world->archetypes[oldArchetypeId];
    uint16_t componentMaskIndex = componentId / 64;

    if (componentMaskIndex < oldArchetype->componentMaskCount) {
        if (oldArchetype->componentMask[componentMaskIndex] & (1ULL << (componentId % 64))) {
            EcstaticError(__func__, "Entity %u  already has component %u ", entityId, componentId);
            return;
        }
    }

    uint16_t newComponentMaskCount = componentMaskIndex >= oldArchetype->componentMaskCount ? componentMaskIndex + 1 : oldArchetype->componentMaskCount;
    uint64_t* newComponentMask = calloc(1, newComponentMaskCount * sizeof(uint64_t));
    if (!newComponentMask) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint64_t* newComponentMask", newComponentMaskCount * sizeof(uint64_t));
        return;
    }

    memcpy(newComponentMask, oldArchetype->componentMask, oldArchetype->componentMaskCount * sizeof(uint64_t));
    newComponentMask[componentMaskIndex] |= 1ULL << (componentId % 64);
    
    EcstaticUpdateEntityComponents(world, entityId, newComponentMask, newComponentMaskCount);
    free(newComponentMask);
}

void EcstaticRemoveComponentFromEntity(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId) {
    if (componentId >= world->componentCount) {
        EcstaticError(__func__, "Invalid component: %hu", componentId);
        return;
    }

    uint32_t oldArchetypeId = EcstaticGetArchetypeIdFromEntityId(world, entityId);
    EcstaticArchetype* oldArchetype = &world->archetypes[oldArchetypeId];
    uint16_t componentMaskIndex = componentId / 64;

    if (componentMaskIndex < oldArchetype->componentMaskCount) {
        if (!(oldArchetype->componentMask[componentMaskIndex] & (1ULL << (componentId % 64)))) {
            EcstaticError(__func__, "Entity %u does not have component %u ", entityId, componentId);
            return;
        }
    }

    uint64_t* newComponentMask = calloc(1, oldArchetype->componentMaskCount * sizeof(uint64_t));
    if (!newComponentMask) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint64_t* newComponentMask", oldArchetype->componentMaskCount * sizeof(uint64_t));
        return;
    }

    memcpy(newComponentMask, oldArchetype->componentMask, oldArchetype->componentMaskCount * sizeof(uint64_t));

    newComponentMask[componentMaskIndex] &= ~(1ULL << (componentId % 64));

    uint16_t newComponentMaskCount = oldArchetype->componentMaskCount;
    while (newComponentMaskCount > 0 && newComponentMask[newComponentMaskCount - 1] == 0ULL) newComponentMaskCount--;
    
    EcstaticUpdateEntityComponents(world, entityId, newComponentMask, newComponentMaskCount);
    free(newComponentMask);
}

void* EcstaticGetEntityComponent(EcstaticWorld* world, EcstaticEntityId entityId, EcstaticComponentId componentId) {
    uint32_t archetypeId = EcstaticGetArchetypeIdFromEntityId(world, entityId);
    EcstaticArchetype* archetype = &world->archetypes[archetypeId];
    uint16_t archetypeComponentId = EcstaticGetArchetypeComponentIdFromComponentId(archetype->componentMask, world->archetypes[archetypeId].componentMaskCount, componentId, false);

    if (archetypeComponentId == COMPONENT_INVALID) {
        return NULL;
    }

    uint8_t* archetypeComponent = archetype->components[archetypeComponentId];
    
    uint32_t archetypeEntityId = EcstaticGetArchetypeEntityIdFromEntityId(world, entityId);
    if (archetypeEntityId == ENTITY_INVALID) {
        EcstaticError(__func__, "Invalid entity: %u ", entityId);
        return NULL;
    }

    return (void*)(&archetypeComponent[archetypeEntityId * world->componentSizes[componentId]]);
}

void EcstaticDestroyEntity(EcstaticWorld* world, EcstaticEntityId entityId) {
    if (entityId >= world->entityCapacity) {
        EcstaticError(__func__, "Invalid entity: %u ", entityId);
        return;
    }

    uint32_t archetypeId = EcstaticGetArchetypeIdFromEntityId(world, entityId);
    if (archetypeId == ARCHETYPE_INVALID) {
        EcstaticError(__func__, "Invalid entity: %u ", entityId);
        return;
    }

    EcstaticArchetype* archetype = &world->archetypes[archetypeId];
    uint32_t archetypeEntityId = EcstaticGetArchetypeEntityIdFromEntityId(world, entityId);
    if (archetypeEntityId == ENTITY_INVALID) {
        EcstaticError(__func__, "Invalid entity: %u ", entityId);
        return;
    }

    uint32_t lastArchetypeEntityId = archetype->entityCount - 1;

    if (lastArchetypeEntityId != archetypeEntityId) {
        uint32_t lastArchetypeEntityIdGlobal = archetype->archetypeEntityIdToEntityId[lastArchetypeEntityId];
        archetype->archetypeEntityIdToEntityId[archetypeEntityId] = lastArchetypeEntityIdGlobal;
        world->entityIdToArchetypeEntityId[lastArchetypeEntityIdGlobal] = archetypeEntityId;

        for (uint32_t i = 0; i < archetype->componentCount; i++) {
            uint32_t componentSize = world->componentSizes[EcstaticGetComponentIdFromArchetypeComponentId(archetype->componentMask, archetype->componentMaskCount, i)];
            uint8_t* component = archetype->components[i];

            memmove(component + (archetypeEntityId * componentSize), component + (lastArchetypeEntityId * componentSize), componentSize);
        }
    }

    archetype->entityCount--;

    world->entityIdToArchetypeId[entityId] = ARCHETYPE_INVALID;
    world->entityIdToArchetypeEntityId[entityId] = ENTITY_INVALID;
    archetype->archetypeEntityIdToEntityId[lastArchetypeEntityId] = ENTITY_INVALID;
}

uint32_t EcstaticCreateArchetype(EcstaticWorld* world, uint64_t* componentMask, uint16_t componentMaskCount, uint32_t initialEntityCapacity) {
    world->archetypeCount++;

    void* tmp = realloc(world->archetypes, world->archetypeCount * sizeof(EcstaticArchetype));
    if (!tmp) {
        EcstaticError(__func__, "Failed to reallocate %zu bytes for EcstaticArchetype* archetypes", world->archetypeCount * sizeof(EcstaticArchetype));
        return ARCHETYPE_INVALID;
    }
    EcstaticArchetype* archetypes = tmp;

    world->archetypes = archetypes;

    EcstaticArchetype* archetype = &archetypes[world->archetypeCount - 1];

    archetype->componentMask = calloc(1, (componentMaskCount == 0 ? 1 : componentMaskCount) * sizeof(uint64_t));

    if (componentMaskCount > 0) {
        if (!archetype->componentMask) {
            EcstaticError(__func__, "Failed to allocate %zu bytes for uint64_t* componentMask", componentMaskCount * sizeof(uint64_t));
            return ARCHETYPE_INVALID;
        }

        if (componentMask) memcpy(archetype->componentMask, componentMask, componentMaskCount * sizeof(uint64_t));
    }

    archetype->componentMaskCount = componentMaskCount;

    archetype->archetypeEntityIdToEntityId = malloc(initialEntityCapacity * sizeof(uint32_t));
    if (!archetype->archetypeEntityIdToEntityId) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for uint32_t* archetypeEntityIdToEntityId", initialEntityCapacity * sizeof(uint32_t));
        free(archetype->componentMask);
        return ARCHETYPE_INVALID;
    }

    for (uint32_t i = 0; i < initialEntityCapacity; i++) {
        archetype->archetypeEntityIdToEntityId[i] = ENTITY_INVALID;
    }

    uint32_t componentCount = 0;

    for (uint16_t i = 0; i < componentMaskCount; i++) {
        componentCount += __builtin_popcountll(archetype->componentMask[i]);
    }

    archetype->componentCount = componentCount;

    archetype->entityCapacity = initialEntityCapacity;

    archetype->entityCount = 0;

    archetype->components = calloc(1, componentCount * sizeof(void*));
    if (!archetype->components) {
        EcstaticError(__func__, "Failed to allocate %zu bytes for void** components", componentCount * sizeof(void*));
        free(archetype->archetypeEntityIdToEntityId);
        free(archetype->componentMask);
        return ARCHETYPE_INVALID;
    }

    for (uint16_t i = 0; i < componentCount; i++) {
        uint16_t globalComponentId = EcstaticGetComponentIdFromArchetypeComponentId(archetype->componentMask, componentMaskCount, i);

        uint32_t size = initialEntityCapacity * world->componentSizes[globalComponentId];

        archetype->components[i] = malloc(size);
        if (!archetype->components[i]) {
            EcstaticError(__func__, "Failed to allocate %zu bytes for void* components[i]", size);

            for (uint16_t j = 0; j < i; j++) {
                free(archetype->components[j]);
            }

            free(archetype->components);
            free(archetype->archetypeEntityIdToEntityId);
            free(archetype->componentMask);
            return ARCHETYPE_INVALID;
        }
    }

    uint32_t hash = EcstaticGetArchetypeIdHashFromComponentMask(world, archetype->componentMask, componentMaskCount);

    world->componentMaskToArchetypeBucketArchetypeCounts[hash]++;

    void* tmp2 = realloc(world->componentMaskToArchetypeBucketArchetypeIds[hash], world->componentMaskToArchetypeBucketArchetypeCounts[hash] * sizeof(uint32_t));
    if (!tmp2) {
        EcstaticError(__func__, "Failed to reallocate %zu bytes for uint32_t* componentMaskToArchetypeBucketArchetypeIds[hash]", world->componentMaskToArchetypeBucketArchetypeCounts[hash] * sizeof(uint32_t));
        for (uint16_t i = 0; i < componentCount; i++) {
            free(archetype->components[i]);
        }

        free(archetype->components);
        free(archetype->archetypeEntityIdToEntityId);
        free(archetype->componentMask);
        return ARCHETYPE_INVALID;
    }
    world->componentMaskToArchetypeBucketArchetypeIds[hash] = tmp2;

    world->componentMaskToArchetypeBucketArchetypeIds[hash][world->componentMaskToArchetypeBucketArchetypeCounts[hash] - 1] = world->archetypeCount - 1;

    return world->archetypeCount - 1;
}

uint32_t EcstaticGetArchetypeIdHashFromComponentMask(const EcstaticWorld* world, const uint64_t* componentMask, uint16_t componentMaskCount) {
    return rapidhash(componentMask, componentMaskCount * sizeof(uint64_t)) % world->componentMaskToArchetypeBucketCount;
}

uint32_t EcstaticGetArchetypeIdFromComponentMask(const EcstaticWorld* world, const uint64_t* componentMask, uint16_t componentMaskCount) {
    uint32_t hash = EcstaticGetArchetypeIdHashFromComponentMask(world, componentMask, componentMaskCount);

    uint32_t* hashBucketIds = world->componentMaskToArchetypeBucketArchetypeIds[hash];
    uint16_t hashBucketCounts = world->componentMaskToArchetypeBucketArchetypeCounts[hash];

    for (uint16_t i = 0; i < hashBucketCounts; i++) {
        if (world->archetypes[hashBucketIds[i]].componentMaskCount != componentMaskCount) {
            continue;
        }

        uint64_t* iComponentMask = world->archetypes[hashBucketIds[i]].componentMask;

        for (uint16_t j = 0; j < componentMaskCount; j++) {
            if (iComponentMask[j] != componentMask[j]) {
                goto continueOuter;
            }
        }

        return hashBucketIds[i];

        continueOuter:;
    }

    return ARCHETYPE_INVALID;
}

uint32_t EcstaticGetArchetypeIdFromEntityId(const EcstaticWorld* world, EcstaticEntityId entityId) {
    return entityId >= world->entityCapacity ? ARCHETYPE_INVALID : world->entityIdToArchetypeId[entityId];
}

uint32_t EcstaticGetArchetypeEntityIdFromEntityId(const EcstaticWorld* world, EcstaticEntityId entityId) {
    return entityId >= world->entityCapacity ? ENTITY_INVALID : world->entityIdToArchetypeEntityId[entityId];
}

uint16_t EcstaticGetComponentIdFromArchetypeComponentId(uint64_t* componentMask, uint16_t componentMaskCount, uint16_t archetypeComponentId) {
    uint16_t setBitsSeen = 0;

    for (uint16_t j = 0; j < componentMaskCount; j++) {
        uint8_t bitsInMask = __builtin_popcountll(componentMask[j]);

        if (setBitsSeen + bitsInMask > archetypeComponentId) {
            uint16_t nthInMask = archetypeComponentId - setBitsSeen;
            return j * 64 + EcstaticGetNthSetBitIndex(componentMask[j], nthInMask);
        }

        setBitsSeen += bitsInMask;
    }

    EcstaticError(__func__, "Invalid component");
    return COMPONENT_INVALID;
}


uint16_t EcstaticGetArchetypeComponentIdFromComponentId(uint64_t* componentMask, uint16_t componentMaskCount, uint16_t componentId, bool silence) {
    uint16_t componentMaskIndex = componentId / 64;
    if (componentMaskIndex >= componentMaskCount) {
        if (!silence) EcstaticError(__func__, "Invalid component: %u ", componentId);
        return COMPONENT_INVALID;
    }
    
    uint64_t componentMaskBits = componentMask[componentMaskIndex];
    uint64_t componentMaskBit = 1ULL << (componentId % 64);
    if (!(componentMaskBits & componentMaskBit)) {
        if (!silence) EcstaticError(__func__, "Invalid component: %u ", componentId);
        return COMPONENT_INVALID;
    }
    
    uint16_t index = 0;

    for (uint16_t i = 0; i < componentMaskIndex; i++) {
        index += __builtin_popcountll(componentMask[i]);
    }

    uint64_t lowerBits = componentMaskBits & (componentMaskBit - 1);

    return index + __builtin_popcountll(lowerBits);
}

uint8_t EcstaticGetNthSetBitIndex(uint64_t x, int n) {
    while (n--) x &= x - 1;
    return x ? __builtin_ctzll(x) : UINT8_MAX;
}