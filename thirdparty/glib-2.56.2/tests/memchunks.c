/* GLIB - Library of useful routines for C programming
 * Copyright (C) 1995-1997  Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

/*
 * Modified by the GLib Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GLib Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GLib at ftp://ftp.gtk.org/pub/gtk/. 
 */

/* 
 * MT safe
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include "glib.h"

/* notes on macros:
 * if ENABLE_GC_FRIENDLY is defined, freed memory should be 0-wiped.
 */

#define MEM_PROFILE_TABLE_SIZE 4096

#define MEM_AREA_SIZE 4L

static guint mem_chunk_recursion = 0;
#  define MEM_CHUNK_ROUTINE_COUNT()	(mem_chunk_recursion)
#  define ENTER_MEM_CHUNK_ROUTINE()	(mem_chunk_recursion = MEM_CHUNK_ROUTINE_COUNT () + 1)
#  define LEAVE_MEM_CHUNK_ROUTINE()	(mem_chunk_recursion = MEM_CHUNK_ROUTINE_COUNT () - 1)

/* --- old memchunk prototypes --- */
GMemChunk*      old_mem_chunk_new       (const gchar  *name,
                                         gint          atom_size,
                                         gulong        area_size,
                                         gint          type);
void            old_mem_chunk_destroy   (GMemChunk *mem_chunk);
gpointer        old_mem_chunk_alloc     (GMemChunk *mem_chunk);
gpointer        old_mem_chunk_alloc0    (GMemChunk *mem_chunk);
void            old_mem_chunk_free      (GMemChunk *mem_chunk,
                                         gpointer   mem);
void            old_mem_chunk_clean     (GMemChunk *mem_chunk);
void            old_mem_chunk_reset     (GMemChunk *mem_chunk);
void            old_mem_chunk_print     (GMemChunk *mem_chunk);
void            old_mem_chunk_info      (void);


/* --- MemChunks --- */
#ifndef G_ALLOC_AND_FREE
typedef struct _GAllocator GAllocator;
typedef struct _GMemChunk  GMemChunk;
#define G_ALLOC_ONLY	  1
#define G_ALLOC_AND_FREE  2
#endif

typedef struct _GFreeAtom      GFreeAtom;
typedef struct _GMemArea       GMemArea;

struct _GFreeAtom
{
  GFreeAtom *next;
};

struct _GMemArea
{
  GMemArea *next;            /* the next mem area */
  GMemArea *prev;            /* the previous mem area */
  gulong index;              /* the current index into the "mem" array */
  gulong free;               /* the number of free bytes in this mem area */
  gulong allocated;          /* the number of atoms allocated from this area */
  gulong mark;               /* is this mem area marked for deletion */
  gchar mem[MEM_AREA_SIZE];  /* the mem array from which atoms get allocated
			      * the actual size of this array is determined by
			      *  the mem chunk "area_size". ANSI says that it
			      *  must be declared to be the maximum size it
			      *  can possibly be (even though the actual size
			      *  may be less).
			      */
};

struct _GMemChunk
{
  const gchar *name;         /* name of this MemChunk...used for debugging output */
  gint type;                 /* the type of MemChunk: ALLOC_ONLY or ALLOC_AND_FREE */
  gint num_mem_areas;        /* the number of memory areas */
  gint num_marked_areas;     /* the number of areas marked for deletion */
  guint atom_size;           /* the size of an atom */
  gulong area_size;          /* the size of a memory area */
  GMemArea *mem_area;        /* the current memory area */
  GMemArea *mem_areas;       /* a list of all the mem areas owned by this chunk */
  GMemArea *free_mem_area;   /* the free area...which is about to be destroyed */
  GFreeAtom *free_atoms;     /* the free atoms list */
  GTree *mem_tree;           /* tree of mem areas sorted by memory address */
  GMemChunk *next;           /* pointer to the next chunk */
  GMemChunk *prev;           /* pointer to the previous chunk */
};


