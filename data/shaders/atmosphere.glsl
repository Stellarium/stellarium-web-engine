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
} <qresource prefix="/">	    <qresource prefix="/">
        <file>shaders/xyYToRGB.glsl</file>	        <file>shaders/xyYToRGB.glsl</file>
    </qresource>	    </qresource>
    <qresource prefix="/">
        <file>shaders/xyYToRGB.frag</file>
    </qresource>
    <qresource prefix="/">
        <file>shaders/AtmosphereFunctions.frag</file>
    </qresource>
    <qresource prefix="/">
        <file>shaders/AtmosphereMain.vert</file>
    </qresource>
    <qresource prefix="/">
        <file>shaders/AtmosphereMain.frag</file>
    </qresource>
</RCC>	</RCC>
#version 130

const float kLengthUnitInMeters = 1000.000000;
const float earthRadius=6.36e6/kLengthUnitInMeters;
const float sunAngularRadius=0.00935/2;

const vec2 sun_size=vec2(tan(sunAngularRadius),cos(sunAngularRadius));
const vec3 earth_center=vec3(0,0,-earthRadius);
const float exposure=0.6;

uniform vec3 camera;
uniform vec3 sun_direction;

in vec3 view_ray;
out vec4 color;
vec3 GetSolarLuminance();
vec3 GetSkyLuminance(vec3 camera, vec3 view_ray, float shadow_length,
                     vec3 sun_direction, out vec3 transmittance);
vec3 adaptColorToVisionMode(vec3 rgb);
vec3 dither(vec3);
void main()
{
    vec3 view_direction=normalize(view_ray);
    float fragment_angular_size = length(dFdx(view_direction) + dFdy(view_direction));
    vec3 transmittance;
    vec3 radiance = GetSkyLuminance(camera - earth_center, view_direction, 0, sun_direction, transmittance);
    /*
    // This 'if' case is useful when we want to check that the Sun is where
    // it's supposed to be and, ideally, has the color it's supposed to have
    if (dot(view_direction, sun_direction) > sun_size.y)
        radiance += transmittance * GetSolarLuminance();
     */
    color = vec4(dither(pow(adaptColorToVisionMode(radiance * exposure), vec3(1.0 / 2.2))),1);
}uniform mediump mat4 projectionMatrix;

attribute vec2 vertex;
attribute vec3 viewRay;

varying vec3 view_ray;

