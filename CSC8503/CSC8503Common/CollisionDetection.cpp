#include "CollisionDetection.h"
#include "CollisionVolume.h"
#include "AABBVolume.h"
#include "OBBVolume.h"
#include "SphereVolume.h"
#include "../../Common/Vector2.h"
#include "../../Common/Window.h"
#include "../../Common/Maths.h"
#include "Debug.h"
#include "GameObject.h"

#include <list>

using namespace NCL;

bool CollisionDetection::RayPlaneIntersection(const Ray&r, const Plane&p, RayCollision& collisions) {
	float ln = Vector3::Dot(p.GetNormal(), r.GetDirection());

	if (ln == 0.0f) {
		return false; //direction vectors are perpendicular!
	}
	
	Vector3 planePoint = p.GetPointOnPlane();

	Vector3 pointDir = planePoint - r.GetPosition();

	float d = Vector3::Dot(pointDir, p.GetNormal()) / ln;

	collisions.collidedAt = r.GetPosition() + (r.GetDirection() * d);

	return true;
}

bool CollisionDetection::RayIntersection(const Ray& r,GameObject& object, RayCollision& collision) {
	bool hasCollided = false;

	const Transform& worldTransform = object.GetTransform();
	const CollisionVolume* volume	= object.GetBoundingVolume();

	if (!volume) {
		return false;
	}

	switch (volume->type) {
		case VolumeType::AABB:		hasCollided = RayAABBIntersection(r, worldTransform, (const AABBVolume&)*volume	, collision); break;
		case VolumeType::OBB:		hasCollided = RayOBBIntersection(r, worldTransform, (const OBBVolume&)*volume	, collision); break;
		case VolumeType::Sphere:	hasCollided = RaySphereIntersection(r, worldTransform, (const SphereVolume&)*volume	, collision); break;
		case VolumeType::Capsule:	hasCollided = RayCapsuleIntersection(r, worldTransform, (const CapsuleVolume&)*volume, collision); break;
	}

	return hasCollided;
}

bool CollisionDetection::RayBoxIntersection(const Ray&r, const Vector3& boxPos, const Vector3& boxSize, RayCollision& collision) {
	Vector3 boxMin = boxPos - boxSize;
	Vector3 boxMax = boxPos + boxSize;

	Vector3 rayPos = r.GetPosition();
	Vector3 rayDir = r.GetDirection();

	Vector3 tVals(-1, -1, -1);

	for (int i = 0; i < 3; ++i) {
		if (rayDir[i] > 0) {
			tVals[i] = (boxMin[i] - rayPos[i]) / rayDir[i];
		}
		else if (rayDir[i] < 0) {
			tVals[i] = (boxMax[i] - rayPos[i]) / rayDir[i];
		}
	}
	float bestT = tVals.GetMaxElement();
	if (bestT < 0.0f) {
		return false;
	}
	 
	Vector3 intersection = rayPos + (rayDir * bestT);
	const float epsilon = 0.0001f;
	for (int i = 0; i < 3; ++i) {
		if (intersection[i] + epsilon < boxMin[i] || intersection[i] - epsilon > boxMax[i]) {
			return false;
		}
	}
	collision.collidedAt  = intersection;
	collision.rayDistance = bestT;
	return true;
}

bool CollisionDetection::RayAABBIntersection(const Ray&r, const Transform& worldTransform, const AABBVolume& volume, RayCollision& collision) {
	Vector3 boxPos  = worldTransform.GetPosition() + volume.GetOffset();
	Vector3 boxSize = volume.GetHalfDimensions();
	return RayBoxIntersection(r, boxPos, boxSize, collision);
}

bool CollisionDetection::RayOBBIntersection(const Ray&r, const Transform& worldTransform, const OBBVolume& volume, RayCollision& collision) {
	Quaternion orientation	= worldTransform.GetOrientation();
	Vector3 position		= worldTransform.GetPosition() + volume.GetOffset();

	Matrix3 transform		= Matrix3(orientation);
	Matrix3 invTransform	= Matrix3(orientation.Conjugate());

	Vector3 localRayPos		= r.GetPosition() - position;

	Ray tempRay(invTransform * localRayPos, invTransform * r.GetDirection());

	bool collided = RayBoxIntersection(tempRay, Vector3(), volume.GetHalfDimensions(), collision);

	if (collided) {
		collision.collidedAt = transform * collision.collidedAt + position;
	}
	return collided;
}

