/* === S Y N F I G ========================================================= */
/*!	\file valuenode_bone.cpp
**	\brief Implementation of the "Bone" valuenode conversion.
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007, 2008 Chris Moore
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

#include "valuenode_bone.h"
#include "valuenode_const.h"
#include "valuenode_animated.h"
#include "general.h"
#include "canvas.h"

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;

/* === M A C R O S ========================================================= */

// #define HIDE_BONE_FIELDS

#define GET_NODE_PARENT_NODE(node,t) (*node->get_link("parent"))(t).get(ValueNode_Bone::Handle())
#define GET_NODE_PARENT(node,t) GET_NODE_PARENT_NODE(node,t)->get_guid()
#define GET_NODE_NAME(node,t) node->get_bone_name(t)
#define GET_NODE_BONE(node,t) (*node)(t).get(Bone())

#define GET_GUID_CSTR(guid) guid.get_string().substr(0,GUID_PREFIX_LEN).c_str()
#define GET_NODE_GUID_CSTR(node) GET_GUID_CSTR(node->get_guid())
#define GET_NODE_NAME_CSTR(node,t) GET_NODE_NAME(node,t).c_str()
#define GET_NODE_BONE_CSTR(node,t) GET_NODE_BONE(node,t).c_str()
#define GET_NODE_DESC_CSTR(node,t) (node ? strprintf("%s (%s)", GET_NODE_GUID_CSTR(node), GET_NODE_NAME_CSTR(node,t)) : strprintf("%s <root>", GET_GUID_CSTR(GUID(0)))).c_str()
#define GET_NODE_PARENT_CSTR(node,t) GET_GUID_CSTR(GET_NODE_PARENT(node,t))

/* === G L O B A L S ======================================================= */

static ValueNode_Bone::CanvasMap canvas_map;
static int bone_counter;
// static map<ValueNode_Bone::Handle, Matrix> setup_matrix_map;
// static map<ValueNode_Bone::Handle, Matrix> animated_matrix_map;
static Time last_time = Time::begin();

static ValueNode_Bone_Root::Handle rooot;

/* === P R O C E D U R E S ================================================= */

struct compare_bones
{
	bool operator() (const ValueNode_Bone::LooseHandle b1, const ValueNode_Bone::LooseHandle b2) const
	{
		String n1(GET_NODE_NAME(b1,0));
		String n2(GET_NODE_NAME(b2,0));

		if (n1 < n2) return true;
		if (n1 > n2) return false;

		return b1->get_guid() < b2->get_guid();
	}
};

void
ValueNode_Bone::show_bone_map(Canvas::LooseHandle canvas, const char *file, int line, String text, Time t)
{
	if (!getenv("SYNFIG_SHOW_BONE_MAP")) return;

	BoneMap bone_map(canvas_map[canvas]);

	set<ValueNode_Bone::LooseHandle, compare_bones> bone_set;
	for (ValueNode_Bone::BoneMap::iterator iter = bone_map.begin(); iter != bone_map.end(); iter++)
		bone_set.insert(iter->second);

	printf("\n  %s:%d (%lx) %s we now have %d bones (%d unreachable):\n", file, line, ulong(canvas.get()), text.c_str(), int(bone_map.size()), int(bone_map.size() - bone_set.size()));

	for (set<ValueNode_Bone::LooseHandle>::iterator iter = bone_set.begin(); iter != bone_set.end(); iter++)
	{
		ValueNode_Bone::LooseHandle bone(*iter);
		GUID guid(bone->get_guid());
		printf("%s:%d loop 1 get_node_parent_node\n", __FILE__, __LINE__);
		ValueNode_Bone::LooseHandle parent(GET_NODE_PARENT_NODE(bone,t));
		String id;
		if (bone->is_exported()) id = String(" ") + bone->get_id();
//		printf("%s : %s (%d)\n",           		GET_GUID_CSTR(guid), GET_NODE_BONE_CSTR(bone,t), bone->rcount());
		printf("    %-20s : parent %-20s (%d refs, %d rrefs)%s\n",
			   GET_NODE_DESC_CSTR(bone,t),
			   GET_NODE_DESC_CSTR(parent,t),
			   bone->count(), bone->rcount(),
			   id.c_str());
	}
	printf("\n");
}

ValueNode_Bone::BoneMap
ValueNode_Bone::get_bone_map(Canvas::ConstHandle canvas)
{
	return canvas_map[canvas];
}

