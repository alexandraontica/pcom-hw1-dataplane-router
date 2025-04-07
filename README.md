*Ontica Alexandra-Elena, 321CB*
# Tema 1 - Dataplane Router

Am incercat sa implementez toate cerintele din enunt.

## Implementare
Am urmat pasii din enuntul temei, asa ca nu voi mai descie pas cu pas ce am implementat pentru ca va fi cam acelasi lucru ca in cerinta. In orice caz, am adaugat comentarii peste tot in cod, sper sa fie suficient pentru a se intelege clar ce implementez.

Uitandu-ma pe comentarii, decizii care nu sunt mentionate in cerinta pe care le-am luat:
- cand trimit pachetul mai departe, imi trebuie si lungimea lui; creez tipul `packet` (structura) ca sa adaug in coada pachetelor care asteapta ARP reply nu doar continutul pachetelor, ci si lungimea lor; retin si interfata pt care trebuie sa le trimit mai departe (pt next hop)
- pachtele ARP si ICMP (pt debugging, nu ICMP echo reply) am preferat sa le construiesc de la 0; am incercat si sa suprascriu pachtele pe care le primeam, dar nu stiu ce faceam gresit, nu imi ajungeau pachetele cand le trimiteam dupa aceea; asa mi-a fost mai usor sa controlez informatia stocata in headere
- cand primesc un ARP reply trebuie sa caut in coada pachetul care are nevoie de MAC-ul primit; pt asta trebuie sa scot pe rand pachetele din coada, dar cele care nu se potrivesc trebuie sa le salvez undeva ca sa nu le pierd; mi-am creat deci o coada suplimentara (ca sa pastrez si ordinea) care stocheaza pachetele pe care inca nu le pot da mai departe; la final pun aceste pachete in coada initiala (care ajunge sa fie goala)

## Longest Prefix Match
Am folosit o trie, asa cum este sugerat si in cerinta.

**!! Am adaugat implementarea mea pentru trie in directorul `/lib`, fisierul `trie.c`, respectiv in directorul `/include`, fisierul `trie.h`. Implementarea functiei `longest_prefix_match()` se afla in fisierul `lib/trie.c`.**

Pentru trie am ales o varianta simplificata, in care fiecare nod corespunde unui octet dintr-o adresa IP. Am ales varianta aceasta cu un vector de 256 copii pentru fiecare nod deoarece am avut de implementat ceva similar anul trecut la SDA (arbore de sufixe, dar structura cu vectorul de copii semana).

Am scris functii care aloca un nod pentru trie, initializeaza o trie goala, adauga un prefix in trie, face longest prefix match in functie de prefixele din trie si elibereaza memoria ocupata de arbore. In `router.c` initializez tria (triele?) la inceput si apoi parcurg tabela de routare si adaug toate prefixele in trie. La final dezaloc memoria ocupata.

Campuri in structura de trie:
- `children` - vector de copii (daca de ex un nod are ca si copil diferit de null children[168], inseamna ca urmatorul octet din prefix este 168)
- `is_end` - marcheaza finalul unui prefix valid
- `prefix_len` - lungimea prefixului curent
- `prefix` - valoarea efectiva a prefixului
- `ip_addr` - adresa IP asociata unui prefix valid
- `interface` - interfata asociata unui prefix valid

Ultimele doua campuri mentionate sunt populate (valid) doar daca sunatem pe ultimul nod din prefix (`is_end` == 1). Am salvat si aceste informatii ca sa nu trebuiasca sa parcurg din nou route table-ul, sa caut prefixul si din entry-ul corespunzator sa extrag IP-ul si interfata. Am considerat ca in cazul de fata eficienta temporala este mai importanta decat memoria folosita.

Ca sa pot sa intorc in `longest_prefix_match()` doua valori (si adresa IP si interfata) in acelasi timp mi-am creat tipul `LPM` (structura) care contine fix aceste doua campuri.

