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
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include <glib/gi18n.h>

#include "nwam-menuitem.h"

static void nwam_obj_proxy_init(NwamObjProxyInterface *iface);
static GObject* nwam_menu_item_get_proxy(NwamObjProxyIFace *iface);
static void nwam_menu_item_refresh(NwamObjProxyIFace *iface);

static void nwam_menu_item_set_property (GObject         *object,
  guint            prop_id,
  const GValue    *value,
  GParamSpec      *pspec);
static void nwam_menu_item_get_property (GObject         *object,
  guint            prop_id,
  GValue          *value,
  GParamSpec      *pspec);
static void nwam_menu_item_size_request (GtkWidget        *widget, 
  GtkRequisition   *requisition); 
static void nwam_menu_item_size_allocate (GtkWidget        *widget, 
  GtkAllocation    *allocation);
static void nwam_menu_item_toggle_size_request (GtkMenuItem *menu_item,
  gint        *requisition);
static void nwam_menu_item_activate (GtkMenuItem *menu_item);
static void nwam_menu_item_forall (GtkContainer   *container,
  gboolean    include_internals,
  GtkCallback     callback,
  gpointer        callback_data);
static void nwam_menu_item_remove (GtkContainer   *container,
  GtkWidget *child);
static void nwam_menu_item_draw_indicator(GtkCheckMenuItem *check_menu_item,
  GdkRectangle *area);
static void nwam_menu_item_finalize (NwamMenuItem *self);

static void default_connect_object(NwamMenuItem *self, GObject *object);
static void default_disconnect_object(NwamMenuItem *self, GObject *object);
static void default_sync_object(NwamMenuItem *self, GObject *object);
static gint default_compare(NwamMenuItem *self, NwamMenuItem *other);

enum {
	PROP_ZERO,
	PROP_OBJECT,
	PROP_LWIDGET,
	PROP_RWIDGET,
};

typedef struct _NwamMenuItemPrivate NwamMenuItemPrivate;
#define NWAM_MENU_ITEM_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), NWAM_TYPE_MENU_ITEM, NwamMenuItemPrivate))

struct _NwamMenuItemPrivate {
	GtkWidget *w[MAX_WIDGET_NUM];
    GObject *object;
};

G_DEFINE_TYPE_EXTENDED(NwamMenuItem, nwam_menu_item, GTK_TYPE_CHECK_MENU_ITEM,
  0,
  G_IMPLEMENT_INTERFACE(NWAM_TYPE_OBJ_PROXY_IFACE, nwam_obj_proxy_init))

static void
nwam_obj_proxy_init(NwamObjProxyInterface *iface)
{
    iface->get_proxy = nwam_menu_item_get_proxy;
    iface->refresh = nwam_menu_item_refresh;
    iface->delete_notify = NULL;
}

