/* === S Y N F I G ========================================================= */
/*!	\file layer_pastecanvas.cpp
**	\brief Template Header
**
**	$Id$
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007 Chris Moore
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

#include "layer_pastecanvas.h"
#include "string.h"
#include "time.h"
#include "context.h"
#include "paramdesc.h"
#include "renddesc.h"
#include "surface.h"
#include "value.h"
#include "valuenode.h"
#include "canvas.h"

#endif

/* === U S I N G =========================================================== */

using namespace etl;
using namespace std;
using namespace synfig;

/* === M A C R O S ========================================================= */

#define MAX_DEPTH 10

//#ifdef __APPLE__
//#define SYNFIG_NO_CLIP
//#endif

/* === C L A S S E S ======================================================= */

class depth_counter	// Makes our recursive depth counter exception-safe
{
	int *depth;
public:
	depth_counter(int &x):depth(&x) { (*depth)++; }
	~depth_counter() { (*depth)--; }
};

/* === G L O B A L S ======================================================= */

SYNFIG_LAYER_INIT(Layer_PasteCanvas);
SYNFIG_LAYER_SET_NAME(Layer_PasteCanvas,"PasteCanvas"); // todo: use paste_canvas
SYNFIG_LAYER_SET_LOCAL_NAME(Layer_PasteCanvas,N_("Paste Canvas"));
SYNFIG_LAYER_SET_CATEGORY(Layer_PasteCanvas,N_("Other"));
SYNFIG_LAYER_SET_VERSION(Layer_PasteCanvas,"0.1");
SYNFIG_LAYER_SET_CVS_ID(Layer_PasteCanvas,"$Id$");

/* === M E T H O D S ======================================================= */

Layer_PasteCanvas::Layer_PasteCanvas():
	origin(0,0),
	depth(0),
	zoom(0),
	time_offset(0)
{
	children_lock=false;
	muck_with_time_=true;
	curr_time=Time::begin();
}

Layer_PasteCanvas::~Layer_PasteCanvas()
{
/*	if(canvas)
		canvas->parent_set.erase(this);
*/

	//if(canvas)DEBUGINFO(strprintf("%d",canvas->count()));

	set_sub_canvas(0);

	//if(canvas && (canvas->is_inline() || !get_canvas() || get_canvas()->get_root()!=canvas->get_root()))
	//	canvas->unref();
}

String
Layer_PasteCanvas::get_local_name()const
{
	if(!canvas)	return _("Pasted Canvas");
	if(canvas->is_inline()) return _("Inline Canvas");
	if(canvas->get_root()==get_canvas()->get_root()) return '[' + canvas->get_id() + ']';

	return '[' + canvas->get_file_name() + ']';
}

Layer::Vocab
Layer_PasteCanvas::get_param_vocab()const
{
	Layer::Vocab ret(Layer_Composite::get_param_vocab());

	ret.push_back(ParamDesc("origin")
		.set_local_name(_("Origin"))
		.set_description(_("Point where you want the origin to be"))
	);
	ret.push_back(ParamDesc("canvas")
		.set_local_name(_("Canvas"))
		.set_description(_("Canvas to paste"))
	);
	ret.push_back(ParamDesc("zoom")
		.set_local_name(_("Zoom"))
		.set_description(_("Size of canvas"))
	);

	ret.push_back(ParamDesc("time_offset")
		.set_local_name(_("Time Offset"))
	);

	ret.push_back(ParamDesc("children_lock")
		.set_local_name(_("Children Lock"))
	);

	return ret;
}

bool
Layer_PasteCanvas::set_param(const String & param, const ValueBase &value)
{
	IMPORT(origin);

	// IMPORT(canvas);
	if(param=="canvas" && value.same_type_as(Canvas::Handle()))
	{
		set_sub_canvas(value.get(Canvas::Handle()));
		return true;
	}

	// IMPORT(time_offset);
	if (param=="time_offset" && value.same_type_as(time_offset))
	{
		if (time_offset != value.get(Time()))
		{
			value.put(&time_offset);
			// notify that the time_offset has changed so we can update the
			// waypoint positions in parent layers
			changed();
		}
		return true;
	}

	IMPORT(children_lock);
	IMPORT(zoom);

	return Layer_Composite::set_param(param,value);
}

