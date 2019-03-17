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
#include "ProxyShape.h"
#include "utils/Logger.h"
#include "collision/RaycastInfo.h"
#include "memory/MemoryManager.h"
#include "engine/CollisionWorld.h"

using namespace reactphysics3d;

// Constructor
/**
 * @param body Pointer to the parent body
 * @param shape Pointer to the collision shape
 * @param transform Transformation from collision shape local-space to body local-space
 * @param mass Mass of the collision shape (in kilograms)
 */
ProxyShape::ProxyShape(Entity entity, CollisionBody* body, MemoryManager& memoryManager)
           :mMemoryManager(memoryManager), mEntity(entity), mBody(body),
            mUserData(nullptr) {

}

// Destructor
ProxyShape::~ProxyShape() {

}

// Return the mass of the collision shape
/**
 * @return Mass of the collision shape (in kilograms)
 */
decimal ProxyShape::getMass() const {
    return mBody->mWorld.mProxyShapesComponents.getMass(mEntity);
}


// Return true if a point is inside the collision shape
/**
 * @param worldPoint Point to test in world-space coordinates
 * @return True if the point is inside the collision shape
 */
bool ProxyShape::testPointInside(const Vector3& worldPoint) {
    const Transform localToWorld = mBody->mWorld.mTransformComponents.getTransform(mBody->getEntity()) *
                                   mBody->mWorld.mProxyShapesComponents.getLocalToBodyTransform(mEntity);
    const Vector3 localPoint = localToWorld.getInverse() * worldPoint;
    const CollisionShape* collisionShape = mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
    return collisionShape->testPointInside(localPoint, this);
}

// Set the collision category bits
/**
 * @param collisionCategoryBits The collision category bits mask of the proxy shape
 */
void ProxyShape::setCollisionCategoryBits(unsigned short collisionCategoryBits) {

    mBody->mWorld.mProxyShapesComponents.setCollisionCategoryBits(mEntity, collisionCategoryBits);

    int broadPhaseId = mBody->mWorld.mProxyShapesComponents.getBroadPhaseId(mEntity);

    RP3D_LOG(mLogger, Logger::Level::Information, Logger::Category::ProxyShape,
             "ProxyShape " + std::to_string(broadPhaseId) + ": Set collisionCategoryBits=" +
             std::to_string(collisionCategoryBits));
}

// Set the collision bits mask
/**
 * @param collideWithMaskBits The bits mask that specifies with which collision category this shape will collide
 */
void ProxyShape::setCollideWithMaskBits(unsigned short collideWithMaskBits) {

    mBody->mWorld.mProxyShapesComponents.setCollideWithMaskBits(mEntity, collideWithMaskBits);

    int broadPhaseId = mBody->mWorld.mProxyShapesComponents.getBroadPhaseId(mEntity);

    RP3D_LOG(mLogger, Logger::Level::Information, Logger::Category::ProxyShape,
             "ProxyShape " + std::to_string(broadPhaseId) + ": Set collideWithMaskBits=" +
             std::to_string(collideWithMaskBits));
}

// Set the local to parent body transform
void ProxyShape::setLocalToBodyTransform(const Transform& transform) {

    // TODO : Make sure this method is never called by the internal physics engine

    //mLocalToBodyTransform = transform;
    mBody->mWorld.mProxyShapesComponents.setLocalToBodyTransform(mEntity, transform);

    mBody->setIsSleeping(false);

    mBody->mWorld.mCollisionDetection.updateProxyShape(mEntity);

    int broadPhaseId = mBody->mWorld.mProxyShapesComponents.getBroadPhaseId(mEntity);

    RP3D_LOG(mLogger, Logger::Level::Information, Logger::Category::ProxyShape,
             "ProxyShape " + std::to_string(broadPhaseId) + ": Set localToBodyTransform=" +
             transform.to_string());
}

// Return the AABB of the proxy shape in world-space
/**
 * @return The AABB of the proxy shape in world-space
 */
const AABB ProxyShape::getWorldAABB() const {
    AABB aabb;
    CollisionShape* collisionShape = mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
    collisionShape->computeAABB(aabb, getLocalToWorldTransform());
    return aabb;
}

// Return the collision shape
/**
 * @return Pointer to the internal collision shape
 */
const CollisionShape* ProxyShape::getCollisionShape() const {
    return mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
}

// Return the collision shape
/**
* @return Pointer to the internal collision shape
*/
CollisionShape* ProxyShape::getCollisionShape() {
    return mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
}

// Return the broad-phase id
int ProxyShape::getBroadPhaseId() const {
    return mBody->mWorld.mProxyShapesComponents.getBroadPhaseId(mEntity);
}

// Return the local to parent body transform
/**
 * @return The transformation that transforms the local-space of the collision shape
 *         to the local-space of the parent body
 */
const Transform& ProxyShape::getLocalToBodyTransform() const {
    return mBody->mWorld.mProxyShapesComponents.getLocalToBodyTransform(mEntity);
}

// Raycast method with feedback information
/**
 * @param ray Ray to use for the raycasting
 * @param[out] raycastInfo Result of the raycasting that is valid only if the
 *             methods returned true
 * @return True if the ray hits the collision shape
 */
bool ProxyShape::raycast(const Ray& ray, RaycastInfo& raycastInfo) {

    // If the corresponding body is not active, it cannot be hit by rays
    if (!mBody->isActive()) return false;

    // Convert the ray into the local-space of the collision shape
    const Transform localToWorldTransform = getLocalToWorldTransform();
    const Transform worldToLocalTransform = localToWorldTransform.getInverse();
    Ray rayLocal(worldToLocalTransform * ray.point1,
                 worldToLocalTransform * ray.point2,
                 ray.maxFraction);

    const CollisionShape* collisionShape = mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
    bool isHit = collisionShape->raycast(rayLocal, raycastInfo, this, mMemoryManager.getPoolAllocator());

    // Convert the raycast info into world-space
    raycastInfo.worldPoint = localToWorldTransform * raycastInfo.worldPoint;
    raycastInfo.worldNormal = localToWorldTransform.getOrientation() * raycastInfo.worldNormal;
    raycastInfo.worldNormal.normalize();

    return isHit;
}

// Return the collision category bits
/**
 * @return The collision category bits mask of the proxy shape
 */
unsigned short ProxyShape::getCollisionCategoryBits() const {
    return mBody->mWorld.mProxyShapesComponents.getCollisionCategoryBits(mEntity);
}

// Return the collision bits mask
/**
 * @return The bits mask that specifies with which collision category this shape will collide
 */
unsigned short ProxyShape::getCollideWithMaskBits() const {
    return mBody->mWorld.mProxyShapesComponents.getCollideWithMaskBits(mEntity);
}

// Return the local to world transform
/**
 * @return The transformation that transforms the local-space of the collision
 *         shape to the world-space
 */
const Transform ProxyShape::getLocalToWorldTransform() const {
    return mBody->mWorld.mTransformComponents.getTransform(mBody->getEntity()) *
           mBody->mWorld.mProxyShapesComponents.getLocalToBodyTransform(mEntity);
}

#ifdef IS_PROFILING_ACTIVE

// Set the profiler
void ProxyShape::setProfiler(Profiler* profiler) {

    mProfiler = profiler;

    CollisionShape* collisionShape = mBody->mWorld.mProxyShapesComponents.getCollisionShape(mEntity);
    collisionShape->setProfiler(profiler);
}

#endif

