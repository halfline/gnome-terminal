/*
 * Copyright © 2008, 2010 Christian Persch
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>

#include "terminal-screen-container.h"
#include "terminal-debug.h"
#include "terminal-intl.h"
#include "gedit-animated-overlay.h"
#include "gedit-floating-slider.h"

#include <gtk/gtk.h>

#define SEARCH_POPUP_MARGIN 20

#define TERMINAL_SCREEN_CONTAINER_GET_PRIVATE(screen_container)(G_TYPE_INSTANCE_GET_PRIVATE ((screen_container), TERMINAL_TYPE_SCREEN_CONTAINER, TerminalScreenContainerPrivate))

struct _TerminalScreenContainerPrivate
{
  GtkWidget *overlay;
  GtkWidget *slider;
  GtkWidget *search_entry;
  GtkWidget *go_up_button;
  GtkWidget *go_down_button;

  TerminalScreen *screen;
#ifdef USE_SCROLLED_WINDOW
  GtkWidget *scrolled_window;
#else
  GtkWidget *hbox;
  GtkWidget *vscrollbar;
#endif
  GtkPolicyType hscrollbar_policy;
  GtkPolicyType vscrollbar_policy;
  GtkCornerType window_placement;
  guint window_placement_set : 1;
};

enum
{
  PROP_0,
  PROP_SCREEN,
  PROP_HSCROLLBAR_POLICY,
  PROP_VSCROLLBAR_POLICY,
  PROP_WINDOW_PLACEMENT,
  PROP_WINDOW_PLACEMENT_SET
};

G_DEFINE_TYPE (TerminalScreenContainer, terminal_screen_container, GTK_TYPE_VBOX)

/* helper functions */

static void
terminal_screen_container_set_placement_internal (TerminalScreenContainer *container,
                                                  GtkCornerType corner)
{
  TerminalScreenContainerPrivate *priv = container->priv;

#ifdef USE_SCROLLED_WINDOW
  gtk_scrolled_window_set_placement (GTK_SCROLLED_WINDOW (priv->scrolled_window), corner);
#else
  switch (corner) {
    case GTK_CORNER_TOP_LEFT:
    case GTK_CORNER_BOTTOM_LEFT:
      gtk_box_reorder_child (GTK_BOX (priv->hbox), priv->vscrollbar, 1);
      break;
    case GTK_CORNER_TOP_RIGHT:
    case GTK_CORNER_BOTTOM_RIGHT:
      gtk_box_reorder_child (GTK_BOX (priv->hbox), priv->vscrollbar, 0);
      break;
    default:
      g_assert_not_reached ();
  }
#endif

  priv->window_placement = corner;
  g_object_notify (G_OBJECT (container), "window-placement");
}

static void
terminal_screen_container_set_placement_set (TerminalScreenContainer *container,
                                             gboolean set)
{
  TerminalScreenContainerPrivate *priv = container->priv;

#ifdef USE_SCROLLED_WINDOW
  g_object_set (priv->scrolled_window, "window-placement-set", set, NULL);
#endif

  priv->window_placement_set = set != FALSE;
  g_object_notify (G_OBJECT (container), "window-placement-set");
}

#if defined(USE_SCROLLED_WINDOW) && defined(GNOME_ENABLE_DEBUG)
static void
size_request_cb (GtkWidget *widget,
                 GtkRequisition *req,
                 TerminalScreenContainer *container)
{
  _terminal_debug_print (TERMINAL_DEBUG_GEOMETRY,
                         "[screen %p] scrolled-window size req %d : %d\n",
                         container->priv->screen, req->width, req->height);
}
#endif

/* Class implementation */

static void
terminal_screen_container_init (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv;

  priv = container->priv = TERMINAL_SCREEN_CONTAINER_GET_PRIVATE (container);

  priv->hscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->vscrollbar_policy = GTK_POLICY_AUTOMATIC;
  priv->window_placement = GTK_CORNER_BOTTOM_RIGHT;
  priv->window_placement_set = FALSE;
}

