/* Stellarium Web Engine - Copyright (c) 2020 - Noctua Software Ltd
 *
 * This program is licensed under the terms of the GNU AGPL v3, or
 * alternatively under a commercial license.
 *
 * The terms of the AGPL v3 license can be found in the main directory of this
 * repository.
 */

/*
 * A few functions that could be in core.h, but that I put here to
 * cleanup the code a bit.
 *
 * Note: we should probably try to make the navigation code totally
 * independent of the core.
 */

/*
 * Function: core_update_observer
 * Update the observer time and direction.
 *
 * Should be called at each frame.
 */
void core_update_observer(double dt);
