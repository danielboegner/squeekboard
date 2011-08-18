/* 
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
 * SECTION:eek-keyboard
 * @short_description: Base class of a keyboard
 * @see_also: #EekSection
 *
 * The #EekKeyboardClass class represents a keyboard, which consists
 * of one or more sections of the #EekSectionClass class.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif  /* HAVE_CONFIG_H */

#include "eek-keyboard.h"
#include "eek-section.h"
#include "eek-key.h"
#include "eek-symbol.h"
#include "eek-serializable.h"
#include "eek-enumtypes.h"

enum {
    PROP_0,
    PROP_LAYOUT,
    PROP_MODIFIER_BEHAVIOR,
    PROP_LAST
};

enum {
    KEY_PRESSED,
    KEY_RELEASED,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0, };

static void eek_serializable_iface_init (EekSerializableIface *iface);

G_DEFINE_TYPE_WITH_CODE (EekKeyboard, eek_keyboard, EEK_TYPE_CONTAINER,
                         G_IMPLEMENT_INTERFACE (EEK_TYPE_SERIALIZABLE,
                                                eek_serializable_iface_init));

#define EEK_KEYBOARD_GET_PRIVATE(obj)                                  \
    (G_TYPE_INSTANCE_GET_PRIVATE ((obj), EEK_TYPE_KEYBOARD, EekKeyboardPrivate))


struct _EekKeyboardPrivate
{
    EekLayout *layout;
    EekModifierBehavior modifier_behavior;
    EekModifierType modifiers;
    GArray *outline_array;

    /* modifiers dynamically assigned at run time */
    EekModifierType num_lock_mask;
    EekModifierType alt_gr_mask;
};

static EekSerializableIface *eek_keyboard_parent_serializable_iface;

static GVariant *_g_variant_new_outline (EekOutline *outline);
static EekOutline *_g_variant_get_outline (GVariant *variant);

static GVariant *
_g_variant_new_outline (EekOutline *outline)
{
    GVariantBuilder builder, array;
    gint i;

    g_variant_builder_init (&builder, G_VARIANT_TYPE ("(div)"));
    g_variant_builder_add (&builder, "d", outline->corner_radius);
    g_variant_builder_add (&builder, "i", outline->num_points);
    g_variant_builder_init (&array, G_VARIANT_TYPE ("a(dd)"));
    for (i = 0; i < outline->num_points; i++)
        g_variant_builder_add (&array,
                               "(dd)",
                               outline->points[i].x,
                               outline->points[i].y);
    g_variant_builder_add (&builder, "v", g_variant_builder_end (&array));
    return g_variant_builder_end (&builder);
}

static EekOutline *
_g_variant_get_outline (GVariant *variant)
{
    EekOutline *outline;
    GVariant *array;
    GVariantIter iter;
    gdouble x, y;
    gint i;

    outline = g_slice_new0 (EekOutline);

    g_variant_get_child (variant, 0, "d", &outline->corner_radius);
    g_variant_get_child (variant, 1, "i", &outline->num_points);

    outline->points = g_slice_alloc0 (sizeof (EekPoint) * outline->num_points);

    g_variant_get_child (variant, 2, "v", &array);
    g_variant_iter_init (&iter, array);
    for (i = 0; i < outline->num_points; i++) {
        if (!g_variant_iter_next (&iter, "(dd)", &x, &y)) {
            eek_outline_free (outline);
            g_return_val_if_reached (NULL);
        }
        outline->points[i].x = x;
        outline->points[i].y = y;
    }

    return outline;
}

static void
eek_keyboard_real_serialize (EekSerializable *self,
                             GVariantBuilder *builder)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(self);
    GVariantBuilder array;
    guint i;

    eek_keyboard_parent_serializable_iface->serialize (self, builder);

    g_variant_builder_init (&array, G_VARIANT_TYPE ("av"));
    for (i = 0; i < priv->outline_array->len; i++) {
        EekOutline *outline =
            eek_keyboard_get_outline (EEK_KEYBOARD(self), i + 1);
        g_variant_builder_add (&array, "v",
                               _g_variant_new_outline (outline));
    }
    g_variant_builder_add (builder, "v", g_variant_builder_end (&array));
    g_variant_builder_add (builder, "u", priv->num_lock_mask);
    g_variant_builder_add (builder, "u", priv->alt_gr_mask);
}

