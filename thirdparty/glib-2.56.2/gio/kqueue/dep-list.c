/*******************************************************************************
  Copyright (c) 2011, 2012 Dmitry Matveev <me@dmitrymatveev.co.uk>

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*******************************************************************************/

#include <glib.h>

#include <stdlib.h>  /* calloc */
#include <stdio.h>   /* printf */
#include <dirent.h>  /* opendir, readdir, closedir */
#include <string.h>  /* strcmp */
#include <assert.h>

#include "dep-list.h"

static gboolean kdl_debug_enabled = FALSE;
#define perror_msg if (kdl_debug_enabled) g_warning


/**
 * Print a list to stdout.
 *
 * @param[in] dl A pointer to a list.
 **/
void
dl_print (const dep_list *dl)
{
    while (dl != NULL) {
        printf ("%lld:%s ", (long long int) dl->inode, dl->path);
        dl = dl->next;
    }
    printf ("\n");
}

/**
 * Create a new list item.
 *
 * Create a new list item and initialize its fields.
 *
 * @param[in] path  A name of a file (the string is not copied!).
 * @param[in] inode A file's inode number.
 * @return A pointer to a new item or NULL in the case of error.
 **/
dep_list* dl_create (char *path, ino_t inode)
{
    dep_list *dl = calloc (1, sizeof (dep_list));
    if (dl == NULL) {
        perror_msg ("Failed to create a new dep-list item");
        return NULL;
    }

    dl->path = path;
    dl->inode = inode;
    return dl;
}

/**
 * Create a shallow copy of a list.
 *
 * A shallow copy is a copy of a structure, but not the copy of the
 * contents. All data pointers ('path' in our case) of a list and its
 * shallow copy will point to the same memory.
 *
 * @param[in] dl A pointer to list to make a copy. May be NULL.
 * @return A shallow copy of the list.
 **/ 
dep_list*
dl_shallow_copy (const dep_list *dl)
{
    if (dl == NULL) {
        return NULL;
    }

    dep_list *head = calloc (1, sizeof (dep_list));
    if (head == NULL) {
        perror_msg ("Failed to allocate head during shallow copy");
        return NULL;
    }

    dep_list *cp = head;
    const dep_list *it = dl;

    while (it != NULL) {
        cp->path = it->path;
        cp->inode = it->inode;
        if (it->next) {
            cp->next = calloc (1, sizeof (dep_list));
            if (cp->next == NULL) {
                perror_msg ("Failed to allocate a new element during shallow copy");
                dl_shallow_free (head);
                return NULL;
            }
            cp = cp->next;
        }
        it = it->next;
    }

    return head;
}

/**
 * Free the memory allocated for shallow copy.
 *
 * This function will free the memory used by a list structure, but
 * the list data will remain in the heap.
 *
 * @param[in] dl A pointer to a list. May be NULL.
 **/
void
dl_shallow_free (dep_list *dl)
{
    while (dl != NULL) {
        dep_list *ptr = dl;
        dl = dl->next;
        free (ptr);
    }
}

/**
 * Free the memory allocated for a list.
 *
 * This function will free all the memory used by a list: both
 * list structure and the list data.
 *
 * @param[in] dl A pointer to a list. May be NULL.
 **/
void
dl_free (dep_list *dl)
{
    while (dl != NULL) {
        dep_list *ptr = dl;
        dl = dl->next;

        free (ptr->path);
        free (ptr);
    }
}

/**
 * Create a directory listing and return it as a list.
 *
 * @param[in] path A path to a directory.
 * @return A pointer to a list. May return NULL, check errno in this case.
 **/
dep_list*
dl_listing (const char *path)
{
    assert (path != NULL);

    dep_list *head = NULL;
    dep_list *prev = NULL;
    DIR *dir = opendir (path);
    if (dir != NULL) {
        struct dirent *ent;

        while ((ent = readdir (dir)) != NULL) {
            if (!strcmp (ent->d_name, ".") || !strcmp (ent->d_name, "..")) {
                continue;
            }

            if (head == NULL) {
                head = calloc (1, sizeof (dep_list));
                if (head == NULL) {
                    perror_msg ("Failed to allocate head during listing");
                    goto error;
                }
            }

            dep_list *iter = (prev == NULL) ? head : calloc (1, sizeof (dep_list));
            if (iter == NULL) {
                perror_msg ("Failed to allocate a new element during listing");
                goto error;
            }

            iter->path = strdup (ent->d_name);
            if (iter->path == NULL) {
                perror_msg ("Failed to copy a string during listing");
                goto error;
            }

            iter->inode = ent->d_ino;
            iter->next = NULL;
            if (prev) {
                prev->next = iter;
            }
            prev = iter;
        }

        closedir (dir);
    }
    return head;

error:
    if (dir != NULL) {
        closedir (dir);
    }
    dl_free (head);
    return NULL;
}

