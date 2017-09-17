/**************************************************************************
 *   history.c  --  This file is part of GNU nano.                        *
 *                                                                        *
 *   Copyright (C) 2003-2011, 2013-2017 Free Software Foundation, Inc.    *
 *   Copyright (C) 2016 Benno Schulenberg                                 *
 *                                                                        *
 *   GNU nano is free software: you can redistribute it and/or modify     *
 *   it under the terms of the GNU General Public License as published    *
 *   by the Free Software Foundation, either version 3 of the License,    *
 *   or (at your option) any later version.                               *
 *                                                                        *
 *   GNU nano is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty          *
 *   of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.              *
 *   See the GNU General Public License for more details.                 *
 *                                                                        *
 *   You should have received a copy of the GNU General Public License    *
 *   along with this program.  If not, see http://www.gnu.org/licenses/.  *
 *                                                                        *
 **************************************************************************/

#include "proto.h"

#include <errno.h>
#include <string.h>

#ifndef DISABLE_HISTORIES
static bool history_changed = FALSE;
	/* Have any of the history lists changed? */

/* Initialize the search and replace history lists. */
void history_init(void)
{
    search_history = make_new_node(NULL);
    search_history->data = mallocstrcpy(NULL, "");
    searchage = search_history;
    searchbot = search_history;

    replace_history = make_new_node(NULL);
    replace_history->data = mallocstrcpy(NULL, "");
    replaceage = replace_history;
    replacebot = replace_history;

    execute_history = make_new_node(NULL);
    execute_history->data = mallocstrcpy(NULL, "");
    executetop = execute_history;
    executebot = execute_history;
}

/* Set the current position in the history list h to the bottom. */
void history_reset(const filestruct *h)
{
    if (h == search_history)
	search_history = searchbot;
    else if (h == replace_history)
	replace_history = replacebot;
    else if (h == execute_history)
	execute_history = executebot;
}

/* Return the first node containing the first len characters of the
 * string s in the history list, starting at h_start and ending at
 * h_end, or NULL if there isn't one. */
filestruct *find_history(const filestruct *h_start, const filestruct
	*h_end, const char *s, size_t len)
{
    const filestruct *p;

    for (p = h_start; p != h_end->prev && p != NULL; p = p->prev) {
	if (strncmp(s, p->data, len) == 0)
	    return (filestruct *)p;
    }

    return NULL;
}

/* Update a history list (the one in which h is the current position)
 * with a fresh string s.  That is: add s, or move it to the end. */
void update_history(filestruct **h, const char *s)
{
    filestruct **hage = NULL, **hbot = NULL, *thesame;

    assert(h != NULL && s != NULL);

    if (*h == search_history) {
	hage = &searchage;
	hbot = &searchbot;
    } else if (*h == replace_history) {
	hage = &replaceage;
	hbot = &replacebot;
    } else if (*h == execute_history) {
	hage = &executetop;
	hbot = &executebot;
    }

    assert(hage != NULL && hbot != NULL);

    /* See if the string is already in the history. */
    thesame = find_history(*hbot, *hage, s, HIGHEST_POSITIVE);

    /* If an identical string was found, delete that item. */
    if (thesame != NULL) {
	filestruct *after = thesame->next;

	/* If the string is at the head of the list, move the head. */
	if (thesame == *hage)
	    *hage = after;

	unlink_node(thesame);
	renumber(after);
    }

    /* If the history is full, delete the oldest item (the one at the
     * head of the list), to make room for a new item at the end. */
    if ((*hbot)->lineno == MAX_SEARCH_HISTORY + 1) {
	filestruct *oldest = *hage;

	*hage = (*hage)->next;
	unlink_node(oldest);
	renumber(*hage);
    }

    /* Store the fresh string in the last item, then create a new item. */
    (*hbot)->data = mallocstrcpy((*hbot)->data, s);
    splice_node(*hbot, make_new_node(*hbot));
    *hbot = (*hbot)->next;
    (*hbot)->data = mallocstrcpy(NULL, "");

    /* Indicate that the history needs to be saved on exit. */
    history_changed = TRUE;

    /* Set the current position in the list to the bottom. */
    *h = *hbot;
}

/* Move h to the string in the history list just before it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_older(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->prev == NULL)
	return NULL;

    *h = (*h)->prev;

    return (*h)->data;
}

/* Move h to the string in the history list just after it, and return
 * that string.  If there isn't one, don't move h and return NULL. */
