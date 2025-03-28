#ifndef _TRIE_H_
#define _TRIE_H_

// adaptare a temei 2 la SDA de anul trecut (arbori de sufixe)

#define MAX_TRIE_CHILDREN 256

typedef struct node {
  int is_end;
  int prefix_len;
  uint32_t prefix;
  struct node *children[MAX_TRIE_CHILDREN];
} tnode, *trie;

trie alloc_node(int is_end, int prefix_len, uint32_t prefix);
trie create_trie();
trie add_to_trie(trie t, uint32_t prefix, uint32_t mask);
void free_trie(trie *t);

#endif /* _TRIE_H_ */