/* === S Y N F I G ========================================================= */
/*!	\file layerparamtreestore.cpp
**	\brief Template File
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

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include "layerparamtreestore.h"
#include "iconcontroller.h"
#include <gtkmm/button.h>
#include <synfig/paramdesc.h>
#include "layertree.h"
#include <synfigapp/action_system.h>
#include <synfigapp/instance.h>
#include "app.h"
#include <ETL/clock>

#include "general.h"

#endif

/* === U S I N G =========================================================== */

using namespace std;
using namespace etl;
using namespace synfig;
using namespace studio;

/* === M A C R O S ========================================================= */

class Profiler : private etl::clock
{
	const std::string name;
public:
	Profiler(const std::string& name):name(name) { reset(); }
	~Profiler() { float time(operator()()); synfig::info("%s: took %f msec",name.c_str(),time*1000); }
};

/* === G L O B A L S ======================================================= */

/* === P R O C E D U R E S ================================================= */

/* === M E T H O D S ======================================================= */

static LayerParamTreeStore::Model& ModelHack()
{
	static LayerParamTreeStore::Model* model(0);
	if(!model)model=new LayerParamTreeStore::Model;
	return *model;
}

LayerParamTreeStore::LayerParamTreeStore(etl::loose_handle<synfigapp::CanvasInterface> canvas_interface_,LayerTree* layer_tree):
	Gtk::TreeStore			(ModelHack()),
	CanvasTreeStore			(canvas_interface_),
	layer_tree				(layer_tree)
{
	queued=false;
	// Connect all the signals
	canvas_interface()->signal_value_node_changed().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_changed));
	canvas_interface()->signal_value_node_renamed().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_renamed));
	canvas_interface()->signal_value_node_added().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_added));
	canvas_interface()->signal_value_node_deleted().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_deleted));
	canvas_interface()->signal_value_node_replaced().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_replaced));
	canvas_interface()->signal_layer_param_changed().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_layer_param_changed));

	canvas_interface()->signal_value_node_child_added().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_child_added));
	canvas_interface()->signal_value_node_child_removed().connect(sigc::mem_fun(*this,&studio::LayerParamTreeStore::on_value_node_child_removed));


	layer_tree->get_selection()->signal_changed().connect(sigc::mem_fun(*this,&LayerParamTreeStore::queue_rebuild));

	signal_changed().connect(sigc::mem_fun(*this,&LayerParamTreeStore::queue_refresh));
	rebuild();
}

LayerParamTreeStore::~LayerParamTreeStore()
{
	while(!changed_connection_list.empty())
	{
		changed_connection_list.back().disconnect();
		changed_connection_list.pop_back();
	}
	synfig::info("LayerParamTreeStore::~LayerParamTreeStore(): Deleted");
}

Glib::RefPtr<LayerParamTreeStore>
LayerParamTreeStore::create(etl::loose_handle<synfigapp::CanvasInterface> canvas_interface_, LayerTree*layer_tree)
{
	return Glib::RefPtr<LayerParamTreeStore>(new LayerParamTreeStore(canvas_interface_,layer_tree));
}



void
LayerParamTreeStore::get_value_vfunc (const Gtk::TreeModel::iterator& iter, int column, Glib::ValueBase& value)const
{
	if(column<0)
	{
		synfig::error("LayerParamTreeStore::get_value_vfunc(): Bad column!");
		return;
	}

/*	if(column==model.label.index())
	{
		synfig::Layer::Handle layer((*iter)[model.layer]);

		if(!layer)return;

		Glib::Value<Glib::ustring> x;
		g_value_init(x.gobj(),x.value_type());


		if(!layer->get_description().empty())
			x.set(layer->get_description());
		else
			x.set(layer->get_local_name());

		g_value_init(value.gobj(),x.value_type());
		g_value_copy(x.gobj(),value.gobj());
	}
	else
*/
	if(column==model.label.index())
	{
		synfigapp::ValueDesc value_desc((*iter)[model.value_desc]);
		Glib::ustring label;

		if(!(*iter)[model.is_toplevel])
			return CanvasTreeStore::get_value_vfunc(iter,column,value);
		synfig::ParamDesc param_desc((*iter)[model.param_desc]);
		label=param_desc.get_local_name();

		if(!(*iter)[model.is_inconsistent])
		if(value_desc.is_value_node() && value_desc.get_value_node()->is_exported())
		{
			label+=strprintf(" (%s)",value_desc.get_value_node()->get_id().c_str());
		}

		Glib::Value<Glib::ustring> x;
		g_value_init(x.gobj(),x.value_type());

		x.set(label);

		g_value_init(value.gobj(),x.value_type());
		g_value_copy(x.gobj(),value.gobj());
	}
	else
	if(column==model.is_toplevel.index())
	{
		Glib::Value<bool> x;
		g_value_init(x.gobj(),x.value_type());

		TreeModel::Path path(get_path(iter));

		x.set(path.get_depth()<=1);

		g_value_init(value.gobj(),x.value_type());
		g_value_copy(x.gobj(),value.gobj());
	}
	else
	if(column==model.is_inconsistent.index())
	{
		if((*iter)[model.is_toplevel])
		{
			CanvasTreeStore::get_value_vfunc(iter,column,value);
			return;
		}

		Glib::Value<bool> x;
		g_value_init(x.gobj(),x.value_type());

		x.set(false);

		g_value_init(value.gobj(),x.value_type());
		g_value_copy(x.gobj(),value.gobj());
	}
	else
	CanvasTreeStore::get_value_vfunc(iter,column,value);
}