static void
nwam_menu_item_class_init (NwamMenuItemClass *klass)
{
	GObjectClass *gobject_class;
	GtkWidgetClass *widget_class;
    GtkMenuItemClass *menu_item_class; 
	GtkCheckMenuItemClass *check_menu_item_class;
	GtkContainerClass *container_class;

	gobject_class = G_OBJECT_CLASS (klass);
	widget_class = GTK_WIDGET_CLASS (klass);
    menu_item_class = GTK_MENU_ITEM_CLASS (klass); 
	check_menu_item_class = GTK_CHECK_MENU_ITEM_CLASS (klass);
	container_class = GTK_CONTAINER_CLASS (klass);

	gobject_class->set_property = nwam_menu_item_set_property;
	gobject_class->get_property = nwam_menu_item_get_property;
	gobject_class->finalize = (void (*)(GObject*)) nwam_menu_item_finalize;

	container_class->forall = nwam_menu_item_forall;
    container_class->remove = nwam_menu_item_remove;

    widget_class->size_allocate = nwam_menu_item_size_allocate;
	widget_class->size_request = nwam_menu_item_size_request;

    menu_item_class->toggle_size_request = nwam_menu_item_toggle_size_request;
    /* menu_item_class->activate = nwam_menu_item_activate; */

	check_menu_item_class->draw_indicator = nwam_menu_item_draw_indicator;

    klass->connect_object = default_connect_object;
    klass->disconnect_object = default_disconnect_object;
    klass->sync_object = default_sync_object;
    klass->compare = default_compare;

	g_type_class_add_private (klass, sizeof (NwamMenuItemPrivate));

	g_object_class_install_property (gobject_class,
      PROP_OBJECT,
      g_param_spec_object ("proxy-object",
        N_("proxy-object"),
        N_("proxy-object"),
        G_TYPE_OBJECT,
        G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

	g_object_class_install_property (gobject_class,
      PROP_LWIDGET,
      g_param_spec_object ("left-widget",
        N_("Left widget, ref'd"),
        N_("Left widget, ref'd"),
        GTK_TYPE_WIDGET,
        G_PARAM_READWRITE));

	g_object_class_install_property (gobject_class,
      PROP_RWIDGET,
      g_param_spec_object ("right-widget",
        N_("Right widget, ref'd"),
        N_("Right widget, ref'd"),
        GTK_TYPE_WIDGET,
        G_PARAM_READWRITE));

}

static void
nwam_menu_item_init (NwamMenuItem *self)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    GSList *group = NULL;

    /*
     * TODO - DISABLED because it was causing CRITICAL warnings...
     *
    group = g_slist_prepend (NULL, self);
    gtk_radio_menu_item_set_group (self, group);
    */
}

static void
nwam_menu_item_set_property (GObject         *object,
			     guint            prop_id,
			     const GValue    *value,
			     GParamSpec      *pspec)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (object);
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);

	switch (prop_id) {
	case PROP_OBJECT: {
        GObject *object = g_value_get_object(value);

        if (prv->object != (gpointer)object) {
            if (prv->object) {
                /* Handle disconnect object proxy */
                NWAM_MENU_ITEM_GET_CLASS(self)->disconnect_object(self, prv->object);
                g_object_unref(prv->object);
            }
            if ((prv->object = object) != NULL) {
                g_object_ref(prv->object);

                /* Handle connect object proxy */
                NWAM_MENU_ITEM_GET_CLASS(self)->connect_object(self, prv->object);
                /* Initializing */
                NWAM_MENU_ITEM_GET_CLASS(self)->sync_object(self, prv->object);
            } else {
                /* Todo reset all. */
                default_sync_object(self, NULL);
            }
        }
    }
        break;
	case PROP_LWIDGET:
        nwam_menu_item_set_widget (self, 0,
          GTK_WIDGET (g_value_get_object (value)));
        break;
	case PROP_RWIDGET:
        nwam_menu_item_set_widget (self, 1,
          GTK_WIDGET (g_value_get_object (value)));
        break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_item_get_property (GObject         *object,
                                  guint            prop_id,
                                  GValue          *value,
                                  GParamSpec      *pspec)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (object);
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);

	switch (prop_id)
	{
	case PROP_OBJECT:
		g_value_set_object (value, (GObject*) prv->object);
		break;
	case PROP_LWIDGET:
		g_value_set_object (value, (GObject*) prv->w[0]);
		break;
	case PROP_RWIDGET:
		g_value_set_object (value, (GObject*) prv->w[1]);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
nwam_menu_item_forall (GtkContainer   *container,
                            gboolean    include_internals,
                            GtkCallback     callback,
                            gpointer        callback_data)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (container);
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    int i;

    //g_debug ("foreach container 0x%p", container);

	(*GTK_CONTAINER_CLASS (nwam_menu_item_parent_class)->forall) (container,
									   include_internals,
									   callback,
									   callback_data);
    for (i = 0; i < MAX_WIDGET_NUM; i++) {
        if (prv->w[i]) {
            (*callback) (prv->w[i], callback_data);
        }
    }
}

static void
nwam_menu_item_remove (GtkContainer   *container,
                       GtkWidget *child)
{
	NwamMenuItem *self = NWAM_MENU_ITEM (container);
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    int i;

    for (i = 0; i < MAX_WIDGET_NUM; i++) {
        if (prv->w[i] == child) {
            gtk_widget_unparent (child);
            prv->w[i] = NULL;
            if (GTK_WIDGET_VISIBLE (container) && GTK_WIDGET_VISIBLE (child))
                gtk_widget_queue_resize (GTK_WIDGET (container));
            return;
        }
    }
    if (i == MAX_WIDGET_NUM) {
        (*GTK_CONTAINER_CLASS (nwam_menu_item_parent_class)->remove) (container, child);
    }
}

