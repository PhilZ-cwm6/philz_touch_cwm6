/*
    PhilZ Touch - touch_gui library
    Copyright (C) <2014>  <phytowardt@gmail.com>

    This file is part of PhilZ Touch Recovery

    PhilZ Touch is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PhilZ Touch is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PhilZ Touch.  If not, see <http://www.gnu.org/licenses/>.

*/

#ifndef __NANDROID_GUI_H
#define __NANDROID_GUI_H

// timer reset on each touch screen event (dim screen during nandroid jobs)
long long last_key_ev;

int user_cancel_nandroid(FILE **fp, const char* backup_file_image, int is_backup, int *nand_starts);

#endif // __NANDROID_GUI_H