void
LayerParamTreeStore::set_value_impl(const Gtk::TreeModel::iterator& iter, int column, const Glib::ValueBase& value)
{
	//if(!iterator_sane(row))
	//	return;

	if(column>=get_n_columns_vfunc())
	{
		g_warning("LayerTreeStore::set_value_impl: Bad column (%d)",column);
		return;
	}

	if(!g_value_type_compatible(G_VALUE_TYPE(value.gobj()),get_column_type_vfunc(column)))
	{
		g_warning("LayerTreeStore::set_value_impl: Bad value type");
		return;
	}

	try
	{
		if(column==model.value.index())
		{
			Glib::Value<synfig::ValueBase> x;
			g_value_init(x.gobj(),model.value.type());
			g_value_copy(value.gobj(),x.gobj());

			if((bool)(*iter)[model.is_toplevel])
			{
				synfigapp::Action::PassiveGrouper group(canvas_interface()->get_instance().get(),_("Set Layer Params"));

				synfig::ParamDesc param_desc((*iter)[model.param_desc]);

				LayerList::iterator iter2(layer_list.begin());

				for(;iter2!=layer_list.end();++iter2)
				{
					if(!canvas_interface()->change_value(synfigapp::ValueDesc(*iter2,param_desc.get_name()),x.get()))
					{
						// ERROR!
						group.cancel();
						App::dialog_error_blocking(_("Error"),_("Unable to set all layer parameters."));

						return;
					}
				}
			}
			else
			{
				canvas_interface()->change_value((*iter)[model.value_desc],x.get());
			}
			return;
		}
		else
/*
		if(column==model.active.index())
		{
			synfig::Layer::Handle layer((*iter)[model.layer]);

			if(!layer)return;

			Glib::Value<bool> x;
			g_value_init(x.gobj(),model.active.type());
			g_value_copy(value.gobj(),x.gobj());

			synfigapp::Action::Handle action(synfigapp::Action::create("layer_activate"));

			if(!action)
				return;

			action->set_param("canvas",canvas_interface()->get_canvas());
			action->set_param("canvas_interface",canvas_interface());
			action->set_param("layer",layer);
			action->set_param("new_status",bool(x.get()));

			canvas_interface()->get_instance()->perform_action(action);
			return;
		}
		else
*/
		CanvasTreeStore::set_value_impl(iter,column, value);
	}
	catch(std::exception x)
	{
		g_warning(x.what());
	}
}










void
LayerParamTreeStore::rebuild()
{
	Profiler profiler("LayerParamTreeStore::rebuild()");
	if(queued)queued=false;
	clear();
	layer_list=layer_tree->get_selected_layers();

	if(layer_list.size()<=0)
		return;

	// Get rid of all the connections,
	// and clear the connection map.
	//while(!connection_map.empty())connection_map.begin()->second.disconnect(),connection_map.erase(connection_map.begin());
	while(!changed_connection_list.empty())
	{
		changed_connection_list.back().disconnect();
		changed_connection_list.pop_back();
	}

	struct REBUILD_HELPER
	{
		ParamVocab vocab;

		static ParamVocab::iterator find_param_desc(ParamVocab& vocab, const synfig::String& x)
		{
			ParamVocab::iterator iter;

			for(iter=vocab.begin();iter!=vocab.end();++iter)
				if(iter->get_name()==x)
					break;
			return iter;
		}

		void process_vocab(ParamVocab x)
		{
			ParamVocab::iterator iter;

			for(iter=vocab.begin();iter!=vocab.end();++iter)
			{
				ParamVocab::iterator iter2(find_param_desc(x,iter->get_name()));
				if(iter2==x.end())
				{
					// remove it and start over
					vocab.erase(iter);
					iter=vocab.begin();
					iter--;
					continue;
				}
			}
		}

	} rebuild_helper;


	{
		LayerList::iterator iter(layer_list.begin());
		rebuild_helper.vocab=(*iter)->get_param_vocab();

		for(++iter;iter!=layer_list.end();++iter)
		{
			rebuild_helper.process_vocab((*iter)->get_param_vocab());
			changed_connection_list.push_back(
				(*iter)->signal_changed().connect(
					sigc::mem_fun(
						*this,
						&LayerParamTreeStore::changed
					)
				)
			);
		}
	}

	ParamVocab::iterator iter;
	for(iter=rebuild_helper.vocab.begin();iter!=rebuild_helper.vocab.end();++iter)
	{
		if(iter->get_hidden())
			continue;

		/*
		if(iter->get_animation_only())
		{
			int length(layer_list.front()->get_canvas()->rend_desc().get_frame_end()-layer_list.front()->get_canvas()->rend_desc().get_frame_start());
			if(!length)
				continue;
		}
		*/
		Gtk::TreeRow row(*(append()));
		synfigapp::ValueDesc value_desc(layer_list.front(),iter->get_name());
		CanvasTreeStore::set_row(row,value_desc);
		if(value_desc.is_value_node())
		{
			changed_connection_list.push_back(
				value_desc.get_value_node()->signal_changed().connect(
					sigc::mem_fun(
						this,
						&LayerParamTreeStore::changed
					)
				)
			);
		}
		if(value_desc.get_value_type()==ValueBase::TYPE_CANVAS)
		{
			Canvas::Handle canvas_handle = value_desc.get_value().get(Canvas::Handle());
			if(canvas_handle) changed_connection_list.push_back(
				canvas_handle->signal_changed().connect(
					sigc::mem_fun(
						this,
						&LayerParamTreeStore::changed
					)
				)
			);
		}
		//row[model.label] = iter->get_local_name();
		row[model.param_desc] = *iter;
		row[model.canvas] = layer_list.front()->get_canvas();
		row[model.is_inconsistent] = false;
		//row[model.is_toplevel] = true;


		LayerList::iterator iter2(layer_list.begin());
		ValueBase value((*iter2)->get_param(iter->get_name()));
		for(++iter2;iter2!=layer_list.end();++iter2)
		{
			if(value!=((*iter2)->get_param(iter->get_name())))
			{
				row[model.is_inconsistent] = true;
				while(!row.children().empty() && erase(row.children().begin()));
				break;
			}
		}
	}
}