char *get_history_newer(filestruct **h)
{
    assert(h != NULL);

    if ((*h)->next == NULL)
	return NULL;

    *h = (*h)->next;

    return (*h)->data;
}

/* More placeholders. */
void get_history_newer_void(void)
{
    ;
}
void get_history_older_void(void)
{
    ;
}

#ifdef ENABLE_TABCOMP
/* Move h to the next string that's a tab completion of the string s,
 * looking at only the first len characters of s, and return that
 * string.  If there isn't one, or if len is 0, don't move h and return
 * s. */
char *get_history_completion(filestruct **h, char *s, size_t len)
{
    assert(s != NULL);

    if (len > 0) {
	filestruct *hage = NULL, *hbot = NULL, *p;

	assert(h != NULL);

	if (*h == search_history) {
	    hage = searchage;
	    hbot = searchbot;
	} else if (*h == replace_history) {
	    hage = replaceage;
	    hbot = replacebot;
	} else if (*h == execute_history) {
	    hage = executetop;
	    hbot = executebot;
	}

	assert(hage != NULL && hbot != NULL);

	/* Search the history list from the current position to the top
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history((*h)->prev, hage, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->prev, hage, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}

	/* Search the history list from the bottom to the current position
	 * for a match of len characters.  Skip over an exact match. */
	p = find_history(hbot, *h, s, len);

	while (p != NULL && strcmp(p->data, s) == 0)
	    p = find_history(p->prev, *h, s, len);

	if (p != NULL) {
	    *h = p;
	    return mallocstrcpy(s, (*h)->data);
	}
    }

    /* If we're here, we didn't find a match, we didn't find an inexact
     * match, or len is 0.  Return s. */
    return (char *)s;
}
#endif /* ENSABLE_TABCOMP */

/* Return the constructed dirfile path, or NULL if we can't find the home
 * directory.  The string is dynamically allocated, and should be freed. */
char *construct_filename(const char *str)
{
    char *newstr = NULL;

    if (homedir != NULL) {
	size_t homelen = strlen(homedir);

	newstr = charalloc(homelen + strlen(str) + 1);
	strcpy(newstr, homedir);
	strcpy(newstr + homelen, str);
    }

    return newstr;
}

char *histfilename(void)
{
    return construct_filename("/.nano/search_history");
}

/* Construct the legacy history filename. */
/* (To be removed in 2018.) */
char *legacyhistfilename(void)
{
    return construct_filename("/.nano_history");
}

char *poshistfilename(void)
{
    return construct_filename("/.nano/filepos_history");
}

void history_error(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, _(msg), ap);
    va_end(ap);

    fprintf(stderr, _("\nPress Enter to continue\n"));
    while (getchar() != '\n')
	;
}

/* Now that we have more than one history file, let's just rely on a
 * .nano dir for this stuff.  Return 1 if the dir exists or was
 * successfully created, and return 0 otherwise. */
int check_dotnano(void)
{
    int ret = 1;
    struct stat dirstat;
    char *nanodir = construct_filename("/.nano");

    if (stat(nanodir, &dirstat) == -1) {
	if (mkdir(nanodir, S_IRWXU | S_IRWXG | S_IRWXO) == -1) {
	    history_error(N_("Unable to create directory %s: %s\n"
				"It is required for saving/loading "
				"search history or cursor positions.\n"),
				nanodir, strerror(errno));
	    ret = 0;
	}
    } else if (!S_ISDIR(dirstat.st_mode)) {
	history_error(N_("Path %s is not a directory and needs to be.\n"
				"Nano will be unable to load or save "
				"search history or cursor positions.\n"),
				nanodir);
	ret = 0;
    }

    free(nanodir);
    return ret;
}

