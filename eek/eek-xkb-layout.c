/* 
 * Copyright (C) 2006 Sergey V. Udaltsov <svu@gnome.org>
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
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
 * SECTION:eek-xkb-layout
 * @short_description: Layout engine using XKB configuration
 *
 * The #EekXkbLayout inherits #EekLayout class and arranges keyboard
 * elements using XKB.
 */

#include <gdk/gdkx.h>
#include <X11/XKBlib.h>
#include <X11/extensions/XKBgeom.h>
#include <string.h>
#include <stdarg.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eek-xkb-layout.h"
#include "eek-keyboard.h"
#include "eek-section.h"
#include "eek-key.h"
#include "eek-keysym.h"

#define noKBDRAW_DEBUG

G_DEFINE_TYPE (EekXkbLayout, eek_xkb_layout, EEK_TYPE_LAYOUT);

#define EEK_XKB_LAYOUT_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEK_TYPE_XKB_LAYOUT, EekXkbLayoutPrivate))

enum {
    PROP_0,
    PROP_KEYCODES,
    PROP_GEOMETRY,
    PROP_SYMBOLS,
    PROP_LAST
};

struct _EekXkbLayoutPrivate
{
    /* Configuration names that should synch'ed to the symbolic names
       in priv->xkb->names.  Since we use GLib's memory allocator,
       don't store any address returned from the X server here. */
    XkbComponentNamesRec names;

    Display *display;

    /* Actual XKB configuration of DISPLAY. */
    XkbDescRec *xkb;

    /* Hash table to cache orefs by shape address. */
    GHashTable *shape_oref_hash;

    gint scale_numerator;
    gint scale_denominator;
};

static guint
find_keycode (EekXkbLayout *layout, gchar *key_name);

static void
get_keyboard (EekXkbLayout *layout);

static void
get_names (EekXkbLayout *layout);

static void
setup_scaling (EekXkbLayout *layout,
               gdouble       width,
               gdouble       height);

G_INLINE_FUNC gint
xkb_to_pixmap_coord (EekXkbLayout *layout,
                     gint          n)
{
    EekXkbLayoutPrivate *priv = layout->priv;
    return n * priv->scale_numerator / priv->scale_denominator;
}

G_INLINE_FUNC gdouble
xkb_to_pixmap_double (EekXkbLayout *layout,
                     gdouble       d)
{
    EekXkbLayoutPrivate *priv = layout->priv;
    return d * priv->scale_numerator / priv->scale_denominator;
}