ValueNode_Bone::BoneList
ValueNode_Bone::get_ordered_bones(etl::handle<const Canvas> canvas)
{
	std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle> uses;
	std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle> is_used_by;
	BoneList current_list;

	{
		BoneMap bone_map(canvas_map[canvas]);
		for(BoneMap::const_iterator iter=bone_map.begin();iter!=bone_map.end();++iter)
		{
			ValueNode_Bone::Handle user(iter->second);
			BoneSet ref(get_bones_referenced_by(user, false));
			if (ref.empty())
			{
				printf("%s:%d %s doesn't need anybody\n", __FILE__, __LINE__,
					   user->get_bone_name(0).c_str());
				current_list.push_back(user);
			}
			else
				for(BoneSet::iterator iter=ref.begin();iter!=ref.end();++iter)
				{
					ValueNode_Bone::Handle used(*iter);
					printf("%s:%d %s is used by %s\n", __FILE__, __LINE__,
						   used->get_bone_name(0).c_str(),
						   user->get_bone_name(0).c_str());
					is_used_by.insert(make_pair(used, user));
					uses.insert(make_pair(user, used));
				}
		}
	}

	BoneList ret;
	BoneSet seen;
	BoneList new_list;

	while (current_list.size())
	{
		printf("%s:%d current_list has %zd members; we have %zd in is_used_by and %zd in uses\n",
			   __FILE__, __LINE__, current_list.size(), is_used_by.size(), uses.size());
		for(BoneList::iterator iter=current_list.begin();iter!=current_list.end();++iter)
		{
			ValueNode_Bone::Handle bone(*iter);
			printf("%s:%d bone: %s\n", __FILE__, __LINE__, bone->get_bone_name(0).c_str());
			ret.push_back(bone);

			std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator begin(is_used_by.lower_bound(bone));
			std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator end(is_used_by.upper_bound(bone));
			for (std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator iter = begin; iter != end; iter++)
			{
				ValueNode_Bone::Handle user(iter->second);
				printf("\t\t\t%s:%d user: %s\n", __FILE__, __LINE__, user->get_bone_name(0).c_str());

				// erase (user,bone) from uses
				printf("%s:%d trying to erase - searching %zd\n", __FILE__, __LINE__, uses.count(user));
				std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator begin2(uses.lower_bound(user));
				std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator end2(uses.upper_bound(user));
				std::multimap<ValueNode_Bone::Handle, ValueNode_Bone::Handle>::iterator iter2;
				for (iter2 = begin2; iter2 != end2; iter2++)
				{
					if (iter2->second == bone)
					{
						uses.erase(iter2);
						printf("%s:%d found it\n", __FILE__, __LINE__);
						break;
					}
					else
					{
						printf("no\n");
					}
				}
				if (iter2 == end2)
				{
					printf("%s:%d didn't find it?!?\n", __FILE__, __LINE__);
					assert(0);
				}

				printf("%s:%d now there are %zd\n", __FILE__, __LINE__, uses.count(user));
				if (uses.count(user) == 0)
				{
					printf("\t\t\t%s:%d adding %s\n", __FILE__, __LINE__, user->get_bone_name(0).c_str());
					new_list.push_back(user);
				}
			}
		}
		current_list = new_list;
		new_list.clear();
	}

	assert(uses.empty());

	return ret;
}

/* === M E T H O D S ======================================================= */

// this should only be used when creating the root bone
ValueNode_Bone::ValueNode_Bone():
	LinkableValueNode(ValueBase::TYPE_BONE)
{
	printf("%s:%d ValueNode_Bone::ValueNode_Bone() this line should only appear once guid %s\n", __FILE__, __LINE__, get_guid().get_string().c_str());
}

