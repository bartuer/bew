#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include "cbt.h"

#define POINTER_SIZE 4
int
cbt_contains(cbt_tree *t, const char *u) {
  const uint8 *ubytes = (void *) u;
  const size_t ulen = strlen(u);
  uint8 *p = t->root;

  if (!p) return 0;                                                        /* test for empty tree */

  while (1 & (intptr_t) p) {                                               /* walk tree for best member */
    cbt_node *q = (void *) (p - 1);
    uint8 c = 0;                                                           /* calculate direction */
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
  }
  return 0 == strcmp(u, (const char *) p);
}

int
cbt_insert(cbt_tree *t, const char *u, void* value) {
  const uint8 *const ubytes = (void *) u;                                  /* insert into a empty tree */
  const size_t ulen = strlen(u);
  uint8 *p = t->root;

  if (!p) {
    char *x;
    int a = posix_memalign((void **) &x, sizeof(void *), ulen + 1 + sizeof(void*));
    if (a) {
      return 0;
    }
    memcpy(x, u, ulen + 1);
    if ( value != NULL ) {
      memset(x + ulen + 1, 0, sizeof(void *));
      memcpy(x + ulen + 1, value, POINTER_SIZE);
    } else {
      memset(x + ulen + 1, 0, sizeof(void *));
    }

    t->root = x;
    return 2;
  }

  while (1 & (intptr_t) p) {                                               /* walk tree for best memeber */
    cbt_node *q = (void *) (p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
  }

  uint32 newbyte;                                                          /* find critical bit */
  uint32 newotherbits;

  for (newbyte = 0; newbyte < ulen; ++newbyte) {                           /* find different byte */
    if (p[newbyte] != ubytes[newbyte]) {
      newotherbits = p[newbyte] ^ ubytes[newbyte];
      goto different_byte_found;
    }
  }

  if (p[newbyte] != 0) {
    newotherbits = p[newbyte];
    goto different_byte_found;
  }
  return 1;

 different_byte_found:                                                     /* find different bit */

  while (newotherbits & (newotherbits - 1))
    newotherbits &= newotherbits - 1;
  newotherbits ^= 255;
  uint8 c = p[newbyte];
  int newdirection = (1 + (newotherbits | c)) >> 8;

  cbt_node *newnode;                                                      /* allocate new node strcuture */
  if (posix_memalign((void **) &newnode,
                     sizeof(void *),
                     sizeof(cbt_node)))
    return 0;

  char *x;
  if (posix_memalign((void **) &x, sizeof(void *), ulen + 1 + sizeof(void*))) {
    free(newnode);
    return 0;
  }
  memcpy(x, ubytes, ulen + 1);
  if ( value != NULL ) {
    memset(x + ulen + 1, 0, sizeof(void *));
    memcpy(x + ulen + 1, value, POINTER_SIZE);
  } else {
    memset(x + ulen + 1, 0, sizeof(void *));
  }
  newnode->byte = newbyte;
  newnode->otherbits = newotherbits;
  newnode->child[1 - newdirection] = x;


  void **wherep = &t->root;                                                /* insert a new node into the tree */
  for (;;) {
    uint8 *p = *wherep;
    if (!(1 & (intptr_t) p)) break;
    cbt_node *q = (void *) (p - 1);
    if (q->byte > newbyte) break;
    if (q->byte == newbyte && q->otherbits > newotherbits) break;
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    wherep = q->child + direction;
  }
  newnode->child[newdirection] = *wherep;
  *wherep = (void *) (1 + (char *) newnode);
  return 2;
}

int
cbt_delete(cbt_tree *t, const char *u) {
  const uint8 *ubytes = (void *) u;
  const size_t ulen = strlen(u);
  uint8 *p = t->root;
  void **wherep = &t->root;
  void **whereq = 0;
  cbt_node *q = 0;
  int direction = 0;

  if (!p) return 0;                                                        /* Deal with deleting from an empty tree*/

  while (1 & (intptr_t) p) {                                               /* Walk the tree for the best match */
    whereq = wherep;
    q = (void *) (p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    direction = (1 + (q->otherbits | c)) >> 8;
    wherep = q->child + direction;
    p = *wherep;
  }

  if (0 != strcmp(u, (const char *) p)) return 0;                          /* Check the best match */
  free(p);

  if (!whereq) {                                                           /* Remove the element and/or node */
    t->root = 0;
    return 1;
  }

  *whereq = q->child[1 - direction];
  free(q);

  return 1;
}

static void
traverse(void *top) {
  uint8 *p = top;

  if (1 & (intptr_t) p) {                                                  /* Recursively free current node */
    cbt_node *q = (void *) (p - 1);
    traverse(q->child[0]);
    traverse(q->child[1]);
    free(q);
  } else {
    free(p);
  }
}

void
cbt_clear(cbt_tree *t)
{
  if (t->root) traverse(t->root);
  t->root = NULL;
}

static int
allprefixed_traverse(uint8 *top,
                     int (*handle) (const char *, void *, void *), void *arg){
  if (1 & (intptr_t) top) {                                               /* Deal with an internal node */
    cbt_node *q = (void *) (top - 1);
    for (int direction = 0; direction < 2; ++direction)
      switch(allprefixed_traverse(q->child[direction], handle, arg)){
        case 1: break;
        case 0: return 0;
        default: return -1;
      }
    return 1;
  }

  void* value = NULL;
  const char* key = (const char*)(top);
  value = top + strlen(key) + 1;
  return handle((const char *)top, value, arg);                           /* Deal with an external node */   
}

int
cbt_allprefixed(cbt_tree *t, const char *prefix,
                int (*handle) (const char *, void *, void *), void *arg){
  const uint8 *ubytes = (void *) prefix;
  const size_t ulen = strlen(prefix);
  uint8 *p = t->root;
  uint8 *top = p;

  if (!p) return 1;                                                        /* emptyset */

  while (1 & (intptr_t) p) {                                               /* Walk tree, maintaining top pointer */
    cbt_node *q = (void *) (p - 1);
    uint8 c = 0;
    if (q->byte < ulen) c = ubytes[q->byte];
    const int direction = (1 + (q->otherbits | c)) >> 8;
    p = q->child[direction];
    if (q->byte < ulen) top = p;
  }

  for (size_t i = 0; i < ulen; ++i) {                                      /* Check prefix */
    if (p[i] != ubytes[i]) return 1;
  }

  return allprefixed_traverse(top, handle, arg);
}