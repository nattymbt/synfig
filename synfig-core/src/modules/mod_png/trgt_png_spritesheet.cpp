/* === S Y N F I G ========================================================= */
/*!	\file trgt_png_spritesheet.cpp
**	\brief png_trgt Target Module
**
**	\legal
**	Copyright (c) 2002-2005 Robert B. Quattlebaum Jr., Adrian Bentley
**	Copyright (c) 2007 Chris Moore
**	Copyright (c) 2013 Moritz Grosch (LittleFox) <littlefox@fsfe.org>
**
**  Based on trgt_png.cpp
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
**
** ========================================================================= */

/* === H E A D E R S ======================================================= */

#ifdef USING_PCH
#	include "pch.h"
#else
#ifdef HAVE_CONFIG_H
#	include <config.h>
#endif

#include <synfig/localization.h>
#include <synfig/general.h>

#include <glib/gstdio.h>
#include "trgt_png_spritesheet.h"
#include <png.h>
#include <cstdio>
#include <cstring> 
#include <ETL/misc>
#include <ETL/stringf>
#include <iostream>

#endif

/* === M A C R O S ========================================================= */

using namespace synfig;
using namespace etl;

/* === G L O B A L S ======================================================= */

SYNFIG_TARGET_INIT(png_trgt_spritesheet);
SYNFIG_TARGET_SET_NAME(png_trgt_spritesheet,"png-spritesheet");
SYNFIG_TARGET_SET_EXT(png_trgt_spritesheet,"png");
SYNFIG_TARGET_SET_VERSION(png_trgt_spritesheet,"0.1");

/* === M E T H O D S ======================================================= */

void
png_trgt_spritesheet::png_out_error(png_struct *png_data,const char *msg)
{
    png_trgt_spritesheet *me=(png_trgt_spritesheet*)png_get_error_ptr(png_data);
    synfig::error(strprintf("png_trgt_spritesheet: error: %s",msg));
    me->ready=false;
}

void
png_trgt_spritesheet::png_out_warning(png_struct *png_data,const char *msg)
{
    png_trgt_spritesheet *me=(png_trgt_spritesheet*)png_get_error_ptr(png_data);
    synfig::warning(strprintf("png_trgt_spritesheet: warning: %s",msg));
    me->ready=false;
}

bool png_trgt_spritesheet::is_final_image_size_acceptable() const
{
	return !(sheet_width * sheet_height > 5000 * 2000);
}

std::string png_trgt_spritesheet::get_image_size_error_message() const
{
	return strprintf(
				_("The image is too large. It's size must be not more than 5000*2000=10000000 px. Currently it's %d*%d=%d px."),
				sheet_width, sheet_height, sheet_width * sheet_height);
}

png_trgt_spritesheet::png_trgt_spritesheet(const char *Filename, const synfig::TargetParam &params):
	ready(false),
	//initialized(false),
	imagecount(),
	lastimage(),
	numimages(),
	cur_y(0),
	cur_row(0),
	cur_col(0),
	params(params),
	color_data(0),
	sheet_width(0),
	sheet_height(0),
	in_file_pointer(0),
	out_file_pointer(0),
	cur_out_image_row(0),
	filename(Filename),
	sequence_separator(params.sequence_separator),
	overflow_buff(0)
{
	std::cout << "png_trgt_spritesheet() " << params.offset_x << " " << params.offset_y << std::endl;
}

png_trgt_spritesheet::~png_trgt_spritesheet()
{
	std::cout << "~png_trgt_spritesheet()" << std::endl;
	if (ready)
		write_png_file ();
	if (color_data)
	{
		for (unsigned int i = 0; i < sheet_height; i++)
			delete []color_data[i];
		delete []color_data;
	}
	if (overflow_buff)
		delete []overflow_buff;
}