/**
 * Perform a diff on lists.
 *
 * This function performs something like a set intersection. The same items
 * will be removed from the both lists. Items are comapred by a filename.
 * 
 * @param[in,out] before A pointer to a pointer to a list. Will contain items
 *     which were not found in the 'after' list.
 * @param[in,out] after  A pointer to a pointer to a list. Will containt items
 *     which were not found in the 'before' list.
 **/
void
dl_diff (dep_list **before, dep_list **after)
{
    assert (before != NULL);
    assert (after != NULL);

    if (*before == NULL || *after == NULL) {
        return;
    }

    dep_list *before_iter = *before;
    dep_list *before_prev = NULL;

    while (before_iter != NULL) {
        dep_list *after_iter = *after;
        dep_list *after_prev = NULL;

        int matched = 0;
        while (after_iter != NULL) {
            if (strcmp (before_iter->path, after_iter->path) == 0) {
                matched = 1;
                /* removing the entry from the both lists */
                if (before_prev) {
                    before_prev->next = before_iter->next;
                } else {
                    *before = before_iter->next;
                }

                if (after_prev) {
                    after_prev->next = after_iter->next;
                } else {
                    *after = after_iter->next;
                }
                free (after_iter);
                break;
            }
            after_prev = after_iter;
            after_iter = after_iter->next;
        }

        dep_list *oldptr = before_iter;
        before_iter = before_iter->next;
        if (matched == 0) {
            before_prev = oldptr;
        } else {
            free (oldptr);
        }
    }
}


/**
 * Traverses two lists. Compares items with a supplied expression
 * and performs the passed code on a match. Removes the matched entries
 * from the both lists.
 **/