static gulong old_mem_chunk_compute_size (gulong    size,
                                          gulong    min_size) G_GNUC_CONST;
static gint   old_mem_chunk_area_compare (GMemArea *a,
                                          GMemArea *b);
static gint   old_mem_chunk_area_search  (GMemArea *a,
                                          gchar    *addr);

/* here we can't use StaticMutexes, as they depend upon a working
 * g_malloc, the same holds true for StaticPrivate
 */
static GMutex         mem_chunks_lock;
static GMemChunk     *mem_chunks = NULL;

GMemChunk*
old_mem_chunk_new (const gchar  *name,
                   gint          atom_size,
                   gulong        area_size,
                   gint          type)
{
  GMemChunk *mem_chunk;
  gulong rarea_size;
  
  g_return_val_if_fail (atom_size > 0, NULL);
  g_return_val_if_fail (area_size >= atom_size, NULL);
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
  area_size = (area_size + atom_size - 1) / atom_size;
  area_size *= atom_size;
  
  mem_chunk = g_new (GMemChunk, 1);
  mem_chunk->name = name;
  mem_chunk->type = type;
  mem_chunk->num_mem_areas = 0;
  mem_chunk->num_marked_areas = 0;
  mem_chunk->mem_area = NULL;
  mem_chunk->free_mem_area = NULL;
  mem_chunk->free_atoms = NULL;
  mem_chunk->mem_tree = NULL;
  mem_chunk->mem_areas = NULL;
  mem_chunk->atom_size = atom_size;
  
  if (mem_chunk->type == G_ALLOC_AND_FREE)
    mem_chunk->mem_tree = g_tree_new ((GCompareFunc) old_mem_chunk_area_compare);
  
  if (mem_chunk->atom_size % G_MEM_ALIGN)
    mem_chunk->atom_size += G_MEM_ALIGN - (mem_chunk->atom_size % G_MEM_ALIGN);
  
  rarea_size = area_size + sizeof (GMemArea) - MEM_AREA_SIZE;
  rarea_size = old_mem_chunk_compute_size (rarea_size, atom_size + sizeof (GMemArea) - MEM_AREA_SIZE);
  mem_chunk->area_size = rarea_size - (sizeof (GMemArea) - MEM_AREA_SIZE);
  
  g_mutex_lock (&mem_chunks_lock);
  mem_chunk->next = mem_chunks;
  mem_chunk->prev = NULL;
  if (mem_chunks)
    mem_chunks->prev = mem_chunk;
  mem_chunks = mem_chunk;
  g_mutex_unlock (&mem_chunks_lock);
  
  LEAVE_MEM_CHUNK_ROUTINE ();
  
  return mem_chunk;
}

void
old_mem_chunk_destroy (GMemChunk *mem_chunk)
{
  GMemArea *mem_areas;
  GMemArea *temp_area;
  
  g_return_if_fail (mem_chunk != NULL);
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
  mem_areas = mem_chunk->mem_areas;
  while (mem_areas)
    {
      temp_area = mem_areas;
      mem_areas = mem_areas->next;
      g_free (temp_area);
    }
  
  g_mutex_lock (&mem_chunks_lock);
  if (mem_chunk->next)
    mem_chunk->next->prev = mem_chunk->prev;
  if (mem_chunk->prev)
    mem_chunk->prev->next = mem_chunk->next;
  
  if (mem_chunk == mem_chunks)
    mem_chunks = mem_chunks->next;
  g_mutex_unlock (&mem_chunks_lock);
  
  if (mem_chunk->type == G_ALLOC_AND_FREE)
    g_tree_destroy (mem_chunk->mem_tree);  
  
  g_free (mem_chunk);
  
  LEAVE_MEM_CHUNK_ROUTINE ();
}