void
Layer_PasteCanvas::set_sub_canvas(etl::handle<synfig::Canvas> x)
{
	if(canvas && muck_with_time_)
		remove_child(canvas.get());

	if(canvas && (canvas->is_inline() || !get_canvas() || get_canvas()->get_root()!=canvas->get_root()))
		canvas->unref();

	child_changed_connection.disconnect();

	canvas=x;

	/*if(canvas)
		child_changed_connection=canvas->signal_changed().connect(
			sigc::mem_fun(
				*this,
				&Layer_PasteCanvas::changed
			)
		);
	*/
	if(canvas)
		bounds=(canvas->get_context().get_full_bounding_rect()-canvas->rend_desc().get_focus())*exp(zoom)+origin+canvas->rend_desc().get_focus();

	if(canvas && muck_with_time_)
		add_child(canvas.get());

	if(canvas && (canvas->is_inline() || !get_canvas() || get_canvas()->get_root()!=canvas->get_root()))
		canvas->ref();

	if(canvas)
		on_canvas_set();
}

// This is called whenever the parent canvas gets set/changed
void
Layer_PasteCanvas::on_canvas_set()
{
	//synfig::info("before count()=%d",count());
	if(get_canvas() && canvas && canvas->is_inline() && canvas->parent()!=get_canvas())
	{
		//synfig::info("during count()=%d",count());
		canvas->set_inline(get_canvas());
	}
	//synfig::info("after count()=%d",count());
}

ValueBase
Layer_PasteCanvas::get_param(const String& param)const
{
	EXPORT(origin);
	EXPORT(canvas);
	EXPORT(zoom);
	EXPORT(time_offset);
	EXPORT(children_lock);

	EXPORT_NAME();
	EXPORT_VERSION();

	return Layer_Composite::get_param(param);
}

void
Layer_PasteCanvas::set_time(Context context, Time time)const
{
	if(depth==MAX_DEPTH)return;depth_counter counter(depth);
	curr_time=time;

	context.set_time(time);
	if(canvas)
	{
		canvas->set_time(time+time_offset);

		bounds=(canvas->get_context().get_full_bounding_rect()-canvas->rend_desc().get_focus())*exp(zoom)+origin+canvas->rend_desc().get_focus();
	}
	else
		bounds=Rect::zero();
}

synfig::Layer::Handle
Layer_PasteCanvas::hit_check(synfig::Context context, const synfig::Point &pos)const
{
	if(depth==MAX_DEPTH)return 0;depth_counter counter(depth);

	if (canvas) {
		Point target_pos=(pos-canvas->rend_desc().get_focus()-origin)/exp(zoom)+canvas->rend_desc().get_focus();

		if(canvas && get_amount() && canvas->get_context().get_color(target_pos).get_a()>=0.25)
		{
			if(!children_lock)
			{
				return canvas->get_context().hit_check(target_pos);
			}
			return const_cast<Layer_PasteCanvas*>(this);
		}
	}
	return context.hit_check(pos);
}

Color
Layer_PasteCanvas::get_color(Context context, const Point &pos)const
{
	if(!canvas || !get_amount())
		return context.get_color(pos);

	if(depth==MAX_DEPTH)return Color::alpha();depth_counter counter(depth);

	Point target_pos=(pos-canvas->rend_desc().get_focus()-origin)/exp(zoom)+canvas->rend_desc().get_focus();

	return Color::blend(canvas->get_context().get_color(target_pos),context.get_color(pos),get_amount(),get_blend_method());
}


