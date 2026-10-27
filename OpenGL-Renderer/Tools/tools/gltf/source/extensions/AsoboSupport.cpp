// Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.

#include "gltf/extensions/AsoboSupport.h"

namespace ux3d::gltf::asobo {

const std::vector<SupportInfo>& supportTable()
{
    static const std::vector<SupportInfo> table = {
        {"ASOBO_animation_retargeting", Support::Metadata, "A7", ""},
        {"ASOBO_asset_optimized", Support::Rendered, "A1.9", "packaged vertex decoding and bounds-checked ASOBO_primitive triangle index slices", false},
        {"ASOBO_collision_object", Support::NotYetSupported, "A7", ""},
        {"ASOBO_extra_occlusion", Support::NotYetSupported, "A1", ""},
        {"ASOBO_extra_occlusion_texture", Support::NotYetSupported, "A1", ""},
        {"ASOBO_gizmo_object", Support::NotYetSupported, "A7", ""},
        {"ASOBO_macro_light", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_SSS", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_UV_options", Support::NotYetSupported, "A5", ""},
        {"ASOBO_material_alphamode_dither", Support::NotYetSupported, "A1", ""},
        {"ASOBO_material_anisotropic", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_anisotropic_v2", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_antialiasing_options", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_blend_gbuffer", Support::Partial, "A2.2", "default mode; blendMasked/frosted and decalChan in A2.3"},
        {"ASOBO_material_clear_coat", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_clear_coat_v2", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_code", Support::Partial, "A4", "extras Glass/Windshield type fallback; Porthole is parallax and is not glass"},
        {"ASOBO_material_detail_map", Support::Rendered, "A5.1", "detail colour, ORM and normal over UVScale; vertex-alpha or blend-mask mode; object-attribute scale ignored; detail AO needs a core occlusion map"},
        {"ASOBO_material_disable_motion_blur", Support::NotApplicable, "A7", "Glider has no motion blur"},
        {"ASOBO_material_draw_order", Support::Rendered, "A1.4", "decals, glass and windshield sort by drawOrderOffset"},
        {"ASOBO_material_emissive", Support::Rendered, "A1", "day/night multipliers; MSFS emissive scale"},
        {"ASOBO_material_environment_occluder", Support::Rendered, "A2.4", "hidden: Glider draws no weather or vegetation to occlude; switch off to draw it"},
        {"ASOBO_material_fake_terrain", Support::NotApplicable, "A2.4", "Glider has no terrain; drawn as plain glTF and receives decals"},
        {"ASOBO_material_fresnel_fade", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_geometry_decal", Support::Partial, "A2.2", "default mode; blendMasked/frosted and decalChan in A2.3"},
        {"ASOBO_material_ghost_effect", Support::NotYetSupported, "A4", ""},
        {"ASOBO_material_glass_v2", Support::Partial, "A4", "opacity-independent GGX reflection and screen-space slab refraction; width units inferred; no glass behind glass"},
        {"ASOBO_material_glass", Support::Partial, "A4", "MSFS 2020 GGX reflection scaled by glassReflectionMaskFactor; zero slab width; baked-image deformation unavailable"},
        {"ASOBO_material_kitty_glass", Support::Partial, "A4", "legacy glass alias inferred from packaged assets; reflection mask; zero slab width"},
        {"ASOBO_material_invisible", Support::Rendered, "A1.1", "hidden; switch off in Viewport Settings > MSFS Extensions"},
        {"ASOBO_material_iridescent", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_parallax_window", Support::NotYetSupported, "A5", ""},
        {"ASOBO_material_pearlescent", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_sail", Support::NotYetSupported, "A7", ""},
        {"ASOBO_material_shadow_options", Support::NotYetSupported, "A3", ""},
        {"ASOBO_material_windshield", Support::Partial, "A4.2", "MSFS 2020 reflection/SSR, lit scratch/fingerprint scattering, detail reflection, secondary details, icing; wipers and rain out of scope"},
        {"ASOBO_material_windshield_v3", Support::Partial, "A4.2", "reflection/SSR, lit scratch/fingerprint scattering, detail reflection, secondary details, micro-scratches, icing; wipers, insects, rain and refraction out of scope"},
        {"ASOBO_normal_map_convention", Support::Rendered, "A1.2", "DirectX convention flips normal-map green"},
        {"ASOBO_occlusion_strength", Support::Rendered, "A1.5", "Replaces the core occlusion strength (core operator; >1 deepens AO, clamped at 0)"},
        {"ASOBO_property_animation", Support::NotYetSupported, "A7", ""},
        {"ASOBO_softbody_mesh", Support::Metadata, "A7", ""},
        {"ASOBO_tags", Support::Metadata, "A7", ""},
        {"ASOBO_unique_id", Support::Metadata, "A7", ""},
        {"MSFT_texture_dds", Support::Rendered, "A1.8", "DDS image preferred over core source with automatic fallback; always read when declared", false},
    };
    return table;
}

SupportInfo supportOf(std::string_view extensionName)
{
    for (const SupportInfo& info : supportTable()) {
        if (info.name == extensionName) {
            return info;
        }
    }
    return {extensionName, Support::NotYetSupported, "", ""};
}

const char* supportLabel(Support status)
{
    switch (status) {
        case Support::Rendered: return "Rendered";
        case Support::Partial: return "Partial";
        case Support::ViewerSetting: return "Viewer setting";
        case Support::Metadata: return "Metadata";
        case Support::NotApplicable: return "Not applicable";
        case Support::NotYetSupported: return "Not yet supported";
    }
    return "Not yet supported";
}

} // namespace ux3d::gltf::asobo