static GObject *
terminal_screen_container_constructor (GType type,
                                guint n_construct_properties,
                                GObjectConstructParam *construct_params)
{
  GObject *object;
  TerminalScreenContainer *container;
  TerminalScreenContainerPrivate *priv;

  object = G_OBJECT_CLASS (terminal_screen_container_parent_class)->constructor
             (type, n_construct_properties, construct_params);

  container = TERMINAL_SCREEN_CONTAINER (object);
  priv = container->priv;

  g_assert (priv->screen != NULL);

  priv->overlay = gedit_animated_overlay_new ();
  gtk_widget_show (priv->overlay);

#ifdef USE_SCROLLED_WINDOW
#if GTK_CHECK_VERSION (2, 91, 2)
  priv->scrolled_window = gtk_scrolled_window_new (gtk_scrollable_get_hadjustment(GTK_SCROLLABLE(priv->screen)),
                                                   gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(priv->screen)));
#else
  priv->scrolled_window = gtk_scrolled_window_new (NULL, vte_terminal_get_adjustment (VTE_TERMINAL (priv->screen)));
#endif

  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                  priv->hscrollbar_policy,
                                  priv->vscrollbar_policy);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (priv->scrolled_window),
                                       GTK_SHADOW_NONE);
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), GTK_WIDGET (priv->screen));
  gtk_widget_show (GTK_WIDGET (priv->screen));
  gtk_widget_show (priv->scrolled_window);

  gtk_container_add (GTK_CONTAINER (priv->overlay), priv->scrolled_window);

#ifdef GNOME_ENABLE_DEBUG
  g_signal_connect (priv->scrolled_window, "size-request", G_CALLBACK (size_request_cb), container);
#endif

#else

  priv->hbox = gtk_hbox_new (FALSE, 0);

#if GTK_CHECK_VERSION (2, 91, 2)
  priv->vscrollbar = gtk_vscrollbar_new (gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(priv->screen)));
#else
  priv->vscrollbar = gtk_vscrollbar_new (vte_terminal_get_adjustment (VTE_TERMINAL (priv->screen)));
#endif

  gtk_box_pack_start (GTK_BOX (priv->hbox), GTK_WIDGET (priv->screen), TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (priv->hbox), priv->vscrollbar, FALSE, FALSE, 0);

  gtk_container_add (GTK_CONTAINER (priv->overlay), priv->hbox);
  gtk_widget_show_all (priv->hbox);
#endif /* USE_SCROLLED_WINDOW */

  gtk_box_pack_end (GTK_BOX (container), priv->overlay, TRUE, TRUE, 0);

  _terminal_screen_update_scrollbar (priv->screen);

  /* Add slider */
  priv->slider = gedit_floating_slider_new ();
  gtk_widget_set_name (priv->slider, "search-slider");
  gtk_widget_set_halign (priv->slider, GTK_ALIGN_END);
  gtk_widget_set_valign (priv->slider, GTK_ALIGN_START);

  if (gtk_widget_get_direction (priv->slider) == GTK_TEXT_DIR_LTR)
    gtk_widget_set_margin_right (priv->slider, SEARCH_POPUP_MARGIN);
  else
    gtk_widget_set_margin_left (priv->slider, SEARCH_POPUP_MARGIN);

  g_object_set (G_OBJECT (priv->slider),
                "easing", GEDIT_THEATRICS_CHOREOGRAPHER_EASING_EXPONENTIAL_IN_OUT,
                "blocking", GEDIT_THEATRICS_CHOREOGRAPHER_BLOCKING_DOWNSTAGE,
                "orientation", GTK_ORIENTATION_VERTICAL,
                NULL);

  gedit_animated_overlay_add_animated_overlay (GEDIT_ANIMATED_OVERLAY (priv->overlay),
                                               GEDIT_ANIMATABLE (priv->slider));

  return object;
}