ValueNode_Bone::ValueNode_Bone(const ValueBase &value, etl::loose_handle<Canvas> canvas):
	LinkableValueNode(value.get_type())
{
	if (getenv("SYNFIG_DEBUG_BONE_CONSTRUCTORS"))
	{
		printf("\n%s:%d ------------------------------------------------------------------------\n", __FILE__, __LINE__);
		printf("%s:%d --- ValueNode_Bone() for %s at %lx---\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), ulong(this));
		printf("%s:%d ------------------------------------------------------------------------\n\n", __FILE__, __LINE__);
	}

	switch(value.get_type())
	{
	case ValueBase::TYPE_BONE:
	{
		Bone bone(value.get(Bone()));
		String name(bone.get_name());

		if (name.empty())
			name = strprintf(_("Bone %d"), ++bone_counter);

		name = unique_name(name);

		set_link("name",ValueNode_Const::create(name));
#ifndef HIDE_BONE_FIELDS
		set_link("origin",ValueNode_Const::create(bone.get_origin()));
		set_link("origin0",ValueNode_Const::create(bone.get_origin0()));
		set_link("angle",ValueNode_Const::create(bone.get_angle()));
		set_link("angle0",ValueNode_Const::create(bone.get_angle0()));
		set_link("scale",ValueNode_Const::create(bone.get_scale()));
		set_link("length",ValueNode_Const::create(bone.get_length()));
		set_link("strength",ValueNode_Const::create(bone.get_strength()));
#endif
		ValueNode_Bone::ConstHandle parent(ValueNode_Bone::Handle::cast_const(bone.get_parent()));
		if (!parent) parent = get_root_bone();
		set_link("parent",ValueNode_Const::create(ValueNode_Bone::Handle::cast_const(parent)));

		if (getenv("SYNFIG_DEBUG_SET_PARENT_CANVAS"))
			printf("%s:%d set parent canvas for bone %lx to %lx\n", __FILE__, __LINE__, ulong(this), ulong(canvas.get()));
		set_parent_canvas(canvas);

		printf("%s:%d adding to canvas_map\n", __FILE__, __LINE__);
		canvas_map[get_root_canvas()][get_guid()] = this;

		show_bone_map(get_root_canvas(), __FILE__, __LINE__, strprintf("in constructor of %s at %lx", GET_GUID_CSTR(get_guid()), ulong(this)));

		break;
	}
	default:
		throw Exception::BadType(ValueBase::type_local_name(value.get_type()));
	}

	DCAST_HACK_ENABLE();
}

void ValueNode_Bone::on_changed()
{
	if (getenv("SYNFIG_DEBUG_ON_CHANGED"))
		printf("%s:%d ValueNode_Bone::on_changed()\n", __FILE__, __LINE__);

	LinkableValueNode::on_changed();
}

LinkableValueNode*
ValueNode_Bone::create_new()const
{
	return new ValueNode_Bone(get_type());
}

ValueNode_Bone*
ValueNode_Bone::create(const ValueBase &x, Canvas::LooseHandle canvas)
{
	return new ValueNode_Bone(x, canvas);
}

ValueNode_Bone::~ValueNode_Bone()
{
	if (getenv("SYNFIG_DEBUG_BONE_CONSTRUCTORS"))
	{
		printf("\n%s:%d ------------------------------------------------------------------------\n", __FILE__, __LINE__);
		printf("%s:%d --- ~ValueNode_Bone() for %s at %lx---\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), ulong(this));
		printf("%s:%d ------------------------------------------------------------------------\n\n", __FILE__, __LINE__);
	}

	printf("%s:%d removing from canvas_map\n", __FILE__, __LINE__);
	canvas_map[get_root_canvas()].erase(get_guid());

	show_bone_map(get_root_canvas(), __FILE__, __LINE__, "in destructor");

	unlink_all();
}

void
ValueNode_Bone::set_guid(const GUID& new_guid)
{
	GUID old_guid(get_guid());
	Canvas::LooseHandle canvas(get_root_canvas());
	show_bone_map(canvas, __FILE__, __LINE__, strprintf("before changing guid from %s to %s", GET_GUID_CSTR(old_guid), GET_GUID_CSTR(new_guid)));
	LinkableValueNode::set_guid(new_guid);
	canvas_map[canvas][new_guid] = canvas_map[canvas][old_guid];
	canvas_map[canvas].erase(old_guid);
	show_bone_map(canvas, __FILE__, __LINE__, strprintf("after changing guid from %s to %s", GET_GUID_CSTR(old_guid), GET_GUID_CSTR(new_guid)));
}

void
ValueNode_Bone::set_root_canvas(etl::loose_handle<Canvas> canvas)
{
	GUID guid(get_guid());
	Canvas::LooseHandle old_canvas(get_root_canvas());
	show_bone_map(old_canvas, __FILE__, __LINE__, strprintf("before changing canvas from %lx to (%lx)", ulong(old_canvas.get()), ulong(canvas.get())));
	LinkableValueNode::set_root_canvas(canvas);
	Canvas::LooseHandle new_canvas(get_root_canvas()); // it isn't necessarily what we passed in, because set_root_canvas walks up to the root
	if (new_canvas != old_canvas)
	{
		canvas_map[new_canvas][guid] = canvas_map[old_canvas][guid];
		canvas_map[old_canvas].erase(guid);
		show_bone_map(new_canvas, __FILE__, __LINE__, strprintf("after changing canvas from %lx to %lx", ulong(old_canvas.get()), ulong(new_canvas.get())));
	}
	else
		printf("%s:%d canvases are the same\n", __FILE__, __LINE__);
}

//!Setup Transformation matrix.
//!This matrix applied to a setup point in global
//!coordinates calculates the local coordinates of
//!the point relative to the current bone.
Matrix
ValueNode_Bone::get_setup_matrix(Time t)const
{
	Point  origin0	((*origin0_	)(t).get(Point()));
	Angle  angle0	((*angle0_	)(t).get(Angle()));
	ValueNode_Bone::ConstHandle parent	(get_parent(t));

	return get_setup_matrix(t, origin0, angle0, parent);
}

Matrix
ValueNode_Bone::get_setup_matrix(Time t, Point translate, Angle rotate, ValueNode_Bone::ConstHandle parent)const
{
	return Matrix(-translate) * Matrix(-rotate) * parent->get_setup_matrix(t);
}

//!Animated Transformation matrix.
//!This matrix applied to a setup point in local
//!coordinates (the one obtained form the Setup
//!Transformation matrix) would obtain the
//!animated position of the point due the current
//!bone influence
Matrix
ValueNode_Bone::get_animated_matrix(Time t)const
{
	Real   scale	((*scale_	)(t).get(Real ()));
	Angle  angle	((*angle_	)(t).get(Angle()));
	Point  origin	((*origin_	)(t).get(Point()));
	ValueNode_Bone::ConstHandle   parent	(get_parent(t));

	return get_animated_matrix(t, scale, angle, origin, parent);
}

Matrix
ValueNode_Bone::get_animated_matrix(Time t, Real scale, Angle rotate, Point translate, ValueNode_Bone::ConstHandle parent)const
{
	return parent->get_animated_matrix(t) * Matrix(scale) * Matrix(rotate) * Matrix(translate);
}

ValueNode_Bone::ConstHandle
ValueNode_Bone::get_parent(Time t)const
{
	// check if we are an ancestor of the proposed parent
	ValueNode_Bone::ConstHandle parent((*parent_)(t).get(ValueNode_Bone::Handle()));
	if (ValueNode_Bone::ConstHandle result = is_ancestor_of(parent,t))
	{
		if (result == ValueNode_Bone::ConstHandle(this))
			synfig::error("A bone cannot be parent of itself or any of its descendants");
		else
			synfig::error("A loop was detected in the ancestry at bone %s", GET_NODE_DESC_CSTR(result,t));
		printf("%s:%d root 1\n", __FILE__, __LINE__);
		return get_root_bone();
	}

	// proposed parent is root or not a descendant of current bone
	if (parent)
	{
		return parent;
	}
	assert(0);
	return ValueNode_Bone::ConstHandle::cast_dynamic(new ValueNode_Bone_Root);
}

ValueBase
ValueNode_Bone::operator()(Time t)const
{
	if (getenv("SYNFIG_DEBUG_VALUENODE_OPERATORS"))
		printf("%s:%d operator()\n", __FILE__, __LINE__);

//	show_bone_map(get_root_canvas(), __FILE__, __LINE__, strprintf("in op() at %s", t.get_string().c_str()), t);

	String bone_name			((*name_	)(t).get(String()));
	ValueNode_Bone::ConstHandle   bone_parent			(get_parent(t)); // <-- get ValueNode_Bone_Root here
#ifndef HIDE_BONE_FIELDS
	Point  bone_origin			((*origin_	)(t).get(Point()));
	Point  bone_origin0			((*origin0_	)(t).get(Point()));
	Angle  bone_angle			((*angle_	)(t).get(Angle()));
	Angle  bone_angle0			((*angle0_	)(t).get(Angle()));
	Real   bone_scale			((*scale_	)(t).get(Real()));
	Real   bone_length			((*length_	)(t).get(Real()));
	Real   bone_strength		((*strength_)(t).get(Real()));
	Matrix bone_setup_matrix	(get_setup_matrix   (t, bone_origin0, bone_angle0, bone_parent));
	Matrix bone_animated_matrix	(get_animated_matrix(t, bone_scale,   bone_angle,  bone_origin, bone_parent));
#endif

	Bone ret;

	ret.set_name			(bone_name);
	ret.set_parent			(bone_parent.get());
#ifndef HIDE_BONE_FIELDS
	ret.set_origin			(bone_origin);
	ret.set_origin0			(bone_origin0);
	ret.set_angle			(bone_angle);
	ret.set_angle0			(bone_angle0);
	ret.set_scale			(bone_scale);
	ret.set_length			(bone_length);
	ret.set_strength		(bone_strength);
	ret.set_setup_matrix	(bone_setup_matrix);
	ret.set_animated_matrix	(bone_animated_matrix);
#endif

	return ret;					// <-- lose it here
}

ValueNode*
ValueNode_Bone::clone(Canvas::LooseHandle canvas, const GUID& deriv_guid)const
{
	String old_name;
	ValueNode_Const::Handle const_name_link;
	ValueNode::Handle name_link(get_link("name"));

	if (!name_link->is_exported())
	{
		if (const_name_link = ValueNode_Const::Handle::cast_dynamic(name_link))
		{
			String name(old_name = const_name_link->get_value().get(String()));
//			printf("got old name '%s'\n", name.c_str());
			name = unique_name(name);
//			printf("using new name '%s'\n", name.c_str());
			const_name_link->set_value(name);
		}
//		else
//			printf("%s:%d bone's name is not constant, so not editing\n", __FILE__, __LINE__);
	}
//	else
//		printf("%s:%d cloned bone's name is exported, so not editing\n", __FILE__, __LINE__);

	ValueNode* ret(LinkableValueNode::clone(canvas, deriv_guid));

	if (const_name_link)
		const_name_link->set_value(old_name);

	return ret;
}

String
ValueNode_Bone::get_name()const
{
	return "bone";
}

String
ValueNode_Bone::get_local_name()const
{
	return _("Bone");
}

String
ValueNode_Bone::get_bone_name(Time t)const
{
	return (*get_link("name"))(t).get(String());
}

bool
ValueNode_Bone::check_type(ValueBase::Type type)
{
	return type==ValueBase::TYPE_BONE;
}

bool
ValueNode_Bone::set_link_vfunc(int i,ValueNode::Handle value)
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: CHECK_TYPE_AND_SET_VALUE(name_,		ValueBase::TYPE_STRING);
#ifdef HIDE_BONE_FIELDS
	case 1:
#else
	case 1: CHECK_TYPE_AND_SET_VALUE(origin_,	ValueBase::TYPE_VECTOR);
	case 2: CHECK_TYPE_AND_SET_VALUE(origin0_,	ValueBase::TYPE_VECTOR);
	case 3: CHECK_TYPE_AND_SET_VALUE(angle_,	ValueBase::TYPE_ANGLE);
	case 4: CHECK_TYPE_AND_SET_VALUE(angle0_,	ValueBase::TYPE_ANGLE);
	case 5: CHECK_TYPE_AND_SET_VALUE(scale_,	ValueBase::TYPE_REAL);
	case 6: CHECK_TYPE_AND_SET_VALUE(length_,	ValueBase::TYPE_REAL);
	case 7: CHECK_TYPE_AND_SET_VALUE(strength_,	ValueBase::TYPE_REAL);
	case 8:
#endif
	{
		VALUENODE_CHECK_TYPE(ValueBase::TYPE_VALUENODE_BONE);

		// check for loops
		ValueNode_Bone::BoneSet parents(ValueNode_Bone::get_bones_referenced_by(value));
		if (parents.count(this))
		{
			error("creating potential loops in the bone ancestry isn't allowed");
			return false;
		}

		VALUENODE_SET_VALUE(parent_);
	}
	}
	return false;
}

