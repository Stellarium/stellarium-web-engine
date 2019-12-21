/* Stellarium Web Engine - Copyright (c) 2018 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial licence.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

#ifdef GL_ES
precision mediump float;
#endif

uniform highp float u_atm_p[12];
uniform highp vec3  u_sun;
uniform highp float u_tm[3]; // Tonemapping koefs.

varying lowp    vec4        v_color;

#ifdef VERTEX_SHADER

attribute highp   vec4       a_pos;
attribute highp   vec3       a_sky_pos;
attribute highp   float      a_luminance;

highp float gammaf(highp float c)
{
    if (c < 0.0031308)
      return 19.92 * c;
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
}

vec3 xyy_to_srgb(highp vec3 xyy)
{
    const highp mat3 xyz_to_rgb = mat3(3.2406, -0.9689, 0.0557,
                                      -1.5372, 1.8758, -0.2040,
                                      -0.4986, 0.0415, 1.0570);
    highp vec3 xyz = vec3(xyy[0] * xyy[2] / xyy[1], xyy[2],
               (1.0 - xyy[0] - xyy[1]) * xyy[2] / xyy[1]);
    highp vec3 srgb = xyz_to_rgb * xyz;
    clamp(srgb, 0.0, 1.0);
    return srgb;
}

void main()
{
    highp vec3 xyy;
    highp float cos_gamma, cos_gamma2, gamma, cos_theta;
    highp vec3 p = a_sky_pos;

    gl_Position = a_pos;

    // First compute the xy color component (chromaticity) from Preetham model
    // and re-inject a_luminance for Y component (luminance).
    p[2] = abs(p[2]); // Mirror below horizon.
    cos_gamma = dot(p, u_sun);
    cos_gamma2 = cos_gamma * cos_gamma;
    gamma = acos(cos_gamma);
    cos_theta = p[2];

    xyy.x = ((1. + u_atm_p[0] * exp(u_atm_p[1] / cos_theta)) *
             (1. + u_atm_p[2] * exp(u_atm_p[3] * gamma) +
              u_atm_p[4] * cos_gamma2)) * u_atm_p[5];
    xyy.y = ((1. + u_atm_p[6] * exp(u_atm_p[7] / cos_theta)) *
             (1. + u_atm_p[8] * exp(u_atm_p[9] * gamma) +
              u_atm_p[10] * cos_gamma2)) * u_atm_p[11];
    xyy.z = a_luminance;

    // Ad-hoc tuning. Scaling before the blue shift allows to obtain proper
    // blueish colors at sun set instead of very red, which is a shortcoming
    // of preetham model.
    xyy.z *= 0.08;

    // Convert this sky luminance/chromaticity into perceived color using model
    // from Henrik Wann Jensen (2000)
    // We deal with 3 cases:
    //  * Y <= 0.01: scotopic vision. Only the eyes' rods see. No colors,
    //    everything is converted to night blue (xy = 0.25, 0.25)
    //  * Y > 3.981: photopic vision. Only the eyes's cones seew ith full colors
    //  * Y > 0.01 and Y <= 3.981: mesopic vision. Rods and cones see in a
    //    transition state.
    //
    // Compute s, ratio between scotopic and photopic vision
    highp float op = (log(xyy.z) / log(10.) + 2.) / 2.6;
    highp float s = (xyy.z <= 0.01) ? 0.0 : (xyy.z > 3.981) ? 1.0 : op * op * (3. - 2. * op);

    // Perform the blue shift on chromaticity
    xyy.x = mix(0.25, xyy.x, s);
    xyy.y = mix(0.25, xyy.y, s);
    // Scale scotopic luminance for scotopic pixels
    xyy.z = 0.4468 * (1. - s) * xyy.z + s * xyy.z;


    // Convert xyY to 
    highp vec3 rgb = xyy_to_(xyy);

    // Apply gamma correction
    v_color = vec4(gammaf(rgb.r), gammaf(rgb.g),gammaf(rgb.b), 1.0);
}                       constexpr int SCATTERING_TEXTURE_MU_SIZE = 128;
constexpr int SCATTERING_TEXTURE_MU_S_SIZE = 32;
constexpr int SCATTERING_TEXTURE_NU_SIZE = 8;

constexpr int SCATTERING_TEXTURE_WIDTH =
    SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_R_SIZE;
constexpr int SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_MU_S_SIZE;
constexpr int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
constexpr int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_MU_S_SIZE;
constexpr int SCATTERING_TEXTURE_DEPTH =
    SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_R_SIZE;

constexpr int IRRADIANCE_TEXTURE_WIDTH = 64;
constexpr int IRRADIANCE_TEXTURE_HEIGHT = 16;void SaveTexture(const GLenum texture_unit, const GLenum texture_target,
  {
      using namespace atmosphere;
      const std::int32_t header[]={SCATTERING_TEXTURE_MU_S_SIZE, SCATTERING_TEXTURE_MU_SIZE,
                                   SCATTERING_TEXTURE_R_SIZE, SCATTERING_TEXTURE_NU_SIZE,
                                   SCATTERING_TEXTURE_NU_SIZE, SCATTERING_TEXTURE_R_SIZE,
                                   4};
      output_stream.write(reinterpret_cast<const char*>(header), sizeof header);
  }

vec4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
  // Distance to the horizon.
  Length rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);
  Number u_r = rho / H;

  // Discriminant of the quadratic equation for the intersections of the ray
  // (r,mu) with the ground (see RayIntersectsGround).
@@ -821,8 +821,9 @@ vec4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
  Number u_mu_s = GetTextureCoordFromUnitRange(
      max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);

  Number u_nu = (nu + 1.0) / 2.0;
  return vec4(u_nu, u_r, u_mu, u_mu_s);
  Number u_nu = GetTextureCoordFromUnitRange((nu + 1.0) / 2.0,
                                             SCATTERING_TEXTURE_NU_SIZE);
  return vec4(u_nu, u_mu_s, u_mu, u_r);
}

/*
@@ -842,7 +843,7 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
      atmosphere.bottom_radius * atmosphere.bottom_radius);
  // Distance to the horizon.
  Length rho =
      H * GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_R_SIZE);
      H * uvwz.w;
  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);

  if (uvwz.z < 0.5) {
@@ -870,7 +871,7 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
  }

  Number x_mu_s =
      GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_MU_S_SIZE);
      GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
  Length d_max = H;
  Number A =
@@ -880,7 +881,8 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
  mu_s = d == 0.0 * m ? Number(1.0) :
     ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));

  nu = ClampCosine(uvwz.x * 2.0 - 1.0);
  nu = ClampCosine(GetUnitRangeFromTextureCoord(uvwz.x, SCATTERING_TEXTURE_NU_SIZE)
                   * 2.0 - 1.0);
}

/*
@@ -901,16 +903,16 @@ void GetRMuMuSNuFromScatteringTextureFragCoord(
    OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s, OUT(Number) nu,
    OUT(bool) ray_r_mu_intersects_ground) {
  const vec4 SCATTERING_TEXTURE_SIZE = vec4(
      SCATTERING_TEXTURE_NU_SIZE - 1,
      SCATTERING_TEXTURE_R_SIZE,
      SCATTERING_TEXTURE_NU_SIZE,
      SCATTERING_TEXTURE_MU_S_SIZE,
      SCATTERING_TEXTURE_MU_SIZE,
      SCATTERING_TEXTURE_MU_S_SIZE);
  Number frag_coord_nu =
      floor(frag_coord.x / Number(SCATTERING_TEXTURE_R_SIZE));
      SCATTERING_TEXTURE_R_SIZE - 1);
  Number frag_coord_r =
      mod(frag_coord.x, Number(SCATTERING_TEXTURE_R_SIZE));
      floor(frag_coord.z / Number(SCATTERING_TEXTURE_NU_SIZE));
  Number frag_coord_nu =
      mod(frag_coord.z, Number(SCATTERING_TEXTURE_NU_SIZE));
  vec4 uvwz =
      vec4(frag_coord_nu, frag_coord_r, frag_coord.y, frag_coord.z) /
      vec4(frag_coord_nu, frag_coord.x, frag_coord.y, frag_coord_r) /
          SCATTERING_TEXTURE_SIZE;
  GetRMuMuSNuFromScatteringTextureUvwz(
      atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
@@ -958,13 +960,13 @@ AbstractSpectrum GetScattering(
    bool ray_r_mu_intersects_ground) {
  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
  Number tex_x = floor(tex_coord_x);
  Number lerp = tex_coord_x - tex_x;
  vec3 uvw0 = vec3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  Number tex_coord_w = uvwz.w * Number(SCATTERING_TEXTURE_R_SIZE - 1);
  Number tex_w = floor(tex_coord_w);
  Number lerp = tex_coord_w - tex_w;
  vec3 uvw0 = vec3(uvwz.y,
      uvwz.z, (tex_w + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  vec3 uvw1 = vec3(uvwz.y,
      uvwz.z, (tex_w + 1.0 + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  return AbstractSpectrum(texture(scattering_texture, uvw0) * (1.0 - lerp) +
      texture(scattering_texture, uvw1) * lerp);
}
@@ -1656,13 +1658,13 @@ IrradianceSpectrum GetCombinedScattering(
    OUT(IrradianceSpectrum) single_mie_scattering) {
  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);
  Number tex_x = floor(tex_coord_x);
  Number lerp = tex_coord_x - tex_x;
  vec3 uvw0 = vec3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),
      uvwz.z, uvwz.w);
  Number tex_coord_w = uvwz.w * Number(SCATTERING_TEXTURE_R_SIZE - 1);
  Number tex_w = floor(tex_coord_w);
  Number lerp = tex_coord_w - tex_w;
  vec3 uvw0 = vec3(uvwz.y,
      uvwz.z, (tex_w + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  vec3 uvw1 = vec3(uvwz.y,
      uvwz.z, (tex_w + 1.0 + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
#ifdef COMBINED_SCATTERING_TEXTURES
  vec4 combined_scattering =
      texture(scattering_texture, uvw0) * (1.0 - lerp) +
#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
