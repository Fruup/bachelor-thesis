#pragma once

#include <engine/renderer/Renderer.h>

#include "FluidRenderer.h"

class _AdvancedFluidRenderer : public Renderer, public FluidRenderer
{
public:
	//bool VInit() override
	//{
	//}

	//void VExit() override
	//{
	//}

	//void RenderFrame() override
	//{
	//	/*
	//	 * - generate density mask
	//	 * - generate depth buffers for
	//	 *	 - all particles (D_all)
	//	 *	 - only aggregated particles (D_agg)
	//	 * - smooth aggregated depth buffer
	//	 * - generate normals from smoothed depth buffer
	//	 * 
	//	 * - for each tile of the screen:
	//	 *   - for each pixel of the tile:
	//	 *       ...
	//	 * 
	//	 * - render fluid from "geometry buffer" with positions and normals
	//	 */
	//}

	//void generateDensityMask()
	//{
	//	// - construct a 3D grid
	//	// - estimate maximum density at the center of each grid point
	//	// - 
	//}
};
