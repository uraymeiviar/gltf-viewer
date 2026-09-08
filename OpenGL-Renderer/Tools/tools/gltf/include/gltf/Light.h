// Copyright (c) 2019 UX3D GmbH. All rights reserved.

#pragma once

#include "ChildOfRoot.h"
#include "gltf/GltfObject.h"
#include "math/VectorTypes.h"
#include <optional>

//See https://github.com/ux3d/glTF/tree/KHR_materials_pbrClearcoat/extensions/2.0/Khronos/KHR_lights_punctual#spot

namespace ux3d::gltf {
class Light : public ChildOfRoot {
public:
    enum class Type : uint32_t {
        Point,
        Spot,
        Directional
    };

    class Spot : public GltfObject {
    private:
        std::optional<double> m_innerConeAngle;
        std::optional<double> m_outerConeAngle;

    public:
        explicit Spot() = default;
        Spot(const Spot&) = default;
        Spot(Spot&&) = default;

        bool hasInnerConeAngle() const { return m_innerConeAngle.has_value(); }
        bool hasOuterConeAngle() const { return m_outerConeAngle.has_value(); }
        double getInnerConeAngleOrDefault() const { return m_innerConeAngle.value_or(0); }
        double getOuterConeAngleOrDefault() const { return m_outerConeAngle.value_or(0.7853981634); }
        void unsetInnerConeAngle() { m_innerConeAngle.reset(); }
        void unsetOuterConeAngle() { m_outerConeAngle.reset(); }
        GltfResult setInnerConeAngle(double _innerConeAngle);
        GltfResult setOuterConeAngle(double _outerConeAngle);
    };

private:
    std::optional<math::dvec3> m_color;
    std::optional<double> m_intensity;
    std::optional<double> m_range;
    std::optional<Spot> m_spot;
    Type m_type;

public:
    Light(const Light&) = delete;
    Light(Light&&) = delete;

    Light(uint32_t _id, Type _type); // regular constructor
    Light(uint32_t _id, const Light& _source); //  duplicate in same glTF

    GltfType getGltfType() const override { return GltfType::Light; }

    GltfResult setColor(const math::dvec3& _color);
    GltfResult setIntensity(double _intensity);
    GltfResult setRange(double _range);
    GltfResult setType(const Type& _type);

    bool hasColor() const { return m_color.has_value(); }
    math::dvec3 getColorOrDefault() const { return m_color.value_or(math::dvec3(1)); }
    bool hasIntensity() const { return m_intensity.has_value(); }
    double getIntensityOrDefault() const { return m_intensity.value_or(1); }
    Type getType() const { return m_type; }
    bool hasRange() const { return m_range.has_value(); }
    double getRangeOrDefault() const { return m_range.value_or(-1); }

    void unsetColor() { m_color.reset(); }
    void unsetIntensity() { m_intensity.reset(); }
    void unsetRange() { m_range.reset(); }

    std::optional<Spot>& getSpot() { return m_spot; }
    const std::optional<Spot>& getSpot() const { return m_spot; }
    void destroySpot();
    void createSpot();

    Extensions getUsedExtensions() const override { return Extension::PunctualLights; }

    bool isValid() const override;
    GltfResult getInvalidProperties(core::Vector<core::string>& _invalidProperties) const override;
};
}
