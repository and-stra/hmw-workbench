#pragma once

#include <DirectXMath.h>

namespace wb {

// Orbit camera around a target point. Yaw/pitch are in degrees, distance in world units.
// Angles are clamped so the view matrix never degenerates.
class Camera {
public:
	using XMFLOAT3 = DirectX::XMFLOAT3;
	using XMMATRIX = DirectX::XMMATRIX;

	void SetYawPitch(float yaw, float pitch);
	void SetDistance(float distance);
	void SetFov(float fov);
	void SetTarget(const XMFLOAT3& target);

	float Yaw() const { return yaw_; }
	float Pitch() const { return pitch_; }
	float Distance() const { return distance_; }
	float Fov() const { return fov_; }
	XMFLOAT3 Target() const { return target_; }
	XMFLOAT3 Eye() const;

	XMMATRIX View() const;
	XMMATRIX Projection(float aspect) const;
	XMMATRIX ViewProjection(float aspect) const;

	void Orbit(float dxPixels, float dyPixels);
	void Pan(float dxPixels, float dyPixels, float viewportHeight);
	void Zoom(float wheelDelta);
	void Frame(float radius);

private:
	XMFLOAT3 target_{0.0f, 0.0f, 0.0f};
	float yaw_ = 45.0f;
	float pitch_ = 25.0f;
	float distance_ = 20.0f;
	float fov_ = 60.0f;
};

} // namespace wb
