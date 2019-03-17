/********************************************************************************
* ReactPhysics3D physics library, http://www.reactphysics3d.com                 *
* Copyright (c) 2010-2018 Daniel Chappuis                                       *
*********************************************************************************
*                                                                               *
* This software is provided 'as-is', without any express or implied warranty.   *
* In no event will the authors be held liable for any damages arising from the  *
* use of this software.                                                         *
*                                                                               *
* Permission is granted to anyone to use this software for any purpose,         *
* including commercial applications, and to alter it and redistribute it        *
* freely, subject to the following restrictions:                                *
*                                                                               *
* 1. The origin of this software must not be misrepresented; you must not claim *
*    that you wrote the original software. If you use this software in a        *
*    product, an acknowledgment in the product documentation would be           *
*    appreciated but is not required.                                           *
*                                                                               *
* 2. Altered source versions must be plainly marked as such, and must not be    *
*    misrepresented as being the original software.                             *
*                                                                               *
* 3. This notice may not be removed or altered from any source distribution.    *
*                                                                               *
********************************************************************************/

// Libraries
#include "BroadPhaseSystem.h"
#include "collision/CollisionDetection.h"
#include "utils/Profiler.h"
#include "collision/RaycastInfo.h"
#include "memory/MemoryManager.h"
#include "engine/CollisionWorld.h"

// We want to use the ReactPhysics3D namespace
using namespace reactphysics3d;

// Constructor
BroadPhaseSystem::BroadPhaseSystem(CollisionDetection& collisionDetection, ProxyShapeComponents& proxyShapesComponents,
                                   TransformComponents& transformComponents)
                    :mDynamicAABBTree(collisionDetection.getMemoryManager().getPoolAllocator(), DYNAMIC_TREE_AABB_GAP),
                     mProxyShapesComponents(proxyShapesComponents), mTransformsComponents(transformComponents),
                     mMovedShapes(collisionDetection.getMemoryManager().getPoolAllocator()),
                     mPotentialPairs(collisionDetection.getMemoryManager().getPoolAllocator()),
                     mCollisionDetection(collisionDetection) {

#ifdef IS_PROFILING_ACTIVE

	mProfiler = nullptr;

#endif

}

// Return true if the two broad-phase collision shapes are overlapping
bool BroadPhaseSystem::testOverlappingShapes(const ProxyShape* shape1,
                                                       const ProxyShape* shape2) const {

    if (shape1->getBroadPhaseId() == -1 || shape2->getBroadPhaseId() == -1) return false;

    // Get the two AABBs of the collision shapes
    const AABB& aabb1 = mDynamicAABBTree.getFatAABB(shape1->getBroadPhaseId());
    const AABB& aabb2 = mDynamicAABBTree.getFatAABB(shape2->getBroadPhaseId());

    // Check if the two AABBs are overlapping
    return aabb1.testCollision(aabb2);
}

// Ray casting method
void BroadPhaseSystem::raycast(const Ray& ray, RaycastTest& raycastTest,
                                         unsigned short raycastWithCategoryMaskBits) const {

    RP3D_PROFILE("BroadPhaseAlgorithm::raycast()", mProfiler);

    BroadPhaseRaycastCallback broadPhaseRaycastCallback(mDynamicAABBTree, raycastWithCategoryMaskBits, raycastTest);

    mDynamicAABBTree.raycast(ray, broadPhaseRaycastCallback);
}

// Add a proxy collision shape into the broad-phase collision detection
void BroadPhaseSystem::addProxyCollisionShape(ProxyShape* proxyShape, const AABB& aabb) {

    assert(proxyShape->getBroadPhaseId() == -1);

    // Add the collision shape into the dynamic AABB tree and get its broad-phase ID
    int nodeId = mDynamicAABBTree.addObject(aabb, proxyShape);

    // Set the broad-phase ID of the proxy shape
    mProxyShapesComponents.setBroadPhaseId(proxyShape->getEntity(), nodeId);

    // Add the collision shape into the array of bodies that have moved (or have been created)
    // during the last simulation step
    addMovedCollisionShape(proxyShape->getBroadPhaseId());
}