static gsize
eek_keyboard_real_deserialize (EekSerializable *self,
                               GVariant        *variant,
                               gsize            index)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(self);
    GVariant *array, *outline;
    GVariantIter iter;

    index = eek_keyboard_parent_serializable_iface->deserialize (self,
                                                                 variant,
                                                                 index);

    g_variant_get_child (variant, index++, "v", &array);

    g_variant_iter_init (&iter, array);
    while (g_variant_iter_next (&iter, "v", &outline)) {
        EekOutline *_outline = _g_variant_get_outline (outline);
        g_array_append_val (priv->outline_array, *_outline);
        /* don't use eek_outline_free here, so as to keep _outline->points */
        g_slice_free (EekOutline, _outline);
    }
    g_variant_get_child (variant, index++, "u", &priv->num_lock_mask);
    g_variant_get_child (variant, index++, "u", &priv->alt_gr_mask);

    return index;
}

static void
eek_serializable_iface_init (EekSerializableIface *iface)
{
    eek_keyboard_parent_serializable_iface =
        g_type_interface_peek_parent (iface);

    iface->serialize = eek_keyboard_real_serialize;
    iface->deserialize = eek_keyboard_real_deserialize;
}

static void
on_key_pressed (EekSection  *section,
                EekKey      *key,
                EekKeyboard *keyboard)
{
    g_signal_emit_by_name (keyboard, "key-pressed", key);
}

static void
on_key_released (EekSection  *section,
                 EekKey      *key,
                 EekKeyboard *keyboard)
{
    g_signal_emit_by_name (keyboard, "key-released", key);
}

static void
on_symbol_index_changed (EekSection *section,
                         gint group,
                         gint level,
                         EekKeyboard *keyboard)
{
    g_signal_emit_by_name (keyboard, "symbol-index-changed", group, level);
}

static EekSection *
eek_keyboard_real_create_section (EekKeyboard *self)
{
    EekSection *section;

    section = g_object_new (EEK_TYPE_SECTION, NULL);
    g_return_val_if_fail (section, NULL);

    EEK_CONTAINER_GET_CLASS(self)->add_child (EEK_CONTAINER(self),
                                              EEK_ELEMENT(section));
    return section;
}

struct _FindKeyByKeycodeCallbackData {
    EekKey *key;
    guint keycode;
};
typedef struct _FindKeyByKeycodeCallbackData FindKeyByKeycodeCallbackData;

static gint
find_key_by_keycode_section_callback (EekElement *element, gpointer user_data)
{
    FindKeyByKeycodeCallbackData *data = user_data;

    data->key = eek_section_find_key_by_keycode (EEK_SECTION(element),
                                                 data->keycode);
    if (data->key)
        return 0;
    return -1;
}

static EekKey *
eek_keyboard_real_find_key_by_keycode (EekKeyboard *self,
                                       guint        keycode)
{
    FindKeyByKeycodeCallbackData data;

    data.keycode = keycode;
    if (eek_container_find (EEK_CONTAINER(self),
                            find_key_by_keycode_section_callback,
                            &data))
        return data.key;
    return NULL;
}

static void
eek_keyboard_set_property (GObject      *object,
                           guint         prop_id,
                           const GValue *value,
                           GParamSpec   *pspec)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(object);

    switch (prop_id) {
    case PROP_LAYOUT:
        priv->layout = g_value_get_object (value);
        if (priv->layout)
            g_object_ref (priv->layout);
        break;
    case PROP_MODIFIER_BEHAVIOR:
        eek_keyboard_set_modifier_behavior (EEK_KEYBOARD(object),
                                            g_value_get_enum (value));
        break;
    default:
        g_object_set_property (object,
                               g_param_spec_get_name (pspec),
                               value);
        break;
    }
}

static void
eek_keyboard_get_property (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(object);

    switch (prop_id) {
    case PROP_LAYOUT:
        g_value_set_object (value, priv->layout);
        break;
    case PROP_MODIFIER_BEHAVIOR:
        g_value_set_enum (value,
                          eek_keyboard_get_modifier_behavior (EEK_KEYBOARD(object)));
        break;
    default:
        g_object_get_property (object,
                               g_param_spec_get_name (pspec),
                               value);
        break;
    }
}

