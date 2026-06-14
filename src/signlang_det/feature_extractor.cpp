#include "feature_extractor.hpp"

#include <algorithm>
#include <cmath>

namespace signlang::signlang_det {
  namespace {

    static_assert(signlang::handpose_det::kHandPoseKeypointCount == signlang::signlang_det::kKeypointCount,
                  "Keypoint count mismatch between handpose and signlang modules");

    constexpr auto kScaleEpsilon = float{1e-6f};

    auto select_best_hand_impl(
      const handpose_det::HandPoseDetection* detections,
      std::uint32_t count,
      float min_confidence)
      -> const handpose_det::HandPoseDetection*
    {
      if (count == 0) {
        return nullptr;
      }

      const handpose_det::HandPoseDetection* best = nullptr;
      float best_confidence = min_confidence;

      for (std::uint32_t i = 0; i < count; ++i) {
        const auto& detection = detections[i];
        if (detection.confidence > best_confidence) {
          best = &detection;
          best_confidence = detection.confidence;
        }
      }

      return best;
    }

    auto compute_bounding_box_scale_impl(
      const std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>& keypoints)
      -> float
    {
      const auto& wrist = keypoints[0];
      auto max_distance = float{0.0f};

      for (const auto& kp : keypoints) {
        const auto dx = std::abs(kp.x - wrist.x);
        const auto dy = std::abs(kp.y - wrist.y);
        max_distance = std::max(max_distance, std::max(dx, dy));
      }

      return max_distance + kScaleEpsilon;
    }

  } // namespace

  FeatureExtractor::FeatureExtractor(float min_confidence)
    : min_confidence_(min_confidence) {}

  void FeatureExtractor::reset() {
    prev_keypoints_.reset();
    prev_sequence_number_ = 0;
  }

  auto FeatureExtractor::select_best_hand(
    const handpose_det::HandPoseDetection* detections,
    std::uint32_t count) const
    -> const handpose_det::HandPoseDetection*
  {
    return select_best_hand_impl(detections, count, min_confidence_);
  }

  auto FeatureExtractor::compute_bounding_box_scale(
    const std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>& keypoints) const
    -> float
  {
    return compute_bounding_box_scale_impl(keypoints);
  }

auto FeatureExtractor::compute_velocity_magnitudes(
  const std::array<handpose_det::HandPoseKeypoint, handpose_det::kHandPoseKeypointCount>& current,
  float scale) const
  -> std::array<float, handpose_det::kHandPoseKeypointCount>
{
  auto velocities = std::array<float, handpose_det::kHandPoseKeypointCount>{};

  if (!prev_keypoints_.has_value()) {
    return velocities;
  }

  const auto& prev = prev_keypoints_.value();
  for (std::size_t i = 0; i < current.size(); ++i) {
    const auto dx = (current[i].x - prev[i].x) / scale;
    const auto dy = (current[i].y - prev[i].y) / scale;
    velocities[i] = std::sqrt(dx * dx + dy * dy);
  }

  return velocities;
}

auto FeatureExtractor::extract(
  const handpose_det::HandPoseFrameMetadata& metadata,
  const handpose_det::HandPoseDetection* detections,
  std::uint32_t detection_count)
  -> std::optional<FeatureVector>
{
  const auto* best_hand = select_best_hand(detections, detection_count);
  if (best_hand == nullptr) {
    return std::nullopt;
  }

  const auto& keypoints = best_hand->keypoints;
  const auto& wrist = keypoints[0];
  const auto scale = compute_bounding_box_scale(keypoints);

  const auto sequence_continuous =
    prev_keypoints_.has_value() &&
    (metadata.source_sequence_number == prev_sequence_number_ + 1);

  const auto velocities = sequence_continuous
    ? compute_velocity_magnitudes(keypoints, scale)
    : std::array<float, handpose_det::kHandPoseKeypointCount>{};

  auto feature = FeatureVector{};
  feature.source_sequence_number = metadata.source_sequence_number;
  feature.timestamp_ns = metadata.timestamp_ns;
  feature.source_confidence = best_hand->confidence;

  for (std::size_t i = 0; i < keypoints.size(); ++i) {
    feature.features[i].normalized_x = (keypoints[i].x - wrist.x) / scale;
    feature.features[i].normalized_y = (keypoints[i].y - wrist.y) / scale;
    feature.features[i].velocity_magnitude = velocities[i];
  }

  if (sequence_continuous) {
    prev_keypoints_ = keypoints;
  } else {
    prev_keypoints_.reset();
  }
  prev_sequence_number_ = metadata.source_sequence_number;

  return feature;
}

} // namespace signlang::signlang_det