// Remove a proxy collision shape from the broad-phase collision detection
void BroadPhaseSystem::removeProxyCollisionShape(ProxyShape* proxyShape) {

    assert(proxyShape->getBroadPhaseId() != -1);

    int broadPhaseID = proxyShape->getBroadPhaseId();

    mProxyShapesComponents.setBroadPhaseId(proxyShape->getEntity(), -1);

    // Remove the collision shape from the dynamic AABB tree
    mDynamicAABBTree.removeObject(broadPhaseID);

    // Remove the collision shape into the array of shapes that have moved (or have been created)
    // during the last simulation step
    removeMovedCollisionShape(broadPhaseID);
}

// Update the broad-phase state of a single proxy-shape
void BroadPhaseSystem::updateProxyShape(Entity proxyShapeEntity) {

    assert(mProxyShapesComponents.mMapEntityToComponentIndex.containsKey(proxyShapeEntity));

    // Get the index of the proxy-shape component in the array
    uint32 index = mProxyShapesComponents.mMapEntityToComponentIndex[proxyShapeEntity];

    // Update the proxy-shape component
    updateProxyShapesComponents(index, index + 1);
}

// Update the broad-phase state of all the enabled proxy-shapes
void BroadPhaseSystem::updateProxyShapes() {

    // Update all the enabled proxy-shape components
    updateProxyShapesComponents(0, mProxyShapesComponents.getNbEnabledComponents());
}

// Notify the broad-phase that a collision shape has moved and need to be updated
void BroadPhaseSystem::updateProxyShapeInternal(int broadPhaseId, const AABB& aabb, const Vector3& displacement) {

    assert(broadPhaseId >= 0);

    // Update the dynamic AABB tree according to the movement of the collision shape
    bool hasBeenReInserted = mDynamicAABBTree.updateObject(broadPhaseId, aabb, displacement);

    // If the collision shape has moved out of its fat AABB (and therefore has been reinserted
    // into the tree).
    if (hasBeenReInserted) {

        // Add the collision shape into the array of shapes that have moved (or have been created)
        // during the last simulation step
        addMovedCollisionShape(broadPhaseId);
    }
}

// Update the broad-phase state of some proxy-shapes components
void BroadPhaseSystem::updateProxyShapesComponents(uint32 startIndex, uint32 endIndex) {

    assert(startIndex <= endIndex);
    assert(startIndex < mProxyShapesComponents.getNbComponents());
    assert(endIndex <= mProxyShapesComponents.getNbComponents());

    // Make sure we do not update disabled components
    startIndex = std::min(startIndex, mProxyShapesComponents.getNbEnabledComponents());
    endIndex = std::min(endIndex, mProxyShapesComponents.getNbEnabledComponents());

    // For each proxy-shape component to update
    for (uint32 i = startIndex; i < endIndex; i++) {

        const int broadPhaseId = mProxyShapesComponents.mBroadPhaseIds[i];
        if (broadPhaseId != -1) {

            const Entity& bodyEntity = mProxyShapesComponents.mBodiesEntities[i];
            const Transform& transform = mTransformsComponents.getTransform(bodyEntity);

            // TODO : Use body linear velocity and compute displacement
            const Vector3 displacement = Vector3::zero();
            //const Vector3 displacement = world.mTimeStep * mLinearVelocity;

            // Recompute the world-space AABB of the collision shape
            AABB aabb;
            mProxyShapesComponents.mCollisionShapes[i]->computeAABB(aabb, transform * mProxyShapesComponents.mLocalToBodyTransforms[i]);

            // Update the broad-phase state for the proxy collision shape
            updateProxyShapeInternal(broadPhaseId, aabb, displacement);
        }
    }
}

void BroadPhaseSystem::reportAllShapesOverlappingWithAABB(const AABB& aabb,
                                                             LinkedList<int>& overlappingNodes) const {

    AABBOverlapCallback callback(overlappingNodes);

    // Ask the dynamic AABB tree to report all collision shapes that overlap with this AABB
    mDynamicAABBTree.reportAllShapesOverlappingWithAABB(aabb, callback);
}

