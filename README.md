
## Tema 2 PCom - Aplicatie client-server TCP si UDP pentru gestionarea mesajelor

### Teodorescu Ioan, 323CB

Tema consta in implementarea unei aplicatii care respecte modulul client-server pentru gestiunea mesajelor. Motivatia temei este implementarea server-ului 
si creearea unui protocol TCP pentru clienti. Serverul si clientii isi trimit / primesc comenzi atat intre ei, cat si de la tastatura. Fiecare client TCP 
doreste sa se aboneze la un client UDP si sa primeasca mesaje de la acesta. In plus, daca clientul TCP se deconecteaza si revine, va primi mesajele pierdute 
de la clientii UDP abonati, cu ajutor functionalitatii store-and-forward. 

Pentru testare, recomand asteptarea terminarii conexiunilor care folosesc portul cerut (12345), folosind `sudo netstat | grep 12345` (posibil sa apara delay-uri
 ale server-ului daca inca mai exista conexiuni). 

### Structuri folosite in implementarea aplicatiei

1. structura `udp_info` - defineste un mesaj al clientului UDP. Sunt prezente urmatoarele variabile:
    1. char ip_address[16] - adresa ip a clientului UDP
    2. char port[6] - portul sursa al clientului
    - `char topic[50]` (un titlu mic), `char type` (tine tipul de date al continutului), `char content[1500]` (mesajul clientului).
2. structura `channel` - pastreaza un mesaj UDP, impreuna cu parametrul `sf`, astfel incat sa se memoreze care topic va memora mesajele si le va trimite la 
reconectarea clientului UDP.
3. structura `client_info` - memoreaza informatii despre clientii tcp precum: 
    1. file descriptor-ul socketului
    2. id-ul clientului
    3. numarul de alocari curente ale celor doi vectori de mai jos (aloc). `subscribed_topics` mentine topicurile la care clientul este abonat, in timp ce 
    `unsent_topics` , mesajele netrimise atunci cand clientii UDP trimit date, deoarece clientii TCP s-au deconectat, insa au activat functionalitatea 
    store-and-forward.
    

### Implementarea server-ului

- `init_client` initializeaza o instanta a clientului, aloca memorie pentru vectori si seteaza variabilele la valori prestabilite.
- `generate_message` porneste de la un buffer si adresa unui client si construieste un mesaj de tip-ul UDP.

Pentru inceput, se dezactiveaza buffering-ul, se instantiaza vectorii, creez socketsii pentru UDP, si TCP, se dezactiveaza algoritmul lui Naegle, etc. Pentru 
TCP, se leaga socket-ul la un port cunoscut si se face pasiv, doar pentru conectare (nu se pot citi si scrie date). Pentru monitorizarea sockets-ilor se va 
folosi `poll()`, care ajuta la controlarea mai multor descriptori in acelasi timp. Primele 3 elemente din multimea de file descriptori monitorizati vor fi 
pastrati pentru `STDIN`, `tcp_socket` si `udp_socket`. 

Se intra in loop-ul serverului si se asteapta date pe unul dintre file descriptori. Se parcurge multimea de file descriptori astfel:

Daca fd-ul este egal cu `STDIN_FILENO`, asta inseamna ca primim date de la tastatura. Singura comanda pentru acest file descriptor este cea de **********exit**********,
 unde se va trimite un mesaj de ****exit**** pentru fiecare client conectat la server, se seteaza exit_sock cu 1 (se iese din loop) si break pentru a iesii din for. 

Daca fd-ul este egal cu cel al socket-ului de TCP, asta inseamna ca a venit o cerere de conexiune de la unii dintre clienti. Se blocheaza, asteptand conectarea
 clientului. Intoarce un nou socket, folosit pentru a scrie si a citi date de la clientul respectiv. Dupa care, se va citi ID-ul clientului cu care tocmai am 
 stabilit conexiunea. Se parcurge vectorul de clienti TCP si se verifica daca mai exista un client cu acelasi id. Daca fd-ul al clientului curent din vector 
 este 0, asta inseamna ca conexiunea a fost inchisa, si acum, clientul vrea sa se reconecteze (se seteaza variabila reconnected cu 1).

Daca fd-ul nu este 0, asta inseamna ca clientul TCP este inca activ → se va afisa un mesaj care confirma faptul ca exista deja clientul ala in server si se 
inchide socketul. 

Se va afisa un mesaj de confirmare a conectarii clientului, apoi se verifica daca acesta s-a reconectat. 

Daca **da**, se adauga fd-ul in pfds. Dupa, se vor lua mesajele care nu au putut fi trimise de clientii UDP la care erau abonati (clientul era deconectat):
 Se ia fiecare mesaj, se verifica daca se aplica SF, se trimite la client, apoi se golesc mesajele trimise.

Daca **nu**, (mai intai se verifica daca avem loc sa punem clientul in vector → daca nu realocam vectorii clients si pfds) clientul va fi adaugat in vectorul
 `clients`. 

