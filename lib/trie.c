#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "trie.h"

trie alloc_node(int is_end, int prefix_len, uint32_t prefix)
// aloca un nod in arborele de sufixe
{
    trie aux = (trie)calloc(1, sizeof(tnode));
    if (aux) {
        aux->is_end = is_end;
        aux->prefix_len = prefix_len;
        aux->prefix = prefix;

        int i;
        for (i = 0; i < 256; i++) {
            aux->children[i] = NULL;
        }
    }
    return aux;
}

trie create_trie()
// creez arborele de sufixe
{
    trie t = alloc_node(0, 0, 0);
    return t;
    // nu imi trebuie verificare suplimentare a alocarii,
    // oricum il returnez pe t (care ar fi NULL cand alocarea esueaza),
    // deci logica se pastreaza
}

trie add_to_trie(trie t, uint32_t prefix, uint32_t mask)
// adaug un nod nou in arborele de sufixe

{
    if (!t) {
        t = create_trie();

        if (!t) {
            return NULL;
        }
    }

    trie aux = t;

    // calculez cati biti am in prefix:
    // nu pot determina lungimea prefixului doar bazandu-ma pe bitii 
    // de 1 din prefix deoarece 0 e o valoarea valida ce poate face
    // parte dintr-un prefix
    int bits = 0;
    while (mask) {
        bits += (mask & 1);
        mask = mask >> 1;
    }

    int num_bytes = bits / 8;  // mastile sunt multipli de 8
    
    int current_prefix = 0;

    // parcurg doar octetii care ma intereseaza (cf mastii)
    // merg pana la penultimul octet important
    int i;
    for (i = 3; i > 4 - num_bytes; i--) {
        // extrag din prefix octetul curent ca sa il adaug in arbore
        uint8_t current_byte = ((prefix >> (8 * i)) & 0xFF);
        current_prefix = ((current_prefix << 8) | current_byte);

        if (!aux->children[current_byte]) {
            aux->children[current_byte] = alloc_node(0, num_bytes, (current_prefix << (8 * i)));
            if (!aux->children[current_byte]) {
                free_trie(&t);
                return NULL;
            }
        }

        aux = aux->children[current_byte];
    }

    // am ajuns la ultimul octet care ma intereseaza,
    // adaug ultimul nod
    int current_byte = (prefix >> (8 * i)) & 0xFF;
    current_prefix = ((current_prefix << 8) | current_byte);

    if (!aux->children[current_byte]) {
        aux->children[current_byte] = alloc_node(1, num_bytes, (current_prefix << (8 * i)));
        if (!aux->children[current_byte]) {
            free_trie(&t);
            return NULL;
        }
    }
    aux = aux->children[current_byte];
    aux->is_end = 1;
    aux->prefix_len = num_bytes;
    aux->prefix = (current_prefix << (8 * i));

    // (current_prefix << (8 * i)) ar trebui sa fie egal cu prefixul initial at this point
    printf("Prefix vechi: %u, prefix added to trie: %u\n", prefix, (current_prefix << (8 * i)));

    return t;
}

void free_trie(trie *t)
// elibereaza memoria ocupata de arborele de sufixe
{
    if (!(*t)) {
        return;
    }

    int i;
    for (i = 0; i < MAX_TRIE_CHILDREN; i++) {
        free_trie(&((*t)->children[i]));
    }

    free(*t);
    *t = NULL;
}

int main() {
    // cod de test sa vad ca am implementat bine
    // ar fi ceva sa imi pice testele checker-ului din cauza unui segfault aici...

    // code

    return 0;
}