static void
default_connect_object(NwamMenuItem *self, GObject *object)
{
}

static void
default_disconnect_object(NwamMenuItem *self, GObject *object)
{
}

static void
default_sync_object(NwamMenuItem *self, GObject *object)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    if (object == NULL) {
        g_object_set(self, "label", "(NULL)", NULL);
        nwam_menu_item_set_widget (self, 0, NULL);
        nwam_menu_item_set_widget (self, 1, NULL);
    }
}

static gint
default_compare(NwamMenuItem *self, NwamMenuItem *other)
{
    return 0;
}

static GObject*
nwam_menu_item_get_proxy(NwamObjProxyIFace *iface)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(iface);
    return (GObject *)prv->object;
}

static void
nwam_menu_item_refresh(NwamObjProxyIFace *iface)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(iface);

    NWAM_MENU_ITEM_GET_CLASS(iface)->sync_object(NWAM_MENU_ITEM(iface), prv->object);
}

/**
 * nwam_menu_item_new:
 * @returns: a new #NwamMenuItem.
 *
 * Creates a new #NwamMenuItem with an empty label.
 **/
GtkWidget*
nwam_menu_item_new (void)
{
	return g_object_new (NWAM_TYPE_MENU_ITEM, NULL);
}

/**
 * nwam_menu_item_new_with_object:
 * @returns: a new #NwamMenuItem.
 *
 * Creates a new #NwamMenuItem with an empty label.
 **/
GtkWidget*
nwam_menu_item_new_with_object(GObject *object)
{
	return g_object_new (NWAM_TYPE_MENU_ITEM,
      "proxy-object", object,
      NULL);
}

/**
 * nwam_menu_item_new_with_label:
 * @label: the text of the menu item.
 * @returns: a new #NwamMenuItem.
 *
 * Creates a new #NwamMenuItem containing a label. 
 **/
GtkWidget*
nwam_menu_item_new_with_label (const gchar *label)
{
	NwamMenuItem *nwam_menu_item;
	nwam_menu_item = g_object_new (NWAM_TYPE_MENU_ITEM, NULL);
    menu_item_set_label(GTK_MENU_ITEM(nwam_menu_item), label);
	return GTK_WIDGET(nwam_menu_item);
}

/** 
 * nwam_menu_item_set_widget:
 * @nwam_menu_item: a #NwamMenuItem.
 * @nwam: a widget to set as the nwam for the menu item.
 * 
 * Sets the nwam of @nwam_menu_item to the given widget.
 * Note that it depends on the show-menu-images setting whether
 * the nwam will be displayed or not.
 **/ 
void
nwam_menu_item_set_widget (NwamMenuItem *self,
  gint pos,
  GtkWidget *widget)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);

	g_return_if_fail (NWAM_IS_MENU_ITEM (self));
	g_assert (pos >= 0 && pos < MAX_WIDGET_NUM);

    if (widget == prv->w[pos])
        return;

    if (prv->w[pos]) {
        gtk_container_remove (GTK_CONTAINER(self), prv->w[pos]);
    }
    prv->w[pos] = widget;

    if (widget == NULL)
        return;

    gtk_widget_set_parent (widget, GTK_WIDGET (self));
    //gtk_widget_queue_resize (GTK_WIDGET (self));
    gtk_widget_show (widget);
}

/**
 * nwam_menu_item_get_widget:
 * @nwam_menu_item: a #NwamMenuItem.
 * @returns: the widget set as nwam of @nwam_menu_item.
 *
 * Gets the widget that is currently set as the nwam of @nwam_menu_item.
 * See nwam_menu_item_set_nwam().
 **/
GtkWidget*
nwam_menu_item_get_widget (NwamMenuItem *self,
			  gint pos)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);

	g_return_val_if_fail (NWAM_IS_MENU_ITEM (self), NULL);
	g_assert (pos >= 0 && pos < MAX_WIDGET_NUM);

    return prv->w[pos];
}

