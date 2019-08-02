/* 
 * Copyright (C) 2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2011 Red Hat, Inc.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

/**
 * SECTION:eek-text
 * @short_description: an #EekText represents a text symbol
 */

#include "config.h"

#include "eek-text.h"

EekSymbol *
eek_text_new (const gchar *text)
{
    EekSymbol *ret = eek_symbol_new("");
    eek_symbol_set_label(ret, text);
    ret->text = g_strdup (text);
    return ret;
}

const gchar *
eek_text_get_text (EekSymbol *text)
{
    return text->text;
}