static void
create_key (EekXkbLayout *layout,
            EekKeyboard  *keyboard,
            EekSection   *section,
            gint          column,
            gint          row,
            gdouble       x,
            gdouble       y,
            XkbKeyRec    *xkbkey)
{
    XkbGeometryRec *xkbgeometry;
    XkbBoundsRec *xkbbounds;
    XkbShapeRec *xkbshape;
    XkbOutlineRec *xkboutline;
    EekXkbLayoutPrivate *priv = layout->priv;
    EekKey *key;
    EekBounds bounds;
    EekSymbolMatrix *matrix = NULL;
    gchar name[XkbKeyNameLength + 1];
    KeyCode keycode;
    gint num_groups, num_levels, num_symbols;
    gulong oref;

    xkbgeometry = priv->xkb->geom;
    xkbshape = &xkbgeometry->shapes[xkbkey->shape_ndx];
    oref = (gulong)g_hash_table_lookup (priv->shape_oref_hash, xkbshape);
    if (oref == 0) {
        EekOutline *outline;

        xkboutline = xkbshape->primary == NULL ? &xkbshape->outlines[0] :
            xkbshape->primary;

        outline = g_slice_new (EekOutline);
        outline->corner_radius = xkb_to_pixmap_coord(layout, xkboutline->corner_radius);

        if (xkboutline->num_points <= 2) { /* rectangular */
            gdouble x1, y1, x2, y2;

            outline->num_points = 4;
            outline->points = g_slice_alloc0 (sizeof (EekPoint) *
                                              outline->num_points);
            if (xkboutline->num_points == 1) {
                x1 = xkb_to_pixmap_coord(layout, xkbshape->bounds.x1);
                y1 = xkb_to_pixmap_coord(layout, xkbshape->bounds.y1);
                x2 = xkb_to_pixmap_coord(layout, xkboutline->points[0].x);
                y2 = xkb_to_pixmap_coord(layout, xkboutline->points[0].y);
            } else {
                x1 = xkb_to_pixmap_coord(layout, xkboutline->points[0].x);
                y1 = xkb_to_pixmap_coord(layout, xkboutline->points[0].y);
                x2 = xkb_to_pixmap_coord(layout, xkboutline->points[1].x);
                y2 = xkb_to_pixmap_coord(layout, xkboutline->points[1].y);
            }
            outline->points[0].x = outline->points[3].x = x1;
            outline->points[0].y = outline->points[1].y = y1;
            outline->points[1].x = outline->points[2].x = x2;
            outline->points[2].y = outline->points[3].y = y2;
        } else {                /* polygon */
            gint i;

            outline->num_points = xkboutline->num_points;
            outline->points = g_new0 (EekPoint, outline->num_points);
            for (i = 0; i < xkboutline->num_points; i++) {
                outline->points[i].x =
                    xkb_to_pixmap_coord(layout, xkboutline->points[i].x);
                outline->points[i].y =
                    xkb_to_pixmap_coord(layout, xkboutline->points[i].y);
            }
        }
        oref = eek_keyboard_add_outline (keyboard, outline);
        g_hash_table_insert (priv->shape_oref_hash, xkbshape, (gpointer)oref);
    }

    memset (name, 0, sizeof name);
    memcpy (name, xkbkey->name.name, sizeof name - 1);

    xkbbounds = &xkbgeometry->shapes[xkbkey->shape_ndx].bounds;
    bounds.x = xkb_to_pixmap_coord(layout, xkbbounds->x1 + x);
    bounds.y = xkb_to_pixmap_coord(layout, xkbbounds->y1 + y);
    bounds.width = xkb_to_pixmap_coord(layout, xkbbounds->x2 - xkbbounds->x1);
    bounds.height = xkb_to_pixmap_coord(layout, xkbbounds->y2 - xkbbounds->y1);

    keycode = find_keycode (layout, name);
    if (keycode == EEK_INVALID_KEYCODE) {
        num_groups = num_levels = 0;
        matrix = eek_symbol_matrix_new (0, 0);
    } else {
        KeySym keysym;
        gint i, j;

        num_groups = XkbKeyNumGroups (priv->xkb, keycode);
        num_levels = XkbKeyGroupsWidth (priv->xkb, keycode);
        num_symbols = num_groups * num_levels;
        matrix = eek_symbol_matrix_new (num_groups, num_levels);
        for (i = 0; i < num_groups; i++)
            for (j = 0; j < num_levels; j++) {
                EekModifierType modifier;

                keysym = XkbKeySymEntry (priv->xkb, keycode, j, i);
                modifier = XkbKeysymToModifiers (priv->display, keysym);
                matrix->data[i * num_levels + j] =
                    EEK_SYMBOL(eek_keysym_new_with_modifier (keysym,
                                                             modifier));
            }
    }

    key = eek_section_create_key (section, column, row);
    eek_element_set_name (EEK_ELEMENT(key), name);
    eek_element_set_bounds (EEK_ELEMENT(key), &bounds);
    eek_key_set_keycode (key, keycode);
    eek_key_set_symbol_matrix (key, matrix);
    eek_symbol_matrix_free (matrix);
    eek_key_set_oref (key, oref);
}

