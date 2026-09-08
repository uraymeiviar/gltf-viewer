#pragma once

#include "animation/IAnimationTarget.h"
#include "math/VectorTypes.h"

#include <functional>
#include <variant>
#include <vector>

namespace ux3d::renderer {
class KHRAnimationPointerTarget final : public animation::IAnimationTarget<float>,
                                        public animation::IAnimationTarget<glm::vec2>,
                                        public animation::IAnimationTarget<glm::vec3>,
                                        public animation::IAnimationTarget<glm::vec4>,
                                        public animation::IAnimationTarget<glm::quat>,
                                        public animation::IAnimationTarget<std::vector<float>> {
public:
    template <typename T>
    using Callback = std::function<void(const T&)>;

    ~KHRAnimationPointerTarget() override = default;

    void update(const float& value) override
    {
        update_inner(value);
    }

    void update(const glm::vec2& value) override
    {
        update_inner(value);
    }

    void update(const glm::vec3& value) override
    {
        update_inner(value);
    }

    void update(const glm::vec4& value) override
    {
        update_inner(value);
    }

    void update(const glm::quat& value) override
    {
        update_inner(value);
    }

    void update(const std::vector<float>& value) override
    {
        update_inner(value);
    }

    template <typename T>
    void update_inner(const T& value)
    {
        if (std::holds_alternative<Callback<T>>(callback)) {
            if (auto cb = std::get<Callback<T>>(callback); cb != nullptr) {
                cb(value);
            }
        }
    }

    std::variant<
        Callback<float>,
        Callback<glm::vec2>,
        Callback<glm::vec3>,
        Callback<glm::vec4>,
        Callback<glm::quat>,
        Callback<std::vector<float>>>
        callback;
};
}
