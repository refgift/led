#pragma once

#include "editor.h"

/**
 * undo_store.h - keep the undo history of a file between sessions.
 *
 * The history is written when the file is saved and read back when the
 * file is next opened, so Ctrl+Z still works after quitting.
 *
 * Where: a directory of the user's own under the system's temporary
 * directory, "$TMPDIR/led-UID" or "/tmp/led-UID". The system clears it
 * (at boot, or daily), so old histories do not pile up. It is created
 * with mode 0700 and refused if it is a link, belongs to someone else,
 * or is open to others: the history holds deleted text.
 *
 * When it is used: only if the file on disk is byte for byte the file
 * the history was saved with. A file changed by anything else since
 * then gets no history, because undo steps name positions in the text.
 */

/** Write the history of filename. Call after the file itself is saved.
 *  Returns 0 on success, -1 if it could not be written (not an error
 *  for the user: the file is saved either way). */
int undo_store_save (const char *filename, const UndoStack *undo,
                     const UndoStack *redo);

/** Read the history of filename into two empty stacks.
 *  Returns 0 if a history was loaded, -1 if there is none that matches
 *  the file as it is now. The stacks are left empty then. */
int undo_store_load (const char *filename, UndoStack *undo, UndoStack *redo);

/** Use dir instead of the temporary directory (tests). NULL restores
 *  the default. The string is copied. */
void undo_store_set_dir (const char *dir);

/** The directory histories are kept in, written to out. Creates it if
 *  missing. Returns 0, or -1 if there is no directory that is safe to use. */
int undo_store_directory (char *out, size_t out_size);