static void
create_section (EekXkbLayout  *layout,
                EekKeyboard   *keyboard,
                XkbSectionRec *xkbsection)
{
    XkbGeometryRec *xkbgeometry;
    EekXkbLayoutPrivate *priv;
    EekSection *section;
    EekBounds bounds;
    gchar *name;
    gfloat left, top;
    gint i, j;

    bounds.x = xkb_to_pixmap_coord(layout, xkbsection->left);
    bounds.y = xkb_to_pixmap_coord(layout, xkbsection->top);
    bounds.width = xkb_to_pixmap_coord(layout, xkbsection->width);
    bounds.height = xkb_to_pixmap_coord(layout, xkbsection->height);

    priv = layout->priv;
    xkbgeometry = priv->xkb->geom;
    section = eek_keyboard_create_section (keyboard);
    name = XGetAtomName (priv->display, xkbsection->name);
    eek_element_set_name (EEK_ELEMENT(section), name);
    XFree (name);
    eek_element_set_bounds (EEK_ELEMENT(section), &bounds);
    eek_section_set_angle (section,
                           /* angle is in tenth of degree */
                           xkbsection->angle / 10);

    for (i = 0; i < xkbsection->num_rows; i++) {
        XkbRowRec *xkbrow;

        xkbrow = &xkbsection->rows[i];
        left = xkbrow->left;
        top = xkbrow->top;
        eek_section_add_row (section,
                             xkbrow->num_keys,
                             xkbrow->vertical ?
                             EEK_ORIENTATION_VERTICAL :
                             EEK_ORIENTATION_HORIZONTAL);
        for (j = 0; j < xkbrow->num_keys; j++) {
            XkbKeyRec *xkbkey;
            XkbBoundsRec *xkbbounds;

            xkbkey = &xkbrow->keys[j];
            if (xkbrow->vertical)
                top += xkbkey->gap;
            else
                left += xkbkey->gap;
            create_key (layout, keyboard, section, j, i, left, top, xkbkey);
            xkbbounds = &xkbgeometry->shapes[xkbkey->shape_ndx].bounds;
            if (xkbrow->vertical)
                top += xkbbounds->y2 - xkbbounds->y1;
            else
                left += xkbbounds->x2 - xkbbounds->x1;
        }
    }
}

static void
create_keyboard (EekXkbLayout *layout, EekKeyboard *keyboard)
{
    EekXkbLayoutPrivate *priv = layout->priv;
    XkbGeometryRec *xkbgeometry;
    EekBounds bounds;
    gint i;

    g_return_if_fail (priv->xkb);
    g_return_if_fail (priv->xkb->geom);

    xkbgeometry = priv->xkb->geom;

    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);
    setup_scaling (EEK_XKB_LAYOUT(layout), bounds.width, bounds.height);

    bounds.x = bounds.y = 0;
    bounds.width = xkb_to_pixmap_coord(layout, xkbgeometry->width_mm);
    bounds.height = xkb_to_pixmap_coord(layout, xkbgeometry->height_mm);

    for (i = 0; i < xkbgeometry->num_sections; i++) {
        XkbSectionRec *xkbsection;

        xkbsection = &xkbgeometry->sections[i];
        create_section (layout, keyboard, xkbsection);
    }
    eek_element_set_bounds (EEK_ELEMENT(keyboard), &bounds);
}

static EekKeyboard *
eek_xkb_layout_real_create_keyboard (EekLayout *self,
                                     gdouble    initial_width,
                                     gdouble    initial_height)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (self);
    EekBounds bounds;
    EekKeyboard *keyboard;

    keyboard = g_object_new (EEK_TYPE_KEYBOARD, "layout", self, NULL);
    bounds.x = bounds.y = 0.0;
    bounds.width = initial_width;
    bounds.height = initial_height;
    eek_element_set_bounds (EEK_ELEMENT(keyboard), &bounds);

    eek_keyboard_set_num_lock_mask (keyboard,
                                    XkbKeysymToModifiers (priv->display,
                                                          XK_Num_Lock));

    if (priv->shape_oref_hash)
        g_hash_table_destroy (priv->shape_oref_hash);

    priv->shape_oref_hash = g_hash_table_new (g_direct_hash, g_direct_equal);
    create_keyboard (EEK_XKB_LAYOUT(self), keyboard);
    g_hash_table_destroy (priv->shape_oref_hash);

    return keyboard;
}

static void
eek_xkb_layout_finalize (GObject *object)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (object);

    g_free (priv->names.keycodes);
    g_free (priv->names.geometry);
    g_free (priv->names.symbols);
    XkbFreeKeyboard (priv->xkb, 0, TRUE);	/* free_all = TRUE */
    G_OBJECT_CLASS (eek_xkb_layout_parent_class)->finalize (object);
}