gpointer
old_mem_chunk_alloc (GMemChunk *mem_chunk)
{
  GMemArea *temp_area;
  gpointer mem;
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
  g_return_val_if_fail (mem_chunk != NULL, NULL);
  
  while (mem_chunk->free_atoms)
    {
      /* Get the first piece of memory on the "free_atoms" list.
       * We can go ahead and destroy the list node we used to keep
       *  track of it with and to update the "free_atoms" list to
       *  point to its next element.
       */
      mem = mem_chunk->free_atoms;
      mem_chunk->free_atoms = mem_chunk->free_atoms->next;
      
      /* Determine which area this piece of memory is allocated from */
      temp_area = g_tree_search (mem_chunk->mem_tree,
				 (GCompareFunc) old_mem_chunk_area_search,
				 mem);
      
      /* If the area has been marked, then it is being destroyed.
       *  (ie marked to be destroyed).
       * We check to see if all of the segments on the free list that
       *  reference this area have been removed. This occurs when
       *  the amount of free memory is less than the allocatable size.
       * If the chunk should be freed, then we place it in the "free_mem_area".
       * This is so we make sure not to free the mem area here and then
       *  allocate it again a few lines down.
       * If we don't allocate a chunk a few lines down then the "free_mem_area"
       *  will be freed.
       * If there is already a "free_mem_area" then we'll just free this mem area.
       */
      if (temp_area->mark)
        {
          /* Update the "free" memory available in that area */
          temp_area->free += mem_chunk->atom_size;
	  
          if (temp_area->free == mem_chunk->area_size)
            {
              if (temp_area == mem_chunk->mem_area)
                mem_chunk->mem_area = NULL;
	      
              if (mem_chunk->free_mem_area)
                {
                  mem_chunk->num_mem_areas -= 1;
		  
                  if (temp_area->next)
                    temp_area->next->prev = temp_area->prev;
                  if (temp_area->prev)
                    temp_area->prev->next = temp_area->next;
                  if (temp_area == mem_chunk->mem_areas)
                    mem_chunk->mem_areas = mem_chunk->mem_areas->next;
		  
		  if (mem_chunk->type == G_ALLOC_AND_FREE)
		    g_tree_remove (mem_chunk->mem_tree, temp_area);
                  g_free (temp_area);
                }
              else
                mem_chunk->free_mem_area = temp_area;
	      
	      mem_chunk->num_marked_areas -= 1;
	    }
	}
      else
        {
          /* Update the number of allocated atoms count.
	   */
          temp_area->allocated += 1;
	  
          /* The area wasn't marked...return the memory
	   */
	  goto outa_here;
        }
    }
  
  /* If there isn't a current mem area or the current mem area is out of space
   *  then allocate a new mem area. We'll first check and see if we can use
   *  the "free_mem_area". Otherwise we'll just malloc the mem area.
   */
  if ((!mem_chunk->mem_area) ||
      ((mem_chunk->mem_area->index + mem_chunk->atom_size) > mem_chunk->area_size))
    {
      if (mem_chunk->free_mem_area)
        {
          mem_chunk->mem_area = mem_chunk->free_mem_area;
	  mem_chunk->free_mem_area = NULL;
        }
      else
        {
#ifdef ENABLE_GC_FRIENDLY
	  mem_chunk->mem_area = (GMemArea*) g_malloc0 (sizeof (GMemArea) -
						       MEM_AREA_SIZE +
						       mem_chunk->area_size); 
#else /* !ENABLE_GC_FRIENDLY */
	  mem_chunk->mem_area = (GMemArea*) g_malloc (sizeof (GMemArea) -
						      MEM_AREA_SIZE +
						      mem_chunk->area_size);
#endif /* ENABLE_GC_FRIENDLY */
	  
	  mem_chunk->num_mem_areas += 1;
	  mem_chunk->mem_area->next = mem_chunk->mem_areas;
	  mem_chunk->mem_area->prev = NULL;
	  
	  if (mem_chunk->mem_areas)
	    mem_chunk->mem_areas->prev = mem_chunk->mem_area;
	  mem_chunk->mem_areas = mem_chunk->mem_area;
	  
	  if (mem_chunk->type == G_ALLOC_AND_FREE)
	    g_tree_insert (mem_chunk->mem_tree, mem_chunk->mem_area, mem_chunk->mem_area);
        }
      
      mem_chunk->mem_area->index = 0;
      mem_chunk->mem_area->free = mem_chunk->area_size;
      mem_chunk->mem_area->allocated = 0;
      mem_chunk->mem_area->mark = 0;
    }
  
  /* Get the memory and modify the state variables appropriately.
   */
  mem = (gpointer) &mem_chunk->mem_area->mem[mem_chunk->mem_area->index];
  mem_chunk->mem_area->index += mem_chunk->atom_size;
  mem_chunk->mem_area->free -= mem_chunk->atom_size;
  mem_chunk->mem_area->allocated += 1;
  
 outa_here:
  
  LEAVE_MEM_CHUNK_ROUTINE ();
  
  return mem;
}

