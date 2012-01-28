#ifndef CBT_H_
#define CBT_H_

#define _POSIX_C_SOURCE 200112
#define uint8 uint8_t
#define uint32 uint32_t

typedef struct {
  void *child[2];
  uint32 byte;
  uint8 otherbits;
} cbt_node;

typedef struct {
  void *root;
} cbt_tree;

int cbt_contains(cbt_tree *t, const char *u);
int cbt_insert(cbt_tree *t, const char *u, void* data);
int cbt_delete(cbt_tree *t, const char *u);
void cbt_clear(cbt_tree *t);
int cbt_allprefixed(cbt_tree *t, const char *prefix,
                    int (*handle) (const char *, void *, void *), void *arg);

#endif  // CBT_H_
