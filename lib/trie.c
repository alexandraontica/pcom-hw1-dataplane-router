#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include "trie.h"

char* int_to_ip(uint32_t ip) {
    uint8_t byte1 = (ip >> 24) & 0xFF;
    uint8_t byte2 = (ip >> 16) & 0xFF;
    uint8_t byte3 = (ip >> 8) & 0xFF;
    uint8_t byte4 = ip & 0xFF;

    char buffer[16];
    sprintf(buffer, "%u.%u.%u.%u", byte1, byte2, byte3, byte4);

    return strdup(buffer);
}

trie alloc_node(int is_end, int prefix_len, uint32_t prefix)
// aloc un nod in arbore
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
// creez arborele
{
    trie t = alloc_node(0, 0, 0);
    return t;
    // nu imi trebuie verificare suplimentare a alocarii,
    // oricum il returnez pe t (care ar fi NULL cand alocarea esueaza),
    // deci logica se pastreaza
}

trie add_to_trie(trie t, uint32_t prefix, uint32_t mask)
// adaug un nod nou in arbore
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
    int current_len = 0;

    // parcurg doar octetii care ma intereseaza (cf mastii)
    // merg pana la penultimul octet important
    int i;
    for (i = 3; i > 4 - num_bytes; i--) {
        // extrag din prefix octetul curent ca sa il adaug in arbore
        uint8_t current_byte = ((prefix >> (8 * i)) & 0xFF);
        current_prefix = ((current_prefix << 8) | current_byte);
        current_len++;

        if (!aux->children[current_byte]) {
            aux->children[current_byte] = alloc_node(0, current_len, (current_prefix << (8 * i)));
            if (!aux->children[current_byte]) {
                free_trie(&t);
                return NULL;
            }
        }

        printf("prefix curent: %s\n", int_to_ip((current_prefix << (8 * i))));

        aux = aux->children[current_byte];
    }

    // am ajuns la ultimul octet care ma intereseaza,
    // adaug ultimul nod
    int current_byte = (prefix >> (8 * i)) & 0xFF;
    current_prefix = ((current_prefix << 8) | current_byte);
    current_len++;

    if (!aux->children[current_byte]) {
        aux->children[current_byte] = alloc_node(1, current_len, (current_prefix << (8 * i)));
        if (!aux->children[current_byte]) {
            free_trie(&t);
            return NULL;
        }
    }
    aux = aux->children[current_byte];
    aux->is_end = 1;
    aux->prefix_len = current_len;
    aux->prefix = (current_prefix << (8 * i));

    // (current_prefix << (8 * i)) ar trebui sa fie egal cu prefixul initial at this point
    printf("Prefix de adaugat: %s, prefix curent: %s\n", int_to_ip(prefix), int_to_ip((current_prefix << (8 * i))));

    return t;
}

uint32_t longest_prefix_match(trie t, uint32_t ip) {
    if (!t) {
        return 0;
    }

    trie aux = t;
    uint32_t longest_prefix = 0;
    int longest_len = 0;
    int current_num_byte = 3; // incep de la ultimul octet

    // parcurg arborele
    while (aux) {
        uint8_t current_byte = (ip >> (8 * current_num_byte)) & 0xFF;

        // verific daca am un nod corespunzator octetului curent
        if (aux->children[current_byte]) {
            aux = aux->children[current_byte];

            // daca am ajuns la un nod final verific daca prefixul
            // curent e mai lung decat cel mai lung prefix gasit 
            // pana acum
            if (aux->is_end && aux->prefix_len > longest_len) {
                longest_len = aux->prefix_len;
                longest_prefix = aux->prefix;
            }
        } else {
            break; // nu mai am copii care sa se potriveasca
        }

        current_num_byte--;
    }

    return longest_prefix;
}

void free_trie(trie *t)
// elibereaza memoria ocupata de arbore
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

// int main() {
//     // cod de test sa vad ca am implementat bine
//     // ar fi ceva sa imi pice testele checker-ului din cauza unui segfault aici...

//     trie t = create_trie();

//     uint32_t prefix = (192 << 24) | (168 << 16) | (5 << 8) | 0;
//     uint32_t mask = (255 << 24) | (255 << 16) | (255 << 8) | 0;

//     t = add_to_trie(t, prefix, mask);

//     prefix = (10 << 24) | 0;
//     mask = (255 << 24) | 0;

//     t = add_to_trie(t, prefix, mask);

//     prefix = (192 << 24) | (167 << 16) | 0;
//     mask = (255 << 24) | (255 << 16) | 0;

//     t = add_to_trie(t, prefix, mask);

//     prefix = (192 << 24) | 0;
//     mask = (255 << 24) | 0;

//     t = add_to_trie(t, prefix, mask);

//     uint32_t ip = (192 << 24) | (163 << 16) | (5 << 8) | 3;

//     printf("\nIP: %s\n", int_to_ip(ip));
//     printf("LPM: %s\n", int_to_ip(longest_prefix_match(t, ip)));
//     free_trie(&t);

//     return 0;
// }