// Compute all the overlapping pairs of collision shapes
void BroadPhaseSystem::computeOverlappingPairs(MemoryManager& memoryManager) {

    // TODO : Try to see if we can allocate potential pairs in single frame allocator

    // Reset the potential overlapping pairs
    mPotentialPairs.clear();

    LinkedList<int> overlappingNodes(memoryManager.getPoolAllocator());

    // For all collision shapes that have moved (or have been created) during the last simulation step
    for (auto it = mMovedShapes.begin(); it != mMovedShapes.end(); ++it) {
        int shapeID = *it;

        if (shapeID == -1) continue;

        AABBOverlapCallback callback(overlappingNodes);

        // Get the AABB of the shape
        const AABB& shapeAABB = mDynamicAABBTree.getFatAABB(shapeID);

        // Ask the dynamic AABB tree to report all collision shapes that overlap with
        // this AABB. The method BroadPhase::notifiyOverlappingPair() will be called
        // by the dynamic AABB tree for each potential overlapping pair.
        mDynamicAABBTree.reportAllShapesOverlappingWithAABB(shapeAABB, callback);

        // Add the potential overlapping pairs
        addOverlappingNodes(shapeID, overlappingNodes);

        // Remove all the elements of the linked list of overlapping nodes
        overlappingNodes.reset();
    }

    // Reset the array of collision shapes that have move (or have been created) during the
    // last simulation step
    mMovedShapes.clear();

    // Sort the array of potential overlapping pairs in order to remove duplicate pairs
    std::sort(mPotentialPairs.begin(), mPotentialPairs.end(), BroadPhasePair::smallerThan);

    // Check all the potential overlapping pairs avoiding duplicates to report unique
    // overlapping pairs
    auto it = mPotentialPairs.begin();
    while (it != mPotentialPairs.end()) {

        // Get a potential overlapping pair
        BroadPhasePair& pair = *it;
        ++it;

        assert(pair.collisionShape1ID != pair.collisionShape2ID);

        // Get the two collision shapes of the pair
        ProxyShape* shape1 = static_cast<ProxyShape*>(mDynamicAABBTree.getNodeDataPointer(pair.collisionShape1ID));
        ProxyShape* shape2 = static_cast<ProxyShape*>(mDynamicAABBTree.getNodeDataPointer(pair.collisionShape2ID));

        // If the two proxy collision shapes are from the same body, skip it
        if (shape1->getBody()->getId() != shape2->getBody()->getId()) {

            // Notify the collision detection about the overlapping pair
            mCollisionDetection.broadPhaseNotifyOverlappingPair(shape1, shape2);
        }

        // Skip the duplicate overlapping pairs
        while (it != mPotentialPairs.end()) {

            // Get the next pair
            BroadPhasePair& nextPair = *it;

            // If the next pair is different from the previous one, we stop skipping pairs
            if (nextPair.collisionShape1ID != pair.collisionShape1ID ||
                nextPair.collisionShape2ID != pair.collisionShape2ID) {
                break;
            }
            ++it;
        }
    }
}

// Notify the broad-phase about a potential overlapping pair in the dynamic AABB tree
void BroadPhaseSystem::addOverlappingNodes(int referenceNodeId, const LinkedList<int>& overlappingNodes) {

    // For each overlapping node in the linked list
    LinkedList<int>::ListElement* elem = overlappingNodes.getListHead();
    while (elem != nullptr) {

        // If both the nodes are the same, we do not create the overlapping pair
        if (referenceNodeId != elem->data) {

            // Add the new potential pair into the array of potential overlapping pairs
            mPotentialPairs.add(BroadPhasePair(std::min(referenceNodeId, elem->data),
                                               std::max(referenceNodeId, elem->data)));
        }

        elem = elem->next;
    }
}

// Called when a overlapping node has been found during the call to
// DynamicAABBTree:reportAllShapesOverlappingWithAABB()
void AABBOverlapCallback::notifyOverlappingNode(int nodeId) {
    mOverlappingNodes.insert(nodeId);
}

// Called for a broad-phase shape that has to be tested for raycast
decimal BroadPhaseRaycastCallback::raycastBroadPhaseShape(int32 nodeId, const Ray& ray) {

    decimal hitFraction = decimal(-1.0);

    // Get the proxy shape from the node
    ProxyShape* proxyShape = static_cast<ProxyShape*>(mDynamicAABBTree.getNodeDataPointer(nodeId));

    // Check if the raycast filtering mask allows raycast against this shape
    if ((mRaycastWithCategoryMaskBits & proxyShape->getCollisionCategoryBits()) != 0) {

        // Ask the collision detection to perform a ray cast test against
        // the proxy shape of this node because the ray is overlapping
        // with the shape in the broad-phase
        hitFraction = mRaycastTest.raycastAgainstShape(proxyShape, ray);
    }

    return hitFraction;
}