static void
nwam_menu_item_finalize (NwamMenuItem *self)
{
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
/*     g_debug ("nwam_menu_item_finalize"); */

    if (prv->object) {
        NWAM_MENU_ITEM_GET_CLASS(self)->disconnect_object(self, prv->object);

        g_object_unref(prv->object);
    }

	G_OBJECT_CLASS(nwam_menu_item_parent_class)->finalize(G_OBJECT (self));
}

static void
nwam_menu_item_toggle_size_request (GtkMenuItem *menu_item,
                                    gint        *requisition)
{
    NwamMenuItem *self = NWAM_MENU_ITEM (menu_item);
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    GtkPackDirection pack_dir; 

    //g_debug ("nwam_menu_item_toggle_size_request 0x%p", self);
    if (GTK_IS_MENU_BAR (GTK_WIDGET(self)->parent)) 
        pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (GTK_WIDGET(self)->parent)); 
    else 
        pack_dir = GTK_PACK_DIRECTION_LTR; 

	(*GTK_MENU_ITEM_CLASS(nwam_menu_item_parent_class)->toggle_size_request) (menu_item, requisition);

    if (prv->w[0])
    { 
        GtkRequisition child_requisition; 

        gtk_widget_get_child_requisition (prv->w[0],
                                           &child_requisition);

#if 0
        g_debug ("w0, [%d,%d] [%d,%d]",
                 prv->w[0]->allocation.x,
                 prv->w[0]->allocation.y,
                 prv->w[0]->allocation.width,
                 prv->w[0]->allocation.height);
#endif

        if (pack_dir == GTK_PACK_DIRECTION_LTR || 
            pack_dir == GTK_PACK_DIRECTION_RTL) 
        { 
            if (child_requisition.width > 0) 
                *requisition += child_requisition.width; 
        } 
        else 
        { 
            if (child_requisition.height > 0) 
                *requisition += child_requisition.height; 
        } 
    } 
}

static void 
nwam_menu_item_size_request (GtkWidget      *widget, 
                             GtkRequisition *requisition) 
{
    NwamMenuItem *self = NWAM_MENU_ITEM (widget); 
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    gint i = 0;
    gint child_width = 0; 
    gint child_height = 0; 
    GtkPackDirection pack_dir; 
   
    if (GTK_IS_MENU_BAR (widget->parent)) 
        pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent)); 
    else 
        pack_dir = GTK_PACK_DIRECTION_LTR; 
 
    (*GTK_WIDGET_CLASS (nwam_menu_item_parent_class)->size_request) (widget, requisition); 
    for (i = 0; i < MAX_WIDGET_NUM; i++) {
        if (prv->w[i])
        { 
            GtkRequisition child_requisition; 
       
            gtk_widget_size_request (prv->w[i], 
                                     &child_requisition); 
 
#if 0
            g_debug ("w[%d], [%d,%d] [%d,%d]", i,
                     prv->w[i]->allocation.x,
                     prv->w[i]->allocation.y,
                     prv->w[i]->allocation.width,
                     prv->w[i]->allocation.height);
#endif

            child_width = child_requisition.width; 
            child_height = child_requisition.height; 
        } 

        if (pack_dir == GTK_PACK_DIRECTION_LTR || pack_dir == GTK_PACK_DIRECTION_RTL) {
            requisition->height = MAX (requisition->height, child_height); 
            requisition->width += child_width;
        }
        else {
            requisition->height += child_height;
            requisition->width = MAX (requisition->width, child_width); 
        }
    }
} 