`add_to_trie()` are complexitate O(1), deci ca sa adaug toate prefixele in trie am complexitate O(nr intrari tabela routare).
Functia `longest_prefix_match()` are complexitate **O(1)** (inaltimea arborelui este maxim 5 = numar octeti adresa IP/prefix + radacina goala, adica o constanta). 


## Debugging
Pentru debugging am folosit `printf`-uri. Le-am comentat acum ca sa nu adauge overhead. In fisierul `trie.c` am implementat si o functie care imi transforma un `int` reprezentand o adresa IP in format xxx.xxx.xxx.xxx (human readable) pe care am folosit-o in afisari.

Pentru trie am scris un `main` in care am testat adaugarea unor mai multe adrese IP (am harcodat si tot modificat pentru diferite cazuri), cu diferite masti. Apoi am incercat sa fac prefix match pe mai multe exemple ca sa ma asigur ca nu imi crapa algortimul din cauza structurii de date.

Initial nu stiam ca `ping` are timeout si nu intelegeam de ce nu imi ajungeau pachete cand simulam cu mininet-ul (am crezut ca am probleme la setup). Ulterior m-am prins ca trebuie sa cresc timeout-ul cu `-W` si cu `-i` timpul intre requesturile facute de `ping` si am reusit sa urmaresc pachetele si cu **Wireshark** (unele ping-uri, cand caut prima oara MAC-ul unui host, imi dureaza foarte mult si nu ma prind de ce).

## Ce probleme am intampinat?
In principal am avut probleme din cauza endianess-ului, din neatentie am uitat in repetate randuri sa fac conversia de la host la network (sau invers cand foloseam valorile in algoritm) si ma miram de ce nu imi ajung pachetele. Din fericire in fisierele de eroare cand pica testul scrie ce parte din pachet este diferita fata de ce este asteptat.

De asemenea, nu stiam ca fiecare interfata a routerului are o alta adresa MAC si o alta adresa IP. Pana sa ma prind de acest lucru eu gaseam interfata pentru next hop, dar cand trimiteam pachete foloseam adresele asociate interfetei pe care am primit pachetul, ceea ce nu era corect si pachetele nu ajungeau la destinatie. Deci pot spune ca am invatat ceva nou the hard way (am pierdut prea mult timp din cauza asta).

De asemenea, inca nu stiu de ce unele ping-uri imi merg foarte greu. Din cauz asta (cred) imi pica 2 teste. Cand rulez cu tabela ARP statica am 70 de puncte, adica trec toate forward-urile (inca am codul comentat pentru partea asta), deci presupun ca problema apare de la ARP dinamic. Inca nu am identificat de la ce mai exact.

## Posibile optimizari
(optimizari pe care nu le-am facut pentru ca ar fi fost mai complicat in C si nu am mai avut timp)

Pentru cache-ul ARP se poate folosi un hash map pentru a avea cautari in O(1) (acum fac cautarea in timp liniar). Optimizarea este utila pentru retele mai ample, acum sunt putine intrari in cache.

Cand primesc ARP reply, trebuie sa caut in coada de pachete care asteapta reply. Inainte de asta eu mi-am creat inca o coada in care sa salvez pachetele pe care le scot din prima si nu le trimit inca mai departe (adica pachetele a caror adresa IP destinatie nu se potriveste cu IP-ul sursa al ARP reply-ului). In loc sa folosesc 2 cozi ca sa nu pierd pachetele care inca asteapta ARP reply, aici as fi putut sa foloses un dequeue (as folosi mai putina memorie). Util pentru cazul in care sunt mai multe pachete primite care asteapta reply.

Se mai poate folosi si o implementare mai compacta pentru trie ca sa se economiseasca memorie, dar poate fi mai greu de gestionat (pot aparea mai multe probleme la pointeri, seg faulturi). De asemenea, in functie de implementare, cautarea nu va mai fi in timp constant (daca de ex copiii sunt reprezentati ca o lista sau chiar ca un vector fara a mai tine cont de <pozitie = valoarea octetului>).