 atmosphere/constants.h 
@@ -52,10 +52,10 @@ constexpr int SCATTERING_TEXTURE_MU_SIZE = 128;
constexpr int SCATTERING_TEXTURE_MU_S_SIZE = 32;	constexpr int SCATTERING_TEXTURE_MU_S_SIZE = 32;
constexpr int SCATTERING_TEXTURE_NU_SIZE = 8;	constexpr int SCATTERING_TEXTURE_NU_SIZE = 8;


constexpr int SCATTERING_TEXTURE_WIDTH =	constexpr int SCATTERING_TEXTURE_WIDTH = SCATTERING_TEXTURE_MU_S_SIZE;
    SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_R_SIZE;	
constexpr int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;	constexpr int SCATTERING_TEXTURE_HEIGHT = SCATTERING_TEXTURE_MU_SIZE;
constexpr int SCATTERING_TEXTURE_DEPTH = SCATTERING_TEXTURE_MU_S_SIZE;	constexpr int SCATTERING_TEXTURE_DEPTH =
    SCATTERING_TEXTURE_NU_SIZE * SCATTERING_TEXTURE_R_SIZE;


constexpr int IRRADIANCE_TEXTURE_WIDTH = 64;	constexpr int IRRADIANCE_TEXTURE_WIDTH = 64;
constexpr int IRRADIANCE_TEXTURE_HEIGHT = 16;  atmosphere/demo/webgl/precompute.cc 
@@ -89,7 +89,7 @@ void SaveTexture(const GLenum texture_unit, const GLenum texture_target,
  {	  {
      using namespace atmosphere;	      using namespace atmosphere;
      const std::int32_t header[]={SCATTERING_TEXTURE_MU_S_SIZE, SCATTERING_TEXTURE_MU_SIZE,	      const std::int32_t header[]={SCATTERING_TEXTURE_MU_S_SIZE, SCATTERING_TEXTURE_MU_SIZE,
                                   SCATTERING_TEXTURE_R_SIZE, SCATTERING_TEXTURE_NU_SIZE,	                                   SCATTERING_TEXTURE_NU_SIZE, SCATTERING_TEXTURE_R_SIZE,
                                   4};	                                   4};
      output_stream.write(reinterpret_cast<const char*>(header), sizeof header);	      output_stream.write(reinterpret_cast<const char*>(header), sizeof header);
  }	  }
 atmosphere/functions.glsl 
@@ -784,7 +784,7 @@ vec4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
  // Distance to the horizon.	  // Distance to the horizon.
  Length rho =	  Length rho =
      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);	      SafeSqrt(r * r - atmosphere.bottom_radius * atmosphere.bottom_radius);
  Number u_r = GetTextureCoordFromUnitRange(rho / H, SCATTERING_TEXTURE_R_SIZE);	  Number u_r = rho / H;


  // Discriminant of the quadratic equation for the intersections of the ray	  // Discriminant of the quadratic equation for the intersections of the ray
  // (r,mu) with the ground (see RayIntersectsGround).	  // (r,mu) with the ground (see RayIntersectsGround).
@@ -821,8 +821,9 @@ vec4 GetScatteringTextureUvwzFromRMuMuSNu(IN(AtmosphereParameters) atmosphere,
  Number u_mu_s = GetTextureCoordFromUnitRange(	  Number u_mu_s = GetTextureCoordFromUnitRange(
      max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);	      max(1.0 - a / A, 0.0) / (1.0 + a), SCATTERING_TEXTURE_MU_S_SIZE);


  Number u_nu = (nu + 1.0) / 2.0;	  Number u_nu = GetTextureCoordFromUnitRange((nu + 1.0) / 2.0,
  return vec4(u_nu, u_r, u_mu, u_mu_s);	                                             SCATTERING_TEXTURE_NU_SIZE);
  return vec4(u_nu, u_mu_s, u_mu, u_r);
}	}


