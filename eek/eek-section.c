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
 * SECTION:eek-section
 * @short_description: Base class of a section
 * @see_also: #EekKey
 *
 * The #EekSectionClass class represents a section, which consists
 * of one or more keys of the #EekKeyClass class.
 */

#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */
#include "eek-section.h"
#include "eek-key.h"

enum {
    PROP_0,
    PROP_ANGLE,
    PROP_LAST
};

enum {
    KEY_PRESSED,
    KEY_RELEASED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

G_DEFINE_TYPE (EekSection, eek_section, EEK_TYPE_CONTAINER);

#define EEK_SECTION_GET_PRIVATE(obj)                           \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEK_TYPE_SECTION, EekSectionPrivate))

struct _EekRow
{
    gint num_columns;
    EekOrientation orientation;
};

typedef struct _EekRow EekRow;

struct _EekSectionPrivate
{
    gint angle;
    GSList *rows;
    GSList *keys;
};

static void
eek_section_real_set_angle (EekSection *self,
                                   gint        angle)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(self);

    priv->angle = angle;

    g_object_notify (G_OBJECT(self), "angle");
}

static gint
eek_section_real_get_angle (EekSection *self)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(self);

    return priv->angle;
}

static gint
eek_section_real_get_n_rows (EekSection *self)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(self);

    return g_slist_length (priv->rows);
}

static void
eek_section_real_add_row (EekSection    *self,
                          gint           num_columns,
                          EekOrientation orientation)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(self);
    EekRow *row;

    row = g_slice_new (EekRow);
    row->num_columns = num_columns;
    row->orientation = orientation;
    priv->rows = g_slist_append (priv->rows, row);
}

static void
eek_section_real_get_row (EekSection     *self,
                          gint            index,
                          gint           *num_columns,
                          EekOrientation *orientation)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(self);
    EekRow *row;

    row = g_slist_nth_data (priv->rows, index);
    g_return_if_fail (row);
    if (num_columns)
        *num_columns = row->num_columns;
    if (orientation)
        *orientation = row->orientation;
}

static void
pressed_event (EekKey *key, EekSection *section)
{
    g_signal_emit_by_name (section, "key-pressed", key);
}

static void
released_event (EekKey *key, EekSection *section)
{
    g_signal_emit_by_name (section, "key-released", key);
}

static EekKey *
eek_section_real_create_key (EekSection  *self,
                             gint column,
                             gint row)
{
    EekKey *key;
    gint num_columns, num_rows;
    EekOrientation orientation;

    num_rows = eek_section_get_n_rows (self);
    g_return_val_if_fail (0 <= row && row < num_rows, NULL);
    eek_section_get_row (self, row, &num_columns, &orientation);
    g_return_val_if_fail (column < num_columns, NULL);

    key = g_object_new (EEK_TYPE_KEY,
                        "column", column,
                        "row", row,
                        NULL);
    g_return_val_if_fail (key, NULL);
    g_object_ref_sink (key);

    g_signal_connect (key, "pressed", G_CALLBACK(pressed_event), self);
    g_signal_connect (key, "released", G_CALLBACK(released_event), self);

    EEK_CONTAINER_GET_CLASS(self)->add_child (EEK_CONTAINER(self),
                                              EEK_ELEMENT(key));

    return key;
}

static void
eek_section_finalize (GObject *object)
{
    EekSectionPrivate *priv = EEK_SECTION_GET_PRIVATE(object);
    GSList *head;

    for (head = priv->rows; head; head = g_slist_next (head))
        g_slice_free (EekRow, head->data);
    g_slist_free (priv->rows);

    for (head = priv->keys; head; head = g_slist_next (head))
        g_object_unref (head->data);
    g_slist_free (priv->keys);

    G_OBJECT_CLASS (eek_section_parent_class)->finalize (object);
}

static void
eek_section_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
    switch (prop_id) {
    case PROP_ANGLE:
        eek_section_set_angle (EEK_SECTION(object),
                               g_value_get_int (value));
        break;
    default:
        g_object_set_property (object,
                               g_param_spec_get_name (pspec),
                               value);
        break;
    }
}

static void
eek_section_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
    switch (prop_id) {
    case PROP_ANGLE:
        g_value_set_int (value, eek_section_get_angle (EEK_SECTION(object)));
        break;
    default:
        g_object_get_property (object,
                               g_param_spec_get_name (pspec),
                               value);
        break;
    }
}

static void
eek_section_class_init (EekSectionClass *klass)
{
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    g_type_class_add_private (gobject_class, sizeof (EekSectionPrivate));

    klass->set_angle = eek_section_real_set_angle;
    klass->get_angle = eek_section_real_get_angle;
    klass->get_n_rows = eek_section_real_get_n_rows;
    klass->add_row = eek_section_real_add_row;
    klass->get_row = eek_section_real_get_row;
    klass->create_key = eek_section_real_create_key;

    gobject_class->set_property = eek_section_set_property;
    gobject_class->get_property = eek_section_get_property;
    gobject_class->finalize     = eek_section_finalize;

    /**
     * EekSection:angle:
     *
     * The rotation angle of #EekSection.
     */
    pspec = g_param_spec_int ("angle",
                              "Angle",
                              "Rotation angle of the section",
                              -360, 360, 0,
                              G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_ANGLE,
                                     pspec);

    signals[KEY_PRESSED] =
        g_signal_new ("key-pressed",
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0,
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      EEK_TYPE_KEY);

    signals[KEY_RELEASED] =
        g_signal_new ("key-released",
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_FIRST,
                      0,
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      EEK_TYPE_KEY);
}

static void
eek_section_init (EekSection *self)
{
    EekSectionPrivate *priv;

    priv = self->priv = EEK_SECTION_GET_PRIVATE (self);
    priv->angle = 0;
    priv->rows = NULL;
    priv->keys = NULL;
}

void
eek_section_set_angle (EekSection  *section,
                       gint         angle)
{
    g_return_if_fail (EEK_IS_SECTION(section));
    EEK_SECTION_GET_CLASS(section)->set_angle (section, angle);
}

gint
eek_section_get_angle (EekSection *section)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), -1);
    return EEK_SECTION_GET_CLASS(section)->get_angle (section);
}

gint
eek_section_get_n_rows (EekSection *section)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), -1);
    return EEK_SECTION_GET_CLASS(section)->get_n_rows (section);
}

void
eek_section_add_row (EekSection    *section,
                     gint           num_columns,
                     EekOrientation orientation)
{
    g_return_if_fail (EEK_IS_SECTION(section));
    EEK_SECTION_GET_CLASS(section)->add_row (section,
                                             num_columns,
                                             orientation);
}

void
eek_section_get_row (EekSection     *section,
                     gint            index,
                     gint           *num_columns,
                     EekOrientation *orientation)
{
    g_return_if_fail (EEK_IS_SECTION(section));
    EEK_SECTION_GET_CLASS(section)->get_row (section,
                                             index,
                                             num_columns,
                                             orientation);
}

EekKey *
eek_section_create_key (EekSection  *section,
                        gint         column,
                        gint         row)
{
    g_return_val_if_fail (EEK_IS_SECTION(section), NULL);
    return EEK_SECTION_GET_CLASS(section)->create_key (section, column, row);
}