bool
png_trgt_spritesheet::set_rend_desc(RendDesc *given_desc)
{
	std::cout << "set_rend_desc()" << std::endl;
    //given_desc->set_pixel_format(PixelFormat((int)PF_RGB|(int)PF_A));
    desc=*given_desc;
    imagecount=desc.get_frame_start();
    lastimage=desc.get_frame_end();
    numimages = (lastimage - imagecount) + 1;		

	overflow_buff = new Color[desc.get_w()];
	
	//Reset on uninitialized values
	if ((params.columns == 0) || (params.rows == 0))
	{
		std::cout << "Uninitialized sheet parameters. Reset parameters." << std::endl;
		params.columns = numimages;
		params.rows = 1;
		params.append = true;
		params.dir = TargetParam::HR;
	}

	//Break on overflow
	if (params.columns * params.rows < numimages)
	{
		std::cout << "Sheet overflow. Break." << std::endl;
		synfig::error("Bad sheet parameters. Sheet overflow.");
		return false;
	}

	std::cout << "Frame count" << numimages << std::endl;

	bool is_loaded = false;

	if (params.append)
	{
		in_file_pointer = g_fopen(filename.c_str(), "rb");
		if (!in_file_pointer)
			synfig::error(strprintf("[read_png_file] File %s could not be opened for reading", filename.c_str()));
		else
		{
			is_loaded = load_png_file();
			if (!is_loaded)
				fclose(in_file_pointer);
		}
	}
		
	//I select such size which appropriate to contain whole sprite sheet.
	unsigned int target_width = params.columns * desc.get_w() + params.offset_x;
	unsigned int target_height = params.rows * desc.get_h() + params.offset_y;
	sheet_width = in_image.width > target_width? in_image.width : target_width;
	sheet_height = in_image.height > target_height? in_image.height : target_height;

	if (!is_final_image_size_acceptable())
	{
		synfig::error(get_image_size_error_message());
		return false;
	}

	std::cout << "Sheet size: " << sheet_width << "x" << sheet_height << std::endl;

	std::cout << "Color size: " << sizeof(Color) << std::endl;
	
	color_data = new Color*[sheet_height];
	if (color_data)
		for (unsigned int i = 0; i < sheet_height; i++)
			color_data[i] = new Color[sheet_width];
	
	if (is_loaded)
		ready = read_png_file();
	else
		ready = true;
	
    return true;
}

void
png_trgt_spritesheet::end_frame()
{
	std::cout << "end_frame()" << std::endl;
		
    imagecount++;
	cur_y = 0;
	if (params.dir == TargetParam::HR)
	{
		//Horizontal render. Columns increment
		cur_col++;
		if (cur_col >= (unsigned int)params.columns)
		{
			cur_row++;
			cur_col = 0;
		}
	}
	else
	{
		//Vertical render. Rows increment.
		cur_row++;
		if (cur_row >= (unsigned int) params.rows)
		{
			cur_col++;
			cur_row = 0;
		}
	}
}

bool
png_trgt_spritesheet::start_frame(synfig::ProgressCallback *callback)
{
	synfig::info("start_frame()");
	if(!color_data) {
		if (callback && !is_final_image_size_acceptable())
			callback->error(get_image_size_error_message());
		return false;
	}

    if(callback)
		callback->task(strprintf("%s, (frame %d/%d)", filename.c_str(), 
		                         imagecount - (lastimage - numimages), numimages).c_str());
    return true;
}

Color *
png_trgt_spritesheet::start_scanline(int /*scanline*/)
{
	unsigned int y = cur_y + params.offset_y + cur_row * desc.get_h();
	unsigned int x = cur_col * desc.get_w() + params.offset_x;
	if ((x + desc.get_w() > sheet_width) || (y > sheet_height) || !color_data)
	{
		std::cout << "Buffer overflow. x: " << x << " y: " << y << std::endl;
		//TODO: Fix exception processing outside the module.
		return overflow_buff; //Spike. Bad exception processing
	}
    return &color_data[y][x];
}

bool
png_trgt_spritesheet::end_scanline()
{
	cur_y++;
    return true;
}

