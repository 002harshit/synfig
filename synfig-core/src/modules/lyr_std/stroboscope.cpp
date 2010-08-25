/* === S Y N F I G ========================================================= */
/*!	\file stroboscope.cpp
**	\brief Implementation of the "Stroboscope" layer
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
**	Copyright (c) 2009 Ray Frederikson
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
**
** === N O T E S ===========================================================
**
** ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "stroboscope.h"
#include <synfig/valuenode.h>
#include <synfig/valuenode_const.h>
#include <synfig/valuenode_subtract.h>
#include <synfig/time.h>
#include <synfig/context.h>
#include <synfig/paramdesc.h>
#include <synfig/renddesc.h>
#include <synfig/value.h>
#include <synfig/canvas.h>

#endif

using namespace synfig;
using namespace std;
using namespace etl;

/* === M A C R O S ========================================================= */

/* === G L O B A L S ======================================================= */

SYNFIG_LAYER_INIT(Layer_Stroboscope);
SYNFIG_LAYER_SET_NAME(Layer_Stroboscope,"stroboscope");
SYNFIG_LAYER_SET_LOCAL_NAME(Layer_Stroboscope,N_("Stroboscope"));
SYNFIG_LAYER_SET_CATEGORY(Layer_Stroboscope,N_("Other"));
SYNFIG_LAYER_SET_VERSION(Layer_Stroboscope,"0.1");
SYNFIG_LAYER_SET_CVS_ID(Layer_Stroboscope,"$Id$");

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

Layer_Stroboscope::Layer_Stroboscope()
{
	ratio=2;
}

Layer_Stroboscope::~Layer_Stroboscope()
{
}

bool
Layer_Stroboscope::set_param(const String & param, const ValueBase &value)
{
	IMPORT(ratio);

	return Layer::set_param(param,value);
}

ValueBase
Layer_Stroboscope::get_param(const String & param)const
{
	EXPORT(ratio);
	EXPORT_NAME();
	EXPORT_VERSION();

	return Layer::get_param(param);
}

Layer::Vocab
Layer_Stroboscope::get_param_vocab()const
{
	Layer::Vocab ret(Layer::get_param_vocab());

	ret.push_back(ParamDesc("ratio")
		.set_local_name(_("Ratio"))
	);

	return ret;
}

void
Layer_Stroboscope::set_time(Context context, Time t)const
{
	if (ratio != 0)
	{
		float fps = 24.0;
		Canvas::LooseHandle canvas(get_canvas());
		if(canvas)
			fps = canvas->rend_desc().get_frame_rate(); //not works :(
		float frame = floor((t*fps)/ratio)*ratio;
		t = Time(1)*(frame/fps);
	}

	context.set_time(t);
}

Color
Layer_Stroboscope::get_color(Context context, const Point &pos)const
{
	return context.get_color(pos);
}

bool
Layer_Stroboscope::accelerated_render(Context context,Surface *surface,int quality, const RendDesc &renddesc, ProgressCallback *cb)const
{
	return context.accelerated_render(surface,quality,renddesc,cb);
}
