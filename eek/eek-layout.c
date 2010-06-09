/* 
 * Copyright (C) 2010 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010 Red Hat, Inc.
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
 * SECTION:eek-layout
 * @short_description: Base interface of a layout engine
 *
 * The #EekLayout class is a base interface of layout engine which
 * arranges keyboard elements.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eek-layout.h"
#include "eek-keyboard.h"

static void
eek_layout_base_init (gpointer gobject_class)
{
    static gboolean is_initialized = FALSE;

    if (!is_initialized) {
        /* TODO: signals */
        is_initialized = TRUE;
    }
}

GType
eek_layout_get_type (void)
{
    static GType iface_type = 0;
    if (iface_type == 0) {
        static const GTypeInfo info = {
            sizeof (EekLayoutIface),
            eek_layout_base_init,
            NULL,
        };
        iface_type = g_type_register_static (G_TYPE_INTERFACE,
                                             "EekLayout",
                                             &info, 0);
    }
    return iface_type;
}

void
eek_layout_apply (EekLayout   *layout,
                  EekKeyboard *keyboard)
{
    g_return_if_fail (EEK_IS_LAYOUT(layout));
    EEK_LAYOUT_GET_IFACE(layout)->apply (layout, keyboard);
}