void
LayerParamTreeStore::queue_refresh()
{
	if(queued)
		return;
	queued=1;
	queue_connection.disconnect();
	queue_connection=Glib::signal_timeout().connect(
		sigc::bind_return(
			sigc::mem_fun(*this,&LayerParamTreeStore::refresh),
			false
		)
	,150);

}

void
LayerParamTreeStore::queue_rebuild()
{
	if(queued==2)
		return;
	queued=2;
	queue_connection.disconnect();
	queue_connection=Glib::signal_timeout().connect(
		sigc::bind_return(
			sigc::mem_fun(*this,&LayerParamTreeStore::rebuild),
			false
		)
	,150);

}

void
LayerParamTreeStore::refresh()
{
	if(queued)queued=false;

	Gtk::TreeModel::Children children_(children());

	Gtk::TreeModel::Children::iterator iter;

	if(!children_.empty())
		for(iter = children_.begin(); iter && iter != children_.end(); ++iter)
		{
			Gtk::TreeRow row=*iter;
			refresh_row(row);
		}
}

void
LayerParamTreeStore::refresh_row(Gtk::TreeModel::Row &row)
{
	if(row[model.is_toplevel])
	{
		row[model.is_inconsistent] = false;
		ParamDesc param_desc(row[model.param_desc]);

		LayerList::iterator iter2(layer_list.begin());
		ValueBase value((*iter2)->get_param(param_desc.get_name()));
		for(++iter2;iter2!=layer_list.end();++iter2)
		{
			if(value!=((*iter2)->get_param(param_desc.get_name())))
			{
				row[model.is_inconsistent] = true;
				while(!row.children().empty() && erase(row.children().begin()));
				return;
			}
		}
	}

	//handle<ValueNode> value_node=row[model.value_node];
	//if(value_node)
	{
		CanvasTreeStore::refresh_row(row);
		return;
	}
}

void
LayerParamTreeStore::set_row(Gtk::TreeRow row,synfigapp::ValueDesc value_desc)
{
	Gtk::TreeModel::Children children = row.children();
	while(!children.empty() && erase(children.begin()));

	CanvasTreeStore::set_row(row,value_desc);
}

void
LayerParamTreeStore::on_value_node_added(ValueNode::Handle /*value_node*/)
{
//	queue_refresh();
}

void
LayerParamTreeStore::on_value_node_deleted(etl::handle<ValueNode> /*value_node*/)
{
//	queue_refresh();
}

void
LayerParamTreeStore::on_value_node_child_added(synfig::ValueNode::Handle /*value_node*/,synfig::ValueNode::Handle /*child*/)
{
	queue_rebuild();
}

void
LayerParamTreeStore::on_value_node_child_removed(synfig::ValueNode::Handle /*value_node*/,synfig::ValueNode::Handle /*child*/)
{
	rebuild();
}

void
LayerParamTreeStore::on_value_node_changed(etl::handle<ValueNode> /*value_node*/)
{
	queue_refresh();
}

void
LayerParamTreeStore::on_value_node_renamed(synfig::ValueNode::Handle /*value_node*/)
{
	rebuild();
}

void
LayerParamTreeStore::on_value_node_replaced(synfig::ValueNode::Handle /*replaced_value_node*/,synfig::ValueNode::Handle /*new_value_node*/)
{
	queue_rebuild();
}

void
LayerParamTreeStore::on_layer_param_changed(synfig::Layer::Handle /*handle*/,synfig::String /*param_name*/)
{
	queue_refresh();
}