gpointer
old_mem_chunk_alloc0 (GMemChunk *mem_chunk)
{
  gpointer mem;
  
  mem = old_mem_chunk_alloc (mem_chunk);
  if (mem)
    {
      memset (mem, 0, mem_chunk->atom_size);
    }
  
  return mem;
}

void
old_mem_chunk_free (GMemChunk *mem_chunk,
                    gpointer   mem)
{
  GMemArea *temp_area;
  GFreeAtom *free_atom;
  
  g_return_if_fail (mem_chunk != NULL);
  g_return_if_fail (mem != NULL);
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
#ifdef ENABLE_GC_FRIENDLY
  memset (mem, 0, mem_chunk->atom_size);
#endif /* ENABLE_GC_FRIENDLY */
  
  /* Don't do anything if this is an ALLOC_ONLY chunk
   */
  if (mem_chunk->type == G_ALLOC_AND_FREE)
    {
      /* Place the memory on the "free_atoms" list
       */
      free_atom = (GFreeAtom*) mem;
      free_atom->next = mem_chunk->free_atoms;
      mem_chunk->free_atoms = free_atom;
      
      temp_area = g_tree_search (mem_chunk->mem_tree,
				 (GCompareFunc) old_mem_chunk_area_search,
				 mem);
      
      temp_area->allocated -= 1;
      
      if (temp_area->allocated == 0)
	{
	  temp_area->mark = 1;
	  mem_chunk->num_marked_areas += 1;
	}
    }
  
  LEAVE_MEM_CHUNK_ROUTINE ();
}

/* This doesn't free the free_area if there is one */
void
old_mem_chunk_clean (GMemChunk *mem_chunk)
{
  GMemArea *mem_area;
  GFreeAtom *prev_free_atom;
  GFreeAtom *temp_free_atom;
  gpointer mem;
  
  g_return_if_fail (mem_chunk != NULL);
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
  if (mem_chunk->type == G_ALLOC_AND_FREE)
    {
      prev_free_atom = NULL;
      temp_free_atom = mem_chunk->free_atoms;
      
      while (temp_free_atom)
	{
	  mem = (gpointer) temp_free_atom;
	  
	  mem_area = g_tree_search (mem_chunk->mem_tree,
				    (GCompareFunc) old_mem_chunk_area_search,
				    mem);
	  
          /* If this mem area is marked for destruction then delete the
	   *  area and list node and decrement the free mem.
           */
	  if (mem_area->mark)
	    {
	      if (prev_free_atom)
		prev_free_atom->next = temp_free_atom->next;
	      else
		mem_chunk->free_atoms = temp_free_atom->next;
	      temp_free_atom = temp_free_atom->next;
	      
	      mem_area->free += mem_chunk->atom_size;
	      if (mem_area->free == mem_chunk->area_size)
		{
		  mem_chunk->num_mem_areas -= 1;
		  mem_chunk->num_marked_areas -= 1;
		  
		  if (mem_area->next)
		    mem_area->next->prev = mem_area->prev;
		  if (mem_area->prev)
		    mem_area->prev->next = mem_area->next;
		  if (mem_area == mem_chunk->mem_areas)
		    mem_chunk->mem_areas = mem_chunk->mem_areas->next;
		  if (mem_area == mem_chunk->mem_area)
		    mem_chunk->mem_area = NULL;
		  
		  if (mem_chunk->type == G_ALLOC_AND_FREE)
		    g_tree_remove (mem_chunk->mem_tree, mem_area);
		  g_free (mem_area);
		}
	    }
	  else
	    {
	      prev_free_atom = temp_free_atom;
	      temp_free_atom = temp_free_atom->next;
	    }
	}
    }
  LEAVE_MEM_CHUNK_ROUTINE ();
}