bool CollisionDetection::RayCapsuleIntersection(const Ray& r, const Transform& worldTransform, const CapsuleVolume& volume, RayCollision& collision) {

	Vector3 top((worldTransform.GetPosition() + volume.GetOffset()) + (worldTransform.GetOrientation() * Vector3(0, 1, 0) * (volume.GetHalfHeight() - volume.GetRadius())));
	Vector3 bottom((worldTransform.GetPosition() + volume.GetOffset()) - (worldTransform.GetOrientation() * Vector3(0, 1, 0) * (volume.GetHalfHeight() - volume.GetRadius())));
	
	Vector3 normal = r.GetPosition() - (worldTransform.GetPosition() + volume.GetOffset());
	Vector3 capsuleDir = top - bottom;
	Vector3 point = Vector3::Cross(capsuleDir, normal);
	point = worldTransform.GetPosition() + volume.GetOffset() + (point.Normalised() * 1.0f);

	Plane capPlane = Plane::PlaneFromTri(top, bottom, point);

	// No hit
	if (!RayPlaneIntersection(r, capPlane, collision)) 
		return false;

	float capsuleLineLength = capsuleDir.Length();
	capsuleDir.Normalise();

	Vector3 rayCapDir = collision.collidedAt - bottom;
	float dot = Maths::Clamp(Vector3::Dot(rayCapDir, capsuleDir), 0.0f, capsuleLineLength);

	Vector3 spherePos = bottom + (capsuleDir * dot);

	SphereVolume sphere(volume.GetRadius());
	Transform t;
	t.SetPosition(spherePos);
	t.SetScale(Vector3(1, 1, 1) * volume.GetRadius());

	return RaySphereIntersection(r, t, sphere, collision);
}

bool CollisionDetection::RaySphereIntersection(const Ray&r, const Transform& worldTransform, const SphereVolume& volume, RayCollision& collision) {
	Vector3 spherePos  = worldTransform.GetPosition() + volume.GetOffset();
	float sphereRadius = volume.GetRadius();

	Vector3 dir = (spherePos - r.GetPosition());

	float sphereProj = Vector3::Dot(dir, r.GetDirection());

	if (sphereProj < 0.0f) {
		return false;
	}

	Vector3 point = r.GetPosition() + (r.GetDirection() * sphereProj);

	float sphereDist = (point - spherePos).Length();

	if (sphereDist > sphereRadius) {
		return false;
	}

	float offset = sqrt((sphereRadius * sphereRadius) - (sphereDist * sphereDist));

	collision.rayDistance = sphereProj - (offset);
	collision.collidedAt = r.GetPosition() + (r.GetDirection() * collision.rayDistance);
	return true;
}

Matrix4 GenerateInverseView(const Camera &c) {
	float pitch = c.GetPitch();
	float yaw	= c.GetYaw();
	Vector3 position = c.GetPosition();

	Matrix4 iview =
		Matrix4::Translation(position) *
		Matrix4::Rotation(-yaw, Vector3(0, -1, 0)) *
		Matrix4::Rotation(-pitch, Vector3(-1, 0, 0));

	return iview;
}

Vector3 CollisionDetection::Unproject(const Vector3& screenPos, const Camera& cam) {
	Vector2 screenSize = Window::GetWindow()->GetScreenSize();

	float aspect	= screenSize.x / screenSize.y;
	float fov		= cam.GetFieldOfVision();
	float nearPlane = cam.GetNearPlane();
	float farPlane  = cam.GetFarPlane();

	//Create our inverted matrix! Note how that to get a correct inverse matrix,
	//the order of matrices used to form it are inverted, too.
	Matrix4 invVP = GenerateInverseView(cam) * GenerateInverseProjection(aspect, fov, nearPlane, farPlane);

	//Our mouse position x and y values are in 0 to screen dimensions range,
	//so we need to turn them into the -1 to 1 axis range of clip space.
	//We can do that by dividing the mouse values by the width and height of the
	//screen (giving us a range of 0.0 to 1.0), multiplying by 2 (0.0 to 2.0)
	//and then subtracting 1 (-1.0 to 1.0).
	Vector4 clipSpace = Vector4(
		(screenPos.x / (float)screenSize.x) * 2.0f - 1.0f,
		(screenPos.y / (float)screenSize.y) * 2.0f - 1.0f,
		(screenPos.z),
		1.0f
	);

	//Then, we multiply our clipspace coordinate by our inverted matrix
	Vector4 transformed = invVP * clipSpace;

	//our transformed w coordinate is now the 'inverse' perspective divide, so
	//we can reconstruct the final world space by dividing x,y,and z by w.
	return Vector3(transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w);
}