static void 
nwam_menu_item_size_allocate (GtkWidget     *widget, 
                              GtkAllocation *allocation) 
{ 
    NwamMenuItem *self = NWAM_MENU_ITEM (widget); 
    NwamMenuItemPrivate *prv = NWAM_MENU_ITEM_GET_PRIVATE(self);
    GtkPackDirection pack_dir; 
    guint toggle_spacing;
    guint toggle_size;
    gint i = 0;
   
    if (GTK_IS_MENU_BAR (widget->parent)) 
        pack_dir = gtk_menu_bar_get_child_pack_direction (GTK_MENU_BAR (widget->parent)); 
    else 
        pack_dir = GTK_PACK_DIRECTION_LTR; 
   
    (*GTK_WIDGET_CLASS (nwam_menu_item_parent_class)->size_allocate) (widget, allocation); 

    gtk_widget_style_get (widget, 
                          "toggle-spacing", &toggle_spacing, 
                          NULL); 
    toggle_size = GTK_MENU_ITEM (self)->toggle_size; 

    i = 0;
    if (prv->w[i])
    { 
        gint x, y, offset; 
        GtkRequisition child_requisition; 
        GtkAllocation child_allocation; 
        guint horizontal_padding;
        guint indicator_size;
 
        gtk_widget_style_get (widget, 
                              "horizontal-padding", &horizontal_padding, 
                              "indicator-size", &indicator_size,
                              NULL); 
       
        gtk_widget_get_child_requisition (prv->w[i],
                                          &child_requisition); 
 
        if (pack_dir == GTK_PACK_DIRECTION_LTR || 
            pack_dir == GTK_PACK_DIRECTION_RTL) 
        { 
            offset = GTK_CONTAINER (self)->border_width + 
                widget->style->xthickness; 
           
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) == 
                (pack_dir == GTK_PACK_DIRECTION_LTR)) 
                x = offset + horizontal_padding + indicator_size +
                    horizontal_padding;
            else 
                x = widget->allocation.width - offset - horizontal_padding - 
                    indicator_size - child_requisition.width;
           
            y = (widget->allocation.height - child_requisition.height) / 2; 
        } 
        else 
        { 
            offset = GTK_CONTAINER (self)->border_width +
                widget->style->ythickness;
           
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) == 
                (pack_dir == GTK_PACK_DIRECTION_TTB)) 
                y = offset + horizontal_padding + indicator_size +
                    horizontal_padding;
            else 
                y = widget->allocation.height - offset - horizontal_padding - 
                    indicator_size - child_requisition.height;
 
            x = (widget->allocation.width - child_requisition.width) / 2; 
        } 
       
        child_allocation.width = child_requisition.width; 
        child_allocation.height = child_requisition.height; 
        child_allocation.x = widget->allocation.x + MAX (x, 0); 
        child_allocation.y = widget->allocation.y + MAX (y, 0); 
 
        gtk_widget_size_allocate (prv->w[i], &child_allocation); 
    } 

    if (prv->w[1])
    { 
        gint x, y, offset; 
        GtkRequisition child_requisition; 
        GtkAllocation child_allocation; 
 
        gtk_widget_get_child_requisition (prv->w[1],
                                          &child_requisition); 
 
        if (pack_dir == GTK_PACK_DIRECTION_LTR || 
            pack_dir == GTK_PACK_DIRECTION_RTL) 
        { 
            offset = GTK_CONTAINER (self)->border_width + 
                widget->style->xthickness;
           
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) == 
                (pack_dir == GTK_PACK_DIRECTION_LTR)) 
                x = widget->allocation.width - offset -
                    toggle_spacing - 
                    child_requisition.width;
            else 
                x = offset + toggle_spacing;

            y = (widget->allocation.height - child_requisition.height) / 2; 
        } 
        else 
        { 
            offset = GTK_CONTAINER (self)->border_width + 
                widget->style->ythickness + prv->w[1]->style->ythickness;
           
            if ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) == 
                (pack_dir == GTK_PACK_DIRECTION_TTB)) 
                y = widget->allocation.height - offset -
                    toggle_spacing -
                    child_requisition.height;
            else
                y = offset + toggle_spacing;
 
            x = (widget->allocation.width - child_requisition.width) / 2; 
        } 
       
        child_allocation.width = child_requisition.width; 
        child_allocation.height = child_requisition.height; 
        child_allocation.x = widget->allocation.x + MAX (x, 0); 
        child_allocation.y = widget->allocation.y + MAX (y, 0); 
 
        gtk_widget_size_allocate (prv->w[1], &child_allocation); 
    } 
}