/*	/*
@@ -842,7 +843,7 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
      atmosphere.bottom_radius * atmosphere.bottom_radius);	      atmosphere.bottom_radius * atmosphere.bottom_radius);
  // Distance to the horizon.	  // Distance to the horizon.
  Length rho =	  Length rho =
      H * GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_R_SIZE);	      H * uvwz.w;
  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);	  r = sqrt(rho * rho + atmosphere.bottom_radius * atmosphere.bottom_radius);


  if (uvwz.z < 0.5) {	  if (uvwz.z < 0.5) {
@@ -870,7 +871,7 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
  }	  }


  Number x_mu_s =	  Number x_mu_s =
      GetUnitRangeFromTextureCoord(uvwz.w, SCATTERING_TEXTURE_MU_S_SIZE);	      GetUnitRangeFromTextureCoord(uvwz.y, SCATTERING_TEXTURE_MU_S_SIZE);
  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;	  Length d_min = atmosphere.top_radius - atmosphere.bottom_radius;
  Length d_max = H;	  Length d_max = H;
  Number A =	  Number A =
@@ -880,7 +881,8 @@ void GetRMuMuSNuFromScatteringTextureUvwz(IN(AtmosphereParameters) atmosphere,
  mu_s = d == 0.0 * m ? Number(1.0) :	  mu_s = d == 0.0 * m ? Number(1.0) :
     ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));	     ClampCosine((H * H - d * d) / (2.0 * atmosphere.bottom_radius * d));


  nu = ClampCosine(uvwz.x * 2.0 - 1.0);	  nu = ClampCosine(GetUnitRangeFromTextureCoord(uvwz.x, SCATTERING_TEXTURE_NU_SIZE)
                   * 2.0 - 1.0);
}	}


/*	/*
@@ -901,16 +903,16 @@ void GetRMuMuSNuFromScatteringTextureFragCoord(
    OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s, OUT(Number) nu,	    OUT(Length) r, OUT(Number) mu, OUT(Number) mu_s, OUT(Number) nu,
    OUT(bool) ray_r_mu_intersects_ground) {	    OUT(bool) ray_r_mu_intersects_ground) {
  const vec4 SCATTERING_TEXTURE_SIZE = vec4(	  const vec4 SCATTERING_TEXTURE_SIZE = vec4(
      SCATTERING_TEXTURE_NU_SIZE - 1,	      SCATTERING_TEXTURE_NU_SIZE,
      SCATTERING_TEXTURE_R_SIZE,	      SCATTERING_TEXTURE_MU_S_SIZE,
      SCATTERING_TEXTURE_MU_SIZE,	      SCATTERING_TEXTURE_MU_SIZE,
      SCATTERING_TEXTURE_MU_S_SIZE);	      SCATTERING_TEXTURE_R_SIZE - 1);
  Number frag_coord_nu =	
      floor(frag_coord.x / Number(SCATTERING_TEXTURE_R_SIZE));	
  Number frag_coord_r =	  Number frag_coord_r =
      mod(frag_coord.x, Number(SCATTERING_TEXTURE_R_SIZE));	      floor(frag_coord.z / Number(SCATTERING_TEXTURE_NU_SIZE));
  Number frag_coord_nu =
      mod(frag_coord.z, Number(SCATTERING_TEXTURE_NU_SIZE));
  vec4 uvwz =	  vec4 uvwz =
      vec4(frag_coord_nu, frag_coord_r, frag_coord.y, frag_coord.z) /	      vec4(frag_coord_nu, frag_coord.x, frag_coord.y, frag_coord_r) /
          SCATTERING_TEXTURE_SIZE;	          SCATTERING_TEXTURE_SIZE;
  GetRMuMuSNuFromScatteringTextureUvwz(	  GetRMuMuSNuFromScatteringTextureUvwz(
      atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);	      atmosphere, uvwz, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
@@ -958,13 +960,13 @@ AbstractSpectrum GetScattering(
    bool ray_r_mu_intersects_ground) {	    bool ray_r_mu_intersects_ground) {
  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(	  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);	      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);	  Number tex_coord_w = uvwz.w * Number(SCATTERING_TEXTURE_R_SIZE - 1);
  Number tex_x = floor(tex_coord_x);	  Number tex_w = floor(tex_coord_w);
  Number lerp = tex_coord_x - tex_x;	  Number lerp = tex_coord_w - tex_w;
  vec3 uvw0 = vec3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),	  vec3 uvw0 = vec3(uvwz.y,
      uvwz.z, uvwz.w);	      uvwz.z, (tex_w + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),	  vec3 uvw1 = vec3(uvwz.y,
      uvwz.z, uvwz.w);	      uvwz.z, (tex_w + 1.0 + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  return AbstractSpectrum(texture(scattering_texture, uvw0) * (1.0 - lerp) +	  return AbstractSpectrum(texture(scattering_texture, uvw0) * (1.0 - lerp) +
      texture(scattering_texture, uvw1) * lerp);	      texture(scattering_texture, uvw1) * lerp);
}	}
@@ -1656,13 +1658,13 @@ IrradianceSpectrum GetCombinedScattering(
    OUT(IrradianceSpectrum) single_mie_scattering) {	    OUT(IrradianceSpectrum) single_mie_scattering) {
  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(	  vec4 uvwz = GetScatteringTextureUvwzFromRMuMuSNu(
      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);	      atmosphere, r, mu, mu_s, nu, ray_r_mu_intersects_ground);
  Number tex_coord_x = uvwz.x * Number(SCATTERING_TEXTURE_NU_SIZE - 1);	  Number tex_coord_w = uvwz.w * Number(SCATTERING_TEXTURE_R_SIZE - 1);
  Number tex_x = floor(tex_coord_x);	  Number tex_w = floor(tex_coord_w);
  Number lerp = tex_coord_x - tex_x;	  Number lerp = tex_coord_w - tex_w;
  vec3 uvw0 = vec3((tex_x + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),	  vec3 uvw0 = vec3(uvwz.y,
      uvwz.z, uvwz.w);	      uvwz.z, (tex_w + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
  vec3 uvw1 = vec3((tex_x + 1.0 + uvwz.y) / Number(SCATTERING_TEXTURE_NU_SIZE),	  vec3 uvw1 = vec3(uvwz.y,
      uvwz.z, uvwz.w);	      uvwz.z, (tex_w + 1.0 + uvwz.x) / Number(SCATTERING_TEXTURE_R_SIZE));
#ifdef COMBINED_SCATTERING_TEXTURES	#ifdef COMBINED_SCATTERING_TEXTURES
  vec4 combined_scattering =	  vec4 combined_scattering =
      texture(scattering_texture, uvw0) * (1.0 - lerp) +	      texture(scattering_texture, uvw0) * (1.0 - lerp) +