Ray CollisionDetection::BuildRayFromMouse(const Camera& cam) {
	Vector2 screenMouse = Window::GetMouse()->GetAbsolutePosition();
	Vector2 screenSize	= Window::GetWindow()->GetScreenSize();

	//We remove the y axis mouse position from height as OpenGL is 'upside down',
	//and thinks the bottom left is the origin, instead of the top left!
	Vector3 nearPos = Vector3(screenMouse.x,
		screenSize.y - screenMouse.y,
		-0.99999f
	);

	//We also don't use exactly 1.0 (the normalised 'end' of the far plane) as this
	//causes the unproject function to go a bit weird. 
	Vector3 farPos = Vector3(screenMouse.x,
		screenSize.y - screenMouse.y,
		0.99999f
	);

	Vector3 a = Unproject(nearPos, cam);
	Vector3 b = Unproject(farPos, cam);
	Vector3 c = b - a;

	c.Normalise();

	//std::cout << "Ray Direction:" << c << std::endl;

	return Ray(cam.GetPosition(), c);
}

//http://bookofhook.com/mousepick.pdf
Matrix4 CollisionDetection::GenerateInverseProjection(float aspect, float fov, float nearPlane, float farPlane) {
	Matrix4 m;

	float t = tan(fov*PI_OVER_360);

	float neg_depth = nearPlane - farPlane;

	const float h = 1.0f / t;

	float c = (farPlane + nearPlane) / neg_depth;
	float e = -1.0f;
	float d = 2.0f*(nearPlane*farPlane) / neg_depth;

	m.array[0]  = aspect / h;
	m.array[5]  = tan(fov*PI_OVER_360);

	m.array[10] = 0.0f;
	m.array[11] = 1.0f / d;

	m.array[14] = 1.0f / e;

	m.array[15] = -c / (d*e);

	return m;
}

/*
And here's how we generate an inverse view matrix. It's pretty much
an exact inversion of the BuildViewMatrix function of the Camera class!
*/
Matrix4 CollisionDetection::GenerateInverseView(const Camera &c) {
	float pitch = c.GetPitch();
	float yaw	= c.GetYaw();
	Vector3 position = c.GetPosition();

	Matrix4 iview =
Matrix4::Translation(position) *
Matrix4::Rotation(yaw, Vector3(0, 1, 0)) *
Matrix4::Rotation(pitch, Vector3(1, 0, 0));

return iview;
}

Vector3 NCL::CollisionDetection::OBBSupport(const Transform& worldTransform, Vector3 worldDir)
{
	Vector3 localDir = worldTransform.GetOrientation().Conjugate() * worldDir;
	Vector3 vertex;
	vertex.x = localDir.x < 0 ? -0.5f : 0.5f;
	vertex.y = localDir.y < 0 ? -0.5f : 0.5f;
	vertex.z = localDir.z < 0 ? -0.5f : 0.5f;

	return worldTransform.GetMatrix() * vertex;
}

Vector3 NCL::CollisionDetection::SpherePosFromCapsule(const CapsuleVolume& capsule, const Transform& capTransform, const Vector3& otherObjPos)
{
	Vector3 extentPosition = capTransform.GetOrientation() * Vector3(0, 1, 0) * (capsule.GetHalfHeight() - capsule.GetRadius());
	Vector3 capTop(capTransform.GetPosition() + capsule.GetOffset() + extentPosition);
	Vector3 capBottom(capTransform.GetPosition() + capsule.GetOffset() - extentPosition);

	Vector3 capsuleDir = capTop - capBottom;
	float capLineLength = capsuleDir.Length();
	capsuleDir.Normalise();

	Vector3 pointCapDir = otherObjPos - capBottom;
	float dot = Maths::Clamp(Vector3::Dot(pointCapDir, capsuleDir), 0.0f, capLineLength);

	return capBottom + (capsuleDir * dot);
}

Vector3 NCL::CollisionDetection::ClosestPointOnLineSegment(Vector3 a, Vector3 b, Vector3 point)
{
	Vector3 ab = b - a;
	float t = Vector3::Dot(point - a, ab) / Vector3::Dot(ab, ab);
	return a + ab * Maths::Clamp(t, 0.0f, 1.0f);
}

