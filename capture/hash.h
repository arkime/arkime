/******************************************************************************/
/* hash.h  -- Hashtable
 *
 * Simple macro based hashtable using DLL buckets with counts that supports 
 * having a single item in multiple lists or hash tables by using a prefix.  
 * Every element in the hash table needs to follow the DLL rules.
 *
 * To Use:
 * Create item structure and optional head structure for use with a DLL
 * Create the key function and cmp function.
 * Use HASH_VAR to declare the actual variable.  Can be global or in a structure
 * Use HASH_INIT to initialze the hashtable
 *
 * A key can also just be the element
 *
 * The same WARNING in dll.h applies since hash.h just uses DLL
 */

#ifndef _HASH_HEADER
#define _HASH_HEADER

/* Convert a key to a u32 hash value */
typedef uint32_t (* HASH_KEY_FUNC)(const void *key);

/* Given a key does it match the element */
typedef int (* HASH_CMP_FUNC)(const void *key, const void *element);

/* buckets is last on purpose so functions can be written */
#define HASH_VAR(name, varname, elementtype, num) \
   struct \
   { \
       HASH_KEY_FUNC hash; \
       HASH_CMP_FUNC cmp; \
       int size; \
       int count; \
       elementtype buckets[num]; \
   } varname

#define HASH_INIT(name, varname, hashfunc, cmpfunc) \
  do { \
       int i; \
       (varname).size = sizeof((varname).buckets)/sizeof((varname).buckets[0]); \
       (varname).hash = hashfunc; \
       (varname).cmp = cmpfunc; \
       (varname).count = 0; \
       for (i = 0; i < (varname).size; i++) { \
           DLL_INIT(name, &((varname).buckets[i])); \
       } \
     } while (0)

// Allocated buckets
#define HASHP_VAR(name, varname, elementtype) \
   struct \
   { \
       HASH_KEY_FUNC hash; \
       HASH_CMP_FUNC cmp; \
       int size; \
       int count; \
       elementtype *buckets; \
   } varname

#define HASHP_INIT(name, varname, sz, hashfunc, cmpfunc) \
  do { \
       int i; \
       (varname).size = sz; \
       (varname).hash = hashfunc; \
       (varname).cmp = cmpfunc; \
       (varname).count = 0; \
       (varname).buckets = malloc(sz * sizeof((varname).buckets[0])); \
       for (i = 0; i < (varname).size; i++) { \
           DLL_INIT(name, &((varname).buckets[i])); \
       } \
     } while (0)

#define HASH_HASH(varname, key) (varname).hash(key)


#define HASH_ADD_HASH(name, varname, h, key, element) \
  do { \
      const uint32_t _hh = element->name##hash = h; \
      const int _b = element->name##bucket = element->name##hash % (varname).size; \
      const void *_end = (void*)&((varname).buckets[_b]); \
      for (element->name##next = (varname).buckets[_b].name##next; element->name##next != _end; element->name##next = element->name##next->name##next) { \
          if (_hh > element->name##next->name##hash) break; \
      }\
     element->name##prev             = element->name##next->name##prev; \
     element->name##prev->name##next = element; \
     element->name##next->name##prev = element; \
     (varname).buckets[_b].name##count++;\
     (varname).count++; \
  } while(0)

#define HASH_ADD(name, varname, key, element) HASH_ADD_HASH(name, varname, HASH_HASH(varname, key), key, element)

#define HASH_REMOVE(name, varname, element) \
  do { \
      DLL_REMOVE(name, &((varname).buckets[element->name##bucket]), element); \
      (varname).count--; \
  } while(0)

#define HASH_FIND_HASH(name, varname, h, key, element) \
  do { \
      const uint32_t _hh = h; \
      const int _b = _hh % (varname).size; \
      const void *_end = (void*)&((varname).buckets[_b]); \
      for (element = (varname).buckets[_b].name##next; element != _end; element = element->name##next) { \
          if (_hh == element->name##hash && (varname).cmp(key, element)) break; \
          if (_hh > element->name##hash) {element = 0; break;} \
      } \
      if (element == _end) element = 0; \
  } while(0)

#define HASH_FIND_INT(name, varname, key, element) HASH_FIND_HASH(name, varname, (uint32_t)key, (void*)(long)key, element)

#define HASH_FIND(name, varname, key, element) HASH_FIND_HASH(name, varname, HASH_HASH(varname, key), key, element)

#define HASH_COUNT(name, varname) ((varname).count)

#define HASH_BUCKET_COUNT(name, varname, h) ((varname).buckets[((uint32_t)h) % (varname).size].name##count)

#define HASH_FORALL_POP_HEAD(name, varname, element, code) \
  do { \
      int  _##name##b; \
      for ( _##name##b = 0;  _##name##b < (varname).size;  _##name##b++) {\
          while((varname).buckets[_##name##b].name##count) { \
              DLL_POP_HEAD(name, &((varname).buckets[_##name##b]), element); \
              (varname).count--; \
              code \
          } \
      } \
  } while(0)

#define HASH_FORALL(name, varname, element, code) \
  do { \
      int  _##name##b; \
      for ( _##name##b = 0;  _##name##b < (varname).size;  _##name##b++) {\
          for (element = (varname).buckets[_##name##b].name##next; element != (void*)&((varname).buckets[_##name##b]); element = element->name##next) { \
              code \
          } \
      } \
  } while(0)

#endif
