/*
 * Copyright (C) 2010-2011 Daiki Ueno <ueno@unixuser.org>
 * Copyright (C) 2010-2011 Red Hat, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * SECTION:eekboard-service
 * @short_description: base implementation of eekboard service
 *
 * Provides a dbus object, and contains the context.
 *
 * The #EekboardService class provides a base server side
 * implementation of eekboard service.
 */

#include "config.h"


#include <stdio.h>

#include <gio/gio.h>

#include "eekboard/eekboard-service.h"

void
eekboard_service_destroy(EekboardService *service)
{
    g_free (service->object_path);

    if (service->connection) {
        if (service->registration_id > 0) {
            g_dbus_connection_unregister_object (service->connection,
                                                 service->registration_id);
            service->registration_id = 0;
        }

        g_object_unref (service->connection);
        service->connection = NULL;
    }

    if (service->introspection_data) {
        g_dbus_node_info_unref (service->introspection_data);
        service->introspection_data = NULL;
    }

    if (service->context) {
        g_signal_handlers_disconnect_by_data (service->context, service);
        service->context = NULL;
    }

    free(service);
}

static gboolean
handle_set_visible(SmPuriOSK0 *object, GDBusMethodInvocation *invocation,
                   gboolean arg_visible, gpointer user_data) {
    EekboardService *service = user_data;

    if (service->context) {
        if (arg_visible) {
            server_context_service_show_keyboard (service->context);
        } else {
            server_context_service_hide_keyboard (service->context);
        }
    }
    sm_puri_osk0_complete_set_visible(object, invocation);
    return TRUE;
}

static void on_visible(EekboardService *service,
                       GParamSpec *pspec,
                       ServerContextService *context)
{
    (void)pspec;
    gboolean visible;

    g_return_if_fail (SERVER_IS_CONTEXT_SERVICE (context));

    g_object_get (context, "visible", &visible, NULL);

    sm_puri_osk0_set_visible(service->dbus_interface, visible);
}

/**
 * eekboard_service_new:
 * @connection: a #GDBusConnection
 * @object_path: object path
 */
EekboardService *
eekboard_service_new (GDBusConnection *connection,
                      const gchar     *object_path)
{
    EekboardService *self = calloc(1, sizeof(EekboardService));
    self->object_path = g_strdup(object_path);
    self->connection = connection;

    self->dbus_interface = sm_puri_osk0_skeleton_new();
    g_signal_connect(self->dbus_interface, "handle-set-visible",
                     G_CALLBACK(handle_set_visible), self);

    if (self->connection && self->object_path) {
        GError *error = NULL;

        if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(self->dbus_interface),
                                              self->connection,
                                              self->object_path,
                                              &error)) {
            g_warning("Error registering dbus object: %s\n", error->message);
            g_clear_error(&error);
            // TODO: return an error
        }
    }
    return self;
}

void
eekboard_service_set_context(EekboardService *service,
                             ServerContextService *context)
{
    g_return_if_fail (!service->context);

    service->context = context;

    g_signal_connect_swapped (service->context,
                              "notify::visible",
                              G_CALLBACK(on_visible),
                              service);
}