ValueNode::LooseHandle
ValueNode_Bone::get_link_vfunc(int i)const
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: return name_;
#ifdef HIDE_BONE_FIELDS
	case 1: return parent_;
#else
	case 1: return origin_;
	case 2: return origin0_;
	case 3: return angle_;
	case 4: return angle0_;
	case 5: return scale_;
	case 6: return length_;
	case 7: return strength_;
	case 8: return parent_;
#endif
	}

	return 0;
}

int
ValueNode_Bone::link_count()const
{
#ifdef HIDE_BONE_FIELDS
	return 2;
#else
	return 9;
#endif
}

String
ValueNode_Bone::link_name(int i)const
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: return "name";
#ifdef HIDE_BONE_FIELDS
	case 1: return "parent";
#else
	case 1: return "origin";
	case 2: return "origin0";
	case 3: return "angle";
	case 4: return "angle0";
	case 5: return "scale";
	case 6: return "length";
	case 7: return "strength";
	case 8: return "parent";
#endif
	}

	return String();
}

String
ValueNode_Bone::link_local_name(int i)const
{
	assert(i>=0 && i<link_count());

	switch(i)
	{
	case 0: return _("Name");
#ifdef HIDE_BONE_FIELDS
	case 1: return _("Parent");
#else
	case 1: return _("Origin");
	case 2: return _("Origin0");
	case 3: return _("Angle");
	case 4: return _("Angle0");
	case 5: return _("Scale");
	case 6: return _("Length");
	case 7: return _("Strength");
	case 8: return _("Parent");
#endif
	}

	return String();
}