void main()
{* Stellarium
 * Copyright (C) 2002-2018 Fabien Chereau and Stellarium contributors
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#version 130

const highp float pi = 3.1415926535897931;
const highp float ln10 = 2.3025850929940459;

// Variable for the xyYTo RGB conversion
uniform highp float alphaWaOverAlphaDa;
uniform highp float oneOverGamma;
uniform highp float term2TimesOneOverMaxdLpOneOverGamma;
uniform highp float brightnessScale;

vec3 XYZ2xyY(vec3 XYZ)
{
	return vec3(XYZ.xy/(XYZ.x+XYZ.y+XYZ.z), XYZ.y);
}

vec3 RGB2XYZ(vec3 rgb)
{
	return rgb*mat3(0.4124564, 0.3575761, 0.1804375,
					0.2126729, 0.7151522, 0.0721750,
					0.0193339, 0.1191920, 0.9503041);
}

vec3 adaptColorToVisionMode(vec3 rgb)
{
    vec3 color=XYZ2xyY(RGB2XYZ(rgb));
	///////////////////////////////////////////////////////////////////////////
	// Now we have the xyY components in color, need to convert to RGB

	// 1. Hue conversion
	// if log10Y>0.6, photopic vision only (with the cones, colors are seen)
	// else scotopic vision if log10Y<-2 (with the rods, no colors, everything blue),
	// else mesopic vision (with rods and cones, transition state)

    mediump vec3 resultColor;
	if (color[2] <= 0.01)
	{
		// special case for s = 0 (x=0.25, y=0.25)
		color[2] *= 0.5121445;
		color[2] = pow(abs(color[2]*pi*0.0001), alphaWaOverAlphaDa*oneOverGamma)* term2TimesOneOverMaxdLpOneOverGamma;
		color[0] = 0.787077*color[2];
		color[1] = 0.9898434*color[2];
		color[2] *= 1.9256125;
		resultColor = color.xyz*brightnessScale;
	}
	else
	{
		if (color[2]<3.9810717055349722)
		{
			// Compute s, ratio between scotopic and photopic vision
			float op = (log(color[2])/ln10 + 2.)/2.6;
			float s = op * op *(3. - 2. * op);
			// Do the blue shift for scotopic vision simulation (night vision) [3]
			// The "night blue" is x,y(0.25, 0.25)
			color[0] = (1. - s) * 0.25 + s * color[0];	// Add scotopic + photopic components
			color[1] = (1. - s) * 0.25 + s * color[1];	// Add scotopic + photopic components
			// Take into account the scotopic luminance approximated by V [3] [4]
			float V = color[2] * (1.33 * (1. + color[1] / color[0] + color[0] * (1. - color[0] - color[1])) - 1.68);
			color[2] = 0.4468 * (1. - s) * V + s * color[2];
		}

		// 2. Adapt the luminance value and scale it to fit in the RGB range [2]
		// color[2] = std::pow(adaptLuminanceScaled(color[2]), oneOverGamma);
		color[2] = pow(abs(color[2]*pi*0.0001), alphaWaOverAlphaDa*oneOverGamma)* term2TimesOneOverMaxdLpOneOverGamma;

		// Convert from xyY to XZY
		// Use a XYZ to Adobe RGB (1998) matrix which uses a D65 reference white
		mediump vec3 tmp = vec3(color[0] * color[2] / color[1], color[2], (1. - color[0] - color[1]) * color[2] / color[1]);
		resultColor = vec3(2.04148*tmp.x-0.564977*tmp.y-0.344713*tmp.z, -0.969258*tmp.x+1.87599*tmp.y+0.0415557*tmp.z, 0.0134455*tmp.x-0.118373*tmp.y+1.01527*tmp.z);
		resultColor*=brightnessScale;
	}core/planetsephems/de431.cpp	     core/planetsephems/de431.cpp
     core/planetsephems/de431.hpp	     core/planetsephems/de431.hpp


     core/modules/Atmosphere.cpp	     core/modules/AtmospherePreetham.cpp
     core/modules/AtmospherePreetham.hpp
     core/modules/AtmosphereBruneton.cpp
     core/modules/AtmosphereBruneton.hpp
     core/modules/Atmosphere.hpp	     core/modules/Atmosphere.hpp
     core/modules/Asterism.cpp	     core/modules/Asterism.cpp
     core/modules/Asterism.hpp	     core/modules/Asterism.hpp

    return resultColor;
}/*	/*
 * Stellarium	 * Stellarium
 * Copyright (C) 2003 Fabien Chereau	 * Copyright (C) 2019 Ruslan Kabatsayev
 * Copyright (C) 2012 Timothy Reaves	
 *	 *
 * This program is free software; you can redistribute it and/or	 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License	 * modify it under the terms of the GNU General Public License
@@ -18,106 +17,53 @@
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.	 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */	 */


#ifndef ATMOSTPHERE_HPP	#ifndef ATMOSPHERE_HPP
#define ATMOSTPHERE_HPP	#define ATMOSPHERE_HPP


#include "Skylight.hpp"	#include "StelCore.hpp"
#include "VecMath.hpp"	#include "VecMath.hpp"


#include "Skybright.hpp"	
#include "StelFader.hpp"	

#include <QOpenGLBuffer>	

class StelProjector;	
class StelToneReproducer;	
class StelCore;	

//! Compute and display the daylight sky color using openGL.	
//! The sky brightness is computed with the SkyBright class, the color with the SkyLight.	
//! Don't use this class directly but use it through the LandscapeMgr.	
class Atmosphere	class Atmosphere
{	{
public:	public:
	Atmosphere();		virtual ~Atmosphere() = default;
	virtual ~Atmosphere();	

	//! Compute sky brightness values and average luminance.		//! Compute sky brightness values and average luminance.
	void computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude, StelCore* core,		virtual void computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude, StelCore* core,
		float latitude = 45.f, float altitude = 200.f,		                          float latitude = 45.f, float altitude = 200.f, float temperature = 15.f, float relativeHumidity = 40.f) = 0;
		float temperature = 15.f, float relativeHumidity = 40.f);		virtual void draw(StelCore* core) = 0;
	void draw(StelCore* core);		virtual void update(double deltaTime) = 0;
	void update(double deltaTime) {fader.update((int)(deltaTime*1000));}	


	//! Set fade in/out duration in seconds		//! Set fade in/out duration in seconds
	void setFadeDuration(float duration) {fader.setDuration((int)(duration*1000.f));}		virtual void setFadeDuration(float duration) = 0;
	//! Get fade in/out duration in seconds		//! Get fade in/out duration in seconds
	float getFadeDuration() const {return (float)fader.getDuration()/1000.f;}		virtual float getFadeDuration() const = 0;


	//! Define whether to display atmosphere		//! Define whether to display atmosphere
	void setFlagShow(bool b){fader = b;}		virtual void setFlagShow(bool b) = 0;
	//! Get whether atmosphere is displayed		//! Get whether atmosphere is displayed
	bool getFlagShow() const {return fader;}		virtual bool getFlagShow() const = 0;


	//! Get the actual atmosphere intensity due to eclipses + fader		//! Get the actual atmosphere intensity due to eclipses + fader
	//! @return the display intensity ranging from 0 to 1		//! @return the display intensity ranging from 0 to 1
	float getRealDisplayIntensityFactor() const {return fader.getInterstate()*eclipseFactor;}		virtual float getRealDisplayIntensityFactor() const = 0;

/*
 * Stellarium
 * Copyright (C) 2019 Ruslan Kabatsayev
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#ifndef ATMOSPHERE_BRUNETON_HPP
#define ATMOSPHERE_BRUNETON_HPP

#include "Atmosphere.hpp"
#include "VecMath.hpp"

#include "Skybright.hpp"
#include "StelFader.hpp"

#include <array>
#include <memory>
#include <QOpenGLBuffer>

class StelProjector;
class StelToneReproducer;
class StelCore;
class QOpenGLFunctions;

//! Compute and display the daylight sky color using openGL.
//! The sky brightness is computed with the SkyBright class, the color with the SkyLight.
//! Don't use this class directly but use it through the LandscapeMgr.
class AtmosphereBruneton : public Atmosphere
{
public:
	AtmosphereBruneton();
	~AtmosphereBruneton();

	void computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude, StelCore* core,
					  float latitude, float altitude, float temperature, float relativeHumidity) override;
	void draw(StelCore* core) override;
	void update(double deltaTime) override {fader.update((int)(deltaTime*1000));}

	void setFadeDuration(float duration) override {fader.setDuration((int)(duration*1000.f));}
	float getFadeDuration() const override {return (float)fader.getDuration()/1000.f;}

	void setFlagShow(bool b) override {fader = b;}
	bool getFlagShow() const override  {return fader;}

	float getRealDisplayIntensityFactor() const override  {return fader.getInterstate()*eclipseFactor;}

	float getFadeIntensity() const override  {return fader.getInterstate();}

	float getAverageLuminance() const override  {return averageLuminance;}

	void setAverageLuminance(float overrideLum) override;
	void setLightPollutionLuminance(float f) override { lightPollutionLuminance = f; }
	float getLightPollutionLuminance() const override { return lightPollutionLuminance; }

private:
	Vec4i viewport;
	Skybright skyb;
	int gridMaxY,gridMaxX;

	QVector<Vec2f> posGrid;
	QOpenGLBuffer posGridBuffer;
	QOpenGLBuffer indexBuffer;
	QVector<Vec4f> viewRayGrid;
	QOpenGLBuffer viewRayGridBuffer;

	//! The average luminance of the atmosphere in cd/m2
	float averageLuminance;
	bool overrideAverageLuminance; // if true, don't compute but keep value set via setAverageLuminance(float)
	float eclipseFactor;
	LinearFader fader;
	float lightPollutionLuminance;

	//! Vertex shader used for xyYToRGB computation
	std::unique_ptr<QOpenGLShaderProgram> atmoShaderProgram;
	struct {
		int bayerPattern;
		int rgbMaxValue;
		int alphaWaOverAlphaDa;
		int oneOverGamma;
		int term2TimesOneOverMaxdLpOneOverGamma;
		int brightnessScale;
		int sunDir;
		int cameraPos;
		int projectionMatrix;
		int skyVertex;
		int viewRay;
		int transmittanceTexture;
		int scatteringTexture;
		int irradianceTexture;
		int singleMieScatteringTexture;
	} shaderAttribLocations;

	GLuint bayerPatternTex=0;

	enum
	{
		TRANSMITTANCE_TEXTURE,
		SCATTERING_TEXTURE,
		IRRADIANCE_TEXTURE,
		MIE_SCATTERING_TEXTURE,

		TEX_COUNT
	};
	GLuint textures[TEX_COUNT];
	Vec3d sunDir;
	double altitude;
	void loadTextures();
	void loadShaders();
	void regenerateGrid();
	void updateEclipseFactor(StelCore* core, Vec3d sunPos, Vec3d moonPos);
	bool separateMieTexture=false;

	struct TextureSize4D
	{
		std::array<std::int32_t,4> sizes;
		int muS_size() const { return sizes[0]; }
		int mu_size() const { return sizes[1]; }
		int nu_size() const { return sizes[2]; }
		int r_size() const { return sizes[3]; }
		int width() const { return muS_size(); }
		int height() const { return mu_size(); }
		int depth() const { return nu_size()*r_size(); }
		bool operator!=(TextureSize4D const& rhs) const { return sizes!=rhs.sizes; }
	};
	struct TextureSize2D
	{
		std::array<std::int32_t,2> sizes;
		int width() const { return sizes[0]; }
		int height() const { return sizes[1]; }
	};
	TextureSize2D transmittanceTextureSize, irradianceTextureSize;
	TextureSize4D scatteringTextureSize, mieScatteringTextureSize;

	static TextureSize4D getTextureSize4D(QVector<char> const& data);
	static TextureSize2D getTextureSize2D(QVector<char> const& data);  * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.	 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */	 */


#include "Atmosphere.hpp"	#include "AtmospherePreetham.hpp"
#include "StelUtils.hpp"	#include "StelUtils.hpp"
#include "StelApp.hpp"	#include "StelApp.hpp"
#include "StelProjector.hpp"	#include "StelProjector.hpp"
@@ -32,7 +32,7 @@
#include <QOpenGLShaderProgram>	#include <QOpenGLShaderProgram>




Atmosphere::Atmosphere(void)	AtmospherePreetham::AtmospherePreetham(void)
	: viewport(0,0,0,0)		: viewport(0,0,0,0)
	, skyResolutionY(44)		, skyResolutionY(44)
	, skyResolutionX(44)		, skyResolutionX(44)
@@ -103,7 +103,7 @@ Atmosphere::Atmosphere(void)
	atmoShaderProgram->release();		atmoShaderProgram->release();
}	}