/* Load the search and replace histories from ~/.nano/search_history. */
void load_history(void)
{
    char *searchhist = histfilename();
    char *legacyhist = legacyhistfilename();
    struct stat hstat;
    FILE *hist;

    /* If no home directory was found, we can't do anything. */
    if (searchhist == NULL || legacyhist == NULL)
	return;

    /* If there is an old history file, migrate it. */
    /* (To be removed in 2018.) */
    if (stat(legacyhist, &hstat) != -1 && stat(searchhist, &hstat) == -1) {
	if (rename(legacyhist, searchhist) == -1)
	    history_error(N_("Detected a legacy nano history file (%s) which I tried to move\n"
			     "to the preferred location (%s) but encountered an error: %s"),
				legacyhist, searchhist, strerror(errno));
	else
	    history_error(N_("Detected a legacy nano history file (%s) which I moved\n"
			     "to the preferred location (%s)\n(see the nano FAQ about this change)"),
				legacyhist, searchhist);
    }

    hist = fopen(searchhist, "rb");

    if (hist == NULL) {
	if (errno != ENOENT) {
	    /* When reading failed, don't save history when we quit. */
	    UNSET(HISTORYLOG);
	    history_error(N_("Error reading %s: %s"), searchhist,
			strerror(errno));
	}
    } else {
	/* Load the two history lists -- first the search history, then
	 * the replace history -- from the oldest entry to the newest.
	 * The two lists are separated by an empty line. */
	filestruct **history = &search_history;
	char *line = NULL;
	size_t buf_len = 0;
	ssize_t read;

	while ((read = getline(&line, &buf_len, hist)) > 0) {
	    line[--read] = '\0';
	    if (read > 0) {
		/* Encode any embedded NUL as 0x0A. */
		unsunder(line, read);
		update_history(history, line);
	    } else if (history == &search_history)
		history = &replace_history;
	   else
		history = &execute_history;

	}

	fclose(hist);
	free(line);
    }

    free(searchhist);
    free(legacyhist);
}

/* Write the lines of a history list, starting with the line at head, to
 * the open file at hist.  Return TRUE if the write succeeded, and FALSE
 * otherwise. */
bool writehist(FILE *hist, const filestruct *head)
{
    const filestruct *item;

    /* Write a history list, from the oldest item to the newest. */
    for (item = head; item != NULL; item = item->next) {
	size_t length = strlen(item->data);

	/* Decode 0x0A bytes as embedded NULs. */
	sunder(item->data);

	if (fwrite(item->data, sizeof(char), length, hist) < length)
	    return FALSE;
	if (putc('\n', hist) == EOF)
	    return FALSE;
    }

    return TRUE;
}

/* Save the search and replace histories to ~/.nano/search_history. */
void save_history(void)
{
    char *searchhist;
    FILE *hist;

    /* If the histories are unchanged or empty, don't bother saving them. */
    if (!history_changed || (searchbot->lineno == 1 &&
		replacebot->lineno == 1 && executebot->lineno == 1))
	return;

    searchhist = histfilename();

    if (searchhist == NULL)
	return;

    hist = fopen(searchhist, "wb");

    if (hist == NULL)
	fprintf(stderr, _("Error writing %s: %s\n"), searchhist,
			strerror(errno));
    else {
	/* Don't allow others to read or write the history file. */
	chmod(searchhist, S_IRUSR | S_IWUSR);

	if (!writehist(hist, searchage) || !writehist(hist, replaceage) ||
				!writehist(hist, executetop))
	    fprintf(stderr, _("Error writing %s: %s\n"), searchhist,
			strerror(errno));

	fclose(hist);
    }

    free(searchhist);
}

/* Load the recorded file positions from ~/.nano/filepos_history. */
void load_poshistory(void)
{
    char *poshist = poshistfilename();
    FILE *hist;

    /* If the home directory is missing, do_rcfile() will have reported it. */
    if (poshist == NULL)
	return;

    hist = fopen(poshist, "rb");

    if (hist == NULL) {
	if (errno != ENOENT) {
	    /* When reading failed, don't save history when we quit. */
	    UNSET(POS_HISTORY);
	    history_error(N_("Error reading %s: %s"), poshist, strerror(errno));
	}
    } else {
	char *line = NULL, *lineptr, *xptr;
	size_t buf_len = 0;
	ssize_t read, count = 0;
	poshiststruct *record_ptr = NULL, *newrecord;

	/* Read and parse each line, and store the extracted data. */
	while ((read = getline(&line, &buf_len, hist)) > 5) {
	    /* Decode nulls as embedded newlines. */
	    unsunder(line, read);

	    /* Find where the x index and line number are in the line. */
	    xptr = revstrstr(line, " ", line + read - 3);
	    if (xptr == NULL)
		continue;
	    lineptr = revstrstr(line, " ", xptr - 2);
	    if (lineptr == NULL)
		continue;

	    /* Now separate the three elements of the line. */
	    *(xptr++) = '\0';
	    *(lineptr++) = '\0';

	    /* Create a new position record. */
	    newrecord = (poshiststruct *)nmalloc(sizeof(poshiststruct));
	    newrecord->filename = mallocstrcpy(NULL, line);
	    newrecord->lineno = atoi(lineptr);
	    newrecord->xno = atoi(xptr);
	    newrecord->next = NULL;

	    /* Add the record to the list. */
	    if (position_history == NULL)
		position_history = newrecord;
	    else
		record_ptr->next = newrecord;

	    record_ptr = newrecord;

	    /* Impose a limit, so the file will not grow indefinitely. */
	    if (++count > 200) {
		poshiststruct *drop_record = position_history;

		position_history = position_history->next;

		free(drop_record->filename);
		free(drop_record);
	    }
	}
	fclose(hist);
	free(line);
    }
    free(poshist);
}