bool
Layer_PasteCanvas::accelerated_render(Context context,Surface *surface,int quality, const RendDesc &renddesc, ProgressCallback *cb)const
{
	if(cb && !cb->amount_complete(0,10000)) return false;

	if(depth==MAX_DEPTH)
	{
		DEBUGPOINT();
		// if we are at the extent of our depth,
		// then we should just return whatever is under us.
		return context.accelerated_render(surface,quality,renddesc,cb);
	}
	depth_counter counter(depth);

	if(!canvas || !get_amount())
		return context.accelerated_render(surface,quality,renddesc,cb);

	if(muck_with_time_ && curr_time!=Time::begin() && canvas->get_time()!=curr_time+time_offset)
	{
		canvas->set_time(curr_time+time_offset);
	}

	SuperCallback stageone(cb,0,4500,10000);
	SuperCallback stagetwo(cb,4500,9000,10000);
	SuperCallback stagethree(cb,9000,9999,10000);

	RendDesc desc(renddesc);
	Vector::value_type zoomfactor=1.0/exp(zoom);
	desc.clear_flags();
	desc.set_tl((desc.get_tl()-canvas->rend_desc().get_focus()-origin)*zoomfactor+canvas->rend_desc().get_focus());
	desc.set_br((desc.get_br()-canvas->rend_desc().get_focus()-origin)*zoomfactor+canvas->rend_desc().get_focus());
	desc.set_flags(RendDesc::PX_ASPECT);

	if(is_solid_color() || context->empty())
	{
		surface->set_wh(renddesc.get_w(),renddesc.get_h());
		surface->clear();
	}
	else if(!context.accelerated_render(surface,quality,renddesc,&stageone))
		return false;
	Color::BlendMethod blend_method(get_blend_method());

	const Rect full_bounding_rect(canvas->get_context().get_full_bounding_rect());

	if(context->empty())
	{
		if(Color::is_onto(blend_method))
			return true;

		if(blend_method==Color::BLEND_COMPOSITE)
			blend_method=Color::BLEND_STRAIGHT;
	}
	else
	if(!etl::intersect(context.get_full_bounding_rect(),full_bounding_rect+origin))
	{
		if(Color::is_onto(blend_method))
			return true;

		if(blend_method==Color::BLEND_COMPOSITE)
			blend_method=Color::BLEND_STRAIGHT;
	}

#ifndef SYNFIG_NO_CLIP
	{
		//synfig::info("PasteCanv Clip");
		Rect area(desc.get_rect()&full_bounding_rect);

		Point min(area.get_min());
		Point max(area.get_max());

		if(desc.get_tl()[0]>desc.get_br()[0])
			swap(min[0],max[0]);
		if(desc.get_tl()[1]>desc.get_br()[1])
			swap(min[1],max[1]);

		const int
			x(floor_to_int((min[0]-desc.get_tl()[0])/desc.get_pw())),
			y(floor_to_int((min[1]-desc.get_tl()[1])/desc.get_ph())),
			w(ceil_to_int((max[0]-desc.get_tl()[0])/desc.get_pw())-x),
			h(ceil_to_int((max[1]-desc.get_tl()[1])/desc.get_ph())-y);

		desc.set_subwindow(x,y,w,h);

		Surface pastesurface;

		// \todo this used to also have "area.area()<=0.000001 || " - is it useful?
		//		 it was causing bug #1809480 (Zoom in beyond 8.75 in nested canvases fails)
		if(desc.get_w()==0 || desc.get_h()==0)
		{
			if(cb && !cb->amount_complete(10000,10000)) return false;

			return true;
		}

		if(!canvas->get_context().accelerated_render(&pastesurface,quality,desc,&stagetwo))
			return false;

		Surface::alpha_pen apen(surface->get_pen(x,y));

		apen.set_alpha(get_amount());
		apen.set_blend_method(blend_method);

		pastesurface.blit_to(apen);
	}
#else
	{
		Surface pastesurface;

		if(!canvas->get_context().accelerated_render(&pastesurface,quality,desc,&stagetwo))
			return false;

		Surface::alpha_pen apen(surface->begin());

		apen.set_alpha(get_amount());
		apen.set_blend_method(blend_method);

		pastesurface.blit_to(apen);
	}
#endif

	if(cb && !cb->amount_complete(10000,10000)) return false;

	return true;
}

Rect
Layer_PasteCanvas::get_bounding_rect()const
{
	return bounds;
}

void Layer_PasteCanvas::get_times_vfunc(Node::time_set &set) const
{
	Node::time_set tset;
	if(canvas) tset = canvas->get_times();

	Node::time_set::iterator i = tset.begin(), end = tset.end();

	//Make sure we offset the time...
	//! \todo: SOMETHING STILL HAS TO BE DONE WITH THE OTHER DIRECTION
	//		   (recursing down the tree needs to take this into account too...)
	for(; i != end; ++i)
		set.insert(*i
#ifdef ADJUST_WAYPOINTS_FOR_TIME_OFFSET // see node.h
				   - time_offset
#endif
			);

	Layer::get_times_vfunc(set);
}
