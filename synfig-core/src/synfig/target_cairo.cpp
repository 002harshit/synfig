/* === S Y N F I G ========================================================= */
/*!	\file target_cairo.cpp
**	\brief Template File
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2012 Carlos López
**
**	This package is free software; you can redistribute it and/or
**	modify it under the terms of the GNU General Public License as
**	published by the Free Software Foundation; either version 2 of
**	the License, or (at your option) any later version.
**
**	This package is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
**	General Public License for more details.
**	\endlegal
*/
/* ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "target_cairo.h"
#include "string.h"
#include "surface.h"
#include "render.h"
#include "canvas.h"
#include "context.h"

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;

/* === M A C R O S ========================================================= */

// note that if this isn't defined then the rendering is incorrect for
// the straight blend method since the optimize_layers() function in
// canvas.cpp which makes the straight blend method work correctly
// isn't called.  ie. leave this defined.  to see the problem, draw a
// small circle over a solid background.  set circle to amount 0.99
// and blend method 'straight'.  the background should vanish but doesn't
#define SYNFIG_OPTIMIZE_LAYER_TREE

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

Target_Cairo::Target_Cairo()
{
	curr_frame_=0;
}

int
Target_Cairo::next_frame(Time& time)
{
	return Target::next_frame(time);
}

bool
synfig::Target_Cairo::render(ProgressCallback *cb)
{
	int
		frames=0,
		total_frames,
		quality=get_quality(),
		frame_start,
		frame_end;
	Time
		t=0;

	assert(canvas);
	curr_frame_=0;

	if( !init() ){
		if(cb) cb->error(_("Target initialization failure"));
		return false;
	}

	frame_start=desc.get_frame_start();
	frame_end=desc.get_frame_end();

	// Calculate the number of frames
	total_frames=frame_end-frame_start+1;
	if(total_frames<=0)total_frames=1;

	try {
		do{
			// Grab the time
			if(total_frames>1)
				frames=next_frame(t);
			else
				frames=0;

			// If we have a callback, and it returns
			// false, go ahead and bail. (it may be a user cancel)
			if(cb && !cb->amount_complete(total_frames-frames,total_frames))
				return false;

			// Set the time that we wish to render
			if(!get_avoid_time_sync() || canvas->get_time()!=t)
				canvas->set_time(t);

			Context context;

	#ifdef SYNFIG_OPTIMIZE_LAYER_TREE
			Canvas::Handle op_canvas;
			if (!getenv("SYNFIG_DISABLE_OPTIMIZE_LAYER_TREE"))
			{
				op_canvas = Canvas::create();
				op_canvas->set_file_name(canvas->get_file_name());
				optimize_layers(canvas->get_time(), canvas->get_context(), op_canvas);
				context=op_canvas->get_context();
			}
			else
				context=canvas->get_context();
	#else
			context=canvas->get_context();
	#endif
			// Obtain a pointer to the cairo_surface_t given by the Target instance.
			cairo_surface_t* surface;
			if(obtain_surface(surface))
			{
				if(!context.accelerated_cairorender(surface,quality,desc,cb))
				{
					// For some reason, the accelerated renderer failed.
					if(cb)cb->error(_("Accelerated Renderer Failure"));
					return false;
				}
				else
				{
					// Put the surface we renderer onto the target's device.
					// and destrois cairo_surface_t
					if(!put_surface(surface, cb))
					{
						if(cb)cb->error(_("Unable to put surface on target"));
						return false;
					}
				}
			}
			else
			{
				if(cb)cb->error(_("Not supported render method"));
				return false;						
			}
		}while(frames);
	}
	catch(String str)
	{
		if(cb)cb->error(_("Caught string :")+str);
		return false;
	}
	catch(std::bad_alloc)
	{
		if(cb)cb->error(_("Ran out of memory (Probably a bug)"));
		return false;
	}
	catch(...)
	{
		if(cb)cb->error(_("Caught unknown error, rethrowing..."));
		throw;
	}
	return true;
}

bool
Target_Cairo::put_surface(cairo_surface_t *surface, ProgressCallback *cb)
{
	if(cairo_surface_status(surface))
	{
		if(cb) cb->error(_("Cairo Surface bad status"));
		return false;
	}	
	cairo_surface_flush(surface);
	cairo_surface_destroy(surface);
	return true;
}