/*
If you've read through the Deferred Rendering tutorial you should have a pretty
good idea what this function does. It takes a 2D position, such as the mouse
position, and 'unprojects' it, to generate a 3D world space position for it.

Just as we turn a world space position into a clip space position by multiplying
it by the model, view, and projection matrices, we can turn a clip space
position back to a 3D position by multiply it by the INVERSE of the
view projection matrix (the model matrix has already been assumed to have
'transformed' the 2D point). As has been mentioned a few times, inverting a
matrix is not a nice operation, either to understand or code. But! We can cheat
the inversion process again, just like we do when we create a view matrix using
the camera.

So, to form the inverted matrix, we need the aspect and fov used to create the
projection matrix of our scene, and the camera used to form the view matrix.

*/
Vector3	CollisionDetection::UnprojectScreenPosition(Vector3 position, float aspect, float fov, const Camera &c) {
	//Create our inverted matrix! Note how that to get a correct inverse matrix,
	//the order of matrices used to form it are inverted, too.
	Matrix4 invVP = GenerateInverseView(c) * GenerateInverseProjection(aspect, fov, c.GetNearPlane(), c.GetFarPlane());

	Vector2 screenSize = Window::GetWindow()->GetScreenSize();

	//Our mouse position x and y values are in 0 to screen dimensions range,
	//so we need to turn them into the -1 to 1 axis range of clip space.
	//We can do that by dividing the mouse values by the width and height of the
	//screen (giving us a range of 0.0 to 1.0), multiplying by 2 (0.0 to 2.0)
	//and then subtracting 1 (-1.0 to 1.0).
	Vector4 clipSpace = Vector4(
		(position.x / (float)screenSize.x) * 2.0f - 1.0f,
		(position.y / (float)screenSize.y) * 2.0f - 1.0f,
		(position.z) - 1.0f,
		1.0f
	);

	//Then, we multiply our clipspace coordinate by our inverted matrix
	Vector4 transformed = invVP * clipSpace;

	//our transformed w coordinate is now the 'inverse' perspective divide, so
	//we can reconstruct the final world space by dividing x,y,and z by w.
	return Vector3(transformed.x / transformed.w, transformed.y / transformed.w, transformed.z / transformed.w);
}

