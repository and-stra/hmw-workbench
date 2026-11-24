#pragma once

#include <DirectXMath.h>

namespace wb {

// Orbit camera around a target point in game space (Z-up, X forward, Y left).
// Yaw/pitch are in degrees, distance in world units.
class Camera {
public:
	using XMFLOAT3 = DirectX::XMFLOAT3;
	using XMMATRIX = DirectX::XMMATRIX;

	void SetYawPitch(float yaw, float pitch);
	void SetDistance(float distance);
	void SetFov(float fov);
	void SetTarget(const XMFLOAT3& target);
	void SetFirstPerson(const XMFLOAT3& eye, const XMFLOAT3& target);
	void SetOrbitMode() { firstPerson_ = false; }
	bool IsFirstPerson() const { return firstPerson_; }

	float Yaw() const { return yaw_; }
	float Pitch() const { return pitch_; }
	float Distance() const { return distance_; }
	float Fov() const { return fov_; }
	XMFLOAT3 Target() const { return target_; }
	// The point the camera looks at: the orbit target, or the first-person look target.
	XMFLOAT3 LookTarget() const { return firstPerson_ ? firstPersonTarget_ : target_; }
	XMFLOAT3 Eye() const;

	XMMATRIX View() const;
	XMMATRIX Projection(float aspect) const;
	XMMATRIX ViewProjection(float aspect) const;

	void Orbit(float dxPixels, float dyPixels);
	void Pan(float dxPixels, float dyPixels, float viewportHeight);
	void Zoom(float wheelDelta);
	void Frame(const XMFLOAT3& center, float radius);

private:
	XMFLOAT3 target_{0.0f, 0.0f, 0.0f};
	float yaw_ = 45.0f;
	float pitch_ = 25.0f;
	float distance_ = 20.0f;
	float fov_ = 60.0f;
	bool firstPerson_ = false;
	XMFLOAT3 firstPersonEye_{0.0f, 0.0f, 0.0f};
	XMFLOAT3 firstPersonTarget_{1.0f, 0.0f, 0.0f};
};

} // namespace wb