/* Save the recorded last file positions to ~/.nano/filepos_history. */
void save_poshistory(void)
{
    char *poshist = poshistfilename();
    poshiststruct *posptr;
    FILE *hist;

    if (poshist == NULL)
	return;

    hist = fopen(poshist, "wb");

    if (hist == NULL)
	fprintf(stderr, _("Error writing %s: %s\n"), poshist, strerror(errno));
    else {
	/* Don't allow others to read or write the history file. */
	chmod(poshist, S_IRUSR | S_IWUSR);

	for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
	    char *path_and_place;
	    size_t length;

	    /* Assume 20 decimal positions each for line and column number,
	     * plus two spaces, plus the line feed, plus the null byte. */
	    path_and_place = charalloc(strlen(posptr->filename) + 44);
	    sprintf(path_and_place, "%s %ld %ld\n", posptr->filename,
			(long)posptr->lineno, (long)posptr->xno);
	    length = strlen(path_and_place);

	    /* Encode newlines in filenames as nulls. */
	    sunder(path_and_place);
	    /* Restore the terminating newline. */
	    path_and_place[length - 1] = '\n';

	    if (fwrite(path_and_place, sizeof(char), length, hist) < length)
		fprintf(stderr, _("Error writing %s: %s\n"),
					poshist, strerror(errno));
	    free(path_and_place);
	}
	fclose(hist);
    }
    free(poshist);
}

/* Update the recorded last file positions, given a filename, a line
 * and a column.  If no entry is found, add a new one at the end. */
void update_poshistory(char *filename, ssize_t lineno, ssize_t xpos)
{
    poshiststruct *posptr, *theone, *posprev = NULL;
    char *fullpath = get_full_path(filename);

    if (fullpath == NULL || fullpath[strlen(fullpath) - 1] == '/' || inhelp) {
	free(fullpath);
	return;
    }

    /* Look for a matching filename in the list. */
    for (posptr = position_history; posptr != NULL; posptr = posptr->next) {
	if (!strcmp(posptr->filename, fullpath))
	    break;
	posprev = posptr;
    }

    /* Don't record files that have the default cursor position. */
    if (lineno == 1 && xpos == 1) {
	if (posptr != NULL) {
	    if (posprev == NULL)
		position_history = posptr->next;
	    else
		posprev->next = posptr->next;
	    free(posptr->filename);
	    free(posptr);
	}
	free(fullpath);
	return;
    }

    theone = posptr;

    /* If we didn't find it, make a new node; otherwise, if we're
     * not at the end, move the matching one to the end. */
    if (theone == NULL) {
	theone = (poshiststruct *)nmalloc(sizeof(poshiststruct));
	theone->filename = mallocstrcpy(NULL, fullpath);
	if (position_history == NULL)
	    position_history = theone;
	else
	    posprev->next = theone;
    } else if (posptr->next != NULL) {
	if (posprev == NULL)
	    position_history = posptr->next;
	else
	    posprev->next = posptr->next;
	while (posptr->next != NULL)
	    posptr = posptr->next;
	posptr->next = theone;
    }

    /* Store the last cursor position. */
    theone->lineno = lineno;
    theone->xno = xpos;
    theone->next = NULL;

    free(fullpath);
}

/* Check whether the given file matches an existing entry in the recorded
 * last file positions.  If not, return FALSE.  If yes, return TRUE and
 * set line and column to the retrieved values. */
bool has_old_position(const char *file, ssize_t *line, ssize_t *column)
{
    poshiststruct *posptr = position_history;
    char *fullpath = get_full_path(file);

    if (fullpath == NULL)
	return FALSE;

    while (posptr != NULL && strcmp(posptr->filename, fullpath) != 0)
	posptr = posptr->next;

    free(fullpath);

    if (posptr == NULL)
	return FALSE;

    *line = posptr->lineno;
    *column = posptr->xno;
    return TRUE;
}
#endif /* !DISABLE_HISTORIES */