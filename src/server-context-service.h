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
#ifndef SERVER_CONTEXT_SERVICE_H
#define SERVER_CONTEXT_SERVICE_H 1

#include "src/layout.h"
#include "src/submission.h"
#include "ui_manager.h"

G_BEGIN_DECLS

#define SERVER_TYPE_CONTEXT_SERVICE (server_context_service_get_type())

/** Manages the lifecycle of the window displaying layouts. */
G_DECLARE_FINAL_TYPE (ServerContextService, server_context_service, SERVER, CONTEXT_SERVICE, GObject)

ServerContextService *server_context_service_new(EekboardContextService *self, struct submission *submission, struct squeek_layout_state *layout, struct ui_manager *uiman, struct vis_manager *visman);
enum squeek_arrangement_kind server_context_service_get_layout_type(ServerContextService *);
void server_context_service_force_show_keyboard (ServerContextService *self);
void server_context_service_hide_keyboard (ServerContextService *self);
G_END_DECLS
#endif  /* SERVER_CONTEXT_SERVICE_H */