//The func only loads file. Reading into the buffer in read_png_file().
bool
png_trgt_spritesheet::load_png_file()
{
	std::cout << "load_png_file()" << std::endl;

    char header[8];    // 8 is the maximum size that can be checked

	//Reads header for next checking.
    int length = fread(header, 1, 8, in_file_pointer);
    if ((length != 8) || png_sig_cmp((unsigned char *)header, 0, 8))
	{
		synfig::error(strprintf("[read_png_file] File %s is not recognized as a PNG file", filename.c_str()));
		return false;
	}

    /* initialize stuff */
	in_image.png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);

    if (!in_image.png_ptr)
	{
		synfig::error("[read_png_file] png_create_read_struct failed");
		return false;
	}

    in_image.info_ptr = png_create_info_struct(in_image.png_ptr);
    if (!in_image.info_ptr)
	{
		synfig::error("[read_png_file] png_create_info_struct failed");
		return false;
	}

    if (setjmp(png_jmpbuf(in_image.png_ptr)))
	{
		synfig::error("[read_png_file] Error during init_io");
		return false;
	}

    png_init_io(in_image.png_ptr, in_file_pointer);
    png_set_sig_bytes(in_image.png_ptr, 8);

    png_read_info(in_image.png_ptr, in_image.info_ptr);

    in_image.width = png_get_image_width(in_image.png_ptr, in_image.info_ptr);
    in_image.height = png_get_image_height(in_image.png_ptr, in_image.info_ptr);
	std::cout << "Img size: " << in_image.width << "x" << in_image.height << std::endl;
    in_image.color_type = png_get_color_type(in_image.png_ptr, in_image.info_ptr);
    in_image.bit_depth = png_get_bit_depth(in_image.png_ptr, in_image.info_ptr);

    png_read_update_info(in_image.png_ptr, in_image.info_ptr);


    /* read file */
    if (setjmp(png_jmpbuf(in_image.png_ptr)))
	{
        synfig::error("[read_png_file] Error during read_image");
		return false;
	}

	return true;
}

bool 
png_trgt_spritesheet::read_png_file()
{
	std::cout << "read_png_file()" << std::endl;
	//Byte buffer for png data
	png_bytep * row_pointers;
    row_pointers = new png_bytep[in_image.height];
    for (unsigned int y = 0; y < in_image.height; y++)
            row_pointers[y] = new png_byte[png_get_rowbytes(in_image.png_ptr,in_image.info_ptr)];
	std::cout << "row_pointers created" << std::endl;
	
	png_read_image(in_image.png_ptr, row_pointers);
	std::cout << "image read" << std::endl;
	
    if (png_get_color_type(in_image.png_ptr, in_image.info_ptr) == PNG_COLOR_TYPE_RGB)
	{
        synfig::error("[process_file] input file is PNG_COLOR_TYPE_RGB but must be PNG_COLOR_TYPE_RGBA "
               "(lacks the alpha channel)");
		return false;
	}

    if (png_get_color_type(in_image.png_ptr, in_image.info_ptr) != PNG_COLOR_TYPE_RGBA)
	{
        synfig::error("[process_file] color_type of input file must be PNG_COLOR_TYPE_RGBA (%d) (is %d)",
			PNG_COLOR_TYPE_RGBA, png_get_color_type(in_image.png_ptr, in_image.info_ptr));
		return false;
	}

	std::cout << "colors checked" << std::endl;

	//From png bytes to synfig::Color conversion
	const ColorReal k = 1/255.0;
    for (unsigned int y = 0; y < in_image.height; y++) 
	{
        png_byte* row = row_pointers[y];
        for (unsigned int x = 0; x < in_image.width; x++) 
		{
            png_byte* ptr = &(row[x*4]);
			color_data[y][x].set_r(ptr[0]*k);
			color_data[y][x].set_g(ptr[1]*k);
			color_data[y][x].set_b(ptr[2]*k);
			color_data[y][x].set_a(ptr[3]*k);
        }
    }

	std::cout << "colors converted" << std::endl;
	
    for (unsigned int y = 0; y < in_image.height; y++)
            delete []row_pointers[y];
    
	delete[] row_pointers;
	std::cout << "row_pointers deleted" << std::endl;
	return true;
}

