/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set expandtab ts=4 shiftwidth=4: */
/* 
 * Copyright 2007-2009 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 *
 * CDDL HEADER START
 * 
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 * 
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 * 
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 * 
 * CDDL HEADER END
 * 
 * File:   nwamui_object.c
 *
 */

#include <glib-object.h>
#include <glib/gi18n.h>

#include "libnwamui.h"

enum {
    PROP_NWAM_STATE = 1,
    PROP_NAME,
    PROP_ENABLED,
    PROP_ACTIVE,
    PROP_ACTIVATION_MODE,
    PROP_CONDITIONS,
    LAST_PROP
};

enum {
	PLACEHOLDER,
	LAST_SIGNAL
};

static guint nwamui_object_signals[LAST_SIGNAL] = {0};

struct _NwamuiObjectPrivate {
    /* Cache */
    const gchar      *name;
    gint              activation_mode;
    /* State caching, store up to NWAM_NCU_TYPE_ANY values to allow for NCU
     * classes to have extra state per NCU class type */
    nwam_state_t      nwam_state;
    nwam_aux_state_t  nwam_aux_state;
};

static GObject* nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties);

static void nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec);

static void nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec);

static void nwamui_object_finalize(NwamuiObject *self);

/* Callbacks */
static void nwamui_object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data);

#define NWAMUI_OBJECT_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NWAMUI_TYPE_OBJECT, NwamuiObjectPrivate))
G_DEFINE_TYPE(NwamuiObject, nwamui_object, G_TYPE_OBJECT)

/* Default vfuns */
static const gchar*
default_nwamui_object_get_name(NwamuiObject *object)
{
    return NULL;
}

static gboolean
default_nwamui_object_can_rename (NwamuiObject *object)
{
    return FALSE;
}

static gboolean
default_nwamui_object_set_name(NwamuiObject *object, const gchar* name)
{
}

static void
default_nwamui_object_set_conditions(NwamuiObject *object, const GList* conditions)
{
}

static GList*
default_nwamui_object_get_conditions(NwamuiObject *object)
{
    return NULL;
}

static gint
default_nwamui_object_get_activation_mode(NwamuiObject *object)
{
    return 0;
}

static void
default_nwamui_object_set_activation_mode(NwamuiObject *object, gint activation_mode)
{
}

static gboolean
default_nwamui_object_get_active(NwamuiObject *object)
{
    return FALSE;
}

static void
default_nwamui_object_set_active(NwamuiObject *object, gboolean active)
{
}

static gboolean
default_nwamui_object_get_enabled(NwamuiObject *object)
{
    return FALSE;
}

static void
default_nwamui_object_set_enabled(NwamuiObject *object, gboolean enabled)
{
}

static gboolean
default_nwamui_object_is_modifiable(NwamuiObject *object)
{
    return TRUE;
}

static gboolean
default_nwamui_object_has_modifications(NwamuiObject *object)
{
    return FALSE;
}

static nwam_state_t
default_nwamui_object_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state, const gchar**aux_state_string)
{
    NwamuiObjectPrivate *prv  = NWAMUI_OBJECT_GET_PRIVATE(object);
    nwam_state_t         rval = NWAM_STATE_UNINITIALIZED;

    if ( aux_state ) {
        *aux_state = prv->nwam_aux_state;
    }

    if ( aux_state_string ) {
        *aux_state_string = (const gchar*)nwam_aux_state_to_string(prv->nwam_aux_state);
    }
    return prv->nwam_state;
}

static void
default_nwamui_object_set_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state)
{
    NwamuiObjectPrivate *prv           = NWAMUI_OBJECT_GET_PRIVATE(object);
    gboolean             state_changed = FALSE;

    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    /* For NCU object state, we will see dup events for link and interface. But
     * we actually only (or should) care interface events to avoid dup events and
     * dup GUI notifications.
     * We will map cache_index = 0 to type_interface. Then We should monitor 
     * cache_index is 0 
     */
    if ( prv->nwam_state != state ||
         prv->nwam_aux_state != aux_state ) {
        state_changed = TRUE;
    }

    prv->nwam_state = state;
    prv->nwam_aux_state = aux_state;
    if ( state_changed ) {
/*         g_debug("Set: %-10s - %s", nwam_state_to_string(state), nwam_aux_state_to_string(aux_state)); */
        g_object_notify(G_OBJECT(object), "nwam_state" );
        /* Active property is likely to have changed too, but first check if there
         * is a get_active function, if so the active property should exist. */
        /* For NCUs, only care cache_index == 0. Compatible to ENV, ENM, etc. */
        g_object_notify(G_OBJECT(object), "active" );
    }
}

