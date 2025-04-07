*Ontica Alexandra-Elena, 321CB*
# Tema 1 - Dataplane Router

**!! Am adaugat implementarea mea pentru trie in directorul `/lib`, fisierul `trie.c`, respectiv in directorul `/include`, fisierul `trie.h`. Implementarea functiei `longest_prefix_match()` se afla in fisierul `lib/trie.c`.**

## Implementare
Am urmat pasii din enuntul temei, asa ca nu voi mai descie pas cu pas ce am implementat pentru ca va fi cam acelasi lucru ca in cerinta. In orice caz, am adaugat comentarii peste tot in cod, sper sa fie suficient pentru a se intelege clar ce implementez.

Am incercat sa implementez toate cerintele din enunt.

## Debugging
Pentru debugging am folosit `printf`-uri. Nu stiu de ce, dar mininet-ul imi merge doar cand are el chef (desi am Linux nativ). Din cauza asta mi-a fost foarte greu sa ma prind ce nu merge in unele momente. Acum cand scriu asta am 94 de puncte pe local si incerc sa ma prind de ce imi pica doua forwardXY si restul trec fara probleme. Am folosit si Wireshark cand imi mergea ca sa vad ce contin pachetele pe care le trimit/primesc.

## Ce probleme am intampinat?
In principal am avut probleme din cauza endianess-ului, din neatentie am uitat in repetate randuri sa fac conversia de la host la network (sau invers cand foloseam valorile in algoritm) si ma miram de ce nu imi ajung pachetele.

De asemenea, nu stiam ca fiecare interfata a routerului are o alta adresa MAC si o alta adresa IP. Pana sa ma prind de acest lucru eu gaseam interfata pentru next hop, dar cand trimiteam pachete foloseam adresele interfetei pe care le-am primit, ceea ce nu era corect si pachetele nu ajungeau la destinatie. Deci pot spune ca am invatat ceva nou the hard way (am pierdut prea mult timp din cauza asta).

In afara de asta, am avut probleme pentru ca populam gresit headerele (mai ales in cazul pachetelor pe care le construiam de la 0), asta am rezolvat uitandu-ma in Wireshark.

## Posibile optimizari
(optimizari pe care nu le-am facut pentru ca ar fi fost mai complicat in C si nu am mai avut timp)

Pentru cache-ul ARP se poate folosi un hash map pentru a avea cautari in O(1) (acum fac cautarea in timp liniar).

Cand primesc ARP reply, trebuie sa caut in coada de pachete care asteapta reply. Inainte de asta eu mi-am creat inca o coada in care sa salvez pachetele pe care le scot din prima si nu le trimit inca mai departe (adica pachetele a caror adresa IP destinatie nu se potriveste cu IP-ul sursa al ARP reply-ului). In loc sa folosesc 2 cozi, aici as fi putut sa foloses un dequeue (folosesc mai putina memorie).
