#include "render/camera.h"

#include <algorithm>
#include <cmath>

namespace wb {
namespace {

constexpr float kMinPitch = -89.0f;
constexpr float kMaxPitch = 89.0f;
constexpr float kMinDistance = 0.1f;
constexpr float kMaxDistance = 1000.0f;
constexpr float kMinFov = 10.0f;
constexpr float kMaxFov = 140.0f;
constexpr float kOrbitSpeed = 0.3f; // degrees per pixel

} // namespace

void Camera::SetYawPitch(float yaw, float pitch) {
	yaw_ = yaw;
	pitch_ = std::clamp(pitch, kMinPitch, kMaxPitch);
}

void Camera::SetDistance(float distance) {
	distance_ = std::clamp(distance, kMinDistance, kMaxDistance);
}

void Camera::SetFov(float fov) {
	fov_ = std::clamp(fov, kMinFov, kMaxFov);
}

void Camera::SetTarget(const XMFLOAT3& target) {
	target_ = target;
}

DirectX::XMFLOAT3 Camera::Eye() const {
	const float yaw = DirectX::XMConvertToRadians(yaw_);
	const float pitch = DirectX::XMConvertToRadians(pitch_);
	const float cp = std::cos(pitch);
	const float sp = std::sin(pitch);
	return DirectX::XMFLOAT3(
	    target_.x + distance_ * cp * std::sin(yaw),
	    target_.y + distance_ * sp,
	    target_.z + distance_ * cp * std::cos(yaw));
}

DirectX::XMMATRIX Camera::View() const {
	const DirectX::XMFLOAT3 eye = Eye();
	const DirectX::XMVECTOR eyeVector = DirectX::XMLoadFloat3(&eye);
	const DirectX::XMVECTOR targetVector = DirectX::XMLoadFloat3(&target_);
	const DirectX::XMVECTOR up = DirectX::XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	return DirectX::XMMatrixLookAtLH(eyeVector, targetVector, up);
}

DirectX::XMMATRIX Camera::Projection(float aspect) const {
	if (aspect <= 0.0f) {
		aspect = 1.0f;
	}
	return DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(fov_), aspect, 0.05f, 2000.0f);
}

DirectX::XMMATRIX Camera::ViewProjection(float aspect) const {
	return View() * Projection(aspect);
}

void Camera::Orbit(float dxPixels, float dyPixels) {
	yaw_ -= dxPixels * kOrbitSpeed;
	pitch_ = std::clamp(pitch_ - dyPixels * kOrbitSpeed, kMinPitch, kMaxPitch);
	if (yaw_ > 360.0f || yaw_ < -360.0f) {
		yaw_ = std::fmod(yaw_, 360.0f);
	}
}

void Camera::Pan(float dxPixels, float dyPixels, float viewportHeight) {
	const float height = std::max(viewportHeight, 1.0f);
	const float worldPerPixel =
	    (2.0f * distance_ * std::tan(DirectX::XMConvertToRadians(fov_ * 0.5f))) / height;
	const float scale = std::max(worldPerPixel, 1.0e-5f);

	const DirectX::XMMATRIX view = View();
	DirectX::XMFLOAT4X4 viewFloats;
	DirectX::XMStoreFloat4x4(&viewFloats, view);

	// Row 0/1 of the view matrix are the camera right/up axes (row-vector convention).
	const DirectX::XMVECTOR right = DirectX::XMVectorSet(
	    viewFloats.m[0][0], viewFloats.m[0][1], viewFloats.m[0][2], 0.0f);
	const DirectX::XMVECTOR up = DirectX::XMVectorSet(
	    viewFloats.m[1][0], viewFloats.m[1][1], viewFloats.m[1][2], 0.0f);

	DirectX::XMVECTOR target = DirectX::XMLoadFloat3(&target_);
	target = DirectX::XMVectorAdd(target, DirectX::XMVectorScale(right, -dxPixels * scale));
	target = DirectX::XMVectorAdd(target, DirectX::XMVectorScale(up, dyPixels * scale));
	DirectX::XMStoreFloat3(&target_, target);
}

void Camera::Zoom(float wheelDelta) {
	if (wheelDelta == 0.0f) {
		return;
	}
	distance_ = std::clamp(distance_ * std::pow(0.85f, wheelDelta), kMinDistance, kMaxDistance);
}

void Camera::Frame(float radius) {
	target_ = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
	yaw_ = 45.0f;
	pitch_ = 25.0f;
	distance_ = std::clamp(std::max(radius, 0.5f) * 3.0f, 0.5f, kMaxDistance);
}

} // namespace wb