static void
set_level_from_modifiers (EekKeyboard *self)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(self);
    gint level = 0;

    if (priv->modifiers & priv->alt_gr_mask)
        level |= 2;
    if (priv->modifiers & EEK_SHIFT_MASK)
        level |= 1;
    eek_element_set_level (EEK_ELEMENT(self), level);
}

static void
eek_keyboard_real_key_pressed (EekKeyboard *self,
                               EekKey      *key)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(self);
    EekSymbol *symbol;
    EekModifierType modifier;

    symbol = eek_key_get_symbol_with_fallback (key, 0, 0);
    if (!symbol)
        return;

    modifier = eek_symbol_get_modifier_mask (symbol);
    if (priv->modifier_behavior == EEK_MODIFIER_BEHAVIOR_NONE) {
        priv->modifiers |= modifier;
        set_level_from_modifiers (self);
    }
}

static void
eek_keyboard_real_key_released (EekKeyboard *self,
                                EekKey      *key)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(self);
    EekSymbol *symbol;
    EekModifierType modifier;

    symbol = eek_key_get_symbol_with_fallback (key, 0, 0);
    if (!symbol)
        return;

    modifier = eek_symbol_get_modifier_mask (symbol);
    switch (priv->modifier_behavior) {
    case EEK_MODIFIER_BEHAVIOR_NONE:
        priv->modifiers &= ~modifier;
        break;
    case EEK_MODIFIER_BEHAVIOR_LOCK:
        priv->modifiers ^= modifier;
        break;
    case EEK_MODIFIER_BEHAVIOR_LATCH:
        if (modifier == priv->alt_gr_mask || modifier == EEK_SHIFT_MASK)
            priv->modifiers ^= modifier;
        else
            priv->modifiers = (priv->modifiers ^ modifier) & modifier;
        break;
    }
    set_level_from_modifiers (self);
}

static void
eek_keyboard_dispose (GObject *object)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(object);

    if (priv->layout) {
        g_object_unref (priv->layout);
        priv->layout = NULL;
    }

    G_OBJECT_CLASS (eek_keyboard_parent_class)->dispose (object);
}

static void
eek_keyboard_finalize (GObject *object)
{
    EekKeyboardPrivate *priv = EEK_KEYBOARD_GET_PRIVATE(object);
    gint i;

    for (i = 0; i < priv->outline_array->len; i++) {
        EekOutline *outline = &g_array_index (priv->outline_array,
                                              EekOutline,
                                              i);
        g_slice_free1 (sizeof (EekPoint) * outline->num_points,
                       outline->points);
    }
    g_array_free (priv->outline_array, TRUE);
        
    G_OBJECT_CLASS (eek_keyboard_parent_class)->finalize (object);
}

static void
eek_keyboard_real_child_added (EekContainer *self,
                               EekElement   *element)
{
    g_signal_connect (element, "key-pressed",
                      G_CALLBACK(on_key_pressed), self);
    g_signal_connect (element, "key-released",
                      G_CALLBACK(on_key_released), self);
    g_signal_connect (element, "symbol-index-changed",
                      G_CALLBACK(on_symbol_index_changed), self);
}

static void
eek_keyboard_real_child_removed (EekContainer *self,
                                 EekElement   *element)
{
    g_signal_handlers_disconnect_by_func (element, on_key_pressed, self);
    g_signal_handlers_disconnect_by_func (element, on_key_released, self);
}

