/* === S Y N F I G ========================================================= */
/*!	\file valuenode_greyed.h
**	\brief Header file for implementation of the "Greyed" valuenode conversion.
**
**	\legal
**	Copyright (c) 2008 Chris Moore
**  Copyright (c) 2011 Carlos López
**
**	This file is part of Synfig.
**
**	Synfig is free software: you can redistribute it and/or modify
**	it under the terms of the GNU General Public License as published by
**	the Free Software Foundation, either version 2 of the License, or
**	(at your option) any later version.
**
**	Synfig is distributed in the hope that it will be useful,
**	but WITHOUT ANY WARRANTY; without even the implied warranty of
**	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**	GNU General Public License for more details.
**
**	You should have received a copy of the GNU General Public License
**	along with Synfig.  If not, see <https://www.gnu.org/licenses/>.
**	\endlegal
*/
/* ========================================================================= */

/* === S T A R T =========================================================== */

#ifndef __SYNFIG_VALUENODE_GREYED_H
#define __SYNFIG_VALUENODE_GREYED_H

/* === H E A D E R S ======================================================= */

#include "valuenode_reference.h"

/* === M A C R O S ========================================================= */

/* === C L A S S E S & S T R U C T S ======================================= */

namespace synfig {

class ValueNode_Greyed : public ValueNode_Reference
{
public:
	typedef etl::handle<ValueNode_Greyed> Handle;
	ValueNode_Greyed(Type &x);
	ValueNode_Greyed(const ValueNode::Handle &x);

	virtual String get_name()const;
	virtual String get_local_name()const;

protected:
	LinkableValueNode* create_new()const;

public:
	static ValueNode_Greyed* create(const ValueBase &x);
	virtual Vocab get_children_vocab_vfunc()const;
}; // END of class ValueNode_Greyed

}; // END of namespace synfig

/* === E N D =============================================================== */

#endif
