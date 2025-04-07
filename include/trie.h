#ifndef _TRIE_H_
#define _TRIE_H_

#define MAX_TRIE_CHILDREN 256

typedef struct node {
  int is_end; // marcheaza ultimul nod dintr-un prefix
  int prefix_len;
  uint32_t prefix;
  uint32_t ip_addr; // adresa ip o populez doar daca is_end == 1
  int interface; // interfata o populez doar daca is_end == 1
  struct node *children[MAX_TRIE_CHILDREN];
} tnode, *trie;

typedef struct {
    uint32_t ip_addr;
    int interface;
} LPM;

char* int_to_ip(uint32_t ip);
trie alloc_node(int is_end, int prefix_len, uint32_t prefix, int interface, uint32_t ip_addr);
trie create_trie();
trie add_to_trie(trie t, uint32_t prefix, uint32_t mask, int interface, uint32_t ip_addr);
LPM longest_prefix_match(trie t, uint32_t ip);
void free_trie(trie *t);

#endif /* _TRIE_H_ */