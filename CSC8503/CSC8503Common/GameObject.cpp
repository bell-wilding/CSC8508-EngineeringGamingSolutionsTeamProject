#include "GameObject.h"
#include "CollisionDetection.h"

using namespace NCL::CSC8503;

GameObject::GameObject(string objectName) {
	name = objectName;
	layer = CollisionLayer::DEFAULT;
	worldID = -1;
	flagForRemoval = false;
	isTrigger = false;
	isActive = true;
	isSleeping = false;
	boundingVolume = nullptr;
	physicsObject = nullptr;
	renderObject = nullptr;
	networkObject = nullptr;
}

GameObject::~GameObject() {
	delete boundingVolume;
	delete physicsObject;
	delete renderObject;
	delete networkObject;
}

bool GameObject::GetBroadphaseAABB(Vector3& outSize) const {
	if (!boundingVolume) {
		return false;
	}
	outSize = broadphaseAABB;
	return true;
}

void GameObject::UpdateBroadphaseAABB() {
	if (!boundingVolume) {
		return;
	}
	if (boundingVolume->type == VolumeType::AABB) {
		broadphaseAABB = ((AABBVolume&)*boundingVolume).GetHalfDimensions();
	}
	else if (boundingVolume->type == VolumeType::Sphere) {
		float r = ((SphereVolume&)*boundingVolume).GetRadius();
		broadphaseAABB = Vector3(r, r, r);
	}
	else if (boundingVolume->type == VolumeType::Capsule) {
		Matrix3 mat = Matrix3(transform.GetOrientation());
		mat = mat.Absolute();
		float r = ((CapsuleVolume&)*boundingVolume).GetRadius();
		float halfHeight = ((CapsuleVolume&)*boundingVolume).GetHalfHeight();
		broadphaseAABB = mat * Vector3(r, halfHeight, r);
	}
	else if (boundingVolume->type == VolumeType::OBB) {
		Matrix3 mat = Matrix3(transform.GetOrientation());
		mat = mat.Absolute();
		Vector3 halfSizes = ((OBBVolume&)*boundingVolume).GetHalfDimensions();
		broadphaseAABB = mat * halfSizes;
	}
}