Daca fd-ul este egal cu cel al socket-ului de TCP, asta inseamna ca a venit o cerere de la clientii UDP. Se primesc datele primite si se genereaza o variabila 
de tipul `udp_info`, cu care vom lucra. Verificam daca avem duplicat → daca da, se va ignora, daca nu, se adauga in lista de topic-uri (se verifica daca avem 
loc in vector si realocam daca nu). Acum, vom lua fiecare client care are un numarul de abonari mai mare decat 0 si verificam daca vom gasi topicul clientului 
UPD in lista acestuia. In caz afirmativ, verificam daca file descriptorul este ≠ 0 (activ) → se trimite mesajul catre clientul abonat la acel topic. Daca fd-ul 
este o (inactiv), verificam daca clientul a setat store-and-forward → se salveaza mesajul in `unsent_topics`.

Daca nu am trecut printr-una din conditiile de mai sus, asta inseamna ca am primit date pe unul dintre socketii cu care comunic cu unul dintre clienti. 
Se primesc datele de la client, si caut fd-ul din `clients`, care este echivalent cu pfds[i] (fd-ul curent).  

Aici se vor trata comenzile pe care clientul TCP doreste sa le faca:

- `exit` → se confirma deconectarea sa prin printarea mesajului corespunzator la `[stdout`]. Se inchide socket-ul si fd-ul se sterge din multimea de fd monitorizati
- `subscribe` → se preia topicul si sf-ul din mesaj si apoi se cauta mesajul UDP dorit de client. Cand va fi gasit, se va parcurge vectorul de abonari ale
 clientului TCP corespunzator, pentru a verifica daca nu avem dublicat. Daca nu avem, se adauga in lista de abonari.
- `unsubscribe` →se ia topicul si se cauta in lista de abonari a clientului. Daca exista, abonarile din dreapta sa se vor shifta la stanga cu o pozitie

In final, se iese din loop-ul server-ului, se elibereaza memoria si se inchid socketsii.

### Implementarea subscriber-ului

- `print_message` - afiseaza mesajul de la clientii UDP in formatul corespunzator si in functie de tipul de date al content-ului.

Se incepe prin alocarea multimii de file descriptori monitorizati, deschiderea socket-ului, dezactivarea algoritmului lui Neagle, urmata de stabilirea 
conexiunii (TCP 3 way handshake) catre server. Daca avem conexiunea, se intoarce id-ul clientului. Se adauga in multimea de file descriptori stdin-ul si
 socket-ul ce face legatura cu serverul. La fel ca la server, se intra intr-un loop si se asteapta date pe unul dintre file descriptori.

Daca primim date de la stdin, asta inseamna ca trebuie sa prelucram comenzile. Pentru fiecare comanda, la ```subscribe``` si la ```unsubscribe```, voi lua
 fiecare parametru din verific daca este valid. Daca nu, se va afisa o eroare si trebuie scrisa alta comanda (pentru a evita segmentation fault-uri in server). 
 Daca totul este ok, se trimite comanda la server.

Daca am primit date de la server, asta inseamna ca trebuie sa le prelucram. Aici, avem o singura comanda si e cea de ```exit```, un se va iesii din loop. 
In rest, singurele date trimise de server sunt legate de mesajele de la topic-urile la care clientul este `abonat`.

La final, se elibereaza buffer-ul si se inchide socket-ul. 

### Resurse folosite 
https://beej.us/guide/bgnet/html/
https://pcom.pages.upb.ro/labs/lab7/multiplexing.html
https://pcom.pages.upb.ro/labs/lab5/client_serv.html
https://pcom.pages.upb.ro/labs/lab7/tcp_sockets.html

Pe local, mentionez ca programul imi trece pe toate teste (exista unele cazuri in care testele nu primesc punctaj)

Rezultat checker local: 

```bash
rm -rf server subscriber *.o *.dSYM
Compiling
gcc -o server server.c helper.h -lm -Wall -g -Werror -Wno-error=unused-variable 
gcc -o subscriber subscriber.c helper.h -lm -Wall -g -Werror -Wno-error=unused-variable 
Starting the server
Starting subscriber C1
Generating one message for each topic
Subscribing C1 to all topics without SF
Generating one message for each topic
Disconnecting subscriber C1
Generating one message for each topic
Starting subscriber C1
Starting another subscriber with ID C1
Starting subscriber C2
Subscribing C2 to topic a_non_negative_int without SF
Subscribing C2 to topic a_negative_int with SF
Generating one message for topic a_non_negative_int
Generating one message for topic a_negative_int
Disconnecting subscriber C2
Generating one message for topic a_non_negative_int
Generating three messages for topic a_negative_int
Starting subscriber C2
Generating one message for each topic 30 times in a row
Stopping the server
rm -rf server subscriber *.o *.dSYM

RESULTS
-------
compile...........................passed
server_start......................passed
c1_start..........................passed
data_unsubscribed.................passed
c1_subscribe_all..................passed
data_subscribed...................passed
c1_stop...........................passed
c1_restart........................passed
data_no_clients...................passed
same_id...........................passed
c2_start..........................passed
c2_subscribe......................passed
c2_subscribe_sf...................passed
data_no_sf........................passed
data_sf...........................passed
c2_stop...........................passed
data_no_sf_2......................passed
data_sf_2.........................passed
c2_restart_sf.....................passed
quick_flow........................passed
server_stop.......................passed
```