static void 
eek_xkb_layout_set_property (GObject      *object, 
                               guint         prop_id,
                               const GValue *value, 
                               GParamSpec   *pspec)
{
    const gchar *name;

    switch (prop_id) 
        {
        case PROP_KEYCODES:
            name = g_value_get_string (value);
            eek_xkb_layout_set_keycodes (EEK_XKB_LAYOUT(object), name);
            break;
        case PROP_GEOMETRY:
            name = g_value_get_string (value);
            eek_xkb_layout_set_geometry (EEK_XKB_LAYOUT(object), name);
            break;
        case PROP_SYMBOLS:
            name = g_value_get_string (value);
            eek_xkb_layout_set_symbols (EEK_XKB_LAYOUT(object), name);
            break;
        default:
            g_object_set_property (object,
                                   g_param_spec_get_name (pspec),
                                   value);
            break;
        }
}

static void 
eek_xkb_layout_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
    const gchar *name;

    switch (prop_id) 
        {
        case PROP_KEYCODES:
            name = eek_xkb_layout_get_keycodes (EEK_XKB_LAYOUT(object));
            g_value_set_string (value, name);
            break;
        case PROP_GEOMETRY:
            name = eek_xkb_layout_get_geometry (EEK_XKB_LAYOUT(object));
            g_value_set_string (value, name);
            break;
        case PROP_SYMBOLS:
            name = eek_xkb_layout_get_symbols (EEK_XKB_LAYOUT(object));
            g_value_set_string (value, name);
            break;
        default:
            g_object_get_property (object,
                                   g_param_spec_get_name (pspec),
                                   value);
            break;
        }
}

static void
eek_xkb_layout_class_init (EekXkbLayoutClass *klass)
{
    EekLayoutClass *layout_class = EEK_LAYOUT_CLASS (klass);
    GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec *pspec;

    g_type_class_add_private (gobject_class, sizeof (EekXkbLayoutPrivate));

    layout_class->create_keyboard = eek_xkb_layout_real_create_keyboard;

    gobject_class->finalize = eek_xkb_layout_finalize;
    gobject_class->set_property = eek_xkb_layout_set_property;
    gobject_class->get_property = eek_xkb_layout_get_property;

    pspec = g_param_spec_string ("keycodes",
				 "Keycodes",
				 "XKB keycodes component name",
				 NULL,
				 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_KEYCODES, pspec);

    pspec = g_param_spec_string ("geometry",
                                 "Geometry",
                                 "XKB geometry component name",
                                 NULL,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_GEOMETRY, pspec);

    pspec = g_param_spec_string ("symbols",
                                 "Symbols",
                                 "XKB symbols component name",
                                 NULL,
                                 G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class, PROP_SYMBOLS, pspec);
}

static void
eek_xkb_layout_init (EekXkbLayout *self)
{
    EekXkbLayoutPrivate *priv;

    priv = self->priv = EEK_XKB_LAYOUT_GET_PRIVATE (self);
    memset (&priv->names, 0, sizeof priv->names);

    priv->display = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
    g_return_if_fail (priv->display);

    /* XXX: XkbClientMapMask | XkbIndicatorMapMask | XkbNamesMask |
       XkbGeometryMask */
    priv->xkb = XkbGetKeyboard (priv->display,
                                XkbGBN_GeometryMask |
                                XkbGBN_KeyNamesMask |
                                XkbGBN_OtherNamesMask |
                                XkbGBN_SymbolsMask |
                                XkbGBN_IndicatorMapMask,
                                XkbUseCoreKbd);

    if (priv->xkb == NULL) {
        g_critical ("XkbGetKeyboard failed to get keyboard from the server!");
        return;
    }
    get_names (self);
}