static gint
default_nwamui_object_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    /* Default is sort by name, ignoring sort_by. */
    return g_strcmp0(nwamui_object_get_name(object), nwamui_object_get_name(other));
}

static gboolean
default_nwamui_object_commit(NwamuiObject *object)
{
    return FALSE;
}

static void
default_nwamui_object_reload(NwamuiObject *object)
{
}

static gboolean
default_nwamui_object_destroy(NwamuiObject *object)
{
    return FALSE;
}

static void
nwamui_object_class_init(NwamuiObjectClass *klass)
{
	GObjectClass *gobject_class;

	gobject_class = G_OBJECT_CLASS (klass);
	gobject_class->constructor = nwamui_object_constructor;
	gobject_class->finalize = (void (*)(GObject*)) nwamui_object_finalize;
	gobject_class->set_property = nwamui_object_set_property;
	gobject_class->get_property = nwamui_object_get_property;

    klass->get_name = default_nwamui_object_get_name;
    klass->can_rename = default_nwamui_object_can_rename;
    klass->set_name = default_nwamui_object_set_name;
    klass->get_conditions = default_nwamui_object_get_conditions;
    klass->set_conditions = default_nwamui_object_set_conditions;
    klass->get_activation_mode = default_nwamui_object_get_activation_mode;
    klass->set_activation_mode = default_nwamui_object_set_activation_mode;
    klass->get_active = default_nwamui_object_get_active;
    klass->set_active = default_nwamui_object_set_active;
    klass->get_enabled = default_nwamui_object_get_enabled;
    klass->set_enabled = default_nwamui_object_set_enabled;
    klass->get_nwam_state = default_nwamui_object_get_nwam_state;
    klass->set_nwam_state = default_nwamui_object_set_nwam_state;
    klass->sort = default_nwamui_object_sort;
    klass->commit = default_nwamui_object_commit;
    klass->reload = default_nwamui_object_reload;
    klass->destroy = default_nwamui_object_destroy;
    klass->is_modifiable = default_nwamui_object_is_modifiable;
    klass->has_modifications = default_nwamui_object_has_modifications;

	g_type_class_add_private(klass, sizeof (NwamuiObjectPrivate));

    g_object_class_install_property (gobject_class,
      PROP_NAME,
      g_param_spec_string ("name",
        _("Name of the object"),
        _("Name of the object"),
        NULL,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_ENABLED,
      g_param_spec_boolean ("enabled",
        _("enabled"),
        _("enabled"),
        FALSE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_ACTIVE,
      g_param_spec_boolean ("active",
        _("active"),
        _("active"),
        FALSE,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_ACTIVATION_MODE,
      g_param_spec_int ("activation_mode",
        _("activation_mode"),
        _("activation_mode"),
        NWAMUI_COND_ACTIVATION_MODE_MANUAL,
        NWAMUI_COND_ACTIVATION_MODE_LAST,
        NWAMUI_COND_ACTIVATION_MODE_MANUAL,
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_CONDITIONS,
      g_param_spec_pointer ("conditions",
        _("conditions"),
        _("conditions"),
        G_PARAM_READWRITE));

    g_object_class_install_property (gobject_class,
      PROP_NWAM_STATE,
      g_param_spec_uint ("nwam_state",
        _("nwam_state"),
        _("nwam_state"),
        0,
        G_MAXUINT,
        0,
        G_PARAM_READABLE));
}

static void
nwamui_object_init(NwamuiObject *self)
{
    NwamuiObjectPrivate *prv = NWAMUI_OBJECT_GET_PRIVATE(self);

    self->prv = prv;

    prv->name = NWAMUI_OBJECT_GET_CLASS(self)->get_name(self);
    prv->activation_mode = NWAMUI_OBJECT_GET_CLASS(self)->get_activation_mode(self);

#if 0
    g_signal_connect(G_OBJECT(self), "notify", (GCallback)nwamui_object_notify_cb, (gpointer)self);
#endif
}

static void
nwamui_object_set_property(GObject         *object,
    guint            prop_id,
    const GValue    *value,
    GParamSpec      *pspec)
{
	NwamuiObject *self = NWAMUI_OBJECT(object);

	switch (prop_id) {
    case PROP_NAME:
        nwamui_object_set_name(NWAMUI_OBJECT(object), g_value_get_string(value));
        break;
    case PROP_ENABLED:
        nwamui_object_set_enabled(NWAMUI_OBJECT(object), g_value_get_boolean(value));
        break;
    case PROP_ACTIVE:
        nwamui_object_set_active(NWAMUI_OBJECT(object), g_value_get_boolean(value));
        break;
    case PROP_ACTIVATION_MODE:
        nwamui_object_set_activation_mode(NWAMUI_OBJECT(object), g_value_get_int(value));
        break;
    case PROP_CONDITIONS:
        nwamui_object_set_conditions(NWAMUI_OBJECT(object), g_value_get_pointer(value));
        break;
    default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwamui_object_get_property(GObject         *object,
    guint            prop_id,
    GValue          *value,
    GParamSpec      *pspec)
{
    NwamuiObjectPrivate *prv  = NWAMUI_OBJECT_GET_PRIVATE(object);

	switch (prop_id) {
    case PROP_NAME:
        /* Get property name is to dup name, directly call get_name do not dup */
        g_value_set_string(value, nwamui_object_get_name(NWAMUI_OBJECT(object)));
        break;
    case PROP_ENABLED:
        g_value_set_boolean(value, nwamui_object_get_enabled(NWAMUI_OBJECT(object)));
        break;
    case PROP_ACTIVE:
        g_value_set_boolean(value, nwamui_object_get_active(NWAMUI_OBJECT(object)));
        break;
    case PROP_ACTIVATION_MODE:
        g_value_set_int(value, nwamui_object_get_activation_mode(NWAMUI_OBJECT(object)));
        break;
    case PROP_CONDITIONS:
        g_value_set_pointer(value, nwamui_object_get_conditions(NWAMUI_OBJECT(object)));
        break;
    case PROP_NWAM_STATE:
        g_value_set_uint( value, (guint)prv->nwam_state );
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
        break;
    }
}

static GObject*
nwamui_object_constructor(GType type,
  guint n_construct_properties,
  GObjectConstructParam *construct_properties)
{
	NwamuiObject *self;
	GObject *object;
	GError *error = NULL;

	object = G_OBJECT_CLASS(nwamui_object_parent_class)->constructor(type,
	    n_construct_properties,
	    construct_properties);
	self = NWAMUI_OBJECT(object);

	return object;
}

static void
nwamui_object_finalize(NwamuiObject *self)
{
	G_OBJECT_CLASS(nwamui_object_parent_class)->finalize(G_OBJECT (self));
}

/**
 * nwamui_object_can_rename:
 * @nwamui_object: a #NwamuiObject.
 * @returns: TRUE if the name.can be changed.
 *
 **/
extern gboolean
nwamui_object_can_rename (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->can_rename(object);
}

/** 
 * nwamui_object_set_name:
 * @nwamui_object: a #NwamuiObject.
 * @name: Value to set name to.
 * 
 **/ 
extern gboolean
nwamui_object_set_name(NwamuiObject *object, const gchar* name )
{
    NwamuiObjectPrivate *prv = NWAMUI_OBJECT_GET_PRIVATE(object);

    g_return_val_if_fail(NWAMUI_IS_OBJECT(object), FALSE);

    if (prv->name == NULL || g_strcmp0(prv->name, name) != 0) {
        if (NWAMUI_OBJECT_GET_CLASS (object)->set_name(object, name)) {
            /* Must cache the return value of name */
            prv->name = NWAMUI_OBJECT_GET_CLASS (object)->get_name(object);
            g_object_notify(G_OBJECT(object), "name");
            return TRUE;
        }
    }
    return FALSE;
}

/**
 * nwamui_object_set_handle:
 * Generally be invoked in nwam object walker function, input a constant @handle
 * only used by get the name of the nwam object, so we can get the real handle
 * by the name. We shouldn't remain the @handle.
 *
 * A nwam_*_handle_t is actually a set of nvpairs we need to get our
 * own handle (i.e. snapshot) since the handle we are passed may be freed by
 * the owner of it (e.g. walkprop does this).
 */
extern void
nwamui_object_set_handle(NwamuiObject *object, const gpointer handle)
{
    g_return_if_fail (NWAMUI_IS_OBJECT(object));

    NWAMUI_OBJECT_GET_CLASS(object)->set_handle(object, handle);
}

/**
 * nwamui_object_get_name:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the name.
 *
 **/
extern const gchar *
nwamui_object_get_name (NwamuiObject *object)
{
    NwamuiObjectPrivate *prv = NWAMUI_OBJECT_GET_PRIVATE(object);

    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);

    prv->name = NWAMUI_OBJECT_GET_CLASS (object)->get_name(object);

    return prv->name;
}

/** 
 * nwamui_object_set_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @conditions: Value to set conditions to.
 * 
 **/ 
extern void
nwamui_object_set_conditions (   NwamuiObject *object,
                             const GList* conditions )
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    NWAMUI_OBJECT_GET_CLASS (object)->set_conditions(object, conditions);
}

/**
 * nwamui_object_get_conditions:
 * @nwamui_object: a #NwamuiObject.
 * @returns: the conditions.
 *
 **/
extern GList*
nwamui_object_get_conditions (NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), NULL);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_conditions(object);
}

extern gint
nwamui_object_get_activation_mode(NwamuiObject *object)
{
    NwamuiObjectPrivate *prv = NWAMUI_OBJECT_GET_PRIVATE(object);

    g_return_val_if_fail (NWAMUI_IS_OBJECT(object), NWAMUI_COND_ACTIVATION_MODE_LAST);

    prv->activation_mode = NWAMUI_OBJECT_GET_CLASS(object)->get_activation_mode(object);

    return prv->activation_mode;
}

extern void
nwamui_object_set_activation_mode(NwamuiObject *object, gint activation_mode)
{
    NwamuiObjectPrivate *prv = NWAMUI_OBJECT_GET_PRIVATE(object);

    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    if (prv->activation_mode != activation_mode) {
        g_object_freeze_notify(G_OBJECT(object));

        NWAMUI_OBJECT_GET_CLASS(object)->set_activation_mode(object, activation_mode);
        /* Cache it */
        prv->activation_mode = activation_mode;

        g_object_notify(G_OBJECT(object), "activation-mode");

        g_object_thaw_notify(G_OBJECT(object));
    }
}

extern gboolean
nwamui_object_get_active(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_active(object);
}

extern void
nwamui_object_set_active(NwamuiObject *object, gboolean active)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    NWAMUI_OBJECT_GET_CLASS (object)->set_active(object, active);
}

extern gboolean
nwamui_object_get_enabled(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->get_enabled(object);
}

extern void
nwamui_object_set_enabled(NwamuiObject *object, gboolean enabled)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    NWAMUI_OBJECT_GET_CLASS (object)->set_enabled(object, enabled);
}

