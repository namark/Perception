/********************************************************************
Vireio Perception: Open-Source Stereoscopic 3D Driver
Copyright (C) 2013 Chris Drain

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
********************************************************************/

#ifndef VIEWADJUSTMENT_H_INCLUDED
#define VIEWADJUSTMENT_H_INCLUDED

#include <string>
#include <memory>
#include "d3d9.h"
#include "d3dx9.h"
#include "HMDisplayInfo.h"
#include "Vireio.h"
//#include "GameHandler.h"

#define LEFT_CONSTANT -1
#define RIGHT_CONSTANT 1

#define IPD_DEFAULT 0.064f
// IPD_DEFAULT / 2
#define SEPARATION_DEFAULT 0.032f

//class GameHandler;


class ViewAdjustment
{
public:	
	ViewAdjustment(HMDisplayInfo &hmdInfo, float metersToWorldUnits, bool enableRoll);
	virtual ~ViewAdjustment();

	void UpdateProjectionMatrices(/*float separation, float convergence,*/ float aspectRatio);
	void UpdateRoll(float roll);
	void ComputeViewTransforms();
	
	// Unprojects, shifts view position left/right and reprojects using left/right projection
	D3DXMATRIX LeftAdjustmentMatrix();
	D3DXMATRIX RightAdjustmentMatrix();

	// This shift * projection = left/right shifted projection
	D3DXMATRIX LeftShiftProjection();
	D3DXMATRIX RightShiftProjection();

	D3DXMATRIX Projection();
	D3DXMATRIX ProjectionInverse();

	
	// returns the new separation adjustment in m. (toAdd is the amount to adjust the separation by in m)
	// new adjustment mgiht be the same as old adjustment if adjustment limit is reached
	float ChangeSeparationAdjustment(float toAdd);

	// returns the current separation adjustment being used in m (this is game and user specific and should be saved appropriately)
	//TODO remove this and set on gamehandler which has a 'current user'?
	float SeparationAdjustment();

	// returns the separation being used for view adjustments in game units
	float SeparationInWorldUnits();

	bool RollEnabled();
	void EnableRoll(bool enable);
	void SetWorldScaleFactor(float scale);

	HMDisplayInfo HMDInfo();
	

private:

	float minSeparationAdjusment;
	float maxSeparationAdjusment;

	float separationAdjustment;
	
	
	// Projection Matrix variables
	float n;	//Minimum z-value of the view volume
	float f;	//Maximum z-value of the view volume
	float l;	//Minimum x-value of the view volume
	float r;	//Maximum x-value of the view volume
	float t;	//Minimum y-value of the view volume
	float b;	//Maximum y-value of the view volume

	D3DXMATRIX matProjection;
	D3DXMATRIX matProjectionInv;

	// The translation applied to projection to shift it left/right to get project(Left/Right)
	D3DXMATRIX leftShiftProjection;
	D3DXMATRIX rightShiftProjection;

	// The shifted left and right projection matricies
	D3DXMATRIX projectLeft;
	D3DXMATRIX projectRight;

	D3DXMATRIX rollMatrix;

	D3DXMATRIX matViewProjTransformLeft;
	D3DXMATRIX matViewProjTransformRight;

	HMDisplayInfo hmdInfo;

	bool rollEnabled;
	float metersToWorldMultiplier;
	//std::shared_ptr<GameHandler> m_spGameSpecific;
};



#endif