static void
get_names (EekXkbLayout *layout)
{
    EekXkbLayoutPrivate *priv = layout->priv;
    gchar *name;

    XkbGetNames (priv->display, XkbAllNamesMask, priv->xkb);

    if (priv->xkb->names->keycodes <= 0)
        g_warning ("XKB keycodes setting is not loaded properly");
    else {
        name = XGetAtomName (priv->display, priv->xkb->names->keycodes);
        if (!name)
            g_warning ("Can't get the name of keycodes");
        else if (!priv->names.keycodes ||
                 g_strcmp0 (name, priv->names.keycodes)) {
            g_free (priv->names.keycodes);
            priv->names.keycodes = g_strdup (name);
            XFree (name);
        }
    }

    if (priv->xkb->names->geometry <= 0)
        g_warning ("XKB geometry setting is not loaded");
    else {
        name = XGetAtomName (priv->display, priv->xkb->names->geometry);
        if (!name)
            g_warning ("Can't get the name of geometry");
        else if (!priv->names.geometry ||
                 g_strcmp0 (name, priv->names.geometry)) {
            g_free (priv->names.geometry);
            priv->names.geometry = g_strdup (name);
            XFree (name);
        }
    }

    if (priv->xkb->names->symbols <= 0)
        g_warning ("XKB symbols setting is not loaded");
    else {
        name = XGetAtomName (priv->display, priv->xkb->names->symbols);
        if (!name)
            g_warning ("Can't get the name of symbols");
        else if (!priv->names.symbols ||
                 g_strcmp0 (name, priv->names.symbols)) {
            g_free (priv->names.symbols);
            priv->names.symbols = g_strdup (name);
            XFree (name);
        }
    }
}

/**
 * eek_xkb_layout_new:
 *
 * Create a new #EekXkbLayout.
 */
EekLayout *
eek_xkb_layout_new (void)
{
    EekXkbLayout *layout;

    layout = g_object_new (EEK_TYPE_XKB_LAYOUT, NULL);
    g_return_val_if_fail (layout, NULL);

    get_keyboard (layout);
    if (layout->priv->xkb == NULL) {
        g_object_unref (layout);
        return NULL;
    }
    return EEK_LAYOUT(layout);
}

/**
 * eek_xkb_layout_set_names: (skip)
 * @layout: an #EekXkbLayout
 * @names: XKB component names
 *
 * Set the XKB component names to @layout.
 * Returns: %TRUE if any of the component names changed, %FALSE otherwise
 */
gboolean
eek_xkb_layout_set_names (EekXkbLayout *layout, XkbComponentNamesRec *names)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);
    gboolean retval;

    g_return_val_if_fail (priv, FALSE);

    if (g_strcmp0 (names->keycodes, priv->names.keycodes)) {
        g_free (priv->names.keycodes);
        priv->names.keycodes = g_strdup (names->keycodes);
        retval = TRUE;
    }

    if (g_strcmp0 (names->geometry, priv->names.geometry)) {
        g_free (priv->names.geometry);
        priv->names.geometry = g_strdup (names->geometry);
        retval = TRUE;
    }

    if (g_strcmp0 (names->symbols, priv->names.symbols)) {
        g_free (priv->names.symbols);
        priv->names.symbols = g_strdup (names->symbols);
        retval = TRUE;
    }

    get_keyboard (layout);
    g_assert (priv->xkb);

    return retval;
}

/**
 * eek_xkb_layout_set_names_full:
 * @layout: an #EekXkbLayout
 * @Varargs: pairs of component name and value, terminated by NULL.
 *
 * Set the XKB component names to @layout.  This function is merely a
 * wrapper around eek_xkb_layout_set_names() to avoid passing a
 * pointer of XkbComponentNamesRec, which is not currently available
 * in the gobject-introspection repository.
 *
 * Available component names are: keymap, keycodes, types, compat,
 * symbols, geometry.
 *
 * Returns: %TRUE if the component name is successfully set, %FALSE otherwise
 * Since: 0.0.2
 */
gboolean
eek_xkb_layout_set_names_full (EekXkbLayout *layout,
                               ...)
{
    va_list var_args;
    gboolean retval;

    va_start (var_args, layout);
    retval = eek_xkb_layout_set_names_full_valist (layout, var_args);
    va_end (var_args);

    return retval;
}

/**
 * eek_xkb_layout_set_names_full_valist:
 * @layout: an #EekXkbLayout
 * @var_args: <type>va_list</type> of pairs of component name and value.
 *
 * See eek_xkb_layout_set_names_full(), this version takes a
 * <type>va_list</type> for language bindings to use.
 *
 * Since: 0.0.5
 */