static void
nwam_menu_item_draw_indicator(GtkCheckMenuItem *check_menu_item,
  GdkRectangle *area)
{
    NwamMenuItem *self = NWAM_MENU_ITEM(check_menu_item);
    GtkWidget *widget; 
    GtkStateType state_type; 
    GtkShadowType shadow_type; 
    gint x, y; 
 
    if (GTK_WIDGET_DRAWABLE (self))
    {
        guint offset; 
        guint toggle_size; 
        guint toggle_spacing; 
        guint horizontal_padding; 
        guint indicator_size; 
       
        widget = GTK_WIDGET (self); 
 
        gtk_widget_style_get (GTK_WIDGET (self), 
                              "toggle-spacing", &toggle_spacing, 
                              "horizontal-padding", &horizontal_padding, 
                              "indicator-size", &indicator_size, 
                              NULL); 
 
        toggle_size = GTK_MENU_ITEM (self)->toggle_size; 
        offset = GTK_CONTAINER (self)->border_width + 
            widget->style->xthickness + 2;  
 
        if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_LTR) 
        { 
            x = widget->allocation.x + offset + horizontal_padding;
        } 
        else  
        { 
            x = widget->allocation.x + widget->allocation.width - 
                indicator_size;
        } 
       
        y = widget->allocation.y + (widget->allocation.height - indicator_size) / 2; 
 
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM(self)) ||
            (GTK_WIDGET_STATE (self) == GTK_STATE_PRELIGHT)) 
        { 
            state_type = GTK_WIDGET_STATE (widget); 
           
            if (gtk_check_menu_item_get_inconsistent(GTK_CHECK_MENU_ITEM(self)))
                shadow_type = GTK_SHADOW_ETCHED_IN; 
            else if (gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(self)))
                shadow_type = GTK_SHADOW_IN; 
            else  
                shadow_type = GTK_SHADOW_OUT; 
           
            if (!GTK_WIDGET_IS_SENSITIVE (widget)) 
                state_type = GTK_STATE_INSENSITIVE; 
 
            if (gtk_check_menu_item_get_draw_as_radio (GTK_CHECK_MENU_ITEM(self)))
            { 
                gtk_paint_option (widget->style, widget->window, 
                                  state_type, shadow_type, 
                                  area, widget, "option", 
                                  x, y, indicator_size, indicator_size); 
            } 
            else 
            { 
                gtk_paint_check (widget->style, widget->window, 
                                 state_type, shadow_type, 
                                 area, widget, "check", 
                                 x, y, indicator_size, indicator_size); 
            } 
        } 
    } 
}

static void
nwam_menu_item_activate (GtkMenuItem *menu_item)
{
    //g_debug("item 0x%p group: 0x%p", menu_item, gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM(menu_item)));
    (*GTK_MENU_ITEM_CLASS (nwam_menu_item_parent_class)->activate) (menu_item);
}

void
menu_item_set_label(GtkMenuItem *item, const gchar *label)
{
    g_assert(GTK_IS_MENU_ITEM(item));
#if 0
    /* Only for gtk 2.14 */
    GtkWidget *accel_label = gtk_bin_get_child(GTK_BIN(item));
    if (accel_label == NULL) {
        accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
        gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

        gtk_container_add (GTK_CONTAINER (item), accel_label);
        gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL (accel_label), GTK_WIDGET(item));
        gtk_widget_show (accel_label);
    }
    g_assert(GTK_IS_LABEL(accel_label));
    item = accel_label;
#endif
    g_object_set(item,
      "use-underline", TRUE,
      "label", label ? label : "",
      NULL);
}

void
menu_item_set_markup(GtkMenuItem *item, const gchar *label)
{
    GtkWidget *accel_label = gtk_bin_get_child(GTK_BIN(item));

    g_assert(GTK_IS_MENU_ITEM(item));

    if (accel_label == NULL) {
        accel_label = g_object_new (GTK_TYPE_ACCEL_LABEL, NULL);
        gtk_misc_set_alignment (GTK_MISC (accel_label), 0.0, 0.5);

        gtk_container_add (GTK_CONTAINER (item), accel_label);
        gtk_accel_label_set_accel_widget(GTK_ACCEL_LABEL (accel_label), GTK_WIDGET(item));
        gtk_widget_show (accel_label);
    }
    gtk_label_set_markup(GTK_LABEL (accel_label), label);
}

gint
nwam_menu_item_compare(NwamMenuItem *self, NwamMenuItem *other)
{
    return NWAM_MENU_ITEM_GET_CLASS(self)->compare(NWAM_MENU_ITEM(self), NWAM_MENU_ITEM(other));
}