#define EXCLUDE_SIMILAR(removed_list, added_list, match_expr, matched_code) \
    assert (removed_list != NULL);                                      \
    assert (added_list != NULL);                                        \
                                                                        \
    dep_list *removed_list##_iter = *removed_list;                      \
    dep_list *removed_list##_prev = NULL;                               \
                                                                        \
    int productive = 0;                                                 \
                                                                        \
    while (removed_list##_iter != NULL) {                               \
        dep_list *added_list##_iter = *added_list;                      \
        dep_list *added_list##_prev = NULL;                             \
                                                                        \
        int matched = 0;                                                \
        while (added_list##_iter != NULL) {                             \
            if (match_expr) {                                           \
                matched = 1;                                            \
                ++productive;                                           \
                matched_code;                                           \
                                                                        \
                if (removed_list##_prev) {                              \
                    removed_list##_prev->next = removed_list##_iter->next; \
                } else {                                                \
                    *removed_list = removed_list##_iter->next;          \
                }                                                       \
                if (added_list##_prev) {                                \
                    added_list##_prev->next = added_list##_iter->next;  \
                } else {                                                \
                    *added_list = added_list##_iter->next;              \
                }                                                       \
                free (added_list##_iter);                               \
                break;                                                  \
            }                                                           \
            added_list##_iter = added_list##_iter->next;                \
        }                                                               \
        dep_list *oldptr = removed_list##_iter;                         \
        removed_list##_iter = removed_list##_iter->next;                \
        if (matched == 0) {                                             \
            removed_list##_prev = oldptr;                               \
        } else {                                                        \
            free (oldptr);                                              \
        }                                                               \
    }                                                                   \
    return (productive > 0);


#define cb_invoke(cbs, name, udata, ...) \
    do { \
        if (cbs->name) { \
            (cbs->name) (udata, ## __VA_ARGS__); \
        } \
    } while (0)

/**
 * Detect and notify about moves in the watched directory.
 *
 * A move is what happens when you rename a file in a directory, and
 * a new name is unique, i.e. you didnt overwrite any existing files
 * with this one.
 *
 * @param[in,out] removed  A list of the removed files in the directory.
 * @param[in,out] added    A list of the added files of the directory.
 * @param[in]     cbs      A pointer to #traverse_cbs, an user-defined set of 
 *     traverse callbacks.
 * @param[in]     udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
**/
static int
dl_detect_moves (dep_list           **removed, 
                 dep_list           **added, 
                 const traverse_cbs  *cbs, 
                 void                *udata)
{
    assert (cbs != NULL);

     EXCLUDE_SIMILAR
        (removed, added,
         (removed_iter->inode == added_iter->inode),
         {
             cb_invoke (cbs, moved, udata,
                        removed_iter->path, removed_iter->inode,
                        added_iter->path, added_iter->inode);
         });
}

/**
 * Detect and notify about replacements in the watched directory.
 *
 * Consider you are watching a directory foo with the folloing files
 * insinde:
 *
 *    foo/bar
 *    foo/baz
 *
 * A replacement in a watched directory is what happens when you invoke
 *
 *    mv /foo/bar /foo/bar
 *
 * i.e. when you replace a file in a watched directory with another file
 * from the same directory.
 *
 * @param[in,out] removed  A list of the removed files in the directory.
 * @param[in,out] current  A list with the current contents of the directory.
 * @param[in]     cbs      A pointer to #traverse_cbs, an user-defined set of 
 *     traverse callbacks.
 * @param[in]     udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
 **/
static int
dl_detect_replacements (dep_list           **removed,
                        dep_list           **current,
                        const traverse_cbs  *cbs,
                        void                *udata)
{
    assert (cbs != NULL);

    EXCLUDE_SIMILAR
        (removed, current,
         (removed_iter->inode == current_iter->inode),
         {
            cb_invoke (cbs, replaced, udata,
                        removed_iter->path, removed_iter->inode,
                        current_iter->path, current_iter->inode);
         });
}

/**
 * Detect and notify about overwrites in the watched directory.
 *
 * Consider you are watching a directory foo with a file inside:
 *
 *    foo/bar
 *
 * And you also have a directory tmp with a file 1:
 * 
 *    tmp/1
 *
 * You do not watching directory tmp.
 *
 * An overwrite in a watched directory is what happens when you invoke
 *
 *    mv /tmp/1 /foo/bar
 *
 * i.e. when you overwrite a file in a watched directory with another file
 * from the another directory.
 *
 * @param[in,out] previous A list with the previous contents of the directory.
 * @param[in,out] current  A list with the current contents of the directory.
 * @param[in]     cbs      A pointer to #traverse_cbs, an user-defined set of 
 *     traverse callbacks.
 * @param[in]     udata    A pointer to the user-defined data.
 * @return 0 if no files were renamed, >0 otherwise.
 **/
static int
dl_detect_overwrites (dep_list           **previous,
                      dep_list           **current,
                      const traverse_cbs  *cbs,
                      void                *udata)
{
    assert (cbs != NULL);

    EXCLUDE_SIMILAR
        (previous, current,
         (strcmp (previous_iter->path, current_iter->path) == 0
          && previous_iter->inode != current_iter->inode),
         {
             cb_invoke (cbs, overwritten, udata, current_iter->path, current_iter->inode);
         });
}


/**
 * Traverse a list and invoke a callback for each item.
 * 
 * @param[in] list  A #dep_list.
 * @param[in] cb    A #single_entry_cb callback function.
 * @param[in] udata A pointer to the user-defined data.
 **/
static void 
dl_emit_single_cb_on (dep_list        *list,
                      single_entry_cb  cb,
                      void            *udata)
{
    while (cb && list != NULL) {
        (cb) (udata, list->path, list->inode);
        list = list->next;
    }
}


/**
 * Recognize all the changes in the directory, invoke the appropriate callbacks.
 *
 * This is the core function of directory diffing submodule.
 *
 * @param[in] before The previous contents of the directory.
 * @param[in] after  The current contents of the directory.
 * @param[in] cbs    A pointer to user callbacks (#traverse_callbacks).
 * @param[in] udata  A pointer to user data.
 **/
void
dl_calculate (dep_list           *before,
              dep_list           *after,
              const traverse_cbs *cbs,
              void               *udata)
{
    assert (cbs != NULL);

    int need_update = 0;

    dep_list *was = dl_shallow_copy (before);
    dep_list *pre = dl_shallow_copy (before);
    dep_list *now = dl_shallow_copy (after);
    dep_list *lst = dl_shallow_copy (after);

    dl_diff (&was, &now); 

    need_update += dl_detect_moves (&was, &now, cbs, udata);
    need_update += dl_detect_replacements (&was, &lst, cbs, udata);
    dl_detect_overwrites (&pre, &lst, cbs, udata);
 
    if (need_update) {
        cb_invoke (cbs, names_updated, udata);
    }

    dl_emit_single_cb_on (was, cbs->removed, udata);
    dl_emit_single_cb_on (now, cbs->added, udata);

    cb_invoke (cbs, many_added, udata, now);
    cb_invoke (cbs, many_removed, udata, was);
    
    dl_shallow_free (lst);
    dl_shallow_free (now);
    dl_shallow_free (pre);
    dl_shallow_free (was);
}