int
ValueNode_Bone::get_link_index_from_name(const String &name)const
{
	if (name == "name") return 0;
#ifdef HIDE_BONE_FIELDS
	if (name == "parent") return 1;
#else
	if (name == "origin") return 1;
	if (name == "origin0") return 2;
	if (name == "angle") return 3;
	if (name == "angle0") return 4;
	if (name == "scale") return 5;
	if (name == "length") return 6;
	if (name == "strength") return 7;
	if (name == "parent") return 8;
#endif

	throw Exception::BadLinkName(name);
}

ValueNode_Bone::LooseHandle
ValueNode_Bone::find(String name)const
{
	// printf("%s:%d finding '%s' : ", __FILE__, __LINE__, name.c_str());

	BoneMap bone_map(canvas_map[get_root_canvas()]);

	for (ValueNode_Bone::BoneMap::iterator iter =  bone_map.begin(); iter != bone_map.end(); iter++)
		if ((*iter->second->get_link("name"))(0).get(String()) == name)
		{
			// printf("yes\n");
			return iter->second;
		}

	// printf("no\n");
	return 0;
}

String
ValueNode_Bone::unique_name(String name)const
{
	if (!find(name))
		return name;

	// printf("%s:%d making unique name for '%s'\n", __FILE__, __LINE__, name.c_str());

	size_t last_close(name.size()-1);
	int number = -1;
	String prefix;

	do
	{
		if (name.substr(last_close) != ")")
			break;

		size_t last_open(name.rfind('('));
		if (last_open == String::npos)
			break;

		if (last_open+2 > last_close)
			break;

		String between(name.substr(last_open+1, last_close - (last_open+1)));
		String::iterator iter;
		for (iter = between.begin(); iter != between.end(); iter++)
			if (!isdigit(*iter))
				break;

		if (iter != between.end())
			break;

		prefix = name.substr(0, last_open);
		number = atoi(between.c_str());
	} while (0);

	if (number == -1)
	{
		prefix = name + " ";
		number = 2;
	}

	do
	{
		name = prefix + "(" + strprintf("%d", number++) + ")";
	} while (find(name));

	// printf("%s:%d unique name is '%s'\n", __FILE__, __LINE__, name.c_str());
	return name;
}

