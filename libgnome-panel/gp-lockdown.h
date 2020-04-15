/*
 * Copyright (C) 2020 Alberts Muktupāvels
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
 * License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <https://www.gnu.org/licenses/>.
 */

#ifndef GP_LOCKDOWN_H
#define GP_LOCKDOWN_H

#include <glib.h>

G_BEGIN_DECLS

/**
 * GpLockdownFlags:
 * @GP_LOCKDOWN_FLAGS_NONE: No flags set.
 * @GP_LOCKDOWN_FLAGS_APPLET: Applet is disabled.
 * @GP_LOCKDOWN_FLAGS_FORCE_QUIT: Force quit is disabled.
 * @GP_LOCKDOWN_FLAGS_LOCKED_DOWN: Panel is lockded down.
 * @GP_LOCKDOWN_FLAGS_COMMAND_LINE: Command line is disabled.
 * @GP_LOCKDOWN_FLAGS_LOCK_SCREEN: Lock screen is disabled.
 * @GP_LOCKDOWN_FLAGS_LOG_OUT: Log out is disabled.
 * @GP_LOCKDOWN_FLAGS_USER_SWITCHING: User switching is disabled.
 *
 * Flags indicating active lockdowns.
 */
typedef enum
{
  GP_LOCKDOWN_FLAGS_NONE = 0,

  GP_LOCKDOWN_FLAGS_APPLET = 1 << 0,
  GP_LOCKDOWN_FLAGS_FORCE_QUIT = 1 << 1,
  GP_LOCKDOWN_FLAGS_LOCKED_DOWN = 1 << 2,

  GP_LOCKDOWN_FLAGS_COMMAND_LINE = 1 << 3,
  GP_LOCKDOWN_FLAGS_LOCK_SCREEN = 1 << 4,
  GP_LOCKDOWN_FLAGS_LOG_OUT = 1 << 5,
  GP_LOCKDOWN_FLAGS_USER_SWITCHING = 1 << 6
} GpLockdownFlags;

G_END_DECLS

#endif