gboolean
eek_xkb_layout_set_names_full_valist (EekXkbLayout *layout,
                                      va_list       var_args)
{
    XkbComponentNamesRec names;
    gchar *name, *value;

    memset (&names, 0, sizeof names);
    name = va_arg (var_args, gchar *);
    while (name) {
        value = va_arg (var_args, gchar *);
        if (g_strcmp0 (name, "keymap") == 0)
            names.keymap = (char *)value;
        else if (g_strcmp0 (name, "keycodes") == 0)
            names.keycodes = (char *)value;
        else if (g_strcmp0 (name, "types") == 0)
            names.types = (char *)value;
        else if (g_strcmp0 (name, "compat") == 0)
            names.compat = (char *)value;
        else if (g_strcmp0 (name, "symbols") == 0)
            names.symbols = (char *)value;
        else if (g_strcmp0 (name, "geometry") == 0)
            names.geometry = (char *)value;
        name = va_arg (var_args, gchar *);
    }
    return eek_xkb_layout_set_names (layout, &names);
}

/**
 * eek_xkb_layout_set_keycodes:
 * @layout: an #EekXkbLayout
 * @keycodes: component name for keycodes
 *
 * Set the keycodes component (in the XKB terminology).
 * Returns: %TRUE if the component name is successfully set, %FALSE otherwise
 */
gboolean
eek_xkb_layout_set_keycodes (EekXkbLayout *layout, const gchar *keycodes)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);
    XkbComponentNamesRec names;

    g_return_val_if_fail (priv, FALSE);
    memcpy (&names, &priv->names, sizeof names);
    names.keycodes = (gchar *)keycodes;
    return eek_xkb_layout_set_names (layout, &names);
}

/**
 * eek_xkb_layout_set_geometry:
 * @layout: an #EekXkbLayout
 * @geometry: component name for geometry
 *
 * Returns: %TRUE if the component name is successfully set, %FALSE otherwise
 */
gboolean
eek_xkb_layout_set_geometry (EekXkbLayout *layout, const gchar *geometry)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);
    XkbComponentNamesRec names;

    g_return_val_if_fail (priv, FALSE);
    memcpy (&names, &priv->names, sizeof names);
    names.geometry = (gchar *)geometry;
    return eek_xkb_layout_set_names (layout, &names);
}

/**
 * eek_xkb_layout_set_symbols:
 * @layout: an #EekXkbLayout
 * @symbols: component name for symbols
 *
 * Set the symbols component (in the XKB terminology).
 * Returns: %TRUE if the component name is successfully set, %FALSE otherwise
 */
gboolean
eek_xkb_layout_set_symbols (EekXkbLayout *layout, const gchar *symbols)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);
    XkbComponentNamesRec names;

    g_return_val_if_fail (priv, FALSE);
    memcpy (&names, &priv->names, sizeof names);
    names.symbols = (gchar *)symbols;
    return eek_xkb_layout_set_names (layout, &names);
}

/**
 * eek_xkb_layout_get_keycodes:
 * @layout: an #EekXkbLayout
 *
 * Get the keycodes component name (in the XKB terminology).
 */
G_CONST_RETURN gchar *
eek_xkb_layout_get_keycodes (EekXkbLayout *layout)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);

    g_return_val_if_fail (priv, NULL);
    return priv->names.keycodes;
}

/**
 * eek_xkb_layout_get_geometry:
 * @layout: an #EekXkbLayout
 *
 * Get the geometry component name (in the XKB terminology).
 */
G_CONST_RETURN gchar *
eek_xkb_layout_get_geometry (EekXkbLayout *layout)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);

    g_return_val_if_fail (priv, NULL);
    return priv->names.geometry;
}

/**
 * eek_xkb_layout_get_symbols:
 * @layout: an #EekXkbLayout
 *
 * Get the symbols component name (in the XKB terminology).
 */
G_CONST_RETURN gchar *
eek_xkb_layout_get_symbols (EekXkbLayout *layout)
{
    EekXkbLayoutPrivate *priv = EEK_XKB_LAYOUT_GET_PRIVATE (layout);

    g_return_val_if_fail (priv, NULL);
    return priv->names.symbols;
}