// checks whether the current object is an ancestor of the supplied bone
// returns a handle to NULL if it isn't
// if there's a loop in the ancestry it returns a handle to the valuenode where the loop is detected
// otherwise it returns the current object
ValueNode_Bone::ConstHandle
ValueNode_Bone::is_ancestor_of(ValueNode_Bone::ConstHandle bone, Time t)const
{
	ValueNode_Bone::ConstHandle ancestor(this);
	set<ValueNode_Bone::ConstHandle> seen;

	if (getenv("SYNFIG_DEBUG_ANCESTOR_CHECK"))
		printf("%s:%d checking whether %s is ancestor of %s\n", __FILE__, __LINE__, GET_NODE_DESC_CSTR(ancestor,t), GET_NODE_DESC_CSTR(bone,t));

	while (bone != get_root_bone())
	{
		if (bone == ancestor)
		{
			if (getenv("SYNFIG_DEBUG_ANCESTOR_CHECK"))
				printf("%s:%d bone reached us - so we are its ancestor - return true\n", __FILE__, __LINE__);
			return bone;
		}

		if (seen.count(bone))
		{
			if (getenv("SYNFIG_DEBUG_ANCESTOR_CHECK"))
				printf("%s:%d stuck in a loop - return true\n", __FILE__, __LINE__);
			return bone;
		}

		seen.insert(bone);
		bone=GET_NODE_PARENT_NODE(bone,t);

		if (getenv("SYNFIG_DEBUG_ANCESTOR_CHECK"))
			printf("%s:%d step on to parent %s\n", __FILE__, __LINE__, GET_NODE_DESC_CSTR(bone,t));
	}

	if (getenv("SYNFIG_DEBUG_ANCESTOR_CHECK"))
		printf("%s:%d reached root - return false\n", __FILE__, __LINE__);
	return 0;
}

ValueNode_Bone::BoneSet
ValueNode_Bone::get_bones_referenced_by(ValueNode::Handle value_node, bool recursive)
{
	// printf("%s:%d rec = %d\n", __FILE__, __LINE__,recursive);
	BoneSet ret;
	if (!value_node)
	{
		printf("%s:%d failed?\n", __FILE__, __LINE__);
		assert(0);
		return ret;
	}

	// if it's a ValueNode_Const
	if (ValueNode_Const::Handle value_node_const = ValueNode_Const::Handle::cast_dynamic(value_node))
	{
		ValueBase value_node(value_node_const->get_value());
		if (value_node.get_type() == ValueBase::TYPE_VALUENODE_BONE)
			if (ValueNode_Bone::Handle bone = value_node.get(ValueNode_Bone::Handle()))
			{
				// do we want to check for bone references in other bone fields or just 'parent'?
				if (recursive)
				{
					// printf("%s:%d rec\n", __FILE__, __LINE__);
					ret = get_bones_referenced_by(bone, recursive);
					// ret = get_bones_referenced_by(bone->get_link("parent"), recursive);
				}
				if (!bone->is_root())
					ret.insert(bone);
			}
		return ret;
	}

	// if it's a ValueNode_Animated
	if (ValueNode_Animated::Handle value_node_animated = ValueNode_Animated::Handle::cast_dynamic(value_node))
	{
		// ValueNode_Animated::Handle ret = ValueNode_Animated::create(ValueBase::TYPE_BONE);
		ValueNode_Animated::WaypointList list(value_node_animated->waypoint_list());
		for (ValueNode_Animated::WaypointList::iterator iter = list.begin(); iter != list.end(); iter++)
		{
//			printf("%s:%d getting bones from waypoint\n", __FILE__, __LINE__);
			BoneSet ret2(get_bones_referenced_by(iter->get_value_node(), recursive));
			ret.insert(ret2.begin(), ret2.end());
//			printf("added %d bones from waypoint to get %d\n", int(ret2.size()), int(ret.size()));
		}
//		printf("returning %d bones\n", int(ret.size()));
		return ret;
	}

	// if it's a LinkableValueNode
	if (LinkableValueNode::Handle linkable_value_node = LinkableValueNode::Handle::cast_dynamic(value_node))
	{
		for (int i = 0; i < linkable_value_node->link_count(); i++)
		{
			BoneSet ret2(get_bones_referenced_by(linkable_value_node->get_link(i), recursive));
			ret.insert(ret2.begin(), ret2.end());
		}
		return ret;
	}

	if (PlaceholderValueNode::Handle linkable_value_node = PlaceholderValueNode::Handle::cast_dynamic(value_node))
	{
		// todo: while loading we might be setting up an ancestry loop by ignoring the placeholder valuenode here
		// can we check for loops in badly formatted .sifz files somehow?
		printf("%s:%d found a placeholder - skipping loop check\n", __FILE__, __LINE__);
		return ret;
	}

	error("%s:%d BUG: bad type in valuenode '%s'", __FILE__, __LINE__, value_node->get_string().c_str());
	assert(0);
	return ret;
}