static void
terminal_screen_container_get_property (GObject *object,
                                        guint prop_id,
                                        GValue *value,
                                        GParamSpec *pspec)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
  TerminalScreenContainerPrivate *priv = container->priv;

  switch (prop_id) {
    case PROP_SCREEN:
      break;
    case PROP_HSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->hscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      g_value_set_enum (value, priv->vscrollbar_policy);
      break;
    case PROP_WINDOW_PLACEMENT:
      g_value_set_enum (value, priv->window_placement);
      break;
    case PROP_WINDOW_PLACEMENT_SET:
      g_value_set_boolean (value, priv->window_placement_set);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_screen_container_set_property (GObject *object,
                                        guint prop_id,
                                        const GValue *value,
                                        GParamSpec *pspec)
{
  TerminalScreenContainer *container = TERMINAL_SCREEN_CONTAINER (object);
  TerminalScreenContainerPrivate *priv = container->priv;

  switch (prop_id) {
    case PROP_SCREEN:
      priv->screen = g_value_get_object (value);
      break;
    case PROP_HSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            g_value_get_enum (value),
                                            priv->vscrollbar_policy);
      break;
    case PROP_VSCROLLBAR_POLICY:
      terminal_screen_container_set_policy (container,
                                            priv->hscrollbar_policy,
                                            g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT:
      terminal_screen_container_set_placement_internal (container, g_value_get_enum (value));
      break;
    case PROP_WINDOW_PLACEMENT_SET:
      terminal_screen_container_set_placement_set (container, g_value_get_boolean (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
terminal_screen_container_class_init (TerminalScreenContainerClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

  g_type_class_add_private (gobject_class, sizeof (TerminalScreenContainerPrivate));

  gobject_class->constructor = terminal_screen_container_constructor;
  gobject_class->get_property = terminal_screen_container_get_property;
  gobject_class->set_property = terminal_screen_container_set_property;

  g_object_class_install_property
    (gobject_class,
     PROP_SCREEN,
     g_param_spec_object ("screen", NULL, NULL,
                          TERMINAL_TYPE_SCREEN,
                          G_PARAM_READWRITE |
                          G_PARAM_CONSTRUCT_ONLY |
                          G_PARAM_STATIC_STRINGS));

   g_object_class_install_property
    (gobject_class,
     PROP_HSCROLLBAR_POLICY,
     g_param_spec_enum ("hscrollbar-policy", NULL, NULL,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));
   g_object_class_install_property
    (gobject_class,
     PROP_VSCROLLBAR_POLICY,
     g_param_spec_enum ("vscrollbar-policy", NULL, NULL,
                        GTK_TYPE_POLICY_TYPE,
                        GTK_POLICY_AUTOMATIC,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property
    (gobject_class,
     PROP_WINDOW_PLACEMENT,
     g_param_spec_enum ("window-placement", NULL, NULL,
                        GTK_TYPE_CORNER_TYPE,
                        GTK_CORNER_TOP_LEFT,
                        G_PARAM_READWRITE |
                        G_PARAM_STATIC_STRINGS));
  
  g_object_class_install_property
    (gobject_class,
     PROP_WINDOW_PLACEMENT_SET,
     g_param_spec_boolean ("window-placement-set", NULL, NULL,
                           FALSE,
                           G_PARAM_READWRITE |
                           G_PARAM_STATIC_STRINGS));
}

static GtkWidget *
create_button_from_symbolic (const gchar *icon_name)
{
  GtkWidget *button;

  button = gtk_button_new ();
  gtk_widget_set_can_focus (button, FALSE);
  gtk_button_set_image (GTK_BUTTON (button),
                              gtk_image_new_from_icon_name (icon_name,
                                                            GTK_ICON_SIZE_MENU));

  return button;
}

static GtkWidget *
create_search_widget (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv = container->priv;
  GtkWidget *search_widget;
  GtkStyleContext *context;

  search_widget = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0);

  context = gtk_widget_get_style_context (search_widget);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_LINKED);

  gtk_widget_show (search_widget);

  /* add entry */
  priv->search_entry = gtk_entry_new ();
  gtk_widget_show (priv->search_entry);

  gtk_entry_set_width_chars (GTK_ENTRY (priv->search_entry), 25);

  /*g_signal_connect (frame->priv->search_entry, "populate-popup",
                    G_CALLBACK (search_entry_populate_popup),
                    frame);
  g_signal_connect (frame->priv->search_entry, "icon-release",
                    G_CALLBACK (search_entry_icon_release),
                    frame);
  g_signal_connect (priv->search_entry, "activate",
                    G_CALLBACK (search_entry_activate),
                    container);*/

  gtk_container_add (GTK_CONTAINER (search_widget),
                     priv->search_entry);

  /* Up/Down buttons */
  priv->go_up_button = create_button_from_symbolic ("go-up-symbolic");
  gtk_widget_show (priv->go_up_button);
  gtk_box_pack_start (GTK_BOX (search_widget), priv->go_up_button,
                      FALSE, FALSE, 0);
  /*g_signal_connect (frame->priv->go_up_button,
                    "clicked",
                    G_CALLBACK (on_go_up_button_clicked),
                    frame);*/

  priv->go_down_button = create_button_from_symbolic ("go-down-symbolic");
  gtk_widget_show (priv->go_down_button);
  gtk_box_pack_start (GTK_BOX (search_widget), priv->go_down_button,
                      FALSE, FALSE, 0);
  /*g_signal_connect (frame->priv->go_down_button,
                    "clicked",
                    G_CALLBACK (on_go_down_button_clicked),
                    frame);*/

  return search_widget;
}

/* public API */

/**
 * terminal_screen_container_new:
 * @screen: a #TerminalScreen
 *
 * Returns: a new #TerminalScreenContainer for @screen
 */
GtkWidget *
terminal_screen_container_new (TerminalScreen *screen)
{
  return g_object_new (TERMINAL_TYPE_SCREEN_CONTAINER,
                       "screen", screen,
                       NULL);
}

/**
 * terminal_screen_container_get_screen:
 * @container: a #TerminalScreenContainer
 *
 * Returns: @container's #TerminalScreen
 */
TerminalScreen *
terminal_screen_container_get_screen (TerminalScreenContainer *container)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container), NULL);

  return container->priv->screen;
}

/**
 * terminal_screen_container_get_from_screen:
 * @screen: a #TerminalScreenContainerPrivate
 *
 * Returns the #TerminalScreenContainer containing @screen.
 */
TerminalScreenContainer *
terminal_screen_container_get_from_screen (TerminalScreen *screen)
{
  g_return_val_if_fail (TERMINAL_IS_SCREEN (screen), NULL);

  return TERMINAL_SCREEN_CONTAINER (gtk_widget_get_ancestor (GTK_WIDGET (screen), TERMINAL_TYPE_SCREEN_CONTAINER));
}

/**
 * terminal_screen_container_set_policy:
 * @container: a #TerminalScreenContainer
 * @hpolicy: a #GtkPolicyType
 * @vpolicy: a #GtkPolicyType
 *
 * Sets @container's scrollbar policy.
 */
void
terminal_screen_container_set_policy (TerminalScreenContainer *container,
                                      GtkPolicyType hpolicy G_GNUC_UNUSED,
                                      GtkPolicyType vpolicy)
{
  TerminalScreenContainerPrivate *priv;
  GObject *object;

  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  object = G_OBJECT (container);
  priv = container->priv;

  g_object_freeze_notify (object);

  if (priv->hscrollbar_policy != hpolicy) {
    priv->hscrollbar_policy = hpolicy;
    g_object_notify (object, "hscrollbar-policy");
  }
  if (priv->vscrollbar_policy != vpolicy) {
    priv->vscrollbar_policy = vpolicy;
    g_object_notify (object, "vscrollbar-policy");
  }

#ifdef USE_SCROLLED_WINDOW
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (priv->scrolled_window), hpolicy, vpolicy);
#else
  switch (vpolicy) {
    case GTK_POLICY_NEVER:
      gtk_widget_hide (priv->vscrollbar);
      break;
    case GTK_POLICY_AUTOMATIC:
    case GTK_POLICY_ALWAYS:
      gtk_widget_show (priv->vscrollbar);
      break;
    default:
      g_assert_not_reached ();
  }
#endif

  g_object_thaw_notify (object);
}

/**
 * terminal_screen_container_set_placement:
 * @container: a #TerminalScreenContainer
 * @corner: a #GtkCornerType
 *
 * Sets @container's window placement.
 */
void
terminal_screen_container_set_placement (TerminalScreenContainer *container,
                                         GtkCornerType corner)
{
  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  terminal_screen_container_set_placement_internal (container, corner);
  terminal_screen_container_set_placement_set (container, TRUE);
}

void
terminal_screen_container_present_search (TerminalScreenContainer *container)
{
  TerminalScreenContainerPrivate *priv;

  g_return_if_fail (TERMINAL_IS_SCREEN_CONTAINER (container));

  priv = container->priv;

  if (gtk_bin_get_child (GTK_BIN (priv->slider)) == NULL)
    gtk_container_add (GTK_CONTAINER (priv->slider),
                       create_search_widget (container));

  /* To slide in we set the right animation state in the animatable */
  g_object_set (G_OBJECT (priv->slider),
                "animation-state", GEDIT_THEATRICS_ANIMATION_STATE_COMING,
                NULL);
}