extern gint
nwamui_object_sort(NwamuiObject *object, NwamuiObject *other, guint sort_by)
{
    g_return_val_if_fail(NWAMUI_IS_OBJECT(object), 0);
    g_return_val_if_fail(NWAMUI_IS_OBJECT(other), 0);

    return NWAMUI_OBJECT_GET_CLASS(object)->sort(object, other, sort_by);
}

extern gint
nwamui_object_sort_by_name(NwamuiObject *object, NwamuiObject *other)
{
    g_return_val_if_fail(NWAMUI_IS_OBJECT(object), 0);
    g_return_val_if_fail(NWAMUI_IS_OBJECT(other), 0);

    return NWAMUI_OBJECT_GET_CLASS(object)->sort(object, other, NWAMUI_OBJECT_SORT_BY_NAME);
}

extern gboolean
nwamui_object_commit(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->commit(object);
}

extern gboolean
nwamui_object_destroy(NwamuiObject *object)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS (object)->destroy(object);
}

extern void
nwamui_object_reload(NwamuiObject *object)
{
    g_return_if_fail (NWAMUI_IS_OBJECT (object));

    NWAMUI_OBJECT_GET_CLASS (object)->reload(object);
}

static time_t   _nwamui_state_timeout = 60; /* Seconds */

extern nwam_state_t         
nwamui_object_get_nwam_state(NwamuiObject *object, nwam_aux_state_t* aux_state_p, const gchar**aux_state_string_p)
{
    NwamuiObjectPrivate *prv         = NWAMUI_OBJECT_GET_PRIVATE(object);
    nwam_state_t         rval        = NWAM_STATE_UNINITIALIZED;
    nwam_state_t         _state      = NWAM_STATE_UNINITIALIZED;
    nwam_aux_state_t     _aux_state  = NWAM_AUX_STATE_UNINITIALIZED;
    time_t               _elapsed_time;
    time_t               _current_time;

    g_return_val_if_fail (NWAMUI_IS_OBJECT (object), rval );

    /* Initializing nwam state, important for initially show panel icon. */
    if (prv->nwam_state == NWAM_STATE_UNINITIALIZED && prv->nwam_aux_state == NWAM_AUX_STATE_UNINITIALIZED) {
        prv->nwam_state = NWAMUI_OBJECT_GET_CLASS (object)->get_nwam_state(object, &prv->nwam_aux_state, NULL);
    }
    _state = prv->nwam_state;
    _aux_state = prv->nwam_aux_state;

    rval = _state;

    if ( aux_state_p ) {
        *aux_state_p = _aux_state;
    }
    if ( aux_state_string_p ) {
        *aux_state_string_p = (const gchar*)nwam_aux_state_to_string( _aux_state );
    }

/*     g_debug("Get: %-10s - %s", nwam_state_to_string(_state), nwam_aux_state_to_string(_aux_state)); */
    return(rval);
}

