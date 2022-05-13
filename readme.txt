Radu Sabina - 331CB - Tema 2 - Aplicatie client-server TCP si UDP pentru 
gestionarea mesajelor

Pentru realizarea temei 2, am folosit de scheletul laboratorului 8 pe baza 
caruia am facut modificari in functie de cerintele date.

In subscriber.cpp, creez conexiunea cu serverul, apoi trimit ID-ul 
clientului si dezactivez algoritmul lui Nagle. Clientul poate primi 
de la tastatura comenzile "subscribe", "unsubscribe" si "exit". 
Daca primeste o comanda invalida, primeste o eroare.

In server.cpp, m-am folosit de unordered_map pentru a contoriza 
clientii conectati la server si datele care trebuie sa fie trimise 
catre acestia. Aceasta abordare este mai eficienta decat 
folosind un map intrucat este implementat cu hash table, astfel costul 
pentru cautare, insertie si stergere este O(1).

Se creeaza un socket pentru conexiunile UDP si un socket pe care 
primeste cereri de la clientii TCP. Atunci cand se primeste o cerere de 
conexiune, se verifica daca clientul respectiv este deja conectat. Daca nu, 
atunci setez starea acestuia online si se trimit mesajele pentru 
abonamentele cu SF.

Cand se primeste un mesaj de la clientii UDP, este convertit in formatul 
cerut in enunt, apoi se verifica pentru fiecare client daca este abonat 
la topicul respectiv. Daca este abonat si este online, se trimite 
mesajul direct. Daca este abonat si nu este online, mesajul va fi pastrat 
doar daca abonamentul pentru acel topic este unul cu SF. 

Cand se primesc date de la clientii TCP, verific daca conexiunea este oprita 
si setez starea clientului offline, inchid socketul si nu mai atribui ID-ul 
clientului respectiv. Daca nu s-a inchis conexiunea, atunci am primit 
comenzile "subscribe" sau "unsubscribe" si se updateaza abonamentele 
clientilor pentru topicul respectiv. 

Daca serverul primeste de la tastatura mesajul "exit", atunci se inchide 
serverul si conexiunea cu clientii.