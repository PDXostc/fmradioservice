#
/*
 *
 *    Copyright (C) 2014
 *    Jan van Katwijk (J.vanKatwijk@gmail.com)
 *    Lazy Chair Programming
 *
 *    This file is part of the SDR-J.
 *    Many of the ideas as implemented in SDR-J are derived from
 *    other work, made available through the GNU general Public License. 
 *    All copyrights of the original authors are recognized.
 *
 *    SDR-J is free software; you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation; either version 2 of the License, or
 *    (at your option) any later version.
 *
 *    SDR-J is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with SDR-J; if not, write to the Free Software
 *    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *	Main program
 */
#include	<unistd.h>
#include	"fm-constants.h"
#include	"gui.h"

/*
void	fullPathfor (const char *v, char *out) {
int16_t	i;
QString	homeDir;

	if (v == NULL) {
	   sprintf (out, "%s", "weet niet");
	   return;	// should not happen
	}

	if (v [0] == '/') {		// full path specified
	   sprintf (out, "%s", v);
	   return;
	}

	homeDir = QDir::homePath ();
	homeDir. append ("/");
	homeDir. append (v);
	homeDir	= QDir::toNativeSeparators (homeDir);
	sprintf (out, "%s", homeDir. toLatin1 (). data ());
	fprintf (stderr, "ini file = %s\n", out);

	for (i = 0; out [i] != 0; i ++);
	if (out [i - 4] != '.' ||
	    out [i - 3] != 'i' ||
	    out [i - 2] != 'n' ||
	    out [i - 1] != 'i') {
	    out [i] = '.';
	    out [i + 1] = 'i';
	    out [i + 2] = 'n';
	    out [i + 3] = 'i';
	    out [i + 4] = 0;
	}
}
*/

bool	fileExists (char *v) {
FILE *f;

	f = fopen (v, "r");
	if (f == NULL)
	   return false;
	fclose (f);
	return true;
}

int	main (int argc, char **argv) {
RadioInterface	*MyRadioInterface;

	MyRadioInterface = new RadioInterface;
	while (1) sleep (10);
	delete MyRadioInterface;
	exit (0);
}

