/* === S Y N F I G ========================================================= */
/*!	\file import.h
**	\brief Header file for implementation of the "Import Image" layer
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
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

/* === S T A R T =========================================================== */

#ifndef __SYNFIG_IMPORT_H
#define __SYNFIG_IMPORT_H

/* === H E A D E R S ======================================================= */

#include <synfig/layer_bitmap.h>
#include <synfig/color.h>
#include <synfig/vector.h>
#include <synfig/importer.h>
#include <synfig/cairoimporter.h>
#include <synfig/rendermethod.h>

using namespace synfig;

/* === M A C R O S ========================================================= */

/* === T Y P E D E F S ===================================================== */

/* === C L A S S E S & S T R U C T S ======================================= */

class Import : public Layer_Bitmap
{
	SYNFIG_LAYER_MODULE_EXT

private:
	String filename;
	String abs_filename;
	Importer::Handle importer;
	CairoImporter::Handle cimporter;
	Time time_offset;

protected:
	Import();

public:
	~Import();

	virtual bool set_param(const String & param, const ValueBase &value);

	virtual ValueBase get_param(const String & param)const;

	virtual Vocab get_param_vocab()const;

	virtual void on_canvas_set();

	virtual void set_time(Context context, Time time)const;

	virtual void set_time(Context context, Time time, const Point &point)const;
	
	virtual void set_render_method(Context context, RenderMethod x);
};

/* === E N D =============================================================== */

#endif