static void
eek_keyboard_class_init (EekKeyboardClass *klass)
{
    EekContainerClass *container_class = EEK_CONTAINER_CLASS (klass);
    GObjectClass      *gobject_class = G_OBJECT_CLASS (klass);
    GParamSpec        *pspec;

    g_type_class_add_private (gobject_class,
                              sizeof (EekKeyboardPrivate));

    klass->create_section = eek_keyboard_real_create_section;
    klass->find_key_by_keycode = eek_keyboard_real_find_key_by_keycode;

    /* signals */
    klass->key_pressed = eek_keyboard_real_key_pressed;
    klass->key_released = eek_keyboard_real_key_released;

    container_class->child_added = eek_keyboard_real_child_added;
    container_class->child_removed = eek_keyboard_real_child_removed;

    gobject_class->get_property = eek_keyboard_get_property;
    gobject_class->set_property = eek_keyboard_set_property;
    gobject_class->dispose = eek_keyboard_dispose;
    gobject_class->finalize = eek_keyboard_finalize;

    /**
     * EekKeyboard:layout:
     *
     * The layout used to create this #EekKeyboard.
     */
    pspec = g_param_spec_object ("layout",
                                 "Layout",
                                 "Layout used to create the keyboard",
                                 EEK_TYPE_LAYOUT,
                                 G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_LAYOUT,
                                     pspec);

    /**
     * EekKeyboard:modifier-behavior:
     *
     * The modifier handling mode of #EekKeyboard.
     */
    pspec = g_param_spec_enum ("modifier-behavior",
                               "Modifier behavior",
                               "Modifier handling mode of the keyboard",
                               EEK_TYPE_MODIFIER_BEHAVIOR,
                               EEK_MODIFIER_BEHAVIOR_NONE,
                               G_PARAM_READWRITE);
    g_object_class_install_property (gobject_class,
                                     PROP_MODIFIER_BEHAVIOR,
                                     pspec);

    /**
     * EekKeyboard::key-pressed:
     * @keyboard: an #EekKeyboard
     * @key: an #EekKey
     *
     * The ::key-pressed signal is emitted each time a key in @keyboard
     * is shifted to the pressed state.
     */
    signals[KEY_PRESSED] =
        g_signal_new (I_("key-pressed"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekKeyboardClass, key_pressed),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      EEK_TYPE_KEY);

    /**
     * EekKeyboard::key-released:
     * @keyboard: an #EekKeyboard
     * @key: an #EekKey
     *
     * The ::key-released signal is emitted each time a key in @keyboard
     * is shifted to the released state.
     */
    signals[KEY_RELEASED] =
        g_signal_new (I_("key-released"),
                      G_TYPE_FROM_CLASS(gobject_class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET(EekKeyboardClass, key_released),
                      NULL,
                      NULL,
                      g_cclosure_marshal_VOID__OBJECT,
                      G_TYPE_NONE,
                      1,
                      EEK_TYPE_KEY);
}

static void
eek_keyboard_init (EekKeyboard *self)
{
    EekKeyboardPrivate *priv;

    priv = self->priv = EEK_KEYBOARD_GET_PRIVATE(self);
    priv->modifier_behavior = EEK_MODIFIER_BEHAVIOR_NONE;
    priv->outline_array = g_array_new (FALSE, TRUE, sizeof (EekOutline));
    eek_element_set_symbol_index (EEK_ELEMENT(self), 0, 0);
}

/**
 * eek_keyboard_set_symbol_index:
 * @keyboard: an #EekKeyboard
 * @group: row index of the symbol matrix of keys on @keyboard
 * @level: column index of the symbol matrix of keys on @keyboard
 *
 * Set the default index of the symbol matrices of keys in @keyboard.
 * To unset, pass -1 as group/level.
 *
 * Deprecated: 1.0: Use eek_element_set_symbol_index()
 */
void
eek_keyboard_set_symbol_index (EekKeyboard *keyboard,
                               gint         group,
                               gint         level)
{
    g_return_if_fail (EEK_IS_KEYBOARD(keyboard));
    eek_element_set_symbol_index (EEK_ELEMENT(keyboard), group, level);
}

/**
 * eek_keyboard_get_symbol_index:
 * @keyboard: an #EekKeyboard
 * @group: a pointer where the group value of the symbol index will be stored
 * @level: a pointer where the level value of the symbol index will be stored
 *
 * Get the default index of the symbol matrices of keys in @keyboard.
 * If the index is not set, -1 will be returned.
 *
 * Deprecated: 1.0: Use eek_element_get_symbol_index()
 */
void
eek_keyboard_get_symbol_index (EekKeyboard *keyboard,
                               gint        *group,
                               gint        *level)
{
    g_return_if_fail (EEK_IS_KEYBOARD(keyboard));
    eek_element_get_symbol_index(EEK_ELEMENT(keyboard), group, level);
}

/**
 * eek_keyboard_set_group:
 * @keyboard: an #EekKeyboard
 * @group: group index of @keyboard
 *
 * Set the group value of the default symbol index of @keyboard.  To
 * unset, pass -1 as @group.
 *
 * See also: eek_keyboard_set_symbol_index()
 * Deprecated: 1.0: Use eek_element_set_group()
 */
void
eek_keyboard_set_group (EekKeyboard *keyboard,
                        gint         group)
{
    eek_element_set_group (EEK_ELEMENT(keyboard), group);
}

/**
 * eek_keyboard_set_level:
 * @keyboard: an #EekKeyboard
 * @level: level index of @keyboard
 *
 * Set the level value of the default symbol index of @keyboard.  To
 * unset, pass -1 as @level.
 *
 * See also: eek_keyboard_set_symbol_index()
 * Deprecated: 1.0: Use eek_element_set_level()
 */
void
eek_keyboard_set_level (EekKeyboard *keyboard,
                        gint         level)
{
    eek_element_set_level (EEK_ELEMENT(keyboard), level);
}

/**
 * eek_keyboard_get_group:
 * @keyboard: an #EekKeyboard
 *
 * Return the group value of the default symbol index of @keyboard.
 * If the value is not set, -1 will be returned.
 *
 * See also: eek_keyboard_get_symbol_index()
 * Deprecated: 1.0: Use eek_element_get_group()
 */
gint
eek_keyboard_get_group (EekKeyboard *keyboard)
{
    return eek_element_get_group (EEK_ELEMENT(keyboard));
}

/**
 * eek_keyboard_get_level:
 * @keyboard: an #EekKeyboard
 *
 * Return the level value of the default symbol index of @keyboard.
 * If the value is not set, -1 will be returned.
 *
 * See also: eek_keyboard_get_symbol_index()
 * Deprecated: 1.0: Use eek_element_get_level()
 */
gint
eek_keyboard_get_level (EekKeyboard *keyboard)
{
    return eek_element_get_level (EEK_ELEMENT(keyboard));
}

/**
 * eek_keyboard_create_section:
 * @keyboard: an #EekKeyboard
 *
 * Create an #EekSection instance and append it to @keyboard.  This
 * function is rarely called by application but called by #EekLayout
 * implementation.
 */
EekSection *
eek_keyboard_create_section (EekKeyboard *keyboard)
{
    EekSection *section;
    g_return_val_if_fail (EEK_IS_KEYBOARD(keyboard), NULL);
    section = EEK_KEYBOARD_GET_CLASS(keyboard)->create_section (keyboard);
    return section;
}

/**
 * eek_keyboard_find_key_by_keycode:
 * @keyboard: an #EekKeyboard
 * @keycode: a keycode
 *
 * Find an #EekKey whose keycode is @keycode.
 * Return value: (transfer none): #EekKey whose keycode is @keycode
 */
EekKey *
eek_keyboard_find_key_by_keycode (EekKeyboard *keyboard,
                                  guint        keycode)
{
    g_assert (EEK_IS_KEYBOARD(keyboard));
    return EEK_KEYBOARD_GET_CLASS(keyboard)->
        find_key_by_keycode (keyboard, keycode);
}

/**
 * eek_keyboard_get_layout:
 * @keyboard: an #EekKeyboard
 *
 * Get the layout used to create @keyboard.
 * Returns: an #EekLayout
 */
EekLayout *
eek_keyboard_get_layout (EekKeyboard *keyboard)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);
    return priv->layout;
}