ValueNode_Bone::BoneSet
ValueNode_Bone::get_bones_affected_by(ValueNode::Handle value_node)
{
	BoneSet ret;
	set<const Node*> seen, current_nodes, new_nodes;
	int generation = 0;

//	printf("getting bones affected by %lx %s\n", ulong(value_node.get()), value_node->get_string().c_str());

	// initialise current_nodes with the node we're editing
	current_nodes.insert(value_node.get());
	do
	{
		generation++;
//		printf("generation %d has %zd nodes\n", generation, current_nodes.size());

		int count = 0;
		// loop through current_nodes
		for (set<const Node*>::iterator iter = current_nodes.begin(); iter != current_nodes.end(); iter++, count++)
		{
			// loop through the parents of each node in current_nodes
			set<Node*> node_parents((*iter)->parent_set);
//			printf("%s:%d node %d %lx (%s) has %zd parents\n", __FILE__, __LINE__, count, ulong(*iter), (*iter)->get_string().c_str(), node_parents.size());
			int count2 = 0;
			for (set<Node*>::iterator iter2 = node_parents.begin(); iter2 != node_parents.end(); iter2++, count2++)
			{
				Node* node(*iter2);
//				printf("%s:%d parent %d: %lx (%s)\n", __FILE__, __LINE__, count2, ulong(node), node->get_string().c_str());
				// for each parent we've not already seen
				if (!seen.count(node))
				{
					// note that we've seen it now
					seen.insert(node);
					// add it to the list of new nodes to loop though in the next iteration
					new_nodes.insert(node);
					// and if it's a ValueNode_Bone, add it to the set to be returned
					if (dynamic_cast<ValueNode_Bone*>(node))
					{
						ret.insert(dynamic_cast<ValueNode_Bone*>(node));
//						printf("%s:%d it's an affected bone\n", __FILE__, __LINE__);
					}
				}
			}
		}
		current_nodes = new_nodes;
		new_nodes.clear();
	} while (current_nodes.size());

//	printf("%s:%d got %zd affected bones\n", __FILE__, __LINE__, ret.size());
	return ret;
}

ValueNode_Bone::BoneSet
ValueNode_Bone::get_possible_parent_bones(ValueNode::Handle value_node)
{
	BoneSet ret;

//	printf("%s:%d which bones can be parents of %lx (%s)\n", __FILE__, __LINE__, ulong(value_node.get()), value_node->get_string().c_str());

	// which bones are we currently editing the parent of - it can be more than one due to linking
	ValueNode_Bone::BoneSet affected_bones(ValueNode_Bone::get_bones_affected_by(value_node));
//	printf("%s:%d got %zd affected bones\n", __FILE__, __LINE__, affected_bones.size());
	Canvas::LooseHandle canvas(value_node->get_root_canvas());
//	printf("%s:%d canvas %lx\n", __FILE__, __LINE__, ulong(canvas.get()));
	for (ValueNode_Bone::BoneSet::iterator iter = affected_bones.begin(); iter != affected_bones.end(); iter++)
	{
		if (!canvas)
		{
			canvas = (*iter)->get_root_canvas();
			printf("%s:%d root canvas %lx\n", __FILE__, __LINE__, ulong((*iter)->get_root_canvas().get()));
			printf("%s:%d parent canvas %lx\n", __FILE__, __LINE__, ulong((*iter)->get_parent_canvas().get()));
			printf("%s:%d ancestor canvas %lx\n", __FILE__, __LINE__, ulong((*iter)->get_non_inline_ancestor_canvas().get()));

		}
		if (canvas != (*iter)->get_root_canvas())
			warning("%s:%d multiple root canvases in affected bones: %lx and %lx", __FILE__, __LINE__,
					ulong(canvas.get()), ulong((*iter)->get_root_canvas().get()));
	}

	BoneMap bone_map(canvas_map[canvas]);

	// loop through all the bones that exist
	for (ValueNode_Bone::BoneMap::const_iterator iter=bone_map.begin(); iter!=bone_map.end(); iter++)
	{
		ValueNode_Bone::Handle bone_value_node(iter->second);

		// if the bone would be affected by our editing, skip it - it would cause a loop if the user selected it
		if (affected_bones.count(bone_value_node.get()))
			continue;

		// loop through the list of bones referenced by this bone's parent link;
		// if any of them would be affected by editing the cell, don't offer this bone in the menu
		{
			ValueNode_Bone::BoneSet parents(ValueNode_Bone::get_bones_referenced_by(bone_value_node->get_link("parent")));

			ValueNode_Bone::BoneSet::iterator iter;
			for (iter = parents.begin(); iter != parents.end(); iter++)
				if (affected_bones.count(iter->get()))
					break;
			if (iter == parents.end())
				ret.insert(bone_value_node);
		}
	}

//	printf("%s:%d returning %zd possible parents\n", __FILE__, __LINE__, ret.size());
	return ret;
}

