/* === S Y N F I G ========================================================= */
/*!	\file importer.h
**	\brief It is the base class for all the importers.
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

#ifndef __SYNFIG_IMPORTER_H
#define __SYNFIG_IMPORTER_H

/* === H E A D E R S ======================================================= */

#include <cstdio>
#include <map>
#include <ETL/handle>
#include "string.h"
#include "time.h"
#include "gamma.h"

/* === M A C R O S ========================================================= */

//! Defines various variables and the create method, common for all importers.
//! To be used in the private part of the importer class definition.
#define SYNFIG_IMPORTER_MODULE_EXT public: static const char name__[], version__[], ext__[],cvs_id__[]; static Importer *create(const char *filename);

//! Sets the name of the importer.
#define SYNFIG_IMPORTER_SET_NAME(class,x) const char class::name__[]=x

//! Sets the primary file extension of the importer.
#define SYNFIG_IMPORTER_SET_EXT(class,x) const char class::ext__[]=x

//! Sets the version of the importer.
#define SYNFIG_IMPORTER_SET_VERSION(class,x) const char class::version__[]=x

//! Sets the CVS ID of the importer.
#define SYNFIG_IMPORTER_SET_CVS_ID(class,x) const char class::cvs_id__[]=x

//! Defines de implementation of the create method for the importer
//! \param filename The file name to be imported by the importer.
#define SYNFIG_IMPORTER_INIT(class) synfig::Importer* class::create(const char *filename) { return new class(filename); }

/* === T Y P E D E F S ===================================================== */

/* === C L A S S E S & S T R U C T S ======================================= */

namespace synfig {

class Surface;
class ProgressCallback;

/*!	\class Importer
**	\brief Used for importing bitmaps of various formats, including animations.
*
*	It is the base class for all the importers. It defines the has a static Book
*	pointer class that is a map for the importers factory creators and the strings
*	of the extension that the importer can understand. It allows to create the a
*	pointer to a particular importer just by using the extension of the name of file
*	to import. Also it creates a virtual member get_frame that must be declared in
*	the inherited classes.
*	\see module.h
**	\
*/
class Importer : public etl::shared_object
{
public:
	//! Type that represents a pointer to a Importer's constructor.
	//! As a pointer to the constructor, it represents a "factory" of importers.
	typedef Importer* (*Factory)(const char *filename);
	typedef std::map<String,Factory> Book;
	static Book* book_;

	static Book& book();

	//! Initializes the Import module by creating a book of importers names
	//! and its creators and the list of open importers
	static bool subsys_init();
	//! Stops the Import module by deleting the book and the list of open
	//! importers
	static bool subsys_stop();

	typedef etl::handle<Importer> Handle;
	typedef etl::loose_handle<Importer> LooseHandle;
	typedef etl::handle<const Importer> ConstHandle;

private:
	//! Gamma of the importer.
	//! \todo Do not hardcode the gamma to 2.2
	Gamma gamma_;

protected:
	Importer();

public:

	Gamma& gamma() { return gamma_; }
	const Gamma& gamma()const { return gamma_; }

	virtual ~Importer();

	//! Gets a frame and puts it into \a surface
	/*!	\param	surface Reference to surface to put frame into
	**	\param	time	For animated importers, determines which frame to get.
	**		For static importers, this parameter is unused.
	**	\param	callback Pointer to callback class for progress, errors, etc.
	**	\return \c true on success, \c false on error
	**	\see ProgressCallback, Surface
	*/
	virtual bool get_frame(Surface &surface,Time time, ProgressCallback *callback=NULL)=0;
	virtual bool get_frame(Surface &surface,Time time,
						   bool &trimmed __attribute__ ((unused)),
						   unsigned int &width __attribute__ ((unused)),
						   unsigned int &height __attribute__ ((unused)),
						   unsigned int &top __attribute__ ((unused)),
						   unsigned int &left __attribute__ ((unused)),
						   ProgressCallback *callback=NULL)
	{
		return get_frame(surface,time,callback);
	}

	//! Returns \c true if the importer pays attention to the \a time parameter of get_frame()
	virtual bool is_animated() { return false; }

	//! Attempts to open \a filename, and returns a handle to the associated Importer
	static Handle open(const String &filename);
};

}; // END of namespace synfig

/* === E N D =============================================================== */

#endif