/**
 * eek_keyboard_get_size:
 * @keyboard: an #EekKeyboard
 * @width: width of @keyboard
 * @height: height of @keyboard
 *
 * Get the size of @keyboard.
 */
void
eek_keyboard_get_size (EekKeyboard *keyboard,
                       gdouble     *width,
                       gdouble     *height)
{
    EekBounds bounds;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    eek_element_get_bounds (EEK_ELEMENT(keyboard), &bounds);
    *width = bounds.width;
    *height = bounds.height;
}

/**
 * eek_keyboard_set_modifier_behavior:
 * @keyboard: an #EekKeyboard
 * @modifier_behavior: modifier behavior of @keyboard
 *
 * Set the modifier handling mode of @keyboard.
 */
void
eek_keyboard_set_modifier_behavior (EekKeyboard        *keyboard,
                                    EekModifierBehavior modifier_behavior)
{
    EekKeyboardPrivate *priv;

    g_return_if_fail (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    priv->modifier_behavior = modifier_behavior;
}

/**
 * eek_keyboard_get_modifier_behavior:
 * @keyboard: an #EekKeyboard
 *
 * Get the modifier handling mode of @keyboard.
 * Returns: #EekModifierBehavior
 */
EekModifierBehavior
eek_keyboard_get_modifier_behavior (EekKeyboard *keyboard)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    return priv->modifier_behavior;
}

