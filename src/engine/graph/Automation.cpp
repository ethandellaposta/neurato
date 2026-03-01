#include "Automation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace ampl {

// AutomationLane implementation
AutomationLane::AutomationLane() {
}

void AutomationLane::addPoint(const AutomationPoint& point) {
    // Find insertion position (maintain sorted order)
    auto it = std::lower_bound(points_.begin(), points_.end(), point);

    // Check if point already exists at this position
    if (it != points_.end() && it->position == point.position) {
        // Update existing point
        *it = point;
    } else {
        // Insert new point
        points_.insert(it, point);
    }
}

void AutomationLane::removePoint(SampleCount position) {
    auto it = findPointAt(position);
    if (it != points_.end()) {
        points_.erase(it);
    }
}

void AutomationLane::removePointsInRange(SampleCount start, SampleCount end) {
    auto startIt = std::lower_bound(points_.begin(), points_.end(),
                                   AutomationPoint{start, 0.0f});
    auto endIt = std::upper_bound(points_.begin(), points_.end(),
                                 AutomationPoint{end, 0.0f});

    points_.erase(startIt, endIt);
}

void AutomationLane::clear() {
    points_.clear();
}

float AutomationLane::getValueAt(SampleCount position) const {
    if (points_.empty()) return 0.0f;

    // Find exact point
    auto it = findPointAt(position);
    if (it != points_.end()) {
        return it->value;
    }

    // Interpolate between surrounding points
    return getInterpolatedValue(position);
}

float AutomationLane::getInterpolatedValue(SampleCount position) const {
    if (points_.empty()) return 0.0f;

    if (points_.size() == 1) {
        return points_[0].value;
    }

    // Find surrounding points
    auto beforeIt = findPointBefore(position);
    auto afterIt = findPointAfter(position);

    // Handle edge cases
    if (beforeIt == points_.end()) {
        // Position is before first point
        return points_.front().value;
    }

    if (afterIt == points_.end()) {
        // Position is after last point
        return points_.back().value;
    }

    // Interpolate
    if (beforeIt->curve == 0.0f) {
        return linearInterpolate(*beforeIt, *afterIt, position);
    } else {
        return curveInterpolate(*beforeIt, *afterIt, position);
    }
}

void AutomationLane::moveRange(SampleCount start, SampleCount end, SampleCount offset) {
    for (auto& point : points_) {
        if (point.position >= start && point.position <= end) {
            point.position += offset;
        }
    }

    // Re-sort points
    std::sort(points_.begin(), points_.end());
}

void AutomationLane::scaleRange(SampleCount start, SampleCount end, float scaleFactor) {
    for (auto& point : points_) {
        if (point.position >= start && point.position <= end) {
            point.value *= scaleFactor;
        }
    }
}

void AutomationLane::offsetRange(SampleCount start, SampleCount end, float offset) {
    for (auto& point : points_) {
        if (point.position >= start && point.position <= end) {
            point.value += offset;
        }
    }
}

SampleCount AutomationLane::getStart() const {
    return points_.empty() ? 0 : points_.front().position;
}

SampleCount AutomationLane::getEnd() const {
    return points_.empty() ? 0 : points_.back().position;
}

float AutomationLane::getMinValue() const {
    if (points_.empty()) return 0.0f;

    auto minIt = std::min_element(points_.begin(), points_.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.value < b.value;
        });

    return minIt->value;
}

float AutomationLane::getMaxValue() const {
    if (points_.empty()) return 0.0f;

    auto maxIt = std::max_element(points_.begin(), points_.end(),
        [](const AutomationPoint& a, const AutomationPoint& b) {
            return a.value < b.value;
        });

    return maxIt->value;
}

float AutomationLane::linearInterpolate(const AutomationPoint& p1, const AutomationPoint& p2,
                                       SampleCount position) const {
    if (p2.position == p1.position) return p1.value;

    float t = static_cast<float>(position - p1.position) /
              static_cast<float>(p2.position - p1.position);

    return p1.value + t * (p2.value - p1.value);
}

float AutomationLane::curveInterpolate(const AutomationPoint& p1, const AutomationPoint& p2,
                                      SampleCount position) const {
    if (p2.position == p1.position) return p1.value;

    float t = static_cast<float>(position - p1.position) /
              static_cast<float>(p2.position - p1.position);

    // Apply curve function
    if (p1.curve > 0.0f) {
        // Exponential curve
        t = std::pow(t, 1.0f + p1.curve);
    } else if (p1.curve < 0.0f) {
        // Logarithmic curve
        t = 1.0f - std::pow(1.0f - t, 1.0f - p1.curve);
    }

    return p1.value + t * (p2.value - p1.value);
}