bool 
png_trgt_spritesheet::write_png_file()
{
	std::cout << "write_png_file()" << std::endl;
	png_structp png_ptr;
	png_infop info_ptr;

	
    if (filename == "-")
    	out_file_pointer=stdout;
    else
    	out_file_pointer=g_fopen(filename.c_str(), POPEN_BINARY_WRITE_TYPE);

	
    png_ptr=png_create_write_struct(PNG_LIBPNG_VER_STRING, (png_voidp)this,png_out_error, png_out_warning);
    if (!png_ptr)
    {
        synfig::error("Unable to setup PNG struct");
        fclose(out_file_pointer);
		out_file_pointer=nullptr;
        return false;
    }

	
    info_ptr= png_create_info_struct(png_ptr);
    if (!info_ptr)
    {
        synfig::error("Unable to setup PNG info struct");
        fclose(out_file_pointer);
		out_file_pointer=nullptr;
		png_destroy_write_struct(&png_ptr,(png_infopp)nullptr);
        return false;
    }

	
    if (setjmp(png_jmpbuf(png_ptr)))
    {
        synfig::error("Unable to setup longjump");
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(out_file_pointer);
		out_file_pointer=nullptr;
        return false;
    }
    png_init_io(png_ptr,out_file_pointer);
    png_set_filter(png_ptr,0,PNG_FILTER_NONE);

	
    setjmp(png_jmpbuf(png_ptr));

	png_set_IHDR(png_ptr,info_ptr,
	             sheet_width,
	             sheet_height,
	             8,
	             (get_alpha_mode()==TARGET_ALPHA_MODE_KEEP)?PNG_COLOR_TYPE_RGBA:PNG_COLOR_TYPE_RGB,
	             PNG_INTERLACE_NONE,
	             PNG_COMPRESSION_TYPE_DEFAULT,
	             PNG_FILTER_TYPE_DEFAULT);

    // Write the physical size
    png_set_pHYs(png_ptr,info_ptr,round_to_int(desc.get_x_res()),round_to_int(desc.get_y_res()),PNG_RESOLUTION_METER);

    char title      [] = "Title";
    char description[] = "Description";
    char software   [] = "Software";
    char synfig     [] = "SYNFIG";

	// Output any text info along with the file
	png_text comments[3];
	memset(comments, 0, sizeof(comments));

	comments[0].compression = PNG_TEXT_COMPRESSION_NONE;
	comments[0].key         = title;
	comments[0].text        = const_cast<char *>(get_canvas()->get_name().c_str());
	comments[0].text_length = strlen(comments[0].text);

	comments[1].compression = PNG_TEXT_COMPRESSION_NONE;
	comments[1].key         = description;
	comments[1].text        = const_cast<char *>(get_canvas()->get_description().c_str());
	comments[1].text_length = strlen(comments[1].text);

	comments[2].compression = PNG_TEXT_COMPRESSION_NONE;
	comments[2].key         = software;
	comments[2].text        = synfig;
	comments[2].text_length = strlen(comments[2].text);

	png_set_text(png_ptr, info_ptr, comments, sizeof(comments)/sizeof(png_text));

    png_write_info_before_PLTE(png_ptr, info_ptr);
    png_write_info(png_ptr, info_ptr);

	std::unique_ptr<unsigned char[]> buffer {new unsigned char[4 * sheet_width]};
    //Writing spritesheet into png image
	for (cur_out_image_row = 0; cur_out_image_row < sheet_height; cur_out_image_row++)
	{
		color_to_pixelformat(
			buffer.get(),
			color_data[cur_out_image_row],
            //PF_RGB|(get_alpha_mode()==TARGET_ALPHA_MODE_KEEP)?PF_A:PF_RGB, //Note: PF_RGB == 0
			(get_alpha_mode() == TARGET_ALPHA_MODE_KEEP) ? PF_RGB | PF_A : PF_RGB, //Note: PF_RGB == 0
			0,
			sheet_width );
		setjmp(png_jmpbuf(png_ptr));
		png_write_row(png_ptr, buffer.get());
	}

	cur_out_image_row = 0;
    if(out_file_pointer)
    {
        png_write_end(png_ptr,info_ptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        fclose(out_file_pointer);
		out_file_pointer=nullptr;

    }
	return true;
}
