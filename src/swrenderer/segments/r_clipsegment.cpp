
#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "swrenderer/r_main.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_things.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "swrenderer/scene/r_bsp.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "swrenderer/segments/r_clipsegment.h"

namespace swrenderer
{
	namespace
	{
		struct cliprange_t
		{
			short first, last;
		};

		cliprange_t *newend; // newend is one past the last valid seg
		cliprange_t solidsegs[MAXWIDTH / 2 + 2];
	}

	void R_ClearClipSegs(short left, short right)
	{
		solidsegs[0].first = -0x7fff;
		solidsegs[0].last = left;
		solidsegs[1].first = right;
		solidsegs[1].last = 0x7fff;
		newend = solidsegs+2;
	}

	bool R_CheckClipWallSegment(int first, int last)
	{
		cliprange_t *start;

		// Find the first range that touches the range
		// (adjacent pixels are touching).
		start = solidsegs;
		while (start->last < first)
			start++;

		if (first < start->first)
		{
			return true;
		}

		// Bottom contained in start?
		if (last > start->last)
		{
			return true;
		}

		return false;
	}

	bool R_IsWallSegmentVisible(int sx1, int sx2)
	{
		// Does not cross a pixel.
		if (sx2 <= sx1)
			return false;

		cliprange_t *start = solidsegs;
		while (start->last < sx2)
			start++;

		if (sx1 >= start->first && sx2 <= start->last)
		{
			// The clippost contains the new span.
			return false;
		}

		return true;
	}

	bool R_ClipWallSegment(int first, int last, bool solid, VisibleSegmentCallback callback)
	{
		cliprange_t *next, *start;
		int i, j;
		bool res = false;

		// Find the first range that touches the range
		// (adjacent pixels are touching).
		start = solidsegs;
		while (start->last < first)
			start++;

		if (first < start->first)
		{
			res = true;
			if (last <= start->first)
			{
				// Post is entirely visible (above start).
				if (!callback(first, last))
					return true;

				// Insert a new clippost for solid walls.
				if (solid)
				{
					if (last == start->first)
					{
						start->first = first;
					}
					else
					{
						next = newend;
						newend++;
						while (next != start)
						{
							*next = *(next - 1);
							next--;
						}
						next->first = first;
						next->last = last;
					}
				}
				return true;
			}

			// There is a fragment above *start.
			if (!callback(first, start->first) && solid)
			{
				start->first = first;
			}
		}

		// Bottom contained in start?
		if (last <= start->last)
			return res;

		bool clipsegment;
		next = start;
		while (last >= (next + 1)->first)
		{
			// There is a fragment between two posts.
			clipsegment = callback(next->last, (next + 1)->first);
			next++;

			if (last <= next->last)
			{
				// Bottom is contained in next.
				last = next->last;
				goto crunch;
			}
		}

		// There is a fragment after *next.
		clipsegment = callback(next->last, last);

	crunch:
		if (!clipsegment)
		{
			return true;
		}
		if (solid)
		{
			// Adjust the clip size.
			start->last = last;

			if (next != start)
			{
				// Remove start+1 to next from the clip list,
				// because start now covers their area.
				for (i = 1, j = (int)(newend - next); j > 0; i++, j--)
				{
					start[i] = next[i];
				}
				newend = start + i;
			}
		}
		return true;
	}
}