/**
 * nwamui_object_set_nwam_state:
 * Child object must call parent implement to cache state and aux_state correctly.
 */
extern void
nwamui_object_set_nwam_state(NwamuiObject *object, nwam_state_t state, nwam_aux_state_t aux_state)
{
    NwamuiObjectPrivate *prv         = NWAMUI_OBJECT_GET_PRIVATE(object);

    g_return_if_fail(NWAMUI_IS_OBJECT(object));

    g_object_freeze_notify(G_OBJECT(object));

    NWAMUI_OBJECT_GET_CLASS(object)->set_nwam_state(object, state, aux_state);
    /* Always cache the value even if this function is overridden. Since the
     * default function need to detect state change. So update it after that.
     */
    prv->nwam_state = state;
    prv->nwam_aux_state = aux_state;

    g_object_thaw_notify(G_OBJECT(object));
}

extern gboolean
nwamui_object_is_modifiable(NwamuiObject *object)
{
    g_return_val_if_fail(NWAMUI_IS_OBJECT(object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS(object)->is_modifiable(object);
}

/**
 * nwamui_object_has_modifications:   test if there are un-saved changes
 * @returns: TRUE if unsaved changes exist.
 **/
extern gboolean
nwamui_object_has_modifications(NwamuiObject *object)
{
    g_return_val_if_fail(NWAMUI_IS_OBJECT(object), FALSE);

    return NWAMUI_OBJECT_GET_CLASS(object)->has_modifications(object);
}

extern NwamuiObject*
nwamui_object_clone(NwamuiObject *object, const gchar *name, NwamuiObject *parent)
{
    g_return_val_if_fail (NWAMUI_IS_OBJECT(object), NULL);

    return NWAMUI_OBJECT_GET_CLASS (object)->clone(object, name, parent);
}

/* Callbacks */
static void
nwamui_object_notify_cb( GObject *gobject, GParamSpec *arg1, gpointer data)
{
    NwamuiObject* self = NWAMUI_OBJECT(data);

    g_debug("%s '%s' 0x%p : notify '%s'", g_type_name(G_TYPE_FROM_INSTANCE(gobject)), nwamui_object_get_name(self), gobject, arg1->name);
}