std::vector<AutomationPoint>::const_iterator AutomationLane::findPointAt(SampleCount position) const {
    auto it = std::lower_bound(points_.begin(), points_.end(),
                              AutomationPoint{position, 0.0f});

    if (it != points_.end() && it->position == position) {
        return it;
    }

    return points_.end();
}

std::vector<AutomationPoint>::const_iterator AutomationLane::findPointBefore(SampleCount position) const {
    auto it = std::lower_bound(points_.begin(), points_.end(),
                              AutomationPoint{position, 0.0f});

    if (it == points_.begin()) {
        return points_.end();
    }

    return --it;
}

std::vector<AutomationPoint>::const_iterator AutomationLane::findPointAfter(SampleCount position) const {
    return std::upper_bound(points_.begin(), points_.end(),
                           AutomationPoint{position, 0.0f});
}

// AutomationManager implementation
AutomationManager::AutomationManager() {
}

void AutomationManager::addLane(const std::string& parameterId, std::shared_ptr<AutomationLane> lane) {
    std::lock_guard<std::mutex> lock(lanesMutex_);
    lanes_[parameterId] = lane;
}

void AutomationManager::removeLane(const std::string& parameterId) {
    std::lock_guard<std::mutex> lock(lanesMutex_);
    lanes_.erase(parameterId);
}

std::shared_ptr<AutomationLane> AutomationManager::getLane(const std::string& parameterId) {
    std::lock_guard<std::mutex> lock(lanesMutex_);
    auto it = lanes_.find(parameterId);
    return (it != lanes_.end()) ? it->second : nullptr;
}

bool AutomationManager::hasLane(const std::string& parameterId) const {
    std::lock_guard<std::mutex> lock(lanesMutex_);
    return lanes_.find(parameterId) != lanes_.end();
}

float AutomationManager::getParameterValue(const std::string& parameterId, SampleCount position) const {
    std::lock_guard<std::mutex> lock(lanesMutex_);

    auto it = lanes_.find(parameterId);
    if (it != lanes_.end() && it->second) {
        return it->second->getValueAt(position);
    }

    return 0.0f;
}

void AutomationManager::clear() {
    std::lock_guard<std::mutex> lock(lanesMutex_);
    lanes_.clear();
}

void AutomationManager::clearRange(SampleCount start, SampleCount end) {
    std::lock_guard<std::mutex> lock(lanesMutex_);

    for (auto& pair : lanes_) {
        if (pair.second) {
            pair.second->removePointsInRange(start, end);
        }
    }
}

AutomationManager::AutomationData AutomationManager::getData() const {
    std::lock_guard<std::mutex> lock(lanesMutex_);

    AutomationData automationData;
    for (const auto& pair : lanes_) {
        if (pair.second) {
            automationData.lanes[pair.first] = pair.second->getData();
        }
    }

    return automationData;
}

void AutomationManager::setData(const AutomationData& automationData) {
    std::lock_guard<std::mutex> lock(lanesMutex_);

    lanes_.clear();

    for (const auto& pair : automationData.lanes) {
        auto lane = std::make_shared<AutomationLane>();
        lane->setData(pair.second);
        lanes_[pair.first] = lane;
    }
}

// Command implementations
AddAutomationPointCommand::AddAutomationPointCommand(const std::string& paramId,
                                                   const AutomationPoint& point)
    : parameterId_(paramId), point_(point) {
}

void AddAutomationPointCommand::execute() {
    // This would need access to the automation manager
    // Implementation would depend on the command system integration
    wasAdded_ = true;
}

void AddAutomationPointCommand::undo() {
    if (wasAdded_) {
        // Remove the point
        wasAdded_ = false;
    }
}

RemoveAutomationPointCommand::RemoveAutomationPointCommand(const std::string& paramId,
                                                          SampleCount position)
    : parameterId_(paramId), position_(position) {
}

void RemoveAutomationPointCommand::execute() {
    // Store the point before removing for undo
    wasRemoved_ = true;
}

void RemoveAutomationPointCommand::undo() {
    if (wasRemoved_) {
        // Restore the point
        wasRemoved_ = false;
    }
}

MoveAutomationPointCommand::MoveAutomationPointCommand(const std::string& paramId,
                                                      SampleCount oldPos, SampleCount newPos,
                                                      float oldValue, float newValue)
    : parameterId_(paramId), oldPosition_(oldPos), newPosition_(newPos),
      oldValue_(oldValue), newValue_(newValue) {
}

void MoveAutomationPointCommand::execute() {
    wasMoved_ = true;
}

void MoveAutomationPointCommand::undo() {
    if (wasMoved_) {
        // Move point back to original position
        wasMoved_ = false;
    }
}

} // namespace ampl