ValueNode_Bone::Handle
ValueNode_Bone::get_root_bone()
{
	if (!rooot) rooot = new ValueNode_Bone_Root();
	return rooot;
}

#ifdef _DEBUG
void
ValueNode_Bone::ref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s   ref valuenode_bone %*s -> %2d\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), (count()*2), "", count()+1);

	LinkableValueNode::ref();
}

bool
ValueNode_Bone::unref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s unref valuenode_bone %*s%2d <-\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), ((count()-1)*2), "", count()-1);

	return LinkableValueNode::unref();
}

void
ValueNode_Bone::rref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s               rref valuenode_bone %d -> ", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), rcount());

	LinkableValueNode::rref();

	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%d\n", rcount());
}

void
ValueNode_Bone::runref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s             runref valuenode_bone %d -> ", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), rcount());

	LinkableValueNode::runref();

	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%d\n", rcount());
}
#endif

// ------------------------------------------------------------------------

ValueNode_Bone_Root::ValueNode_Bone_Root()
{
	printf("%s:%d ValueNode_Bone_Root::ValueNode_Bone_Root()\n", __FILE__, __LINE__);
}

ValueNode_Bone_Root::~ValueNode_Bone_Root()
{
	printf("%s:%d ValueNode_Bone_Root::~ValueNode_Bone_Root()\n", __FILE__, __LINE__);
}

ValueBase
ValueNode_Bone_Root::operator()(Time t __attribute__ ((unused)))const
{
	Bone ret;
	ret.set_name			(get_local_name());
	ret.set_parent			(0);
	return ret;
}

void
ValueNode_Bone_Root::set_guid(const GUID& new_guid)
{
	printf("%s:%d bypass set_guid() for root bone\n", __FILE__, __LINE__);
	LinkableValueNode::set_guid(new_guid);
}

void
ValueNode_Bone_Root::set_root_canvas(etl::loose_handle<Canvas> canvas)
{
	printf("%s:%d bypass set_root_canvas() for root bone\n", __FILE__, __LINE__);
	LinkableValueNode::set_root_canvas(canvas);
}

ValueNode_Bone*
ValueNode_Bone_Root::create(const ValueBase &x __attribute__ ((unused)))
{
	return get_root_bone().get();
}

String
ValueNode_Bone_Root::get_name()const
{
	return "bone_root";
}

String
ValueNode_Bone_Root::get_local_name()const
{
	return _("Root");
}

String
ValueNode_Bone_Root::get_bone_name(Time t __attribute__ ((unused)))const
{
	return get_local_name();
}

int
ValueNode_Bone_Root::link_count()const
{
	return 0;
}

LinkableValueNode*
ValueNode_Bone_Root::create_new()const
{
	assert(0);
	return rooot.get();
}

Matrix
ValueNode_Bone_Root::get_setup_matrix(Time t __attribute__ ((unused)))const
{
	return Matrix();
}

Matrix
ValueNode_Bone_Root::get_animated_matrix(Time t __attribute__ ((unused)))const
{
	return Matrix();
}

bool
ValueNode_Bone_Root::check_type(ValueBase::Type type __attribute__ ((unused)))
{
	return false;
}

#ifdef _DEBUG
void
ValueNode_Bone_Root::ref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s   ref valuenode_bone_root %*s -> %2d\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), (count()*2), "", count()+1);

	LinkableValueNode::ref();
}

bool
ValueNode_Bone_Root::unref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s unref valuenode_bone_root %*s%2d <-\n", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), ((count()-1)*2), "", count()-1);

	return LinkableValueNode::unref();
}

void
ValueNode_Bone_Root::rref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s               rref valuenode_bone_root %d -> ", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), rcount());

	LinkableValueNode::rref();

	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%d\n", rcount());
}

void
ValueNode_Bone_Root::runref()const
{
	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%s:%d %s             runref valuenode_bone_root %d -> ", __FILE__, __LINE__, GET_GUID_CSTR(get_guid()), rcount());

	LinkableValueNode::runref();

	if (getenv("SYNFIG_DEBUG_BONE_REFCOUNT"))
		printf("%d\n", rcount());
}
#endif

