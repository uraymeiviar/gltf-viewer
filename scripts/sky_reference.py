# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A6.2 reference implementation of Glider's procedural sky model in float64 (numpy).

The model is Hillaire 2020 ("A Scalable and Production Ready Sky and Atmosphere Rendering Technique", EGSR 2020)
with Bruneton 2017's Earth atmosphere, exactly as header/imgui/SkyAtmosphere.h states it: single scattering of a white
top-of-atmosphere sun (Rayleigh phase, Cornette-Shanks Mie phase g = 0.8), the multiple-scattering term Psi_ms
(isotropic second order over the sphere of directions with the geometric series 1 / (1 - f_ms)), a Lambert ground of a
given albedo, observer 50 m above the ground, radiance scaled by the top-of-atmosphere solar illuminance (10 environment
units = 128 klux).

It shares no code and no lookup table with the GPU passes: transmittance is integrated directly along every ray (no
256 x 64 LUT), single scattering uses thousands of steps instead of 48, the sky is evaluated per direction (no 192 x 108
sky-view LUT) and Psi_ms is tabulated at 64 x 64 from 256 directions and 96 steps (the GPU: 32 x 32, 64 directions,
32 steps). What it checks is therefore the GPU implementation, its parameterisations and its resolution, against the
model itself. scripts/validate_sky.py freezes values computed here (--regenerate-references prints them).
"""

import math

import numpy as np

BOTTOM, TOP = 6360.0, 6460.0
OBSERVER = 0.05
RAYLEIGH = np.array([5.802e-3, 13.558e-3, 33.1e-3])
RAYLEIGH_H = 8.0
MIE_SCATTERING, MIE_EXTINCTION, MIE_H, MIE_G = 3.996e-3, 4.440e-3, 1.2, 0.8
OZONE = np.array([0.650e-3, 1.881e-3, 0.085e-3])
OZONE_PEAK, OZONE_HALF = 25.0, 15.0
SOLAR = 10.0
LUMA = np.array([0.2126, 0.7152, 0.0722])


class Atmosphere:
    def __init__(self, mie=1.0, ozone=True, albedo=0.3):
        self.mie = mie
        self.ozone = ozone
        self.albedo = np.broadcast_to(np.asarray(albedo, dtype=np.float64), (3,)).copy()
        self.psi_table = None

    # ---- medium ----
    def medium(self, h):
        h = np.maximum(np.asarray(h, dtype=np.float64), 0.0)[..., None]
        rayleigh = RAYLEIGH * np.exp(-h / RAYLEIGH_H)
        mie_density = np.exp(-h / MIE_H) * self.mie
        mie = MIE_SCATTERING * mie_density * np.ones(3)
        ozone = OZONE * np.maximum(0.0, 1.0 - np.abs(h - OZONE_PEAK) / OZONE_HALF) if self.ozone else 0.0
        extinction = rayleigh + MIE_EXTINCTION * mie_density + ozone
        return rayleigh, mie, extinction

    # ---- geometry ----
    @staticmethod
    def hits_ground(r, mu):
        h = np.maximum(r - BOTTOM, 0.0)
        return (mu < 0.0) & (r * r * mu * mu - h * (2.0 * BOTTOM + h) >= 0.0)

    @staticmethod
    def distance_top(r, mu):
        return np.maximum(-r * mu + np.sqrt(np.maximum(r * r * (mu * mu - 1.0) + TOP * TOP, 0.0)), 0.0)

    @staticmethod
    def distance_bottom(r, mu):
        h = np.maximum(r - BOTTOM, 0.0)
        return np.maximum(-r * mu - np.sqrt(np.maximum(r * r * mu * mu - h * (2.0 * BOTTOM + h), 0.0)), 0.0)

    @staticmethod
    def along(r0, mu0, t):
        h0 = np.maximum(r0 - BOTTOM, 0.0)
        r = np.sqrt(np.maximum(r0 * r0 + t * t + 2.0 * r0 * mu0 * t, 0.0))
        h = (h0 * (2.0 * BOTTOM + h0) + t * t + 2.0 * r0 * mu0 * t) / (r + BOTTOM)
        return np.maximum(h, 0.0)

    # ---- transmittance to the top of the atmosphere, integrated directly (0 where the ray hits the ground) ----
    def transmittance(self, r, mu, steps=256, chunk=8192):
        r = np.asarray(r, dtype=np.float64).ravel()
        mu = np.clip(np.asarray(mu, dtype=np.float64).ravel(), -1.0, 1.0)
        out = np.zeros((r.size, 3))
        for start in range(0, r.size, chunk):
            rr, mm = r[start:start + chunk], mu[start:start + chunk]
            length = self.distance_top(rr, mm)
            t = (np.arange(steps) + 0.5)[None, :] / steps * length[:, None]
            h = self.along(rr[:, None], mm[:, None], t)
            _, _, extinction = self.medium(h)
            depth = extinction.sum(axis=1) * (length / steps)[:, None]
            value = np.exp(-depth)
            value[self.hits_ground(rr, mm)] = 0.0
            out[start:start + chunk] = value
        return out

    # ---- transmittance table (used only to build Psi_ms) ----
    def _transmittance_table(self, nh=128, nmu=512):
        xs = (np.arange(nh) + 0.5) / nh
        heights = (TOP - BOTTOM) * xs * xs
        mus = np.linspace(-1.0, 1.0, nmu)
        hh, mm = np.meshgrid(heights, mus, indexing="ij")
        table = self.transmittance(BOTTOM + hh, mm, steps=192).reshape(nh, nmu, 3)
        return heights, mus, table

    def _table_lookup(self, heights, mus, table, h, mu):
        x = np.sqrt(np.clip(h / (TOP - BOTTOM), 0.0, 1.0)) * len(heights) - 0.5
        y = (np.clip(mu, -1.0, 1.0) + 1.0) * 0.5 * (len(mus) - 1)
        x0 = np.clip(np.floor(x).astype(int), 0, len(heights) - 2)
        y0 = np.clip(np.floor(y).astype(int), 0, len(mus) - 2)
        fx = np.clip(x - x0, 0.0, 1.0)[..., None]
        fy = np.clip(y - y0, 0.0, 1.0)[..., None]
        return ((table[x0, y0] * (1 - fx) + table[x0 + 1, y0] * fx) * (1 - fy) +
                (table[x0, y0 + 1] * (1 - fx) + table[x0 + 1, y0 + 1] * fx) * fy)

    # ---- multiple scattering Psi_ms(height, sun zenith cosine), Hillaire eq. 5-10 ----
    def build_psi(self, n=64, sqrt_dirs=16, steps=96):
        t_heights, t_mus, ttable = self._transmittance_table()
        u = (np.arange(sqrt_dirs) + 0.5) / sqrt_dirs
        theta = 2.0 * math.pi * u
        phi = np.arccos(1.0 - 2.0 * u)
        th, ph = np.meshgrid(theta, phi, indexing="ij")
        dirs = np.stack([np.cos(th) * np.sin(ph), np.cos(ph), np.sin(th) * np.sin(ph)], -1).reshape(-1, 3)
        psi = np.zeros((n, n, 3))
        sun_cos_values = np.linspace(-1.0, 1.0, n)
        height_values = np.linspace(0.0, TOP - BOTTOM, n)
        height_values[0] = 1.0e-3
        for j, height in enumerate(height_values):
            r0 = BOTTOM + height
            for i, sun_cos in enumerate(sun_cos_values):
                sun = np.array([math.sqrt(max(0.0, 1.0 - sun_cos * sun_cos)), sun_cos, 0.0])
                mu0 = dirs[:, 1]
                nu = dirs @ sun
                ground = self.hits_ground(r0, mu0)
                tmax = np.where(ground, self.distance_bottom(r0, mu0), self.distance_top(r0, mu0))
                a = (np.arange(steps) + 1.0) / steps
                edges = np.concatenate([np.zeros((dirs.shape[0], 1)), tmax[:, None] * a[None, :]], axis=1)
                dt = np.diff(edges, axis=1)
                t = edges[:, :-1] + 0.5 * dt
                h = self.along(r0, mu0[:, None], t)
                r = BOTTOM + h
                mus = np.clip((r0 * sun_cos + t * nu[:, None]) / r, -1.0, 1.0)
                rayleigh, mie, extinction = self.medium(h)
                scattering = rayleigh + mie
                horizon = -np.sqrt(np.maximum(h * (2.0 * BOTTOM + h), 0.0)) / r
                visible = (mus >= horizon)[..., None]
                sun_t = self._table_lookup(t_heights, t_mus, ttable, h, mus) * visible
                seg = np.exp(-extinction * dt[..., None])
                through = np.cumprod(np.concatenate([np.ones((dirs.shape[0], 1, 3)), seg[:, :-1]], axis=1), axis=1)
                ext = np.maximum(extinction, 1e-12)
                source = sun_t * scattering / (4.0 * math.pi)
                L = (through * (source - source * seg) / ext).sum(axis=1)
                fms = (through * (scattering - scattering * seg) / ext).sum(axis=1)
                end_through = through[:, -1] * seg[:, -1]
                ground_mus = np.clip((r0 * sun_cos + tmax * nu) / BOTTOM, -1.0, 1.0)
                ground_t = self._table_lookup(t_heights, t_mus, ttable, np.zeros_like(ground_mus), ground_mus)
                L += np.where(ground[:, None], end_through * ground_t * np.clip(ground_mus, 0.0, 1.0)[:, None] *
                              self.albedo / math.pi, 0.0)
                second = L.mean(axis=0)
                transfer = fms.mean(axis=0)
                psi[i, j] = second / np.maximum(1.0 - transfer, 1e-4)
        self.psi_table = (sun_cos_values, height_values, psi)
        return self.psi_table

    def psi(self, h, sun_cos):
        sun_cos_values, height_values, table = self.psi_table
        x = (np.clip(sun_cos, -1.0, 1.0) + 1.0) * 0.5 * (len(sun_cos_values) - 1)
        y = np.clip(h, 0.0, TOP - BOTTOM) / (TOP - BOTTOM) * (len(height_values) - 1)
        x0 = np.clip(np.floor(x).astype(int), 0, len(sun_cos_values) - 2)
        y0 = np.clip(np.floor(y).astype(int), 0, len(height_values) - 2)
        fx = np.clip(x - x0, 0.0, 1.0)[..., None]
        fy = np.clip(y - y0, 0.0, 1.0)[..., None]
        return ((table[x0, y0] * (1 - fx) + table[x0 + 1, y0] * fx) * (1 - fy) +
                (table[x0, y0 + 1] * (1 - fx) + table[x0 + 1, y0 + 1] * fx) * fy)

    # ---- sky radiance seen from the observer ----
    def radiance(self, view, sun, steps=2000, sun_steps=192):
        """Linear RGB radiance in environment units for world directions view (unit, +Y up) and sun (unit)."""
        if self.psi_table is None:
            self.build_psi()
        view = np.asarray(view, dtype=np.float64)
        sun = np.asarray(sun, dtype=np.float64)
        r0 = BOTTOM + OBSERVER
        mu0 = float(np.clip(view[1], -1.0, 1.0))
        sun_cos0 = float(np.clip(sun[1], -1.0, 1.0))
        nu = float(np.clip(view @ sun, -1.0, 1.0))
        ground = bool(self.hits_ground(np.array(r0), np.array(mu0)))
        tmax = float(self.distance_bottom(r0, mu0) if ground else self.distance_top(r0, mu0))
        a = (np.arange(steps) + 1.0) / steps
        edges = np.concatenate([[0.0], tmax * a * a])
        dt = np.diff(edges)
        t = edges[:-1] + 0.5 * dt
        h = self.along(r0, mu0, t)
        r = BOTTOM + h
        mus = np.clip((r0 * sun_cos0 + t * nu) / r, -1.0, 1.0)
        rayleigh, mie, extinction = self.medium(h)
        horizon = -np.sqrt(np.maximum(h * (2.0 * BOTTOM + h), 0.0)) / r
        visible = (mus >= horizon)[:, None]
        sun_t = self.transmittance(r, mus, steps=sun_steps) * visible
        phase_r = 3.0 / (16.0 * math.pi) * (1.0 + nu * nu)
        g = MIE_G
        phase_m = 3.0 / (8.0 * math.pi) * (1 - g * g) / (2 + g * g) * (1 + nu * nu) / (1 + g * g - 2 * g * nu) ** 1.5
        source = sun_t * (rayleigh * phase_r + mie * phase_m) + self.psi(h, mus) * (rayleigh + mie)
        seg = np.exp(-extinction * dt[:, None])
        through = np.cumprod(np.concatenate([np.ones((1, 3)), seg[:-1]], axis=0), axis=0)
        L = (through * (source - source * seg) / np.maximum(extinction, 1e-12)).sum(axis=0)
        if ground:
            ground_mus = float(np.clip((r0 * sun_cos0 + tmax * nu) / BOTTOM, -1.0, 1.0))
            ground_t = self.transmittance(np.array([BOTTOM + 1e-9]), np.array([ground_mus]), steps=sun_steps)[0]
            L += through[-1] * seg[-1] * ground_t * max(ground_mus, 0.0) * self.albedo / math.pi
        return L * SOLAR

    def sun_transmittance(self, sun):
        sun = np.asarray(sun, dtype=np.float64)
        return self.transmittance(np.array([BOTTOM + OBSERVER]), np.array([sun[1]]), steps=4096)[0]

    def cosine_mean(self, normal, sun, n_theta=36, n_phi=72, steps=400):
        """Cosine-weighted mean radiance over the hemisphere around `normal` (= irradiance / pi): what the Lambertian
        IBL cube holds and a white Lambert surface reflects."""
        normal = np.asarray(normal, dtype=np.float64)
        normal = normal / np.linalg.norm(normal)
        helper = np.array([0.0, 1.0, 0.0]) if abs(normal[1]) < 0.9 else np.array([1.0, 0.0, 0.0])
        tangent = np.cross(helper, normal)
        tangent /= np.linalg.norm(tangent)
        bitangent = np.cross(normal, tangent)
        total = np.zeros(3)
        weight = 0.0
        for i in range(n_theta):
            # cos(theta) uniform in sin^2: cosine-weighted strata
            u = (i + 0.5) / n_theta
            cos_t = math.sqrt(1.0 - u)
            sin_t = math.sqrt(u)
            for j in range(n_phi):
                p = 2.0 * math.pi * (j + 0.5) / n_phi
                d = normal * cos_t + (tangent * math.cos(p) + bitangent * math.sin(p)) * sin_t
                total += self.radiance(d, sun, steps=steps, sun_steps=96)
                weight += 1.0
        return total / weight


def direction(azimuth, elevation):
    """A3.1 convention: azimuth around +Y from +Z towards +X, elevation above the horizon."""
    az, el = math.radians(azimuth), math.radians(elevation)
    return np.array([math.cos(el) * math.sin(az), math.sin(el), math.cos(el) * math.cos(az)])
