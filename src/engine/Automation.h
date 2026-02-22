#pragma once

#include "util/Types.h"
#include <vector>
#include <memory>
#include <map>
#include <algorithm>

namespace neurato {

// Automation breakpoint with sample-accurate timing
struct AutomationPoint {
    SampleCount position{0};
    float value{0.0f};
    float curve{0.0f};  // 0.0 = linear, >0 = exponential, <0 = logarithmic
    
    bool operator<(const AutomationPoint& other) const {
        return position < other.position;
    }
};

// Automation lane containing points and interpolation logic
class AutomationLane {
public:
    AutomationLane();
    ~AutomationLane() = default;
    
    // Point management
    void addPoint(const AutomationPoint& point);
    void removePoint(SampleCount position);
    void removePointsInRange(SampleCount start, SampleCount end);
    void clear();
    
    // Value retrieval with sample-accurate interpolation
    float getValueAt(SampleCount position) const;
    float getInterpolatedValue(SampleCount position) const;
    
    // Point access
    const std::vector<AutomationPoint>& getPoints() const { return points_; }
    std::vector<AutomationPoint>& getPoints() { return points_; }
    
    // Range operations
    void moveRange(SampleCount start, SampleCount end, SampleCount offset);
    void scaleRange(SampleCount start, SampleCount end, float scaleFactor);
    void offsetRange(SampleCount start, SampleCount end, float offset);
    
    // Utility
    bool isEmpty() const { return points_.empty(); }
    SampleCount getStart() const;
    SampleCount getEnd() const;
    float getMinValue() const;
    float getMaxValue() const;
    
    // Serialization
    struct LaneData {
        std::vector<AutomationPoint> points;
    };
    
    LaneData getData() const { return {points_}; }
    void setData(const LaneData& data) { points_ = data.points; }

private:
    std::vector<AutomationPoint> points_;
    
    // Interpolation methods
    float linearInterpolate(const AutomationPoint& p1, const AutomationPoint& p2, 
                           SampleCount position) const;
    float curveInterpolate(const AutomationPoint& p1, const AutomationPoint& p2, 
                          SampleCount position) const;
    
    // Helper methods
    std::vector<AutomationPoint>::const_iterator findPointAt(SampleCount position) const;
    std::vector<AutomationPoint>::const_iterator findPointBefore(SampleCount position) const;
    std::vector<AutomationPoint>::const_iterator findPointAfter(SampleCount position) const;
};

// Automation manager for handling multiple lanes and parameter mapping
class AutomationManager {
public:
    AutomationManager();
    ~AutomationManager() = default;
    
    // Lane management
    void addLane(const std::string& parameterId, std::shared_ptr<AutomationLane> lane);
    void removeLane(const std::string& parameterId);
    std::shared_ptr<AutomationLane> getLane(const std::string& parameterId);
    bool hasLane(const std::string& parameterId) const;
    
    // Value retrieval
    float getParameterValue(const std::string& parameterId, SampleCount position) const;
    
    // Global operations
    void clear();
    void clearRange(SampleCount start, SampleCount end);
    
    // Serialization
    struct AutomationData {
        std::map<std::string, AutomationLane::LaneData> lanes;
    };
    
    AutomationData getData() const;
    void setData(const AutomationData& data);

private:
    std::map<std::string, std::shared_ptr<AutomationLane>> lanes_;
    mutable std::mutex lanesMutex_;
};

// Automation editor state for UI
struct AutomationEditorState {
    std::string parameterId;
    SampleCount viewStart{0};
    SampleCount viewEnd{44100 * 4};  // 4 bars at 120bpm default
    float minValue{0.0f};
    float maxValue{1.0f};
    bool snapToGrid{true};
    SampleCount gridDivision{44100 / 4};  // 16th notes default
    bool showCurve{true};
    bool isEditing{false};
    
    // Selection state
    std::vector<SampleCount> selectedPoints;
    SampleCount selectionStart{-1};
    SampleCount selectionEnd{-1};
};

// Automation commands for undo/redo
class AddAutomationPointCommand {
public:
    AddAutomationPointCommand(const std::string& paramId, const AutomationPoint& point);
    void execute();
    void undo();

private:
    std::string parameterId_;
    AutomationPoint point_;
    bool wasAdded_{false};
};

class RemoveAutomationPointCommand {
public:
    RemoveAutomationPointCommand(const std::string& paramId, SampleCount position);
    void execute();
    void undo();

private:
    std::string parameterId_;
    SampleCount position_;
    AutomationPoint removedPoint_;
    bool wasRemoved_{false};
};

class MoveAutomationPointCommand {
public:
    MoveAutomationPointCommand(const std::string& paramId, SampleCount oldPos, 
                              SampleCount newPos, float oldValue, float newValue);
    void execute();
    void undo();

private:
    std::string parameterId_;
    SampleCount oldPosition_;
    SampleCount newPosition_;
    float oldValue_;
    float newValue_;
    bool wasMoved_{false};
};

} // namespace neurato