bool CollisionDetection::ObjectIntersection(GameObject* a, GameObject* b, CollisionInfo& collisionInfo) {
	const CollisionVolume* volA = a->GetBoundingVolume();
	const CollisionVolume* volB = b->GetBoundingVolume();

	if (!volA || !volB) {
		return false;
	}

	collisionInfo.a = a;
	collisionInfo.b = b;

	Transform& transformA = a->GetTransform();
	Transform& transformB = b->GetTransform();

	VolumeType pairType = (VolumeType)((int)volA->type | (int)volB->type);

	if (pairType == VolumeType::AABB) {
		return AABBIntersection((AABBVolume&)*volA, transformA, (AABBVolume&)*volB, transformB, collisionInfo);
	}

	if (pairType == VolumeType::Sphere) {
		return SphereIntersection((SphereVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}

	if (pairType == VolumeType::OBB) {
		return OBBIntersection((OBBVolume&)*volA, transformA, (OBBVolume&)*volB, transformB, collisionInfo);
	}

	if (pairType == VolumeType::Capsule) {
		return CapsuleIntersection((CapsuleVolume&)*volA, transformA, (CapsuleVolume&)*volB, transformB, collisionInfo);
	}

	if (volA->type == VolumeType::AABB && volB->type == VolumeType::Sphere) {
		return AABBSphereIntersection((AABBVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::AABB) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return AABBSphereIntersection((AABBVolume&)*volB, transformB, (SphereVolume&)*volA, transformA, collisionInfo);
	}

	if (volA->type == VolumeType::Capsule && volB->type == VolumeType::Sphere) {
		return SphereCapsuleIntersection((CapsuleVolume&)*volA, transformA, (SphereVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::Capsule) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return SphereCapsuleIntersection((CapsuleVolume&)*volB, transformB, (SphereVolume&)*volA, transformA, collisionInfo);
	}

	if (volA->type == VolumeType::Capsule && volB->type == VolumeType::AABB) {
		return AABBCapsuleIntersection((CapsuleVolume&)* volA, transformA, (AABBVolume&)* volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::AABB && volB->type == VolumeType::Capsule) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return AABBCapsuleIntersection((CapsuleVolume&)* volB, transformB, (AABBVolume&)* volA, transformA, collisionInfo);
	}

	if (volA->type == VolumeType::Sphere && volB->type == VolumeType::OBB) {
		return SphereOBBIntersection((SphereVolume&)*volA, transformA, (OBBVolume&)*volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::OBB && volB->type == VolumeType::Sphere) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return SphereOBBIntersection((SphereVolume&)*volB, transformB, (OBBVolume&)*volA, transformA, collisionInfo);
	}

	if (volA->type == VolumeType::Capsule && volB->type == VolumeType::OBB) {
		return CapsuleOBBIntersection((CapsuleVolume&)* volA, transformA, (OBBVolume&)* volB, transformB, collisionInfo);
	}
	if (volA->type == VolumeType::OBB && volB->type == VolumeType::Capsule) {
		collisionInfo.a = b;
		collisionInfo.b = a;
		return CapsuleOBBIntersection((CapsuleVolume&)* volB, transformB, (OBBVolume&)* volA, transformA, collisionInfo);
	}

	if ((volA->type == VolumeType::AABB && volB->type == VolumeType::OBB) || (volA->type == VolumeType::OBB && volB->type == VolumeType::AABB)) {
		return OBBIntersection((OBBVolume&)*volA, transformA, (OBBVolume&)*volB, transformB, collisionInfo);
	}

	return false;
}

bool CollisionDetection::AABBTest(const Vector3& posA, const Vector3& posB, const Vector3& halfSizeA, const Vector3& halfSizeB) {

	Vector3 delta = posB - posA;
	Vector3 totalSize = halfSizeA + halfSizeB;

	if (abs(delta.x) < totalSize.x &&
		abs(delta.y) < totalSize.y &&
		abs(delta.z) < totalSize.z) {
		return true;
	}
	return false;
}

//AABB/AABB Collisions
bool CollisionDetection::AABBIntersection(const AABBVolume& volumeA, const Transform& worldTransformA,
	const AABBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	Vector3 boxAPos = worldTransformA.GetPosition() + volumeA.GetOffset();
	Vector3 boxBPos = worldTransformB.GetPosition() + volumeB.GetOffset();

	Vector3 boxASize = volumeA.GetHalfDimensions();
	Vector3 boxBSize = volumeB.GetHalfDimensions();

	bool overlap = AABBTest(boxAPos, boxBPos, boxASize, boxBSize);

	if (overlap) {
		static const Vector3 faces[6] = {
			Vector3(-1, 0, 0), Vector3(1, 0, 0),
			Vector3(0, -1, 0), Vector3(0, 1, 0),
			Vector3(0, 0, -1), Vector3(0, 0, 1)
		};

		Vector3 maxA = boxAPos + boxASize;
		Vector3 minA = boxAPos - boxASize;

		Vector3 maxB = boxBPos + boxBSize;
		Vector3 minB = boxBPos - boxBSize;

		float distances[6] = {
			(maxB.x - minA.x),
			(maxA.x - minB.x),
			(maxB.y - minA.y),
			(maxA.y - minB.y),
			(maxB.z - minA.z),
			(maxA.z - minB.z)
		};
		float penetration = FLT_MAX;
		Vector3 bestAxis;

		for (int i = 0; i < 6; i++) {
			if (distances[i] < penetration) {
				penetration = distances[i];
				bestAxis = faces[i];
			}
		}
		collisionInfo.AddContactPoint(Vector3(), Vector3(), bestAxis, penetration);
		return true;
	}
	return false;
}

bool NCL::CollisionDetection::CapsuleIntersection(const CapsuleVolume& volumeA, const Transform& worldTransformA, 
	const CapsuleVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	// Code adjusted from: https://wickedengine.net/2020/04/26/capsule-collision-detection/

	Vector3 capExtPositionA = worldTransformA.GetOrientation() * Vector3(0, 1, 0) * (volumeA.GetHalfHeight() - volumeA.GetRadius());
	Vector3 capTopA(worldTransformA.GetPosition() + volumeA.GetOffset() + capExtPositionA);
	Vector3 capBottomA(worldTransformA.GetPosition() + volumeA.GetOffset() - capExtPositionA);

	Vector3 capExtPositionB = worldTransformB.GetOrientation() * Vector3(0, 1, 0) * (volumeB.GetHalfHeight() - volumeB.GetRadius());
	Vector3 capTopB(worldTransformB.GetPosition() + volumeB.GetOffset() + capExtPositionB);
	Vector3 capBottomB(worldTransformB.GetPosition() + volumeB.GetOffset() - capExtPositionB);

	Vector3 v0 = capBottomB - capBottomA;
	Vector3 v1 = capTopB - capBottomA;
	Vector3 v2 = capBottomB - capTopA;
	Vector3 v3 = capTopB - capTopA;

	float d0 = Vector3::Dot(v0, v0);
	float d1 = Vector3::Dot(v1, v1);
	float d2 = Vector3::Dot(v2, v2);
	float d3 = Vector3::Dot(v3, v3);

	Vector3 bestA;
	if (d2 < d0 || d2 < d1 || d3 < d0 || d3 < d1) {
		bestA = capTopA;
	}
	else {
		bestA = capBottomA;
	}

	Vector3 bestB = ClosestPointOnLineSegment(capBottomB, capTopB, bestA);
	bestA = ClosestPointOnLineSegment(capBottomA, capTopA, bestB);

	SphereVolume sphereA(volumeA.GetRadius());
	Transform sphereTransformA;
	sphereTransformA.SetPosition(bestA);
	sphereTransformA.SetScale(Vector3(1, 1, 1) * volumeA.GetRadius());

	SphereVolume sphereB(volumeB.GetRadius());
	Transform sphereTransformB;
	sphereTransformB.SetPosition(bestB);
	sphereTransformB.SetScale(Vector3(1, 1, 1) * volumeB.GetRadius());

	bool collision = SphereIntersection(sphereA, sphereTransformA, sphereB, sphereTransformB, collisionInfo);
	collisionInfo.point.localA = collisionInfo.point.localA + (sphereTransformA.GetPosition() - worldTransformA.GetPosition() + volumeA.GetOffset());
	collisionInfo.point.localB = collisionInfo.point.localB + (sphereTransformB.GetPosition() - worldTransformB.GetPosition() + volumeB.GetOffset());
	return collision;
}

//Sphere / Sphere Collision
bool CollisionDetection::SphereIntersection(const SphereVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	
	float radii = volumeA.GetRadius() + volumeB.GetRadius();
	Vector3 delta = (worldTransformB.GetPosition() + volumeB.GetOffset())- (worldTransformA.GetPosition() + volumeA.GetOffset());

	float deltaLength = delta.Length();

	if (deltaLength < radii) {
		float penetration = (radii - deltaLength);
		Vector3 normal = delta.Normalised();
		Vector3 localA = normal * volumeA.GetRadius();
		Vector3 localB = -normal * volumeB.GetRadius();

		collisionInfo.AddContactPoint(localA, localB, normal, penetration);
		return true;
	}
	return false;
}

//Sphere / OBB Collision
bool NCL::CollisionDetection::SphereOBBIntersection(const SphereVolume& volumeA, const Transform& worldTransformA,
	const OBBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {
	
	Quaternion orientation = worldTransformB.GetOrientation();

	Matrix3 transform = Matrix3(orientation);
	Matrix3 invTransform = Matrix3(orientation.Conjugate());

	SphereVolume sphere(volumeA.GetRadius());
	Transform sphereTransform;
	sphereTransform.SetPosition(invTransform * worldTransformA.GetPosition() + volumeA.GetOffset());
	sphereTransform.SetScale(Vector3(1, 1, 1) * volumeA.GetRadius());

	AABBVolume aabb(volumeB.GetHalfDimensions());
	Transform aabbTransform;
	aabbTransform.SetPosition(invTransform * worldTransformB.GetPosition() + volumeB.GetOffset());
	aabbTransform.SetScale(volumeB.GetHalfDimensions());

	bool collided = AABBSphereIntersection(aabb, aabbTransform, sphere, sphereTransform, collisionInfo, true);
	collisionInfo.point.localA = transform * collisionInfo.point.localA;
	collisionInfo.point.localB = transform * collisionInfo.point.localB;
	collisionInfo.point.normal = transform * -collisionInfo.point.normal;
	return collided;
}

//Capsule / OBB Collision
bool NCL::CollisionDetection::CapsuleOBBIntersection(const CapsuleVolume& volumeA, const Transform& worldTransformA, 
	const OBBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	Matrix3 invTransform = Matrix3(worldTransformB.GetOrientation().Conjugate());
	Vector3 point = worldTransformB.GetOrientation() * Maths::Clamp(
		invTransform * worldTransformA.GetPosition() + volumeA.GetOffset(),
		worldTransformB.GetPosition() + volumeB.GetOffset() - volumeB.GetHalfDimensions(),
		worldTransformB.GetPosition() + volumeB.GetOffset() + volumeB.GetHalfDimensions()
	);
	
	SphereVolume sphere(volumeA.GetRadius());
	Transform sphereTransform;
	sphereTransform.SetPosition(SpherePosFromCapsule(volumeA, worldTransformA, point));
	sphereTransform.SetScale(Vector3(1, 1, 1) * volumeA.GetRadius());

	bool collision = SphereOBBIntersection(sphere, sphereTransform, volumeB, worldTransformB, collisionInfo);
	collisionInfo.point.localA = collisionInfo.point.localA + (sphereTransform.GetPosition() - worldTransformA.GetPosition() + volumeA.GetOffset());
	return collision;
}

//AABB / Sphere Collision
bool CollisionDetection::AABBSphereIntersection(const AABBVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo, bool useBoxPoint) {

	Vector3 boxSize = volumeA.GetHalfDimensions();

	Vector3 delta = (worldTransformB.GetPosition() + volumeB.GetOffset()) - (worldTransformA.GetPosition() + volumeA.GetOffset());

	Vector3 closestPointOnBox = Maths::Clamp(delta, -boxSize, boxSize);
	Vector3 localPoint = delta - closestPointOnBox;
	float distance = localPoint.Length();

	if (distance < volumeB.GetRadius()) {
		Vector3 collisionNormal = localPoint.Normalised();
		float penetration = (volumeB.GetRadius() - distance);

		Vector3 localA = -collisionNormal * volumeB.GetRadius(); 
		Vector3 localB = useBoxPoint ? closestPointOnBox : Vector3();

		collisionInfo.AddContactPoint(localA, localB, collisionNormal, penetration);
		return true;
	}
	return false;
}

bool CollisionDetection::OBBIntersection(
	const OBBVolume& volumeA, const Transform& worldTransformA,
	const OBBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	Vector3 directions[15]{

		// A XYZ
		worldTransformA.GetOrientation() * Vector3(1, 0, 0),
		worldTransformA.GetOrientation() * Vector3(0, 1, 0),
		worldTransformA.GetOrientation() * Vector3(0, 0, 1),

		// B XYZ
		worldTransformB.GetOrientation() * Vector3(1, 0, 0),
		worldTransformB.GetOrientation() * Vector3(0, 1, 0),
		worldTransformB.GetOrientation() * Vector3(0, 0, 1),

		// Ax cross B XYZ
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(1, 0, 0), worldTransformB.GetOrientation() * Vector3(1, 0, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(1, 0, 0), worldTransformB.GetOrientation() * Vector3(0, 1, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(1, 0, 0), worldTransformB.GetOrientation() * Vector3(0, 0, 1)),

		// Ay cross B XYZ
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 1, 0), worldTransformB.GetOrientation() * Vector3(1, 0, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 1, 0), worldTransformB.GetOrientation() * Vector3(0, 1, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 1, 0), worldTransformB.GetOrientation() * Vector3(0, 0, 1)),

		// Az cross B XYZ
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 0, 1), worldTransformB.GetOrientation() * Vector3(1, 0, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 0, 1), worldTransformB.GetOrientation() * Vector3(0, 1, 0)),
		Vector3::Cross(worldTransformA.GetOrientation() * Vector3(0, 0, 1), worldTransformB.GetOrientation() * Vector3(0, 0, 1)),
	};

	float leastPenetration = FLT_MAX;
	int penIndex = -1;

	for (int i = 0; i < 15; ++i) {
		if (Vector3::Dot(directions[i], directions[i]) < 0.99f)
			continue;

		// Get min and max extents for both shapes along an axis
		Vector3 maxA = OBBSupport(worldTransformA, directions[i]);
		Vector3 minA = OBBSupport(worldTransformA, -directions[i]);

		Vector3 maxB = OBBSupport(worldTransformB, directions[i]);
		Vector3 minB = OBBSupport(worldTransformB, -directions[i]);

		// Project those points on to the line
		float denom = Vector3::Dot(directions[i], directions[i]);

		Vector3 minExtentA = directions[i] * (Vector3::Dot(minA, directions[i]) / denom);
		Vector3 maxExtentA = directions[i] * (Vector3::Dot(maxA, directions[i]) / denom);
		Vector3 minExtentB = directions[i] * (Vector3::Dot(minB, directions[i]) / denom);
		Vector3 maxExtentB = directions[i] * (Vector3::Dot(maxB, directions[i]) / denom);

		float distance = FLT_MAX;
		float length = FLT_MAX;
		float penDist = FLT_MAX;

		float left = Vector3::Dot(maxExtentA - minExtentA, minExtentB - minExtentA);
		float right = Vector3::Dot(minExtentA - maxExtentA, maxExtentB - maxExtentA);

		if (right > 0.0f) {
			// Object B to the left
			distance = (maxExtentB - maxExtentA).Length();
			length = (maxExtentA - minExtentA).Length();
			if (distance <= length) {
				penDist = length - distance;
				if (penDist < leastPenetration) {
					leastPenetration = penDist;
					collisionInfo.point.localA = minA;
					collisionInfo.point.localB = maxB;
					collisionInfo.point.normal = -directions[i];
					penIndex = i;
				}
				continue;
			}
		}

		if (left > 0.0f) {
			// Object A to the left
			distance = (minExtentB - minExtentA).Length();
			length = (maxExtentA - minExtentA).Length();
			if (distance <= length) {
				penDist = length - distance;
				if (penDist < leastPenetration) {
					leastPenetration = penDist;
					collisionInfo.point.localA = maxA;
					collisionInfo.point.localB = minB;
					collisionInfo.point.normal = directions[i];
					penIndex = i;
				}
				continue;
			}
		}

		if (left < 0.0f && right < 0.0f) {
			penIndex = i;
			continue;
		}

		return false;
	}

	collisionInfo.point.penetration = leastPenetration;
	collisionInfo.point.localA -= worldTransformA.GetPosition() + volumeA.GetOffset();
	collisionInfo.point.localB -= worldTransformB.GetPosition() + volumeB.GetOffset();
	return true;
}

// Sphere / Capsule Collision
bool CollisionDetection::SphereCapsuleIntersection(
	const CapsuleVolume& volumeA, const Transform& worldTransformA,
	const SphereVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	SphereVolume sphere(volumeA.GetRadius());
	Transform sphereTransform;
	sphereTransform.SetPosition(SpherePosFromCapsule(volumeA, worldTransformA, worldTransformB.GetPosition() + volumeB.GetOffset()));
	sphereTransform.SetScale(Vector3(1, 1, 1) * volumeA.GetRadius());

	bool collision = SphereIntersection(sphere, sphereTransform, volumeB, worldTransformB, collisionInfo);
	collisionInfo.point.localA = collisionInfo.point.localA + (sphereTransform.GetPosition() - worldTransformA.GetPosition() + volumeA.GetOffset());
	return collision;
}

//AABB / Capsule Collision
bool CollisionDetection::AABBCapsuleIntersection(
	const CapsuleVolume& volumeA, const Transform& worldTransformA,
	const AABBVolume& volumeB, const Transform& worldTransformB, CollisionInfo& collisionInfo) {

	Vector3 point = Maths::Clamp(worldTransformA.GetPosition() + volumeA.GetOffset(), worldTransformB.GetPosition() + volumeB.GetOffset() - volumeB.GetHalfDimensions(), worldTransformB.GetPosition() + volumeB.GetOffset() + volumeB.GetHalfDimensions());

	SphereVolume sphere(volumeA.GetRadius());
	Transform sphereTransform;
	sphereTransform.SetPosition(SpherePosFromCapsule(volumeA, worldTransformA, point));
	sphereTransform.SetScale(Vector3(1, 1, 1) * volumeA.GetRadius());

	bool collision = AABBSphereIntersection(volumeB, worldTransformB, sphere, sphereTransform, collisionInfo);
	collisionInfo.point.normal = -collisionInfo.point.normal;
	collisionInfo.point.localA = collisionInfo.point.localA + (sphereTransform.GetPosition() - worldTransformA.GetPosition() + volumeA.GetOffset());
	return collision;
}

bool NCL::CollisionDetection::CollisionInfo::operator<(const CollisionInfo& other) const
{
	size_t otherHash = (size_t)other.a->GetWorldID() + ((size_t)other.b->GetWorldID() << 32);
	size_t thisHash = (size_t)a->GetWorldID() + ((size_t)b->GetWorldID() << 32);

	if (thisHash < otherHash) {
		return true;
	}
	return false;
}