static void
get_keyboard (EekXkbLayout *layout)
{
    EekXkbLayoutPrivate *priv = layout->priv;

    if (priv->xkb)
        XkbFreeKeyboard (priv->xkb, 0, TRUE);	/* free_all = TRUE */
    priv->xkb = NULL;

    if (priv->names.keycodes &&
        priv->names.geometry &&
        priv->names.symbols) {
        priv->xkb = XkbGetKeyboardByName (priv->display, XkbUseCoreKbd,
                                          &priv->names, 0,
                                          XkbGBN_GeometryMask |
                                          XkbGBN_KeyNamesMask |
                                          XkbGBN_OtherNamesMask |
                                          XkbGBN_ClientSymbolsMask |
                                          XkbGBN_IndicatorMapMask, FALSE);
    } else {
        priv->xkb = XkbGetKeyboard (priv->display,
                                    XkbGBN_GeometryMask |
                                    XkbGBN_KeyNamesMask |
                                    XkbGBN_OtherNamesMask |
                                    XkbGBN_SymbolsMask |
                                    XkbGBN_IndicatorMapMask,
                                    XkbUseCoreKbd);
        get_names (layout);
    }

    if (priv->xkb == NULL) {
        g_free (priv->names.keycodes);
        priv->names.keycodes = NULL;
        g_free (priv->names.geometry);
        priv->names.geometry = NULL;
        g_free (priv->names.symbols);
        priv->names.symbols = NULL;
    }
}


static guint
find_keycode (EekXkbLayout *layout, gchar *key_name)
{
#define KEYSYM_NAME_MAX_LENGTH 4
    guint keycode;
    gint i, j;
    XkbKeyNamePtr pkey;
    XkbKeyAliasPtr palias;
    guint is_name_matched;
    gchar *src, *dst;
    EekXkbLayoutPrivate *priv = layout->priv;

    if (!priv->xkb)
        return EEK_INVALID_KEYCODE;

#ifdef KBDRAW_DEBUG
    printf ("    looking for keycode for (%c%c%c%c)\n",
            key_name[0], key_name[1], key_name[2], key_name[3]);
#endif

    pkey = priv->xkb->names->keys + priv->xkb->min_key_code;
    for (keycode = priv->xkb->min_key_code;
         keycode <= priv->xkb->max_key_code; keycode++) {
        is_name_matched = 1;
        src = key_name;
        dst = pkey->name;
        for (i = KEYSYM_NAME_MAX_LENGTH; --i >= 0;) {
            if ('\0' == *src)
                break;
            if (*src++ != *dst++) {
                is_name_matched = 0;
                break;
            }
        }
        if (is_name_matched) {
#ifdef KBDRAW_DEBUG
            printf ("      found keycode %u\n", keycode);
#endif
            return keycode;
        }
        pkey++;
    }

    palias = priv->xkb->names->key_aliases;
    for (j = priv->xkb->names->num_key_aliases; --j >= 0;) {
        is_name_matched = 1;
        src = key_name;
        dst = palias->alias;
        for (i = KEYSYM_NAME_MAX_LENGTH; --i >= 0;) {
            if ('\0' == *src)
                break;
            if (*src++ != *dst++) {
                is_name_matched = 0;
                break;
            }
        }

        if (is_name_matched) {
            keycode = find_keycode (layout, palias->real);
#ifdef KBDRAW_DEBUG
            printf ("found alias keycode %u\n", keycode);
#endif
            return keycode;
        }
        palias++;
    }

    return EEK_INVALID_KEYCODE;
}

static void
setup_scaling (EekXkbLayout *layout,
               gdouble       width,
               gdouble       height)
{
    EekXkbLayoutPrivate *priv = layout->priv;

    g_return_if_fail (priv->xkb);

    g_return_if_fail (priv->xkb->geom->width_mm > 0);
    g_return_if_fail (priv->xkb->geom->height_mm > 0);

    if (width * priv->xkb->geom->height_mm <
        height * priv->xkb->geom->width_mm) {
        priv->scale_numerator = width;
        priv->scale_denominator = priv->xkb->geom->width_mm;
    } else {
        priv->scale_numerator = height;
        priv->scale_denominator = priv->xkb->geom->height_mm;
    }
}