void
old_mem_chunk_reset (GMemChunk *mem_chunk)
{
  GMemArea *mem_areas;
  GMemArea *temp_area;
  
  g_return_if_fail (mem_chunk != NULL);
  
  ENTER_MEM_CHUNK_ROUTINE ();
  
  mem_areas = mem_chunk->mem_areas;
  mem_chunk->num_mem_areas = 0;
  mem_chunk->mem_areas = NULL;
  mem_chunk->mem_area = NULL;
  
  while (mem_areas)
    {
      temp_area = mem_areas;
      mem_areas = mem_areas->next;
      g_free (temp_area);
    }
  
  mem_chunk->free_atoms = NULL;
  
  if (mem_chunk->mem_tree)
    {
      g_tree_destroy (mem_chunk->mem_tree);
      mem_chunk->mem_tree = g_tree_new ((GCompareFunc) old_mem_chunk_area_compare);
    }
  
  LEAVE_MEM_CHUNK_ROUTINE ();
}

void
old_mem_chunk_print (GMemChunk *mem_chunk)
{
  GMemArea *mem_areas;
  gulong mem;
  
  g_return_if_fail (mem_chunk != NULL);
  
  mem_areas = mem_chunk->mem_areas;
  mem = 0;
  
  while (mem_areas)
    {
      mem += mem_chunk->area_size - mem_areas->free;
      mem_areas = mem_areas->next;
    }
  
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO,
	 "%s: %ld bytes using %d mem areas",
	 mem_chunk->name, mem, mem_chunk->num_mem_areas);
}

void
old_mem_chunk_info (void)
{
  GMemChunk *mem_chunk;
  gint count;
  
  count = 0;
  g_mutex_lock (&mem_chunks_lock);
  mem_chunk = mem_chunks;
  while (mem_chunk)
    {
      count += 1;
      mem_chunk = mem_chunk->next;
    }
  g_mutex_unlock (&mem_chunks_lock);
  
  g_log (G_LOG_DOMAIN, G_LOG_LEVEL_INFO, "%d mem chunks", count);
  
  g_mutex_lock (&mem_chunks_lock);
  mem_chunk = mem_chunks;
  g_mutex_unlock (&mem_chunks_lock);
  
  while (mem_chunk)
    {
      old_mem_chunk_print ((GMemChunk*) mem_chunk);
      mem_chunk = mem_chunk->next;
    }  
}

static gulong
old_mem_chunk_compute_size (gulong size,
                            gulong min_size)
{
  gulong power_of_2;
  gulong lower, upper;
  
  power_of_2 = 16;
  while (power_of_2 < size)
    power_of_2 <<= 1;
  
  lower = power_of_2 >> 1;
  upper = power_of_2;
  
  if (size - lower < upper - size && lower >= min_size)
    return lower;
  else
    return upper;
}

static gint
old_mem_chunk_area_compare (GMemArea *a,
                            GMemArea *b)
{
  if (a->mem > b->mem)
    return 1;
  else if (a->mem < b->mem)
    return -1;
  return 0;
}

static gint
old_mem_chunk_area_search (GMemArea *a,
                           gchar    *addr)
{
  if (a->mem <= addr)
    {
      if (addr < &a->mem[a->index])
	return 0;
      return 1;
    }
  return -1;
}