Atmosphere::~Atmosphere(void)	AtmospherePreetham::~AtmospherePreetham(void)
{	{
	delete [] posGrid;		delete [] posGrid;
	posGrid = Q_NULLPTR;		posGrid = Q_NULLPTR;
@@ -113,7 +113,7 @@ Atmosphere::~Atmosphere(void)
	atmoShaderProgram = Q_NULLPTR;		atmoShaderProgram = Q_NULLPTR;
}	}


void Atmosphere::computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude,	void AtmospherePreetham::computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude,
							   StelCore* core, float latitude, float altitude, float temperature, float relativeHumidity)								   StelCore* core, float latitude, float altitude, float temperature, float relativeHumidity)
{	{
	const StelProjectorP prj = core->getProjection(StelCore::FrameAltAz, StelCore::RefractionOff);		const StelProjectorP prj = core->getProjection(StelCore::FrameAltAz, StelCore::RefractionOff);
@@ -316,7 +316,7 @@ void Atmosphere::computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moo


// override computable luminance. This is for special operations only, e.g. for scripting of brightness-balanced image export.	// override computable luminance. This is for special operations only, e.g. for scripting of brightness-balanced image export.
// To return to auto-computed values, set any negative value.	// To return to auto-computed values, set any negative value.
void Atmosphere::setAverageLuminance(float overrideLum)	void AtmospherePreetham::setAverageLuminance(float overrideLum)
{	{
	if (overrideLum<0.f)		if (overrideLum<0.f)
	{		{
@@ -331,7 +331,7 @@ void Atmosphere::setAverageLuminance(float overrideLum)
}	}


// Draw the atmosphere using the precalc values stored in tab_sky	// Draw the atmosphere using the precalc values stored in tab_sky
void Atmosphere::draw(StelCore* core)	void AtmospherePreetham::draw(StelCore* core)
{	{
	if (StelApp::getInstance().getVisionModeNight())		if (StelApp::getInstance().getVisionModeNight())
		return;			return;
@@ -409,6 +409,6 @@ void Atmosphere::draw(StelCore* core)
	//StelPainter painter(prj);		//StelPainter painter(prj);
	//painter.setFont(font);		//painter.setFont(font);
	//sPainter.setColor(0.7, 0.7, 0.7);		//sPainter.setColor(0.7, 0.7, 0.7);
	//sPainter.drawText(83, 120, QString("Atmosphere::getAverageLuminance(): %1" ).arg(getAverageLuminance()));		//sPainter.drawText(83, 120, QString("AtmospherePreetham::getAverageLuminance(): %1" ).arg(getAverageLuminance()));
	//qDebug() << atmosphere->getAverageLuminance();		//qDebug() << atmosphere->getAverageLuminance();
}	} /*
 * Stellarium
 * Copyright (C) 2003 Fabien Chereau
 * Copyright (C) 2012 Timothy Reaves 
 *#include "Atmosphere.hpp"
 * This program is free software; you can redistribute it and/or AtmospherePreetham. "AtmosphereBruneton.hpp"
 * modify it under the terms of the GNU General Public License 	const auto atmosphereModelConfig=conf->value("landscape/atmosphere_model", "preetham").toString();
	if(atmosphereModelConfig=="preetham")
		atmosphere = new AtmospherePreetham();
	else if(atmosphereModelConfig=="bruneton")
		atmosphere = new AtmosphereBruneton();    
	else    ########### install files ###############	########### install files ###############


INSTALL(DIRECTORY ./ DESTINATION ${SDATALOC}/textures	INSTALL(DIRECTORY ./ DESTINATION ${SDATALOC}/textures
        FILES_MATCHING PATTERN "*.png"	        FILES_MATCHING PATTERN "*.png" PATTERN "atmosphere/*.dat"
        PATTERN "CMakeFiles" EXCLUDE )	        PATTERN "CMakeFiles" EXCLUDE )

	{
		qWarning() << "Unsupported atmosphere model" << atmosphereModelConfig;
		atmosphere = new AtmospherePreetham();
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Suite 500, Boston, MA  02110-1335, USA.
 */

#ifndef ATMOSPHERE_PREETHAM_HPP
#define ATMOSPHERE_PREETHAM_HPP

#include "Atmosphere.hpp"
#include "Skylight.hpp"
#include "VecMath.hpp"

#include "Skybright.hpp"
#include "StelFader.hpp"

#include <QOpenGLBuffer>

class StelProjector;
class StelToneReproducer;
class StelCore;

//! Compute and display the daylight sky color using openGL.
//! The sky brightness is computed with the SkyBright class, the color with the SkyLight.
//! Don't use this class directly but use it through the LandscapeMgr.
class AtmospherePreetham : public Atmosphere
{
public:
	AtmospherePreetham();
	virtual ~AtmospherePreetham();

	//! Compute sky brightness values and average luminance.
	void computeColor(double JD, Vec3d _sunPos, Vec3d moonPos, float moonPhase, float moonMagnitude, StelCore* core,
		float latitude = 45.f, float altitude = 200.f,
		float temperature = 15.f, float relativeHumidity = 40.f);
	void draw(StelCore* core);
	void update(double deltaTime) {fader.update((int)(deltaTime*1000));}

	//! Set fade in/out duration in seconds
	void setFadeDuration(float duration) {fader.setDuration((int)(duration*1000.f));}
	//! Get fade in/out duration in seconds
	float getFadeDuration() const {return (float)fader.getDuration()/1000.f;}

	//! Define whether to display atmosphere
	void setFlagShow(bool b){fader = b;}
	//! Get whether atmosphere is displayed
	bool getFlagShow() const {return fader;}

	//! Get the actual atmosphere intensity due to eclipses + fader
	//! @return the display intensity ranging from 0 to 1
	float getRealDisplayIntensityFactor() const {return fader.getInterstate()*eclipseFactor;}

	// lets you know how far faded in or out the atmosphere is (0..1)
	float getFadeIntensity() const {return fader.getInterstate();}

	//! Get the average luminance of the atmosphere in cd/m2
	//! If atmosphere is off, the luminance equals the background starlight (0.001cd/m2).
	// TODO: Find reference for this value? Why 1 mcd/m2 without atmosphere and 0.1 mcd/m2 inside? Absorption?
	//! Otherwise it includes the (atmosphere + background starlight (0.0001cd/m2) * eclipse factor + light pollution.
	//! @return the last computed average luminance of the atmosphere in cd/m2.
	float getAverageLuminance() const {return averageLuminance;}

	//! override computable luminance. This is for special operations only, e.g. for scripting of brightness-balanced image export.
	//! To return to auto-computed values, set any negative value at the end of the script.
	void setAverageLuminance(float overrideLum);
	//! Set the light pollution luminance in cd/m^2
	void setLightPollutionLuminance(float f) { lightPollutionLuminance = f; }
	//! Get the light pollution luminance in cd/m^2
	float getLightPollutionLuminance() const { return lightPollutionLuminance; }

private:
	Vec4i viewport;
	Skylight sky;
	Skybright skyb;
	int skyResolutionY,skyResolutionX;

	Vec2f* posGrid;
	QOpenGLBuffer posGridBuffer;
	QOpenGLBuffer indicesBuffer;
	Vec4f* colorGrid;
	QOpenGLBuffer colorGridBuffer;

	//! The average luminance of the atmosphere in cd/m2
	float averageLuminance;
	bool overrideAverageLuminance; // if true, don't compute but keep value set via setAverageLuminance(float)
	float eclipseFactor;
	LinearFader fader;
	float lightPollutionLuminance;

	//! Vertex shader used for xyYToRGB computation
	class QOpenGLShaderProgram* atmoShaderProgram;
	struct {
		int bayerPattern;
		int rgbMaxValue;
		int alphaWaOverAlphaDa;
		int oneOverGamma;
		int term2TimesOneOverMaxdLpOneOverGamma;
		int brightnessScale;
		int sunPos;
		int term_x, Ax, Bx, Cx, Dx, Ex;
		int term_y, Ay, By, Cy, Dy, Ey;
		int projectionMatrix;
		int skyVertex;
		int skyColor;
	} shaderAttribLocations;

	GLuint bayerPatternTex=0;
};

#endif // ATMOSTPHERE_HPP
};

#endif // ATMOSTPHERE_HPP
	// lets you know how far faded in or out the atmosphere is (0..1)		// lets you know how far faded in or out the atmosphere is (0..1)
	float getFadeIntensity() const {return fader.getInterstate();}		virtual float getFadeIntensity() const = 0;


	//! Get the average luminance of the atmosphere in cd/m2		/*!
	//! If atmosphere is off, the luminance equals the background starlight (0.001cd/m2).		 * Get the average luminance of the atmosphere in cd/m2
	// TODO: Find reference for this value? Why 1 mcd/m2 without atmosphere and 0.1 mcd/m2 inside? Absorption?		 * If atmosphere is off, the luminance equals the background starlight (0.001cd/m2).
	//! Otherwise it includes the (atmosphere + background starlight (0.0001cd/m2) * eclipse factor + light pollution.		 * Otherwise it includes the (atmosphere + background starlight (0.0001cd/m2) * eclipse factor + light pollution.
	//! @return the last computed average luminance of the atmosphere in cd/m2.		 * @return the last computed average luminance of the atmosphere in cd/m2.
	float getAverageLuminance() const {return averageLuminance;}		 */

	virtual float getAverageLuminance() const = 0;
	//! override computable luminance. This is for special operations only, e.g. for scripting of brightness-balanced image export.		//! override computable luminance. This is for special operations only, e.g. for scripting of brightness-balanced image export.
	//! To return to auto-computed values, set any negative value at the end of the script.		//! To return to auto-computed values, set any negative value at the end of the script.
	void setAverageLuminance(float overrideLum);		virtual void setAverageLuminance(float overrideLum) = 0;
	//! Set the light pollution luminance in cd/m^2		//! Set the light pollution luminance in cd/m^2
	void setLightPollutionLuminance(float f) { lightPollutionLuminance = f; }		virtual void setLightPollutionLuminance(float f) = 0;
	//! Get the light pollution luminance in cd/m^2		//! Get the light pollution luminance in cd/m^2
	float getLightPollutionLuminance() const { return lightPollutionLuminance; }		virtual float getLightPollutionLuminance() const = 0;

private:	
	Vec4i viewport;	
	Skylight sky;	
	Skybright skyb;	
	int skyResolutionY,skyResolutionX;	

	Vec2f* posGrid;	
	QOpenGLBuffer posGridBuffer;	
	QOpenGLBuffer indicesBuffer;	
	Vec4f* colorGrid;	
	QOpenGLBuffer colorGridBuffer;	

	//! The average luminance of the atmosphere in cd/m2	
	float averageLuminance;	
	bool overrideAverageLuminance; // if true, don't compute but keep value set via setAverageLuminance(float)	
	float eclipseFactor;	
	LinearFader fader;	
	float lightPollutionLuminance;	

	//! Vertex shader used for xyYToRGB computation	
	class QOpenGLShaderProgram* atmoShaderProgram;	
	struct {	
		int bayerPattern;	
		int rgbMaxValue;	
		int alphaWaOverAlphaDa;	
		int oneOverGamma;	
		int term2TimesOneOverMaxdLpOneOverGamma;	
		int brightnessScale;	
		int sunPos;	
		int term_x, Ax, Bx, Cx, Dx, Ex;	
		int term_y, Ay, By, Cy, Dy, Ey;	
		int projectionMatrix;	
		int skyVertex;	
		int skyColor;	
	} shaderAttribLocations;	

	GLuint bayerPatternTex=0;	
};	};


#endif // ATMOSTPHERE_HPP	#endif
    view_ray = viewRay.xyz;
	gl_Position = projectionMatrix*vec4(vertex, 0., 1.);
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

    // Apply logarithmic tonemapping on luminance Y only.
    // Code should be the same as in tonemapper.c, assuming q == 1.
    xyy.z = log(1.0 + xyy.z * u_tm[0]) / log(1.0 + u_tm[1] * u_tm[0]) * u_tm[2];

    // Convert xyY to sRGB
    highp vec3 rgb = xyy_to_srgb(xyy);

    // Apply gamma correction
    v_color = vec4(gammaf(rgb.r), gammaf(rgb.g),gammaf(rgb.b), 1.0);
}

#endif
#ifdef FRAGMENT_SHADER

void main()
{
    gl_FragColor = v_color;
}

#endif