void
eek_keyboard_set_modifiers (EekKeyboard    *keyboard,
                            EekModifierType modifiers)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    priv->modifiers = modifiers;
    set_level_from_modifiers (keyboard);
}

/**
 * eek_keyboard_get_modifiers:
 * @keyboard: an #EekKeyboard
 *
 * Get the current modifier status of @keyboard.
 * Returns: #EekModifierType
 */
EekModifierType
eek_keyboard_get_modifiers (EekKeyboard *keyboard)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    return priv->modifiers;
}

/**
 * eek_keyboard_add_outline:
 * @keyboard: an #EekKeyboard
 * @outline: an #EekOutline
 *
 * Register an outline of @keyboard.
 * Returns: an unsigned long id of the registered outline, for later reference
 */
gulong
eek_keyboard_add_outline (EekKeyboard *keyboard,
                          EekOutline  *outline)
{
    EekKeyboardPrivate *priv;
    EekOutline *_outline;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    _outline = eek_outline_copy (outline);
    g_array_append_val (priv->outline_array, *_outline);
    /* don't use eek_outline_free here, so as to keep _outline->points */
    g_slice_free (EekOutline, _outline);
    return priv->outline_array->len;
}

/**
 * eek_keyboard_get_outline:
 * @keyboard: an #EekKeyboard
 * @oref: an unsigned long id
 *
 * Get an outline associated with @oref in @keyboard.
 * Returns: an #EekOutline, which should not be released
 */
EekOutline *
eek_keyboard_get_outline (EekKeyboard *keyboard,
                          gulong oref)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    if (oref > priv->outline_array->len)
        return NULL;

    return &g_array_index (priv->outline_array, EekOutline, oref - 1);
}

/**
 * eek_keyboard_set_num_lock_mask:
 * @keyboard: an #EekKeyboard
 * @num_lock_mask: an #EekModifierType
 *
 * Set modifier mask used as Num_Lock.
 */
void
eek_keyboard_set_num_lock_mask (EekKeyboard    *keyboard,
                                EekModifierType num_lock_mask)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    priv->num_lock_mask = num_lock_mask;
}

/**
 * eek_keyboard_get_num_lock_mask:
 * @keyboard: an #EekKeyboard
 *
 * Get modifier mask used as Num_Lock.
 * Returns: an #EekModifierType
 */
EekModifierType
eek_keyboard_get_num_lock_mask (EekKeyboard *keyboard)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    return priv->num_lock_mask;
}

/**
 * eek_keyboard_set_alt_gr_mask:
 * @keyboard: an #EekKeyboard
 * @alt_gr_mask: an #EekModifierType
 *
 * Set modifier mask used as Alt_Gr.
 */
void
eek_keyboard_set_alt_gr_mask (EekKeyboard    *keyboard,
                              EekModifierType alt_gr_mask)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    priv->alt_gr_mask = alt_gr_mask;
}

/**
 * eek_keyboard_get_alt_gr_mask:
 * @keyboard: an #EekKeyboard
 *
 * Get modifier mask used as Alt_Gr.
 * Returns: an #EekModifierType
 */
EekModifierType
eek_keyboard_get_alt_gr_mask (EekKeyboard *keyboard)
{
    EekKeyboardPrivate *priv;

    g_assert (EEK_IS_KEYBOARD(keyboard));
    priv = EEK_KEYBOARD_GET_PRIVATE(keyboard);

    return priv->alt_gr_mask;
}
